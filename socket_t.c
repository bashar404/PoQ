#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>

#ifdef _WIN32
#include <Windows.h>
#else

#include <unistd.h>
#include <errno.h>

#endif

#include "poet_common_definitions.h"

#define SELECT_TIMEOUT {5,0} /* 5 seconds */
#define RETRIES_THRESHOLD 10
#define ENDING_CHARACTER '\0'
#define ENDING_STRING "\r\n"

#include "socket_t.h"
#include "queue_t.h"

#define DEFAULT_ADDRESS INADDR_ANY

// TODO: set C preprocessor conditionals for SSL

static int create_primitive_socket(int domain, int type, int protocol, int *opt) {
    int errsv = 0;
    int fd = socket(domain, type, protocol);
    errsv = errno;
    if (fd == -1) goto error;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, opt, sizeof(*opt))) {
        errsv = max(errsv, errno);
        goto error;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, opt, sizeof(*opt))) {
        errsv = max(errsv, errno);
        goto error;
    }

    return fd;

    error:
    if (fd != -1) {
        close(fd);
    }

    errno = errsv;
    perror("create_primitive_socket");
    errno = errsv;
    return -1;
}

socket_t *socket_constructor(int domain, int type, int protocol, const char *ip, int port) {
    socket_t *s = (socket_t *) malloc(sizeof(socket_t));
    if (s == NULL) goto error;

    memset(s, 0, sizeof(socket_t));

    s->domain = domain;
    s->type = type;
    s->protocol = protocol;

    s->opt = 1; /* 1 to enable options */
    s->socket_descriptor = create_primitive_socket(domain, type, protocol, &(s->opt));
    if (s->socket_descriptor == -1) goto error;

    struct sockaddr_in *address = &(s->address);
    address->sin_family = domain;
    address->sin_port = htons(port);

    if (ip != NULL) {
        if (inet_pton(domain, ip, &(address->sin_addr)) != 1) goto error;
    } else {
        address->sin_addr.s_addr = DEFAULT_ADDRESS;
    }

    s->addrlen = sizeof(*address);
    s->fd_set.is_parent = 1;
    FD_ZERO(&s->fd_set.data.set);
    FD_SET(s->socket_descriptor, &s->fd_set.data.set);
    s->fd_set.close_queue = queue_constructor();

    return s;

    error:
    perror("socket constructor");
    // XXX: probably more cleaning needs to be done with the socket
    if (s != NULL) {
        free(s);
    }
    return NULL;
}

int socket_bind(socket_t *soc) {
    assert(soc != NULL);

    if (soc->is_closed) {
        ERROR("socket is closed, not binding anything ... \n");
        return 0;
    }

    int ret;
    if ((ret = bind(soc->socket_descriptor, (struct sockaddr *) &(soc->address), sizeof(soc->address))) < 0) goto error;
    return ret;

    error:
    perror("socket bind");
    return ret;
}

int socket_listen(socket_t *soc, int max_connections) {
    assert(max_connections > 0);
    assert(soc != NULL);

    if (soc->is_closed) {
        ERROR("socket is closed, not listening anything ... \n");
        return -1;
    }

    max_connections = max(0, min(max_connections, FD_SETSIZE));

    int ret;
    if ((ret = listen(soc->socket_descriptor, max_connections)) < 0) goto error;
    soc->max_connections = max_connections;

    return ret;

    error:
    perror("socket listen");
    return ret;
}

static socket_t *get_parent(socket_t *soc) {
    if (soc == NULL) return NULL;
    if (soc->fd_set.is_parent == 0) {
        return get_parent(soc->fd_set.data.parent);
    }

    return soc;
}

int socket_select_parent(socket_t *soc) {
    assert(soc != NULL);
    socket_t *parent = get_parent(soc);
    assert(parent != NULL);
    struct timeval val = SELECT_TIMEOUT;

    if (soc->is_closed) {
        ERROR("socket is closed, not receiving anything ... \n");
        return 0;
    }

    queue_t *q = parent->fd_set.close_queue;
    if (soc->fd_set.is_parent == 1) {
        while (!queue_is_empty(q)) {
            int fd = queue_front(q);
            queue_pop(q);
            FD_CLR(fd, &parent->fd_set.data.set);
        }
    }

    int errsv = 0;
    struct timeval tmp;
    int status = 1;

    retry:
    tmp = val;
    fd_set read_fd_set = parent->fd_set.data.set;
    if (select(parent->socket_descriptor+1, &read_fd_set, NULL, NULL, &tmp) < 0) {
        errsv = errno;
        perror("select");
        status = 0;
    }

    if (status) {
        if (FD_ISSET(parent->socket_descriptor, &parent->fd_set.data.set)) {
            errno = errsv;
            status = 1;
        } else {
            ERR("select timeout on socket %d reached, retrying ...\n", soc->socket_descriptor);
            goto retry;
        }
    }

    errno = errsv;
    return status;
}

int socket_select(socket_t *soc) {
    assert(soc != NULL);
    return socket_select_parent(soc);
}

socket_t *socket_accept(socket_t *soc) {
    assert(soc != NULL);

    if (soc->is_closed) {
        ERROR("socket is closed, not accepting any connection ... \n");
        return NULL;
    }

    int new_socket_fd;
    if ((new_socket_fd = accept(soc->socket_descriptor, (struct sockaddr *) &(soc->address),
                                (socklen_t *) &(soc->addrlen))) < 0)
        goto error;

    socket_t *new_socket = malloc(sizeof(socket_t));
    if (new_socket == NULL) goto error;
    memcpy(new_socket, soc, sizeof(socket_t));
    new_socket->socket_descriptor = new_socket_fd;
    new_socket->fd_set.is_parent = 0;
    new_socket->fd_set.data.parent = soc;

    socket_t *parent = get_parent(soc);
    FD_SET(new_socket->socket_descriptor, &parent->fd_set.data.set);

    return new_socket;
    error:
    perror("socket accept");
    return NULL;
}

int socket_connect(socket_t *soc) {
    assert(soc != NULL);

    if (soc->is_closed) {
        ERROR("socket is closed, not connecting to anything ... \n");
        return -1;
    }

    int ret;
    int errsv = 0;
    if ( (ret = connect(soc->socket_descriptor, (struct sockaddr *) &(soc->address), soc->addrlen)) != 0) {
        errsv = errno;
        if (errsv == EISCONN) {
            errsv = 0;
            ret = 0;
        } else {
            ERR("connect error code is: %d (%s)\n", errsv, strerror(errsv));
            goto error;
        }
    }

    return ret;

    error:
    perror("socket connect");
    errno = errsv;
    return ret;
}

int socket_connect_retry(socket_t *soc) {
    assert(soc != NULL);

    int errsv = 0;
    int retries = 0;
    int connected = 0;
    do {
        int ret = socket_connect(soc);
        connected = ret;
        if (connected != 0) {
            /* Source: http://man7.org/linux/man-pages/man2/connect.2.html
             * If connect() fails, consider the state of the socket as unspecified.
             * Portable applications should close the socket and create a new one
             * for reconnecting. */
            close(soc->socket_descriptor);
            int new_fd = create_primitive_socket(soc->domain, soc->type, soc->protocol, &(soc->opt));
            if (new_fd == -1) {
                errsv = errno;
                goto error;
            }

            soc->socket_descriptor = new_fd;
            sleep(1);
        }
        retries++;
    } while(connected != 0 && retries < RETRIES_THRESHOLD);

    error:
    errno = errsv;
    perror("socket_connect_retry");
    errno = errsv;
    return connected;
}

int socket_recv(socket_t *soc, void *buffer, int buffer_len, int flags) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    if (soc->is_closed) {
        ERROR("socket is closed, not receiving anything ... \n");
        return -1;
    }

    int errsv = 0;
    int valread;
    int empty_queue = 0;
    again:
    valread = recv(soc->socket_descriptor, buffer, buffer_len, flags);

    if (valread < 0 && !empty_queue) {
        ERRR("first try on getting data is unsuccessfull\n");
        errsv = errno;
        perror("socket recv");
        if (errsv == EINTR || errsv == EAGAIN) {
            empty_queue = 1;
            ERR("seems that there is no data available on the recv queue, waiting for a change in the socket %d descriptor ...\n", soc->socket_descriptor);
            int s = socket_select(soc);
            if (!s) {
                errsv = errno;
                perror("socket_recv -> select");
            }
            goto again;
        }
        ERRR( "valread: %d\n", valread);
    }

    REPORT("Received raw data = %d\n", valread);

    errno = errsv;
    return valread;
}

static char *concat_buffers(queue_t *queue) {
    size_t expected_size = queue_size(queue) * BUFFER_SIZE;

    char *buffer = malloc(expected_size);
    if (buffer == NULL) {
        perror("malloc");
        goto error;
    }
    memset(buffer, ENDING_CHARACTER, expected_size);

    size_t pos = 0;
    while (!queue_is_empty(queue)) {
        char *current_buffer = queue_front(queue);
        queue_pop(queue);

        memcpy(buffer + (pos++ * BUFFER_SIZE), current_buffer, BUFFER_SIZE);

        free(current_buffer);
    }

    assert(pos <= expected_size);

    goto terminate;

    error:
    ERROR("Fatal error, can not proceed with receiving message\n");
    if (buffer != NULL) {
        free(buffer);
    }
    buffer = NULL;

    terminate:
    return buffer;
}

int socket_get_message_custom(socket_t *soc, void **buffer, size_t *buff_size, int flags) {
    assert(soc != NULL);
    assert(buffer != NULL);
//    assert(*buffer == NULL);
    assert(buff_size != NULL);

    if (soc->is_closed) {
        ERROR("socket is closed, not receiving anything ... \n");
        return -1;
    }

    queue_t *buffer_queue = queue_constructor();

    size_t current_size = 0;
    size_t received;
    uint retries;
    int errsv;

    int finished = 0;
    do {
        char *current_buffer = malloc(BUFFER_SIZE);
        if (current_buffer == NULL) {
            perror("malloc");
            goto error;
        }
        memset(current_buffer, 0, BUFFER_SIZE);
        queue_push(buffer_queue, current_buffer);

        retries = 0;
        retry:
        received = socket_recv(soc, current_buffer, BUFFER_SIZE, flags);
        errsv = errno;
        if (received < 0) {
            ERROR("Error receiving part of the buffer, retrying...\n");
            retries++;
            if (retries < RETRIES_THRESHOLD && errsv == EAGAIN) {
                goto retry;
            } else {
                errno = errsv;
                perror("socket get message -> retry");
                goto error;
            }
        } else if (received == 0) {
            ERROR("Error, seems that the connection is closed.\n");
            goto error;
        }

        char *end_of_message = strstr(current_buffer, "\r\n");
        if (end_of_message != NULL) { /* The message have been all received */
            *end_of_message = '\0';
            received = strlen(current_buffer);
            finished = 1;
        }

        current_size += received;
    } while (finished == 0);

    *buffer = (void *) concat_buffers(buffer_queue);
    *buff_size = strlen(*buffer);

    goto terminate;

    error:
    ERROR("Fatal error, can not proceed with receiving message\n");
    if (*buffer != NULL) {
        free(*buffer);
    }
    *buffer = NULL;
    *buff_size = 0;
    current_size = 0;

    terminate:
    queue_destructor(buffer_queue, 1);
    REPORT("Received data on message: %lu\n", current_size);
    return current_size;
}

int socket_get_message(socket_t *soc, void **buffer, size_t *buff_size) {
    return socket_get_message_custom(soc, buffer, buff_size, MSG_DONTWAIT & 0);
}

int socket_send(socket_t *soc, const void *buffer, size_t buffer_len, int flags) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    if (soc->is_closed) {
        ERROR("socket is closed, not sending anything ... \n");
        return -1;
    }

    int val, errsv;
    again:
    val = send(soc->socket_descriptor, buffer, buffer_len, MSG_NOSIGNAL | flags);
    errsv = errno;

    if (val <= 0) {
        if (errno == EINTR || errno == EAGAIN) goto again;
        perror("socket send");
    }

    errno = errsv;
    REPORT("Sent raw data: %d\nRaw data lost: %lu\n", val, buffer_len - val);
    return val;
}

int socket_send_message_custom(socket_t *soc, void *buffer, size_t buffer_len, int flags) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    ERRR("Message to be sent: [%.*s] on socket %d\n", (int) buffer_len, buffer, soc->socket_descriptor);
    if (soc->is_closed) {
        ERROR("socket is closed, not sending anything ... \n");
        return -1;
    }

    int sent, total_sent = 0;
    int retries;
    int errsv;
    do {
        retries = 0;

        retry:
        sent = socket_send(soc, buffer + total_sent, min(buffer_len - total_sent, BUFFER_SIZE), flags);
        errsv = errno;
        if (sent < 0) {
            perror("socket send message");
            retries++;
            if (retries < RETRIES_THRESHOLD && errsv == EAGAIN) {
                ERROR("Error sending part of the buffer, retrying ...\n");
                goto retry;
            } else {
                ERROR("Fatal error sending part of the buffer, aborting ... \n");
                goto error;
            }
        } else if (sent == 0) {
            ERROR("Error sending part of the buffer, seems that the connection is closed. Closing socket ...\n");
            socket_close(soc);
            goto error;
        }
        total_sent += sent;
    } while (total_sent < buffer_len && retries < RETRIES_THRESHOLD);

    sent = socket_send(soc, ENDING_STRING, strlen(ENDING_STRING), flags);
    if (sent < strlen(ENDING_STRING)) {
        ERROR("Error sending ending of message\n");
        goto error;
    }
//    total_sent += sent;

#ifdef DEBUG2
    ERRR("Message sent: [%.*s] on socket %d\n", total_sent, buffer, soc->socket_descriptor);
    if (total_sent < buffer_len) {
        WARN("sent buffer is smaller than intended (%d < %lu)\n", total_sent, buffer_len);
    }
#endif

    goto terminate;

    error:
    ERROR("Fatal error, can not proceed with sending message\n");
    total_sent = 0;

    terminate:
    REPORT("Message data sent: %d\n", total_sent);
    return total_sent;
}

int socket_send_message(socket_t *soc, void *buffer, size_t buffer_len) {
    return socket_send_message_custom(soc, buffer, buffer_len, MSG_DONTWAIT & 0);
}

void socket_close(socket_t *soc) {
    assert(soc != NULL);
    ERR("closing socket: %d\n", soc->socket_descriptor);

    if (soc->is_closed) {
        WARN("Trying to close an already closed socket %d\n", soc->socket_descriptor);
        return;
    }

    if (close(soc->socket_descriptor) == -1) {
        perror("socket_close");
        ERROR("Error trying to close socket %d\n", soc->socket_descriptor);
    }

    socket_t *parent = get_parent(soc);
    queue_push(parent->fd_set.close_queue, (void *) soc->socket_descriptor);

    soc->is_closed = 1;
}

void socket_destructor(socket_t *soc) {
    assert(soc != NULL);

    if (!soc->is_closed) {
        socket_close(soc);
    }

    if (soc->fd_set.is_parent == 1) {
        queue_destructor(soc->fd_set.close_queue, 0);
    }

    free(soc);
}

#ifdef __cplusplus
}
#endif
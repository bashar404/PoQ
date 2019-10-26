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

#ifdef DEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#define ERR(...) /**/
#endif

#ifndef max
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#endif

#define BUFFER_SIZE 1024*8
#define RETRIES_THRESHOLD 10
#define ENDING_CHARACTER '\0'

#include "socket_t.h"
#include "queue_t.h"

#define DEFAULT_ADDRESS INADDR_ANY

// TODO: set C preprocessor conditionals for SSL

socket_t *socket_constructor(int domain, int type, int protocol, const char *ip, int port) {
    socket_t *s = (socket_t *) malloc(sizeof(socket_t));
    if (s == NULL) goto error;

    memset(s, 0, sizeof(socket_t));

    s->socket_descriptor = socket(domain, type, protocol);
    if (s->socket_descriptor == 0) goto error;

    if (setsockopt(s->socket_descriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &(s->opt), sizeof(s->opt)))
        goto error;

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

    max_connections = max(0, min(max_connections, FD_SETSIZE));

    int ret;
    if ((ret = listen(soc->socket_descriptor, max_connections)) < 0) goto error;
    soc->max_connections = max_connections;

    return ret;

    error:
    perror("socket listen");
    return -1;
}

static socket_t *get_parent(socket_t *soc) {
    if (soc == NULL) return NULL;
    if (soc->fd_set.is_parent == 0) {
        return get_parent(soc->fd_set.data.parent);
    }

    return soc;
}

socket_t *socket_select(socket_t *soc) {
    assert(soc != NULL);
    socket_t *parent = get_parent(soc);
    assert(parent != NULL);
    struct timeval val = {5, 0};
    if (select(parent->socket_descriptor+1, &parent->fd_set.data.set, NULL, NULL, &val) < 0) {
        perror("select");
        exit(EXIT_FAILURE);
    }

    if (FD_ISSET(parent->socket_descriptor, &parent->fd_set.data.set)) {
        return socket_accept(parent);
    }

    return NULL;
//    else {
//        fprintf(stderr, "If this reached this point then something is wrong with select\n");
//        assert(0);
//        exit(EXIT_FAILURE);
//    }
}

socket_t *socket_accept(socket_t *soc) {
    assert(soc != NULL);

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

    int ret;
    if ((ret = connect(soc->socket_descriptor, (struct sockaddr *) &(soc->address), soc->addrlen)) != 0) {
        goto error;
    }

    return ret;

    error:
    perror("socket connect");
    return ret;
}

int socket_recv(socket_t *soc, void *buffer, int buffer_len) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    int valread;
    again:
    valread = recv(soc->socket_descriptor, buffer, buffer_len, MSG_DONTWAIT);

    if (valread < 0) {
        if (errno == EINTR || errno == EAGAIN) goto again;
        perror("socket recv");
        fprintf(stderr, "valread: %d\n", valread);
    }

    return valread;
}

char *concat_buffers(queue_t *queue) {
    size_t expected_size = queue_size(queue) * BUFFER_SIZE;

    char *buffer = malloc(expected_size);
    if (buffer == NULL) {
        perror("malloc");
        goto error;
    }
    memset(buffer, ENDING_CHARACTER, expected_size);

    unsigned char c;
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
    fprintf(stderr, "Fatal error, can not proceed with receiving message\n");
    if (buffer != NULL) {
        free(buffer);
    }
    buffer = NULL;

    terminate:
    return buffer;
}

int socket_get_message(socket_t *soc, void **buffer, size_t *buff_size) {
    assert(soc != NULL);
    assert(buffer != NULL);
//    assert(*buffer == NULL);
    assert(buff_size != NULL);

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
        memset(current_buffer, ENDING_CHARACTER, BUFFER_SIZE);
        queue_push(buffer_queue, current_buffer);

        retries = 0;
        retry:
        received = socket_recv(soc, current_buffer, BUFFER_SIZE);
        errsv = errno;
        if (received < 0) {
            fprintf(stderr, "Error receiving part of the buffer, retrying...\n");
            retries++;
            if (retries < RETRIES_THRESHOLD && errsv == EAGAIN) {
                goto retry;
            } else {
                errno = errsv;
                perror("socket get message -> retry");
                goto error;
            }
        } else if (received == 0) {
            fprintf(stderr, "Error, seems that the connection is closed.\n");
            goto error;
        }

        if (current_buffer[BUFFER_SIZE - 1] == ENDING_CHARACTER) { /* The message have been all received */
            received = strlen(current_buffer);
            finished = 1;
        }

        current_size += received;
    } while (finished == 0);

    *buffer = (void *) concat_buffers(buffer_queue);
    *buff_size = strlen(*buffer);

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with receiving message\n");
    if (*buffer != NULL) {
        free(*buffer);
    }
    *buffer = NULL;
    *buff_size = 0;
    current_size = 0;

    terminate:
    queue_destructor(buffer_queue, 1);
    return current_size;
}

int socket_send(socket_t *soc, const void *buffer, size_t buffer_len) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    int val, errsv;
    again:
    val = send(soc->socket_descriptor, buffer, buffer_len, MSG_NOSIGNAL | MSG_DONTWAIT);
    errsv = errno;

    if (val <= 0) {
        if (errno == EINTR || errno == EAGAIN) goto again;
        perror("socket send");
    }

    errno = errsv;
    return val;
}

int socket_send_message(socket_t *soc, void *buffer, size_t buffer_len) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    int sent, total_sent = 0;
    int retries;
    int errsv;
    do {
        retries = 0;

        retry:
        sent = socket_send(soc, buffer + total_sent, min(buffer_len - total_sent, BUFFER_SIZE));
        errsv = errno;
        if (sent < 0) {
            perror("socket send message");
            retries++;
            if (retries < RETRIES_THRESHOLD && errsv == EAGAIN) {
                fprintf(stderr, "Error sending part of the buffer, retrying ...\n");
                goto retry;
            } else {
                fprintf(stderr, "Fatal error sending part of the buffer, aborting ... \n");
                goto error;
            }
        } else if (sent == 0) {
            fprintf(stderr, "Error sending part of the buffer, seems that the connection is closed.\n");
            goto error;
        }
        total_sent += sent;
    } while (total_sent < buffer_len && retries < RETRIES_THRESHOLD);

#ifdef DEBUG
    do {
        const char *emsg = "Buffer sent: [%%.%lus]\n";
        char *msg = malloc(BUFFER_SIZE);
        if (msg != NULL) {
            sprintf(msg, emsg, buffer_len);
            ERR(msg, buffer);
            free(msg);
        }
    } while (0);
#endif

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with sending message\n");
    total_sent = 0;

    terminate:
    return total_sent;
}

void socket_close(socket_t *soc) {
    assert(soc != NULL);
    ERR("closing socket: %d\n", soc->socket_descriptor);

    if (close(soc->socket_descriptor) == -1) {
        fprintf(stderr, "Error trying to close socket %d\n", soc->socket_descriptor);
        perror("socket_close");
    }

    socket_t *parent = get_parent(soc);
    FD_CLR(soc->socket_descriptor, &parent->fd_set.data.set);

    soc->is_closed = 1;
}

void socket_destructor(socket_t *soc) {
    assert(soc != NULL);

    if (!soc->is_closed) {
        socket_close(soc);
    }
    free(soc);
}

#ifdef __cplusplus
}
#endif
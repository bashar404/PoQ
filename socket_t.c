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

#ifdef _WIN32
#include <Windows.h>
#else

#include <unistd.h>

#endif

#ifndef NDEBUG
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

#define BUFFER_SIZE 1024

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

    int ret;
    if ((ret = listen(soc->socket_descriptor, max_connections)) < 0) goto error;
    return ret;

    error:
    perror("socket listen");
    return -1;
}

socket_t *socket_accept(socket_t *soc) {
    assert(soc != NULL);

    int new_socket_fd;
    if ((new_socket_fd = accept(soc->socket_descriptor, (struct sockaddr *) &(soc->address),
                                (socklen_t * ) & (soc->addrlen))) < 0)
        goto error;

    socket_t *new_socket = malloc(sizeof(socket_t));
    if (new_socket == NULL) goto error;
    memcpy(new_socket, soc, sizeof(socket_t));
    new_socket->socket_descriptor = new_socket_fd;

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

    int valread = recv(soc->socket_descriptor, buffer, buffer_len, 0);

    return valread;
}

char *concat_buffers(queue_t *queue) {
    size_t expected_size = queue_size(queue) * BUFFER_SIZE;

    char *buffer = malloc(expected_size);
    if (buffer == NULL) {
        perror("malloc");
        goto error;
    }
    memset(buffer, 0, expected_size);

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
    fprintf(stderr, "Fatal error, can not proceed with receiving message");
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
    int finished = 0;
    do {
        char *current_buffer = malloc(BUFFER_SIZE);
        if (current_buffer == NULL) {
            perror("malloc");
            goto error;
        }
        memset(current_buffer, 0, BUFFER_SIZE);
        queue_push(buffer_queue, current_buffer);

        retry:
        received = socket_recv(soc, current_buffer, BUFFER_SIZE);
        if (received < 0) {
            fprintf(stderr, "Error receiving part of the buffer, retrying...\n");
            goto retry;
        } else if (received == 0) {
            fprintf(stderr, "Error receiving part of the buffer, seems that the connection is closed.\n");
            goto error;
        }

        if (current_buffer[BUFFER_SIZE - 1] == 0) { /* The message have been all received */
            received = strlen(current_buffer);
            finished = 1;
        }

        current_size += received;
    } while (finished == 0);

    *buffer = (void *) concat_buffers(buffer_queue);
    *buff_size = strlen(*buffer);

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with receiving message");

    terminate:
    queue_destructor(buffer_queue, 1);
    return current_size;
}

int socket_send(socket_t *soc, const void *buffer, size_t buffer_len) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    return send(soc->socket_descriptor, buffer, buffer_len, 0);
}

int socket_send_message(socket_t *soc, void *buffer, size_t buffer_len) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    int sent, total_sent = 0;
    do {
        retry:
        sent = socket_send(soc, buffer + total_sent, buffer_len - total_sent);
        if (sent < 0) {
            fprintf(stderr, "Error sending part of the buffer, retrying...\n");
            goto retry;
        } else if (sent == 0) {
            fprintf(stderr, "Error sending part of the buffer, seems that the connection is closed.\n");
            goto error;
        }
        total_sent += sent;
    } while (total_sent < buffer_len);

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with sending message");
    total_sent = 0;

    terminate:
    return total_sent;
}

void socket_close(socket_t *soc) {
    assert(soc != NULL);
    ERR("closing socket: %d\n", soc->socket_descriptor);

    close(soc->socket_descriptor);
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
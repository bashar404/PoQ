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

#include "socket_t.h"

#define DEFAULT_ADDRESS INADDR_ANY

// TODO: set C preprocessor conditionals for SSL

socket_t* socket_constructor(int domain, int type, int protocol, char *ip, int port) {
    socket_t *s = (socket_t *) malloc(sizeof(socket_t));
    if (s == NULL) goto error;

    memset(s, 0, sizeof(socket_t));

    s->socket_descriptor = socket(domain, type, protocol);
    if (s->socket_descriptor == 0) goto error;

    if (setsockopt(s->socket_descriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &(s->opt), sizeof(s->opt)) ) goto error;

    struct sockaddr_in *address = &(s->address);
    address->sin_family = domain;
    address->sin_port = htons( port );

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

int socket_bind(socket_t *soc){
    assert(soc != NULL);

    int ret;
    if ((ret = bind(soc->socket_descriptor, (struct sockaddr *)&(soc->address), sizeof(soc->address))) < 0) goto error;
    return ret;

    error:
    perror("socket bind");
    return ret;
}

int socket_listen(socket_t *soc, int max_connections){
    assert(max_connections > 0);
    assert(soc != NULL);

    int ret;
    if ((ret = listen(soc->socket_descriptor, max_connections)) < 0) goto error;
    return ret;

    error:
    perror("socket listen");
    return -1;
}

socket_t* socket_accept(socket_t *soc){
    assert(soc != NULL);

    int new_socket_fd;
    if ((new_socket_fd = accept(soc->socket_descriptor, (struct sockaddr *)&(soc->address),
                                (socklen_t*)&(soc->addrlen))) <0) goto error;

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

int socket_recv(socket_t *soc, void *buffer, int buffer_len){
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    int valread = recv(soc->socket_descriptor, buffer, buffer_len, 0);

    return valread;
}

int socket_get_message(socket_t *soc, void **buffer, size_t *buff_size) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buff_size != NULL);

    char num_buff[MSG_BYTES_SIZE + 1];
    memset(num_buff, 0, sizeof(num_buff));
    size_t buffer_size = 0;

    int valread = socket_recv(soc, num_buff, MSG_BYTES_SIZE);
    if (valread < MSG_BYTES_SIZE) {
        fprintf(stderr, "Message size is not %d bytes or socket was closed\n", valread);
        goto err_general;
    }

    sscanf(num_buff, "%lu", &buffer_size);

    if (buffer_size == 0) {
        fprintf(stderr, "Buffer size is invalid: %lu\n", buffer_size);
        valread = -1;
        goto err_general;
    }

    char *msg_buffer = malloc(buffer_size +1);
    if (msg_buffer == NULL) goto err_not_enough_mem;
    memset(msg_buffer, 0, buffer_size +1);

    size_t sum = 0;
    do {
        valread = socket_recv(soc, msg_buffer + sum, buffer_size - sum);
        assert(sum <= buffer_size);
        if (valread == 0 && sum < buffer_size) {
            fprintf(stderr, "Communication ended unexpectedly, expected msg of size %lu bytes and got %lu\n", buffer_size, sum);
            goto err_general;
        }
        if (valread  < 0) {
            fprintf(stderr, "Communication closed unexpectedly\n");
            goto err_general;
        }
        sum += valread;
    } while(sum < buffer_size);

    *buffer = msg_buffer;
    *buff_size = buffer_size;
    return 0;

    err_not_enough_mem:
    fprintf(stderr, "Not enough memory to allocate buffer of size: %lu\n", buffer_size);

    err_general:
    perror("socket get message");
    if (msg_buffer != NULL) {
        free(msg_buffer);
    }
    *buffer = NULL;
    return valread;
}

int socket_send(socket_t *soc, const void *buffer, size_t buffer_len){
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    return send(soc->socket_descriptor, buffer, buffer_len, 0);
}

int socket_send_message(socket_t *soc, void *buffer, size_t buffer_len) {
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);
    assert(buffer_len < pow(10, MSG_BYTES_SIZE)); /* must be less than 10^(size of message bytes in string) */

    size_t blen = buffer_len + MSG_BYTES_SIZE;
    char *fbuff = malloc(blen +1);
    if (fbuff == NULL) goto err_not_enough_mem;
    memset(fbuff, 0, blen +1);

    sprintf(fbuff, "%" MSG_BYTES_SIZE_CSTR "lu", buffer_len);
    memcpy(fbuff + MSG_BYTES_SIZE, buffer, buffer_len);

    int valsent;
    size_t sum = 0;
    do {
        valsent = socket_send(soc, fbuff + sum, blen - sum);
        if (valsent == 0 && sum < blen) {
            fprintf(stderr, "Message could not be sent successfully");
            goto err_general;
        }
        if (valsent < 0) {
            fprintf(stderr, "Communication closed unexpectedly\n");
            goto err_general;
        }
        sum += valsent;
    } while(sum < blen);

    err_not_enough_mem:
    fprintf(stderr, "Not enough memory to allocate buffer of size: %lu\n", blen +1);

    err_general:
    perror("socket send message");
    if (fbuff != NULL) {
        free(fbuff);
    }
    return -1;
}

void socket_close(socket_t *soc) {
    assert(soc != NULL);
    ERR("closing socket: %d\n", soc->socket_descriptor);

    close(soc->socket_descriptor);
}

void socket_destructor(socket_t *soc){
    assert(soc != NULL);

    socket_close(soc);
    free(soc);
}

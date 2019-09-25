#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <assert.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h>
#endif

#include "socket_t.h"

#define DEFAULT_ADDRESS INADDR_ANY

// TODO: set C preprocessor conditionals for SSL

socket_t* socket_constructor(int domain, int type, int protocol, char *ip, int port) {
    socket_t *s = (socket_t *) malloc(sizeof(socket_t));
    if (s == NULL) goto error;

    memset(s, 0, sizeof(socket_t));

    s->file_descriptor = socket(domain, type, protocol);
    if (s->file_descriptor == 0) goto error;

    if (setsockopt(s->file_descriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
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
    if ((ret = bind(soc->file_descriptor, (struct sockaddr *)&(soc->address), sizeof(soc->address))) < 0) goto error;
    return ret;

    error:
    perror("socket bind");
    return -1;
}

int socket_listen(socket_t *soc, int max_connections){
    assert(max_connections > 0);
    assert(soc != NULL);

    int ret;
    if ((ret = listen(soc->file_descriptor, max_connections)) < 0) goto error;
    return ret;

    error:
    perror("socket listen");
    return -1;
}

int socket_accept(socket_t *soc){
    assert(soc != NULL);

    int new_socket;
    if ((new_socket = accept(soc->file_descriptor, (struct sockaddr *)&(soc->address),
                             (socklen_t*)&(soc->addrlen))) <0) goto error;

    return new_socket;
    error:
    perror("socket accept");
    return new_socket;
}

int socket_read(socket_t *soc, void *buffer, int buffer_len){
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    int valread = read(soc->file_descriptor, buffer, buffer_len);

    return valread;
}

int socket_send(socket_t *soc, const void *buffer, int buffer_len){
    assert(soc != NULL);
    assert(buffer != NULL);
    assert(buffer_len > 0);

    return send(soc->file_descriptor, buffer, buffer_len, 0);
}

void socket_destructor(socket_t *soc){
    assert(soc != NULL);

    free(soc);
}

#ifndef POET_CODE_SOCKET_T_H
#define POET_CODE_SOCKET_T_H

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>

struct socket_t {
    struct sockaddr_in address;
    socklen_t addrlen;
    int file_descriptor;
    int domain;
    int type;
    int protocol;
    int opt;
};

typedef struct socket_t socket_t;

socket_t* socket_constructor(int domain, int type, int protocol, char *ip, int port);
int socket_bind(socket_t *soc);
int socket_listen(socket_t *soc, int max_connections);
int socket_accept(socket_t *soc);
int socket_read(socket_t *soc, void *buffer, int buffer_len);
int socket_send(socket_t *soc, const void *buffer, int buffer_len);
void socket_destructor(socket_t *soc);

#endif //POET_CODE_SOCKET_T_H

#ifndef POET_CODE_SOCKET_T_H
#define POET_CODE_SOCKET_T_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>

#define STR1(x)  #x
#define STR(x)  STR1(x)

#define MSG_BYTES_SIZE 4
#define MSG_BYTES_SIZE_CSTR STR(MSG_BYTES_SIZE)

struct socket_t {
    struct sockaddr_in address;
    socklen_t addrlen;
    int socket_descriptor;
    int domain;
    int type;
    int protocol;
    int opt;
    int is_closed;
};

typedef struct socket_t socket_t;

socket_t *socket_constructor(int domain, int type, int protocol, const char *ip, int port);

int socket_bind(socket_t *soc);

int socket_listen(socket_t *soc, int max_connections);

socket_t *socket_accept(socket_t *soc);

int socket_connect(socket_t *soc);

int socket_recv(socket_t *soc, void *buffer, int buffer_len);

int socket_get_message(socket_t *soc, void **buffer, size_t *buff_size);

int socket_send(socket_t *soc, const void *buffer, size_t buffer_len);

int socket_send_message(socket_t *soc, void *buffer, size_t buffer_len);

void socket_close(socket_t *soc);

void socket_destructor(socket_t *soc);

#ifdef __cplusplus
}
#endif

#endif //POET_CODE_SOCKET_T_H
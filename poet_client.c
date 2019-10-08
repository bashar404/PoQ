#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "socket_t.h"
#include "general_structs.h"

#ifndef NDEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#define ERR(...) /**/
#endif

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFFER_SZ 100000

socket_t *node_socket = NULL;

void global_variable_initialization() {
    node_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, PORT);
}

void global_variable_destructors() {
    socket_destructor(node_socket);
}

int main(int argc, char *argv[]) {
    // TODO: receive parameters by command line

    global_variable_initialization();

    ERR("trying to establish connection with server\n");
    socket_connect(node_socket);
    ERR("Connection established\n");

    char *buffer = malloc(2048);
    size_t len;

    public_key_t pk;
    signature_t sign;
    sprintf(buffer, "{\"method\":\"register\", \"data\":{\"public_key\":\"%s\", \"signature\":\"%s\"}}",
            encode_hex(&pk, sizeof(pk)),
            encode_hex(&sign, sizeof(sign)));
    len = strlen(buffer);

    socket_send_message(node_socket, buffer, len);

    global_variable_destructors();

    return EXIT_SUCCESS;
}

// TODO
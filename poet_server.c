#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include "socket_t.h"
#include "queue_t.h"
#include "poet_node_t.h"

#ifndef NDEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#define ERR(...) /**/
#endif

#define MAX_THREADS 20
#define MAX_NODES 10000
#define TRUE 1
#define FALSE 0

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define PORT 9000
#define IP "127.0.0.1"

/********** GLOBAL VARIABLES **********/
pthread_t threads[MAX_THREADS];

queue_t *queue = NULL;
pthread_mutex_t queue_lock;

// TODO: add sgx table struct
node_t sgx_table[MAX_NODES];
pthread_mutex_t sgx_table_lock;

socket_t *server_socket = NULL;

int current_time;
/********** GLOBAL VARIABLES **********/

void global_variables_initialization() {
    queue = queue_constructor();
    server_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, IP, PORT);

    if (queue == NULL || server_socket == NULL) {
        perror("queue or socket constructor");
        goto error;
    }

    if (socket_bind(server_socket) != FALSE) {
        perror("socket bind");
        goto error;
    }

    if (pthread_mutex_init(&queue_lock, NULL) != FALSE) {
        perror("queue_lock init");
        goto error;
    }

    if (pthread_mutex_init(&sgx_table_lock, NULL) != FALSE) {
        perror("sgx_table_lock init");
        goto error;
    }

    current_time = 0;

    return;

    error:
    fprintf(stderr, "Failure in global variable initialization\n");
    exit(EXIT_FAILURE);
}

void global_variables_destruction() {
    queue_destructor(queue);
    socket_destructor(server_socket);

    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&sgx_table_lock);
}

int main(int argc, char *argv[]) {
    // TODO: receive command line arguments for variable initialization

    global_variables_initialization();

    printf("queue: %p | server_socket: %p\n", queue, server_socket);

    global_variables_destruction();

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h>
#endif

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
#define BUFFER_SZ 1024

struct thread_tuple {
    pthread_t *thread;
    void *data;
};

/********** GLOBAL VARIABLES **********/
pthread_t threads[MAX_THREADS];
queue_t *threads_queue = NULL;
pthread_mutex_t threads_queue_lock;

queue_t *queue = NULL;
pthread_mutex_t queue_lock;

node_t sgx_table[MAX_NODES];
pthread_mutex_t sgx_table_lock;

socket_t *server_socket = NULL;

int current_time;
/********** GLOBAL VARIABLES **********/

void global_variables_initialization() {
    queue = queue_constructor();
    threads_queue = queue_constructor();
    server_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, IP, PORT);

    if (queue == NULL || server_socket == NULL || threads_queue == NULL) {
        perror("queue, socket or threads_queue constructor");
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

    if (pthread_mutex_init(&threads_queue_lock, NULL) != FALSE) {
        perror("threads_queue_lock init");
        goto error;
    }

    current_time = 0;
    for(int i = 0; i < MAX_THREADS; i++) {
        queue_push(threads_queue, threads+i);
    }

    return;

    error:
    fprintf(stderr, "Failure in global variable initialization\n");
    exit(EXIT_FAILURE);
}

void global_variables_destruction() {
    queue_destructor(queue, 1);
    queue_destructor(threads_queue, 0);
    socket_destructor(server_socket);

    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&sgx_table_lock);
}

int received_termination_signal() { // dummy function
    return FALSE;
}

void* process_new_node(void *arg) {
    struct thread_tuple* curr_thread = (struct thread_tuple*) arg;
    socket_t *node_socket = (socket_t *) curr_thread->data;
    ERR("Processing node in thread: %p and socket fd: %d\n", curr_thread->thread, node_socket->file_descriptor);

    // TODO

    char buffer[BUFFER_SZ];
    socket_read(node_socket, buffer, BUFFER_SZ);

    buffer[BUFFER_SZ-1] = '\0';
    pthread_t current_thread = pthread_self();
    printf("thread: %lu | received msg: '%s'\n", current_thread, buffer);

    char *s = "su puta madre es asi, cierto?";
    strcpy(buffer, s);

    socket_send(node_socket, buffer, strlen(buffer));

    socket_destructor(node_socket);

    pthread_mutex_lock(&threads_queue_lock);
    queue_push(threads_queue, curr_thread->thread);
    pthread_mutex_unlock(&threads_queue_lock);
    free(curr_thread);
}

int main(int argc, char *argv[]) {
    // TODO: receive command line arguments for variable initialization

    global_variables_initialization();

    ERR("queue: %p | server_socket: %p\n", queue, server_socket);

    if (socket_listen(server_socket, MAX_THREADS) != FALSE) {
        goto error;
    }
    ERR("Starting to listen\n");

    while(received_termination_signal() == FALSE) { // TODO: change condition to an OS signal
        socket_t *new_socket = socket_accept(server_socket);
        if (new_socket == NULL) {
            fprintf(stderr, "Error accepting socket connection\n");
            continue;
        }

        while(queue_is_empty(threads_queue)) { // FIXME: remove active waiting of threads
            // nothing
        }

        pthread_t *next_thread = (pthread_t *) queue_front(threads_queue);
        queue_pop(threads_queue);
        struct thread_tuple * curr_thread = malloc(sizeof(struct thread_tuple));
        curr_thread->thread = next_thread;
        curr_thread->data = new_socket;
        int error = pthread_create(next_thread, NULL, &process_new_node, curr_thread);
        if (error != FALSE) {
            fprintf(stderr, "A thread could not be created: %p\n", next_thread);
        }
    }

    global_variables_destruction();
    return 0;

    error:
    fprintf(stderr, "server finished with error\n");
    global_variables_destruction();
    return EXIT_FAILURE;
}
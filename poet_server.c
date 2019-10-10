#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#ifndef _WIN32

#include <unistd.h>
#include <json-parser/json.h>

#else
#include <Windows.h>
#endif

#include "socket_t.h"
#include "queue_t.h"
#include "poet_node_t.h"
#include "general_structs.h"
#include "poet_server_functions.h"

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
#define SERVER_IP "0.0.0.0"
#define BUFFER_SZ 1024

struct thread_tuple {
    pthread_t *thread;
    void *data;
};

/********** GLOBAL VARIABLES **********/
pthread_t threads[MAX_THREADS];
queue_t *threads_queue = NULL;

queue_t *queue = NULL;

node_t sgx_table[MAX_NODES];
pthread_mutex_t sgx_table_lock;

socket_t *server_socket = NULL;

time_t current_time = 0;

int should_terminate = 0;

/********** GLOBAL VARIABLES **********/

void global_variables_initialization() {
    queue = queue_constructor();
    threads_queue = queue_constructor();
    server_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, PORT);

    if (queue == NULL || server_socket == NULL || threads_queue == NULL) {
        perror("queue, socket or threads_queue constructor");
        goto error;
    }

    if (socket_bind(server_socket) != FALSE) {
        perror("socket bind");
        goto error;
    }

    if (pthread_mutex_init(&sgx_table_lock, NULL) != FALSE) {
        perror("sgx_table_lock init");
        goto error;
    }

    current_time = time(NULL); // take current time

    for (int i = 0; i < MAX_THREADS; i++) {
        queue_push(threads_queue, threads + i);
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

    pthread_mutex_destroy(&sgx_table_lock);
}

// Define the function to be called when ctrl-c (SIGINT) signal is sent to process
void signal_callback_handler(int signum) {
    should_terminate = 1;
    global_variables_destruction();
    fprintf(stderr, "Caught signal %d\n",signum);
    exit(SIGINT);
}

int received_termination_signal() { // dummy function
    return should_terminate;
}

/* Check whether the message has the intended JSON structure for communication */
int check_message_integrity(json_value *json) {
    assert(json != NULL);
    int valid = 1;

    valid = valid && json->type == json_object;
    valid = valid && json->u.object.length >= 2;
    valid = valid && strcmp(json->u.object.values[0].name, "method") == 0;
    valid = valid && json->u.object.values[0].value->type == json_string;

    if (valid) {
        char *s = json->u.object.values[0].value->u.string.ptr;
        int found = 0;
        for (struct function_handle *i = functions; i->name != NULL && !found; i++) {
            found = found || strcmp(s, i->name) == 0;
        }

        valid = valid && found;
    }

    valid = valid && strcmp(json->u.object.values[1].name, "data") == 0;

    return valid;
}

int delegate_message(char *buffer, size_t buffer_len, socket_t *soc) {
    /* FIXME: this assumes that the JSON is well formed, the opposite can happen */
    json_value *json = json_parse(buffer, buffer_len);

    int ret = EXIT_SUCCESS;

    if (!check_message_integrity(json)) {
        fprintf(stderr, "JSON format of message doesn't have a valid format for communication\n");
        goto error;
    }

    fprintf(stderr, "JSON message is valid\n");
    char *func_name = json->u.object.values[0].value->u.string.ptr;
    struct function_handle *function = NULL;
    for (struct function_handle *i = functions; i->name != NULL && function == NULL; i++) {
        function = strcmp(func_name, i->name) == 0 ? i : NULL;
    }
    assert(function != NULL);

    printf("Calling: %s\n", function->name);

    ret = function->function(json->u.object.values[1].value, soc);
    goto terminate;

    error:
    fprintf(stderr, "method delegation finished with failure\n");
    ret = EXIT_FAILURE;

    terminate:
    json_value_free(json);
    return ret;
}

void *process_new_node(void *arg) {
    struct thread_tuple *curr_thread = (struct thread_tuple *) arg;
    socket_t *node_socket = (socket_t *) curr_thread->data;
    ERR("Processing node in thread: %p and socket %3d\n", curr_thread->thread, node_socket->socket_descriptor);

    char *buffer = NULL;
    size_t buffer_size = 0;

    int socket_state;
    socket_state = socket_get_message(node_socket, (void *) &buffer, &buffer_size);

    while (socket_state > 0) {
        printf("message received from socket %d on thread %p\n: \"%s\"\n",
               node_socket->socket_descriptor,
               curr_thread->thread,
               buffer);

        if (delegate_message(buffer, buffer_size, node_socket) != 0) {
            goto error;
        }

        free(buffer);
        buffer = NULL;

        socket_state = socket_get_message(node_socket, (void *) &buffer, &buffer_size);
    }

    if (socket_state < 0) {
        fprintf(stderr, "error receiving message from socket %d on thread %p\n",
                node_socket->socket_descriptor,
                curr_thread->thread);
    } else {
        printf("Connection was closed in socket %d on thread %p\n",
               node_socket->socket_descriptor,
               curr_thread->thread);
    }

    error:
    if (buffer != NULL) {
        free(buffer);
    }
    socket_destructor(node_socket);
    queue_push(threads_queue, curr_thread->thread);
    free(curr_thread);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // TODO: receive command line arguments for variable initialization

    signal(SIGINT, signal_callback_handler);

    global_variables_initialization();

    ERR("queue: %p | server_socket: %p\n", queue, server_socket);

    if (socket_listen(server_socket, MAX_THREADS) != FALSE) {
        goto error;
    }
    printf("Starting to listen\n");

    while (received_termination_signal() == FALSE) { // TODO: change condition to an OS signal
        socket_t *new_socket = socket_accept(server_socket);
        if (new_socket == NULL) {
            fprintf(stderr, "Error accepting socket connection\n");
            continue;
        }

        // FIXME: Figure out a way to eliminate active waiting in here
        int checking;
        do {
            checking = queue_is_empty(threads_queue);
        } while (checking);

        pthread_t *next_thread = (pthread_t *) queue_front(threads_queue);
        queue_pop(threads_queue);
        struct thread_tuple *curr_thread = malloc(sizeof(struct thread_tuple));
        curr_thread->thread = next_thread;
        curr_thread->data = new_socket;

        int error = pthread_create(next_thread, NULL, &process_new_node, curr_thread);
        if (error != FALSE) {
            fprintf(stderr, "A thread could not be created: %p (error code: %d)\n", next_thread, error);
            perror("thread creation");
            exit(EXIT_FAILURE);
        }
        /* To avoid a memory leak of pthread, since there is a thread queue we dont want a pthread_join */
        pthread_detach(*next_thread);
    }

    global_variables_destruction();
    return EXIT_SUCCESS;

    error:
    fprintf(stderr, "server finished with error\n");
    global_variables_destruction();
    return EXIT_FAILURE;
}
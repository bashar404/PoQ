#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cassert>
#include <csignal>
#include <vector>

#ifndef _WIN32

#include <unistd.h>

#else
#include <Windows.h>
#endif

#define MAX_THREADS 20
#define MAX_CONNECTIONS MAX_THREADS
#define THREAD_RETRIES_THRESHOLD 100
#define THREAD_RETRY_WAIT 2
#define MAX_NODES 10000
#define TRUE 1
#define FALSE 0

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define PORT 9000
#define SERVER_IP "0.0.0.0"
#define BUFFER_SZ 1024

#include "socket_t.h"
#include "queue_t.h"
#include "general_structs.h"
#include "poet_server_functions.h"
#include "poet_shared_functions.h"
#include "json_checks.h"

struct thread_tuple {
    pthread_t *thread;
    void *data;
};

/********** GLOBAL VARIABLES **********/
int should_terminate = 0;

pthread_t threads[MAX_THREADS];
queue_t *threads_queue = nullptr;

/********** PoET variables **********/

queue_t *queue = nullptr;
pthread_rwlock_t queue_lock = PTHREAD_RWLOCK_INITIALIZER;

std::vector<node_t *> sgx_table;
pthread_rwlock_t sgx_table_lock = PTHREAD_RWLOCK_INITIALIZER;

socket_t *server_socket = nullptr;

time_t current_time = 0;
uint current_id = 0;

size_t sgxt_lowerbound;
size_t sgxmax;
uint n_tiers;

/********** PoET variables END **********/

/********** GLOBAL VARIABLES END **********/

static void global_variables_initialization() {
    queue = queue_constructor();
    threads_queue = queue_constructor();
    server_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, PORT);

    if (queue == nullptr || server_socket == nullptr || threads_queue == nullptr) {
        perror("queue, socket or threads_queue constructor");
        goto error;
    }

    if (socket_bind(server_socket) != FALSE) {
        goto error;
    }

    current_time = time(nullptr); // FIXME: maybe not necessary

    for (int i = 0; i < MAX_THREADS; i++) {
        queue_push(threads_queue, threads + i);
    }

    return;

    error:
    fprintf(stderr, "Failure in global variable initialization\n");
    exit(EXIT_FAILURE);
}

static void global_variables_destruction() {
    queue_destructor(queue, 1);
    queue_destructor(threads_queue, 0);
    socket_destructor(server_socket);
}

// Define the function to be called when ctrl-c (SIGINT) signal is sent to process
static void signal_callback_handler(int signum) {
    should_terminate = 1;
//    global_variables_destruction();
    fprintf(stderr, "Caught signal %d\n", signum);

    bool queue_lock_locked = pthread_rwlock_tryrdlock(&queue_lock) == 0;
    if (queue_lock_locked) {
        pthread_rwlock_unlock(&queue_lock);
    }
    printf("queue_lock is locked: %d\n", !queue_lock_locked);

    bool sgx_table_locked = pthread_rwlock_tryrdlock(&sgx_table_lock) == 0;
    if (sgx_table_locked) {
        pthread_rwlock_unlock(&sgx_table_lock);
    }
    printf("sgx_table_lock is locked: %d\n", !sgx_table_locked);
    exit(SIGINT);
}

static int received_termination_signal() { // dummy function
    return should_terminate;
}

/* Check whether the message has the intended JSON structure for communication */
static bool check_message_integrity(json_value *json) {
    assert(json != nullptr);
    bool valid = true;

    valid = valid && json->type == json_object;
    valid = valid && json->u.object.length >= 2;

    json_value *json_method = (valid ? find_value(json, "method") : nullptr);
    valid = valid && json_method != nullptr;
    valid = valid && json_method->type == json_string;

    if (valid) {
        char *s = json_method->u.string.ptr;
        int found = 0;
        for (struct function_handle *i = poet_functions; i->name != nullptr && !found; i++) {
            found = found || strcmp(s, i->name) == 0;
        }

        valid = valid && found;
    }

    valid = valid && find_value(json, "data") != nullptr;

    return valid;
}

static bool delegate_message(char *buffer, size_t buffer_len, socket_t *soc, poet_context *context) {
    json_value *json = nullptr;
    bool ret = true;
    struct function_handle *function = nullptr;
    char *func_name = nullptr;

    if (!check_json_compliance(buffer, buffer_len)) {
        fprintf(stderr, "JSON format of message doesn't have a valid format for communication\n");
        goto error;
    }

    json = json_parse(buffer, buffer_len);

    if (!check_message_integrity(json)) {
        fprintf(stderr, "JSON format of message doesn't have a valid format for communication\n");
        goto error;
    }

    ERR("JSON message is valid\n");
    func_name = find_value(json, "method")->u.string.ptr;

    for (struct function_handle *i = poet_functions; i->name != nullptr && function == nullptr; i++) {
        function = strcmp(func_name, i->name) == 0 ? i : nullptr;
    }
    assert(function != nullptr);

    ret = function->function(find_value(json, "data"), soc, context);
    goto terminate;

    error:
    fprintf(stderr, "method delegation for function '%10s...' finished with failure\n", func_name);
    ret = false;

    terminate:
    if (json != nullptr) {
        json_value_free(json);
    }
    return ret;
}

static void *process_new_node(void *arg) {
    auto *curr_thread = (struct thread_tuple *) arg;
    auto *node_socket = (socket_t *) curr_thread->data;
    ERR("Processing node in thread: %p(0x%lx) and socket %3d\n", curr_thread->thread, *(curr_thread->thread), node_socket->socket_descriptor);

    char *buffer = nullptr;
    size_t buffer_size = 0;
    struct poet_context context{};

    int socket_state;
    socket_state = socket_get_message(node_socket, (void **) &buffer, &buffer_size);

    while (socket_state > 0) {
        ERR("message received from socket %d on thread %p(0x%lx)\n: \"%s\"\n",
            node_socket->socket_descriptor,
            curr_thread->thread,
            *(curr_thread->thread),
            buffer);

        if (!delegate_message(buffer, buffer_size, node_socket, &context)) {
            fprintf(stderr, "Could not delegate message from socket %d\n", node_socket->socket_descriptor);
            goto error;
        }

        if (node_socket->is_closed) {
            ERR("The socket %d was intentionally closed by server.\n", node_socket->socket_descriptor);
            goto error;
        }

        free(buffer);
        buffer = nullptr;

        socket_state = socket_get_message(node_socket, (void **) &buffer, &buffer_size);
    }

    if (socket_state == 0 || node_socket->is_closed) {
        fprintf(stderr, "Connection was closed in socket %d on thread %p(0x%lx)\n",
                node_socket->socket_descriptor,
                curr_thread->thread,
                *(curr_thread->thread));
    } else {
        fprintf(stderr, "error receiving message from socket %d on thread %p(0x%lx)\n",
                node_socket->socket_descriptor,
                curr_thread->thread,
                *(curr_thread->thread));
    }

    error:
    if (buffer != nullptr) {
        free(buffer);
    }
    free_poet_context(&context);
    socket_destructor(node_socket);
    queue_push(threads_queue, curr_thread->thread);
    free(curr_thread);

    pthread_exit(nullptr);
}

void set_global_constants() {
    printf("Enter SGXt lowerbound: ");
    scanf("%lu", &sgxt_lowerbound);

    printf("Enter SGXt upperbound: ");
    scanf("%lu", &sgxmax);

    printf("Enter number of tiers: ");
    scanf("%u", &n_tiers);

    if (sgxt_lowerbound >= sgxmax || sgxt_lowerbound == 0 || sgxmax == 0) {
        fprintf(stderr, "invalid sgxt lowerbound and upperbound\n");
        exit(EXIT_FAILURE);
    }

    if (n_tiers == 0) {
        fprintf(stderr, "invalid number of tiers\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // TODO: receive command line arguments for variable initialization

    signal(SIGINT, signal_callback_handler);

    global_variables_initialization();

    set_global_constants();

    ERR("queue: %p | server_socket: %p\n", queue, server_socket);

    if (socket_listen(server_socket, MAX_CONNECTIONS) != FALSE) {
        goto error;
    }
    printf("Starting to listen\n");

    while (received_termination_signal() == FALSE) { // TODO: change condition to an OS signal
        socket_t *new_socket = socket_select(server_socket);
        if (new_socket == nullptr) {
            printf(".");
            continue;
        }

        // FIXME: Figure out a way to eliminate active waiting in here
        bool checking;
        uint retries = 0;
        do {
            checking = queue_is_empty(threads_queue);
            retries++;
            if (retries > THREAD_RETRIES_THRESHOLD) {
                fprintf(stderr, "The thread queue is full, waiting %d seconds\n", THREAD_RETRY_WAIT);
                sleep(THREAD_RETRY_WAIT);
                retries = 0;
            }
        } while (checking && received_termination_signal() == FALSE);

        if (received_termination_signal() != FALSE) {
            break;
        }

        auto *next_thread = (pthread_t *) queue_front(threads_queue);
        queue_pop(threads_queue);
        auto *curr_thread = (struct thread_tuple *) malloc(sizeof(struct thread_tuple));
        curr_thread->thread = next_thread;
        curr_thread->data = new_socket;

        int error = pthread_create(next_thread, nullptr, &process_new_node, curr_thread);
        if (error != FALSE) {
            perror("thread creation");
            fprintf(stderr, "A thread could not be created: %p(0x%lx) (error code: %d)\n", next_thread, *next_thread, error);
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
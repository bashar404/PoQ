#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cassert>
#include <csignal>
#include <vector>
#include <map>

#ifndef _WIN32

#include <unistd.h>
#include <map>
#include <json-parser/json.h>

#else
#include <Windows.h>
#endif

#include "socket_t.h"
#include "queue_t.h"
#include "general_structs.h"
#include "poet_common_definitions.h"
#include "poet_server_functions.h"
#include "poet_shared_functions.h"

#define MAX_NODES 10000
#define MAX_THREADS 20
#define MAX_CONNECTIONS MAX_THREADS
#define THREAD_RETRIES_THRESHOLD 100
#define THREAD_RETRY_WAIT 2
#define TRUE 1
#define FALSE 0

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define MAIN_PORT 9000
#define SECONDARY_PORT 9001
#define SERVER_IP "0.0.0.0"

/********** GLOBAL VARIABLES **********/
int should_terminate = 0;

pthread_t threads[MAX_THREADS];
queue_t *threads_queue = nullptr;

/********** PoET variables **********/
struct global g;

/********** PoET variables END **********/

/********** GLOBAL VARIABLES END **********/

static void global_variables_initialization() {
    g.queue = queue_constructor();
    threads_queue = queue_constructor();
    g.server_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, MAIN_PORT);
    g.secondary_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, SECONDARY_PORT);

    if (g.queue == nullptr || g.server_socket == nullptr || threads_queue == nullptr || g.secondary_socket == nullptr) {
        perror("queue, socket or threads_queue constructor");
        goto error;
    }

    if (socket_bind(g.server_socket) != FALSE) {
        goto error;
    }

    if (socket_bind(g.secondary_socket) != FALSE) {
        goto error;
    }

    g.server_starting_time = time(nullptr); // FIXME: maybe not necessary

    for (int i = 0; i < MAX_THREADS; i++) {
        queue_push(threads_queue, threads + i);
    }

    return;

    error:
    ERROR("Failure in global variable initialization\n");
    exit(EXIT_FAILURE);
}

static void global_variables_destruction() {
    queue_destructor(g.queue, 1);
    queue_destructor(threads_queue, 0);
    socket_destructor(g.server_socket);
}

// Define the function to be called when ctrl-c (SIGINT) signal is sent to process
static void signal_callback_handler(int signum) {
    should_terminate = 1;
//    global_variables_destruction();
    INFO("Caught signal %d\n", signum);

    bool sgx_table_locked = pthread_mutex_trylock(&g.sgx_table_lock) == 0;
    if (sgx_table_locked) {
        pthread_mutex_unlock(&g.sgx_table_lock);
    }
    INFO("sgx_table_lock is locked: %d\n", !sgx_table_locked);
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
        WARN("JSON format of message doesn't have a valid format for communication\n");
        goto error;
    }

    json = json_parse(buffer, buffer_len);

    if (!check_message_integrity(json)) {
        ERROR("JSON format of message doesn't have a valid format for communication\n");
        goto error;
    }

    ERRR("JSON message is valid\n");
    func_name = find_value(json, "method")->u.string.ptr;

    for (struct function_handle *i = poet_functions; i->name != nullptr && function == nullptr; i++) {
        function = strcmp(func_name, i->name) == 0 ? i : nullptr;
    }
    assert(function != nullptr);

    ret = function->function(find_value(json, "data"), soc, context);
    goto terminate;

    error:
    ERROR("method delegation for function '%10s...' finished with failure\n", func_name);
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
    ERR("Processing node in thread: %p(0x%lx) and socket %3d\n", curr_thread->thread, *(curr_thread->thread),
        node_socket->socket_descriptor);

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
            ERROR("Could not delegate message from socket %d\n", node_socket->socket_descriptor);
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
        ERROR("Connection was closed in socket %d on thread %p(0x%lx)\n",
              node_socket->socket_descriptor,
              curr_thread->thread,
              *(curr_thread->thread));
    } else {
        ERROR("error receiving message from socket %d on thread %p(0x%lx)\n",
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
    scanf("%lu", &g.sgxt_lowerbound);

    printf("Enter SGXt upperbound: ");
    scanf("%lu", &g.sgxmax);

    printf("Enter number of tiers: ");
    scanf("%u", &g.n_tiers);

    if (g.sgxt_lowerbound >= g.sgxmax || g.sgxt_lowerbound == 0 || g.sgxmax == 0) {
        ERROR("invalid sgxt lowerbound and upperbound\n");
        exit(EXIT_FAILURE);
    }

    if (g.n_tiers == 0 || g.n_tiers > (g.sgxmax - g.sgxt_lowerbound)) {
        ERROR("invalid number of tiers\n");
        exit(EXIT_FAILURE);
    }
}

static void *asyncronous_send_message(void *arg) {
    auto curr_thread = (struct thread_tuple *) arg;

    auto ptr_lst = (void **) curr_thread->data;
    auto socket = (socket_t *) ptr_lst[0];
    auto buffer = (char *) ptr_lst[1];
    auto len = (size_t) ptr_lst[2];

    ERR("Sending message to socket %d with the updated data: [%.*s]\n", socket->socket_descriptor, std::min(150, (int)len), buffer);

    int sent = socket_send_message(socket, buffer, len);
    if (sent <= 0) {
        ERROR("failed to send message to secondary socket %d\n", socket->socket_descriptor);
    }

    free(ptr_lst);

    queue_push(threads_queue, curr_thread->thread);
    free(curr_thread);

    pthread_exit(nullptr);
}

static void *sgx_table_and_queue_notification(void *_) {
    int ret = 1;
    do {
        ret = queue_wait_change(g.queue); // wait until there is a change in the nodes queue
        ERR("There was a change on the queue, sending message to all subscribers ...\n");
        ret = 0; // ignore return status

        assertp(pthread_mutex_lock(&g.sgx_table_lock) == 0);
        std::string qs = std::move(get_queue_str());
        std::string sgxt_str = std::move(get_sgx_table_str(false));
        pthread_mutex_unlock(&g.sgx_table_lock);

        int state = 1;
        time_t curr_time = time(nullptr);

        char *buffer = (char *) calloc(1, qs.length() + sgxt_str.length() + BUFFER_SIZE);
        state = state && buffer != nullptr;
        if (state) {
            uint written = 0;
            written += sprintf(buffer, R"({"data":{"queue": )");
            strcat(buffer + written, qs.c_str());
            written += qs.length();
            strcat(buffer + written, R"(, "sgx_table": )");
            strcat(buffer + written, sgxt_str.c_str());
            written += sgxt_str.length();
            strcat(buffer + written, "}}");
        }

        size_t len = strlen(buffer);

        if (state) {
            std::queue<pthread_t *> q;

            assertp(pthread_rwlock_rdlock(&g.secondary_socket_comms_lock) == 0);
            for (auto pair = g.secondary_socket_comms.begin(); pair != g.secondary_socket_comms.end(); pair++) {
                auto thread = (pthread_t *) queue_front_and_pop(threads_queue);
                auto ptr_lst = (void **) calloc(3, sizeof(void *));
                ptr_lst[0] = (*pair).second;
                ptr_lst[1] = buffer;
                ptr_lst[2] = (void *) len;
                delegate_thread_to_function(thread, (void *) ptr_lst, asyncronous_send_message, false);
                q.push(thread);
            }
            pthread_rwlock_unlock(&g.secondary_socket_comms_lock);

            while(!q.empty()) {
                pthread_join(*q.front(), nullptr);
                q.pop();
            }
        }

        free(buffer);
    } while (ret == 0);

    pthread_exit(nullptr);
}

static void *process_secondary_node_addition(void *arg) {
    auto *curr_thread = (struct thread_tuple *) arg;
    auto *socket = (socket_t *) curr_thread->data;

    char *buffer = nullptr;
    size_t len;
    int state = socket_get_message(socket, (void **) &buffer, &len) > 0;

    json_value *json = nullptr;

    state = state && check_json_compliance(buffer, len);
    if (state) {
        json = json_parse(buffer, len);
        state = json != nullptr;
        json_value *json_nodeid = nullptr;
        if (state) {
            json_nodeid = find_value(json, "node_id");
            state = json_nodeid != nullptr && json_nodeid->type == json_integer;
        }

        if (state) {
            uint node_id = json_nodeid->u.integer;
            if (g.current_id <= node_id) { // invalid id
                ERR("invalid node id: (current_id) %u <= (node_id) %u\n", g.current_id, node_id);
                state = 0;
            }
            // will only add it if it is a valid id
            if (state)  {
                assertp(pthread_rwlock_wrlock(&g.secondary_socket_comms_lock) == 0);
                g.secondary_socket_comms[node_id] = socket;
                pthread_rwlock_unlock(&g.secondary_socket_comms_lock);
                const char *p = R"({"status":"success"})";
                socket_send_message(socket, (void *) p, strlen(p));
                ERR("Adds node %u into secondary socket message list on socket %d\n", node_id, socket->socket_descriptor);
            } else {
                const char *p = R"({"status":"failure"})";
                socket_send_message(socket, (void *) p, strlen(p));
            }
        }
    }

    if (!state) {
        socket_close(socket);
    }

    json_value_free(json);
    if (buffer != nullptr) {
        free(buffer);
    }

    queue_push(threads_queue, curr_thread->thread);
    free(curr_thread);

    pthread_exit(nullptr);
}

/**
 * Checks new connections on recently added nodes, it delegates to another thread so it doesn't block new connections
 * @param _
 * @return
 */
static void *secondary_socket_sentinel(void *_) {
    ERR("Started to listen on secondary socket\n");
    while (received_termination_signal() == FALSE) {
        int state = socket_select(g.secondary_socket);
        if (!state) {
            printf(",");
            continue;
        }

        socket_t *new_socket = socket_accept(g.secondary_socket);

        ERR("Received new connection on secondary socket %d\n", new_socket->socket_descriptor);

        auto thread = (pthread_t *) queue_front_and_pop(threads_queue);
        delegate_thread_to_function(thread, new_socket, process_secondary_node_addition);
    }
}

int main(int argc, char *argv[]) {
    // TODO: receive command line arguments for variable initialization

    signal(SIGINT, signal_callback_handler);
    global_variables_initialization();
    set_global_constants();

    ERRR("queue: %p | server_socket: %p\n", queue, server_socket);

    if (socket_listen(g.server_socket, MAX_CONNECTIONS) != FALSE) {
        goto error;
    }

    if (socket_listen(g.secondary_socket, MAX_CONNECTIONS) != FALSE) {
        goto error;
    }
    INFO("Starting to listen\n");

    pthread_t secondary_socket_thread;
    assertp(pthread_create(&secondary_socket_thread, nullptr, secondary_socket_sentinel, nullptr) == 0);
    pthread_detach(secondary_socket_thread);

    pthread_t sgx_table_and_queue_notification_thread;
    assertp(pthread_create(&sgx_table_and_queue_notification_thread, nullptr, sgx_table_and_queue_notification, nullptr) == 0);
    pthread_detach(sgx_table_and_queue_notification_thread);


    /* ************************ */

    while (received_termination_signal() == FALSE) {
        int state = socket_select(g.server_socket);
        if (!state) {
            printf(".");
            continue;
        }

        socket_t *new_socket = socket_accept(g.server_socket);

        while(queue_is_empty(threads_queue)) {
            WARN("The thread queue is empty, waiting until a thread finishes and gets added again into the queue.\n");
            queue_wait_change_timed(threads_queue, {1, 0});
        }

        if (received_termination_signal() != FALSE) {
            break;
        }

        auto *next_thread = (pthread_t *) queue_front_and_pop(threads_queue);
        int error = delegate_thread_to_function(next_thread, new_socket, process_new_node);
        if (error != FALSE) {
            perror("thread creation");
            ERROR("A thread could not be created: %p(0x%lx) (error code: %d)\n", next_thread, *next_thread, error);
            exit(EXIT_FAILURE);
        }
    }

    global_variables_destruction();
    return EXIT_SUCCESS;

    error:
    ERROR("server finished with error\n");
    global_variables_destruction();
    return EXIT_FAILURE;
}
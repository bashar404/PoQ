#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <json-parser/json.h>
#include <general_structs.h>
#include <vector>
#include <string>
#include <csignal>
#include <sys/file.h>

#include "socket_t.h"
#include "queue_t.h"
#include "poet_shared_functions.h"
#include "enclave_helper.h"

#include "sgx_error.h"       /* sgx_status_t */
#include "sgx_eid.h"     /* sgx_enclave_id_t */
#include "sgx_urts.h"
#include "enclave_u.h"

#include "poet_common_definitions.h"

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define MAIN_PORT 9000
#define SECONDARY_PORT 9001
#define SERVER_IP "127.0.0.1"

#define BLOCKCHAIN_FILE "blockchain.dat"
#define BLOCKCHAIN_WRITE_TIME 1 /* change */

int should_terminate = 0;
char *server_ip = nullptr;

pthread_t starting_time_calculation_thread;
pthread_t blockchain_writer_thread;

socket_t *node_socket = nullptr;
socket_t *subscribe_socket = nullptr;
bool retry_connection = true;
sgx_enclave_id_t eid = 0;

time_t server_starting_time = 0;
time_t node_current_time = 0;

uint sgxt;
uint sgxmax;
uint ntiers;
uint sgx_lowerbound;
uint node_id;

public_key_t public_key;
signature_t signature;

/* Dynamic variables */
std::vector<node_t *> sgx_table;
pthread_mutex_t sgx_table_lock = PTHREAD_MUTEX_INITIALIZER;
queue_t *queue;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

uint rejoin_state = 0;
cond_mutex_t rejoin_cond = {PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

void global_variable_initialization() {
    server_ip = (char *) malloc(18);
    printf("Set the ip address to communicate (-1 for 127.0.0.1): ");
//    scanf("%18s", server_ip);
    strcpy(server_ip, "-1");
    if (strcmp("-1", server_ip) == 0) {
        strcpy(server_ip, SERVER_IP);
    }

    node_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, server_ip, MAIN_PORT);
    assertp(node_socket != nullptr);
    subscribe_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, server_ip, SECONDARY_PORT);
    assertp(subscribe_socket != nullptr);
    queue = queue_constructor();
    assertp(queue != nullptr);

    /* Initialize the enclave */
    assertp(initialize_enclave(&eid) >= 0);
}

void global_variable_destructors() {
    socket_destructor(node_socket);
    sgx_destroy_enclave(eid);
}

static void fill_with_rand(void *input, size_t len) {
    srand(0);
    auto *ptr = (unsigned char *) input;

    for (int i = 0; i < len; i++) {
        *(ptr + i) = rand() % 256;
    }
//    sgx_status_t ret = ecall_random_bytes(eid, input, len);
//    if (ret != SGX_SUCCESS) {
//        ERROR("Something happened with the enclave :c\n");
//        sgx_print_error_message(ret);
//        exit(EXIT_FAILURE);
//    }
}

static int poet_remote_attestation_to_server() {
    INFO("Starting remote attestation ...\n");
    bool state = true;
#ifdef NO_RA
    char *buffer = (char *) malloc(BUFFER_SIZE);

    sprintf(buffer, R"({"method" : "remote_attestation", "data": null})");
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    socket_send_message(node_socket, buffer, len);

    free(buffer);
    socket_get_message(node_socket, (void **) (&buffer), &len);
    if (buffer == nullptr) {
        return 0;
    }

    state = check_json_compliance(buffer, len) == 1;
    if (state == 0) {
        return 0;
    }

    json_value *json = json_parse(buffer, len);
    json_value *json_status = find_value(json, "status");
    if (json_status == nullptr || json_status->type != json_string ||
        strcmp(json_status->u.string.ptr, "success") != 0) {
        ERROR("Remote attestation was not successful\n");
        state = false;
    } else {
        INFO("Remote attestation was successful\n");
    }

    if (json != nullptr) {
        json_value_free(json);
    }

#else

    // TODO: Remote attestation
    ERROR("Remote attestation still not implemented\n");
    exit(EXIT_FAILURE);

#endif

    return state;
}

static uint generate_random_sgx_time() { // TODO: change for the new version with a tuple
    uint sgx_time = 0;
    sgx_status_t ret = ecall_random_bytes(eid, &sgx_time, sizeof(sgx_time));
    if (ret != SGX_SUCCESS) {
        ERROR("Something happened with the enclave :c\n");
        sgx_print_error_message(ret);
        exit(EXIT_FAILURE);
    }
    sgx_time = sgx_lowerbound + sgx_time % (sgxmax - sgx_lowerbound + 1);

    return sgx_time;
}

static int poet_register_with_pk() {
    char *buffer = (char *) malloc(BUFFER_SIZE);
    int state = 1;

    public_key_t &pk = public_key;
    fill_with_rand(&pk, sizeof(public_key_t)); // TODO: change
    signature_t &sign = signature;
    fill_with_rand(&sign, sizeof(signature_t));

    unsigned char *pk_64base = encode_64base(&pk, sizeof(pk));
    unsigned char *sign_64base = encode_64base(&sign, sizeof(sign));
    if (pk_64base == nullptr || sign_64base == nullptr) {
        ERROR("failure encoding pk\n");
        exit(EXIT_FAILURE);
    }

    sprintf(buffer, R"({"method" : "register", "data": {"public_key": "%s", "signature" : "%s"}})", pk_64base,
            sign_64base);
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    state = socket_send_message(node_socket, buffer, len);

    free(buffer);
    free(pk_64base);
    free(sign_64base);

    state = socket_get_message(node_socket, (void **) (&buffer), &len) > 0;
    state = state && check_json_compliance(buffer, len);

    json_value *json = nullptr;
    json_value *json_status = nullptr;

    if (state) {
        json = json_parse(buffer, len);

        json_status = find_value(json, "status");
        if (json_status == nullptr || json_status->type != json_string ||
            strcmp(json_status->u.string.ptr, "success") != 0) {
            state = 0;
        }
    }

    if (state) {
        json_value *json_tmp = find_value(json, "sgxmax");
        state = json_tmp != nullptr && json_tmp->type == json_integer;
        if (state) sgxmax = json_tmp->u.integer;

        json_tmp = find_value(json, "sgxt_lower");
        state = json_tmp != nullptr && json_tmp->type == json_integer;
        if (state) sgx_lowerbound = json_tmp->u.integer;

        json_tmp = find_value(json, "node_id");
        state = json_tmp != nullptr && json_tmp->type == json_integer;
        if (state) node_id = json_tmp->u.integer;

        json_tmp = find_value(json, "n_tiers");
        state = json_tmp != nullptr && json_tmp->type == json_integer;
        if (state) ntiers = json_tmp->u.integer;

        json_tmp = find_value(json, "server_starting_time");
        state = json_tmp != nullptr && json_tmp->type == json_integer;
        if (state) server_starting_time = json_tmp->u.integer;
    }

    json_value_free(json);
    return state;
}

static int poet_broadcast_sgxtime() {
    char *buffer;
    int state = 1;
    json_value *json = nullptr;
    json_value *json_status = nullptr;

    sgxt = generate_random_sgx_time();
    ERR("SGXt is generated: %u\n", sgxt);

    buffer = (char *) malloc(BUFFER_SIZE);
    if (buffer == nullptr) {
        perror("malloc");
        state = 0;
        goto error;
    }
    sprintf(buffer, R"({"method":"sgx_time_broadcast", "data":{"sgxt": %u}})",
            sgxt); // TODO: should send its identity from the enclave in it
    ERR("Sending SGXt (%u) to the server\n", sgxt);
    state = socket_send_message(node_socket, buffer, strlen(buffer)) > 0;

    free(buffer);
    buffer = nullptr;

    size_t len;
    if (state) { // getting reply of success
        state = socket_get_message(node_socket, (void **) &buffer, &len) > 0;
        state = state && check_json_compliance(buffer, len);
        if (state) {
            json = json_parse(buffer, len);

            json_status = find_value(json, "status");
            if (json_status == nullptr || json_status->type != json_string ||
                strcmp(json_status->u.string.ptr, "success") != 0) {
                state = 0;
            }

            json_value_free(json);
            json = nullptr;
        }
    }

    error:
    if (buffer != nullptr) {
        free(buffer);
    }

    if (json != nullptr) {
        json_value_free(json);
    }

    if (!state) {
        ERROR("Failed to broadcast SGXtime\n");
    }

    return state;
}

static int poet_register_to_server() {
    int state = 1;

    state = state && poet_register_with_pk();

    if (state) {
        ERR("SGXmax (%u), SGXlower (%u) and Node id (%u) is received from the server\n", sgxmax, sgx_lowerbound,
            node_id);
    }

    state = state && poet_remote_attestation_to_server();

    error:
    ERR("Server registration %s\n", state ? "successful" : "failed");

    return state;
}

static std::string node_to_json(const node_t &node) { // move to general methods
    const char *json = node_t_to_json(&node);
    std::string s(json);
    free((void *) json);
    return s;
}

static void print_sgx_table_and_queue() {
    printf("SGX Table:\n---------------------\n");
    for (auto i = sgx_table.begin(); i != sgx_table.end(); i++) {
        printf("%s\n", node_to_json(**i).c_str());
    }
    printf("---------------------\n");

    printf("Queue: ");
    queue_print(queue);
    printf("\n");
}

static bool get_sgx_table_from_json(json_value *json, bool lock = true) {
    assert(json != nullptr);

    bool state = true;
    json_value *json_sgx_table = nullptr;

    json_sgx_table = find_value(json, "sgx_table");
    if (json_sgx_table == nullptr || json_sgx_table->type != json_array) {
        state = false;
    }

    if (state) {
        if (lock) assertp(mutex_locks(&sgx_table_lock));

        for (auto it = sgx_table.begin(); it != sgx_table.end(); it++) {
            assert(*it != nullptr);
            delete *it;
        }
        sgx_table.clear();

        json_value **json_node = json_sgx_table->u.array.begin();
        while (state && json_node != json_sgx_table->u.array.end()) {
            auto node = new node_t();
            state = json_to_node_t(*json_node, node);
            json_node++;
            sgx_table.push_back(node);
        }

        if (lock) mutex_unlocks(mutex_unlocks(&sgx_table_lock));
    }

    return state;
}

static bool get_sgx_table() {
    json_value *json = nullptr;

    char *buffer = (char *) malloc(BUFFER_SIZE);
    sprintf(buffer, R"({"method":"get_sgxtable", "data": null})");
    socket_send_message(node_socket, buffer, strlen(buffer));
    free(buffer);
    buffer = nullptr;

    size_t len;
    bool state = socket_get_message(node_socket, (void **) &buffer, &len) > 0;

    state = state && (json = check_json_success_status(buffer, len)) != nullptr;
    state = state && get_sgx_table_from_json(json);

    if (json != nullptr) {
        json_value_free(json);
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

static bool get_queue_from_json(json_value *json, bool lock = true) {
    bool state = true;
    json_value *json_queue = nullptr;

    json_queue = find_value(json, "queue");
    if (json_queue == nullptr || json_queue->type != json_array) {
        state = false;
    }

    if (state) {
        /* Empty the queue */
        if (lock) assertp(mutex_locks(&queue_lock) != 0);
        queue_destructor(queue, 0);
        queue = queue_constructor();

        json_value **json_node = json_queue->u.array.begin();
        while (state && json_node != json_queue->u.array.end()) {
            uint nid = 0;
            json_value *value = *json_node;
            state = state && value != nullptr && value->type == json_integer;
            nid = value->u.integer;
            json_node++;
            queue_push(queue, (void *) (nid));
        }

        if (lock) mutex_unlocks(&queue_lock);

        if (!state) {
            ERROR("Failed to get queue correctly\n");
        }
    }

    return state;
}

static bool update_sgx_table_and_queue_from_txt(char *buffer, size_t len) {
    bool state = 1;

    state = check_json_compliance(buffer, len);
    json_value *json = nullptr;
    if (state) {
        assertp(mutex_locks(&sgx_table_lock, &queue_lock));
        json = json_parse(buffer, len);
        state = get_queue_from_json(json, false);
        state = state && get_sgx_table_from_json(json, false);
        if (state) {
            node_current_time = time(nullptr) - server_starting_time;
        }
        mutex_unlocks(&sgx_table_lock, &queue_lock);
    }

    ERR("sgx table and queue was updated: %d\n", state);

    return state;
}

static bool get_queue() {
    json_value *json = nullptr;

    char *buffer = (char *) malloc(BUFFER_SIZE);
    sprintf(buffer, R"({"method":"get_queue", "data": null})");
    int state = socket_send_message(node_socket, buffer, strlen(buffer));
    free(buffer);
    buffer = nullptr;

    size_t len;
    state = state && socket_get_message(node_socket, (void **) &buffer, &len) > 0;
    state = state && (json = check_json_success_status(buffer, len)) != nullptr;
    state = state && get_queue_from_json(json);

    if (json != nullptr) {
        json_value_free(json);
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

static bool get_queue_and_sgx_table() {
    json_value *json = nullptr;

    char *buffer = (char *) malloc(BUFFER_SIZE);
    sprintf(buffer, R"({"method":"get_sgxtable_and_queue", "data": null})");
    int state = socket_send_message(node_socket, buffer, strlen(buffer));
    free(buffer);
    buffer = nullptr;

    size_t len;
    state = state && socket_get_message(node_socket, (void **) &buffer, &len) > 0;

    state = state && (json = check_json_success_status(buffer, len)) != nullptr;

    assertp(mutex_locks(&sgx_table_lock, &queue_lock));
    state = state && get_queue_from_json(json, false);
    state = state && get_sgx_table_from_json(json, false);
    mutex_unlocks(&sgx_table_lock, &queue_lock);

    if (json != nullptr) {
        json_value_free(json);
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

static bool server_close_connection() {
    char *buffer = (char *) malloc(BUFFER_SIZE);
    sprintf(buffer, R"({"method":"close_connection", "data": null})");
    int state = socket_send_message(node_socket, buffer, strlen(buffer)) > 0;
    free(buffer);
    buffer = nullptr;

    size_t len;
    state = socket_get_message(node_socket, (void **) &buffer, &len) > 0;

    json_value *json = nullptr;
    state = state && (json = check_json_success_status(buffer, len)) != nullptr;

    if (json != nullptr) {
        json_value_free(json);
    }

    return state;
}

static void signal_callback_handler(int signum) {
    should_terminate = 1;
    ERROR("\nReceived signal %d\n", signum);
//    exit(EXIT_FAILURE);
}

int calculate_necessary_parameters(uint &quantum_time, uint &tier, uint &starting_time) {
    int state = 1;

    auto quantum_times = calc_quantum_times(sgx_table, ntiers, sgxmax, node_current_time, server_starting_time);

    tier = calc_tier_number(*sgx_table[node_id], ntiers, sgxmax);
    quantum_time = quantum_times[tier];
    starting_time = calc_starting_time(queue, sgx_table, *sgx_table[node_id], ntiers, sgxmax, node_current_time,
                                       server_starting_time); // TODO remove: not used

    return state;
}

static void get_input_from_user_and_write(FILE *f) {
    char name[20], address[20];
    printf("Enter name: ");
    fgets(name, 20, stdin);

    printf("Enter address: ");
    fgets(address, 20, stdin);

//    fputs(name, f);
    fprintf(f, "[Node %d] %20s", node_id, name);
    fprintf(f, "[Node %d] %20s", node_id, address);
//    fputs(address, f);
}

static bool notify_readdition_of_node_into_queue(int time_left) {
    assert(time_left > 0);
    WARN("Time left to notify to the server: %u\n", time_left);

    bool state = true;

    char *buffer = (char *) malloc(BUFFER_SIZE);
    node_t tmp_node{};
    memcpy(&tmp_node, sgx_table[node_id], sizeof(node_t));
    tmp_node.time_left = time_left;
    assert(tmp_node.sgx_time > time_left);

    std::string str = node_to_json(tmp_node);
    sprintf(buffer, R"({"method":"unfinished_node", "data": %s})", str.c_str());
    state = socket_send_message(node_socket, buffer, strlen(buffer)) > 0;
    free(buffer);
    buffer = nullptr;

    size_t len = 0;
    state = state && socket_get_message(node_socket, (void **) &buffer, &len);
    state = state && buffer != nullptr;
    state = state && check_json_success_status(buffer, len);

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

static bool notify_leadership_of_node() {
    // TODO
    ERROR("method not implemented\n");
}

static void *blockchain_work(void *arg) { // thread 4
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

    notify_leadership_of_node();
    uint execution_time = (uint) (long) arg;

    FILE *f;
    assertp((f = fopen(BLOCKCHAIN_FILE, "a")) != nullptr);
    assertp(flock(fileno(f), LOCK_EX) == 0); // exclusive access to file

    time_t curr_time = time(nullptr);
    struct tm time_tm{};
    char buf[80];
    time_tm = *localtime(&curr_time);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &time_tm);

    fprintf(f, "Node %u starts writing at %s (%lu)\n", node_id, buf, curr_time);

    sleep(execution_time);
//    get_input_from_user_and_write(f);

    curr_time = time(nullptr);
    time_tm = *localtime(&curr_time);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &time_tm);
    fprintf(f, "Node %u finishes writing at %s (%lu)\n", node_id, buf, curr_time);

    flock(fileno(f), LOCK_UN); // unlocks file
    fclose(f);

    rejoin_state++;
    pthread_cond_signal(&rejoin_cond.cond);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
}

static void *starting_time_calculation(void *arg) { // thread 3
    // TODO: calculate the starting time of this node
    int oldstate;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    assertp(mutex_locks(&sgx_table_lock, &queue_lock));

    print_sgx_table_and_queue(); // TODO delete

    if (node_id >= sgx_table.size()) {
        ERROR("The received SGXtable is not the most recent\n");
        pthread_exit(nullptr);
    }

    auto notification_times = calc_notification_times(queue, sgx_table, *sgx_table[node_id], ntiers, sgxmax,
                                                      node_current_time, server_starting_time);
    time_t leadership_time = calc_leadership_time(queue, sgx_table, *sgx_table[node_id], ntiers, sgxmax,
                                                  node_current_time, server_starting_time);
    mutex_unlocks(&sgx_table_lock, &queue_lock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);

    if (leadership_time == -1) {
        rejoin_state++;
        pthread_cond_signal(&rejoin_cond.cond); // should join again since something went wrong
        ERROR("something went wrong and we have to rejoin\n");
        pthread_exit(nullptr);
    }

    if (!notification_times.empty()) {
        ERR("last notification time <= leadership time: %d\n", notification_times.back() <= leadership_time);
        ERR("last notification time: %ld <= leadership time: %lu\n", notification_times.back(), leadership_time);
    }

    assert(notification_times.empty() || notification_times.back() <= leadership_time);
    for (auto &notification_time : notification_times) {
        ERR("%ld\n", notification_time);
    }
    fprintf(stderr, "\n");
    INFO("Leadership time: %ld\n", leadership_time);

    while (!notification_times.empty() && notification_times.back() == leadership_time) {
        notification_times.pop_back();
    }

    int curr_time = 0;
    while (!notification_times.empty() && notification_times.front() == curr_time) {
        notification_times.erase(notification_times.begin());
    }

    int remaining_time = sgx_table[node_id]->time_left;
    for (auto &notification_time : notification_times) {
        assert(curr_time < notification_time);
        sleep(notification_time - curr_time);
        remaining_time -= notification_time;
        curr_time = notification_time;

        WARN("Notifying server of readdition: %d\n", curr_time);
        if (remaining_time > 0) {
            notify_readdition_of_node_into_queue(remaining_time);
        }
    }

    sleep(leadership_time - curr_time);

    /* ****** BECOMES LEADER ****** */

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
    pthread_create(&blockchain_writer_thread, nullptr, blockchain_work, (void *) (BLOCKCHAIN_WRITE_TIME));
    pthread_detach(blockchain_writer_thread);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);

    INFO("Started blockchain write job at: %s (%lu)\n", "[PENDING]", time(nullptr));

    pthread_exit(nullptr);
}

void *notifications_sentinel(void *_) { // thread 2
    ERR("Started to listen on secondary socket\n");
    char *buffer = nullptr;
    size_t len;
    while (should_terminate == 0) {
        int valread = socket_get_message(subscribe_socket, (void **) &buffer, &len);
        if (valread < 0) {
            ERRR("Empty message from secondary socket\n");
            continue;
        }

        ERR("Broadcast message received in node %d\n", node_id);

        int r = pthread_cancel(starting_time_calculation_thread);
        int errsv = errno;
        if (r) {
            ERRR("The thread `%s' could not be canceled: %s(%d)\n", STR(starting_time_calculation_thread),
                 strerror(errsv), errsv);
        }

        bool updated = false;
        if (buffer != nullptr) updated = update_sgx_table_and_queue_from_txt(buffer, len);

        if (buffer != nullptr) free(buffer);
        buffer = nullptr;
        len = 0;

        if (updated) {
            ERR("Calling starting time calculation function\n");
            assertp(pthread_create(&starting_time_calculation_thread, nullptr, starting_time_calculation, nullptr) ==
                    0);
        } else {
            ERR("starting time calculation function is NOT being called\n");
            sleep(1);
        }
    }

    pthread_exit(nullptr);
}

static bool connect_to_server() {
    static bool first_time = true;

    bool connected = false;
    bool connect1 = false;
    bool connect2 = false;
    int retries = 0;
    if (retry_connection) {
        do {
            connect1 = connect1 ? true : socket_connect_retry(node_socket) == 0;
            connect2 = connect2 ? true : socket_connect_retry(subscribe_socket) == 0;
            connected = connect1 && connect2;
        } while (!connected && first_time);
    } else {
        assertp(socket_connect(node_socket) == 0);
        assertp(socket_connect(subscribe_socket) == 0);
    }

    first_time = false;

    return connected;
}

static bool setup_secondary_socket() {
    bool state = true;
    // doing registration on subscriber channel
    char *buffer = (char *) malloc(BUFFER_SIZE);
    assertp(buffer != nullptr);
    sprintf(buffer, R"({"node_id": %u})", node_id);
    state = socket_send_message(subscribe_socket, buffer, strlen(buffer)) > 0;
    free(buffer);

    buffer = nullptr;
    size_t len;
    state = socket_get_message(subscribe_socket, (void **) &buffer, &len) > 0;
    ERR("Server responded with: '%s'(%lu)\n", buffer, len);
    free(buffer);
    return state;
}

int main(int argc, char *argv[]) {
    // TODO: receive parameters by command line

    signal(SIGINT, signal_callback_handler);

    global_variable_initialization();
    atexit(global_variable_destructors);

    ERR("trying to establish connection with server\n");
    bool connected = connect_to_server();

    ERR("connection established\n");

    bool state = true;
    assertp(poet_register_to_server());
    assertp(setup_secondary_socket());

    pthread_t notifications_checker_thread;
    assertp(pthread_create(&notifications_checker_thread, nullptr, notifications_sentinel, nullptr) == 0);
    pthread_detach(notifications_checker_thread);

    while (should_terminate != 1) {
        uint initial_state = rejoin_state;

        state = poet_broadcast_sgxtime();

        assertp(pthread_mutex_lock(&rejoin_cond.mutex) == 0);
        struct timespec dt{};
        while (state && initial_state == rejoin_state) {
            dt.tv_sec = time(nullptr) + 1; // must be absolute rather than relative
            dt.tv_nsec = 0;
            int val;
            if ((val = pthread_cond_timedwait(&rejoin_cond.cond, &rejoin_cond.mutex, &dt)) != 0) {
                ERRR("val: %d (%s)\n", val, strerror(val));
            }

            if (initial_state == rejoin_state) {
                ERRR("cond timedwait timedout\n");
            }
        }
        pthread_mutex_unlock(&rejoin_cond.mutex);
        ERR("received signal to generate new sgx time\n");

        if (!state) {
            static int retries = 0;
            ERROR("something went wrong in the poet_broadcast_time\n");
            if (retries > 10) {
//                should_terminate = 1;
                /* trying to reconnect to the server */
                should_terminate = !(connect_to_server() && setup_secondary_socket());
                if (!should_terminate) {
                    retries = 0;
                }
            }
            retries++;
        }
    }

//    state = state && get_queue_and_sgx_table();

//    state = state && server_close_connection();

//    printf("Enter character to exit (%lu).\n", eid);
//    getchar();

//    printf("Ctrl+C to destroy enclave (%lu).\n", eid);
//    sleep(2*60);

    terminate:
    socket_close(node_socket);
    printf("Ending node\n");

    return EXIT_SUCCESS;
}

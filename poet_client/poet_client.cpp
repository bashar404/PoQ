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

#include "socket_t.h"
#include "queue_t.h"
#include "poet_shared_functions.h"
#include "json_checks.h"
#include "enclave_helper.h"

#include "sgx_error.h"       /* sgx_status_t */
#include "sgx_eid.h"     /* sgx_enclave_id_t */
#include "sgx_urts.h"
#include "enclave_u.h"

#include "poet_common_definitions.h"

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define PORT 9000
#define SERVER_IP "127.0.0.1"

int should_terminate = 0;
char *server_ip = nullptr;

socket_t *node_socket = nullptr;
sgx_enclave_id_t eid = 0;

time_t server_current_time;

uint sgxt;
uint sgxmax;
uint ntiers;
uint sgx_lowerbound;
uint node_id;

public_key_t public_key;
signature_t signature;

/* Dynamic variables */
std::vector<node_t *> sgx_table;
queue_t *queue;

void global_variable_initialization() {
    server_ip = (char *) malloc(18);
    printf("Set the ip address to communicate (-1 for 127.0.0.1): ");
    scanf("%18s", server_ip);
    if (strcmp("-1", server_ip) == 0) {
        strcpy(server_ip, SERVER_IP);
    }

    node_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, server_ip, PORT);
    queue = queue_constructor();

    /* Initialize the enclave */
    if (initialize_enclave(&eid) < 0) {
        fprintf(stderr, "Could not create Enclave\n");
        exit(EXIT_FAILURE);
    }
}

void global_variable_destructors() {
    socket_destructor(node_socket);
    sgx_destroy_enclave(eid);
}

static void fill_with_rand(void *input, size_t len) {
//    srand(time(nullptr));
//    auto *ptr = (unsigned char *) input;
//
//    for (int i = 0; i < len; i++) {
//        *(ptr + i) = rand() % 256;
//    }
    sgx_status_t ret = ecall_random_bytes(eid, input, len);
    if (ret != SGX_SUCCESS) {
        fprintf(stderr, "Something happened with the enclave :c\n");
        sgx_print_error_message(ret);
        exit(EXIT_FAILURE);
    }
}

static int poet_remote_attestation_to_server() {
    printf("Starting remote attestation ...\n");
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
        fprintf(stderr, "Remote attestation was not successful\n");
        state = false;
    } else {
        printf("Remote attestation was successful\n");
    }

    if (json != nullptr) {
        json_value_free(json);
    }

#else

    // TODO: Remote attestation
    fprintf(stderr, "Remote attestation still not implemented\n");
    exit(EXIT_FAILURE);

#endif

    return state;
}

static uint generate_random_sgx_time() {
    uint sgx_time = 0;
    sgx_status_t ret = ecall_random_bytes(eid, &sgx_time, sizeof(sgx_time));
    if (ret != SGX_SUCCESS) {
        fprintf(stderr, "Something happened with the enclave :c\n");
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
        fprintf(stderr, "failure encoding pk\n");
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

    size_t len;
    if (state) { // getting reply of success
        socket_get_message(node_socket, (void **) &buffer, &len);
        state = check_json_compliance(buffer, len);
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

    return state;
}

static int poet_register_to_server() {
    int state = 1;

    state = state && poet_register_with_pk();

    if (state)
        ERR("SGXmax (%u), SGXlower (%u) and Node id (%u) is received from the server\n", sgxmax, sgx_lowerbound,
               node_id);

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

static bool get_sgx_table_from_json(json_value *json) {
    assert(json != nullptr);

    bool state = true;
    json_value *json_sgx_table = nullptr;

    json_sgx_table = find_value(json, "sgx_table");
    if (json_sgx_table == nullptr || json_sgx_table->type != json_array) {
        state = false;
    }

    if (state) {
        for(auto it = sgx_table.begin(); it != sgx_table.end(); it++) {
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

    state = state && check_json_compliance(buffer, len);
    if (state) {
        json = json_parse(buffer, len);
        json_value *json_status = find_value(json, "status");
        if (json_status == nullptr || json_status->type != json_string ||
            strcmp(json_status->u.string.ptr, "success") != 0) {
            state = false;
        }
    }

    state = state && get_sgx_table_from_json(json);

    if (json != nullptr) {
        json_value_free(json);
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

static bool get_queue_from_json(json_value *json) {
    bool state = true;
    json_value *json_queue = nullptr;

    json_queue = find_value(json, "queue");
    if (json_queue == nullptr || json_queue->type != json_array) {
        state = false;
    }

    if (state) {
        /* Empty the queue */
        queue_destructor(queue, 0);
        queue = queue_constructor();

        json_value **json_node = json_queue->u.array.begin();
        while (state && json_node != json_queue->u.array.end()) {
            uint nid = 0;
//            json_value *value = find_value(*json_node, "node_id");
            json_value *value = *json_node;
            state = state && value != nullptr && value->type == json_integer;
            nid = value->u.integer;
            json_node++;
            queue_push(queue, (void *) (nid));
        }

        if (!state) {
            fprintf(stderr, "Failed to get queue correctly\n");
        }
    }

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

    state = state && check_json_compliance(buffer, len);
    if (state) {
        json = json_parse(buffer, len);
        json_value *json_status = find_value(json, "status");
        if (json_status == nullptr || json_status->type != json_string ||
            strcmp(json_status->u.string.ptr, "success") != 0) {
            state = 0;
        }
    }

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

    state = state && check_json_compliance(buffer, len);
    if (state) {
        json = json_parse(buffer, len);
        json_value *json_status = find_value(json, "status");
        if (json_status == nullptr || json_status->type != json_string ||
            strcmp(json_status->u.string.ptr, "success") != 0) {
            state = 0;
        }
    }

    if (state) {
        json_value *json_current_time = find_value(json, "current_time");
        state = json_current_time != nullptr && json_current_time->type == json_integer;
        if (state) server_current_time = json_current_time->u.integer;
    }

    state = state && get_queue_from_json(json);
    state = state && get_sgx_table_from_json(json);

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
    int state = socket_send_message(node_socket, buffer, strlen(buffer));
    free(buffer);
    buffer = nullptr;

    size_t len;
    state = socket_get_message(node_socket, (void **) &buffer, &len) > 0;
    state = state && check_json_compliance(buffer, len);
    if (state) {
        json_value *json = json_parse(buffer, len);
        json_value *status = find_value(json, "status");
        if (status != nullptr && status->type == json_string) {
            state = strcmp(status->u.string.ptr, "success") == 0;
        }

        json_value_free(json);
    }

    return state;
}

static void signal_callback_handler(int signum) {
    should_terminate = 1;
    printf("\nReceived signal %d\n", signum);
}

int calculate_necessary_parameters(uint &quantum_time, uint &tier, uint &starting_time) {
    int state = 1;

    auto quantum_times = calc_quantum_times(sgx_table, ntiers, sgxmax);

    tier = calc_tier_number(*sgx_table[node_id], ntiers, sgxmax);
    quantum_time = quantum_times[tier];
    starting_time = calc_starting_time(queue, sgx_table, *sgx_table[node_id], ntiers, sgxmax);

    return state;
}

int main(int argc, char *argv[]) {
    // TODO: receive parameters by command line

    signal(SIGINT, signal_callback_handler);

    global_variable_initialization();

    ERR("trying to establish connection with server\n");
    socket_connect(node_socket);
    ERR("Connection established\n");

    int state = poet_register_to_server();
    if (!state) {
        fprintf(stderr, "Something failed ...\n");
    }

    while(!should_terminate && state) {
        state = poet_broadcast_sgxtime();
        if (state) {
            get_queue_and_sgx_table();
            uint quantum_time, tier, starting_time;
            calculate_necessary_parameters(quantum_time, tier, starting_time);
            printf("%u, %u, %u, %lu\n", quantum_time, tier, starting_time, sgx_table.size());

            // TODO finish
        }

        should_terminate = 1;
    }

    state = state && get_queue_and_sgx_table();
    if (state) {
        printf("SGX Table:\n---------------------\n");
        for (auto i = sgx_table.begin(); i != sgx_table.end(); i++) {
            printf("%s\n", node_to_json(**i).c_str());
        }
        printf("---------------------\n");

        printf("Queue: ");
        queue_print(queue);
    }

    state = state && server_close_connection();

//    printf("Enter character to exit (%lu).\n", eid);
//    getchar();

//    printf("Ctrl+C to destroy enclave (%lu).\n", eid);
//    sleep(2*60);

    terminate:
    socket_close(node_socket);
    global_variable_destructors();
    printf("Ending node\n");

    return EXIT_SUCCESS;
}

#include "poet_server_functions.h"
#include "poet_functions.h"
#include "general_structs.h"
#include "queue_t.h"
#include <cstdio>
#include <cstring>
#include <cassert>

#include <map>
#include <string>
#include <vector>

#define FUNC_PAIR(NAME)  { #NAME, poet_ ## NAME }

#define BUFFER_SIZE 2048

extern queue_t *queue;
extern pthread_rwlock_t queue_lock;
extern std::vector<node_t *> sgx_table;
extern pthread_rwlock_t sgx_table_lock;

extern time_t current_time;
pthread_rwlock_t current_time_lock = PTHREAD_RWLOCK_INITIALIZER;

extern uint current_id;
pthread_rwlock_t current_id_lock = PTHREAD_RWLOCK_INITIALIZER;

extern size_t sgxmax;
extern size_t sgxt_lowerbound;

std::map<std::string, uint> public_keys;
pthread_rwlock_t public_keys_lock = PTHREAD_RWLOCK_INITIALIZER;

// TODO: create two functions to lock and unlock all specified locks depending on their memory address to avoid deadlocks

/** Checks if Public key and Signature is valid (for now just checks if its non-zero) and is not already registered */
static bool check_public_key_and_signature_registration(const std::string &pk_str, const std::string &sign_str,
                                                        poet_context *context) {
    void *buff;

    pthread_rwlock_rdlock(&public_keys_lock);
    bool valid = public_keys.count(pk_str) == 0; // is not already registered

    if (!valid) {
        uint node = public_keys[pk_str];
        fprintf(stderr, "This PK is already registered for node %u\n", node);
    }
    pthread_rwlock_unlock(&public_keys_lock);

    context->public_key = new public_key_t();
    context->signature = new signature_t();

    public_key_t &pk = *(context->public_key);
    size_t buff_len;
    if (valid) {
        buff = decode_64base(pk_str.c_str(), pk_str.length(), &buff_len);
        if (buff == nullptr || buff_len != sizeof(public_key_t)) {
            fprintf(stderr, "public key has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len,
                    sizeof(public_key_t));
            valid = false;
        }
        memcpy(&pk, buff, sizeof(pk));
        if (buff != nullptr) {
            free(buff);
        }
    }

    signature_t &sign = *(context->signature);
    if (valid) {
        buff = decode_64base(sign_str.c_str(), sign_str.length(), &buff_len);
        if (buff == nullptr || buff_len != sizeof(signature_t)) {
            fprintf(stderr, "signature has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len,
                    sizeof(signature_t));
            valid = false;
        }
        memcpy(&sign, buff, sizeof(sign));
        if (buff != nullptr) {
            free(buff);
        }
    }

    auto *pk_8 = (uint8_t *) &pk;
    size_t len = sizeof(public_key_t);
    for (int i = 0; i < len && valid; i++) {
        valid = valid || *(pk_8 + i) != 0;
    }

    auto *sign_8 = (uint8_t *) &sign;
    len = sizeof(signature_t);
    for (int i = 0; i < len && valid; i++) {
        valid = valid || *(sign_8 + i) != 0;
    }

    return valid;
}

int poet_register(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool valid = true;
    char *msg = nullptr;
    void *buff = nullptr;
    char *sign_64base = nullptr;
    size_t sign_64base_len = 0;
    char *pk_64base = nullptr;
    size_t pk_64base_len = 0;
    json_value *sign_json = nullptr;
    if (context->node == nullptr) context->node = new node_t();

    fprintf(stderr, "Register method is called.\n");

    /**************************************/

    json_value *pk_json = find_value(json, "public_key");
    if (pk_json == nullptr || pk_json->type != json_string) {
        fprintf(stderr, "public key from node is not valid or is not present\n");
        goto error;
    }
    pk_64base = pk_json->u.string.ptr;

    sign_json = find_value(json, "signature");
    if (sign_json == nullptr || sign_json->type != json_string) {
        fprintf(stderr, "signature from node is not valid or is not present\n");
        goto error;
    }
    sign_64base = sign_json->u.string.ptr;

    if (check_public_key_and_signature_registration(std::string(pk_64base), std::string(sign_64base), context)) {
        pthread_rwlock_wrlock(&current_id_lock);
        pthread_rwlock_wrlock(&public_keys_lock);

        context->node->node_id = current_id++;
        public_keys.insert({std::string(pk_64base), context->node->node_id});

        pthread_rwlock_unlock(&public_keys_lock);
        pthread_rwlock_unlock(&current_id_lock);
    } else {
        fprintf(stderr, "The PK or the Signature is not valid, closing connection ...\n");
        valid = false;
        goto error;
    }

    /*****************************/

    msg = (char *) malloc(BUFFER_SIZE);
    sprintf(msg, R"({"status":"success", "data": {"sgxmax" : %lu, "sgxt_lower": %lu, "node_id" : %u}})", sgxmax,
            sgxt_lowerbound, context->node->node_id);
    printf("Server is sending sgxmax (%lu) to the node\n", sgxmax);
    socket_send_message(socket, msg, strlen(msg));
    free(msg);

    goto terminate;

    error:
    socket_close(socket);

    terminate:
    return valid;
}

int poet_remote_attestation(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    fprintf(stderr,
            "Remote Attestation method is called\n");

#ifdef NO_RA
    char *buffer = (char *) malloc(BUFFER_SIZE);

    sprintf(buffer,
            R"({"status":"success"})");
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    socket_send_message(socket, buffer, len);
    free(buffer);
#else

    // TODO: Remote attestation
    assert(false);

#endif

    return true;
}

static bool insert_node_into_sgx_table_and_queue(node_t &node) {
    ERR("Adding node (ID: %u, SGXt: %u, At: %u, TL: %u, NOL: %u) into SGX table\n", node.node_id,
        node.sgx_time, node.arrival_time, node.time_left, node.n_leadership);

    pthread_rwlock_wrlock(&sgx_table_lock);
    sgx_table.push_back(&node);
    assert(sgx_table.back() == &node);
    pthread_rwlock_unlock(&sgx_table_lock);

    queue_push(queue, &node);

    ERR("Inserted node (ID: %u, SGXt: %u, At: %u, TL: %u, NOL: %u) into the SGX table and Queue\n", node.node_id,
        node.sgx_time, node.arrival_time, node.time_left, node.n_leadership);

    return true;
}

int poet_sgx_time_broadcast(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    char *msg = nullptr;
    uint sgxt = 0;
    bool valid = true;

    json_value *json_sgxt = find_value(json, "sgxt");
    if (json_sgxt == nullptr || json_sgxt->type != json_integer) {
        valid = false;
    }

    if (valid) {
        sgxt = json_sgxt->u.integer;
        printf("SGXt is received and checking the validity.\n");
    }

    valid = valid && (sgxt_lowerbound <= sgxt && sgxt <= sgxmax);
    printf("SGXt is%s valid: %s (%u)\n", (valid ? "" : " not"), (valid ? "true" : "false"), sgxt);

    if (valid) {
        node_t &node = *(context->node);
        node.arrival_time = time(nullptr) - current_time;
        node.n_leadership = 0;
        node.sgx_time = sgxt;
        node.time_left = sgxt;
        valid = insert_node_into_sgx_table_and_queue(node);
    }

    msg = (char *) malloc(BUFFER_SIZE);
    sprintf(msg, R"({"status":"%s"})", (valid ? "success" : "failure"));
    socket_send_message(socket, msg, strlen(msg));
    free(msg);
    msg = nullptr;

    if (!valid) {
        fprintf(stderr, "Something happened, closing connection ... \n");
        socket_close(socket);
    }

    return valid;
}

static std::string get_sgx_table_str() {
    std::string sgx_table_str = "[";

    char *buffer = (char *) malloc(BUFFER_SIZE);

    pthread_rwlock_rdlock(&sgx_table_lock);
    for (auto i = sgx_table.begin(); i != sgx_table.end(); i++) {
        const node_t &node = *(*i);
        sprintf(buffer, R"({"node_id": %u, "sgx_time": %u, "arrival_time": %u, "time_left": %u, "n_leadership": %u},)",
                node.node_id,
                node.sgx_time, node.arrival_time, node.time_left, node.n_leadership);
        sgx_table_str.append(buffer);
    }
    pthread_rwlock_unlock(&sgx_table_lock);

    free(buffer);

    if (sgx_table_str.back() == ',') sgx_table_str.pop_back(); // delete the trailing comma if present
    sgx_table_str.append("]");

    return sgx_table_str;
}

int poet_get_sgxtable(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;
    std::string str = get_sgx_table_str();

    char *buffer = (char *) malloc(str.length() + BUFFER_SIZE);
    if (buffer == nullptr) {
        state = false;
    }

    if (state) {
        sprintf(buffer, R"({"status":"success", "data":{"sgx_table": %s}})", str.c_str());
    } else {
        sprintf(buffer, R"({"status":"failure"})");
    }

    state = state && (socket_send_message(socket, buffer, strlen(buffer)) > 0);

    if (!state) {
        uint node_id = (context->node != nullptr) ? context->node->node_id : -1;
        fprintf(stderr, "Could not send sgx_table to node %u.\n", node_id);
    }

    if (buffer != nullptr) {
        free(buffer);
    }
    return state;
}

static std::string node_to_json(const node_t &node) { // move to general methods
    char *buffer = (char *) malloc(BUFFER_SIZE);
    sprintf(buffer, R"({"node_id": %u, "sgx_time": %u, "arrival_time": %u, "time_left": %u, "n_leadership": %u})",
            node.node_id,
            node.sgx_time, node.arrival_time, node.time_left, node.n_leadership);
    std::string s(buffer);
    free(buffer);
    return s;
}

static std::string queue_str;

static void print_queue_value_into_buffer(void *d) {
    if (d == nullptr) {
        fprintf(stderr, "Node with NULL value!!\n");
        return;
    }

    node_t &node = *((node_t *) d);

    char buffer[BUFFER_SIZE];
    sprintf(buffer, R"({"node_id": %u},)", node.node_id);
    queue_str.append(buffer);
}

static std::string get_queue_str() {
    queue_str = "[";

    queue_print_func(queue, print_queue_value_into_buffer);

    if (queue_str.back() == ',') queue_str.pop_back();
    queue_str.append("]");

    return queue_str;
}

int poet_get_queue(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;

    std::string s = get_queue_str();
    char *buffer = (char *) malloc(s.length() + BUFFER_SIZE);
    sprintf(buffer, R"({"status":"success", "data":{"queue": %s}})", s.c_str());
    state = socket_send_message(socket, buffer, strlen(buffer)) > 0;

    if (!state) {
        uint node_id = (context->node != nullptr) ? context->node->node_id : -1;
        fprintf(stderr, "Could not send queue to node, buffer length: %lu\n", strlen(buffer));
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

struct function_handle functions[] = {
        FUNC_PAIR(register),
        FUNC_PAIR(remote_attestation),
        FUNC_PAIR(sgx_time_broadcast),
        FUNC_PAIR(get_sgxtable),
        FUNC_PAIR(get_queue),
        {nullptr, nullptr} // to indicate end
};

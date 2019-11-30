#include "general_structs.h"
#include "poet_server_functions.h"
#include "poet_shared_functions.h"
#include "queue_t.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <ctime>

#include <map>
#include <string>
#include <vector>
#include <set>
#include <zconf.h>

#define POET_PREFIX(X) poet_ ## X
#define FUNC_PAIR(NAME)  { #NAME, POET_PREFIX(NAME) }

const struct timespec LOCK_TIMEOUT = {5, 0};

extern struct global g;

std::map<std::string, uint> public_keys;
pthread_rwlock_t public_keys_lock = PTHREAD_RWLOCK_INITIALIZER;

/** Checks if Public key and Signature is valid (for now just checks if its non-zero) and is not already registered */
static bool check_public_key_and_signature_registration(const std::string &pk_str, const std::string &sign_str,
                                                        poet_context *context) {
    void *buff;

    bool valid = true;
    bool already_registered = false;

    valid = pthread_rwlock_timedrdlock(&public_keys_lock, &LOCK_TIMEOUT) == 0;
    if (valid) {
        already_registered = public_keys.count(pk_str) > 0;
        if (already_registered) {
            uint node = public_keys[pk_str];
            context->node->node_id = node;
            ERR("This PK is already registered for node %u\n", node);
        }
        pthread_rwlock_unlock(&public_keys_lock);
    } else {
        int errsv = errno;
        perror("check_public_key_and_signature_registration");
        errno = errsv;
        return false;
    }

    context->public_key = (public_key_t *) malloc(sizeof(public_key_t));
    context->signature = (signature_t *) malloc(sizeof(signature_t));

    public_key_t &pk = *(context->public_key);
    size_t buff_len;
    if (valid) {
        buff = decode_64base(pk_str.c_str(), pk_str.length(), &buff_len);
        if (buff == nullptr || buff_len != sizeof(public_key_t)) {
            ERROR("public key has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len,
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
            ERROR("signature has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len,
                  sizeof(signature_t));
            valid = false;
        }
        memcpy(&sign, buff, sizeof(sign));
        if (buff != nullptr) {
            free(buff);
        }
    }


    // Checking validity of PK and Sign
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

int POET_PREFIX(register)(json_value *json, socket_t *socket, poet_context *context) {
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
    if (context->node == nullptr) context->node = (node_t *) calloc(1, sizeof(node_t));

    ERR("Register method is called.\n");

    /**************************************/

    json_value *pk_json = find_value(json, "public_key");
    if (pk_json == nullptr || pk_json->type != json_string) {
        ERROR("public key from node is not valid or is not present\n");
        goto error;
    }
    pk_64base = pk_json->u.string.ptr;

    sign_json = find_value(json, "signature");
    if (sign_json == nullptr || sign_json->type != json_string) {
        ERROR("signature from node is not valid or is not present\n");
        goto error;
    }
    sign_64base = sign_json->u.string.ptr;

    if (check_public_key_and_signature_registration(std::string(pk_64base), std::string(sign_64base), context)) {
        bool locked = true;
        locked = rwlock_timedrdlocks(&LOCK_TIMEOUT, &g.current_id_lock, &public_keys_lock);
//        locked = pthread_rwlock_timedrdlock(&current_id_lock, &LOCK_TIMEOUT) == 0;
        if (locked) {
            context->node->node_id = g.current_id++;
            public_keys.insert({std::string(pk_64base), context->node->node_id});
            rwlock_unlocks(&g.current_id_lock, &public_keys_lock);
        } else {
            perror("poet_register -> locks for current_id_lock and public_key_lock");
        }
    } else {
        ERROR("The PK or the Signature is not valid, closing connection ...\n");
        valid = false;
        goto error;
    }

    /*****************************/

    msg = (char *) malloc(BUFFER_SIZE);
    sprintf(msg,
            R"({"status":"success", "data": {"sgxmax" : %lu, "sgxt_lower": %lu, "node_id" : %u, "n_tiers": %u, "server_starting_time": %lu}})",
            g.sgxmax,
            g.sgxt_lowerbound, context->node->node_id, g.n_tiers, g.server_starting_time);
    ERR("Server is sending sgxmax (%lu) to the node\n", g.sgxmax);
    socket_send_message(socket, msg, strlen(msg));
    free(msg);

    goto terminate;

    error:
    socket_close(socket);

    terminate:
    return valid;
}

int POET_PREFIX(remote_attestation)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;

    ERR("Remote Attestation method is called\n");

#ifdef NO_RA
    char *buffer = (char *) malloc(BUFFER_SIZE);
    state = state && buffer != nullptr;
    if (state) {
        sprintf(buffer, R"({"status":"success"})");
        size_t len = strlen(buffer);
        state = socket_send_message(socket, buffer, len) > 0;
        free(buffer);
    }
#else

    // TODO: Remote attestation
    assert(false);

#endif

    return state;
}

std::map<void *, int> repetitions;

int queue_delete_continuous_nodes(int first, void *d) {
    static void *previous;
    if (first) {
        previous = d;
        repetitions.clear();
        repetitions[d] = 1;
        return 0;
    }

    repetitions[d]++;

    if (previous == d) {
        assert(repetitions[d] > 1);
        repetitions[d]--;
        return 1;
    }

    previous = d;
    return 0;
}

int queue_delete_repeated_nodes(int _, void *d) {
    if (repetitions[d] > 1) {
        repetitions[d]--;
        return 1;
    }

    return 0;
}

static bool queue_cleanup(bool lock = true) {
    if (lock) {
        struct timespec t = {5, 0};
        assertp(rwlock_rwlocks(g.queue->lock));
        assertp(mutex_locks(&g.sgx_table_lock, &g.queue->cond.cond_mutex));
    }

    ERR("Cleaning up the queue\n");

    if (!g.sgx_table.empty()) {
//        time_t current_time = time(nullptr) - g.server_starting_time;
//
//        uint minimum_arrival_time = -1;
//        for (auto &node : g.sgx_table) {
//            minimum_arrival_time = std::min(minimum_arrival_time, node->arrival_time);
//        }
//
//        bool finished = false;
//        while (!finished && !queue_is_empty(g.queue)) {
//            int id = (int) (long) queue_front(g.queue);
//            assert(id < g.sgx_table.size());
//            node_t &u = *g.sgx_table[id];
//            uint arrival_t = u.arrival_time;
//            time_t leadership_t = calc_leadership_time(g.queue, g.sgx_table, u, g.n_tiers, g.sgxmax,
//                                                       minimum_arrival_time, g.server_starting_time);
//            ERR("Cleaning node %u: arrival_t: %u | leadership_t: %ld | current_time: %ld | minimum_arrival_time: %u\n",
//                u.node_id, arrival_t, leadership_t, current_time, minimum_arrival_time);
//            finished = arrival_t + leadership_t >= current_time;
//            if (!finished) {
//                queue_pop(g.queue);
//            }
//        }

//        bool finished = queue_is_empty(g.queue);
//        int id = (int)(long) queue_front_and_pop(g.queue);
//        while(!finished && queue_size(g.queue) > g.sgx_table.size() +1) {
//            int next = (int)(long) queue_front(g.queue);
//            if (id == next) {
//                queue_pop(g.queue);
//                id = next;
//            }
//        }

        queue_pop_custom(g.queue, 0, 0);
        int deleted = queue_selective_remove(g.queue, queue_delete_continuous_nodes, 0, 0);
        deleted += queue_selective_remove(g.queue, queue_delete_repeated_nodes, 0, 0);
        ERR("deleted %d elements from the queue\n", deleted);
    } else {
        WARN("SGXtable is empty\n");
    }

    if (lock) {
        mutex_unlocks(&g.sgx_table_lock, &g.queue->cond.cond_mutex);
        rwlock_unlocks(g.queue->lock);
    }
}

static void copy_queuet_std_set(void *node_ptr, void *std_queue_ptr) {
    auto &v = *((std::set<uint> *) std_queue_ptr);
    auto node = (uint) ((long long) (node_ptr));

    v.insert(node);
}

static bool insert_node_into_sgx_table_and_queue(node_t &node) {
    ERR("Adding node (ID: %u, SGXt: %u, At: %u, TL: %u, NOL: %u) into SGX table\n", node.node_id,
        node.sgx_time, node.arrival_time, node.time_left, node.n_leadership);

    bool state = true;

    struct timespec t = {3, 0};
    assertp(state = mutex_locks(&g.sgx_table_lock, g.queue->cond.cond_mutex));
    assertp(rwlock_rwlocks(g.queue->lock));
    if (state) {
        if (node.node_id < g.sgx_table.size()) {
            ERR("The node %d is already in the SGXtable\n", node.node_id);
            node_t *n = g.sgx_table[node.node_id];
            assert(n->node_id == node.node_id);
            assert(node.sgx_time == node.time_left);
            n->sgx_time = node.sgx_time;
            n->arrival_time = node.arrival_time;
            n->time_left = node.time_left;
            n->n_leadership++;
            if (queue_size_custom(g.queue, 0) >= g.sgx_table.size()) {
                queue_pop_custom(g.queue, 0, 0);
            }
            queue_push_custom(g.queue, (void *) node.node_id, 0, 0);
            queue_cleanup(false);
            queue_broadcast(g.queue);
        } else {
            auto new_node = (node_t *) calloc(1, sizeof(node_t));
            assert(new_node != nullptr);
            memcpy(new_node, &node, sizeof(node_t));
            g.sgx_table.push_back(new_node);
            assert(g.sgx_table.back() == new_node);
            queue_push_custom(g.queue, (void *) node.node_id, 0, 0);
            queue_cleanup(false);
            queue_broadcast(g.queue);
            ERR("Inserted node (ID: %u, SGXt: %u, At: %u, TL: %u, NOL: %u) into the SGX table and Queue\n",
                node.node_id,
                node.sgx_time, node.arrival_time, node.time_left, node.n_leadership);
        }

        if (queue_size_custom(g.queue, 0) < g.sgx_table.size()) {
            std::set<uint> v;
            queue_print_func_dump_custom((queue_t *) g.queue, copy_queuet_std_set, &v, 0);

            uint rt_sum = 0;
            for (int i = 0; i < g.sgx_table.size(); i++) {
                rt_sum += g.sgx_table[i]->time_left;
            }

            for (int i = 0; i < g.sgx_table.size(); i++) {
                if (v.count(i) == 0 && g.sgx_table[i]->arrival_time) { /* Fills the queue with missing elements */
                    queue_push_custom(g.queue, (void *) i, 0, 0);
                    ERR("Node %d is missing from the Queue\n", i);
                }
            }
        }
    } else {
        perror("insert_node_into_sgx_table_and_queue");
    }

    error:
    rwlock_unlocks(g.queue->lock);
    mutex_unlocks(&g.sgx_table_lock, g.queue->cond.cond_mutex);

    return state;
}

// TODO: remove this two methods

static std::string get_arrival_times() {
    return "[]"; // TODO complete
}

static std::string get_quantum_times() {
    return "[]"; // TODO complete
}

int POET_PREFIX(sgx_time_broadcast)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    char *msg = nullptr;
    uint sgxt = 0;
    bool state = true;

    json_value *json_sgxt = find_value(json, "sgxt");
    if (json_sgxt == nullptr || json_sgxt->type != json_integer) {
        state = false;
    }

    if (state) {
        sgxt = json_sgxt->u.integer;
        ERR("SGXt is received and checking the validity.\n");
    }

    state = state && (g.sgxt_lowerbound <= sgxt && sgxt <= g.sgxmax);
    ERR("SGXt is%s valid: %s (%u)\n", (state ? "" : " not"), (state ? "true" : "false"), sgxt);

    if (state) {
        node_t &node = *(context->node);
        node.arrival_time = time(nullptr) - g.server_starting_time;
//        node.arrival_time = 0;
        node.n_leadership = 0;
        node.sgx_time = sgxt;
        node.time_left = sgxt;
        state = insert_node_into_sgx_table_and_queue(node);
    }

    if (state) {
        node_t &node = *(context->node);
        std::string arrival_times = get_arrival_times();
        std::string quantum_times = get_quantum_times();

        msg = (char *) malloc(BUFFER_SIZE + arrival_times.length() + quantum_times.length());
        state = msg != nullptr;
        if (state) {
            sprintf(msg,
                    R"({"status":"success", "data": {"n_nodes": %u, "n_tiers": %u, "arrival_times": %s, "quantum_times": %s}})",
                    g.current_id, g.n_tiers, arrival_times.c_str(), quantum_times.c_str()); // TODO: complete
        }
    }

    if (!state) {
        msg = (char *) malloc(BUFFER_SIZE);
        sprintf(msg, R"({"status":"failure"})");
    }

    socket_send_message(socket, msg, strlen(msg));
    free(msg);
    msg = nullptr;

    return state;
}

std::string get_sgx_table_str(bool lock = true) {
    std::string sgx_table_str = "[";

    bool state = true;

    if (lock) {
        state = pthread_mutex_timedlock(&g.sgx_table_lock, &LOCK_TIMEOUT) == 0;
        if (!state) {
            perror("get_sgx_table_str");
        }
    }

    if (state) {
        for (auto i = g.sgx_table.begin(); i != g.sgx_table.end(); i++) {
            const char *json = node_t_to_json(*i);
            sgx_table_str.append(json);
            sgx_table_str.append(",");
            free((void *) json);
        }
        if (lock) pthread_mutex_unlock(&g.sgx_table_lock);
    }

    if (sgx_table_str.back() == ',') sgx_table_str.pop_back(); // delete the trailing comma if present
    sgx_table_str.append("]");

    return sgx_table_str;
}

int POET_PREFIX(get_sgxtable)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;
    std::string str = std::move(get_sgx_table_str());

    char *buffer = (char *) malloc(str.length() + BUFFER_SIZE);
    state = buffer != nullptr;

    if (state) {
        uint written = 0;
        written += sprintf(buffer, R"({"status":"success", "data":{"sgx_table": )");
        strcat(buffer + written, str.c_str());
        written += str.length();
        strcat(buffer + written, "}}");
    }

    state = state && (socket_send_message(socket, buffer, strlen(buffer)) > 0);

    if (!state) {
        int node_id = (context->node != nullptr) ? (int) context->node->node_id : -1;
        WARN("Could not send sgx_table to node %d.\n", node_id);
    }

    if (buffer != nullptr) {
        free(buffer);
    }
    return state;
}

static std::string node_to_json(const node_t &node) { // move to general methods
    const char *json = node_t_to_json(&node);
    std::string s(json);
    free((void *) json);
    return s;
}

static void print_queue_value_into_buffer(void *d, void *string_ptr) {
    assert(string_ptr != nullptr);

    std::string &queue_str = *((std::string *) string_ptr);

    int id = (int) (long) d;
    if (id >= g.sgx_table.size()) {
        WARN("id (%d) does not exist in current SGXtable (%lu) ... retrying\n", id, g.sgx_table.size());
        usleep(50);
        if (id >= g.sgx_table.size()) {
            WARN("id (%d) does not exist in current SGXtable (%lu) ... ABORTING\n", id, g.sgx_table.size());
            return;
        }
    }
    node_t &node = *g.sgx_table[id];

    bool valid = true;
    char *buffer = (char *) malloc(BUFFER_SIZE);
    valid = buffer != nullptr;
    if (valid) {
        sprintf(buffer, R"(%u,)", node.node_id);
        queue_str.append(buffer);
        free(buffer);
    }
}

std::string get_queue_str() {
    std::string queue_str = "[";

    queue_print_func_dump(g.queue, print_queue_value_into_buffer, &queue_str);

    if (queue_str.back() == ',') queue_str.pop_back();
    queue_str.append("]");

    return queue_str;
}

int POET_PREFIX(get_queue)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;

    std::string s = std::move(get_queue_str());
    char *buffer = (char *) malloc(s.length() + BUFFER_SIZE);
    state = buffer != nullptr;
    if (state) {
        uint written = 0;
        written += sprintf(buffer, R"({"status":"success", "data":{"queue": )");
        strcat(buffer + written, s.c_str());
        written += s.length();
        strcat(buffer + written, "}}");
    }
    state = state && socket_send_message(socket, buffer, strlen(buffer)) > 0;

    if (!state) {
        uint node_id = (context->node != nullptr) ? context->node->node_id : -1;
        ERROR("Could not send queue to node %u, buffer length: %lu\n", node_id, strlen(buffer));
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

int POET_PREFIX(get_sgxtable_and_queue)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;

    state = pthread_mutex_timedlock(&g.sgx_table_lock, &LOCK_TIMEOUT) == 0;
    if (!state) {
        perror("poet_get_sgxtable_and_queue");
    }
    std::string qs = state ? std::move(get_queue_str()) : "";
    std::string sgxt_str = state ? std::move(get_sgx_table_str(false)) : "";
    if (state) pthread_mutex_unlock(&g.sgx_table_lock);

    char *buffer = (char *) malloc(qs.length() + sgxt_str.length() + BUFFER_SIZE);
    state = state && buffer != nullptr;
    if (state) {
        uint written = 0;
        written += sprintf(buffer, R"({"status":"success", "data":{"queue": )");
        strcat(buffer + written, qs.c_str());
        written += qs.length();
        strcat(buffer + written, R"(, "sgx_table": )");
        strcat(buffer + written, sgxt_str.c_str());
        written += sgxt_str.length();
        strcat(buffer + written, "}}");
    } else {
        char sbuffer[BUFFER_SIZE];
        sprintf(sbuffer, R"({"status":"failure"})");
        socket_send_message(socket, sbuffer, strlen(sbuffer));
    }
    state = state && socket_send_message(socket, buffer, strlen(buffer)) > 0;

    if (!state) {
        uint node_id = (context->node != nullptr) ? context->node->node_id : -1;
        ERROR("Could not send queue and sgx table to node, buffer length: %lu\n", strlen(buffer));
    }

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

int POET_PREFIX(close_connection)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;

    char *buffer = (char *) malloc(BUFFER_SIZE);
    state = buffer != nullptr;
    if (state) {
        sprintf(buffer, R"({"status":"success"})");
        state = socket_send_message(socket, buffer, strlen(buffer)) > 0;
    }

    socket_close(socket);

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

int POET_PREFIX(unfinished_node)(json_value *json, socket_t *socket, poet_context *context) {
    assert(json != nullptr);
    assert(socket != nullptr);
    assert(context != nullptr);

    bool state = true;

    node_t new_node{};

    state = json_to_node_t(json, &new_node);

    assertp(mutex_locks(&g.sgx_table_lock, g.queue->cond.cond_mutex));
    assertp(rwlock_rwlocks(g.queue->lock));
    if (state && new_node.node_id < g.sgx_table.size()) {
        node_t *dest = g.sgx_table[new_node.node_id];
        assert(dest != nullptr);

        assert(dest->node_id == new_node.node_id);
        assert(dest->arrival_time != new_node.arrival_time || dest->time_left >= new_node.time_left);
        if (dest->sgx_time == new_node.sgx_time && dest->arrival_time == new_node.arrival_time) {
            dest->time_left = new_node.time_left;
            queue_cleanup(false);
            queue_push_custom(g.queue, (void *) dest->node_id, 0, 0);
        } else {
            state = false;
        }

    } else {
        state = false;
    }
    rwlock_unlocks(g.queue->lock);
    mutex_unlocks(&g.sgx_table_lock, g.queue->cond.cond_mutex);

    if (state) queue_broadcast(g.queue);

    const char *msg = nullptr;
    if (state) {
        msg = R"({"status": "success"})";
    } else {
        msg = R"({"status": "failure"})";
    }

    socket_send_message(socket, (void *) msg, strlen(msg));
    return state;
}

struct function_handle poet_functions[] = {
        FUNC_PAIR(register),
        FUNC_PAIR(remote_attestation),
        FUNC_PAIR(sgx_time_broadcast),
        FUNC_PAIR(get_sgxtable),
        FUNC_PAIR(get_queue),
        FUNC_PAIR(get_sgxtable_and_queue),
        FUNC_PAIR(close_connection),
        FUNC_PAIR(unfinished_node),
        {nullptr, nullptr} // to indicate end
};
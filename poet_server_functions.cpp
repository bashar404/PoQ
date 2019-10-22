#include "poet_node_t.h"
#include "poet_server_functions.h"
#include "poet_functions.h"
#include "general_structs.h"
#include <cstdio>
#include <cstring>


#include <unordered_map>
#include <string>

#define FUNC_PAIR(NAME)  { #NAME, poet_ ## NAME }

#define BUFFER_SIZE 2048

extern node_t *sgx_table;
extern pthread_mutex_t sgx_table_lock;
extern time_t current_time;
pthread_mutex_t current_time_lock = PTHREAD_MUTEX_INITIALIZER;
extern uint current_id;
pthread_mutex_t current_id_lock = PTHREAD_MUTEX_INITIALIZER;
extern size_t sgxmax;
extern size_t sgxt_lowerbound;

std::unordered_map<std::string, uint> public_keys;
pthread_mutex_t public_keys_lock = PTHREAD_MUTEX_INITIALIZER;

/** Checks if Public key and Signature is valid (for now just checks if its non-zero) and is not already registered */
static bool check_public_key_and_signature_registration(const std::string &pk_str, const std::string &sign_str) {
    bool valid = public_keys.count(pk_str) == 0; // is not already registered

    if (!valid) {
        uint node = public_keys[pk_str];
        fprintf(stderr, "This PK is already registered for node %u\n", node);
        return valid;
    }

    public_key_t pk;
    size_t buff_len;
    void *buff = decode_64base(pk_str.c_str(), pk_str.length(), &buff_len);
    if (buff == nullptr || buff_len != sizeof(public_key_t)) {
        fprintf(stderr, "public key has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len,
                sizeof(public_key_t));
        valid = false;
    }
    memcpy(&pk, buff, sizeof(pk));
    if (buff != nullptr) {
        free(buff);
    }

    signature_t sign;
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

    auto *pk_8 = (uint8_t *) &pk;
    size_t len = sizeof(public_key_t);
    for (int i = 0; i < len && valid; i++) {
        valid = valid || *(pk_8 + i) != 0;
    }

    auto *sign_8 = (uint8_t *) &sign;
    len = sizeof(signature_t);
    for (int i = 0; i < len && valid; i++) {
        valid = valid || *(pk_8 + i) != 0;
    }

    return valid;
}

int poet_register(json_value *json, socket_t *socket) {
    int ret_status = EXIT_SUCCESS;
    int valid;
    char *msg;
    void *buff;
    char *sign_64base;
    size_t sign_64base_len;
    char *pk_64base;
    size_t pk_64base_len;
    json_value *sign_json;

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

    if (check_public_key_and_signature_registration(std::string(pk_64base), std::string(sign_64base))) {
        pthread_mutex_lock(&current_id_lock);
        uint id = current_id++;
        pthread_mutex_unlock(&current_id_lock);

        pthread_mutex_lock(&public_keys_lock);
        public_keys.insert({std::string(pk_64base), id});
        pthread_mutex_unlock(&public_keys_lock);
    } else {
        fprintf(stderr, "The PK or the Signature is not valid, closing connection ...\n");
        ret_status = EXIT_FAILURE;
        goto error;
    }

    /*****************************/

    msg = (char *) malloc(BUFFER_SIZE);
    sprintf(msg, R"({"status":"success", "data": {"sgxmax" : %lu, "sgxt_lower": %lu, "node_id" : %u}})", sgxmax, sgxt_lowerbound, current_id-1);
    printf("Server is sending sgxmax (%lu) to the node\n", sgxmax);
    socket_send_message(socket, msg, strlen(msg));
    free(msg);

    if (valid) goto terminate;

    error:
    socket_close(socket);

    terminate:
    return ret_status;
}

int poet_remote_attestation(json_value *json, socket_t *socket) {
    fprintf(stderr, "Remote Attestation method is called\n");

#ifdef NO_RA
    char *buffer = (char *) malloc(BUFFER_SIZE);

    sprintf(buffer, R"({"status":"success"})");
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    socket_send_message(socket, buffer, len);
    free(buffer);
#else

    // TODO: Remote attestation
    assert(false);

#endif

    return 0;
}

int poet_sgx_time_broadcast(json_value *json, socket_t *socket) {
    char *msg;
    uint sgxt = 0;
    int valid = 1;

//    // receive msg from node
//    size_t len;
//    socket_get_message(socket, (void **) &msg, &len);
//    json = json_parse(msg, len);

    json_value * json_sgxt = find_value(json, "sgxt");
    if (json_sgxt == nullptr) {
        valid = 0;
        goto error;
    }
    sgxt = json_sgxt->u.integer;
    printf("SGXt is received and checking the validity.\n");

    error:

    valid = valid && (sgxt_lowerbound <= sgxt && sgxt <= sgxmax);
    printf("SGXt is%s valid: %s (%u)\n", (valid ? "" : " not"), (valid ? "true" : "false"), sgxt);

    msg = (char *) malloc(BUFFER_SIZE);
    sprintf(msg, R"({"status":"%s"})", (valid ? "success" : "failure") );
    socket_send_message(socket, msg, strlen(msg));

    return valid;
}

struct function_handle functions[] = {
        FUNC_PAIR(register),
        FUNC_PAIR(remote_attestation),
        FUNC_PAIR(sgx_time_broadcast),
        {nullptr, nullptr} // to indicate end
};

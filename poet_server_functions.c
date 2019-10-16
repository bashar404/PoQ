#ifdef __cplusplus
extern "C" {
#endif

#include "poet_node_t.h"
#include "poet_server_functions.h"
#include "poet_functions.h"
#include "general_structs.h"
#include <stdio.h>
#include <string.h>
#include <json-parser/json.h>

#define FUNC_PAIR(NAME)  { #NAME, poet_ ## NAME }

#define BUFFER_SIZE 1024

extern node_t *sgx_table;
extern pthread_mutex_t sgx_table_lock;
extern time_t current_time;
extern uint current_id;
extern size_t sgxmax;
extern size_t sgxt_lowerbound;

int poet_register(json_value *json, socket_t *socket) {
    int ret_status = EXIT_SUCCESS;

    fprintf(stderr, "Register method is called.\n");

    /**************************************/

    public_key_t node_pk;
    signature_t node_sign;
    json_value *pk_json = find_value(json, "public_key");
    if (pk_json == NULL || pk_json->type != json_string) {
        fprintf(stderr, "public key from node is not valid or is not present\n");
        goto error;
    }
    char* pk_64base = pk_json->u.string.ptr;
    size_t pk_64base_len = pk_json->u.string.length;

    json_value *sign_json = find_value(json, "signature");
    if (sign_json == NULL || sign_json->type != json_string) {
        fprintf(stderr, "signature from node is not valid or is not present\n");
        goto error;
    }
    char* sign_64base = sign_json->u.string.ptr;
    size_t sign_64base_len = pk_json->u.string.length;

    size_t buff_len;
    void * buff = decode_64base(pk_64base, pk_64base_len, &buff_len);
    if (buff == NULL || buff_len != sizeof(node_pk)) {
        fprintf(stderr, "public key has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len, sizeof(node_pk));
        goto error;
    }
    memcpy(&node_pk, buff, sizeof(node_pk));

    buff = decode_64base(sign_64base, sign_64base_len, &buff_len);
    if (buff == NULL || buff_len != sizeof(node_sign)) {
        fprintf(stderr, "signature has an incorrect size (%lu bytes), should be (%lu bytes)\n", buff_len, sizeof(node_sign));
        goto error;
    }
    memcpy(&node_sign, buff, sizeof(node_sign));

    printf("%c%c <<<<<\n", node_pk.c, node_sign.c);

    /*****************************/

    char *msg = malloc(BUFFER_SIZE);
    sprintf(msg, "{\"sgxmax\" : %lu, \"sgxt_lower\": %lu, \"node_id\" : %u}", sgxmax, sgxt_lowerbound, current_id++);
    printf("Server is sending sgxmax (%lu) to the node\n", sgxmax);
    socket_send_message(socket, msg, strlen(msg));
    free(msg);

    // receive msg from node
    size_t len;
    socket_get_message(socket, (void **) &msg, &len);
    json = json_parse(msg, len);

    uint sgxt = json->u.object.values[0].value->u.integer;
    printf("SGXt is received and checking the validity.\n");

    int valid = (sgxt_lowerbound <= sgxt && sgxt <= sgxmax);
    printf("SGXt is%s valid: %s\n", (valid ? "" : " not"), (valid ? "true" : "false"));

    error:


    socket_close(socket);

    return 1;
}

int poet_remote_attestation(json_value *json, socket_t *socket) {
    fprintf(stderr, "Remote Attestation method is called\n");
    return 0;
}

struct function_handle functions[] = {
        FUNC_PAIR(register),
        FUNC_PAIR(remote_attestation),
        {NULL, 0} // to indicate end
};

#ifdef __cplusplus
};
#endif
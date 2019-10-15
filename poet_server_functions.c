#ifdef __cplusplus
extern "C" {
#endif

#include "poet_node_t.h"
#include "poet_server_functions.h"
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
    fprintf(stderr, "Register method is called.\n");

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
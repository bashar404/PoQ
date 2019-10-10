#ifdef __cplusplus
extern "C" {
#endif

#include "poet_server_functions.h"
#include <stdio.h>
#include <string.h>

#define FUNC_PAIR(NAME)  { #NAME, poet_ ## NAME }

int poet_register(json_value *json, socket_t *socket) {
    fprintf(stderr, "llamada a poet_register\n");
    char *msg = "le respondo al cliente";
    socket_send_message(socket, msg, strlen(msg));

    socket_close(socket);

    return 1;
}

int poet_remote_attestation(json_value *json, socket_t *socket) {
    fprintf(stderr, "llamada a poet_remote_attestation\n");
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
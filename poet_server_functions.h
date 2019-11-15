#ifndef POET_CODE_POET_SERVER_FUNCTIONS_H
#define POET_CODE_POET_SERVER_FUNCTIONS_H

#include "json_checks.h"
#include "socket_t.h"
#include "general_structs.h"
#include <string>

int poet_register(json_value *json, socket_t *socket, poet_context *context);
int poet_remote_attestation(json_value *json, socket_t *socket, poet_context *context);
int poet_sgx_time_broadcast(json_value *json, socket_t *socket, poet_context *context);
int poet_get_sgxtable(json_value *json, socket_t *socket, poet_context *context);
int poet_get_queue(json_value *json, socket_t *socket, poet_context *context);
int poet_get_sgxtable_and_queue(json_value *json, socket_t *socket, poet_context *context);
int poet_close_connection(json_value *json, socket_t *socket, poet_context *context);


std::string get_sgx_table_str(bool);
std::string get_queue_str();

struct function_handle {
    const char *name;
    int (*function)(json_value *, socket_t *, poet_context *);
};

extern struct function_handle poet_functions[];

#endif //POET_CODE_POET_SERVER_FUNCTIONS_H

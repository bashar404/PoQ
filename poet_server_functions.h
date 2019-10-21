#ifndef POET_CODE_POET_SERVER_FUNCTIONS_H
#define POET_CODE_POET_SERVER_FUNCTIONS_H

#include "json_checks.h"
#include "socket_t.h"

int poet_register(json_value *json, socket_t * socket);
int poet_remote_attestation(json_value *json, socket_t * socket);

struct function_handle {
    const char *name;
    int (*function)(json_value *, socket_t *);
};

extern struct function_handle functions[];

#endif //POET_CODE_POET_SERVER_FUNCTIONS_H

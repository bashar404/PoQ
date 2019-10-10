#ifndef POET_CODE_POET_SERVER_FUNCTIONS_H
#define POET_CODE_POET_SERVER_FUNCTIONS_H

#include "socket_t.h"

int poet_register(void *arg, socket_t * socket);
int poet_remote_attestation(void *arg, socket_t * socket);

struct function_handle {
    char *name;
    int (*function)(void *, socket_t *);
};

extern struct function_handle functions[];

#endif //POET_CODE_POET_SERVER_FUNCTIONS_H

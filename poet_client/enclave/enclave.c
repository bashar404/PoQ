#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

//#include "enclave.h"
#include "enclave_t.h"  /* print_string */

void ecall_test_print(const char *msg) {
    size_t len = strlen(msg);

    char *buffer = "ENCLAVE";

//    strcat(buffer, msg);

//    sprintf(buffer, "ENCLAVE: %s\n", msg);
//    vsnprintf(buffer, len+32, "ENCLAVE: %s\n", ap);

    ocall_print_msg(buffer);
}

#ifdef __cplusplus
};
#endif
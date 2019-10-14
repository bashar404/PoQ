#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

//#include "enclave.h"
#include "enclave_t.h"  /* print_string */
#include "sgx_trts.h"

void ecall_test_print(const char *msg) {
    size_t len = strlen(msg);

    const char *buffer = "ENCLAVE????";
    len = strlen(buffer);
    char buffer2[len+1];

    for(size_t i = 0; i < len+1; i++) {
        buffer2[i] = buffer[i] != '?' ? buffer[i] : '!';
    }

//    strcat(buffer, msg);

//    sprintf(buffer, "ENCLAVE: %s\n", msg);
//    vsnprintf(buffer, len+32, "ENCLAVE: %s\n", ap);

    ocall_print_msg(buffer2);
    ocall_print_msg(msg);
}

void ecall_random(int *a) {
    sgx_read_rand((unsigned char *) a,4);
}

#ifdef __cplusplus
};
#endif
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>

//#include "enclave.h"
#include "enclave_t.h"  /* print_string */
#include "sgx_trts.h"

void ecall_random_bytes(void *a, size_t len) {
    sgx_read_rand(a, len);
}

#ifdef __cplusplus
};
#endif
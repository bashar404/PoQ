#ifndef POET_CODE_ENCLAVE_HELPER_H
#define POET_CODE_ENCLAVE_HELPER_H

#include "sgx_error.h"       /* sgx_status_t */
#include "sgx_eid.h"     /* sgx_enclave_id_t */

#ifdef __cplusplus
extern "C" {
#endif

void sgx_print_error_message(sgx_status_t ret);
int initialize_enclave(sgx_enclave_id_t *eid);

#ifdef __cplusplus
};
#endif

#endif //POET_CODE_ENCLAVE_HELPER_H

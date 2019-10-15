#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <cstring>
#include <ctime>
#include <cassert>

#include <unistd.h>
#include <pwd.h>
#include <json-parser/json.h>

#if   defined(__GNUC__)
# define TOKEN_FILENAME   "enclave.token"
# define ENCLAVE_FILENAME "enclave.signed.so"
#endif

#include "socket_t.h"
#include "general_structs.h"
#include "poet_functions.h"

#include "sgx_error.h"       /* sgx_status_t */
#include "sgx_eid.h"     /* sgx_enclave_id_t */
#include "sgx_urts.h"
#include "sgx_uae_service.h"
#include "../enclave_u.h"
#include "poet_client.h"

#ifndef NDEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#define ERR(...) /**/
#endif

#define DOMAIN AF_INET
#define TYPE SOCK_STREAM
#define PROTOCOL 0
#define PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFFER_SZ 2048

socket_t *node_socket = nullptr;

sgx_enclave_id_t global_eid = 0;

void global_variable_initialization() {
    node_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, PORT);
}

void global_variable_destructors() {
    socket_destructor(node_socket);
}

int main(int argc, char *argv[]) {
    // TODO: receive parameters by command line

    global_variable_initialization();

    ERR("trying to establish connection with server\n");
    socket_connect(node_socket);
    ERR("Connection established\n");

    /* Initialize the enclave */
    if(initialize_enclave() < 0){
        fprintf(stderr, "Could not create Enclave\n");
        return EXIT_FAILURE;
    }

    char *buffer = (char *) malloc(BUFFER_SZ);
    sprintf(buffer, R"({"method" : "register", "data": {}})");
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    socket_send_message(node_socket, buffer, len);
    free(buffer);

    // receive msg from server

    socket_get_message(node_socket, (void **) (&buffer), &len);

    json_value *json = json_parse(buffer, len);
    uint sgxmax = find_value(json, "sgxmax")->u.integer;
    uint sgx_lowerbound = find_value(json, "sgxt_lower")->u.integer;
    uint node_id = find_value(json, "node_id")->u.integer;

    printf("SGXmax (%u), SGXlower (%u) and Node id (%u) is received from the server\n", sgxmax, sgx_lowerbound, node_id);

    uint sgxt = 0;
    if (ecall_random_bytes(global_eid, &sgxt, sizeof(sgxt)) != SGX_SUCCESS) {
        fprintf(stderr, "Something happened :c\n");
    }
    sgxt = sgx_lowerbound + sgxt % (sgxmax - sgx_lowerbound +1);
    printf("SGXt is generated: %u\n", sgxt);

    // respond the server with sgxt

    free(buffer);
    buffer = (char *) malloc(BUFFER_SZ);
    sprintf(buffer, "{\"sgxt\" : %u}", sgxt);
    printf("Sending SGXt (%u) to the server\n", sgxt);
    socket_send_message(node_socket, buffer, strlen(buffer));

    socket_close(node_socket);

    sgx_destroy_enclave(global_eid);

    global_variable_destructors();

    return EXIT_SUCCESS;
}

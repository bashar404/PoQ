#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <json-parser/json.h>
#include <general_structs.h>

#include "socket_t.h"
#include "poet_functions.h"
#include "json_checks.h"
#include "enclave_helper.h"

#include "sgx_error.h"       /* sgx_status_t */
#include "sgx_eid.h"     /* sgx_enclave_id_t */
#include "sgx_urts.h"
#include "enclave_u.h"

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
sgx_enclave_id_t eid = 0;

uint sgxt;
uint sgxmax;
uint sgx_lowerbound;
uint node_id;

void global_variable_initialization() {
    node_socket = socket_constructor(DOMAIN, TYPE, PROTOCOL, SERVER_IP, PORT);
}

void global_variable_destructors() {
    socket_destructor(node_socket);
}

static void fill_with_rand(void *input, size_t len) {
    srand(time(nullptr));
    auto *ptr = (unsigned char *) input;

    for(int i = 0; i < len; i++) {
        *(ptr+i) = rand() % 256;
    }
}

static int poet_remote_attestation_to_server() {
    printf("Starting remote attestation ...\n");
    int state = 1;
#ifdef NO_RA
    char *buffer = (char *) malloc(BUFFER_SZ);

    sprintf(buffer, R"({"method" : "remote_attestation", "data": null})");
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    socket_send_message(node_socket, buffer, len);

    free(buffer);
    socket_get_message(node_socket, (void **)(&buffer), &len);
    if (buffer == nullptr) {
        return 0;
    }

    state = check_json_compliance(buffer, len) == 1;
    if (state == 0) {
        return 0;
    }

    json_value *json = json_parse(buffer, len);
    json_value *json_status = find_value(json, "status");
    if (json_status == nullptr || json_status->type != json_string || strcmp(json_status->u.string.ptr, "success") != 0 ) {
        fprintf(stderr, "Remote attestation was not successfull\n");
        state = 0;
    } else {
        printf("Remote attestation was sucecssfull\n");
    }

#else

    // TODO: Remote attestation
    assert(false);

#endif

    return state;
}

static uint generate_random_sgx_time() {
    uint sgxt = 0;
    if (ecall_random_bytes(eid, &sgxt, sizeof(sgxt)) != SGX_SUCCESS) {
        fprintf(stderr, "Something happened with the enclave :c\n");
    }
    sgxt = sgx_lowerbound + sgxt % (sgxmax - sgx_lowerbound + 1);

    return sgxt;
}

static int poet_register_to_server() {
    char *buffer = (char *) malloc(BUFFER_SZ);
    int state = 1;
    json_value *json;
    json_value *json_status;

    public_key_t pk;
    fill_with_rand(&pk, sizeof(public_key_t)); // TODO: change
    signature_t sign;
    fill_with_rand(&sign, sizeof(signature_t));
    unsigned char *pk_64base = encode_64base(&pk, sizeof(pk));
    unsigned char *sign_64base = encode_64base(&sign, sizeof(sign));
    if (pk_64base == nullptr || sign_64base == nullptr) {
        fprintf(stderr, "failure encoding pk\n");
        exit(EXIT_FAILURE);
    }

    sprintf(buffer, R"({"method" : "register", "data": {"public_key": "%s", "signature" : "%s"}})", pk_64base,
            sign_64base);
    printf("%s\n", buffer);
    size_t len = strlen(buffer);
    state = socket_send_message(node_socket, buffer, len);

    free(buffer);
    free(pk_64base);
    free(sign_64base);

    // receive msg from server

    socket_get_message(node_socket, (void **) (&buffer), &len);

    state = check_json_compliance(buffer, len);
    if (state == 0) {
        goto error;
    }

    json = json_parse(buffer, len);

    json_status = find_value(json, "status");
    if (json_status == nullptr || json_status->type != json_string || strcmp(json_status->u.string.ptr, "success") != 0) {
        state = 0;
        goto error;
    }

    if (state) {
        sgxmax = find_value(json, "sgxmax")->u.integer;
        sgx_lowerbound = find_value(json, "sgxt_lower")->u.integer;
        node_id = find_value(json, "node_id")->u.integer;
    }

    printf("SGXmax (%u), SGXlower (%u) and Node id (%u) is received from the server\n", sgxmax, sgx_lowerbound,
           node_id);

    // respond the server with sgxt

    if (state) {
        // if everything went well then do the remote attestation
        state = poet_remote_attestation_to_server();
    }

    free(buffer);
    if (state) {
        sgxt = generate_random_sgx_time();
        printf("SGXt is generated: %u\n", sgxt);

        buffer = (char *) malloc(BUFFER_SZ);
        sprintf(buffer, R"({"method":"sgx_time_broadcast", "data":{"sgxt" : %u}})", sgxt); // TODO: should send its identity from the enclave in it
        printf("Sending SGXt (%u) to the server\n", sgxt);
        socket_send_message(node_socket, buffer, strlen(buffer));
    }

    error:
    sgx_destroy_enclave(eid);

    if (buffer != nullptr) {
        free(buffer);
    }

    return state;
}

int main(int argc, char *argv[]) {
    // TODO: receive parameters by command line

    global_variable_initialization();

    /* Initialize the enclave */
    if (initialize_enclave(&eid) < 0) {
        fprintf(stderr, "Could not create Enclave\n");
        return EXIT_FAILURE;
    }

    ERR("trying to establish connection with server\n");
    socket_connect(node_socket);
    ERR("Connection established\n");

    int state = poet_register_to_server();
    if (! state) {
        fprintf(stderr, "Something failed ...\n");
    }

    printf("Enter character to exit (%lu).\n", eid);
    getchar();

//    printf("Ctrl+C to destroy enclave (%lu).\n", eid);
//    sleep(2*60);

    terminate:
    socket_close(node_socket);
    global_variable_destructors();
    printf("Ending node\n");

    return EXIT_SUCCESS;
}

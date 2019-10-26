#ifndef POET_CODE_GENERAL_STRUCTS_H
#define POET_CODE_GENERAL_STRUCTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>

#define NUM_TYPE int8_t
#define PUBLIC_KEY_BITS_SIZE 256
#define PUBLIC_KEY_SIZE (PUBLIC_KEY_BITS_SIZE / 8)

#define SIGNATURE_BITS_SIZE 256
#define SIGNATURE_SIZE (SIGNATURE_BITS_SIZE / 8)

typedef unsigned int uint;

typedef struct node {
    uint node_id;
    uint arrival_time;
    uint sgx_time;
    uint n_leadership;
    uint time_left;
} node_t;

typedef struct public_key {
    NUM_TYPE key[PUBLIC_KEY_SIZE];
} public_key_t;

typedef struct signature {
    NUM_TYPE hash[SIGNATURE_SIZE];
} signature_t;

struct poet_context {
    node_t *node;
    public_key_t *public_key;
    signature_t *signature;
};

void free_poet_context(struct poet_context *);

char *encode_hex(void *d, size_t len);

void *decode_hex(const char *buffer, size_t buffer_len);

unsigned char *encode_64base(const void *buffer, size_t buffer_len);

void *decode_64base(const char *buffer, size_t buffer_len, size_t *out_len);

#ifdef __cplusplus
};
#endif

#endif //POET_CODE_GENERAL_STRUCTS_H

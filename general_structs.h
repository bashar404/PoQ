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

struct public_key {
    NUM_TYPE key[PUBLIC_KEY_SIZE];
};

typedef struct public_key public_key_t;

struct signature {
    NUM_TYPE hash[SIGNATURE_SIZE];
};

typedef struct signature signature_t;

typedef enum {
    poet_none,
    poet_public_key,
    poet_signature
} poet_type;

union poet_generic_object {
    public_key_t pk;
    signature_t signature;
};

typedef struct poet_object {
    poet_type type;
    union poet_generic_object value;
} poet_object_t;


char *encode_hex(void *d, size_t len);

void *decode_hex(char *hex, size_t len);

#ifdef __cplusplus
};
#endif

#endif //POET_CODE_GENERAL_STRUCTS_H

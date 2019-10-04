#ifndef POET_CODE_GENERAL_STRUCTS_H
#define POET_CODE_GENERAL_STRUCTS_H

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

public_key_t *public_key_constructor();
void public_key_destructor(public_key_t * p);

signature_t *signature_constructor();
void signature_destructor(signature_t * p);


#endif //POET_CODE_GENERAL_STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "general_structs.h"

public_key_t *public_key_constructor(){
    public_key_t * k = malloc(sizeof(public_key_t));
    memset(k, 0, sizeof(public_key_t));

    return k;
}

void public_key_destructor(public_key_t * p){
    assert(p != NULL);

    free(p);
}

signature_t *signature_constructor(){
    signature_t * s = malloc(sizeof(signature_t));
    memset(s, 0, sizeof(signature_t));

    return s;
}

void signature_destructor(signature_t * p){
    assert(p != NULL);

    free(p);
}
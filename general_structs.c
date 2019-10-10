#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "general_structs.h"

#ifndef max
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#endif

#define UPPERCASE(x) ((x) & ((unsigned char)0xEF))
#define LOWERCASE(x) ((x) | ((unsigned char)0x10))

char *encode_hex(void *buffer, size_t buffer_len) {
    size_t wbuffer_len = buffer_len * 2 + 10;
    char *wbuffer = malloc(wbuffer_len);
    if (wbuffer == NULL) {
        perror("malloc");
        goto error;
    }
    memset(wbuffer, 0, wbuffer_len);

    for (size_t i = 0; i < buffer_len; i++) {
        assert(2 * i < wbuffer_len);
        sprintf(wbuffer + 2 * i, "%02x", *((unsigned char *) buffer + i));
    }

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with sending message");
    if (wbuffer != NULL) {
        free(wbuffer);
    }
    wbuffer = NULL;

    terminate:
    return wbuffer;
}

unsigned char hexchr2bin(const char hex) {
    unsigned char c = 0;

    if (hex >= '0' && hex <= '9') {
        c = hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        c = hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        c = hex - 'a' + 10;
    }

    return c;
}

void *decode_hex(char *buffer, size_t buffer_len) {
    assert(buffer != NULL);
    assert(buffer_len > 0);

    size_t wbuff_len = buffer_len / 2 +1;
    char *wbuffer = malloc(wbuff_len);
    if (wbuffer == NULL) {
        perror("malloc");
        goto error;
    }
    memset(wbuffer, 0, wbuff_len);

    unsigned char c;
    size_t pos = 0;
    for(size_t i = 0; i < buffer_len; i+=2) {
        assert(pos < wbuff_len);
        c = hexchr2bin(buffer[i]);
        c <<= 4;
        c |= hexchr2bin(buffer[i+1]);
        wbuffer[pos++] = c;
    }

    goto terminate;

    error:
    fprintf(stderr, "Fatal error, can not proceed with converting hex message to raw bytes");
    if (wbuffer != NULL) {
        free(wbuffer);
    }
    wbuffer = NULL;

    terminate:
    return wbuffer;
}
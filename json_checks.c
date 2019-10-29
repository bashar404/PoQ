#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <assert.h>
#include "json_checks.h"

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

#define BUFFER_SIZE 1024
#define JSON_ERROR_LEN 30

#ifndef NDEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#define ERR(...) /**/
#endif

int check_json_compliance(const char *buffer, size_t buffer_len) {
    assert(buffer != NULL);
    assert(buffer_len > 0);

    JSON_checker jc = new_JSON_checker(buffer_len);

    int is_valid = 1;
    for (int current_pos = 0; (current_pos < buffer_len) && (buffer[current_pos] != '\0') && is_valid; current_pos++) {
        int next_char = buffer[current_pos];

        is_valid = JSON_checker_char(jc, next_char);

        if (!is_valid) {
            fprintf(stderr, "JSON_checker_char: syntax error\n");
        }
    }

    is_valid = is_valid && JSON_checker_done(jc);
    if (!is_valid) {
        fprintf(stderr, "JSON_checker_end: syntax error\n");
        int len = min(buffer_len, JSON_ERROR_LEN);
        fprintf(stderr, "JSON with invalid syntax: [%.*s]\n", len, buffer + (buffer_len - len));
    }

    return is_valid;
}

#ifdef __cplusplus
};
#endif
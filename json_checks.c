#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <assert.h>
#include "json_checks.h"
#include "poet_common_definitions.h"

#define JSON_ERROR_LEN 30

int check_json_compliance(const char *buffer, size_t buffer_len) {
    assert(buffer != NULL);
    assert(buffer_len > 0);

    JSON_checker jc = new_JSON_checker(buffer_len);

    int is_valid = 1;
    for (int current_pos = 0; (current_pos < buffer_len) && (buffer[current_pos] != '\0') && is_valid; current_pos++) {
        int next_char = buffer[current_pos];

        is_valid = JSON_checker_char(jc, next_char);

        if (!is_valid) {
            ER("JSON_checker_char: syntax error\n");
        }
    }

    is_valid = is_valid && JSON_checker_done(jc);
    if (!is_valid) {
        ER("JSON_checker_end: syntax error\n");
        int len = min(buffer_len, JSON_ERROR_LEN);
        ER("JSON with invalid syntax: [%.*s]\n", len, buffer + (buffer_len - len));
    }

    return is_valid;
}

#ifdef __cplusplus
};
#endif
#ifndef POET_CODE_JSON_CHECKS_H
#define POET_CODE_JSON_CHECKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <json-parser/json.h>
#include <JSON-c/JSON_checker.h>

int check_json_compliance(const char *buffer, size_t buffer_len);

#ifdef __cplusplus
};
#endif

#endif //POET_CODE_JSON_CHECKS_H

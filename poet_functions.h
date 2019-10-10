#ifndef POET_CODE_POET_FUNCTIONS_H
#define POET_CODE_POET_FUNCTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <json-parser/json.h>

/* Should return NULL if not found */
json_value *find_value(json_value *u, const char *name);

#ifdef __cplusplus
};
#endif

#endif //POET_CODE_POET_FUNCTIONS_H

#ifndef POET_CODE_POET_FUNCTIONS_H
#define POET_CODE_POET_FUNCTIONS_H

#include <json-parser/json.h>

/* Should return NULL if not found */
json_value *find_value(json_value *json, char *name);

#endif //POET_CODE_POET_FUNCTIONS_H

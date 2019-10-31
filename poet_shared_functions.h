#ifndef POET_CODE_POET_SHARED_FUNCTIONS_H
#define POET_CODE_POET_SHARED_FUNCTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <json-parser/json.h>

/* Should return NULL if not found */
json_value *find_value(json_value *u, const char *name);

int calc_tier_number(node_t *node, uint total_tiers, uint sgx_max);

#ifdef __cplusplus
};
#endif

#endif //POET_CODE_POET_SHARED_FUNCTIONS_H

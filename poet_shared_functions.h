#ifndef POET_CODE_POET_SHARED_FUNCTIONS_H
#define POET_CODE_POET_SHARED_FUNCTIONS_H

#include <cassert>
#include <cstring>
#include <cmath>
#include <cerrno>
#include <queue>
#include "queue_t.h"
#include "general_structs.h"
#include <cstdarg>
#include <vector>
#include <json-parser/json.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <JSON-c/JSON_checker.h>
#ifdef __cplusplus
};
#endif
#include "queue_t.h"

struct thread_tuple {
    pthread_t *thread;
    void *data;
};

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} cond_mutex_t;

json_value *find_value(json_value *u, const char *name);
/* Should return NULL if not found */
int check_json_compliance(const char *buffer, size_t buffer_len);
json_value * check_json_success_status(char *buffer, size_t len);

int calc_tier_number(const node_t &node, uint total_tiers, uint sgx_max);

std::vector<uint> calc_quantum_times(const std::vector<node_t *> &sgx_table, uint ntiers, uint sgx_max);

time_t calc_leadership_time(queue_t *queue, const std::vector<node_t *> &sgx_table, const node_t &current_node, uint tiers, uint sgx_max);
time_t calc_starting_time(queue_t *queue, const std::vector<node_t *> &sgx_table, const node_t &current_node, uint ntiers, uint sgx_max);

int delegate_thread_to_function(pthread_t *thread, void *data, void * (*func)(void *));
int delegate_thread_to_function(pthread_t *thread, void *data, void * (*func)(void *), bool);

/* *************************** For locking *************************** */

/*PP_NARG counts the amount of arguments given to a Preprocessor function up to 63 arguments*/
#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,N,...) N
#define PP_RSEQ_N() \
         63,62,61,60,                   \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0

#define rwlock_timedrdlocks(...) nrwlock_timedxlocks(0, PP_NARG(__VA_ARGS__) -1, __VA_ARGS__)
#define rwlock_timedrwlocks(...) nrwlock_timedxlocks(1, PP_NARG(__VA_ARGS__) -1, __VA_ARGS__)
#define rwlock_unlocks(...) nrwlock_unlocks(PP_NARG(__VA_ARGS__), __VA_ARGS__)

int nrwlock_timedxlocks(int rw, uint locks, const struct timespec *, ...);
int nrwlock_unlocks(uint locks, ...);

#define mutex_locks(...) nmutex_locks(PP_NARG(__VA_ARGS__) -1, __VA_ARGS__)
#define mutex_unlocks(...) nmutex_unlocks(PP_NARG(__VA_ARGS__) -1, __VA_ARGS__)

int nmutex_locks(uint locks, ...);
int nmutex_unlocks(uint locks, ...);

#endif //POET_CODE_POET_SHARED_FUNCTIONS_H

#include <json-parser/json.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "queue_t.h"
#include "general_structs.h"

// BFS
json_value *find_value(json_value *u, const char *name) {
    assert(u != NULL);
    assert(name != NULL);

    json_value *r = NULL;
    queue_t *queue = queue_constructor();
    queue_push(queue, u);

    while (!queue_is_empty(queue) && r == NULL) {
        u = queue_front(queue);
        queue_pop(queue);

        switch (u->type) {
            case json_object:
                for (int i = 0; i < u->u.object.length && r == NULL; i++) {
                    json_object_entry *entry = &(u->u.object.values[i]);
                    queue_push(queue, entry->value);

                    if (strcmp(entry->name, name) == 0) {
                        r = entry->value;
                    }
                }
                break;
            case json_array:
                for (int i = 0; i < u->u.array.length; i++) {
                    queue_push(queue, u->u.array.values[i]);
                }
                break;
            case json_integer:
            case json_double:
            case json_string:
            case json_boolean:
            case json_null:
            default:
                break;
        }
    }

    queue_destructor(queue, 0);

    return r;
}

int calc_tier_number(node_t *node, uint total_tiers, uint sgx_max) {
    assert(node != NULL);

    /* Since its treated as an index, it is reduced by 1 */
    int tier;
    tier = (int) ceilf(total_tiers * (node->sgx_time / (float) sgx_max)) -1;
    assert(0 <= tier && tier < total_tiers);

    return tier;
}

static int comp_address(const void *a, const void *b) {
    static_assert(sizeof(long) == 8, "size of long is not 8");
    long _a = (long) a;
    long _b = (long) b;
    long c = _a - _b;
    if (c > 0) {
        return 1;
    } else if (c == 0) {
        return 0;
    } else {
        return -1;
    }
}

int nrwlock_timedxlocks(int rw, uint locks, const struct timespec *time, ...) {
    assert(locks > 0);
    pthread_rwlock_t **locks_list = calloc(locks, sizeof(pthread_rwlock_t *));
    if (locks_list == NULL) {
        perror("rwlock_timedrdlock calloc");
        return 0;
    }

    va_list locks_ptr;
    va_start(locks_ptr, time);
    for(uint i = 0; i < locks; i++) {
        locks_list[i] = va_arg(locks_ptr, pthread_rwlock_t*);
    }
    va_end(locks_ptr);

    /* Order according to locks's memory address */
    qsort(locks_list, locks, sizeof(pthread_rwlock_t *), comp_address);

    int locked = 1;
    int i;
    int errv = errno;
    int (*rwlock_func)(pthread_rwlock_t *, const struct timespec *) = (rw ? pthread_rwlock_timedwrlock : pthread_rwlock_timedrdlock);
    for(i = 0; i < locks && locked; i++) {
        locked = locked && rwlock_func(locks_list[i], time) == 0;
        errv = errno;
    }

    if (!locked) {
        ERR("Could not lock all wrlocks");
        i--; // the last one could no be locked
        for(; i >= 0; i--) {
            pthread_rwlock_unlock(locks_list[i]);
        }
    }

    errno = errv;
    return locked;
}

int nrwlock_unlocks(uint locks, ...) {
    assert(locks > 0);
    pthread_rwlock_t **locks_list = calloc(locks, sizeof(pthread_rwlock_t *));
    if (locks_list == NULL) {
        perror("nrwlock_unlock calloc");
        return 0;
    }

    va_list locks_ptr;
    va_start(locks_ptr, locks);
    for(int i = 0; i < locks; i++) {
        locks_list[i] = va_arg(locks_ptr, pthread_rwlock_t *);
    }
    va_end(locks_ptr);

    qsort(locks_list, locks, sizeof(pthread_rwlock_t *), comp_address);

    int unlocked = 1;
    int errv = errno;
    for(int i = locks-1; i >= 0; i--) {
        unlocked = unlocked && pthread_rwlock_unlock(locks_list[i]) == 0;
        errv = errno;
    }

    errno = errv;
    return unlocked;
}

#ifdef __cplusplus
}
#endif
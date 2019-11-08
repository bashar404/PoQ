#include <json-parser/json.h>

#include <cassert>
#include <cstring>
#include <cmath>
#include <cerrno>
#include <queue>

#include "queue_t.h"
#include "general_structs.h"
#include "poet_shared_functions.h"

// BFS
json_value *find_value(json_value *u, const char *name) {
    assert(u != nullptr);
    assert(name != nullptr);

    json_value *r = nullptr;
    queue_t *queue = queue_constructor();
    queue_push(queue, u);

    while (!queue_is_empty(queue) && r == nullptr) {
        u = (json_value *) queue_front(queue);
        queue_pop(queue);

        switch (u->type) {
            case json_object:
                for (int i = 0; i < u->u.object.length && r == nullptr; i++) {
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

int calc_tier_number(const node_t &node, uint total_tiers, uint sgx_max) {
    /* Since its treated as an index, it is reduced by 1 */
    int tier;
    tier = (int) ceilf(((float) total_tiers * node.sgx_time) / (float) sgx_max) -1;
    assert(0 <= tier && tier < total_tiers);

    return tier;
}

std::vector<uint> calc_quantum_times(const std::vector<node *> &sgx_table, uint ntiers, uint sgx_max) {
    assert(ntiers > 0);
    assert(sgx_max > 0);

    std::vector<uint> quantum_times(ntiers, 0);
    std::vector<uint> tier_active_nodes(ntiers, 0);

    for(auto node_i = sgx_table.begin(); node_i != sgx_table.end(); node_i++) {
        int tier = calc_tier_number(*(*node_i), ntiers, sgx_max);
        printf("tier: %d\n", tier);
        quantum_times[tier] += (*node_i)->time_left;
        printf("accumulated quantum time: %d\n", quantum_times[tier]);
        tier_active_nodes[tier]++;
        printf("tier active nodes: %d\n", tier_active_nodes[tier]);
    }

    for(int i = 0; i < quantum_times.size(); i++) {
        uint &qt = quantum_times[i];
        uint &nn = tier_active_nodes[i];

        qt = (uint) ceilf(((float) qt) / ((float) nn*nn));
        printf("Quantum time of tier %d: %u\n", i, qt);
    }

    return quantum_times;
}

static void copy_queuet_std_queue(void * node_ptr, void * std_queue_ptr) {
    auto &q = *((std::queue<uint> *) std_queue_ptr);
    auto node = (uint) ((long long) (node_ptr));

    q.push(node);
}

static uint calc_qt(node_t *u, const std::vector<uint> &quantum_times, uint ntiers, uint sgx_max) {
    assert(u != nullptr);

    int tier = calc_tier_number(*u, ntiers, sgx_max);

    return quantum_times[tier];
}

time_t calc_starting_time(queue_t *queue, const std::vector<node_t *> &sgx_table, const node_t &current_node, uint ntiers, uint sgx_max) {
    std::queue<uint> q;
    queue_print_func_dump((queue_t *) queue, copy_queuet_std_queue, &q);

    auto quantum_times = calc_quantum_times(sgx_table, ntiers, sgx_max);

    for(int i = 0; i < sgx_table.size(); i++) {
        printf("Qt(%d) = %u\n", i, calc_qt(sgx_table[i], quantum_times, ntiers, sgx_max));
    }

    /* **************** */

    time_t starting_time = 0;

    if (q.front() == current_node.node_id) {
        return starting_time;
    }

    uint u = q.front(); q.pop();

    uint lowest_at = 0; // TODO: change to -1
    for(int i = 0; i < sgx_table.size(); i++) {
        lowest_at = std::min(lowest_at, sgx_table[i]->arrival_time);
    }

    int u_tier = calc_tier_number(*sgx_table[u], ntiers, sgx_max);

    uint accumulate_tier_qt = 0;
    uint count_lowest_at = 0;
    for(int i = 0; i < sgx_table.size(); i++) {
        int tier = calc_tier_number(*sgx_table[i], ntiers, sgx_max);
        accumulate_tier_qt += (tier == u_tier ? sgx_table[i]->time_left : 0);
        count_lowest_at += (tier == u_tier ? (lowest_at == sgx_table[i]->arrival_time || 1) : 0); // TODO change (remove || 1)
    }

    uint tier_qt = (uint) ceilf(accumulate_tier_qt / (float) (count_lowest_at * count_lowest_at));
    starting_time += tier_qt;

    while(!q.empty() && sgx_table[q.front()]->node_id != current_node.node_id) {
        u = q.front(); q.pop();
        time_t qt = calc_qt(sgx_table[u], quantum_times, ntiers, sgx_max);
        starting_time += qt;
    }

    ERR("Calculated Starting time: %lu\n", starting_time);

    return starting_time;
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
    auto locks_list = (pthread_rwlock_t **) calloc(locks, sizeof(pthread_rwlock_t *));
    if (locks_list == nullptr) {
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
    auto locks_list = (pthread_rwlock_t **) calloc(locks, sizeof(pthread_rwlock_t *));
    if (locks_list == nullptr) {
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

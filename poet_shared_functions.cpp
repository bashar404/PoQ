#include "poet_shared_functions.h"

#define JSON_ERROR_LEN 30

int check_json_compliance(const char *buffer, size_t buffer_len) {
    assert(buffer != nullptr);
    assert(buffer_len >= 0);

    if (buffer_len == 0) return 0;

    JSON_checker jc = new_JSON_checker(buffer_len);

    int is_valid = 1;
    for (int current_pos = 0; (current_pos < buffer_len) && (buffer[current_pos] != '\0') && is_valid; current_pos++) {
        int next_char = buffer[current_pos];

        is_valid = JSON_checker_char(jc, next_char);

        if (!is_valid) {
            WARN("JSON_checker_char: syntax error\n");
        }
    }

    is_valid = is_valid && JSON_checker_done(jc);
    if (!is_valid) {
        WARN("JSON_checker_end: syntax error\n");
        int len = std::min(buffer_len, (size_t) JSON_ERROR_LEN);
        WARN("JSON with invalid syntax: [%.*s]\n", len, buffer + (buffer_len - len));
    }

    return is_valid;
}

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

json_value * check_json_success_status(char *buffer, size_t len) {
    int state = 1;
    json_value *json = nullptr;
    state = state && check_json_compliance(buffer, len);
    if (state) {
        json = json_parse(buffer, len);
        json_value *status = find_value(json, "status");
        if (status != nullptr && status->type == json_string) {
            state = strcmp(status->u.string.ptr, "success") == 0;
        }
    }

    return state ? json : nullptr;
}

int calc_tier_number(const node_t &node, uint total_tiers, uint sgx_max) {
    /* Since its treated as an index, it is reduced by 1 */
    int tier;
    tier = (int) ceilf(((float) total_tiers * node.sgx_time) / (float) sgx_max) -1;
    assert(0 <= tier && tier < total_tiers);

    return tier;
}

static void copy_queuet_std_queue(void * node_ptr, void * std_queue_ptr) {
    auto &q = *((std::queue<uint> *) std_queue_ptr);
    auto node = (uint) ((long long) (node_ptr));

    q.push(node);
}

std::vector<uint> calc_quantum_times(const std::vector<node *> &sgx_table, uint ntiers, uint sgx_max, time_t node_current_time, time_t server_starting_time) {
    assert(ntiers > 0);
    assert(sgx_max > 0);

    std::vector<uint> quantum_times(sgx_table.size(), 0);
    std::vector<uint> tier_active_nodes(sgx_table.size(), 1);

    for(auto node_i : sgx_table) { // Can be changed to O(n) instead of O(n^2)
        int tier_i = calc_tier_number(*node_i, ntiers, sgx_max);
        quantum_times[node_i->node_id] += node_i->sgx_time;
        for(auto node_j : sgx_table) {
            if (node_i == node_j) continue;

            int tier_j = calc_tier_number(*node_j, ntiers, sgx_max);
            if ((tier_i == tier_j) && (node_j->arrival_time <= node_i->arrival_time)) {
                quantum_times[node_i->node_id] += node_j->time_left;
                tier_active_nodes[node_i->node_id]++;
            }
        }
    }

    for(int i = 0; i < quantum_times.size(); i++) {
        uint &qt = quantum_times[i];
        uint &nn = tier_active_nodes[i];

        qt = nn > 0 ? (uint) ceilf(((float) qt) / ((float) nn*nn)) : 0;
        ERR("Quantum time of node %d: %u\n", i, qt);
    }

#ifdef DEBUG
    for(int i = 0; i < quantum_times.size(); i++) {
        ERR("Node %3d qt: %u\n", i, quantum_times[i]);
    }
#endif

    return quantum_times;
}

static uint calc_qt(node_t *u, const std::vector<uint> &quantum_times, uint ntiers, uint sgx_max) {
    assert(u != nullptr);

    int tier = calc_tier_number(*u, ntiers, sgx_max);

    return quantum_times[tier];
}

static uint remaining_quantum_time(const std::vector<uint> &quantum_times, const node_t &node, int reps, uint tiers, uint sgx_max) {
    assert(reps >= 0);
    int r = 0;
    int quantum_time = quantum_times[node.node_id];
    int sgx_time = node.sgx_time;
    r = std::min(quantum_time, std::max(sgx_time - reps * quantum_time, r));
    return r;
}

time_t calc_leadership_time(queue_t *queue, const std::vector<node_t *> &sgx_table, const node_t &current_node, uint tiers, uint sgx_max, time_t node_current_time, time_t server_starting_time) {
    assert(queue != nullptr);
    std::queue<uint> q;
    queue_print_func_dump((queue_t *) queue, copy_queuet_std_queue, &q);

    /* **************** */

    std::vector<uint> quantum_t_repetitions(sgx_table.size(), 0);
    std::vector<uint> quantum_times = calc_quantum_times(sgx_table, tiers, sgx_max, node_current_time, server_starting_time);

    int accumulated_time = 0;

    int remaining_time = current_node.time_left;
    uint minimum_arrival_time = current_node.arrival_time;

    for(int i = 0; i < sgx_table.size(); i++) {
        minimum_arrival_time = std::min(minimum_arrival_time, sgx_table[i]->arrival_time);
    }

    bool found_myself = false;

    while(!q.empty() && remaining_time > 0) {
        uint u = q.front(); q.pop();
        ERR("Processing node %d\n", u);
        assert(0 <= u && u < sgx_table.size());
        assert(sgx_table[u] != nullptr);

        uint qt = remaining_quantum_time(quantum_times, *sgx_table[u], quantum_t_repetitions[u]++, tiers, sgx_max);
        ERR("Remaining quantum time (node %d): %u\n", u, qt);
        assert(qt >= 0);
        accumulated_time += std::min(qt, (uint) remaining_time);
        if (u == current_node.node_id) {
            found_myself = true;
            remaining_time -= std::min(qt, (uint) remaining_time);
            assert(remaining_time >= 0);
        }

        qt = remaining_quantum_time(quantum_times, *sgx_table[u], quantum_t_repetitions[u], tiers, sgx_max);
        if (qt > 0) {
            ERR("re-adding node %u into queue since he did not finish\n", u);
            q.push(u);
        }
    }

    ERR("Remaining time (should be 0): %d\n", remaining_time);

    assert(!found_myself || remaining_time == 0);
//    assert(accumulated_time > current_node.arrival_time);
//    return std::max(accumulated_time - (int) current_node.arrival_time, (int) current_node.sgx_time);
    long comm_delay = std::max(node_current_time - minimum_arrival_time, (long) 0);
    assert(comm_delay >= 0);
    if (comm_delay) {
        ERR("comm delay was greater than 0: %ld\n", comm_delay);
    }

    ERR("Calculated leadership time: %ld\n", accumulated_time - comm_delay);

    return found_myself ? std::max(accumulated_time - comm_delay, (long) 0) : -1;
}

std::vector<time_t> calc_notification_times(queue_t *queue, const std::vector<node_t *> &sgx_table, const node_t &current_node, uint ntiers, uint sgx_max, time_t node_current_time, time_t server_starting_time) {
    assert(queue != nullptr);
    std::queue<uint> q;
    queue_print_func_dump((queue_t *) queue, copy_queuet_std_queue, &q);

    /* ************* */

    std::vector<time_t> notification_times;

    std::vector<uint> quantum_t_repetitions(sgx_table.size(), 0);
    auto quantum_times = calc_quantum_times(sgx_table, ntiers, sgx_max, node_current_time, server_starting_time);
    int remaining_time = current_node.time_left;
//    assert(current_node.sgx_time == current_node.time_left);
    int accumulated_time = 0;
    uint minimum_arrival_time = current_node.arrival_time;

    for(int i = 0; i < sgx_table.size(); i++) {
        minimum_arrival_time = std::min(minimum_arrival_time, sgx_table[i]->arrival_time);
    }

    long comm_delay = node_current_time - minimum_arrival_time;
    assert(comm_delay >= 0);
    if (comm_delay) {
        ERR("comm delay was greater than 0: %ld\n", comm_delay);
    }

    while(!q.empty() && remaining_time > 0) {
        uint u = q.front(); q.pop();
        assert(0 <= u && u < sgx_table.size());
        assert(sgx_table[u] != nullptr);

        uint qt = remaining_quantum_time(quantum_times, *sgx_table[u], quantum_t_repetitions[u]++, ntiers, sgx_max);
        ERR("Remaining quantum time (node %d): %u\n", u, qt);
        assert(qt >= 0);
        accumulated_time += std::min(qt, (uint) remaining_time);
        if (u == current_node.node_id) {
//            assert(remaining_time >= qt);
            remaining_time -= std::min(qt, (uint) remaining_time);
            assert(remaining_time >= 0);
        }

        qt = remaining_quantum_time(quantum_times, *sgx_table[u], quantum_t_repetitions[u], ntiers, sgx_max);
        if (qt > 0) {
            ERR("Reading node %u into queue since he did not finish\n", u);
            q.push(u);
        }

        if (u == current_node.node_id && remaining_time > 0) {
            notification_times.push_back(std::max(accumulated_time - comm_delay, (long) 0));
        }
    }

    return notification_times;
}

/* TODO should rather be all the starting times of the current node */
time_t calc_starting_time(queue_t *queue, const std::vector<node_t *> &sgx_table, const node_t &current_node, uint ntiers, uint sgx_max, time_t node_current_time, time_t server_starting_time) {
    assert(queue != nullptr);
    std::queue<uint> q;
    queue_print_func_dump((queue_t *) queue, copy_queuet_std_queue, &q);

    auto quantum_times = calc_quantum_times(sgx_table, ntiers, sgx_max, node_current_time, server_starting_time);

    for(int i = 0; i < sgx_table.size(); i++) {
        ERR("Qt(%d) = %u\n", i, calc_qt(sgx_table[i], quantum_times, ntiers, sgx_max));
    }

    /* **************** */

    time_t starting_time = 0;

    if (q.front() == current_node.node_id) {
        return starting_time;
    }

    uint u = q.front(); q.pop();

    uint lowest_at = -1;
    for(int i = 0; i < sgx_table.size(); i++) {
        lowest_at = std::min(lowest_at, sgx_table[i]->arrival_time);
    }

    int u_tier = calc_tier_number(*sgx_table[u], ntiers, sgx_max);

    uint accumulate_tier_qt = 0;
    uint count_lowest_at = 0;
    for(int i = 0; i < sgx_table.size(); i++) {
        int tier = calc_tier_number(*sgx_table[i], ntiers, sgx_max);
        accumulate_tier_qt += (tier == u_tier ? sgx_table[i]->time_left : 0);
        count_lowest_at += (tier == u_tier ? (lowest_at == sgx_table[i]->arrival_time) : 0);
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

int delegate_thread_to_function(pthread_t *thread, void *data, void * (*func)(void *)) {
    return delegate_thread_to_function(thread, data, func, true);
}

int delegate_thread_to_function(pthread_t *thread, void *data, void * (*func)(void *), bool detach) {
    assert(thread != nullptr);
    assert(func != nullptr);

    auto *curr_thread = (struct thread_tuple *) malloc(sizeof(struct thread_tuple));
    curr_thread->thread = thread;
    curr_thread->data = data;
    int ret = pthread_create(thread, nullptr, func, curr_thread);
    if (ret == 0 && detach) {
        /* To avoid a memory leak of pthread, since there is a thread queue we dont want a pthread_join */
        pthread_detach(*thread);
    }
    return ret;
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

int nrwlock_timedxlocks(int rw, int locks, const struct timespec *wait_time, ...) {
    assert(locks > 0);
    auto locks_list = (pthread_rwlock_t **) calloc(locks, sizeof(pthread_rwlock_t *));
    if (locks_list == nullptr) {
        perror("rwlock_timedrdlock calloc");
        return 0;
    }

    va_list locks_ptr;
    va_start(locks_ptr, wait_time);
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
        struct timespec t{};
        memcpy(&t, wait_time, sizeof(struct timespec));
        ERR("time wait for lock: %ld\n", t.tv_sec);
        t.tv_sec += time(nullptr);
        ERR("time wait for lock: %ld\n", t.tv_sec);
        int errnum;
        errnum = rwlock_func(locks_list[i], &t);
        WARN("locked wrlock (rw:%d): %p (errnum:%d)\n", rw, locks_list[i], errnum);
        if (errnum == EDEADLK) {
            WARN("This thread already owns the lock %p\n", locks_list[i]);
            ERR("EDEADLK = %d\n", EDEADLK);
        }
        if (errnum != 0 && errnum != EDEADLK) {
            locked = false;
        }
        errv = errnum;
    }

    if (!locked) {
        ERROR("Could not lock all wrlocks\n");
        i--; // the last one could no be locked
        for(; i >= 0; i--) {
            pthread_rwlock_unlock(locks_list[i]);
        }
    }

    free(locks_list);

    errno = errv;
    return locked;
}

int nrwlock_xlocks(int rw, int locks, ...) {
    assert(locks > 0);
    auto locks_list = (pthread_rwlock_t **) calloc(locks, sizeof(pthread_rwlock_t *));
    if (locks_list == nullptr) {
        perror("rwlock_rdlock calloc");
        return 0;
    }

    va_list locks_ptr;
    va_start(locks_ptr, locks);
    for(uint i = 0; i < locks; i++) {
        locks_list[i] = va_arg(locks_ptr, pthread_rwlock_t*);
    }
    va_end(locks_ptr);

    /* Order according to locks's memory address */
    qsort(locks_list, locks, sizeof(pthread_rwlock_t *), comp_address);

    int locked = 1;
    int i;
    int errv = errno;
    int (*rwlock_func)(pthread_rwlock_t *) = (rw ? pthread_rwlock_wrlock : pthread_rwlock_rdlock);
    for(i = 0; i < locks && locked; i++) {
        int errnum;
        errnum = rwlock_func(locks_list[i]);
        WARN("locked rwlock (rw:%d): %p (errnum:%d)\n", rw, locks_list[i], errnum);
        if (errnum == EDEADLK) {
            WARN("This thread already owns the lock %p\n", locks_list[i]);
            ERR("EDEADLK = %d\n", EDEADLK);
        }
        if (errnum != 0 && errnum != EDEADLK) {
            locked = false;
        }
        errv = errnum;
    }

    if (!locked) {
        ERROR("Could not lock all wrlocks\n");
        i--; // the last one could no be locked
        for(; i >= 0; i--) {
            pthread_rwlock_unlock(locks_list[i]);
        }
    }

    free(locks_list);

    errno = errv;
    return locked;
}

int nrwlock_unlocks(int locks, ...) {
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
    int errv = 0;
    for(int i = locks-1; i >= 0; i--) {
        int errnum = pthread_rwlock_unlock(locks_list[i]);
        if (errnum != 0) {
            ERROR("Could not unlock lock: %p (errnum:%d)\n", locks_list[i], errnum);
        }
        unlocked = unlocked && errnum == 0;
        WARN("unlocked wrlock: %p\n", locks_list[i]);
        errv = std::max(errv, errnum);
    }

    free(locks_list);

    errno = errv;
    return unlocked;
}

int nmutex_locks(int locks, ...) {
    assert(locks > 0);
    ERRR("locks: %d\n", locks);

    auto locks_list = (pthread_mutex_t **) calloc(locks, sizeof(pthread_mutex_t *));
    if (locks_list == nullptr) {
        perror("mutex_lock calloc");
        return 0;
    }

    va_list locks_ptr;
    va_start(locks_ptr, locks);
    for(uint i = 0; i < locks; i++) {
        locks_list[i] = va_arg(locks_ptr, pthread_mutex_t*);
    }
    va_end(locks_ptr);

    /* Order according to locks's memory address */
    qsort(locks_list, locks, sizeof(pthread_mutex_t *), comp_address);

    int locked = 1;
    int i;
    int errv = errno;
    for(i = 0; i < locks && locked; i++) {
        ERRR("Locking lock at %p\n", locks_list[i]);
        locked = locked && pthread_mutex_lock(locks_list[i]) == 0;
        WARN("locked mutex lock: %p\n", locks_list[i]);
        errv = errno;
    }

    if (!locked) {
        ERROR("Could not lock all mutexes\n");
        i--; // the last one could no be locked
        for(; i >= 0; i--) {
            pthread_mutex_unlock(locks_list[i]);
        }
    }

    free(locks_list);

    errno = errv;
    return locked;
}

int nmutex_unlocks(int locks, ...) {
    assert(locks > 0);
    ERRR("unlocks: %d\n", locks);

    auto locks_list = (pthread_mutex_t **) calloc(locks, sizeof(pthread_mutex_t *));
    if (locks_list == nullptr) {
        perror("nmutex_unlock calloc");
        return 0;
    }

    va_list locks_ptr;
    va_start(locks_ptr, locks);
    for(int i = 0; i < locks; i++) {
        locks_list[i] = va_arg(locks_ptr, pthread_mutex_t *);
    }
    va_end(locks_ptr);

    qsort(locks_list, locks, sizeof(pthread_mutex_t *), comp_address);

    int unlocked = 1;
    int errv = 0;
    for(int i = locks-1; i >= 0; i--) {
        ERRR("Unlocking lock at %p\n", locks_list[i]);
        int errnum = pthread_mutex_unlock(locks_list[i]);
        if (errnum != 0) {
            ERROR("Could not unlock lock: %p\n", locks_list[i]);
        }
        unlocked = unlocked && errnum == 0;
        WARN("unlocked mutex lock: %p\n", locks_list[i]);
        errv = std::max(errv, errnum);
    }

    free(locks_list);

    errno = errv;
    return unlocked;
}
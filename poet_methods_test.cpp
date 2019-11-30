#include <bits/stdc++.h>
#include <unistd.h>
#include "poet_common_definitions.h"
#include "poet_shared_functions.h"
#include "queue_t.h"
#include "socket_t.h"

void test_leadership_time() {
    std::vector<node_t *> sgx_table(4, nullptr);

    for(int i = 0; i < 4; i++) {
        sgx_table[i] = (node_t *) calloc(1, sizeof(node_t));
    }

    *sgx_table[0] = {0, 0, 10, 0, 10};
    *sgx_table[1] = {1, 0, 4, 0, 4};
    *sgx_table[2] = {2, 0, 2, 0, 2};
    *sgx_table[3] = {3, 0, 9, 0, 9};

    queue_t *queue = queue_constructor();
    queue_push(queue, (void *) 0);
    queue_push(queue, (void *) 1);
    queue_push(queue, (void *) 2);
    queue_push(queue, (void *) 3);
//    queue_push(queue, (void *) 0);
//    queue_push(queue, (void *) 1);
//    queue_push(queue, (void *) 3);

    time_t time = calc_leadership_time(queue, sgx_table, *sgx_table[3], 2, 10, 0, 0);
    INFO("time: %lu\n", time);
    assertp(time == 25);
}

void *test_thread_locks(void * arg) {
    auto list = (pthread_mutex_t **) arg;
    pthread_mutex_t &lock1 = *list[0];
    pthread_mutex_t &lock2 = *list[1];

    printf("2 time: %lu\n", time(nullptr));
    assertp(mutex_locks(&lock1, &lock2));

    printf("2 time: %lu\n", time(nullptr));

    mutex_unlocks(&lock2, &lock1);

}

void test_locks_methods() {
    pthread_t thread;

    pthread_mutex_t lock1, lock2;
    auto list = (pthread_mutex_t **) calloc(2, sizeof(pthread_mutex_t *));
    list[0] = &lock1;
    list[1] = &lock2;
    assertp(pthread_mutex_init(&lock1, nullptr) == 0);
    assertp(pthread_mutex_init(&lock2, nullptr) == 0);

    assertp(mutex_locks(&lock2, &lock1));

    pthread_create(&thread, nullptr, test_thread_locks, list);

    printf("1 time: %lu\n", time(nullptr));
    sleep(2);
    printf("1 time: %lu\n", time(nullptr));

    mutex_unlocks(&lock1, &lock2);

    pthread_join(thread, nullptr);

    pthread_mutex_destroy(&lock1);
    pthread_mutex_destroy(&lock2);

    free(list);
}

int main() {
    test_leadership_time();
    test_locks_methods();
}
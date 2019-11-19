#include <bits/stdc++.h>
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
    queue_push(queue, (void *) 0);
    queue_push(queue, (void *) 1);
    queue_push(queue, (void *) 3);

    time_t time = calc_leadership_time(queue, sgx_table, *sgx_table[3], 2, 10);
    INFO("time: %lu\n", time);
    assertp(time == 25);
}

int main() {
    test_leadership_time();
}
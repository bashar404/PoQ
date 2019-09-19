/*H**********************************************************************
 * FILENAME : SGX1.c
 * ORGANIZATION : ISPM Research Lab
 * DESCRIPTION: Maximum Node 100, Anything with start from time=0,
 *H*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>

//change this to NDEBUG if don't want to check assertions <assert(...)>
#define DEBUG

#include "queue_t.h"

#ifndef max(a, b)
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#endif

#define MAX_SIZE 10000

/*********************************************************************/

struct node {
    int arrival_time;
    int sgx_time;
    int n_leadership;
    int time_left;
};

typedef struct node node_t;
typedef unsigned int uint;

node_t nodes[MAX_SIZE];

uint node_count, sgx_max, arrival_time_max, tier_count, total_tiers, current_time;
uint nodes_queue[MAX_SIZE], elapsed_time[MAX_SIZE], wait_times[MAX_SIZE], tier_active_nodes[MAX_SIZE], tier_quantum_time[MAX_SIZE];
float sumT[MAX_SIZE];

queue_t *queue = NULL;

uint randint(int start, int end) {
    assert(start <= end);
    start = max(0, min(start, RAND_MAX));
    end = max(0, min(end, RAND_MAX));

    return start + rand() % end;
}

void get_input_from_user(int prompt) {
    if (prompt) printf("Seed for pseudo-random number generator (-1 for random): ");
    int seed;
    scanf("%d", &seed);
    seed = seed >= 0 ? max(0,seed) : (int) time(NULL);
    srand(seed);

    if (prompt) printf("Number of nodes in the network: ");
    scanf("%d", &node_count);

    if (prompt) printf("SGXtime upper bound: ");
    scanf("%d", &sgx_max);

    if (prompt) printf("Total number of tiers: ");
    scanf("%d", &total_tiers);

    if (prompt) printf("Arrival maximum time: ");
    scanf("%d", &arrival_time_max);
    for (int i = 0; i < node_count; i++) {
        int b = randint(1, sgx_max);
        int a = randint(0, 10); // FIXME: what is this used for?
	    int at = randint(0, arrival_time_max); // arrival time is randomly generated
        nodes[i].arrival_time = at;
        nodes[i].sgx_time = b;
        nodes[i].n_leadership = 0;
        nodes[i].time_left = nodes[i].sgx_time;
    }

    tier_count = (int) ceilf(sgx_max / (float) total_tiers);
}

void print_sgx_table() {
    printf("Pass:\tArrivaltime\tSGXtime\t#Leader\ttimeLeft\n");
    for (int i = 0; i < node_count; i++) {
        printf("[Node%03d]:\t%5d\t%5d\t%5d\t%5d\n",
                i,
                nodes[i].arrival_time,
                nodes[i].sgx_time,
                nodes[i].n_leadership,
                nodes[i].time_left);
    }
}

int is_time_left() {
    int b = 0;
    for (int i = 0; i < node_count; i++) {
        // if any node has remaining time it returns true
        b = (nodes[i].time_left > 0);
        nodes[i].n_leadership = (nodes[i].time_left == 0); // FIXME: shouldn't this be incremented by 1?
    }
    return b;
}

void calculate_quantum_time() {
    for (int i = 1; i <= tier_count; i++) {
        sumT[i] = 0;
        tier_active_nodes[i] = 0;
    }

    int temptier = 0;
    for (int i = 0; i < node_count; i++) {
        if (current_time >= nodes[i].arrival_time) {
            temptier = (int) ceilf(nodes[i].sgx_time / (float) total_tiers);
            tier_active_nodes[temptier] += 1;
            sumT[temptier] = sumT[temptier] + nodes[i].time_left;
        }
    }

    for (int i = 1; i <= tier_count; i++) {
        if (sumT[i] != 0) {
            float nt = tier_active_nodes[i];
            float sval = sumT[i] / (nt*nt);
            tier_quantum_time[i] = ceilf(sval);
        }
    }

    printf("CURRENT time: %d\n", current_time);
    for (int i = 1; i <= tier_count; i++) {
        printf("Quantum time for tier %d: %d\n", i, tier_quantum_time[i]);
        printf("Nodes in tier %d: %d\n", i, tier_active_nodes[i]);
    }
}

void node_arrive() {
    for (int i = 0; i < node_count; i++) { /*when index[i=0] means AT is zero*/
        if (nodes[i].arrival_time == current_time) { /*time = 0 already declared*/
            calculate_quantum_time();
            queue_push(queue, i);
        }
    }
}

void arrange() {
    int current_node;

    node_arrive();
    while (is_time_left()) {
        // if queue is empty, no node arrived, increment the time
        if (queue_is_empty(queue)) {
            current_time++;
            node_arrive();
        } else { // some nodes in the nodes_queue
            current_node = queue_front(queue);
            queue_pop(queue);

            int temptier = 0;
            temptier = (int) ceilf(nodes[current_node].sgx_time / (float) total_tiers);
            int qt = (int) tier_quantum_time[temptier];

            if (nodes[current_node].time_left < qt) {
                qt = nodes[current_node].time_left;
            }

            for (uint i = qt; i > 0; i--) {
                nodes_queue[current_time] = current_node;
                nodes[current_node].time_left--; //reducing the remaining time
                current_time++;
                node_arrive(); // keeping track if any node join
            }

            if (nodes[current_node].time_left > 0) { // if nodes has SGX time left, then push to the queue
                queue_push(queue, current_node);
            }
        }

        print_sgx_table();
    }
}

void show_overall_queue() {
    printf("Overall Queue:\n");
    printf("-------------\n");
    for (int i = 0; i <= current_time; i++) {
        printf("[Node%d]", nodes_queue[i]);
    }
    printf("\n");

    /*********************************/

    printf("Waiting time:\n");
    printf("------------\n");
    for (unsigned int i = 0; i < node_count; i++) {
        printf("WT Node%d: %d\n", i, wait_times[i]);
    }

    /*********************************/

    float average_wait_time = 0.0f;
    for (unsigned int i = 0; i < node_count; i++) {
        average_wait_time = average_wait_time + wait_times[i];
    }
    average_wait_time = average_wait_time / node_count;
    printf("Avg Waiting time: %f\n", average_wait_time);
    /* Standard Deviation for Waiting taem */
    float st_deviation = 0.0f;
    for(int i = 0; i < node_count; i++) {
        st_deviation += (wait_times[i] - average_wait_time) * (wait_times[i] - average_wait_time);
    }
    st_deviation = sqrtf(st_deviation / ((float)node_count - 1));
    printf("Standard Deviation for Waiting time: %f\n", st_deviation);
}

void waiting_time() {
    uint release_time, t;
    for (uint i = 0; i < node_count; i++) {
        for (t = current_time - 1; nodes_queue[t] != i; t--);
        release_time = t + 1;
        wait_times[i] = release_time - nodes[i].arrival_time - nodes[i].sgx_time;
    }
}

void average_estimated_time() {
    uint release_time, t;
    float avg_elapsed_time = 0.0f;
    printf("Elapsed time:\n");
    printf("------------\n");
    for (uint i = 0; i < node_count; i++) {
        for (t = current_time - 1; nodes_queue[t] != i; t--); // TODO: this can be improved with a memorization table
        release_time = t + 1;
        elapsed_time[i] = release_time - nodes[i].arrival_time;

        printf("ET Node%d: %d\n", i, elapsed_time[i]);
        avg_elapsed_time += elapsed_time[i];
    }

    avg_elapsed_time /= node_count;
    printf("Avg Elapsed time: %f\n", avg_elapsed_time);

    /* Standard Deviation for Elapsed time */
    float st_deviation = 0.0f;
    for(int i = 0; i < node_count; i++) {
        float tmp = (float) (elapsed_time[i] - avg_elapsed_time);
        st_deviation += tmp*tmp;
    }

    st_deviation = sqrtf(st_deviation / (float) (node_count - 1));
    printf("Standard Deviation for Elapsed time: %f\n", st_deviation);
}

void pause_for_user(int promptUser, int clearStream) {
    if (clearStream) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        clearerr(stdin);
    }
    if (promptUser) fputs("Press [Enter] to continue", stdout);
    getchar();
}

int main(int argc, char *argv[]) {
    /* Variables initialization */
    current_time = 0;
    queue = queue_constructor();

    int show_user_prompt = argc <= 1; // FIXME: temporary
    get_input_from_user(show_user_prompt);

    print_sgx_table();
    arrange();
    waiting_time();
    show_overall_queue();
    average_estimated_time();

#ifdef __linux__
    int promptUser = 0;
#else
    int promptUser = 1;
#endif

    pause_for_user(promptUser, promptUser);
    queue_destructor(queue);

    return 0;
}

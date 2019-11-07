/*H**********************************************************************
 * FILENAME : SGX1.c
 * ORGANIZATION : ISPM Research Lab
 *H*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <signal.h>

#include "poet_common_definitions.h"

#include "queue_t.h"
#include "general_structs.h"

#define ffprintf(...) do{ fprintf(out, __VA_ARGS__); printf(__VA_ARGS__); } while(0)

#define MAX_SIZE 20000000
#define MAX_ITERATIONS 100000

/*********************************************************************/

node_t sgx_table[MAX_SIZE];

int max_iterations;
int should_terminate = 0;
int nodes_rejoin;

uint node_count, sgx_max, arrival_time_max, total_tiers, current_time;
uint nodes_queue[MAX_SIZE], elapsed_time[MAX_SIZE], wait_times[MAX_SIZE], tier_active_nodes[MAX_SIZE], tier_quantum_time[MAX_SIZE];
float sumT[MAX_SIZE];

FILE *out;

queue_t *queue = NULL;

static int calc_tier_number(node_t *node);

uint randint(int start, int end) {
    assert(start <= end);
    start = max(0, min(start, RAND_MAX));
    end = max(0, min(end, RAND_MAX));

    return end > 0 ? start + rand() % end : 0;
}

void get_input_from_user(int prompt) {
    if (prompt) printf("Seed for pseudo-random number generator (-1 for random): ");
    int seed;
    scanf("%d", &seed);
    seed = seed >= 0 ? max(0,seed) : (int) time(NULL);
    srand(seed);

    printf("Max number of iterations (-1 for infinite): ");
    scanf("%d", &max_iterations);

    if (prompt) printf("Number of nodes in the network: ");
    scanf("%d", &node_count);

    if (prompt) printf("SGXtime upper bound: ");
    scanf("%d", &sgx_max);

    if (prompt) printf("Total number of tiers: ");
    scanf("%d", &total_tiers);

    int take_from_stdin = 0;
    printf("Get SGXtable from user (0 for no): ");
    scanf("%d", &take_from_stdin);

    printf("Rejoin nodes after they finish (0 for no): ");
    scanf("%d", &nodes_rejoin);

    if (prompt && !take_from_stdin) {
        printf("Arrival maximum time for first pass: ");
        scanf("%d", &arrival_time_max);
    } else {
        arrival_time_max = 0;
    }

    for (int i = 0; i < node_count; i++) {
        int time, at;
        if (!take_from_stdin) {
            time = randint(1, sgx_max);
            at = randint(0, arrival_time_max); // arrival time is randomly generated
        } else {
            scanf("%d%d", &at, &time);
        }
        sgx_table[i].arrival_time = at;
        sgx_table[i].sgx_time = time;
        sgx_table[i].n_leadership = 0;
        sgx_table[i].time_left = sgx_table[i].sgx_time;
    }

    node_t tmp;
    for(size_t time = 1; time <= sgx_max; time++) {
        tmp.sgx_time = time;
        printf("(%lu)%d,", time, calc_tier_number(&tmp));
    }
    printf("\n");

}

static int calc_tier_number(node_t *node) {
    /* Since its treated as an index, it is reduced by 1 */
    int tier;
    tier = (int) ceilf(total_tiers * (node->sgx_time / (float) sgx_max)) -1;

#ifdef DEBUG
    if (! (0 <= tier && tier < total_tiers)){
        ERR("0 <= %d < %d\n", tier, total_tiers);
    }
#endif
    assert(0 <= tier && tier < total_tiers);

    return tier;
}

void print_sgx_table() {
    ffprintf("Pass:\tArrivaltime\tSGXtime\t#Leader\ttimeLeft\n");
    for (int i = 0; i < node_count; i++) {
        ffprintf("[Node%03d]:\t%5d\t%5d\t%5d\t%5d\n",
               i,
               sgx_table[i].arrival_time,
               sgx_table[i].sgx_time,
               sgx_table[i].n_leadership,
               sgx_table[i].time_left);
    }
}

int is_time_left() {
    int b = 0;
    for (int i = 0; i < node_count; i++) {
        // if any node has remaining time it returns true
        b = b || (sgx_table[i].time_left > 0);
        int previous_leadership = sgx_table[i].n_leadership;
        sgx_table[i].n_leadership += (sgx_table[i].time_left == 0);
        if (nodes_rejoin && previous_leadership != sgx_table[i].n_leadership) {
            ffprintf("The leader of this pass is [Node%03d]\n", i);
            sgx_table[i].time_left = sgx_table[i].sgx_time = randint(1, sgx_max);
            sgx_table[i].arrival_time = current_time+1;
        }
    }
    return b;
}

void calculate_quantum_time() {
    for (int i = 0; i < total_tiers; i++) {
        sumT[i] = 0;
        tier_active_nodes[i] = 0;
    }

    for (int i = 0; i < node_count; i++) {
        if (current_time >= sgx_table[i].arrival_time) {
            assert(sgx_table[i].sgx_time > 0);
            int temptier = calc_tier_number(&sgx_table[i]);
            tier_active_nodes[temptier] += 1;
            sumT[temptier] += sgx_table[i].time_left;
        }
    }

    for (int i = 0; i < total_tiers; i++) {
        assert(sumT[i] >= 0);
        if (sumT[i] > 0) {
            float nt = tier_active_nodes[i];
            float sval = sumT[i] / (nt*nt);
            tier_quantum_time[i] = ceilf(sval);
        }
    }

    ffprintf("CURRENT time: %d\n", current_time);
    for (int i = 0; i < total_tiers; i++) {
        ffprintf("Quantum time for tier %d: %u\n", i, tier_quantum_time[i]);
        ffprintf("Nodes in tier %d: %u\n", i, tier_active_nodes[i]);
    }
}

void check_node_arrive() {
    for (int i = 0; i < node_count; i++) { /*when index[i=0] means AT is zero*/
        if (sgx_table[i].arrival_time == current_time) { /*time = 0 already declared*/
            calculate_quantum_time();
            queue_push(queue, (void*) i);
        }
    }
}

void arrange() {
    int current_node;

    check_node_arrive();
    while (is_time_left() && (max_iterations == -1 || current_time < max_iterations) && !should_terminate) {
        // if queue is empty, no node arrived, increment the time
        if (queue_is_empty(queue)) {
            current_time++;
            check_node_arrive();
        } else { // some sgx_table in the queue
            current_node = (int) queue_front(queue);
            queue_pop(queue);

            int temptier = calc_tier_number(&sgx_table[current_node]);
            uint qt = (uint) tier_quantum_time[temptier];

            qt = min(qt, sgx_table[current_node].time_left);

            for (uint i = qt; i > 0; i--) {
                nodes_queue[current_time] = current_node;
                sgx_table[current_node].time_left--; //reducing the remaining time
                current_time++;
                check_node_arrive(); // keeping track if any node joins
            }

            if (sgx_table[current_node].time_left > 0) { // if sgx_table has SGX time left, then push to the queue
                queue_push(queue, (void *) current_node);
            }
        }

        print_sgx_table();
    }
}

void show_overall_queue() {
    ffprintf("Overall Queue:\n");
    ffprintf("-------------\n");
    for (int i = 0; i < current_time; i++) {
        ffprintf("[Node%d]", nodes_queue[i]);
    }
    ffprintf("\n");

    /*********************************/
//
//    printf("Waiting time:\n");
//    printf("------------\n");
//    for (unsigned int i = 0; i < node_count; i++) {
//        printf("WT Node%d: %u\n", i, wait_times[i]);
//    }

    /*********************************/

//    float average_wait_time = 0.0f;
//    for (unsigned int i = 0; i < node_count; i++) {
//        average_wait_time = average_wait_time + wait_times[i];
//    }
//    average_wait_time = average_wait_time / node_count;
//    printf("Avg Waiting time: %f\n", average_wait_time);
//    /* Standard Deviation for Waiting taem */
//    float st_deviation = 0.0f;
//    for(int i = 0; i < node_count; i++) {
//        st_deviation += (wait_times[i] - average_wait_time) * (wait_times[i] - average_wait_time);
//    }
//    st_deviation = sqrtf(st_deviation / ((float)node_count - 1));
//    printf("Standard Deviation for Waiting time: %f\n", st_deviation);
}

void waiting_time() { // DEBUG
    uint release_time;
    int t;
    for (uint i = 0; i < node_count; i++) {
        for (t = (int) current_time - 1; t >= 0 && nodes_queue[t] != i; t--);
        release_time = t + 1;
        wait_times[i] = release_time - sgx_table[i].arrival_time - sgx_table[i].sgx_time;
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
        elapsed_time[i] = release_time - sgx_table[i].arrival_time;

        printf("ET Node%d: %u\n", i, elapsed_time[i]);
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

void print_average_leadership() {
    ffprintf("Average and standard deviation of leadership\n");
    ffprintf("----------------------");

    float sum = 0.0f;
    float std = 0.0f;

    for(int i = 0; i < (int) node_count; i++) {
        sum += sgx_table[i].n_leadership;
    }

    sum /= (float) node_count;

    for(int i = 0; i < (int) node_count; i++) {
        float sq = sgx_table[i].n_leadership - sum;
        sq *= sq;
        std += sq;
    }

    std /= ((float) node_count -1);
    std = sqrtf(std);

    ffprintf("average: %.3f || std: %.3f\n", sum, std);
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

// Define the function to be called when ctrl-c (SIGINT) signal is sent to process
static void signal_callback_handler(int signum) {
    should_terminate = 1;
//    exit(SIGINT);
}

int main(int argc, char *argv[]) {
    /* Variables initialization */
    current_time = 0;
    signal(SIGINT, signal_callback_handler);
    
    memset(nodes_queue, 0, sizeof(nodes_queue));
    memset(elapsed_time, 0, sizeof(elapsed_time));
    memset(wait_times, 0, sizeof(wait_times));
    memset(tier_active_nodes, 0, sizeof(tier_active_nodes));
    memset(tier_quantum_time, 0, sizeof(tier_quantum_time));

    queue = queue_constructor();

    out = fopen("out.txt", "w");

    int show_user_prompt = argc <= 1; // FIXME: temporary
    get_input_from_user(show_user_prompt);

    ERR("********* Begins execution *********\n");
    time_t start_time = time(NULL);
    struct tm  ts;
    ts = *localtime(&start_time);
    char buf[80];
    ffprintf("Start time: %s\n", buf);

    print_sgx_table();
    arrange();
    waiting_time();
    show_overall_queue();
//    average_estimated_time();
    print_average_leadership();

#ifdef __linux__
    int promptUser = 0;
#else
    int promptUser = 1;
#endif
    time_t finish_time = time(NULL);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    printf("%s\n", buf);
    ffprintf("Finish time: %s\n", buf);
    ffprintf("Time difference: %lu\n", finish_time - start_time);


    pause_for_user(promptUser, promptUser);
    queue_destructor(queue, 0);

#ifdef DEBUG
    /* General invariants after execution */
    for(int i = 0; i < node_count; i++) {
        if (sgx_table[i].time_left > 0) {
            ERR("Node %d Time left: %d\n", i, sgx_table[i].time_left);
        }
    }
#endif

    fclose(out);

    return 0;
}

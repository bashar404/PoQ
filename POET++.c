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

#include "queue.h"

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

/*********************************************************************/

//TODO: change nodes variable into a list of structs for each node

uint nodes[10000][4], n, i, nodes_queue[100000], taem = 0, q = 0, x, elapsed_time[10000], tym = 0, ct, upsgx, maxat, tiercount, tierdiv;
float QTT[10000], sumT[10000], ncT[10000], wait_times[10000];

typedef struct linkedList {
    unsigned int N; /*N --> Index*/
    struct linkedList * next; /*pointer to the next element*/
} Q; /*destructor*/

Q * queue = NULL; /*At Initial stage there nothing in the queue */

uint randint(int start, int end) {
    assert(start<=end);
    start = max(0, min(start, RAND_MAX));
    end = max(0, min(end, RAND_MAX));

    return start + rand() % end;
}

void get_input_from_user(int show_prompt) {
    if (show_prompt) printf("Seed for pseudo-random number generator (-1 to set it automatically): ");
    uint seed;
    scanf("%u", &seed);
    seed = seed < 0 ? max(0,seed) : (uint) time(NULL);
    srand(seed);

    if (show_prompt) printf("Number of nodes in the network: ");
    scanf("%d",&n);

    if (show_prompt) printf("SGXtime upper bound: ");
    scanf("%d",&upsgx);

    if (show_prompt) printf("Split for tiers: ");
    scanf("%d",&tierdiv);

    if (show_prompt) printf("Arrival maximum time: ");
    scanf("%d",&maxat);
    for (i = 0; i < n; i++) {
        int b = randint(1, upsgx);
        int a = randint(0, 10);
	    int at = randint(0, maxat); // arrival time is randomly generated
        nodes[i][0] = at;
        nodes[i][1] = b;
        nodes[i][2] = 0;
        nodes[i][3] = nodes[i][1];
    }

    float uval = (float) upsgx;
    float tval = (float) tierdiv;
    tiercount = ceil((double) uval/tval);
}

void simulate_poet() {
    printf("Pass     :\tArrivaltime\tSGXtime\t#Leader\ttimeLeft\n");
    for (i = 0; i < n; i++) {
        printf("[Node%03d]:\t%5d\t%5d\t%5d\t%5d\n", i, nodes[i][0], nodes[i][1], nodes[i][2], nodes[i][3]);
    }
    // printf("\nNext Node:");
    Q * queue = NULL;
    /*Each time when simulate_poet() is called than it will show whose next and initially Q is zero*/
    Q * n;
//    for (n = queue; n != NULL; n = n->next) {
//        printf("P%d", n->N);
//    }

}

uint time_left() {
    x = 0; // as boolean, 0 means false
    for (int i = 0; i < n; i++) {
        // if any node has remaining time it returns true
        if (nodes[i][3] > 0) {
            x = 1;
        }

        if (nodes[i][3] == 0) {
            nodes[i][2] = 1;
        }
    }
    return x;
}

void updateLL(int k) {
    Q * n, * n1; //add at the end point//
    n = (Q * ) malloc(sizeof(Q)); /*allocation of memory for n, malloc size of destructor*/
    n->next = NULL; //then point to null as LL//
    n->N = k;
    if (queue == NULL) {

        queue = n;
    } else {
        for (n1 = queue; n1-> next != NULL; n1 = n1-> next);
        n1-> next = n;
    }
}

void calculate_qt() {
    for (i = 1; i <= tiercount; i++) {
        sumT[i] = 0;
        ncT[i] = 0;
    }

    int temptier = 0;
    for (i = 0; i < n; i++) {
        if (ct >= nodes[i][0]) {
            float uval3 = nodes[i][1];
            float tval3 = tierdiv;
            temptier = ceil(uval3/tval3);
            ncT[temptier] += 1;
            sumT[temptier] = sumT[temptier] + nodes[i][3];
        }
    }

    for (i = 1; i <= tiercount; i++) {
        if (sumT[i] != 0) {
            float st = sumT[i];
            float nt = ncT[i];
            float sval = (sumT[i] / ncT[i]) * (1 / ncT[i]);
            QTT[i] = ceil(sval);
        }
    }

    printf("\nCURRENT time: %d", ct);
    for (i = 1; i <= tiercount; i++) {
        printf("\nQuantum time for tier %d: %0.1f", i, QTT[i]);
        printf("\nNodes in tier %d: %0.1f", i, ncT[i]);
    }
}

void node_arrive() {
    for (int i = 0; i < n; i++) /*when index[i=0] means AT is zero*/ {
        if (nodes[i][0] == taem) /*time=0 already declared*/ {
            calculate_qt();
            updateLL(i); /*updateLL function is called*/
        }
    }
}

unsigned int upcomming_node() {
    Q * n;
    int x;
    if (queue == NULL)
	/*imagine that there is no nodes in the nodes_queue
	  thus Q =NULL*/
    {
        return -1; //index starts from 0, -1 means no process in the nodes_queue //
    } else {
        x = queue-> N;
        n = queue;
        queue = queue-> next;
        free(n);
        return x;
    }
}

void arrange() {
    int n;

    node_arrive();
    while (time_left()) {
        n = upcomming_node(); //Here, n for next node
        if (n == -1) // if nodes_queue is null, no node arrived, increment the time
        {
            taem++;
            ct++;
            node_arrive();
        } else // some nodes in the nodes_queue
        {
            int temptier2 = 0;
            float uval2 = nodes[n][1];
            float tval2 = tierdiv;
            temptier2 = ceil(uval2/tval2);
            q = QTT[temptier2];

            if (nodes[n][3] < q) {
                q = nodes[n][3];
            }

            for (unsigned int i = q; i > 0; i--) {
                nodes_queue[taem] = n;
                taem++;
                nodes[n][3]--; //reducing the remaining time
                ct++;
                node_arrive(); // keeping track if any node join
            }

            if (nodes[n][3] > 0) // if nodes has SGX time left add to the nodes_queue at the end
            {
                updateLL(n);
            }
        }

        simulate_poet();
        int x;
        taem;
    }
}

void show_overall_queue() {
    float sd = 0.0f;
    printf("\n\nOverall Queue:");
    printf("\n-------------\n");
    for (int i = 0; i <= taem; i++) {
        printf("[Node%d]", nodes_queue[i]);
    }
    printf("\n\nWaiting time:");
    printf("\n------------");
    for (unsigned int i = 0; i < n; i++) {
        printf("\nWaiting time for Node%d: %d", i, wait_times[i]);
    }
    //counting avg wait_times
    float average_wait_time = 0.0f;
    for (unsigned int i = 0; i < n; i++) {
        average_wait_time = average_wait_time + wait_times[i];
    }
    average_wait_time = average_wait_time / n;
    printf("\n\nAvg Waiting time: %f", average_wait_time);
    //Standard Deviation for Waiting taem
    for(int i=0;i<n;i++)
    {
        sd+= (wait_times[i] - average_wait_time) * (wait_times[i] - average_wait_time);
    }
    sd=sqrt(sd/(n-1));
    printf("\n\nStandard Deviation for (Waiting): %f",sd);
}

void waiting_time() {
    uint releasetime, t;
    for (uint i = 0; i < n; i++) {

        for (t = taem - 1; nodes_queue[t] != i; t--);
        releasetime = t + 1;
        wait_times[i] = (float) releasetime - nodes[i][0] - nodes[i][1];
    }
}

void average_estimated_time() {
    float sd2 = 0;
    uint release_time, t;
    float total = 0.0, AvgElp = 0.0;
    printf("\n\n\nElapsed time:");
    printf("\n------------");
    for (unsigned int i = 0; i < n; i++) {
        for (t = taem - 1; nodes_queue[t] != i; t--);
        release_time = t + 1;
        elapsed_time[i] = release_time - nodes[i][0];

        printf("\nElapsed time for Node%d:\t%d", i, elapsed_time[i]);
        total += elapsed_time[i];
    }

    AvgElp = total / n;
    printf("\n\nAvg Elapsed time: %f\n\n\n", AvgElp);

    //Standard Deviation for Elapsed time
    for(int i=0;i<n;i++) {
	    sd2 += (elapsed_time[i] - AvgElp) * (elapsed_time[i] - AvgElp);
    }

    sd2 = sqrt(sd2/(n-1));
    printf("\n\nStandard Deviation for Elapsed time: %f\n", sd2);
}

void pause_for_user(int promptUser, int clearStream) {
    if (clearStream) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            /* all work in condition */
        }
        clearerr(stdin);
    }
    if (promptUser) fputs("Press [Enter] to continue", stdout);
    getchar();
}

int main(int argc, char *argv[]) {
    int show_user_prompt = argc <= 1;
    get_input_from_user(show_user_prompt);

    simulate_poet();
    arrange();
    waiting_time();
    show_overall_queue();
    average_estimated_time();
    pause_for_user(0, 0);
}

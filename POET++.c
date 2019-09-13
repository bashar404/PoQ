/*H**********************************************************************
 * FILENAME : SGX1.c
 * ORGANIZATION : ISPM Research Lab
 *Description: Maximum Node 100, Anything with start from time=0,
 *H*/
#include <stdio.h>
//#include <conio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>


int nodes[10000][4], n, i, queue[100000], WT[10000], taem = 0, n, q = 0, x, ET[10000], tym = 0, ct,upsgx,maxat,tiercount,tierdiv;
float add_SGX = 0.0, QT, QT2, QT3, QT4, sum, sum2, sum3, sum4, nc, nc2, nc3, nc4;
float QTT[10000], sumT[10000], ncT[10000];

typedef struct linkedList {
    unsigned int N; /*N --> Index*/
    struct linkedList * next; /*pointer to the next element*/
}

Q; /*destructor*/
Q * qeue = NULL; /*At Initial stage there nothing in the qeue */

void input_from_user() {
    //int x; //Arrival time

    srand(time(NULL));
   // n = 1 + (rand()%5);
    printf("\nTotal Number of nodes in the network:");
    scanf("%d",&n);
    /*printf("\nQuantum time(It will be dynamic)::");
    scanf("%d",&QT);*/
//Upper bound of SGX time

printf("\nEnter the upper bound of SGXtime:");
scanf("%d",&upsgx);

printf("\nEnter the split for tiers:");
scanf("%d",&tierdiv);

printf("\nEnter the maximum time for Arrival Time:");
scanf("%d",&maxat);
    for (i = 0; i < n; i++) {
        //printf("\nArrival time for Node %d:\t", i);
        //scanf("%d", & nodes[i][0]);
        int b = 1 + rand() % upsgx;
        int a = rand() % 10;
      int at = rand() % maxat; // arrival time is randomly generated
        nodes[i][0]=at;
        nodes[i][1] = b;
        nodes[i][2] = 0;
        nodes[i][3] = nodes[i][1];
    }
    /*Calculate Average of SGX time */

    // for(nodes[i][2]=0;nodes[i][2]<n;nodes[i][2]++)
    //    scanf("%d",&nodes[i][2]);
    // for (nodes[i][2]=0;nodes[i][2]<n;nodes[i][2]++)
    //   add_SGX=add_SGX+nodes[i][2];
    //printf("Sum:%d",add_SGX);

    float uval = upsgx;
    float tval = tierdiv;
    tiercount = ceil(uval/tval);
}

void PoET() {
    printf("\n\nPass: Arrivaltime SGXtime #Leader timeLeft");
    for (i = 0; i < n; i++) {
        printf("\nNode%d:    %d         %d        %d       %d\n", i, nodes[i][0], nodes[i][1], nodes[i][2], nodes[i][3]);
    }
    // printf("\nNext Node:");
    Q * qeue = NULL;
    Q * n; /*Each time when PoET() is called than it will show whose next and initially Q is zero*/
    for (n = qeue; n != NULL; n = n->next) {
        printf("P%d", n->N);

    }

}
unsigned int taemLeft() {
    x = 0; // as boolean, 0 means false
    for (int i = 0; i < n; i++) {
        if (nodes[i][3] > 0) // if any node has remaining time it returns true
        {
            x = 1;
        }
       if (nodes[i][3] == 0)
            nodes[i][2]=1;
    }
    return x;
}

void updateLL(int k) {
    Q * n, * n1; //add at the end point//
    n = (Q * ) malloc(sizeof(Q)); /*allocation of memory for n, malloc size of destructor*/
    n->next = NULL; //then point to null as LL//
    n->N = k;
    if (qeue == NULL) {

        qeue = n;
    } else {
        for (n1 = qeue; n1-> next != NULL; n1 = n1-> next);
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

void Node_arrive() {
    for (int i = 0; i < n; i++) /*when index[i=0] means AT is zero*/ {
        if (nodes[i][0] == taem) /*time=0 already declared*/ {
            calculate_qt();
            updateLL(i); /*updateLL function is called*/
        }
    }
}

unsigned int upcommingNode() {
    Q * n;
    int x;
    if (qeue == NULL)
    /*imagine that there is no nodes in the queue
           thus Q =NULL*/
    {
        return -1; //index starts from 0, -1 means no process in the queue //
    } else {
        x = qeue-> N;
        n = qeue;
        qeue = qeue-> next;
        free(n);
        return x;
    }
}
void arrange() {
    int n;

    Node_arrive();
    while (taemLeft()) {
        n = upcommingNode(); //Here, n for next node
        if (n == -1) // if queue is null, no node arrived, increment the time
        {
            taem++;
            ct++;
            Node_arrive();
        } else // some nodes in the queue
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
                queue[taem] = n;
                taem++;
                nodes[n][3]--; //reducing the remaining time
                ct++;
                Node_arrive(); // keeping track if any node join
            }

            if (nodes[n][3] > 0) // if nodes has SGX time left add to the queue at the end
            {
                updateLL(n);
            }
        }

        PoET();
        int x;
        taem;

    }
}
void FullQueue() {
    float sd=0;
    printf("\n\nOverall Queue:");
     printf("\n-------------\n");
    for (int i = 0; i <= taem; i++) {
        printf("[Node%d]", queue[i]);
    }
    printf("\n\nWaiting time:");
    printf("\n------------");
    for (unsigned int i = 0; i < n; i++) {
        printf("\nWaiting time for Node%d: %d", i, WT[i]);
    }
    //counting avg WT
    float AWT = 0.0;
    for (unsigned int i = 0; i < n; i++) {
        AWT = AWT + WT[i];
    }
    AWT = AWT / n;
    printf("\n\nAvg Waiting time: %f", AWT);
    //Standard Deviation for Waiting taem
    for(int i=0;i<n;i++)
    {
        sd+=(WT[i]-AWT)*(WT[i]-AWT);
    }
    sd=sqrt(sd/(n-1));
    printf("\n\nStandard Deviation for (Waiting): %f",sd);
}
void Waitingtaem() {
    unsigned int releasetime, t;
    for (unsigned int i = 0; i < n; i++) {

        for (t = taem - 1; queue[t] != i; t--);
        releasetime = t + 1;
        WT[i] = releasetime - nodes[i][0] - nodes[i][1];
    }
}
void AET() {
    float sd2=0;
    unsigned int releasetime, t;
    float Total = 0.0, AvgElp=0.0;
    printf("\n\n\nElapsed time:");
    printf("\n------------");
    for (unsigned int i = 0; i < n; i++) {
        for (t = taem - 1; queue[t] != i; t--);
        releasetime = t + 1;
        ET[i] = releasetime - nodes[i][0];

        printf("\nElapsed time for Node%d:\t%d", i, ET[i]);
        Total += ET[i];
    }

    AvgElp = Total / n;
    printf("\n\nAvg Elapsed time: %f\n\n\n", AvgElp);


        //Standard Deviation for Elapsed time
    for(int i=0;i<n;i++)
    {
      sd2+=(ET[i]-AvgElp)*(ET[i]-AvgElp);
    }
    sd2=sqrt(sd2/(n-1));
    printf("\n\nStandard Deviation for Elapsed time: %f\n",sd2);
}



void PauseForUser(int promptUser, int clearStream) {
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

int main() {
    input_from_user();
    PoET();
    arrange();
    Waitingtaem();
    FullQueue();
    AET();
    void PauseForUser();

}

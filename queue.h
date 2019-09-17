#ifndef QUEUE_H
#define QUEUE_H
#include <stdlib.h>
#include <string.h>

typedef unsigned int data;

struct node {
    data d;
    struct node *next;
};

struct queue {
    struct node *top;
};

struct queue* queue_constructor() {
    struct queue * Q = (struct queue*) malloc(sizeof(struct queue));
    memset(Q, 0, sizeof (struct queue));

    return Q;
}

int queue_is_empty(struct queue *Q) {
    return Q->top == 0;
}

data queue_dequeue(struct queue *Q){
    if (Q == 0) {
	return 0;
    }

    if (!queue_is_empty(Q)) {
	struct node *t = Q->top;
	Q->top = Q->top->next;
	data d = t->d;
	free(t);
	return d;
    }
}

void queue_enqueue(struct queue *Q, data d) {
    if (Q == 0) {
	return;
    }

    struct node* new_node = (struct node*) malloc(sizeof(struct node));
    new_node->next = Q->top;
    Q->top = new_node;
    new_node->d = d;
}

void queue_destructor(struct queue *Q) {
    if (Q == 0) {
	return;
    }

    while(!queue_is_empty(Q)) {
	queue_dequeue(Q);
    }

    free(Q);
}

#endif

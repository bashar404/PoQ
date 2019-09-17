#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <string.h>

typedef uint data;

struct item {
    data d;
    struct item *next;
};

struct queue {
    struct item *top;
};

typedef struct item item_t;
typedef struct queue queue_t;

queue_t *queue_constructor() {
    queue_t *Q = (queue_t *) malloc(sizeof(queue_t));
    memset(Q, 0, sizeof(queue_t));

    return Q;
}

int queue_is_empty(queue_t *Q) {
    return Q->top == 0;
}

data queue_dequeue(queue_t *Q) {
    if (Q == 0) {
        return 0;
    }

    if (! queue_is_empty(Q)) {
        item_t *t = Q->top;
        Q->top = Q->top->next;
        data d = t->d;
        free(t);
        return d;
    }
}

void queue_enqueue(queue_t *Q, data d) {
    if (Q == 0) {
        return;
    }

    item_t *new_item = (item_t *) malloc(sizeof(item_t));
    if (new_item == NULL) goto error;

    memset(new_item, 0, sizeof(item_t));
    new_item->next = Q->top;
    Q->top = new_item;
    new_item->d = d;

    return;

    error:
    fprintf(stderr, "Could not allocate memory space for item in queue.\n");
}

void queue_destructor(queue_t *Q) {
    if (Q == 0) {
        return;
    }

    while (! queue_is_empty(Q)) {
        queue_dequeue(Q);
    }

    free(Q);
}

#endif

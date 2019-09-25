#include "queue_t.h"

queue_t* queue_constructor() {
    queue_t *q = (queue_t *) malloc(sizeof(queue_t));
    if (q == NULL) goto error;

    memset(q, 0, sizeof(queue_t));

    return q;

    error:
    fprintf(stderr, "Could not allocate memory for queue\n");
    return NULL;
}

int queue_is_empty(queue_t *q) {
    assert(q->head != NULL || q->head == q->tail); // if the head is NULL, then tail should be NULL as well
    ERR("queue is empty: %s (size: %d)\n", (q != NULL && q->head == NULL ? "true" : "false"), (q != NULL ? (int) q->size : 0));
    return q != NULL && q->head == NULL;
}

void* queue_front(queue_t *q) {
    assert(q != NULL);

    if (q->head != NULL) {
        return q->head->d;
    } else {
        goto error1;
    }

    error1:
    fprintf(stderr, "The queue is empty\n");
    return 0;
}

void* queue_back(queue_t *q) {
    assert(q != NULL);

    if (q->tail != NULL) {
        return q->tail->d;
    } else {
        goto error1;
    }

    error1:
    fprintf(stderr, "The queue is empty\n");
    return 0;
}

void queue_pop(queue_t *q) {
    assert(q != NULL);

    if (q->head != NULL) {
        item_t *t = q->head;
        q->head = q->head->next;
        if (t == q->tail) {
            q->tail = NULL;
            assert(q->head == NULL);
        }
        ERR("Pops element (%d) from queue\n", *((unsigned int*)t->d));
        free(t);
        q->size--;
    } else {
        assert(q->tail == NULL);
    }
}

void queue_push(queue_t *q, void *d) {
    assert(q != NULL);

    item_t *new_item = malloc(sizeof(item_t));
    if (new_item == NULL) goto error1;
    memset(new_item, 0, sizeof(item_t));

    new_item->d = d;

    if (q->tail == NULL){
        assert(q->head == NULL);
        q->head = q->tail = new_item;
    } else {
        q->tail->next = new_item;
        q->tail = new_item;
    }

    q->size++;

    ERR("Pushes %d into the queue (size: %lu)\n", *((int *)d), q->size);

    return;

    error1:
    fprintf(stderr, "Could not allocate new item into queue\n");
}

void queue_print(queue_t *q) {
    assert(q != NULL);

    if (queue_is_empty(q)) {
        printf("[NILL]\n");
        return;
    }

    for(item_t *i = q->head; i != NULL; i=i->next) {
        printf("[%u]", *((unsigned int *) i->d));
    }
    printf("\n");
}

size_t queue_size(queue_t *q) {
    assert(q != NULL);
    return q->size;
}

void queue_destructor(queue_t *q, int deallocate) {
    assert(q != NULL);

    while(!queue_is_empty(q)) {
        if (deallocate != 0) {
            free(queue_front(q));
        }
        queue_pop(q);
    }

    free(q);
}
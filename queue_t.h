#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "poet_common_definitions.h"

struct item {
    void *d;
    struct item *next;
};

struct queue {
    size_t size;
    struct item *head;
    struct item *tail;
    pthread_rwlock_t *lock;
    struct {
        pthread_cond_t *cond;
        pthread_mutex_t *cond_mutex;
    } cond;
};

typedef struct item item_t;
typedef struct queue queue_t;

queue_t *queue_constructor();
int queue_is_empty(queue_t *q);
void *queue_front(queue_t *q);
void *queue_back(queue_t *q);
void queue_pop(queue_t *q);
void *queue_front_and_pop(queue_t *q);
void queue_push(queue_t *q, void *d);
int queue_wait_change(queue_t *q);
int queue_wait_change_timed(queue_t *q, struct timespec time);
void queue_print(queue_t *q);
void queue_print_func(queue_t *q, void (*)(void *));
void queue_print_func_dump(queue_t *q, void (*)(void *, void *), void *);
size_t queue_size(queue_t *q);
void queue_destructor(queue_t *q, int deallocate);

#ifdef __cplusplus
}
#endif

#endif
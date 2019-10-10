#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#ifdef DEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#undef ERR
#define ERR(...) /**/
#endif

struct item {
    void *d;
    struct item *next;
};

struct queue {
    size_t size;
    struct item *head;
    struct item *tail;
    pthread_mutex_t *lock;
};

typedef struct item item_t;
typedef struct queue queue_t;

queue_t* queue_constructor();
int queue_is_empty(queue_t *q);
void* queue_front(queue_t *q);
void* queue_back(queue_t *q);
void queue_pop(queue_t *q);
void queue_push(queue_t *q, void* d);
void queue_print(queue_t *q);
size_t queue_size(queue_t *q);
void queue_destructor(queue_t *q, int deallocate);

#endif
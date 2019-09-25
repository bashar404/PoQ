#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef NDEBUG
#define ERR(...) do {fprintf(stderr, __VA_ARGS__);} while(0);
#define ERRR(...) do {fprintf(stderr, "(%d)", __LINE__); fprintf(stderr, __VA_ARGS__);} while(0);
#else
#define ERR(...) /**/
#endif

typedef unsigned int data;

struct item {
    data d;
    struct item *next;
};

struct queue {
    size_t size;
    struct item *head;
    struct item *tail;
};

typedef struct item item_t;
typedef struct queue queue_t;

queue_t* queue_constructor();
int queue_is_empty(queue_t *q);
data queue_front(queue_t *q);
data queue_back(queue_t *q);
void queue_pop(queue_t *q);
void queue_push(queue_t *q, data d);
void queue_print(queue_t *q);
size_t queue_size(queue_t *q);
void queue_destructor(queue_t *q);

#endif
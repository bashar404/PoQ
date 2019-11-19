#ifdef __cplusplus
extern "C" {
#endif

#include "queue_t.h"

queue_t *queue_constructor() {
    queue_t *q = (queue_t *) malloc(sizeof(queue_t));
    if (q == NULL) goto error;

    memset(q, 0, sizeof(queue_t));

    q->lock = malloc(sizeof(pthread_rwlock_t));
    if (q->lock == NULL){
        perror("queue constructor malloc");
        goto error;
    }

    memset(q->lock, 0, sizeof(pthread_rwlock_t));
    if (pthread_rwlock_init(q->lock, NULL) != 0) {
        perror("queue_lock init");
        goto error;
    }

    q->cond.cond = malloc(sizeof(pthread_cond_t));
    q->cond.cond_mutex = malloc(sizeof(pthread_mutex_t));
    if (q->cond.cond == NULL || q->cond.cond_mutex == NULL) {
        perror("queue constructor malloc 2");
        goto error;
    }

    if (pthread_mutex_init(q->cond.cond_mutex, NULL) != 0) {
        perror("queue_cond_mutex init");
        goto error;
    }

    if (pthread_cond_init(q->cond.cond, NULL) != 0) {
        perror("queue_cond_mutex init");
        goto error;
    }

    ERRR("Created queue %p on thread 0x%lx\n", q, pthread_self());

    return q;

    error:
    if (q != NULL) {
        if (q->lock != NULL) pthread_rwlock_destroy(q->lock);
        if (q->cond.cond != NULL) pthread_cond_destroy(q->cond.cond);
        if (q->cond.cond_mutex != NULL) pthread_mutex_destroy(q->cond.cond_mutex);
    }

    if (q != NULL && q->lock != NULL) free(q->lock);
    if (q != NULL && q->cond.cond != NULL) free(q->cond.cond);
    if (q != NULL && q->cond.cond_mutex != NULL) free(q->cond.cond_mutex);
    if (q != NULL) free(q);
    return NULL;
}

int queue_is_empty(queue_t *q) {
    assert(q != NULL && q->lock != NULL);
    pthread_rwlock_rdlock(q->lock);
    assert(q->head != NULL || q->head == q->tail); // if the head is NULL, then tail should be NULL as well
    ERRR("queue %p is empty: %s (size: %d)\n", q, (q != NULL && q->head == NULL ? "true" : "false"),
        (q != NULL ? (int) q->size : 0));
    int r = q != NULL && q->head == NULL;
    pthread_rwlock_unlock(q->lock);
    return r;
}

void *queue_front(queue_t *q) {
    assert(q != NULL && q->lock != NULL);

    pthread_rwlock_rdlock(q->lock);

    if (q->head != NULL) {
        void *d = q->head->d;
        pthread_rwlock_unlock(q->lock);
        return d;
    } else {
        goto error;
    }

    error:
    pthread_rwlock_unlock(q->lock);
    WARN("The queue %p is empty\n", q);
    return NULL;
}

void *queue_back(queue_t *q) {
    assert(q != NULL && q->lock != NULL);

    pthread_rwlock_rdlock(q->lock);

    if (q->tail != NULL) {
        void *d = q->tail->d;
        pthread_rwlock_unlock(q->lock);
        return d;
    } else {
        goto error;
    }

    error:
    pthread_rwlock_unlock(q->lock);
    WARN("The queue %p is empty\n", q);
    return NULL;
}

void queue_pop(queue_t *q) {
    assert(q != NULL && q->lock != NULL);

    pthread_rwlock_wrlock(q->lock);

    if (q->head != NULL) {
        item_t *t = q->head;
        q->head = q->head->next;
        if (t == q->tail) {
            q->tail = NULL;
            assert(q->head == NULL);
        }
        ERRR("Pops element (%p) from queue %p\n", t->d, q);
        free(t);
        q->size--;
    } else {
        assert(q->tail == NULL);
    }

    pthread_rwlock_unlock(q->lock);
    pthread_cond_broadcast(q->cond.cond);
}

void *queue_front_and_pop(queue_t *q) {
    assert(q != NULL && q->lock != NULL);

    pthread_rwlock_wrlock(q->lock);

    item_t *d = NULL;

    if (q->head != NULL) {
        item_t *t = q->head;
        q->head = q->head->next;
        if (t == q->tail) {
            q->tail = NULL;
            assert(q->head == NULL);
        }
        ERRR("Pops element (%p) from queue %p\n", t->d, q);
        d = t->d;
        free(t);
        q->size--;
    } else {
        assert(q->tail == NULL);
    }

    pthread_rwlock_unlock(q->lock);
    pthread_cond_broadcast(q->cond.cond);

    return d;
}

void queue_push(queue_t *q, void *d) {
    assert(q != NULL && q->lock != NULL);

    pthread_rwlock_wrlock(q->lock);

    item_t *new_item = malloc(sizeof(item_t));
    if (new_item == NULL) goto error;
    memset(new_item, 0, sizeof(item_t));

    new_item->d = d;

    if (q->tail == NULL) {
        assert(q->head == NULL);
        q->head = q->tail = new_item;
    } else {
        q->tail->next = new_item;
        q->tail = new_item;
    }

    q->size++;

    pthread_rwlock_unlock(q->lock);
    pthread_cond_broadcast(q->cond.cond);
    ERRR("Pushes %p into the queue %p (size: %lu)\n", d, q, q->size);

    return;

    error:
    pthread_rwlock_unlock(q->lock);
    ERROR("Could not allocate new item into queue\n");
}

int queue_wait_change(queue_t *q) {
    int ret = 0;
    if((ret = pthread_mutex_trylock(q->cond.cond_mutex)) == 0) {
        ret = pthread_cond_wait(q->cond.cond, q->cond.cond_mutex);
        if (ret) {
            perror("queue_wait_change -> cond_wait");
        }
        pthread_mutex_unlock(q->cond.cond_mutex);
    } else {
        perror("queue_wait_change -> trylock");
    }

    return ret;
}

int queue_wait_change_timed(queue_t *q, struct timespec time) {
    int ret = 0;
    struct timespec waittime = time;
    if((ret = pthread_mutex_trylock(q->cond.cond_mutex)) == 0) {
        ret = pthread_cond_timedwait(q->cond.cond, q->cond.cond_mutex, &waittime);
        if (ret) {
            perror("queue_wait_change_timed -> cond_wait");
        }
        pthread_mutex_unlock(q->cond.cond_mutex);
    } else {
        perror("queue_wait_change -> trylock");
    }

    return ret;
}

static void generic_print(void *d) {
    printf("[%u]", (unsigned int) d);
}

void queue_print(queue_t *q) {
    assert(q != NULL);
    queue_print_func(q, generic_print);
}

void queue_print_func(queue_t *q, void (*print_func)(void *)) {
    assert(q != NULL);
    assert(print_func != NULL);

    pthread_rwlock_rdlock(q->lock);
    for (item_t *i = q->head; i != NULL; i = i->next) {
        print_func(i->d);
    }
    pthread_rwlock_unlock(q->lock);
}

void queue_print_func_dump(queue_t *q, void (*print_func)(void *, void *), void *dest) {
    assert(q != NULL);
    assert(print_func != NULL);

    pthread_rwlock_rdlock(q->lock);
    for (item_t *i = q->head; i != NULL; i = i->next) {
        print_func(i->d, dest);
    }
    pthread_rwlock_unlock(q->lock);
}

size_t queue_size(queue_t *q) {
    assert(q != NULL);
    pthread_rwlock_rdlock(q->lock);
    int r = q->size;
    pthread_rwlock_unlock(q->lock);
    return r;
}

void queue_destructor(queue_t *q, int deallocate) {
    assert(q != NULL);

    // XXX: maybe put a destruction state variable to say that is destroying the object?

    while (!queue_is_empty(q)) {
        if (deallocate != 0) {
            free(queue_front(q));
        }
        queue_pop(q);
    }

    pthread_rwlock_destroy(q->lock);
    pthread_cond_destroy(q->cond.cond);
    pthread_mutex_destroy(q->cond.cond_mutex);

    free(q->cond.cond);
    free(q->cond.cond_mutex);
    free(q->lock);

    free(q);
}

#ifdef __cplusplus
}
#endif
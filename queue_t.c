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

    q->cond.cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    q->cond.cond_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
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

int queue_is_empty_custom(queue_t *q, int lock) {
    assert(q != NULL && q->lock != NULL);
    if (lock) pthread_rwlock_rdlock(q->lock);
    assert(q->head != NULL || q->head == q->tail); // if the head is NULL, then tail should be NULL as well
    ERRR("queue %p is empty: %s (size: %d)\n", q, (q != NULL && q->head == NULL ? "true" : "false"),
         (q != NULL ? (int) q->size : 0));
    int r = q != NULL && q->head == NULL;
    if (lock) pthread_rwlock_unlock(q->lock);
    return r;
}

int queue_is_empty(queue_t *q) {
    return queue_is_empty_custom(q, 1);
}

void *queue_front_custom(queue_t *q, int lock) {
    assert(q != NULL && q->lock != NULL);

    if (lock) pthread_rwlock_rdlock(q->lock);

    if (q->head != NULL) {
        void *d = q->head->d;
        pthread_rwlock_unlock(q->lock);
        return d;
    } else {
        goto error;
    }

    error:
    if (lock) pthread_rwlock_unlock(q->lock);
    WARN("The queue %p is empty\n", q);
    return NULL;
}

void *queue_front(queue_t *q) {
    return queue_front_custom(q, 1);
}

void *queue_back_custom(queue_t *q, int lock) {
    assert(q != NULL && q->lock != NULL);

    if (lock) pthread_rwlock_rdlock(q->lock);

    if (q->tail != NULL) {
        void *d = q->tail->d;
        pthread_rwlock_unlock(q->lock);
        return d;
    } else {
        goto error;
    }

    error:
    if (lock) pthread_rwlock_unlock(q->lock);
    WARN("The queue %p is empty\n", q);
    return NULL;
}

void *queue_back(queue_t *q) {
    return queue_back_custom(q, 1);
}

void queue_pop_custom(queue_t *q, int notify, int lock) {
    assert(q != NULL && q->lock != NULL);

    if (lock) pthread_rwlock_wrlock(q->lock);

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

    if (lock) pthread_rwlock_unlock(q->lock);
    if (notify) pthread_cond_broadcast(q->cond.cond);
}

void queue_pop(queue_t *q) {
    queue_pop_custom(q, 1, 1);
}

void *queue_front_and_pop_custom(queue_t *q, int notify, int lock) {
    assert(q != NULL && q->lock != NULL);

    if (lock) pthread_rwlock_wrlock(q->lock);

    void *d = NULL;

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

    if (lock) pthread_rwlock_unlock(q->lock);
    if (notify) pthread_cond_broadcast(q->cond.cond);

    return d;
}

void *queue_front_and_pop(queue_t *q) {
    return queue_front_and_pop_custom(q, 1, 1);
}

void queue_push_custom(queue_t *q, void *d, int notify, int lock) {
    assert(q != NULL && q->lock != NULL);

    if (lock) {
        pthread_rwlock_wrlock(q->lock);
    }

    item_t *new_item = (item_t *) malloc(sizeof(item_t));
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

    if (lock) pthread_rwlock_unlock(q->lock);
    if (notify) pthread_cond_broadcast(q->cond.cond);
    ERRR("Pushes %p into the queue %p (size: %lu)\n", d, q, q->size);

    return;

    error:
    if (lock) pthread_rwlock_unlock(q->lock);
    ERROR("Could not allocate new item into queue\n");
}

void queue_push(queue_t *q, void *d) {
    queue_push_custom(q, d, 1, 1);
}

int queue_wait_change(queue_t *q) {
    assert(q != NULL && q->lock != NULL);
    int ret = 0;
    if((ret = (pthread_mutex_trylock(q->cond.cond_mutex) == 0))) {
        ret = pthread_cond_wait(q->cond.cond, q->cond.cond_mutex) == 0;
        if (!ret) {
            perror("queue_wait_change -> cond_wait");
        }
        pthread_mutex_unlock(q->cond.cond_mutex);
    } else {
        perror("queue_wait_change -> trylock");
    }

    return ret;
}

int queue_wait_change_timed(queue_t *q, struct timespec time) {
    assert(q != NULL && q->lock != NULL);
    int ret = 0;
    struct timespec waittime = time;
    if((ret = (pthread_mutex_trylock(q->cond.cond_mutex) == 0))) {
        ret = pthread_cond_timedwait(q->cond.cond, q->cond.cond_mutex, &waittime) == 0;
        if (!ret) {
            perror("queue_wait_change_timed -> cond_wait");
        }
        pthread_mutex_unlock(q->cond.cond_mutex);
    } else {
        perror("queue_wait_change -> trylock");
    }

    return ret;
}

void queue_broadcast(queue_t *q) {
    assert(q != NULL && q->cond.cond != NULL);
    pthread_cond_broadcast(q->cond.cond);
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

void queue_print_func_dump_custom(queue_t *q, void (*print_func)(void *, void *), void *dest, int lock) {
    assert(q != NULL);
    assert(print_func != NULL);

    if (lock) pthread_rwlock_rdlock(q->lock);
    for (item_t *i = q->head; i != NULL; i = i->next) {
        print_func(i->d, dest);
    }
    if (lock) pthread_rwlock_unlock(q->lock);
}

void queue_print_func_dump(queue_t *q, void (*print_func)(void *, void *), void *dest) {
    queue_print_func_dump_custom(q, print_func, dest, 1);
}

int queue_selective_remove(queue_t *q, int (*should_delete)(int, void *), int deallocate, int lock) {
    assert(q != NULL);
    assert(should_delete != NULL);

    if (lock) pthread_rwlock_wrlock(q->lock);
    int first_execution = 1;
    int deletions = 0;
    void ** to_delete = deallocate ? calloc(q->size, sizeof(void *)) : NULL;
    if(deallocate && to_delete == NULL) {
        ERROR("Could not allocate list to deallocate elements:\n");
        goto error;
    }

    int preserve = 0;
    item_t **to_preserve = calloc(q->size, sizeof(item_t *));
    if (to_preserve == NULL) {
        ERROR("Could not allocate list to preserve elements:\n");
        goto error;
    }

    /* MODIFYING */

    for(item_t * i = q->head; i != NULL; i = i->next) {
        int delete = should_delete(first_execution, i->d);

        if (delete) {
            if (deallocate) {
                to_delete[deletions] = i->d;
            }

            free(i);
            deletions++;
        } else {
            to_preserve[preserve++] = i;
        }

        first_execution = 0;
    }

    item_t *i_ptr = q->head = to_preserve[0];
    for(int i = 1; i < preserve; i++) {
        i_ptr->next = to_preserve[i];
        i_ptr = i_ptr->next;
    }
    q->tail = to_preserve[max(preserve-1, 0)];
    if (q->tail != NULL) {
        q->tail->next = NULL;
    }
    assert(preserve + deletions == q->size);
    q->size = preserve;

    error:
    if (lock) pthread_rwlock_unlock(q->lock);

//    if (deletions > 0) {
//        pthread_cond_broadcast(q->cond.cond);
//    }

    if (deallocate && to_delete != NULL) {
        for(int i = 0; i < deletions; i++) {
            free(to_delete[i]);
        }
    }

    if (to_delete != NULL) {
        free(to_delete);
    }

    if (to_preserve != NULL) {
        free(to_preserve);
    }

    return deletions;
}

size_t queue_size_custom(queue_t *q, int lock) {
    assert(q != NULL);
    if (lock) pthread_rwlock_rdlock(q->lock);
    int r = q->size;
    if (lock) pthread_rwlock_unlock(q->lock);
    return r;
}

size_t queue_size(queue_t *q) {
    return queue_size_custom(q, 1);
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
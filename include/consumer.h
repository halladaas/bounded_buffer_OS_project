#ifndef CONSUMER_H
#define CONSUMER_H

#include "common.h"
#include "buffer.h"

/* Argument struct passed to each consumer thread */
typedef struct {
    int       consumer_id;
    buffer_t *buf;
    int       items_to_consume;   /* how many items this consumer processes */
    metrics_t *metrics;
    pthread_mutex_t *metrics_mtx;
} consumer_args_t;

/* Thread entry point */
void *consumer_thread(void *arg);

#endif /* CONSUMER_H */

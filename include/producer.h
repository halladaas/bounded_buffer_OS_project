#ifndef PRODUCER_H
#define PRODUCER_H

#include "common.h"
#include "buffer.h"

/* Argument struct passed to each producer thread */
typedef struct {
    int       producer_id;
    buffer_t *buf;
    int       items_to_produce;   /* how many items this producer makes */
    metrics_t *metrics;           /* shared metrics (mutex-protected) */
    pthread_mutex_t *metrics_mtx;
} producer_args_t;

/* Thread entry point — cast return to void* */
void *producer_thread(void *arg);

#endif /* PRODUCER_H */

#include "../include/producer.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>

static int item_counter = 0;
static pthread_mutex_t counter_mtx = PTHREAD_MUTEX_INITIALIZER;

void *producer_thread(void *arg) {
    producer_args_t *pargs = (producer_args_t *)arg;

    for (int i = 0; i < pargs->items_to_produce; i++) {
        /* simulate work before producing */
        usleep(rand() % SLEEP_MAX_US);

        /* build item */
        item_t item;
        pthread_mutex_lock(&counter_mtx);
        item.id = ++item_counter;
        pthread_mutex_unlock(&counter_mtx);

        item.producer_id   = pargs->producer_id;
        item.timestamp_ms  = current_time_ms();
        item.value         = rand() % 100;

        buffer_insert(pargs->buf, item);

        printf("[Producer %d] Inserted item #%d (value=%d)\n",
               pargs->producer_id, item.id, item.value);

        /* update metrics */
        pthread_mutex_lock(pargs->metrics_mtx);
        pargs->metrics->total_produced++;
        pthread_mutex_unlock(pargs->metrics_mtx);
    }

    printf("[Producer %d] Done.\n", pargs->producer_id);
    return NULL;
}

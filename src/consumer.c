#include "../include/consumer.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>

void *consumer_thread(void *arg) {
    consumer_args_t *cargs = (consumer_args_t *)arg;

    for (int i = 0; i < cargs->items_to_consume; i++) {
        item_t item = buffer_remove(cargs->buf);

        /* simulate processing time */
        usleep(rand() % SLEEP_MAX_US);

        long now      = current_time_ms();
        long wait_ms  = now - item.timestamp_ms;

        printf("[Consumer %d] Consumed item #%d (value=%d, wait=%ld ms)\n",
               cargs->consumer_id, item.id, item.value, wait_ms);

        /* update metrics */
        pthread_mutex_lock(cargs->metrics_mtx);
        cargs->metrics->total_consumed++;
        /* running average: new_avg = old_avg + (x - old_avg) / n */
        long n = cargs->metrics->total_consumed;
        cargs->metrics->avg_wait_ms +=
            (wait_ms - cargs->metrics->avg_wait_ms) / (double)n;
        pthread_mutex_unlock(cargs->metrics_mtx);
    }

    printf("[Consumer %d] Done.\n", cargs->consumer_id);
    return NULL;
}

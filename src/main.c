#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "../include/common.h"
#include "../include/buffer.h"
#include "../include/producer.h"
#include "../include/consumer.h"
#include "../include/utils.h"

int main(void) {
    srand((unsigned)time(NULL));

    /* ── Shared state ── */
    buffer_t        buf;
    metrics_t       metrics   = {0, 0, 0.0};
    pthread_mutex_t met_mutex = PTHREAD_MUTEX_INITIALIZER;

    buffer_init(&buf);

    /* ── Thread handles ── */
    pthread_t prod_threads[NUM_PRODUCERS];
    pthread_t cons_threads[NUM_CONSUMERS];

    producer_args_t pargs[NUM_PRODUCERS];
    consumer_args_t cargs[NUM_CONSUMERS];

    /*
     * Distribute items evenly.
     * Total items = NUM_PRODUCERS * NUM_ITEMS
     * Each consumer gets (total / NUM_CONSUMERS) items.
     */
    int total_items     = NUM_PRODUCERS * NUM_ITEMS;
    int items_per_cons  = total_items / NUM_CONSUMERS;

    /* ── Spawn producers ── */
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pargs[i].producer_id      = i + 1;
        pargs[i].buf              = &buf;
        pargs[i].items_to_produce = NUM_ITEMS;
        pargs[i].metrics          = &metrics;
        pargs[i].metrics_mtx      = &met_mutex;
        pthread_create(&prod_threads[i], NULL, producer_thread, &pargs[i]);
    }

    /* ── Spawn consumers ── */
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        cargs[i].consumer_id     = i + 1;
        cargs[i].buf             = &buf;
        cargs[i].items_to_consume = (i < NUM_CONSUMERS - 1)
                                        ? items_per_cons
                                        : total_items - items_per_cons * (NUM_CONSUMERS - 1);
        cargs[i].metrics         = &metrics;
        cargs[i].metrics_mtx     = &met_mutex;
        pthread_create(&cons_threads[i], NULL, consumer_thread, &cargs[i]);
    }

    /* ── Join all threads ── */
    for (int i = 0; i < NUM_PRODUCERS; i++)
        pthread_join(prod_threads[i], NULL);
    for (int i = 0; i < NUM_CONSUMERS; i++)
        pthread_join(cons_threads[i], NULL);

    /* ── Final report ── */
    printf("\n=== Run Complete ===\n");
    printf("Produced : %ld\n", metrics.total_produced);
    printf("Consumed : %ld\n", metrics.total_consumed);
    printf("Avg wait : %.2f ms\n", metrics.avg_wait_ms);

    save_metrics("results/metrics.txt",
                 metrics.total_produced,
                 metrics.total_consumed,
                 metrics.avg_wait_ms);

    buffer_destroy(&buf);
    pthread_mutex_destroy(&met_mutex);
    return 0;
}

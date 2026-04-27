#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "../include/common.h"

#define ITEMS_PER_PRODUCER 20

void *producer_thread(void *arg) {
    producer_args_t *args = (producer_args_t *)arg;
    int id = args->id;

    srand(time(NULL) ^ (id * 1000));

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        item_t item;
        item.value = rand() % 100;
        item.priority = (rand() % 4 == 0) ? 1 : 0;
        clock_gettime(CLOCK_MONOTONIC, &item.enqueue_time);

        sem_wait(empty);
        pthread_mutex_lock(mutex);

        enqueue(item);

        pthread_mutex_unlock(mutex);
        sem_post(full);

        printf("[Producer-%d] Produced item: %d%s\n",
               id,
               item.value,
               item.priority ? " [URGENT]" : "");
    }

    printf("[Producer-%d] Finished producing %d items.\n", id, ITEMS_PER_PRODUCER);
    free(args);
    return NULL;
}
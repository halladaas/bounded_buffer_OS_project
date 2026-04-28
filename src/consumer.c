static void *consumer_thread(void *arg) {
    consumer_args_t *cargs = (consumer_args_t *)arg;
    int id = cargs->id;
    buffer_t *buf = cargs->buf;

    while (1) {
        item_t item = buffer_remove(buf);

        if (item.value == POISON_PILL) {
            printf("[Consumer-%d] got poison pill, shutting down\n", id);
            break;
        }

        struct timespec dequeue_ts;
        clock_gettime(CLOCK_MONOTONIC, &dequeue_ts);
        double latency = timespec_diff_ms(item.enqueue_ts, dequeue_ts);

        if (item.priority == URGENT) {
            printf("[Consumer-%d] Consumed item: %d [URGENT] (latency: %.2f ms)\n",
                   id, item.value, latency);
        } else {
            printf("[Consumer-%d] Consumed item: %d (latency: %.2f ms)\n",
                   id, item.value, latency);
        }

        pthread_mutex_lock(&g_metrics.mtx);
        g_metrics.total_consumed++;
        g_metrics.total_latency_ms += latency;
        pthread_mutex_unlock(&g_metrics.mtx);
    }

    printf("[Consumer-%d] Finished consuming.\n", id);
    return NULL;
}



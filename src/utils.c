#include "../include/utils.h"
#include <stdio.h>
#include <time.h>

long current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

void save_metrics(const char *path, long produced, long consumed, double avg_wait_ms) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen metrics"); return; }

    fprintf(f, "=== Producer-Consumer Run Metrics ===\n");
    fprintf(f, "Total Produced : %ld\n", produced);
    fprintf(f, "Total Consumed : %ld\n", consumed);
    fprintf(f, "Avg Wait (ms)  : %.2f\n", avg_wait_ms);
    fclose(f);

    printf("[Metrics] Saved to %s\n", path);
}

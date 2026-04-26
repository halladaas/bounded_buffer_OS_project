#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

/* ── Buffer configuration ── */
#define BUFFER_SIZE     10
#define NUM_PRODUCERS   2
#define NUM_CONSUMERS   3
#define NUM_ITEMS       20      /* total items each producer will produce */
#define SLEEP_MAX_US    500000  /* max random sleep (0.5 s) */

/* ── Item produced / consumed ── */
typedef struct {
    int  id;            /* unique item id */
    int  producer_id;   /* which producer created it */
    long timestamp_ms;  /* wall-clock ms when produced */
    int  value;         /* payload */
} item_t;

/* ── Shared metrics (for bonus) ── */
typedef struct {
    long total_produced;
    long total_consumed;
    double avg_wait_ms;     /* average time item sat in buffer */
} metrics_t;

#endif /* COMMON_H */

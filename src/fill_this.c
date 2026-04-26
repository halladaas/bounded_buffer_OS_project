/*
 * producer_consumer.c
 * CMP-310 Operating Systems — Mini Project, Spring 2026
 *
 * Multithreaded Producer-Consumer with:
 *   - Circular bounded buffer
 *   - POSIX semaphores + mutex synchronisation
 *   - Poison Pill graceful termination
 *   - BONUS: Priority queue (urgent items always dequeued first)
 *   - BONUS: Throughput & latency metrics
 *
 * Compile:
 *   gcc -o producer_consumer producer_consumer.c -pthread
 *
 * Usage:
 *   ./producer_consumer <num_producers> <num_consumers> <buffer_size>
 *
 * Example:
 *   ./producer_consumer 3 2 10
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <errno.h>

/* ═══════════════════════════════════════════════════════════
 *  CONSTANTS
 * ═══════════════════════════════════════════════════════════ */
#define ITEMS_PER_PRODUCER  20      /* each producer generates this many real items */
#define POISON_PILL         -1      /* sentinel value — signals consumer to exit    */
#define URGENT_RATIO        25      /* % of items that are urgent (bonus)           */

/* ═══════════════════════════════════════════════════════════
 *  DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════ */

/* Priority levels */
typedef enum {
    NORMAL = 0,
    URGENT = 1
} priority_t;

/* A single buffer slot */
typedef struct {
    int        value;           /* payload; POISON_PILL = -1            */
    priority_t priority;        /* NORMAL or URGENT                     */
    struct timespec enqueue_ts; /* wall-clock time when item was put in */
} item_t;

/* Circular bounded buffer — two logical queues share one array:
 * urgent items sit at the front (head side), normal at the back.
 * We implement this with two separate circular queues inside one
 * flat array: urgent[] and normal[], each of capacity buf_size.    */
typedef struct {
    item_t  *urgent_slots;
    item_t  *normal_slots;
    int      u_head, u_tail, u_count;   /* urgent queue pointers  */
    int      n_head, n_tail, n_count;   /* normal queue pointers  */
    int      capacity;                  /* max per-priority queue */

    pthread_mutex_t mutex;
    sem_t           empty;   /* total free slots across both queues */
    sem_t           full;    /* total filled slots                  */
} buffer_t;

/* Thread argument structs */
typedef struct {
    int       id;
    buffer_t *buf;
    int       num_consumers;   /* needed so main can send poison pills */
} producer_args_t;

typedef struct {
    int       id;
    buffer_t *buf;
} consumer_args_t;

/* Metrics (bonus) */
typedef struct {
    long   total_produced;
    long   total_consumed;
    double total_latency_ms;   /* sum of (dequeue - enqueue) for all real items */
    struct timespec run_start;
    struct timespec run_end;
    pthread_mutex_t mtx;
} metrics_t;

/* ═══════════════════════════════════════════════════════════
 *  GLOBALS
 * ═══════════════════════════════════════════════════════════ */
static metrics_t g_metrics;

/* ═══════════════════════════════════════════════════════════
 *  UTILITY
 * ═══════════════════════════════════════════════════════════ */
static double timespec_diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec  - start.tv_sec)  * 1000.0
         + (end.tv_nsec - start.tv_nsec) / 1e6;
}

/* ═══════════════════════════════════════════════════════════
 *  BUFFER LIFECYCLE
 * ═══════════════════════════════════════════════════════════ */
static int buffer_init(buffer_t *buf, int capacity) {
    
}

static void buffer_destroy(buffer_t *buf) {
    
}

/* ═══════════════════════════════════════════════════════════
 *  BUFFER OPERATIONS
 * ═══════════════════════════════════════════════════════════ */

/*
 * buffer_insert — blocks when buffer is full.
 * Urgent items go into the urgent circular queue;
 * normal items go into the normal circular queue.
 */
static void buffer_insert(buffer_t *buf, item_t item) {

}

/*
 * buffer_remove — blocks when buffer is empty.
 * Always dequeues urgent items before normal ones (priority).
 */
static item_t buffer_remove(buffer_t *buf) {
    
}

/* ═══════════════════════════════════════════════════════════
 *  PRODUCER THREAD
 * ═══════════════════════════════════════════════════════════ */
static void *producer_thread(void *arg) {
 
}

/* ═══════════════════════════════════════════════════════════
 *  CONSUMER THREAD
 * ═══════════════════════════════════════════════════════════ */
static void *consumer_thread(void *arg) {

}

/* ═══════════════════════════════════════════════════════════
 *  INPUT VALIDATION
 * ═══════════════════════════════════════════════════════════ */
static int parse_args(int argc, char *argv[],
                      int *num_prod, int *num_cons, int *buf_size) {

}

/* ═══════════════════════════════════════════════════════════
 *  METRICS REPORT
 * ═══════════════════════════════════════════════════════════ */
static void print_metrics(int num_prod, int num_cons, int buf_size) {
    double elapsed_ms = timespec_diff_ms(g_metrics.run_start, g_metrics.run_end);
    double avg_latency = (g_metrics.total_consumed > 0)
                         ? g_metrics.total_latency_ms / g_metrics.total_consumed
                         : 0.0;
    double throughput  = (elapsed_ms > 0)
                         ? (g_metrics.total_consumed / (elapsed_ms / 1000.0))
                         : 0.0;

    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║           RUN METRICS (BONUS)            ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Producers        : %-4d                 ║\n", num_prod);
    printf("║  Consumers        : %-4d                 ║\n", num_cons);
    printf("║  Buffer Size      : %-4d                 ║\n", buf_size);
    printf("║  Total Produced   : %-6ld               ║\n", g_metrics.total_produced);
    printf("║  Total Consumed   : %-6ld               ║\n", g_metrics.total_consumed);
    printf("║  Avg Latency (ms) : %-8.2f             ║\n", avg_latency);
    printf("║  Throughput (i/s) : %-8.2f             ║\n", throughput);
    printf("║  Total Runtime    : %-8.2f ms           ║\n", elapsed_ms);
    printf("╚══════════════════════════════════════════╝\n");

    /* Save to results/metrics.txt */
    FILE *f = fopen("results/metrics.txt", "a");
    if (f) {
        fprintf(f, "--- Run: %d producers, %d consumers, buf_size=%d ---\n",
                num_prod, num_cons, buf_size);
        fprintf(f, "Total Produced   : %ld\n", g_metrics.total_produced);
        fprintf(f, "Total Consumed   : %ld\n", g_metrics.total_consumed);
        fprintf(f, "Avg Latency (ms) : %.2f\n", avg_latency);
        fprintf(f, "Throughput (i/s) : %.2f\n", throughput);
        fprintf(f, "Total Runtime    : %.2f ms\n\n", elapsed_ms);
        fclose(f);
        printf("[Metrics] Saved to results/metrics.txt\n");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
   
}

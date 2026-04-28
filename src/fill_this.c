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
/* Initializes the buffer with given capacity per priority queue */
static int buffer_init(buffer_t *buf, int capacity) {

    /* Allocate queues */
    buf->capacity = capacity;
    buf->u_head = buf->u_tail = buf->u_count = 0;
    buf->n_head = buf->n_tail = buf->n_count = 0;
    buf->urgent_slots = malloc(sizeof(item_t) * capacity);
    buf->normal_slots = malloc(sizeof(item_t) * capacity);
    if (!buf->urgent_slots || !buf->normal_slots) {
        fprintf(stderr, "Error allocating buffer queues: %s\n", strerror(errno));
        return -1;
    }

    /*init semaphores + mutex*/
    pthread_mutex_init(&buf->mutex, NULL);
    sem_init(&buf->empty, 0, capacity * 2); /* total free slots across both queues */
    sem_init(&buf->full, 0, 0);

    return 0;
}

/* Destroys the buffer, freeing resources */
static void buffer_destroy(buffer_t *buf) {
    buf->capacity = 0;
    buf->u_head = buf->u_tail = buf->u_count = 0;
    buf->n_head = buf->n_tail = buf->n_count = 0;
    free(buf->urgent_slots);
    free(buf->normal_slots);

    /* Destroy semaphores + mutex */
    pthread_mutex_destroy(&buf->mutex);
    sem_destroy(&buf->empty);
    sem_destroy(&buf->full);
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

    /*blocks if full*/
    sem_wait(&buf->empty);
    pthread_mutex_lock(&buf->mutex);

    if (item.priority == URGENT) {
        /* Insert into urgent queue */
        buf->urgent_slots[buf->u_tail] = item;
        buf->u_tail = (buf->u_tail + 1) % buf->capacity;
        buf->u_count++;
    } else {
        /* Insert into normal queue */
        buf->normal_slots[buf->n_tail] = item;
        buf->n_tail = (buf->n_tail + 1) % buf->capacity;
        buf->n_count++;
    }

    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->full);

}

/*
 * buffer_remove — blocks when buffer is empty.
 * Always dequeues urgent items before normal ones (priority).
 */
static item_t buffer_remove(buffer_t *buf) {
    sem_wait(&buf->full);
    pthread_mutex_lock(&buf->mutex);
    item_t item;
    if (buf->u_count > 0) {
        /* Dequeue from urgent queue */
        item = buf->urgent_slots[buf->u_head];
        buf->u_head = (buf->u_head + 1) % buf->capacity;
        buf->u_count--;
    } else {
        /* Dequeue from normal queue */
        item = buf->normal_slots[buf->n_head];
        buf->n_head = (buf->n_head + 1) % buf->capacity;
        buf->n_count--;
    }
    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->empty);
    return item;
}

/* ═══════════════════════════════════════════════════════════
 *  PRODUCER THREAD
 * ═══════════════════════════════════════════════════════════ */
void *producer_thread(void *arg) {
    producer_args_t *args = (producer_args_t *)arg;
    int id = args->id;
    buffer_t *buf = args->buf;

    srand(time(NULL) ^ (id * 1000));

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        item_t item;
        item.value = rand() % 100;
        item.priority = (rand() % 4 == 0) ? 1 : 0;
        clock_gettime(CLOCK_MONOTONIC, &item.enqueue_ts);

        buffer_insert(args->buf, item);

        printf("[Producer-%d] Produced item: %d%s\n",
               id,
               item.value,
               item.priority ? " [URGENT]" : "");
    }

    printf("[Producer-%d] Finished producing %d items.\n", id, ITEMS_PER_PRODUCER);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  CONSUMER THREAD
 * ═══════════════════════════════════════════════════════════ */
static void *consumer(void *arg) {
    int id = *(int *)arg;
    while (1) {
        while (sem_wait(filled_slots) == -1 && errno == EINTR);  // CHANGED
        pthread_mutex_lock(&buf_mutex);
        Item item = remove_item();
        pthread_mutex_unlock(&buf_mutex);
        // REMOVED: sem_post(empty_slots) was here
        if (item.value == POISON_PILL) {
            printf("[Consumer-%d] Received poison pill. Exiting.\n", id);
            // ADDED: re-queue the pill and wake next consumer
            pthread_mutex_lock(&buf_mutex);
            insert_item(item);
            pthread_mutex_unlock(&buf_mutex);
            sem_post(filled_slots);
            break;
        }
        sem_post(empty_slots);  // MOVED: now after the poison pill check
        struct timespec deq_ts;
        clock_gettime(CLOCK_MONOTONIC, &deq_ts);
        double latency = ts_to_sec(deq_ts) - ts_to_sec(item.enqueue_ts);
        pthread_mutex_lock(&metrics_mutex);
        total_latency += latency;
        total_consumed++;
        pthread_mutex_unlock(&metrics_mutex);
        printf("[Consumer-%d] Consumed item: %d (priority: %s)\n",
               id, item.value, item.priority ? "URGENT" : "normal");
    }
    printf("[Consumer-%d] Finished consuming.\n", id);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  INPUT VALIDATION
 * ═══════════════════════════════════════════════════════════ */
static int parse_args(int argc, char *argv[],
                      int *num_of_prod, int *num_of_cons, int *size_of_buffer) {

    if (argc != 4) {
            fprintf(stderr,
                "Usage: %s <num_producers> <num_consumers> <buffer_size>\n"
                "Example: %s 3 2 10\n", argv[0], argv[0]);
            return -1;
        }

    *num_of_prod = atoi(argv[1]);
    *num_of_cons = atoi(argv[2]);
    *size_of_buffer = atoi(argv[3]);

    if (*num_of_prod <= 0 || *num_of_cons <= 0 || *size_of_buffer <= 0) {
        fprintf(stderr, "Error: all arguments must be positive integers\n");
        return -1;
    }
    return 0;
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
   
    int num_prod, num_cons, buff_size;
    if (parse_args(argc, argv, &num_prod, &num_cons, &buff_size) != 0){ 
        return EXIT_FAILURE;
    }

    printf("=== Producer-Consumer ===\n");
    printf("Producers: %d | Consumers: %d | Buffer: %d slots\n\n",
           num_prod, num_cons, buff_size);

    /* ── metric initialization ── */
    memset(&g_metrics, 0, sizeof(g_metrics));
    pthread_mutex_init(&g_metrics.mtx, NULL);
    clock_gettime(CLOCK_MONOTONIC, &g_metrics.run_start);

    /* ── shared buffer ── */
    buffer_t buf;
    if (buffer_init(&buf, buff_size) != 0){ 
        fprintf(stderr, "Error: Buffer initialization failed.\n");
        return EXIT_FAILURE;
    }

    /* ── thread handles & args allocation── */
    pthread_t       *prod_threads = malloc(num_prod * sizeof(pthread_t));
    pthread_t       *cons_threads = malloc(num_cons * sizeof(pthread_t));
    producer_args_t *pargs        = malloc(num_prod * sizeof(producer_args_t));
    consumer_args_t *cargs        = malloc(num_cons * sizeof(consumer_args_t));

    if (!prod_threads || !cons_threads || !pargs || !cargs) {
        fprintf(stderr, "Error: memory allocation failed\n");
        return EXIT_FAILURE;
    }

    /* ── creating producer threads first ── */
    for (int i = 0; i < num_prod; i++) {
        pargs[i].id           = i;
        pargs[i].buf          = &buf;
        pargs[i].num_consumers = num_cons;
        if (pthread_create(&prod_threads[i], NULL, producer_thread, &pargs[i]) != 0) {
            fprintf(stderr, "Error: failed to create producer thread %d: %s\n",
                    i, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* ── creating consumer threads ── */
    for (int i = 0; i < num_cons; i++) {
        cargs[i].id  = i;
        cargs[i].buf = &buf;
        if (pthread_create(&cons_threads[i], NULL, consumer_thread, &cargs[i]) != 0) {
            fprintf(stderr, "Error: failed to create consumer thread %d: %s\n",
                    i , strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* ── waiting for all producers to finish ── */
    for (int i = 0; i < num_prod; i++){
        pthread_join(prod_threads[i], NULL);
    }

    /* Update metrics for produced items */
    pthread_mutex_lock(&g_metrics.mtx);
    g_metrics.total_produced = num_prod * ITEMS_PER_PRODUCER;
    pthread_mutex_unlock(&g_metrics.mtx);

    printf("\n[Main] All producers done. Sending poison pills...\n");

    /* ── sending one poison pill per consumer ── */
    for (int i = 0; i < num_cons; i++) {
        item_t pill;
        pill.value    = POISON_PILL;
        pill.priority = NORMAL;        /* pills go through normal queue */
        clock_gettime(CLOCK_MONOTONIC, &pill.enqueue_ts);
        buffer_insert(&buf, pill);
    }

    /* ── waiting for all consumers to finish ── */
    for (int i = 0; i < num_cons; i++)
        pthread_join(cons_threads[i], NULL);

    /* ── final metrics ── */
    clock_gettime(CLOCK_MONOTONIC, &g_metrics.run_end);
    print_metrics(num_prod, num_cons, buff_size);

    /* ── cleanup ── */
    buffer_destroy(&buf);
    pthread_mutex_destroy(&g_metrics.mtx);
    free(prod_threads);
    free(cons_threads);
    free(pargs);
    free(cargs);

    return EXIT_SUCCESS;
}

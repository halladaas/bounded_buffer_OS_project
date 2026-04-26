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
    buf->capacity    = capacity;
    buf->u_head = buf->u_tail = buf->u_count = 0;
    buf->n_head = buf->n_tail = buf->n_count = 0;

    buf->urgent_slots = calloc(capacity, sizeof(item_t));
    buf->normal_slots = calloc(capacity, sizeof(item_t));
    if (!buf->urgent_slots || !buf->normal_slots) {
        fprintf(stderr, "Error: buffer allocation failed\n");
        return -1;
    }

    pthread_mutex_init(&buf->mutex, NULL);
    /* total capacity = capacity urgent + capacity normal slots */
    sem_init(&buf->empty, 0, (unsigned)(capacity * 2));
    sem_init(&buf->full,  0, 0);
    return 0;
}

static void buffer_destroy(buffer_t *buf) {
    free(buf->urgent_slots);
    free(buf->normal_slots);
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
    sem_wait(&buf->empty);              /* wait for a free slot          */
    pthread_mutex_lock(&buf->mutex);

    if (item.priority == URGENT) {
        buf->urgent_slots[buf->u_head] = item;
        buf->u_head = (buf->u_head + 1) % buf->capacity;
        buf->u_count++;
    } else {
        buf->normal_slots[buf->n_head] = item;
        buf->n_head = (buf->n_head + 1) % buf->capacity;
        buf->n_count++;
    }

    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->full);               /* signal one more item ready    */
}

/*
 * buffer_remove — blocks when buffer is empty.
 * Always dequeues urgent items before normal ones (priority).
 */
static item_t buffer_remove(buffer_t *buf) {
    sem_wait(&buf->full);               /* wait for an item              */
    pthread_mutex_lock(&buf->mutex);

    item_t item;
    if (buf->u_count > 0) {
        /* urgent queue has items — dequeue from there first */
        item = buf->urgent_slots[buf->u_tail];
        buf->u_tail = (buf->u_tail + 1) % buf->capacity;
        buf->u_count--;
    } else {
        item = buf->normal_slots[buf->n_tail];
        buf->n_tail = (buf->n_tail + 1) % buf->capacity;
        buf->n_count--;
    }

    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->empty);              /* free up one slot              */
    return item;
}

/* ═══════════════════════════════════════════════════════════
 *  PRODUCER THREAD
 * ═══════════════════════════════════════════════════════════ */
static void *producer_thread(void *arg) {
    producer_args_t *pargs = (producer_args_t *)arg;
    int id  = pargs->id;

    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        item_t item;
        item.value    = rand() % 100;

        /* BONUS: ~25% of items are urgent */
        item.priority = ((rand() % 100) < URGENT_RATIO) ? URGENT : NORMAL;

        clock_gettime(CLOCK_MONOTONIC, &item.enqueue_ts);

        buffer_insert(pargs->buf, item);

        printf("[Producer-%d] Produced item: %d  [%s]\n",
               id, item.value,
               item.priority == URGENT ? "URGENT" : "normal");

        /* update produced count */
        pthread_mutex_lock(&g_metrics.mtx);
        g_metrics.total_produced++;
        pthread_mutex_unlock(&g_metrics.mtx);
    }

    printf("[Producer-%d] Finished producing %d items.\n", id, ITEMS_PER_PRODUCER);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  CONSUMER THREAD
 * ═══════════════════════════════════════════════════════════ */
static void *consumer_thread(void *arg) {
    consumer_args_t *cargs = (consumer_args_t *)arg;
    int id = cargs->id;

    while (1) {
        item_t item = buffer_remove(cargs->buf);

        /* Poison pill — time to exit */
        if (item.value == POISON_PILL) {
            printf("[Consumer-%d] Received poison pill. Exiting.\n", id);
            break;
        }

        struct timespec dequeue_ts;
        clock_gettime(CLOCK_MONOTONIC, &dequeue_ts);
        double latency = timespec_diff_ms(item.enqueue_ts, dequeue_ts);

        printf("[Consumer-%d] Consumed item: %d  [%s]  (latency: %.2f ms)\n",
               id, item.value,
               item.priority == URGENT ? "URGENT" : "normal",
               latency);

        /* update metrics */
        pthread_mutex_lock(&g_metrics.mtx);
        g_metrics.total_consumed++;
        g_metrics.total_latency_ms += latency;
        pthread_mutex_unlock(&g_metrics.mtx);
    }

    printf("[Consumer-%d] Finished consuming.\n", id);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  INPUT VALIDATION
 * ═══════════════════════════════════════════════════════════ */
static int parse_args(int argc, char *argv[],
                      int *num_prod, int *num_cons, int *buf_size) {
    if (argc != 4) {
        fprintf(stderr,
            "Usage: %s <num_producers> <num_consumers> <buffer_size>\n"
            "Example: %s 3 2 10\n", argv[0], argv[0]);
        return -1;
    }

    *num_prod = atoi(argv[1]);
    *num_cons = atoi(argv[2]);
    *buf_size = atoi(argv[3]);

    if (*num_prod <= 0) {
        fprintf(stderr, "Error: num_producers must be > 0\n"); return -1;
    }
    if (*num_cons <= 0) {
        fprintf(stderr, "Error: num_consumers must be > 0\n"); return -1;
    }
    if (*buf_size <= 0) {
        fprintf(stderr, "Error: buffer_size must be > 0\n");   return -1;
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
    srand((unsigned)time(NULL));

    int num_producers, num_consumers, buf_size;
    if (parse_args(argc, argv, &num_producers, &num_consumers, &buf_size) != 0)
        return EXIT_FAILURE;

    printf("=== Producer-Consumer ===\n");
    printf("Producers: %d | Consumers: %d | Buffer: %d slots\n\n",
           num_producers, num_consumers, buf_size);

    /* ── Init shared buffer ── */
    buffer_t buf;
    if (buffer_init(&buf, buf_size) != 0)
        return EXIT_FAILURE;

    /* ── Init metrics ── */
    memset(&g_metrics, 0, sizeof(g_metrics));
    pthread_mutex_init(&g_metrics.mtx, NULL);
    clock_gettime(CLOCK_MONOTONIC, &g_metrics.run_start);

    /* ── Allocate thread handles & args ── */
    pthread_t       *prod_threads = malloc(num_producers * sizeof(pthread_t));
    pthread_t       *cons_threads = malloc(num_consumers * sizeof(pthread_t));
    producer_args_t *pargs        = malloc(num_producers * sizeof(producer_args_t));
    consumer_args_t *cargs        = malloc(num_consumers * sizeof(consumer_args_t));

    if (!prod_threads || !cons_threads || !pargs || !cargs) {
        fprintf(stderr, "Error: memory allocation failed\n");
        return EXIT_FAILURE;
    }

    /* ── Spawn consumer threads first (they block on semaphore) ── */
    for (int i = 0; i < num_consumers; i++) {
        cargs[i].id  = i + 1;
        cargs[i].buf = &buf;
        if (pthread_create(&cons_threads[i], NULL, consumer_thread, &cargs[i]) != 0) {
            fprintf(stderr, "Error: failed to create consumer thread %d: %s\n",
                    i + 1, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* ── Spawn producer threads ── */
    for (int i = 0; i < num_producers; i++) {
        pargs[i].id           = i + 1;
        pargs[i].buf          = &buf;
        pargs[i].num_consumers = num_consumers;
        if (pthread_create(&prod_threads[i], NULL, producer_thread, &pargs[i]) != 0) {
            fprintf(stderr, "Error: failed to create producer thread %d: %s\n",
                    i + 1, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* ── Wait for all producers to finish ── */
    for (int i = 0; i < num_producers; i++)
        pthread_join(prod_threads[i], NULL);

    printf("\n[Main] All producers done. Sending poison pills...\n");

    /* ── Send one POISON_PILL per consumer ── */
    for (int i = 0; i < num_consumers; i++) {
        item_t pill;
        pill.value    = POISON_PILL;
        pill.priority = NORMAL;        /* pills go through normal queue */
        clock_gettime(CLOCK_MONOTONIC, &pill.enqueue_ts);
        buffer_insert(&buf, pill);
    }

    /* ── Wait for all consumers to finish ── */
    for (int i = 0; i < num_consumers; i++)
        pthread_join(cons_threads[i], NULL);

    /* ── Final metrics ── */
    clock_gettime(CLOCK_MONOTONIC, &g_metrics.run_end);
    print_metrics(num_producers, num_consumers, buf_size);

    /* ── Cleanup ── */
    buffer_destroy(&buf);
    pthread_mutex_destroy(&g_metrics.mtx);
    free(prod_threads);
    free(cons_threads);
    free(pargs);
    free(cargs);

    return EXIT_SUCCESS;
}

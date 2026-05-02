# Design Notes
**CMP-310 Operating Systems | Mini Project | Spring 2026**

---

## Team Contribution

| Member | Owns |
|--------|------|
| Sakina | `main()`, input validation, poison pill, thread spawning/joining |
| Halla  | `buffer_init`, `buffer_destroy`, `buffer_insert`, `buffer_remove` |
| Maaziya| `producer_thread`
| Rana   | `consumer_thread`, `print_metrics`

---

## Buffer Design

- **Type:** Circular queue (two internal queues: `urgent[]` and `normal[]`)
- **Size:** Configurable via command-line → `./producer_consumer <producers> <consumers> <buffer_size>`
- **Priority:** Urgent items are always dequeued before normal items. FIFO is preserved within each priority level.

```
Buffer (capacity = N per queue):

  urgent_slots[ 0 | 1 | 2 | ... | N-1 ]   ← always consumed first
  normal_slots[ 0 | 1 | 2 | ... | N-1 ]   ← consumed when urgent queue empty

  head → next write index
  tail → next read  index
  count → current occupancy
```

---

## Synchronisation Plan

| Primitive | Variable | Purpose |
|-----------|----------|---------|
| Semaphore | `empty` | Counts free slots — producers block here when buffer is full |
| Semaphore | `full` | Counts filled slots — consumers block here when buffer is empty |
| Mutex     | `mutex` | Protects `head`, `tail`, `count` during insert/remove |
| Mutex     | `metrics.mtx` | Protects shared metrics counters across consumer threads |

**Rule:** No busy-waiting. No `sleep()` for synchronisation. Only semaphores and mutexes block threads.

---

## Termination (Poison Pill)

```
1. Each producer generates exactly 20 items, then exits.
2. main() joins all producer threads (waits for all to finish).
3. main() inserts ONE poison pill (value = -1) per consumer into the buffer.
4. Each consumer exits its loop when it dequeues a poison pill.
5. main() joins all consumer threads.
6. All semaphores, mutexes, and heap memory are freed.
```

Poison pills go through the **normal** queue and are never counted in metrics.

---

## Shared Structs (DO NOT CHANGE WITHOUT TEAM AGREEMENT)

```c
typedef enum { NORMAL = 0, URGENT = 1 } priority_t;

typedef struct {
    int        value;           // payload; -1 = poison pill
    priority_t priority;
    struct timespec enqueue_ts; // timestamp when inserted
} item_t;

typedef struct {
    item_t  *urgent_slots;
    item_t  *normal_slots;
    int      u_head, u_tail, u_count;
    int      n_head, n_tail, n_count;
    int      capacity;
    pthread_mutex_t mutex;
    sem_t           empty;
    sem_t           full;
} buffer_t;

typedef struct {
    int       id;
    buffer_t *buf;
    int       num_consumers;
} producer_args_t;

typedef struct {
    int       id;
    buffer_t *buf;
} consumer_args_t;
```

---

## Function List

| Function | Owner | Description |
|----------|-------|-------------|
| `buffer_init(buf, capacity)` | Halla | Allocate queues, init semaphores + mutex |
| `buffer_destroy(buf)` | Halla | Free memory, destroy semaphores + mutex |
| `buffer_insert(buf, item)` | Halla | Block if full, insert by priority |
| `buffer_remove(buf)` | Halla | Block if empty, dequeue urgent-first |
| `producer_thread(arg)` | Maaziya | Generate 20 items, insert into buffer |
| `consumer_thread(arg)` | Rana | Consume until poison pill, track metrics |
| `print_metrics(...)` | Rana | Print + save latency/throughput to file |
| `parse_args(...)` | Sakina | Validate argc/argv, return parsed values |
| `main()` | Sakina | Init, spawn threads, poison pills, cleanup |

---

## Compile & Run

```bash
gcc -o producer_consumer src/producer_consumer.c -pthread
./producer_consumer 3 2 10
```

Or with Make:
```bash
make run          # 3 producers, 2 consumers, buffer 10
make test-small   # 4 producers, 4 consumers, buffer 2  (bonus metrics)
make test-large   # 4 producers, 4 consumers, buffer 32 (bonus metrics)
```

---

## Git Workflow

```
main          ← stable, final only
 └── dev      ← integration (merge here first, test, then → main)
      ├── buffer-feature    (Halla)
      ├── producer-feature  (Maaziya)
      ├── consumer-feature  (Rana)
      └── main-feature      (Sakina)
```

**Before pushing always pull from dev first:**
```bash
git pull origin dev
```


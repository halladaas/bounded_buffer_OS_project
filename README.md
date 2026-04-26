# Producer-Consumer Problem — POSIX Threads & Semaphores

A multi-threaded C implementation of the classic **Producer-Consumer** synchronisation problem using:

- **POSIX threads** (`pthreads`) for concurrency  
- **Semaphores** (`sem_t`) for buffer-slot signalling  
- **Mutex locks** (`pthread_mutex_t`) for mutual exclusion on shared data  
- A **circular bounded buffer** as the shared resource  

---

## Project Structure

```
producer-consumer-project/
├── src/
│   ├── main.c        ← spawns threads, collects metrics
│   ├── buffer.c      ← circular buffer (insert / remove)
│   ├── producer.c    ← producer thread logic
│   ├── consumer.c    ← consumer thread logic
│   └── utils.c       ← timestamps, metrics file writer
├── include/
│   ├── common.h      ← shared structs (item_t, metrics_t) & constants
│   ├── buffer.h
│   ├── producer.h
│   ├── consumer.h
│   └── utils.h
├── docs/
│   ├── report.pdf    ← project report
│   └── design_notes.md
├── results/
│   └── metrics.txt   ← generated after each run
├── Makefile
├── .gitignore
└── README.md
```

---

## Configuration

All tunable constants live in `include/common.h`:

| Constant        | Default | Meaning                          |
|-----------------|---------|----------------------------------|
| `BUFFER_SIZE`   | 10      | Max items in the shared buffer   |
| `NUM_PRODUCERS` | 2       | Number of producer threads       |
| `NUM_CONSUMERS` | 3       | Number of consumer threads       |
| `NUM_ITEMS`     | 20      | Items each producer generates    |
| `SLEEP_MAX_US`  | 500000  | Max random sleep per operation   |

---

## Build & Run

```bash
# Build
make

# Run
make run
# or directly:
./producer_consumer

# Clean build artifacts
make clean
```

> Requires GCC and POSIX-compliant OS (Linux / macOS).

---

## How It Works

1. **Main** initialises the buffer and spawns `NUM_PRODUCERS` + `NUM_CONSUMERS` threads.  
2. **Producers** generate `item_t` structs with a unique ID, timestamp, and random value, then call `buffer_insert()`.  
3. `buffer_insert()` calls `sem_wait(&empty)` — blocking when the buffer is full — then locks the mutex, writes the item, and posts `&full`.  
4. **Consumers** call `buffer_remove()`, which mirrors the above using `sem_wait(&full)`.  
5. After all threads finish, metrics (total produced/consumed, average wait time) are printed and saved to `results/metrics.txt`.

---

## Synchronisation Design

```
Producer                   Buffer                  Consumer
   │                          │                        │
   │──sem_wait(empty)─────►  │                        │
   │──pthread_mutex_lock──►  │                        │
   │   write item            │                        │
   │──pthread_mutex_unlock►  │                        │
   │──sem_post(full)──────►  │──sem_wait(full)──────► │
   │                          │──pthread_mutex_lock──► │
   │                          │   read item            │
   │                          │──pthread_mutex_unlock► │
   │   ◄──sem_post(empty)────│                        │
```

---

## Metrics (Bonus)

After each run, `results/metrics.txt` records:
- Total items produced / consumed
- Average time an item waited in the buffer (ms)

---

## Authors

- **[Your Name]** — AUS, CMP466 / OS Project

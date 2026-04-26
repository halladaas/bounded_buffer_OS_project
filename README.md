# Bounded Buffer — Multithreaded Producer-Consumer
**CMP-310 Operating Systems | Mini Project | Spring 2026**

---

## Overview
A multithreaded Producer-Consumer application in C using:
- **POSIX threads** for concurrency
- **Semaphores** to track empty/full buffer slots
- **Mutex** for mutual exclusion on the shared buffer
- **Circular bounded buffer** as the shared resource
- **Poison Pill** technique for graceful termination
- *(Bonus)* **Priority queue** — urgent items always consumed first
- *(Bonus)* **Latency & throughput metrics**

---

## Project Structure
```
bounded_buffer_OS_project/
├── src/
│   └── producer_consumer.c   ← all source code (single file)
├── docs/
│   └── design_notes.md       ← design decisions & sync explanation
├── results/
│   └── metrics.txt           ← auto-generated after each run
├── Makefile
├── .gitignore
└── README.md
```

---

## Compile & Run

### Manual
```bash
gcc -o producer_consumer src/producer_consumer.c -pthread
./producer_consumer <num_producers> <num_consumers> <buffer_size>
```

### Using Make
```bash
make              # compile
make run          # runs with 3 producers, 2 consumers, buffer 10
make test-small   # bonus: 4 producers, 4 consumers, buffer 2
make test-large   # bonus: 4 producers, 4 consumers, buffer 32
make clean        # remove binary and metrics
```

---

## Example
```bash
./producer_consumer 3 2 10
```
Runs with **3 producer threads**, **2 consumer threads**, and a **buffer size of 10**.

Each producer generates **20 items** → total = 3 × 20 = **60 items**.

### Sample Output
```
=== Producer-Consumer ===
Producers: 3 | Consumers: 2 | Buffer: 10 slots

[Producer-1] Produced item: 42  [normal]
[Producer-2] Produced item: 17  [URGENT]
[Consumer-1] Consumed item: 17  [URGENT]  (latency: 0.03 ms)
[Consumer-2] Consumed item: 42  [normal]  (latency: 0.04 ms)
...
[Producer-1] Finished producing 20 items.
[Main] All producers done. Sending poison pills...
[Consumer-1] Received poison pill. Exiting.
[Consumer-1] Finished consuming.
[Consumer-2] Received poison pill. Exiting.
[Consumer-2] Finished consuming.
```

---

## Termination (Poison Pill)
1. Each producer generates exactly 20 items then exits.
2. Main thread joins all producers.
3. Main inserts **one poison pill per consumer** into the buffer.
4. Each consumer exits when it dequeues a poison pill.
5. All resources (semaphores, mutexes, memory) are released.

---

## Bonus Features
### Priority Handling
- Each item has a priority field: `NORMAL (0)` or `URGENT (1)`
- ~25% of items are randomly marked urgent
- Consumers always dequeue urgent items before normal ones (FIFO within each priority)

### Metrics
After each run, printed to console and saved to `results/metrics.txt`:
- Total produced / consumed
- Average latency (ms) per item
- Overall throughput (items/second)

---

## Group Members
| Name | ID | Branch |
|------|----|--------|
| | | |
| | | |
| | | |
| | | |

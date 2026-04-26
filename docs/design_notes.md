# Design Notes
> Fill this in as a team — due with the final submission.

## Buffer Design
- Type: circular queue (two internal queues: urgent + normal)
- Size: configurable via command-line argument
- Semaphores: `empty` (free slots) and `full` (filled slots)
- Mutex: protects head/tail/count during insert/remove

## Synchronisation Plan
| Primitive | Role |
|-----------|------|
| `sem_t empty` | Blocks producer when buffer is full |
| `sem_t full` | Blocks consumer when buffer is empty |
| `pthread_mutex_t mutex` | Mutual exclusion on buffer read/write |
| `pthread_mutex_t metrics_mtx` | Protects shared metrics counters |

## Function List
| Function | File | Description |
|----------|------|-------------|
| `buffer_init` | producer_consumer.c | Initialise circular buffer + semaphores |
| `buffer_insert` | producer_consumer.c | Producer inserts item (blocks if full) |
| `buffer_remove` | producer_consumer.c | Consumer removes item (blocks if empty) |
| `producer_thread` | producer_consumer.c | Thread entry — generates 20 items |
| `consumer_thread` | producer_consumer.c | Thread entry — consumes until poison pill |
| `print_metrics` | producer_consumer.c | Reports latency + throughput |

## Challenges
- 
- 
- 

## Individual Contributions
| Member | Contribution |
|--------|-------------|
| | |
| | |

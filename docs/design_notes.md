# Design Notes

## Synchronisation Strategy

### Why semaphores + mutex?

| Primitive | Purpose |
|-----------|---------|
| `sem_t empty` | Counts free buffer slots — producers block here when buffer is full |
| `sem_t full`  | Counts filled slots — consumers block here when buffer is empty |
| `pthread_mutex_t mutex` | Protects head/tail/count; ensures only one thread modifies the buffer at a time |

Using **two semaphores** avoids busy-waiting. The mutex guards the critical section (the actual read/write of the circular array), keeping it as short as possible.

### Circular Buffer

- `head` = next write index (producer advances it)
- `tail` = next read  index (consumer advances it)
- Both wrap modulo `BUFFER_SIZE`
- `count` tracks occupancy for diagnostics (not used for sync)

## Thread Lifecycle

```
main()
 ├─ buffer_init()
 ├─ pthread_create × NUM_PRODUCERS  → producer_thread()
 ├─ pthread_create × NUM_CONSUMERS  → consumer_thread()
 ├─ pthread_join (all)
 ├─ print + save metrics
 └─ buffer_destroy()
```

## Item Distribution

Total items = `NUM_PRODUCERS × NUM_ITEMS`.  
Divided evenly among consumers; the last consumer absorbs any remainder.

## Metrics

A Welford-style running average is used for `avg_wait_ms` to avoid accumulating a large sum:

```
new_avg = old_avg + (sample - old_avg) / n
```

This is numerically stable and requires O(1) space.

## Potential Extensions

- [ ] Variable production/consumption rates (CLI args)
- [ ] Priority items (min-heap buffer)
- [ ] Multiple buffer instances with routing
- [ ] Monitor-based implementation (`pthread_cond_t`) instead of semaphores

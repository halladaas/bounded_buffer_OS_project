#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"

/* ── Circular bounded buffer ── */
typedef struct {
    item_t  slots[BUFFER_SIZE];
    int     head;       /* next write position */
    int     tail;       /* next read  position */
    int     count;      /* current occupancy   */

    /* synchronisation primitives */
    pthread_mutex_t mutex;
    sem_t           empty;  /* counts empty slots  */
    sem_t           full;   /* counts filled slots */
} buffer_t;

/* Lifecycle */
void buffer_init   (buffer_t *buf);
void buffer_destroy(buffer_t *buf);

/* Thread-safe operations (block when full / empty) */
void    buffer_insert(buffer_t *buf, item_t item);
item_t  buffer_remove(buffer_t *buf);

/* Utility */
void buffer_print_status(const buffer_t *buf);

#endif /* BUFFER_H */

#include "../include/buffer.h"
#include <string.h>

/* ── Init / Destroy ────────────────────────────────────────────────── */

void buffer_init(buffer_t *buf) {
    buf->head  = 0;
    buf->tail  = 0;
    buf->count = 0;
    memset(buf->slots, 0, sizeof(buf->slots));

    pthread_mutex_init(&buf->mutex, NULL);
    sem_init(&buf->empty, 0, BUFFER_SIZE); /* all slots start empty */
    sem_init(&buf->full,  0, 0);           /* no items yet           */
}

void buffer_destroy(buffer_t *buf) {
    pthread_mutex_destroy(&buf->mutex);
    sem_destroy(&buf->empty);
    sem_destroy(&buf->full);
}

/* ── Insert (producer side) ────────────────────────────────────────── */

void buffer_insert(buffer_t *buf, item_t item) {
    sem_wait(&buf->empty);              /* block if no empty slot  */
    pthread_mutex_lock(&buf->mutex);

    buf->slots[buf->head] = item;
    buf->head = (buf->head + 1) % BUFFER_SIZE;
    buf->count++;

    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->full);               /* signal one more item    */
}

/* ── Remove (consumer side) ────────────────────────────────────────── */

item_t buffer_remove(buffer_t *buf) {
    sem_wait(&buf->full);               /* block if nothing to consume */
    pthread_mutex_lock(&buf->mutex);

    item_t item = buf->slots[buf->tail];
    buf->tail = (buf->tail + 1) % BUFFER_SIZE;
    buf->count--;

    pthread_mutex_unlock(&buf->mutex);
    sem_post(&buf->empty);              /* signal one more free slot   */

    return item;
}

/* ── Debug helper ──────────────────────────────────────────────────── */

void buffer_print_status(const buffer_t *buf) {
    printf("[Buffer] count=%d  head=%d  tail=%d\n",
           buf->count, buf->head, buf->tail);
}

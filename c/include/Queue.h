#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include "DList.h"

/* ------------------------------------------------------------------ */
/* Error codes                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    QUEUE_OK        = 0,
    QUEUE_ERR_EMPTY = 1,
    QUEUE_ERR_ALLOC = 2,
} QueueError;

/* ------------------------------------------------------------------ */
/* Queue                                                                */
/* FIFO wrapper around DList: enqueue at back, dequeue from front.     */
/* ------------------------------------------------------------------ */

typedef struct {
    DList base;
} Queue;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

/* Unmanaged: caller owns every value pointer.                         */
QueueError  queue_new(Queue *q);

/* Managed: free_val called on each value when dequeued/destroyed.     *
 * clone_val used when copying; may be NULL if copy not needed.        */
QueueError  queue_new_managed(Queue *q, dl_free_fn free_val,
                              dl_clone_fn clone_val);

void        queue_destroy(Queue *q);

/* ------------------------------------------------------------------ */
/* Core operations                                                      */
/* ------------------------------------------------------------------ */

/* Enqueue: add val to the back of the queue.                          */
QueueError  queue_enqueue(Queue *q, void *val);

/* Dequeue: remove from the front; *out receives the value.            *
 * Caller takes ownership. Returns QUEUE_ERR_EMPTY if empty.           */
QueueError  queue_dequeue(Queue *q, void **out);

/* ------------------------------------------------------------------ */
/* Peek                                                                 */
/* ------------------------------------------------------------------ */

/* *out points into the queue; do not free.                            */
QueueError  queue_peek_front(const Queue *q, void **out);
QueueError  queue_peek_back(const Queue *q, void **out);

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t      queue_len(const Queue *q);
int         queue_is_empty(const Queue *q);

/* ------------------------------------------------------------------ */
/* Mutation                                                             */
/* ------------------------------------------------------------------ */

/* Clear frees managed values (if any) and resets to empty.            */
void        queue_clear(Queue *q);

/* Apply fn to every value in FIFO order (front → back).              */
void        queue_foreach(const Queue *q, void (*fn)(void *val));

/* ------------------------------------------------------------------ */
/* Diagnostics                                                          */
/* ------------------------------------------------------------------ */

const char *queue_strerror(QueueError err);

#endif /* QUEUE_H */

#include "Queue.h"
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static QueueError map_err(DListError e)
{
    switch (e) {
    case DL_OK:        return QUEUE_OK;
    case DL_ERR_EMPTY: return QUEUE_ERR_EMPTY;
    default:           return QUEUE_ERR_ALLOC;
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

QueueError queue_new(Queue *q)
{
    DListError e = dlist_new(&q->base);
    return map_err(e);
}

QueueError queue_new_managed(Queue *q, dl_free_fn free_val,
                              dl_clone_fn clone_val)
{
    DListError e = dlist_new_managed(&q->base, free_val, clone_val);
    return map_err(e);
}

void queue_destroy(Queue *q)
{
    dlist_destroy(&q->base);
}

/* ------------------------------------------------------------------ */
/* Core operations                                                      */
/* ------------------------------------------------------------------ */

QueueError queue_enqueue(Queue *q, void *val)
{
    DListError e = dlist_push_back(&q->base, val);
    return map_err(e);
}

QueueError queue_dequeue(Queue *q, void **out)
{
    DListError e = dlist_pop_front(&q->base, out);
    return map_err(e);
}

/* ------------------------------------------------------------------ */
/* Peek                                                                 */
/* ------------------------------------------------------------------ */

QueueError queue_peek_front(const Queue *q, void **out)
{
    DListError e = dlist_peek_front(&q->base, out);
    return map_err(e);
}

QueueError queue_peek_back(const Queue *q, void **out)
{
    DListError e = dlist_peek_back(&q->base, out);
    return map_err(e);
}

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t queue_len(const Queue *q)
{
    return dlist_len(&q->base);
}

int queue_is_empty(const Queue *q)
{
    return dlist_is_empty(&q->base);
}

/* ------------------------------------------------------------------ */
/* Mutation                                                             */
/* ------------------------------------------------------------------ */

void queue_clear(Queue *q)
{
    dlist_clear(&q->base);
}

void queue_foreach(const Queue *q, void (*fn)(void *val))
{
    dlist_foreach((DList *)&q->base, fn);
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                          */
/* ------------------------------------------------------------------ */

const char *queue_strerror(QueueError err)
{
    switch (err) {
    case QUEUE_OK:        return "ok";
    case QUEUE_ERR_EMPTY: return "queue is empty";
    case QUEUE_ERR_ALLOC: return "memory allocation failed";
    default:              return "unknown error";
    }
}

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Error codes                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    RB_OK             = 0,
    RB_ERR_EMPTY      = 1,
    RB_ERR_FULL       = 2,
    RB_ERR_OOB        = 3,
    RB_ERR_ALLOC      = 4,
    RB_ERR_BAD_CONFIG = 5,
} RingBufError;

/* ------------------------------------------------------------------ */
/* RingBuf                                                              */
/*                                                                      */
/* Fixed-capacity or growable circular buffer of fixed-size elements.  */
/* Elements are stored by value (memcpy in/out).                       */
/* ------------------------------------------------------------------ */

typedef struct {
    void   *data;       /* heap-allocated element array                */
    size_t  len;        /* number of live elements                     */
    size_t  cap;        /* total slots (power-of-2 when growable)      */
    size_t  elem_size;  /* bytes per element                           */
    size_t  head;       /* index of the oldest element                 */
    size_t  tail;       /* index of the next free slot                 */
    int     growable;   /* 0 = fixed, 1 = auto-double on full          */
} RingBuf;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

/* Fixed-capacity buffer of exactly cap slots.                         */
RingBufError rb_new(RingBuf *rb, size_t cap, size_t elem_size);

/* Growable buffer; initial capacity rounded up to next power-of-2.   *
 * Returns RB_ERR_BAD_CONFIG if init_cap is 0.                        */
RingBufError rb_new_growable(RingBuf *rb, size_t init_cap, size_t elem_size);

void         rb_destroy(RingBuf *rb);

/* ------------------------------------------------------------------ */
/* Core operations                                                      */
/* ------------------------------------------------------------------ */

/* Enqueue: copy elem_size bytes from val into the back slot.          *
 * Fixed:   returns RB_ERR_FULL when at capacity.                      *
 * Growable: doubles capacity first, then enqueues.                   */
RingBufError rb_enqueue(RingBuf *rb, const void *val);

/* Dequeue: copy elem_size bytes to *out, remove from front.           *
 * Returns RB_ERR_EMPTY when empty.                                    */
RingBufError rb_dequeue(RingBuf *rb, void *out);

/* ------------------------------------------------------------------ */
/* Peek / random access                                                 */
/* ------------------------------------------------------------------ */

/* *out is set to a pointer into the buffer; do not free, do not       *
 * enqueue while holding the pointer (may invalidate on grow).         */
RingBufError rb_peek_front(const RingBuf *rb, void **out);
RingBufError rb_peek_back(const RingBuf *rb, void **out);

/* Logical index i (0 = oldest). Returns pointer into internal data.  */
RingBufError rb_get(const RingBuf *rb, size_t i, void **out);

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t       rb_len(const RingBuf *rb);
size_t       rb_cap(const RingBuf *rb);
int          rb_is_empty(const RingBuf *rb);
int          rb_is_full(const RingBuf *rb);

/* ------------------------------------------------------------------ */
/* Mutation                                                             */
/* ------------------------------------------------------------------ */

void         rb_clear(RingBuf *rb);

/* Reserve at least additional more slots (growable only).             *
 * No-op on fixed buffers; returns RB_ERR_BAD_CONFIG in that case.    */
RingBufError rb_reserve(RingBuf *rb, size_t additional);

/* ------------------------------------------------------------------ */
/* Diagnostics                                                          */
/* ------------------------------------------------------------------ */

const char  *rb_strerror(RingBufError err);

#endif /* RINGBUF_H */

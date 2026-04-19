#include "RingBuf.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

/* Round n up to the next power of 2 (minimum 1). */
static size_t next_pow2(size_t n)
{
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > 0xFFFFFFFFU
    n |= n >> 32;
#endif
    return n + 1;
}

/* Map logical index i (0 = oldest) to physical array index. */
static size_t rb_phys(const RingBuf *rb, size_t i)
{
    if (rb->growable)
        return (rb->head + i) & (rb->cap - 1);   /* power-of-2 mask */
    else
        return (rb->head + i) % rb->cap;
}

/* Pointer to the physical slot at logical index i. */
static void *rb_slot(const RingBuf *rb, size_t i)
{
    return (char *)rb->data + rb_phys(rb, i) * rb->elem_size;
}

/* Double capacity; rearrange elements to be contiguous from slot 0.  *
 * Only called for growable buffers.                                   */
static RingBufError rb_grow(RingBuf *rb)
{
    size_t new_cap  = rb->cap * 2;
    void  *new_data = malloc(new_cap * rb->elem_size);
    if (!new_data) return RB_ERR_ALLOC;

    /* Copy elements in logical order into the new array. */
    for (size_t i = 0; i < rb->len; i++) {
        void *src = rb_slot(rb, i);
        void *dst = (char *)new_data + i * rb->elem_size;
        memcpy(dst, src, rb->elem_size);
    }

    free(rb->data);
    rb->data = new_data;
    rb->cap  = new_cap;
    rb->head = 0;
    rb->tail = rb->len;
    return RB_OK;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

RingBufError rb_new(RingBuf *rb, size_t cap, size_t elem_size)
{
    if (cap == 0 || elem_size == 0) return RB_ERR_BAD_CONFIG;

    rb->data      = malloc(cap * elem_size);
    if (!rb->data) return RB_ERR_ALLOC;

    rb->len       = 0;
    rb->cap       = cap;
    rb->elem_size = elem_size;
    rb->head      = 0;
    rb->tail      = 0;
    rb->growable  = 0;
    return RB_OK;
}

RingBufError rb_new_growable(RingBuf *rb, size_t init_cap, size_t elem_size)
{
    if (init_cap == 0 || elem_size == 0) return RB_ERR_BAD_CONFIG;

    size_t cap    = next_pow2(init_cap);
    rb->data      = malloc(cap * elem_size);
    if (!rb->data) return RB_ERR_ALLOC;

    rb->len       = 0;
    rb->cap       = cap;
    rb->elem_size = elem_size;
    rb->head      = 0;
    rb->tail      = 0;
    rb->growable  = 1;
    return RB_OK;
}

void rb_destroy(RingBuf *rb)
{
    free(rb->data);
    rb->data = NULL;
    rb->len  = 0;
    rb->cap  = 0;
}

/* ------------------------------------------------------------------ */
/* Core operations                                                      */
/* ------------------------------------------------------------------ */

RingBufError rb_enqueue(RingBuf *rb, const void *val)
{
    if (rb->len == rb->cap) {
        if (!rb->growable) return RB_ERR_FULL;
        RingBufError e = rb_grow(rb);
        if (e != RB_OK) return e;
    }

    void *dst = (char *)rb->data + rb->tail * rb->elem_size;
    memcpy(dst, val, rb->elem_size);

    if (rb->growable)
        rb->tail = (rb->tail + 1) & (rb->cap - 1);
    else
        rb->tail = (rb->tail + 1) % rb->cap;

    rb->len++;
    return RB_OK;
}

RingBufError rb_dequeue(RingBuf *rb, void *out)
{
    if (rb->len == 0) return RB_ERR_EMPTY;

    void *src = (char *)rb->data + rb->head * rb->elem_size;
    memcpy(out, src, rb->elem_size);

    if (rb->growable)
        rb->head = (rb->head + 1) & (rb->cap - 1);
    else
        rb->head = (rb->head + 1) % rb->cap;

    rb->len--;
    return RB_OK;
}

/* ------------------------------------------------------------------ */
/* Peek / random access                                                 */
/* ------------------------------------------------------------------ */

RingBufError rb_peek_front(const RingBuf *rb, void **out)
{
    if (rb->len == 0) return RB_ERR_EMPTY;
    *out = rb_slot(rb, 0);
    return RB_OK;
}

RingBufError rb_peek_back(const RingBuf *rb, void **out)
{
    if (rb->len == 0) return RB_ERR_EMPTY;
    *out = rb_slot(rb, rb->len - 1);
    return RB_OK;
}

RingBufError rb_get(const RingBuf *rb, size_t i, void **out)
{
    if (i >= rb->len) return RB_ERR_OOB;
    *out = rb_slot(rb, i);
    return RB_OK;
}

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t rb_len(const RingBuf *rb)    { return rb->len; }
size_t rb_cap(const RingBuf *rb)    { return rb->cap; }
int    rb_is_empty(const RingBuf *rb) { return rb->len == 0; }
int    rb_is_full(const RingBuf *rb)  { return rb->len == rb->cap; }

/* ------------------------------------------------------------------ */
/* Mutation                                                             */
/* ------------------------------------------------------------------ */

void rb_clear(RingBuf *rb)
{
    rb->len  = 0;
    rb->head = 0;
    rb->tail = 0;
}

RingBufError rb_reserve(RingBuf *rb, size_t additional)
{
    if (!rb->growable) return RB_ERR_BAD_CONFIG;
    if (additional == 0) return RB_OK;

    size_t needed = rb->len + additional;
    while (rb->cap < needed) {
        RingBufError e = rb_grow(rb);
        if (e != RB_OK) return e;
    }
    return RB_OK;
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                          */
/* ------------------------------------------------------------------ */

const char *rb_strerror(RingBufError err)
{
    switch (err) {
    case RB_OK:             return "ok";
    case RB_ERR_EMPTY:      return "ring buffer is empty";
    case RB_ERR_FULL:       return "ring buffer is full";
    case RB_ERR_OOB:        return "index out of bounds";
    case RB_ERR_ALLOC:      return "memory allocation failed";
    case RB_ERR_BAD_CONFIG: return "invalid configuration";
    default:                return "unknown error";
    }
}

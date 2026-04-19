#include "Stack.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  错误信息                                                            */
/* ------------------------------------------------------------------ */

const char *stack_strerror(StackError err)
{
    switch (err) {
    case STACK_OK:          return "ok";
    case STACK_ERR_EMPTY:   return "stack is empty";
    case STACK_ERR_ALLOC:   return "memory allocation failed";
    default:                return "unknown error";
    }
}

/* ================================================================== */
/*  ArrayStack                                                         */
/* ================================================================== */

StackError astack_new(ArrayStack *s, size_t elem_size)
{
    VecError e = vec_new(&s->base, elem_size);
    return e == VEC_OK ? STACK_OK : STACK_ERR_ALLOC;
}

StackError astack_new_with_cap(ArrayStack *s, size_t elem_size, size_t cap)
{
    VecError e = vec_new_with_cap(&s->base, elem_size, cap);
    return e == VEC_OK ? STACK_OK : STACK_ERR_ALLOC;
}

void astack_destroy(ArrayStack *s)
{
    vec_destroy(&s->base);
}

StackError astack_push(ArrayStack *s, const void *val)
{
    VecError e = vec_push(&s->base, val);
    return e == VEC_OK ? STACK_OK : STACK_ERR_ALLOC;
}

StackError astack_pop(ArrayStack *s, void *out)
{
    VecError e = vec_pop(&s->base, out);
    if (e == VEC_ERR_EMPTY) return STACK_ERR_EMPTY;
    return e == VEC_OK ? STACK_OK : STACK_ERR_ALLOC;
}

StackError astack_peek(const ArrayStack *s, void **out)
{
    if (vec_is_empty(&s->base)) return STACK_ERR_EMPTY;
    /* 栈顶是末尾元素：data + (len-1) * elem_size */
    size_t   elem_size = s->base.elem_size;
    size_t   idx       = s->base.len - 1;
    *out = (char *)s->base.data + idx * elem_size;
    return STACK_OK;
}

size_t astack_len(const ArrayStack *s)      { return vec_len(&s->base); }
int    astack_is_empty(const ArrayStack *s) { return vec_is_empty(&s->base); }

void astack_clear(ArrayStack *s)            { vec_clear(&s->base); }

StackError astack_reserve(ArrayStack *s, size_t additional)
{
    VecError e = vec_reserve(&s->base, additional);
    return e == VEC_OK ? STACK_OK : STACK_ERR_ALLOC;
}

StackError astack_shrink_to_fit(ArrayStack *s)
{
    VecError e = vec_shrink_to_fit(&s->base);
    return e == VEC_OK ? STACK_OK : STACK_ERR_ALLOC;
}

/* ================================================================== */
/*  ListStack                                                          */
/* ================================================================== */

StackError lstack_new(ListStack *s)
{
    s->top      = NULL;
    s->len      = 0;
    s->free_val = NULL;
    return STACK_OK;
}

StackError lstack_new_managed(ListStack *s, stack_free_fn free_val)
{
    s->top      = NULL;
    s->len      = 0;
    s->free_val = free_val;
    return STACK_OK;
}

void lstack_destroy(ListStack *s)
{
    lstack_clear(s);
}

StackError lstack_push(ListStack *s, void *val)
{
    StackNode *node = (StackNode *)malloc(sizeof(StackNode));
    if (!node) return STACK_ERR_ALLOC;

    node->val  = val;
    node->next = s->top;
    s->top     = node;
    s->len++;
    return STACK_OK;
}

StackError lstack_pop(ListStack *s, void **out)
{
    if (!s->top) return STACK_ERR_EMPTY;

    StackNode *node = s->top;
    s->top = node->next;
    s->len--;

    *out = node->val;
    free(node);
    return STACK_OK;
}

StackError lstack_peek(const ListStack *s, void **out)
{
    if (!s->top) return STACK_ERR_EMPTY;
    *out = s->top->val;
    return STACK_OK;
}

size_t lstack_len(const ListStack *s)      { return s->len; }
int    lstack_is_empty(const ListStack *s) { return s->top == NULL; }

void lstack_clear(ListStack *s)
{
    StackNode *cur = s->top;
    while (cur) {
        StackNode *next = cur->next;
        if (s->free_val) s->free_val(cur->val);
        free(cur);
        cur = next;
    }
    s->top = NULL;
    s->len = 0;
}

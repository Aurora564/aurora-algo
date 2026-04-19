/**
 * Stack.h — 栈（C99）
 *
 * 提供两种实现：
 *  - ArrayStack：基于 Vec 的动态数组版，连续内存，缓存友好。
 *  - ListStack ：基于轻量单链表，每次 push/pop 动态分配/释放节点。
 *
 * 错误码
 * ------
 *   STACK_OK         操作成功
 *   STACK_ERR_EMPTY  pop/peek 时栈为空
 *   STACK_ERR_ALLOC  内存分配失败
 */

#ifndef STACK_H
#define STACK_H

#include "devc.h"   /* Vec, VecError, free_fn_t */
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    STACK_OK         = 0,
    STACK_ERR_EMPTY,    /* pop/peek 时栈为空     */
    STACK_ERR_ALLOC     /* 内存分配失败          */
} StackError;

const char *stack_strerror(StackError err);

/* ================================================================== */
/*  ArrayStack — 基于 Vec                                              */
/* ================================================================== */

typedef struct {
    Vec base;           /* 底层 Vec；栈顶 = base.data[base.len - 1] */
} ArrayStack;

/* -- 生命周期 ------------------------------------------------------- */

/** 创建空数组栈，不托管元素资源 */
StackError astack_new(ArrayStack *s, size_t elem_size);

/** 创建并预分配 cap 个元素的容量 */
StackError astack_new_with_cap(ArrayStack *s, size_t elem_size, size_t cap);

/** 销毁栈，托管模式下逐元素调用 free_elem */
void astack_destroy(ArrayStack *s);

/* -- 核心操作 ------------------------------------------------------- */

/** 将 val 压入栈顶；必要时扩容；均摊 O(1) */
StackError astack_push(ArrayStack *s, const void *val);

/**
 * 弹出栈顶元素到 out（memcpy）；O(1)。
 * 托管模式下所有权转移给调用者（不调用 free_elem）。
 */
StackError astack_pop(ArrayStack *s, void *out);

/** 返回栈顶元素的指针（不移除）；O(1) */
StackError astack_peek(const ArrayStack *s, void **out);

/* -- 查询 ----------------------------------------------------------- */

size_t astack_len(const ArrayStack *s);
int    astack_is_empty(const ArrayStack *s);

/* -- 容量管理 ------------------------------------------------------- */

/** 清空所有元素；托管模式下逐元素调用 free_elem；容量不变 */
void astack_clear(ArrayStack *s);

/** 预留至少 additional 个额外槽位，规避扩容抖动 */
StackError astack_reserve(ArrayStack *s, size_t additional);

/** 容量收缩至当前元素数量 */
StackError astack_shrink_to_fit(ArrayStack *s);

/* ================================================================== */
/*  ListStack — 基于轻量单链表                                          */
/* ================================================================== */

typedef void (*stack_free_fn)(void *);

typedef struct StackNode {
    void             *val;
    struct StackNode *next;
} StackNode;

typedef struct {
    StackNode      *top;        /* 栈顶节点；空栈时为 NULL */
    size_t          len;
    stack_free_fn   free_val;   /* NULL → 不托管元素       */
} ListStack;

/* -- 生命周期 ------------------------------------------------------- */

/** 创建空链表栈，不托管元素资源 */
StackError lstack_new(ListStack *s);

/** 创建空链表栈，托管元素资源（弹出/销毁时调用 free_val） */
StackError lstack_new_managed(ListStack *s, stack_free_fn free_val);

/** 销毁栈；托管模式下逐元素调用 free_val */
void lstack_destroy(ListStack *s);

/* -- 核心操作 ------------------------------------------------------- */

/** 将 val 压入栈顶；O(1) */
StackError lstack_push(ListStack *s, void *val);

/**
 * 弹出栈顶元素，将指针写入 *out；O(1)。
 * 托管模式下所有权转移给调用者（不调用 free_val）。
 */
StackError lstack_pop(ListStack *s, void **out);

/** 返回栈顶元素的指针（不移除）；O(1) */
StackError lstack_peek(const ListStack *s, void **out);

/* -- 查询 ----------------------------------------------------------- */

size_t lstack_len(const ListStack *s);
int    lstack_is_empty(const ListStack *s);

/* -- 容量管理 ------------------------------------------------------- */

/** 清空所有元素；托管模式下逐元素调用 free_val */
void lstack_clear(ListStack *s);

#endif /* STACK_H */

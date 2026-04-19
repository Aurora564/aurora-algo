#include "SList.h"
#include <stdlib.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/*  内部工具                                                            */
/* ------------------------------------------------------------------ */

/** 分配并初始化一个新节点 */
static SLNode *node_new(void *val, SLNode *next)
{
    SLNode *node = malloc(sizeof(SLNode));
    if (!node) return NULL;
    node->val  = val;
    node->next = next;
    return node;
}

/* ------------------------------------------------------------------ */
/*  生命周期                                                            */
/* ------------------------------------------------------------------ */

SListError slist_new(SList *list)
{
    return slist_new_managed(list, NULL, NULL);
}

SListError slist_new_managed(SList *list, sl_free_fn free_val, sl_clone_fn clone_val)
{
    SLNode *dummy = node_new(NULL, NULL);
    if (!dummy) return SL_ERR_ALLOC;

    list->dummy     = dummy;
    list->tail      = dummy;
    list->len       = 0;
    list->free_val  = free_val;
    list->clone_val = clone_val;
    return SL_OK;
}

void slist_destroy(SList *list)
{
    if (!list || !list->dummy) return;
    slist_clear(list);
    free(list->dummy);
    list->dummy = NULL;
    list->tail  = NULL;
}

void slist_move(SList *dst, SList *src)
{
    *dst = *src;
    src->dummy = NULL;
    src->tail  = NULL;
    src->len   = 0;
}

SListError slist_copy(SList *dst, const SList *src)
{
    /* 浅拷贝：dst 不继承 free_val（避免双重释放），但保留 clone_val */
    SListError err = slist_new_managed(dst, NULL, src->clone_val);
    if (err != SL_OK) return err;

    for (SLNode *curr = src->dummy->next; curr; curr = curr->next) {
        err = slist_push_back(dst, curr->val);
        if (err != SL_OK) {
            slist_destroy(dst);
            return err;
        }
    }
    return SL_OK;
}

SListError slist_clone(SList *dst, const SList *src)
{
    assert(src->clone_val != NULL); /* 编程错误：未提供 clone_fn */
    if (!src->clone_val) return SL_ERR_NO_CLONE_FN;

    /* dst 继承完整钩子：它拥有克隆出的值 */
    SListError err = slist_new_managed(dst, src->free_val, src->clone_val);
    if (err != SL_OK) return err;

    for (SLNode *curr = src->dummy->next; curr; curr = curr->next) {
        void *cloned = src->clone_val(curr->val);
        if (!cloned) {
            slist_destroy(dst);
            return SL_ERR_ALLOC;
        }
        err = slist_push_back(dst, cloned);
        if (err != SL_OK) {
            if (dst->free_val) dst->free_val(cloned);
            slist_destroy(dst);
            return err;
        }
    }
    return SL_OK;
}

/* ------------------------------------------------------------------ */
/*  基本读写                                                            */
/* ------------------------------------------------------------------ */

SListError slist_push_front(SList *list, void *val)
{
    SLNode *node = node_new(val, list->dummy->next);
    if (!node) return SL_ERR_ALLOC;

    list->dummy->next = node;
    if (list->tail == list->dummy)  /* 原来为空 */
        list->tail = node;
    list->len++;
    return SL_OK;
}

SListError slist_push_back(SList *list, void *val)
{
    SLNode *node = node_new(val, NULL);
    if (!node) return SL_ERR_ALLOC;

    list->tail->next = node;
    list->tail       = node;
    list->len++;
    return SL_OK;
}

SListError slist_pop_front(SList *list, void **out)
{
    if (list->len == 0) return SL_ERR_EMPTY;

    SLNode *node = list->dummy->next;
    if (out) *out = node->val;

    list->dummy->next = node->next;
    if (node == list->tail)         /* 移除了最后一个节点 */
        list->tail = list->dummy;

    free(node);
    list->len--;
    return SL_OK;
}

SListError slist_pop_back(SList *list, void **out)
{
    if (list->len == 0) return SL_ERR_EMPTY;

    /* 从哨兵出发，找 tail 的前驱 */
    SLNode *prev = list->dummy;
    while (prev->next != list->tail)
        prev = prev->next;

    if (out) *out = list->tail->val;
    free(list->tail);
    prev->next = NULL;
    list->tail = prev;
    list->len--;
    return SL_OK;
}

SListError slist_peek_front(const SList *list, void **out)
{
    if (list->len == 0) return SL_ERR_EMPTY;
    *out = list->dummy->next->val;
    return SL_OK;
}

SListError slist_peek_back(const SList *list, void **out)
{
    if (list->len == 0) return SL_ERR_EMPTY;
    *out = list->tail->val;
    return SL_OK;
}

SListError slist_insert_after(SList *list, SLNode *node, void *val)
{
    SLNode *new_node = node_new(val, node->next);
    if (!new_node) return SL_ERR_ALLOC;

    node->next = new_node;
    if (node == list->tail)
        list->tail = new_node;
    list->len++;
    return SL_OK;
}

SListError slist_remove_after(SList *list, SLNode *node, void **out)
{
    if (!node->next) return SL_ERR_OOB;     /* node 是尾节点，无后继 */

    SLNode *target = node->next;
    if (out) *out = target->val;

    node->next = target->next;
    if (target == list->tail)
        list->tail = node;

    free(target);
    list->len--;
    return SL_OK;
}

SListError slist_get(const SList *list, size_t i, void **out)
{
    if (i >= list->len) return SL_ERR_OOB;

    SLNode *curr = list->dummy->next;
    for (size_t j = 0; j < i; j++)
        curr = curr->next;

    *out = curr->val;
    return SL_OK;
}

SLNode *slist_find(const SList *list, int (*pred)(const void *val))
{
    for (SLNode *curr = list->dummy->next; curr; curr = curr->next)
        if (pred(curr->val)) return curr;
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  容量管理                                                            */
/* ------------------------------------------------------------------ */

size_t slist_len(const SList *list)      { return list->len; }
int    slist_is_empty(const SList *list) { return list->len == 0; }

void slist_clear(SList *list)
{
    SLNode *curr = list->dummy->next;
    while (curr) {
        SLNode *next = curr->next;
        if (list->free_val) list->free_val(curr->val);
        free(curr);
        curr = next;
    }
    list->dummy->next = NULL;
    list->tail        = list->dummy;
    list->len         = 0;
}

/* ------------------------------------------------------------------ */
/*  遍历                                                                */
/* ------------------------------------------------------------------ */

void slist_foreach(const SList *list,
                   void (*fn)(void *val, void *user_data),
                   void *user_data)
{
    for (SLNode *curr = list->dummy->next; curr; curr = curr->next)
        fn(curr->val, user_data);
}

void slist_reverse(SList *list)
{
    if (list->len <= 1) return;

    SLNode *new_tail = list->dummy->next;   /* 当前头节点将成为新尾节点 */
    SLNode *prev     = NULL;
    SLNode *curr     = list->dummy->next;

    while (curr) {
        SLNode *next = curr->next;
        curr->next   = prev;
        prev         = curr;
        curr         = next;
    }

    list->dummy->next = prev;   /* prev 是原尾节点，成为新头节点 */
    new_tail->next    = NULL;
    list->tail        = new_tail;
}

/* ------------------------------------------------------------------ */
/*  错误信息                                                            */
/* ------------------------------------------------------------------ */

const char *slist_strerror(SListError err)
{
    switch (err) {
    case SL_OK:              return "ok";
    case SL_ERR_EMPTY:       return "list is empty";
    case SL_ERR_OOB:         return "out of bounds";
    case SL_ERR_ALLOC:       return "memory allocation failed";
    case SL_ERR_NO_CLONE_FN: return "clone_fn not provided";
    default:                 return "unknown error";
    }
}

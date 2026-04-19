#include "DList.h"

#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Internal primitives                                                  */
/* ------------------------------------------------------------------ */

/* Insert node immediately before ref.  Wires all 4 affected pointers. */
static void dl_link_before(DLNode *ref, DLNode *node)
{
    node->prev       = ref->prev;
    node->next       = ref;
    ref->prev->next  = node;
    ref->prev        = node;
}

/* Detach node from its neighbours.  Does NOT free anything.           */
static void dl_unlink(DLNode *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

/* Allocate and wire a new node holding val before ref.                */
static DListError node_new_before(DLNode *ref, void *val)
{
    DLNode *n = (DLNode *)malloc(sizeof(DLNode));
    if (!n) return DL_ERR_ALLOC;
    n->val = val;
    dl_link_before(ref, n);
    return DL_OK;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

DListError dlist_new(DList *list)
{
    return dlist_new_managed(list, NULL, NULL);
}

DListError dlist_new_managed(DList *list, dl_free_fn free_val,
                              dl_clone_fn clone_val)
{
    DLNode *dummy = (DLNode *)malloc(sizeof(DLNode));
    if (!dummy) return DL_ERR_ALLOC;

    dummy->val  = NULL;
    dummy->prev = dummy;
    dummy->next = dummy;

    list->dummy     = dummy;
    list->len       = 0;
    list->free_val  = free_val;
    list->clone_val = clone_val;
    return DL_OK;
}

void dlist_destroy(DList *list)
{
    dlist_clear(list);
    free(list->dummy);
    list->dummy = NULL;
}

/* ------------------------------------------------------------------ */
/* Bulk copy / move                                                     */
/* ------------------------------------------------------------------ */

void dlist_move(DList *dst, DList *src)
{
    /* Splice the entire src chain into dst by rewiring the sentinels. */
    if (src->len == 0) {
        *dst = *src;
        /* reset src to a valid empty state using its own sentinel     */
        src->dummy->prev = src->dummy;
        src->dummy->next = src->dummy;
        src->len = 0;
        return;
    }

    DLNode *src_first = src->dummy->next;
    DLNode *src_last  = src->dummy->prev;
    DLNode *dst_dummy = src->dummy; /* reuse src sentinel as dst's    */

    /* Wire src chain into dst sentinel */
    dst_dummy->next       = src_first;
    dst_dummy->prev       = src_last;
    src_first->prev       = dst_dummy;
    src_last->next        = dst_dummy;

    dst->dummy     = dst_dummy;
    dst->len       = src->len;
    dst->free_val  = src->free_val;
    dst->clone_val = src->clone_val;

    /* src sentinel is now owned by dst; point src at its own dummy   */
    /* but we no longer have one — caller must not use src after move  */
    src->dummy = NULL;
    src->len   = 0;
}

DListError dlist_copy(DList *dst, const DList *src)
{
    DListError err = dlist_new(dst);
    if (err != DL_OK) return err;

    DLNode *curr = src->dummy->next;
    while (curr != src->dummy) {
        err = node_new_before(dst->dummy, curr->val);
        if (err != DL_OK) { dlist_destroy(dst); return err; }
        dst->len++;
        curr = curr->next;
    }
    return DL_OK;
}

DListError dlist_clone(DList *dst, const DList *src)
{
    if (!src->clone_val) return DL_ERR_NO_CLONE_FN;

    DListError err = dlist_new_managed(dst, src->free_val, src->clone_val);
    if (err != DL_OK) return err;

    DLNode *curr = src->dummy->next;
    while (curr != src->dummy) {
        void *cloned = src->clone_val(curr->val);
        if (!cloned) { dlist_destroy(dst); return DL_ERR_ALLOC; }
        err = node_new_before(dst->dummy, cloned);
        if (err != DL_OK) {
            if (dst->free_val) dst->free_val(cloned);
            dlist_destroy(dst);
            return err;
        }
        dst->len++;
        curr = curr->next;
    }
    return DL_OK;
}

/* ------------------------------------------------------------------ */
/* Push / pop                                                           */
/* ------------------------------------------------------------------ */

DListError dlist_push_front(DList *list, void *val)
{
    /* Insert before the current first data node (= before dummy->next) */
    DListError err = node_new_before(list->dummy->next, val);
    if (err == DL_OK) list->len++;
    return err;
}

DListError dlist_push_back(DList *list, void *val)
{
    /* Insert before the sentinel = append at tail */
    DListError err = node_new_before(list->dummy, val);
    if (err == DL_OK) list->len++;
    return err;
}

DListError dlist_pop_front(DList *list, void **out)
{
    if (list->len == 0) return DL_ERR_EMPTY;
    DLNode *n = list->dummy->next;
    *out = n->val;
    dl_unlink(n);
    free(n);
    list->len--;
    return DL_OK;
}

DListError dlist_pop_back(DList *list, void **out)
{
    if (list->len == 0) return DL_ERR_EMPTY;
    DLNode *n = list->dummy->prev;
    *out = n->val;
    dl_unlink(n);
    free(n);
    list->len--;
    return DL_OK;
}

/* ------------------------------------------------------------------ */
/* Peek                                                                 */
/* ------------------------------------------------------------------ */

DListError dlist_peek_front(const DList *list, void **out)
{
    if (list->len == 0) return DL_ERR_EMPTY;
    *out = list->dummy->next->val;
    return DL_OK;
}

DListError dlist_peek_back(const DList *list, void **out)
{
    if (list->len == 0) return DL_ERR_EMPTY;
    *out = list->dummy->prev->val;
    return DL_OK;
}

/* ------------------------------------------------------------------ */
/* Node-relative insert / remove                                        */
/* ------------------------------------------------------------------ */

DListError dlist_insert_before(DList *list, DLNode *node, void *val)
{
    DListError err = node_new_before(node, val);
    if (err == DL_OK) list->len++;
    return err;
}

DListError dlist_insert_after(DList *list, DLNode *node, void *val)
{
    DListError err = node_new_before(node->next, val);
    if (err == DL_OK) list->len++;
    return err;
}

DListError dlist_remove(DList *list, DLNode *node, void **out)
{
    *out = node->val;
    dl_unlink(node);
    free(node);
    list->len--;
    return DL_OK;
}

/* ------------------------------------------------------------------ */
/* Access                                                               */
/* ------------------------------------------------------------------ */

DListError dlist_get(const DList *list, size_t i, void **out)
{
    if (i >= list->len) return DL_ERR_OOB;

    DLNode *curr;
    if (i < list->len / 2) {
        curr = list->dummy->next;
        for (size_t k = 0; k < i; k++) curr = curr->next;
    } else {
        curr = list->dummy->prev;
        for (size_t k = list->len - 1; k > i; k--) curr = curr->prev;
    }
    *out = curr->val;
    return DL_OK;
}

DLNode *dlist_find(const DList *list, int (*pred)(const void *val))
{
    DLNode *curr = list->dummy->next;
    while (curr != list->dummy) {
        if (pred(curr->val)) return curr;
        curr = curr->next;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t dlist_len(const DList *list)     { return list->len; }
int    dlist_is_empty(const DList *list){ return list->len == 0; }

/* ------------------------------------------------------------------ */
/* Mutation                                                             */
/* ------------------------------------------------------------------ */

void dlist_clear(DList *list)
{
    DLNode *curr = list->dummy->next;
    while (curr != list->dummy) {
        DLNode *next = curr->next;
        if (list->free_val) list->free_val(curr->val);
        free(curr);
        curr = next;
    }
    list->dummy->prev = list->dummy;
    list->dummy->next = list->dummy;
    list->len = 0;
}

void dlist_foreach(DList *list, void (*fn)(void *val))
{
    DLNode *curr = list->dummy->next;
    while (curr != list->dummy) {
        fn(curr->val);
        curr = curr->next;
    }
}

void dlist_foreach_rev(DList *list, void (*fn)(void *val))
{
    DLNode *curr = list->dummy->prev;
    while (curr != list->dummy) {
        fn(curr->val);
        curr = curr->prev;
    }
}

void dlist_reverse(DList *list)
{
    DLNode *curr = list->dummy;
    do {
        DLNode *tmp = curr->prev;
        curr->prev  = curr->next;
        curr->next  = tmp;
        curr        = curr->prev; /* was curr->next before swap */
    } while (curr != list->dummy);
}

DListError dlist_splice(DList *dst, DLNode *pos, DList *src)
{
    if (src->len == 0) return DL_OK;

    DLNode *src_first = src->dummy->next;
    DLNode *src_last  = src->dummy->prev;

    /* Detach entire src chain from its sentinel */
    src->dummy->next = src->dummy;
    src->dummy->prev = src->dummy;

    /* Wire src chain in before pos */
    src_first->prev  = pos->prev;
    src_last->next   = pos;
    pos->prev->next  = src_first;
    pos->prev        = src_last;

    dst->len += src->len;
    src->len  = 0;
    return DL_OK;
}

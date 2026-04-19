#ifndef DLIST_H
#define DLIST_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Callback types                                                       */
/* ------------------------------------------------------------------ */

typedef void  (*dl_free_fn)(void *val);
typedef void *(*dl_clone_fn)(const void *val);

/* ------------------------------------------------------------------ */
/* Node                                                                 */
/* ------------------------------------------------------------------ */

typedef struct DLNode {
    void            *val;
    struct DLNode   *prev;
    struct DLNode   *next;
} DLNode;

/* ------------------------------------------------------------------ */
/* List                                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    DLNode      *dummy;     /* circular sentinel; never holds user data */
    size_t       len;
    dl_free_fn   free_val;  /* NULL → caller owns values                */
    dl_clone_fn  clone_val; /* NULL → shallow copy                      */
} DList;

/* ------------------------------------------------------------------ */
/* Error codes                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    DL_OK              = 0,
    DL_ERR_EMPTY       = 1,
    DL_ERR_OOB         = 2,
    DL_ERR_ALLOC       = 3,
    DL_ERR_NO_CLONE_FN = 4,
} DListError;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

DListError dlist_new(DList *list);
DListError dlist_new_managed(DList *list, dl_free_fn free_val,
                             dl_clone_fn clone_val);
void       dlist_destroy(DList *list);

/* ------------------------------------------------------------------ */
/* Bulk copy / move                                                     */
/* ------------------------------------------------------------------ */

/* Move: transfer ownership; src becomes empty (invalid to use after). */
void       dlist_move(DList *dst, DList *src);

/* Copy (shallow): duplicates node structure; does NOT clone vals.     */
DListError dlist_copy(DList *dst, const DList *src);

/* Clone (deep): requires clone_val to be set on src.                  */
DListError dlist_clone(DList *dst, const DList *src);

/* ------------------------------------------------------------------ */
/* Push / pop                                                           */
/* ------------------------------------------------------------------ */

DListError dlist_push_front(DList *list, void *val);
DListError dlist_push_back(DList *list, void *val);

/* On success *out receives the value; caller takes ownership.         */
DListError dlist_pop_front(DList *list, void **out);
DListError dlist_pop_back(DList *list, void **out);

/* ------------------------------------------------------------------ */
/* Peek                                                                 */
/* ------------------------------------------------------------------ */

/* *out points into the list; do not free.                             */
DListError dlist_peek_front(const DList *list, void **out);
DListError dlist_peek_back(const DList *list, void **out);

/* ------------------------------------------------------------------ */
/* Node-relative insert / remove                                        */
/* ------------------------------------------------------------------ */

DListError dlist_insert_before(DList *list, DLNode *node, void *val);
DListError dlist_insert_after(DList *list, DLNode *node, void *val);

/* Detach node; *out receives its value (caller owns it).              */
DListError dlist_remove(DList *list, DLNode *node, void **out);

/* ------------------------------------------------------------------ */
/* Access                                                               */
/* ------------------------------------------------------------------ */

/* Bidirectional O(n/2): i < len/2 → forward, else backward.          */
DListError dlist_get(const DList *list, size_t i, void **out);

/* Returns first node satisfying pred, or NULL.                        */
DLNode    *dlist_find(const DList *list, int (*pred)(const void *val));

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t     dlist_len(const DList *list);
int        dlist_is_empty(const DList *list);

/* ------------------------------------------------------------------ */
/* Mutation                                                             */
/* ------------------------------------------------------------------ */

void       dlist_clear(DList *list);

/* Apply fn to every val in order (front → back).                      */
void       dlist_foreach(DList *list, void (*fn)(void *val));

/* Apply fn to every val in reverse order (back → front).              */
void       dlist_foreach_rev(DList *list, void (*fn)(void *val));

/* Reverse the list in-place in O(n).                                  */
void       dlist_reverse(DList *list);

/* Move all nodes from src into dst immediately before pos, in O(1).   *
 * pos must belong to dst. src becomes empty after the call.           */
DListError dlist_splice(DList *dst, DLNode *pos, DList *src);

#endif /* DLIST_H */

#include "AVL.h"
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  height helpers                                                      */
/* ------------------------------------------------------------------ */

static int8_t node_height(const AVLNode *n)
{
    return n ? n->height : 0;
}

static void update_height(AVLNode *n)
{
    int8_t l = node_height(n->left);
    int8_t r = node_height(n->right);
    n->height = (l > r ? l : r) + 1;
}

static int balance_factor(const AVLNode *n)
{
    return (int)node_height(n->left) - (int)node_height(n->right);
}

/* ------------------------------------------------------------------ */
/*  rotations                                                           */
/* ------------------------------------------------------------------ */

static AVLNode *rotate_right(AVLNode *n)
{
    AVLNode *x = n->left;
    n->left  = x->right;
    x->right = n;
    update_height(n);
    update_height(x);
    return x;
}

static AVLNode *rotate_left(AVLNode *n)
{
    AVLNode *y = n->right;
    n->right = y->left;
    y->left  = n;
    update_height(n);
    update_height(y);
    return y;
}

static AVLNode *rebalance(AVLNode *n)
{
    update_height(n);
    int bf = balance_factor(n);
    if (bf == 2) {
        if (balance_factor(n->left) < 0)
            n->left = rotate_left(n->left); /* LR */
        return rotate_right(n);             /* LL */
    }
    if (bf == -2) {
        if (balance_factor(n->right) > 0)
            n->right = rotate_right(n->right); /* RL */
        return rotate_left(n);                 /* RR */
    }
    return n;
}

/* ------------------------------------------------------------------ */
/*  node alloc / free                                                   */
/* ------------------------------------------------------------------ */

static AVLNode *node_new(void *key, void *value)
{
    AVLNode *n = malloc(sizeof(AVLNode));
    if (!n) return NULL;
    n->key    = key;
    n->value  = value;
    n->height = 1;
    n->left   = NULL;
    n->right  = NULL;
    return n;
}

static void destroy_rec(AVLNode *n)
{
    if (!n) return;
    destroy_rec(n->left);
    destroy_rec(n->right);
    free(n);
}

/* ------------------------------------------------------------------ */
/*  recursive insert                                                    */
/* ------------------------------------------------------------------ */

static AVLNode *insert_rec(AVLNode *n, void *key, void *value,
                            avl_cmp_fn cmp, int *inserted)
{
    if (!n) {
        *inserted = 1;
        return node_new(key, value);
    }
    int c = cmp(key, n->key);
    if (c < 0)
        n->left  = insert_rec(n->left,  key, value, cmp, inserted);
    else if (c > 0)
        n->right = insert_rec(n->right, key, value, cmp, inserted);
    else
        n->value = value; /* upsert */
    return rebalance(n);
}

/* ------------------------------------------------------------------ */
/*  recursive delete                                                    */
/* ------------------------------------------------------------------ */

static AVLNode *min_node(AVLNode *n)
{
    while (n->left) n = n->left;
    return n;
}

static AVLNode *delete_rec(AVLNode *n, const void *key,
                            avl_cmp_fn cmp, int *deleted)
{
    if (!n) return NULL;
    int c = cmp(key, n->key);
    if (c < 0) {
        n->left  = delete_rec(n->left,  key, cmp, deleted);
    } else if (c > 0) {
        n->right = delete_rec(n->right, key, cmp, deleted);
    } else {
        *deleted = 1;
        if (!n->left) {
            AVLNode *r = n->right;
            free(n);
            return r;
        }
        if (!n->right) {
            AVLNode *l = n->left;
            free(n);
            return l;
        }
        AVLNode *succ = min_node(n->right);
        n->key   = succ->key;
        n->value = succ->value;
        int dummy = 0;
        n->right = delete_rec(n->right, succ->key, cmp, &dummy);
    }
    return rebalance(n);
}

/* ------------------------------------------------------------------ */
/*  traversals                                                          */
/* ------------------------------------------------------------------ */

static void in_order_rec(const AVLNode *n, avl_visit_fn fn, void *ud)
{
    if (!n) return;
    in_order_rec(n->left, fn, ud);
    fn(n->key, n->value, ud);
    in_order_rec(n->right, fn, ud);
}

/* simple dynamic queue for level_order */
typedef struct {
    AVLNode **buf;
    size_t    head, tail, cap;
} NodeQueue;

static int nq_push(NodeQueue *q, AVLNode *n)
{
    if (q->tail - q->head == q->cap) {
        size_t nc = q->cap ? q->cap * 2 : 16;
        AVLNode **nb = realloc(q->buf, nc * sizeof(AVLNode *));
        if (!nb) return 0;
        /* compact */
        size_t len = q->tail - q->head;
        for (size_t i = 0; i < len; i++)
            nb[i] = nb[(q->head + i) % q->cap];
        q->buf  = nb;
        q->head = 0;
        q->tail = len;
        q->cap  = nc;
    }
    q->buf[q->tail++ % q->cap] = n;
    return 1;
}

static AVLNode *nq_pop(NodeQueue *q)
{
    if (q->head == q->tail) return NULL;
    return q->buf[q->head++ % q->cap];
}

/* ------------------------------------------------------------------ */
/*  invariant helpers                                                   */
/* ------------------------------------------------------------------ */

static int is_balanced_rec(const AVLNode *n)
{
    if (!n) return 1;
    int bf = balance_factor(n);
    if (bf < -1 || bf > 1) return 0;
    return is_balanced_rec(n->left) && is_balanced_rec(n->right);
}

static int is_valid_bst_rec(const AVLNode *n, const void *lo, const void *hi,
                              avl_cmp_fn cmp)
{
    if (!n) return 1;
    if (lo && cmp(n->key, lo) <= 0) return 0;
    if (hi && cmp(n->key, hi) >= 0) return 0;
    return is_valid_bst_rec(n->left,  lo,    n->key, cmp) &&
           is_valid_bst_rec(n->right, n->key, hi,    cmp);
}

/* ================================================================== */
/*  Public API                                                          */
/* ================================================================== */

AVLError avl_new(AVL *t, avl_cmp_fn cmp)
{
    t->root = NULL;
    t->len  = 0;
    t->cmp  = cmp;
    return AVL_OK;
}

void avl_destroy(AVL *t)
{
    destroy_rec(t->root);
    t->root = NULL;
    t->len  = 0;
}

AVLError avl_insert(AVL *t, void *key, void *value)
{
    int inserted = 0;
    t->root = insert_rec(t->root, key, value, t->cmp, &inserted);
    if (!t->root && inserted) return AVL_ERR_ALLOC; /* alloc failed */
    if (inserted) t->len++;
    return AVL_OK;
}

AVLError avl_search(const AVL *t, const void *key, void **value_out)
{
    AVLNode *n = t->root;
    while (n) {
        int c = t->cmp(key, n->key);
        if      (c < 0) n = n->left;
        else if (c > 0) n = n->right;
        else { *value_out = n->value; return AVL_OK; }
    }
    return AVL_ERR_NOT_FOUND;
}

AVLError avl_delete(AVL *t, const void *key)
{
    int deleted = 0;
    t->root = delete_rec(t->root, key, t->cmp, &deleted);
    if (!deleted) return AVL_ERR_NOT_FOUND;
    t->len--;
    return AVL_OK;
}

int avl_contains(const AVL *t, const void *key)
{
    void *v;
    return avl_search(t, key, &v) == AVL_OK;
}

AVLError avl_min(const AVL *t, void **key_out, void **value_out)
{
    if (!t->root) return AVL_ERR_EMPTY;
    AVLNode *n = t->root;
    while (n->left) n = n->left;
    *key_out   = n->key;
    *value_out = n->value;
    return AVL_OK;
}

AVLError avl_max(const AVL *t, void **key_out, void **value_out)
{
    if (!t->root) return AVL_ERR_EMPTY;
    AVLNode *n = t->root;
    while (n->right) n = n->right;
    *key_out   = n->key;
    *value_out = n->value;
    return AVL_OK;
}

void avl_in_order(const AVL *t, avl_visit_fn fn, void *user_data)
{
    in_order_rec(t->root, fn, user_data);
}

void avl_level_order(const AVL *t, avl_visit_fn fn, void *user_data)
{
    if (!t->root) return;
    NodeQueue q = {NULL, 0, 0, 0};
    nq_push(&q, t->root);
    AVLNode *n;
    while ((n = nq_pop(&q))) {
        fn(n->key, n->value, user_data);
        if (n->left)  nq_push(&q, n->left);
        if (n->right) nq_push(&q, n->right);
    }
    free(q.buf);
}

size_t avl_len(const AVL *t)     { return t->len; }
size_t avl_height(const AVL *t)  { return (size_t)node_height(t->root); }
int    avl_is_empty(const AVL *t){ return t->len == 0; }

int avl_is_balanced(const AVL *t)
{
    return is_balanced_rec(t->root);
}

int avl_is_valid_bst(const AVL *t)
{
    return is_valid_bst_rec(t->root, NULL, NULL, t->cmp);
}

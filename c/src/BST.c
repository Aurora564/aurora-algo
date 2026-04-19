#include "BST.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  内部工具                                                            */
/* ------------------------------------------------------------------ */

static BSTNode *node_new(void *key, void *value)
{
    BSTNode *n = malloc(sizeof(BSTNode));
    if (!n) return NULL;
    n->key   = key;
    n->value = value;
    n->left  = NULL;
    n->right = NULL;
    return n;
}

static BSTNode *min_node(BSTNode *n)
{
    while (n->left) n = n->left;
    return n;
}

static BSTNode *max_node(BSTNode *n)
{
    while (n->right) n = n->right;
    return n;
}

static void destroy_rec(BSTNode *n)
{
    if (!n) return;
    destroy_rec(n->left);
    destroy_rec(n->right);
    free(n);
}

static size_t height_rec(const BSTNode *n)
{
    if (!n) return 0;
    size_t l = height_rec(n->left);
    size_t r = height_rec(n->right);
    return (l > r ? l : r) + 1;
}

static int is_valid_rec(const BSTNode *n, const void *lo, const void *hi,
                         bst_cmp_fn cmp)
{
    if (!n) return 1;
    if (lo && cmp(n->key, lo) <= 0) return 0;
    if (hi && cmp(n->key, hi) >= 0) return 0;
    return is_valid_rec(n->left,  lo,    n->key, cmp) &&
           is_valid_rec(n->right, n->key, hi,    cmp);
}

/* ------------------------------------------------------------------ */
/*  递归插入：返回更新后的子树根                                         */
/* ------------------------------------------------------------------ */

static BSTNode *insert_rec(BSTNode *n, void *key, void *value,
                            bst_cmp_fn cmp, int *inserted)
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
        n->value = value;   /* upsert */
    return n;
}

/* ------------------------------------------------------------------ */
/*  递归删除：返回更新后的子树根，通过 *deleted 报告是否找到节点         */
/* ------------------------------------------------------------------ */

static BSTNode *delete_rec(BSTNode *n, const void *key,
                            bst_cmp_fn cmp, int *deleted)
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
            BSTNode *r = n->right;
            free(n);
            return r;
        }
        if (!n->right) {
            BSTNode *l = n->left;
            free(n);
            return l;
        }
        /* 双子节点：用中序后继替换 */
        BSTNode *succ = min_node(n->right);
        n->key   = succ->key;
        n->value = succ->value;
        int dummy = 0;
        n->right = delete_rec(n->right, succ->key, cmp, &dummy);
    }
    return n;
}

/* ------------------------------------------------------------------ */
/*  遍历辅助                                                            */
/* ------------------------------------------------------------------ */

static void in_order_rec(const BSTNode *n, bst_visit_fn fn, void *ud)
{
    if (!n) return;
    in_order_rec(n->left, fn, ud);
    fn(n->key, n->value, ud);
    in_order_rec(n->right, fn, ud);
}

static void pre_order_rec(const BSTNode *n, bst_visit_fn fn, void *ud)
{
    if (!n) return;
    fn(n->key, n->value, ud);
    pre_order_rec(n->left,  fn, ud);
    pre_order_rec(n->right, fn, ud);
}

static void post_order_rec(const BSTNode *n, bst_visit_fn fn, void *ud)
{
    if (!n) return;
    post_order_rec(n->left,  fn, ud);
    post_order_rec(n->right, fn, ud);
    fn(n->key, n->value, ud);
}

/* ------------------------------------------------------------------ */
/*  生命周期                                                            */
/* ------------------------------------------------------------------ */

BSTError bst_new(BST *t, bst_cmp_fn cmp)
{
    if (!t || !cmp) return BST_ERR_ALLOC;
    t->root = NULL;
    t->len  = 0;
    t->cmp  = cmp;
    return BST_OK;
}

void bst_destroy(BST *t)
{
    if (!t) return;
    destroy_rec(t->root);
    t->root = NULL;
    t->len  = 0;
}

/* ------------------------------------------------------------------ */
/*  核心操作                                                            */
/* ------------------------------------------------------------------ */

BSTError bst_insert(BST *t, void *key, void *value)
{
    int inserted = 0;
    BSTNode *new_root = insert_rec(t->root, key, value, t->cmp, &inserted);
    if (!new_root && !t->root) return BST_ERR_ALLOC;  /* malloc 失败 */
    t->root = new_root;
    if (inserted) t->len++;
    return BST_OK;
}

BSTError bst_search(const BST *t, const void *key, void **value_out)
{
    const BSTNode *n = t->root;
    while (n) {
        int c = t->cmp(key, n->key);
        if      (c < 0) n = n->left;
        else if (c > 0) n = n->right;
        else {
            if (value_out) *value_out = n->value;
            return BST_OK;
        }
    }
    return BST_ERR_NOT_FOUND;
}

BSTError bst_delete(BST *t, const void *key)
{
    int deleted = 0;
    t->root = delete_rec(t->root, key, t->cmp, &deleted);
    if (!deleted) return BST_ERR_NOT_FOUND;
    t->len--;
    return BST_OK;
}

int bst_contains(const BST *t, const void *key)
{
    return bst_search(t, key, NULL) == BST_OK;
}

BSTError bst_min(const BST *t, void **key_out, void **value_out)
{
    if (!t->root) return BST_ERR_EMPTY;
    BSTNode *n = min_node(t->root);
    if (key_out)   *key_out   = n->key;
    if (value_out) *value_out = n->value;
    return BST_OK;
}

BSTError bst_max(const BST *t, void **key_out, void **value_out)
{
    if (!t->root) return BST_ERR_EMPTY;
    BSTNode *n = max_node(t->root);
    if (key_out)   *key_out   = n->key;
    if (value_out) *value_out = n->value;
    return BST_OK;
}

BSTError bst_successor(const BST *t, const void *key,
                        void **key_out, void **value_out)
{
    BSTNode *succ = NULL;
    BSTNode *n    = t->root;
    while (n) {
        int c = t->cmp(key, n->key);
        if (c < 0) {
            succ = n;
            n    = n->left;
        } else if (c > 0) {
            n = n->right;
        } else {
            if (n->right) {
                BSTNode *s = min_node(n->right);
                if (key_out)   *key_out   = s->key;
                if (value_out) *value_out = s->value;
                return BST_OK;
            }
            if (succ) {
                if (key_out)   *key_out   = succ->key;
                if (value_out) *value_out = succ->value;
                return BST_OK;
            }
            return BST_ERR_NOT_FOUND;
        }
    }
    return BST_ERR_NOT_FOUND;
}

BSTError bst_predecessor(const BST *t, const void *key,
                          void **key_out, void **value_out)
{
    BSTNode *pred = NULL;
    BSTNode *n    = t->root;
    while (n) {
        int c = t->cmp(key, n->key);
        if (c > 0) {
            pred = n;
            n    = n->right;
        } else if (c < 0) {
            n = n->left;
        } else {
            if (n->left) {
                BSTNode *p = max_node(n->left);
                if (key_out)   *key_out   = p->key;
                if (value_out) *value_out = p->value;
                return BST_OK;
            }
            if (pred) {
                if (key_out)   *key_out   = pred->key;
                if (value_out) *value_out = pred->value;
                return BST_OK;
            }
            return BST_ERR_NOT_FOUND;
        }
    }
    return BST_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/*  遍历                                                                */
/* ------------------------------------------------------------------ */

void bst_in_order(const BST *t, bst_visit_fn fn, void *user_data)
{
    in_order_rec(t->root, fn, user_data);
}

void bst_pre_order(const BST *t, bst_visit_fn fn, void *user_data)
{
    pre_order_rec(t->root, fn, user_data);
}

void bst_post_order(const BST *t, bst_visit_fn fn, void *user_data)
{
    post_order_rec(t->root, fn, user_data);
}

void bst_level_order(const BST *t, bst_visit_fn fn, void *user_data)
{
    if (!t->root) return;

    /* 简单动态数组队列 */
    size_t cap  = 64;
    size_t head = 0, tail = 0;
    BSTNode **q = malloc(cap * sizeof(BSTNode *));
    if (!q) return;

    q[tail++] = t->root;
    while (head < tail) {
        if (tail == cap) {
            /* 循环使用：把未处理元素移到数组头 */
            size_t cnt = tail - head;
            memmove(q, q + head, cnt * sizeof(BSTNode *));
            head = 0;
            tail = cnt;
        }
        BSTNode *n = q[head++];
        fn(n->key, n->value, user_data);
        if (n->left) {
            if (tail >= cap) {
                cap *= 2;
                BSTNode **tmp = realloc(q, cap * sizeof(BSTNode *));
                if (!tmp) { free(q); return; }
                q = tmp;
            }
            q[tail++] = n->left;
        }
        if (n->right) {
            if (tail >= cap) {
                cap *= 2;
                BSTNode **tmp = realloc(q, cap * sizeof(BSTNode *));
                if (!tmp) { free(q); return; }
                q = tmp;
            }
            q[tail++] = n->right;
        }
    }
    free(q);
}

/* ------------------------------------------------------------------ */
/*  统计                                                                */
/* ------------------------------------------------------------------ */

size_t bst_len(const BST *t)      { return t->len; }
int    bst_is_empty(const BST *t) { return t->len == 0; }
size_t bst_height(const BST *t)   { return height_rec(t->root); }

int bst_is_valid(const BST *t)
{
    return is_valid_rec(t->root, NULL, NULL, t->cmp);
}

/* ------------------------------------------------------------------ */
/*  错误信息                                                            */
/* ------------------------------------------------------------------ */

const char *bst_strerror(BSTError err)
{
    switch (err) {
    case BST_OK:            return "ok";
    case BST_ERR_NOT_FOUND: return "key not found";
    case BST_ERR_ALLOC:     return "allocation failed";
    case BST_ERR_EMPTY:     return "tree is empty";
    default:                return "unknown error";
    }
}

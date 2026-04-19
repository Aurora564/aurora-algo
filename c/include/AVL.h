/**
 * AVL.h — Self-balancing AVL Tree (C99)
 *
 * 设计：
 *  - 键和值均为 void *，树不拥有键/值内存，调用者负责其生命周期。
 *  - 比较函数 avl_cmp_fn 在 avl_new 时注入，不可后期修改。
 *  - 重复插入同一键视为 upsert（更新 value），len 不变。
 *  - 删除采用中序后继替换（双子节点情形）。
 *  - 每个节点缓存 int8_t height，空节点高度为 0。
 */

#ifndef AVL_H
#define AVL_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  函数指针类型                                                        */
/* ------------------------------------------------------------------ */

typedef int  (*avl_cmp_fn)(const void *a, const void *b);
typedef void (*avl_visit_fn)(const void *key, void *value, void *user_data);

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    AVL_OK            = 0,
    AVL_ERR_NOT_FOUND,
    AVL_ERR_ALLOC,
    AVL_ERR_EMPTY,
} AVLError;

/* ------------------------------------------------------------------ */
/*  核心数据结构                                                        */
/* ------------------------------------------------------------------ */

typedef struct AVLNode {
    void           *key;
    void           *value;
    int8_t          height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

typedef struct {
    AVLNode    *root;
    size_t      len;
    avl_cmp_fn  cmp;
} AVL;

/* =====================================================================
 * API
 * ==================================================================== */

/* 生命周期 */
AVLError avl_new(AVL *t, avl_cmp_fn cmp);
void     avl_destroy(AVL *t);

/* 核心操作 */
AVLError avl_insert(AVL *t, void *key, void *value);
AVLError avl_search(const AVL *t, const void *key, void **value_out);
AVLError avl_delete(AVL *t, const void *key);
int      avl_contains(const AVL *t, const void *key);
AVLError avl_min(const AVL *t, void **key_out, void **value_out);
AVLError avl_max(const AVL *t, void **key_out, void **value_out);

/* 遍历 */
void avl_in_order(const AVL *t, avl_visit_fn fn, void *user_data);
void avl_level_order(const AVL *t, avl_visit_fn fn, void *user_data);

/* 统计 */
size_t avl_len(const AVL *t);
size_t avl_height(const AVL *t);
int    avl_is_empty(const AVL *t);

/* 测试辅助 */
int avl_is_balanced(const AVL *t);
int avl_is_valid_bst(const AVL *t);

#endif /* AVL_H */

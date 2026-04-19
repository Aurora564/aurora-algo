/**
 * BST.h — Binary Search Tree (C99)
 *
 * 设计：
 *  - 键和值均为 void *，树不拥有键/值内存，调用者负责其生命周期。
 *  - 比较函数 bst_cmp_fn 在 bst_new 时注入，不可后期修改。
 *  - 重复插入同一键视为 upsert（更新 value），len 不变。
 *  - 删除采用中序后继替换（Case 3 双子节点）。
 */

#ifndef BST_H
#define BST_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  函数指针类型                                                        */
/* ------------------------------------------------------------------ */

/** 比较函数：返回负数 / 0 / 正数（与 strcmp 语义一致）*/
typedef int  (*bst_cmp_fn)(const void *a, const void *b);

/** 遍历回调：(key, value, user_data) */
typedef void (*bst_visit_fn)(const void *key, void *value, void *user_data);

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    BST_OK            = 0,
    BST_ERR_NOT_FOUND,   /* search / delete 键不存在 */
    BST_ERR_ALLOC,       /* malloc 失败              */
    BST_ERR_EMPTY,       /* 对空树调用 min / max      */
} BSTError;

/* ------------------------------------------------------------------ */
/*  核心数据结构                                                        */
/* ------------------------------------------------------------------ */

typedef struct BSTNode {
    void           *key;
    void           *value;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

typedef struct {
    BSTNode    *root;
    size_t      len;
    bst_cmp_fn  cmp;
} BST;

/* =====================================================================
 * API
 * ==================================================================== */

/* 生命周期 */
BSTError bst_new(BST *t, bst_cmp_fn cmp);
void     bst_destroy(BST *t);

/* 核心操作 */
BSTError bst_insert(BST *t, void *key, void *value);
BSTError bst_search(const BST *t, const void *key, void **value_out);
BSTError bst_delete(BST *t, const void *key);
int      bst_contains(const BST *t, const void *key);
BSTError bst_min(const BST *t, void **key_out, void **value_out);
BSTError bst_max(const BST *t, void **key_out, void **value_out);
BSTError bst_successor(const BST *t, const void *key,
                        void **key_out, void **value_out);
BSTError bst_predecessor(const BST *t, const void *key,
                          void **key_out, void **value_out);

/* 遍历 */
void bst_in_order(const BST *t, bst_visit_fn fn, void *user_data);
void bst_pre_order(const BST *t, bst_visit_fn fn, void *user_data);
void bst_post_order(const BST *t, bst_visit_fn fn, void *user_data);
void bst_level_order(const BST *t, bst_visit_fn fn, void *user_data);

/* 统计 */
size_t bst_len(const BST *t);
size_t bst_height(const BST *t);
int    bst_is_empty(const BST *t);
int    bst_is_valid(const BST *t);   /* 测试辅助：验证 BST 性质 */

/* 错误信息 */
const char *bst_strerror(BSTError err);

#endif /* BST_H */

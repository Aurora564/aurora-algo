/**
 * test_avl.c — C AVL 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_avl tests/test_avl.c src/AVL.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_avl
 */

#include "AVL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  测试工具                                                            */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                     \
    do {                                                                \
        if (expr) {                                                     \
            printf("  PASS  %s\n", #expr);                             \
            g_pass++;                                                   \
        } else {                                                        \
            printf("  FAIL  %s  (%s:%d)\n", #expr, __FILE__, __LINE__);\
            g_fail++;                                                   \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

/* ------------------------------------------------------------------ */
/*  辅助                                                                */
/* ------------------------------------------------------------------ */

static int int_cmp(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

typedef struct { int buf[512]; int n; } IntBuf;

static void collect_key(const void *key, void *value, void *ud)
{
    (void)value;
    IntBuf *ib = (IntBuf *)ud;
    ib->buf[ib->n++] = *(const int *)key;
}

static int is_sorted_strict(const int *arr, int n)
{
    for (int i = 1; i < n; i++)
        if (arr[i] <= arr[i-1]) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  TC-01  空树 search → NOT_FOUND                                      */
/* ------------------------------------------------------------------ */
static void tc01_empty_search(void)
{
    puts("\n[TC-01] empty search");
    AVL t;
    avl_new(&t, int_cmp);
    void *v = NULL;
    CHECK(avl_search(&t, &(int){1}, &v) == AVL_ERR_NOT_FOUND);
    CHECK(avl_is_empty(&t));
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-02  insert 5 无序键，in_order 严格升序                           */
/* ------------------------------------------------------------------ */
static void tc02_in_order_ascending(void)
{
    puts("\n[TC-02] in_order ascending");
    AVL t;
    avl_new(&t, int_cmp);
    int keys[] = {3, 1, 4, 5, 2};
    for (int i = 0; i < 5; i++)
        avl_insert(&t, &keys[i], &keys[i]);

    IntBuf ib = {{0}, 0};
    avl_in_order(&t, collect_key, &ib);
    CHECK_EQ(ib.n, 5);
    CHECK(is_sorted_strict(ib.buf, ib.n));
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-03  search 存在键                                                */
/* ------------------------------------------------------------------ */
static void tc03_search_found(void)
{
    puts("\n[TC-03] search found");
    AVL t;
    avl_new(&t, int_cmp);
    int k = 42, v = 99;
    avl_insert(&t, &k, &v);
    void *out = NULL;
    CHECK(avl_search(&t, &k, &out) == AVL_OK);
    CHECK_EQ(*(int *)out, 99);
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-04  search 不存在键                                              */
/* ------------------------------------------------------------------ */
static void tc04_search_missing(void)
{
    puts("\n[TC-04] search missing");
    AVL t;
    avl_new(&t, int_cmp);
    int k = 1, v = 1;
    avl_insert(&t, &k, &v);
    void *out = NULL;
    CHECK(avl_search(&t, &(int){99}, &out) == AVL_ERR_NOT_FOUND);
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-05  delete 叶节点                                                */
/* ------------------------------------------------------------------ */
static void tc05_delete_leaf(void)
{
    puts("\n[TC-05] delete leaf");
    AVL t;
    avl_new(&t, int_cmp);
    int ks[] = {5, 3, 7};
    for (int i = 0; i < 3; i++) avl_insert(&t, &ks[i], &ks[i]);
    CHECK(avl_delete(&t, &(int){3}) == AVL_OK);
    CHECK_EQ(avl_len(&t), 2u);
    CHECK(avl_is_balanced(&t));
    CHECK(avl_is_valid_bst(&t));
    CHECK(!avl_contains(&t, &(int){3}));
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-06  delete 单子节点                                              */
/* ------------------------------------------------------------------ */
static void tc06_delete_one_child(void)
{
    puts("\n[TC-06] delete one child");
    AVL t;
    avl_new(&t, int_cmp);
    int ks[] = {5, 3, 7, 6};
    for (int i = 0; i < 4; i++) avl_insert(&t, &ks[i], &ks[i]);
    CHECK(avl_delete(&t, &(int){7}) == AVL_OK);
    CHECK_EQ(avl_len(&t), 3u);
    CHECK(avl_is_balanced(&t));
    CHECK(avl_is_valid_bst(&t));
    CHECK(!avl_contains(&t, &(int){7}));
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-07  delete 双子节点                                              */
/* ------------------------------------------------------------------ */
static void tc07_delete_two_children(void)
{
    puts("\n[TC-07] delete two children");
    AVL t;
    avl_new(&t, int_cmp);
    int ks[] = {5, 3, 7, 1, 4, 6, 9};
    for (int i = 0; i < 7; i++) avl_insert(&t, &ks[i], &ks[i]);
    CHECK(avl_delete(&t, &(int){5}) == AVL_OK);
    CHECK_EQ(avl_len(&t), 6u);
    CHECK(avl_is_balanced(&t));
    CHECK(avl_is_valid_bst(&t));

    IntBuf ib = {{0}, 0};
    avl_in_order(&t, collect_key, &ib);
    int want[] = {1, 3, 4, 6, 7, 9};
    CHECK_EQ(ib.n, 6);
    CHECK(memcmp(ib.buf, want, 6 * sizeof(int)) == 0);
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-08  delete 根节点                                                */
/* ------------------------------------------------------------------ */
static void tc08_delete_root(void)
{
    puts("\n[TC-08] delete root");
    /* single node */
    {
        AVL t; avl_new(&t, int_cmp);
        int k = 1, v = 1;
        avl_insert(&t, &k, &v);
        avl_delete(&t, &k);
        CHECK(avl_is_empty(&t));
        avl_destroy(&t);
    }
    /* left child only */
    {
        AVL t; avl_new(&t, int_cmp);
        int k5 = 5, k3 = 3;
        avl_insert(&t, &k5, &k5); avl_insert(&t, &k3, &k3);
        avl_delete(&t, &k5);
        CHECK_EQ(avl_len(&t), 1u);
        CHECK(avl_is_balanced(&t));
        avl_destroy(&t);
    }
    /* two children */
    {
        AVL t; avl_new(&t, int_cmp);
        int ks[] = {5, 2, 8};
        for (int i = 0; i < 3; i++) avl_insert(&t, &ks[i], &ks[i]);
        avl_delete(&t, &(int){5});
        CHECK_EQ(avl_len(&t), 2u);
        CHECK(avl_is_balanced(&t));
        CHECK(avl_is_valid_bst(&t));
        avl_destroy(&t);
    }
}

/* ------------------------------------------------------------------ */
/*  TC-09  重复 insert → len 不变，value 更新                           */
/* ------------------------------------------------------------------ */
static void tc09_duplicate_insert(void)
{
    puts("\n[TC-09] duplicate insert");
    AVL t;
    avl_new(&t, int_cmp);
    int k = 1, v1 = 10, v2 = 20;
    avl_insert(&t, &k, &v1);
    avl_insert(&t, &k, &v2);
    CHECK_EQ(avl_len(&t), 1u);
    void *out = NULL;
    avl_search(&t, &k, &out);
    CHECK_EQ(*(int *)out, 20);
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-10  min / max                                                    */
/* ------------------------------------------------------------------ */
static void tc10_min_max(void)
{
    puts("\n[TC-10] min / max");
    AVL t;
    avl_new(&t, int_cmp);
    int ks[] = {5, 3, 7, 1, 9};
    for (int i = 0; i < 5; i++) avl_insert(&t, &ks[i], &ks[i]);

    void *ko, *vo;
    avl_min(&t, &ko, &vo);
    CHECK_EQ(*(int *)ko, 1);
    avl_max(&t, &ko, &vo);
    CHECK_EQ(*(int *)ko, 9);

    AVL empty; avl_new(&empty, int_cmp);
    CHECK(avl_min(&empty, &ko, &vo) == AVL_ERR_EMPTY);
    CHECK(avl_max(&empty, &ko, &vo) == AVL_ERR_EMPTY);
    avl_destroy(&t);
    avl_destroy(&empty);
}

/* ------------------------------------------------------------------ */
/*  TC-11  顺序插入 1..500，height ≤ 22，树保持平衡                     */
/* ------------------------------------------------------------------ */
static void tc11_sequential_insert(void)
{
    puts("\n[TC-11] sequential insert stays balanced");
    AVL t;
    avl_new(&t, int_cmp);
    static int ks[500];
    for (int i = 0; i < 500; i++) ks[i] = i + 1;
    for (int i = 0; i < 500; i++) avl_insert(&t, &ks[i], &ks[i]);

    CHECK_EQ(avl_len(&t), 500u);
    CHECK(avl_height(&t) <= 22u);
    CHECK(avl_is_balanced(&t));
    CHECK(avl_is_valid_bst(&t));
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-12  混合 insert/delete，不变式保持                               */
/* ------------------------------------------------------------------ */
static void tc12_mixed_ops(void)
{
    puts("\n[TC-12] mixed insert/delete invariants");
    AVL t;
    avl_new(&t, int_cmp);
    int ins[] = {10, 5, 15, 3, 7, 12, 20, 1, 4, 6, 8, 11, 13, 18, 25};
    for (int i = 0; i < 15; i++) {
        avl_insert(&t, &ins[i], &ins[i]);
        CHECK(avl_is_balanced(&t));
        CHECK(avl_is_valid_bst(&t));
    }
    int del[] = {5, 15, 1, 20, 10};
    for (int i = 0; i < 5; i++) {
        avl_delete(&t, &del[i]);
        CHECK(avl_is_balanced(&t));
        CHECK(avl_is_valid_bst(&t));
    }
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-13  delete 不存在键 → NOT_FOUND，len 不变                        */
/* ------------------------------------------------------------------ */
static void tc13_delete_missing(void)
{
    puts("\n[TC-13] delete missing");
    AVL t;
    avl_new(&t, int_cmp);
    int k = 1, v = 1;
    avl_insert(&t, &k, &v);
    CHECK(avl_delete(&t, &(int){99}) == AVL_ERR_NOT_FOUND);
    CHECK_EQ(avl_len(&t), 1u);
    avl_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    tc01_empty_search();
    tc02_in_order_ascending();
    tc03_search_found();
    tc04_search_missing();
    tc05_delete_leaf();
    tc06_delete_one_child();
    tc07_delete_two_children();
    tc08_delete_root();
    tc09_duplicate_insert();
    tc10_min_max();
    tc11_sequential_insert();
    tc12_mixed_ops();
    tc13_delete_missing();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

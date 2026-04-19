/**
 * test_bst.c — C BST 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_bst tests/test_bst.c src/BST.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_bst
 *   gcc -fsanitize=address,undefined ... && ./tests/test_bst
 */

#include "BST.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  测试工具宏                                                          */
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
/*  辅助：int 比较函数                                                  */
/* ------------------------------------------------------------------ */

static int int_cmp(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

/* 遍历收集回调 */
typedef struct { int buf[256]; int n; } IntBuf;

static void collect_key(const void *key, void *value, void *ud)
{
    (void)value;
    IntBuf *ib = ud;
    ib->buf[ib->n++] = *(const int *)key;
}

/* 整数键数组 */
static int K[32];

static void init_keys(int *vals, int n)
{
    for (int i = 0; i < n; i++) K[i] = vals[i];
}

/* ------------------------------------------------------------------ */
/*  TC-01  空树 search → BST_ERR_NOT_FOUND                             */
/* ------------------------------------------------------------------ */

static void test_empty_search(void)
{
    puts("\n[TC-01] empty search");
    BST t;
    bst_new(&t, int_cmp);
    int k = 1;
    CHECK(bst_search(&t, &k, NULL) == BST_ERR_NOT_FOUND);
    CHECK(bst_is_empty(&t));
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-02  insert 5 无序键，in_order 严格升序                           */
/* ------------------------------------------------------------------ */

static void test_in_order_ascending(void)
{
    puts("\n[TC-02] in_order ascending");
    BST t;
    bst_new(&t, int_cmp);
    int vals[] = {3, 1, 4, 5, 2};
    init_keys(vals, 5);
    for (int i = 0; i < 5; i++) bst_insert(&t, &K[i], &K[i]);
    IntBuf ib = {.n = 0};
    bst_in_order(&t, collect_key, &ib);
    CHECK_EQ(ib.n, 5);
    int ok = 1;
    for (int i = 1; i < ib.n; i++) if (ib.buf[i] <= ib.buf[i-1]) { ok = 0; break; }
    CHECK(ok);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-03  search 存在键 → BST_OK + 正确 value                         */
/* ------------------------------------------------------------------ */

static void test_search_found(void)
{
    puts("\n[TC-03] search found");
    BST t;
    bst_new(&t, int_cmp);
    int k = 42, v = 99;
    bst_insert(&t, &k, &v);
    void *out = NULL;
    CHECK(bst_search(&t, &k, &out) == BST_OK);
    CHECK(out == &v);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-04  search 不存在键 → BST_ERR_NOT_FOUND                         */
/* ------------------------------------------------------------------ */

static void test_search_missing(void)
{
    puts("\n[TC-04] search missing");
    BST t;
    bst_new(&t, int_cmp);
    int k = 1;
    bst_insert(&t, &k, &k);
    int missing = 99;
    CHECK(bst_search(&t, &missing, NULL) == BST_ERR_NOT_FOUND);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-05  delete 叶节点                                                */
/* ------------------------------------------------------------------ */

static void test_delete_leaf(void)
{
    puts("\n[TC-05] delete leaf");
    BST t;
    bst_new(&t, int_cmp);
    int v[] = {5, 3, 7};
    for (int i = 0; i < 3; i++) bst_insert(&t, &v[i], &v[i]);
    CHECK(bst_delete(&t, &v[1]) == BST_OK);  /* delete 3 */
    CHECK_EQ(bst_len(&t), 2);
    CHECK(bst_is_valid(&t));
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-06  delete 单子节点                                              */
/* ------------------------------------------------------------------ */

static void test_delete_one_child(void)
{
    puts("\n[TC-06] delete one child");
    BST t;
    bst_new(&t, int_cmp);
    int v[] = {5, 3, 7, 6};
    for (int i = 0; i < 4; i++) bst_insert(&t, &v[i], &v[i]);
    int del = 7;
    CHECK(bst_delete(&t, &del) == BST_OK);
    CHECK_EQ(bst_len(&t), 3);
    CHECK(bst_is_valid(&t));
    CHECK(!bst_contains(&t, &del));
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-07  delete 双子节点                                              */
/* ------------------------------------------------------------------ */

static void test_delete_two_children(void)
{
    puts("\n[TC-07] delete two children");
    BST t;
    bst_new(&t, int_cmp);
    int v[] = {5, 3, 7, 1, 4, 6, 9};
    for (int i = 0; i < 7; i++) bst_insert(&t, &v[i], &v[i]);
    int del = 5;
    CHECK(bst_delete(&t, &del) == BST_OK);
    CHECK_EQ(bst_len(&t), 6);
    CHECK(bst_is_valid(&t));
    IntBuf ib = {.n = 0};
    bst_in_order(&t, collect_key, &ib);
    int want[] = {1, 3, 4, 6, 7, 9};
    int ok = (ib.n == 6);
    for (int i = 0; ok && i < 6; i++) ok = (ib.buf[i] == want[i]);
    CHECK(ok);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-08  delete 根节点                                                */
/* ------------------------------------------------------------------ */

static void test_delete_root(void)
{
    puts("\n[TC-08] delete root");
    /* 单节点 */
    {
        BST t; bst_new(&t, int_cmp);
        int k = 1; bst_insert(&t, &k, &k);
        bst_delete(&t, &k);
        CHECK(bst_is_empty(&t));
        bst_destroy(&t);
    }
    /* 只有左子 */
    {
        BST t; bst_new(&t, int_cmp);
        int a = 5, b = 3;
        bst_insert(&t, &a, &a); bst_insert(&t, &b, &b);
        bst_delete(&t, &a);
        CHECK_EQ(bst_len(&t), 1);
        CHECK(bst_is_valid(&t));
        bst_destroy(&t);
    }
    /* 双子 */
    {
        BST t; bst_new(&t, int_cmp);
        int v[] = {5, 2, 8};
        for (int i = 0; i < 3; i++) bst_insert(&t, &v[i], &v[i]);
        bst_delete(&t, &v[0]);
        CHECK_EQ(bst_len(&t), 2);
        CHECK(bst_is_valid(&t));
        bst_destroy(&t);
    }
}

/* ------------------------------------------------------------------ */
/*  TC-09  重复 insert → len 不变，value 更新                           */
/* ------------------------------------------------------------------ */

static void test_duplicate_insert(void)
{
    puts("\n[TC-09] duplicate insert");
    BST t; bst_new(&t, int_cmp);
    int k = 1, v1 = 10, v2 = 20;
    bst_insert(&t, &k, &v1);
    bst_insert(&t, &k, &v2);
    CHECK_EQ(bst_len(&t), 1);
    void *out = NULL;
    bst_search(&t, &k, &out);
    CHECK(out == &v2);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-10  min / max                                                    */
/* ------------------------------------------------------------------ */

static void test_min_max(void)
{
    puts("\n[TC-10] min / max");
    BST t; bst_new(&t, int_cmp);
    int v[] = {5, 3, 7, 1, 9};
    for (int i = 0; i < 5; i++) bst_insert(&t, &v[i], &v[i]);
    void *k = NULL;
    bst_min(&t, &k, NULL);
    CHECK(*(int *)k == 1);
    bst_max(&t, &k, NULL);
    CHECK(*(int *)k == 9);
    /* 空树 */
    BST empty; bst_new(&empty, int_cmp);
    CHECK(bst_min(&empty, NULL, NULL) == BST_ERR_EMPTY);
    bst_destroy(&t);
    bst_destroy(&empty);
}

/* ------------------------------------------------------------------ */
/*  TC-11  successor / predecessor 边界                                 */
/* ------------------------------------------------------------------ */

static void test_succ_pred(void)
{
    puts("\n[TC-11] successor / predecessor");
    BST t; bst_new(&t, int_cmp);
    int v[] = {2, 1, 3};
    for (int i = 0; i < 3; i++) bst_insert(&t, &v[i], &v[i]);
    /* successor of max */
    int max_k = 3;
    CHECK(bst_successor(&t, &max_k, NULL, NULL) == BST_ERR_NOT_FOUND);
    /* predecessor of min */
    int min_k = 1;
    CHECK(bst_predecessor(&t, &min_k, NULL, NULL) == BST_ERR_NOT_FOUND);
    /* normal */
    void *sk = NULL;
    int k1 = 1;
    bst_successor(&t, &k1, &sk, NULL);
    CHECK(*(int *)sk == 2);
    void *pk = NULL;
    int k3 = 3;
    bst_predecessor(&t, &k3, &pk, NULL);
    CHECK(*(int *)pk == 2);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-12  顺序插入 1..500，height 退化，中序仍正确                      */
/* ------------------------------------------------------------------ */

static int g_deg_prev = 0, g_deg_ok = 1;

static void check_ascending(const void *key, void *value, void *ud)
{
    (void)value; (void)ud;
    int cur = *(const int *)key;
    if (cur <= g_deg_prev) g_deg_ok = 0;
    g_deg_prev = cur;
}

static void test_degenerate(void)
{
    puts("\n[TC-12] degenerate (sequential insert)");
    BST t; bst_new(&t, int_cmp);
    static int keys[500];
    for (int i = 0; i < 500; i++) { keys[i] = i + 1; bst_insert(&t, &keys[i], &keys[i]); }
    CHECK(bst_height(&t) >= 250);
    CHECK_EQ(bst_len(&t), 500);
    g_deg_prev = 0; g_deg_ok = 1;
    bst_in_order(&t, check_ascending, NULL);
    CHECK(g_deg_ok);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  TC-14  level_order → BFS 顺序                                       */
/* ------------------------------------------------------------------ */

static void test_level_order(void)
{
    puts("\n[TC-14] level_order BFS");
    BST t; bst_new(&t, int_cmp);
    int v[] = {4, 2, 6, 1, 3, 5, 7};
    for (int i = 0; i < 7; i++) bst_insert(&t, &v[i], &v[i]);
    IntBuf ib = {.n = 0};
    bst_level_order(&t, collect_key, &ib);
    int want[] = {4, 2, 6, 1, 3, 5, 7};
    int ok = (ib.n == 7);
    for (int i = 0; ok && i < 7; i++) ok = (ib.buf[i] == want[i]);
    CHECK(ok);
    bst_destroy(&t);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    test_empty_search();
    test_in_order_ascending();
    test_search_found();
    test_search_missing();
    test_delete_leaf();
    test_delete_one_child();
    test_delete_two_children();
    test_delete_root();
    test_duplicate_insert();
    test_min_max();
    test_succ_pred();
    test_degenerate();
    test_level_order();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

/**
 * test_slist.c — C SList 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_slist tests/test_slist.c src/SList.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_slist
 *   gcc -fsanitize=address,undefined ... && ./tests/test_slist
 */

#include "SList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/*  测试工具宏                                                          */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                    \
    do {                                                               \
        if (expr) {                                                    \
            printf("  PASS  %s\n", #expr);                            \
            g_pass++;                                                  \
        } else {                                                       \
            printf("  FAIL  %s  (%s:%d)\n", #expr, __FILE__, __LINE__);\
            g_fail++;                                                  \
        }                                                              \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

/* ------------------------------------------------------------------ */
/*  托管模式辅助                                                        */
/* ------------------------------------------------------------------ */

static int g_free_count = 0;

static void int_free(void *p)
{
    free(p);
    g_free_count++;
}

static void *int_clone(const void *p)
{
    int *dst = malloc(sizeof(int));
    if (!dst) return NULL;
    *dst = *(const int *)p;
    return dst;
}

static int *make_int(int v)
{
    int *p = malloc(sizeof(int));
    *p = v;
    return p;
}

/* ------------------------------------------------------------------ */
/*  TC-01  push_back N 个元素，pop_front 验证 FIFO 顺序                */
/* ------------------------------------------------------------------ */
static void tc01_fifo(void)
{
    puts("TC-01: push_back + pop_front FIFO");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[5] = {10, 20, 30, 40, 50};
    for (int i = 0; i < 5; i++)
        CHECK_EQ(slist_push_back(&list, &vals[i]), SL_OK);

    CHECK_EQ(slist_len(&list), 5u);

    for (int i = 0; i < 5; i++) {
        void *out = NULL;
        CHECK_EQ(slist_pop_front(&list, &out), SL_OK);
        CHECK_EQ(*(int *)out, vals[i]);
    }
    CHECK_EQ(slist_len(&list), 0u);
    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-02  push_front N 个元素，pop_front 验证 LIFO 顺序               */
/* ------------------------------------------------------------------ */
static void tc02_lifo(void)
{
    puts("TC-02: push_front + pop_front LIFO");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        CHECK_EQ(slist_push_front(&list, &vals[i]), SL_OK);

    /* push_front 顺序：1,2,3,4,5 → 链表：5→4→3→2→1 */
    for (int i = 4; i >= 0; i--) {
        void *out = NULL;
        CHECK_EQ(slist_pop_front(&list, &out), SL_OK);
        CHECK_EQ(*(int *)out, vals[i]);
    }
    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-03  pop_front / pop_back 空链表返回 SL_ERR_EMPTY                */
/* ------------------------------------------------------------------ */
static void tc03_pop_empty(void)
{
    puts("TC-03: pop empty");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    void *out = NULL;
    CHECK_EQ(slist_pop_front(&list, &out), SL_ERR_EMPTY);
    CHECK_EQ(slist_pop_back(&list, &out),  SL_ERR_EMPTY);
    CHECK_EQ(slist_peek_front(&list, &out), SL_ERR_EMPTY);
    CHECK_EQ(slist_peek_back(&list, &out),  SL_ERR_EMPTY);
    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-04  get / peek 边界                                             */
/* ------------------------------------------------------------------ */
static void tc04_get_peek(void)
{
    puts("TC-04: get & peek bounds");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int a = 10, b = 20, c = 30;
    slist_push_back(&list, &a);
    slist_push_back(&list, &b);
    slist_push_back(&list, &c);

    void *out = NULL;
    CHECK_EQ(slist_get(&list, 0, &out), SL_OK);
    CHECK_EQ(*(int *)out, 10);

    CHECK_EQ(slist_get(&list, 2, &out), SL_OK);
    CHECK_EQ(*(int *)out, 30);

    CHECK_EQ(slist_get(&list, 3, &out), SL_ERR_OOB);

    CHECK_EQ(slist_peek_front(&list, &out), SL_OK);
    CHECK_EQ(*(int *)out, 10);

    CHECK_EQ(slist_peek_back(&list, &out), SL_OK);
    CHECK_EQ(*(int *)out, 30);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-05  insert_after: 在任意节点后插入                              */
/* ------------------------------------------------------------------ */
static void tc05_insert_after(void)
{
    puts("TC-05: insert_after");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int a = 1, b = 3, ins = 2;
    slist_push_back(&list, &a);
    slist_push_back(&list, &b);     /* [1, 3] */

    /* 在第一个节点（val=1）后插入 2 → [1, 2, 3] */
    SLNode *first = list.dummy->next;
    CHECK_EQ(slist_insert_after(&list, first, &ins), SL_OK);
    CHECK_EQ(slist_len(&list), 3u);

    void *out = NULL;
    slist_get(&list, 1, &out);
    CHECK_EQ(*(int *)out, 2);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-06  insert_after(dummy) 等价于 push_front                      */
/* ------------------------------------------------------------------ */
static void tc06_insert_after_dummy(void)
{
    puts("TC-06: insert_after(dummy) == push_front");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int a = 10, b = 20;
    slist_push_back(&list, &a);
    CHECK_EQ(slist_insert_after(&list, list.dummy, &b), SL_OK);  /* [20, 10] */

    void *out = NULL;
    slist_peek_front(&list, &out);
    CHECK_EQ(*(int *)out, 20);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-07  remove_after: 移除指定节点的后继                            */
/* ------------------------------------------------------------------ */
static void tc07_remove_after(void)
{
    puts("TC-07: remove_after");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[4] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++)
        slist_push_back(&list, &vals[i]);   /* [1, 2, 3, 4] */

    /* 移除 dummy 之后（即 pop_front）→ [2, 3, 4] */
    void *out = NULL;
    CHECK_EQ(slist_remove_after(&list, list.dummy, &out), SL_OK);
    CHECK_EQ(*(int *)out, 1);
    CHECK_EQ(slist_len(&list), 3u);

    /* 移除第一个节点之后 → [2, 4] */
    SLNode *first = list.dummy->next;
    CHECK_EQ(slist_remove_after(&list, first, &out), SL_OK);
    CHECK_EQ(*(int *)out, 3);
    CHECK_EQ(slist_len(&list), 2u);

    /* 移除尾节点的后继（无后继）→ SL_ERR_OOB */
    CHECK_EQ(slist_remove_after(&list, list.tail, &out), SL_ERR_OOB);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-08  reverse 后顺序正确                                          */
/* ------------------------------------------------------------------ */
static void tc08_reverse(void)
{
    puts("TC-08: reverse");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        slist_push_back(&list, &vals[i]);   /* [1,2,3,4,5] */

    slist_reverse(&list);                   /* [5,4,3,2,1] */

    void *out = NULL;
    for (int i = 0; i < 5; i++) {
        slist_get(&list, (size_t)i, &out);
        CHECK_EQ(*(int *)out, vals[4 - i]);
    }

    /* tail 应该指向原头节点（val=1）*/
    slist_peek_back(&list, &out);
    CHECK_EQ(*(int *)out, 1);

    /* 单节点 reverse 不崩溃 */
    SList single;
    CHECK_EQ(slist_new(&single), SL_OK);
    int x = 42;
    slist_push_back(&single, &x);
    slist_reverse(&single);
    slist_peek_front(&single, &out);
    CHECK_EQ(*(int *)out, 42);
    slist_destroy(&single);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-09  find                                                        */
/* ------------------------------------------------------------------ */

static int find_30(const void *val) { return *(const int *)val == 30; }
static int find_99(const void *val) { return *(const int *)val == 99; }

static void tc09_find(void)
{
    puts("TC-09: find");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[3] = {10, 30, 50};
    for (int i = 0; i < 3; i++)
        slist_push_back(&list, &vals[i]);

    SLNode *found = slist_find(&list, find_30);
    CHECK(found != NULL);
    CHECK_EQ(*(int *)found->val, 30);

    SLNode *not_found = slist_find(&list, find_99);
    CHECK(not_found == NULL);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-10  托管模式 clear：free_fn 调用恰好 len 次                     */
/* ------------------------------------------------------------------ */
static void tc10_managed_clear(void)
{
    puts("TC-10: managed clear");
    SList list;
    CHECK_EQ(slist_new_managed(&list, int_free, int_clone), SL_OK);

    for (int i = 0; i < 5; i++)
        slist_push_back(&list, make_int(i));

    g_free_count = 0;
    slist_clear(&list);
    CHECK_EQ(g_free_count, 5);
    CHECK_EQ(slist_len(&list), 0u);
    CHECK(slist_is_empty(&list));

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-11  托管模式 destroy：free_fn 调用 len 次，dummy 被释放         */
/* ------------------------------------------------------------------ */
static void tc11_managed_destroy(void)
{
    puts("TC-11: managed destroy");
    SList list;
    CHECK_EQ(slist_new_managed(&list, int_free, int_clone), SL_OK);

    for (int i = 0; i < 3; i++)
        slist_push_back(&list, make_int(i));

    g_free_count = 0;
    slist_destroy(&list);
    CHECK_EQ(g_free_count, 3);
    CHECK(list.dummy == NULL);
}

/* ------------------------------------------------------------------ */
/*  TC-12  pop_back：O(n) 遍历，正确返回尾节点值                       */
/* ------------------------------------------------------------------ */
static void tc12_pop_back(void)
{
    puts("TC-12: pop_back");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[3] = {10, 20, 30};
    for (int i = 0; i < 3; i++)
        slist_push_back(&list, &vals[i]);   /* [10, 20, 30] */

    void *out = NULL;
    CHECK_EQ(slist_pop_back(&list, &out), SL_OK);
    CHECK_EQ(*(int *)out, 30);
    CHECK_EQ(slist_len(&list), 2u);

    CHECK_EQ(slist_pop_back(&list, &out), SL_OK);
    CHECK_EQ(*(int *)out, 20);

    CHECK_EQ(slist_pop_back(&list, &out), SL_OK);
    CHECK_EQ(*(int *)out, 10);

    CHECK_EQ(slist_len(&list), 0u);
    CHECK(list.tail == list.dummy);     /* tail 回到哨兵 */

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-13  clone 独立性：修改克隆不影响原链表                          */
/* ------------------------------------------------------------------ */
static void tc13_clone_independence(void)
{
    puts("TC-13: clone independence");
    SList src;
    CHECK_EQ(slist_new_managed(&src, int_free, int_clone), SL_OK);

    for (int i = 0; i < 3; i++)
        slist_push_back(&src, make_int(i * 10));    /* [0, 10, 20] */

    SList dst;
    CHECK_EQ(slist_clone(&dst, &src), SL_OK);

    /* 修改克隆链表中第一个节点的值 */
    int *cloned_first = (int *)dst.dummy->next->val;
    *cloned_first = 999;

    /* 原链表不受影响 */
    void *out = NULL;
    slist_get(&src, 0, &out);
    CHECK_EQ(*(int *)out, 0);

    slist_destroy(&src);
    slist_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/*  TC-14  copy 浅拷贝：两链表共享 val 指针（不双重释放）              */
/* ------------------------------------------------------------------ */
static void tc14_shallow_copy(void)
{
    puts("TC-14: shallow copy");
    SList src;
    CHECK_EQ(slist_new(&src), SL_OK);

    int a = 1, b = 2, c = 3;
    slist_push_back(&src, &a);
    slist_push_back(&src, &b);
    slist_push_back(&src, &c);

    SList dst;
    CHECK_EQ(slist_copy(&dst, &src), SL_OK);
    CHECK_EQ(slist_len(&dst), 3u);

    /* dst 和 src 的 val 指针相同（浅拷贝）*/
    void *src_out = NULL, *dst_out = NULL;
    slist_get(&src, 1, &src_out);
    slist_get(&dst, 1, &dst_out);
    CHECK(src_out == dst_out);  /* 同一个指针 */

    slist_destroy(&src);
    slist_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/*  TC-15  move 后 src 进入空状态                                      */
/* ------------------------------------------------------------------ */
static void tc15_move(void)
{
    puts("TC-15: move");
    SList src, dst;
    CHECK_EQ(slist_new(&src), SL_OK);

    int a = 7, b = 8;
    slist_push_back(&src, &a);
    slist_push_back(&src, &b);

    slist_move(&dst, &src);
    CHECK(src.dummy == NULL);
    CHECK_EQ(src.len, 0u);
    CHECK_EQ(slist_len(&dst), 2u);

    void *out = NULL;
    slist_peek_front(&dst, &out);
    CHECK_EQ(*(int *)out, 7);

    slist_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/*  TC-16  foreach 遍历顺序正确                                        */
/* ------------------------------------------------------------------ */

static int g_sum = 0;
static void sum_fn(void *val, void *user_data)
{
    (void)user_data;
    g_sum += *(int *)val;
}

static void tc16_foreach(void)
{
    puts("TC-16: foreach");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    int vals[4] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++)
        slist_push_back(&list, &vals[i]);

    g_sum = 0;
    slist_foreach(&list, sum_fn, NULL);
    CHECK_EQ(g_sum, 10);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  TC-17  哨兵完整性：操作后 tail 始终有效                            */
/* ------------------------------------------------------------------ */
static void tc17_sentinel_invariant(void)
{
    puts("TC-17: sentinel & tail invariant");
    SList list;
    CHECK_EQ(slist_new(&list), SL_OK);

    /* 空链表：tail == dummy */
    CHECK(list.tail == list.dummy);

    int x = 1, y = 2;

    /* push_front 第一个元素：tail 应更新 */
    slist_push_front(&list, &x);
    CHECK(list.tail != list.dummy);
    CHECK_EQ(*(int *)list.tail->val, 1);

    /* push_back 第二个元素：tail 指向新节点 */
    slist_push_back(&list, &y);
    CHECK_EQ(*(int *)list.tail->val, 2);

    /* pop_back 后 tail 退回到第一个节点 */
    void *out = NULL;
    slist_pop_back(&list, &out);
    CHECK_EQ(*(int *)list.tail->val, 1);

    /* pop_front 最后一个节点后 tail == dummy */
    slist_pop_front(&list, &out);
    CHECK(list.tail == list.dummy);

    slist_destroy(&list);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    puts("=== SList C Tests ===");
    tc01_fifo();
    tc02_lifo();
    tc03_pop_empty();
    tc04_get_peek();
    tc05_insert_after();
    tc06_insert_after_dummy();
    tc07_remove_after();
    tc08_reverse();
    tc09_find();
    tc10_managed_clear();
    tc11_managed_destroy();
    tc12_pop_back();
    tc13_clone_independence();
    tc14_shallow_copy();
    tc15_move();
    tc16_foreach();
    tc17_sentinel_invariant();

    printf("\n=== Result: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

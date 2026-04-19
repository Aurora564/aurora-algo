/**
 * test_stack.c — C Stack 功能测试
 *
 * 编译（ArrayStack + ListStack）：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_stack tests/test_stack.c src/Vec.c src/Stack.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_stack
 *   gcc -fsanitize=address,undefined ... && ./tests/test_stack
 */

#include "Stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/*  测试工具宏                                                          */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (expr) {                                                        \
            g_pass++;                                                      \
        } else {                                                           \
            fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        if ((a) == (b)) {                                                  \
            g_pass++;                                                      \
        } else {                                                           \
            fprintf(stderr, "FAIL  %s:%d  %s == %s  (%d != %d)\n",       \
                    __FILE__, __LINE__, #a, #b, (int)(a), (int)(b));      \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

/* ------------------------------------------------------------------ */
/*  托管模式辅助                                                        */
/* ------------------------------------------------------------------ */

static int g_free_count = 0;

static void count_free(void *p)
{
    free(p);
    g_free_count++;
}

static void *make_int(int v)
{
    int *p = (int *)malloc(sizeof(int));
    assert(p);
    *p = v;
    return p;
}

/* ================================================================== */
/*  ArrayStack 测试用例                                                 */
/* ================================================================== */

/* TC-01  LIFO 顺序 */
static void atc01_lifo(void)
{
    ArrayStack s;
    astack_new(&s, sizeof(int));

    for (int i = 1; i <= 5; i++)
        CHECK_EQ(astack_push(&s, &i), STACK_OK);

    CHECK_EQ((int)astack_len(&s), 5);

    for (int i = 5; i >= 1; i--) {
        int out = 0;
        CHECK_EQ(astack_pop(&s, &out), STACK_OK);
        CHECK_EQ(out, i);
    }

    CHECK(astack_is_empty(&s));
    astack_destroy(&s);
}

/* TC-02  pop 空栈返回 ERR_EMPTY */
static void atc02_pop_empty(void)
{
    ArrayStack s;
    astack_new(&s, sizeof(int));

    int out;
    CHECK_EQ(astack_pop(&s, &out), STACK_ERR_EMPTY);

    astack_destroy(&s);
}

/* TC-03  peek 不改变 len */
static void atc03_peek_len(void)
{
    ArrayStack s;
    astack_new(&s, sizeof(int));

    int val = 42;
    astack_push(&s, &val);

    void *ptr = NULL;
    CHECK_EQ(astack_peek(&s, &ptr), STACK_OK);
    CHECK_EQ(*(int *)ptr, 42);
    CHECK_EQ((int)astack_len(&s), 1);  /* len 不变 */

    astack_destroy(&s);
}

/* TC-04  触发扩容后所有元素仍正确 */
static void atc04_grow(void)
{
    ArrayStack s;
    astack_new_with_cap(&s, sizeof(int), 2);  /* 初始容量 2 */

    for (int i = 0; i < 16; i++)
        astack_push(&s, &i);

    CHECK_EQ((int)astack_len(&s), 16);

    for (int i = 15; i >= 0; i--) {
        int out = -1;
        astack_pop(&s, &out);
        CHECK_EQ(out, i);
    }

    astack_destroy(&s);
}

/* TC-05  reserve 后不触发扩容 */
static void atc05_reserve(void)
{
    ArrayStack s;
    astack_new(&s, sizeof(int));
    CHECK_EQ(astack_reserve(&s, 64), STACK_OK);

    size_t cap_before = s.base.cap;

    for (int i = 0; i < 64; i++)
        astack_push(&s, &i);

    CHECK_EQ(s.base.cap, cap_before);  /* 容量未变 */
    CHECK_EQ((int)astack_len(&s), 64);

    astack_destroy(&s);
}

/* TC-06  clear：len 归零，容量保留 */
static void atc06_clear(void)
{
    ArrayStack s;
    astack_new(&s, sizeof(int));

    for (int i = 0; i < 8; i++)
        astack_push(&s, &i);

    size_t cap_before = s.base.cap;
    astack_clear(&s);

    CHECK_EQ((int)astack_len(&s), 0);
    CHECK(astack_is_empty(&s));
    CHECK_EQ(s.base.cap, cap_before);

    astack_destroy(&s);
}

/* TC-07  shrink_to_fit */
static void atc07_shrink(void)
{
    ArrayStack s;
    astack_new_with_cap(&s, sizeof(int), 32);

    int val = 1;
    astack_push(&s, &val);

    CHECK_EQ(astack_shrink_to_fit(&s), STACK_OK);
    CHECK_EQ(s.base.cap, (size_t)1);

    astack_destroy(&s);
}

/* ================================================================== */
/*  ListStack 测试用例                                                  */
/* ================================================================== */

/* TC-01  LIFO 顺序 */
static void ltc01_lifo(void)
{
    ListStack s;
    lstack_new(&s);

    for (int i = 1; i <= 5; i++)
        CHECK_EQ(lstack_push(&s, make_int(i)), STACK_OK);

    CHECK_EQ((int)lstack_len(&s), 5);

    for (int i = 5; i >= 1; i--) {
        void *out = NULL;
        CHECK_EQ(lstack_pop(&s, &out), STACK_OK);
        CHECK_EQ(*(int *)out, i);
        free(out);
    }

    CHECK(lstack_is_empty(&s));
    lstack_destroy(&s);
}

/* TC-02  pop 空栈返回 ERR_EMPTY */
static void ltc02_pop_empty(void)
{
    ListStack s;
    lstack_new(&s);

    void *out = NULL;
    CHECK_EQ(lstack_pop(&s, &out), STACK_ERR_EMPTY);

    lstack_destroy(&s);
}

/* TC-03  peek 不改变 len */
static void ltc03_peek_len(void)
{
    ListStack s;
    lstack_new(&s);

    lstack_push(&s, make_int(99));

    void *ptr = NULL;
    CHECK_EQ(lstack_peek(&s, &ptr), STACK_OK);
    CHECK_EQ(*(int *)ptr, 99);
    CHECK_EQ((int)lstack_len(&s), 1);

    void *out = NULL;
    lstack_pop(&s, &out);
    free(out);

    lstack_destroy(&s);
}

/* TC-04  push/pop 交替，len 始终准确 */
static void ltc04_interleave(void)
{
    ListStack s;
    lstack_new(&s);

    lstack_push(&s, make_int(1));
    lstack_push(&s, make_int(2));
    CHECK_EQ((int)lstack_len(&s), 2);

    void *out = NULL;
    lstack_pop(&s, &out); free(out);
    CHECK_EQ((int)lstack_len(&s), 1);

    lstack_push(&s, make_int(3));
    lstack_push(&s, make_int(4));
    CHECK_EQ((int)lstack_len(&s), 3);

    /* 弹出全部 */
    while (!lstack_is_empty(&s)) {
        lstack_pop(&s, &out);
        free(out);
    }
    CHECK_EQ((int)lstack_len(&s), 0);

    lstack_destroy(&s);
}

/* TC-05  托管模式 clear：free_val 调用次数 == len */
static void ltc05_managed_clear(void)
{
    ListStack s;
    lstack_new_managed(&s, count_free);
    g_free_count = 0;

    for (int i = 0; i < 5; i++)
        lstack_push(&s, make_int(i));

    lstack_clear(&s);
    CHECK_EQ(g_free_count, 5);
    CHECK(lstack_is_empty(&s));

    lstack_destroy(&s);
}

/* TC-06  托管模式 destroy：free_val 调用次数 == len */
static void ltc06_managed_destroy(void)
{
    ListStack s;
    lstack_new_managed(&s, count_free);
    g_free_count = 0;

    for (int i = 0; i < 3; i++)
        lstack_push(&s, make_int(i));

    lstack_destroy(&s);
    CHECK_EQ(g_free_count, 3);
}

/* ================================================================== */
/*  等价性测试：ArrayStack vs ListStack                                  */
/* ================================================================== */

/* TC-07  相同操作序列，两种实现输出相同 */
static void tc07_equivalence(void)
{
    ArrayStack as;
    ListStack  ls;
    astack_new(&as, sizeof(int));
    lstack_new(&ls);

    /* 压入 1..10 */
    for (int i = 1; i <= 10; i++) {
        astack_push(&as, &i);
        lstack_push(&ls, make_int(i));
    }

    /* 逐一弹出并比较 */
    for (int i = 10; i >= 1; i--) {
        int  av = -1;
        void *lv_ptr = NULL;

        astack_pop(&as, &av);
        lstack_pop(&ls, &lv_ptr);
        int lv = *(int *)lv_ptr;
        free(lv_ptr);

        CHECK_EQ(av, lv);
        CHECK_EQ(av, i);
    }

    CHECK(astack_is_empty(&as));
    CHECK(lstack_is_empty(&ls));

    astack_destroy(&as);
    lstack_destroy(&ls);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main(void)
{
    printf("=== ArrayStack ===\n");
    atc01_lifo();
    atc02_pop_empty();
    atc03_peek_len();
    atc04_grow();
    atc05_reserve();
    atc06_clear();
    atc07_shrink();

    printf("=== ListStack ===\n");
    ltc01_lifo();
    ltc02_pop_empty();
    ltc03_peek_len();
    ltc04_interleave();
    ltc05_managed_clear();
    ltc06_managed_destroy();

    printf("=== Equivalence ===\n");
    tc07_equivalence();

    printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

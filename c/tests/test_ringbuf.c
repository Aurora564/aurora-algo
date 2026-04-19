/**
 * test_ringbuf.c — C RingBuf 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_ringbuf tests/test_ringbuf.c src/RingBuf.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_ringbuf
 *   gcc -fsanitize=address,undefined ... && ./tests/test_ringbuf
 */

#include "RingBuf.h"
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

/* ================================================================== */
/*  Fixed buffer 测试                                                   */
/* ================================================================== */

/* TC-01  基本 FIFO 顺序 */
static void ftc01_fifo(void)
{
    RingBuf rb;
    CHECK_EQ(rb_new(&rb, 8, sizeof(int)), RB_OK);

    for (int i = 1; i <= 5; i++)
        CHECK_EQ(rb_enqueue(&rb, &i), RB_OK);

    CHECK_EQ((int)rb_len(&rb), 5);

    for (int i = 1; i <= 5; i++) {
        int out = -1;
        CHECK_EQ(rb_dequeue(&rb, &out), RB_OK);
        CHECK_EQ(out, i);
    }

    CHECK(rb_is_empty(&rb));
    rb_destroy(&rb);
}

/* TC-02  满容量后 enqueue 返回 RB_ERR_FULL */
static void ftc02_full(void)
{
    RingBuf rb;
    rb_new(&rb, 3, sizeof(int));

    int v = 0;
    CHECK_EQ(rb_enqueue(&rb, &v), RB_OK);
    CHECK_EQ(rb_enqueue(&rb, &v), RB_OK);
    CHECK_EQ(rb_enqueue(&rb, &v), RB_OK);
    CHECK(rb_is_full(&rb));
    CHECK_EQ(rb_enqueue(&rb, &v), RB_ERR_FULL);

    rb_destroy(&rb);
}

/* TC-03  空队列 dequeue/peek 返回 RB_ERR_EMPTY */
static void ftc03_empty(void)
{
    RingBuf rb;
    rb_new(&rb, 4, sizeof(int));

    int out = -1;
    CHECK_EQ(rb_dequeue(&rb, &out), RB_ERR_EMPTY);

    void *ptr = NULL;
    CHECK_EQ(rb_peek_front(&rb, &ptr), RB_ERR_EMPTY);
    CHECK_EQ(rb_peek_back(&rb,  &ptr), RB_ERR_EMPTY);

    rb_destroy(&rb);
}

/* TC-04  wrap-around：head 和 tail 跨越边界 */
static void ftc04_wraparound(void)
{
    RingBuf rb;
    rb_new(&rb, 4, sizeof(int));

    /* 填满 */
    for (int i = 0; i < 4; i++) rb_enqueue(&rb, &i);
    /* 取出 2 个，腾出头部 */
    int out;
    rb_dequeue(&rb, &out); /* 0 */
    rb_dequeue(&rb, &out); /* 1 */
    /* 再推入 2 个（wrap） */
    int v = 10; rb_enqueue(&rb, &v);
    v = 11; rb_enqueue(&rb, &v);

    CHECK_EQ((int)rb_len(&rb), 4);
    CHECK(rb_is_full(&rb));

    rb_dequeue(&rb, &out); CHECK_EQ(out, 2);
    rb_dequeue(&rb, &out); CHECK_EQ(out, 3);
    rb_dequeue(&rb, &out); CHECK_EQ(out, 10);
    rb_dequeue(&rb, &out); CHECK_EQ(out, 11);

    CHECK(rb_is_empty(&rb));
    rb_destroy(&rb);
}

/* TC-05  peek 不改变 len */
static void ftc05_peek(void)
{
    RingBuf rb;
    rb_new(&rb, 4, sizeof(int));

    int a = 7, b = 8;
    rb_enqueue(&rb, &a);
    rb_enqueue(&rb, &b);

    void *front = NULL, *back = NULL;
    rb_peek_front(&rb, &front);
    rb_peek_back(&rb, &back);
    CHECK_EQ(*(int *)front, 7);
    CHECK_EQ(*(int *)back,  8);
    CHECK_EQ((int)rb_len(&rb), 2);

    int out;
    rb_dequeue(&rb, &out);
    rb_dequeue(&rb, &out);
    rb_destroy(&rb);
}

/* TC-06  rb_get 随机访问 */
static void ftc06_get(void)
{
    RingBuf rb;
    rb_new(&rb, 8, sizeof(int));

    for (int i = 0; i < 5; i++) rb_enqueue(&rb, &i);

    for (int i = 0; i < 5; i++) {
        void *ptr = NULL;
        CHECK_EQ(rb_get(&rb, (size_t)i, &ptr), RB_OK);
        CHECK_EQ(*(int *)ptr, i);
    }

    void *ptr = NULL;
    CHECK_EQ(rb_get(&rb, 5, &ptr), RB_ERR_OOB);

    int out;
    while (!rb_is_empty(&rb)) rb_dequeue(&rb, &out);
    rb_destroy(&rb);
}

/* TC-07  clear：len 归零，cap 保留 */
static void ftc07_clear(void)
{
    RingBuf rb;
    rb_new(&rb, 8, sizeof(int));

    for (int i = 0; i < 5; i++) rb_enqueue(&rb, &i);
    rb_clear(&rb);

    CHECK_EQ((int)rb_len(&rb), 0);
    CHECK_EQ((int)rb_cap(&rb), 8);
    CHECK(rb_is_empty(&rb));

    rb_destroy(&rb);
}

/* TC-08  reserve 在 fixed 模式返回 RB_ERR_BAD_CONFIG */
static void ftc08_reserve_fixed(void)
{
    RingBuf rb;
    rb_new(&rb, 4, sizeof(int));
    CHECK_EQ(rb_reserve(&rb, 4), RB_ERR_BAD_CONFIG);
    rb_destroy(&rb);
}

/* ================================================================== */
/*  Growable buffer 测试                                                */
/* ================================================================== */

/* TC-09  基本 FIFO，cap 自动翻倍 */
static void gtc09_grow(void)
{
    RingBuf rb;
    CHECK_EQ(rb_new_growable(&rb, 2, sizeof(int)), RB_OK);

    /* 推入超过初始容量的元素，触发自动扩容 */
    for (int i = 0; i < 16; i++)
        CHECK_EQ(rb_enqueue(&rb, &i), RB_OK);

    CHECK_EQ((int)rb_len(&rb), 16);
    CHECK(!rb_is_full(&rb) || (int)rb_cap(&rb) >= 16);

    for (int i = 0; i < 16; i++) {
        int out = -1;
        CHECK_EQ(rb_dequeue(&rb, &out), RB_OK);
        CHECK_EQ(out, i);
    }

    CHECK(rb_is_empty(&rb));
    rb_destroy(&rb);
}

/* TC-10  cap 始终是 2 的幂 */
static void gtc10_pow2_cap(void)
{
    for (size_t init = 1; init <= 32; init++) {
        RingBuf rb;
        rb_new_growable(&rb, init, sizeof(int));
        size_t c = rb_cap(&rb);
        CHECK((c & (c - 1)) == 0);   /* power-of-2 */
        rb_destroy(&rb);
    }
}

/* TC-11  reserve 保证足够容量 */
static void gtc11_reserve(void)
{
    RingBuf rb;
    rb_new_growable(&rb, 2, sizeof(int));

    CHECK_EQ(rb_reserve(&rb, 64), RB_OK);
    CHECK((int)rb_cap(&rb) >= 64);

    /* 推入 64 个元素不应再次扩容 */
    size_t cap_before = rb_cap(&rb);
    for (int i = 0; i < 64; i++)
        rb_enqueue(&rb, &i);

    CHECK_EQ(rb_cap(&rb), cap_before);

    int out;
    while (!rb_is_empty(&rb)) rb_dequeue(&rb, &out);
    rb_destroy(&rb);
}

/* TC-12  init_cap=0 返回 RB_ERR_BAD_CONFIG */
static void gtc12_bad_config(void)
{
    RingBuf rb;
    CHECK_EQ(rb_new_growable(&rb, 0, sizeof(int)), RB_ERR_BAD_CONFIG);
    CHECK_EQ(rb_new(&rb, 0, sizeof(int)),          RB_ERR_BAD_CONFIG);
}

/* ================================================================== */
/*  strerror 覆盖                                                       */
/* ================================================================== */

static void tc13_strerror(void)
{
    CHECK(rb_strerror(RB_OK)             != NULL);
    CHECK(rb_strerror(RB_ERR_EMPTY)      != NULL);
    CHECK(rb_strerror(RB_ERR_FULL)       != NULL);
    CHECK(rb_strerror(RB_ERR_OOB)        != NULL);
    CHECK(rb_strerror(RB_ERR_ALLOC)      != NULL);
    CHECK(rb_strerror(RB_ERR_BAD_CONFIG) != NULL);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main(void)
{
    printf("=== RingBuf (fixed) ===\n");
    ftc01_fifo();
    ftc02_full();
    ftc03_empty();
    ftc04_wraparound();
    ftc05_peek();
    ftc06_get();
    ftc07_clear();
    ftc08_reserve_fixed();

    printf("=== RingBuf (growable) ===\n");
    gtc09_grow();
    gtc10_pow2_cap();
    gtc11_reserve();
    gtc12_bad_config();

    printf("=== Diagnostics ===\n");
    tc13_strerror();

    printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

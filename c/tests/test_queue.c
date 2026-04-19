/**
 * test_queue.c — C Queue 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_queue tests/test_queue.c src/DList.c src/Queue.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_queue
 *   gcc -fsanitize=address,undefined ... && ./tests/test_queue
 */

#include "Queue.h"
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

/* foreach 收集辅助 */
#define FOREACH_BUF_MAX 64
static int  g_foreach_buf[FOREACH_BUF_MAX];
static int  g_foreach_idx = 0;

static void collect_int(void *val)
{
    if (g_foreach_idx < FOREACH_BUF_MAX)
        g_foreach_buf[g_foreach_idx++] = *(int *)val;
}

/* ================================================================== */
/*  TC-01  FIFO 顺序                                                    */
/* ================================================================== */

static void tc01_fifo(void)
{
    Queue q;
    queue_new(&q);

    for (int i = 1; i <= 5; i++)
        CHECK_EQ(queue_enqueue(&q, make_int(i)), QUEUE_OK);

    CHECK_EQ((int)queue_len(&q), 5);

    for (int i = 1; i <= 5; i++) {
        void *out = NULL;
        CHECK_EQ(queue_dequeue(&q, &out), QUEUE_OK);
        CHECK_EQ(*(int *)out, i);
        free(out);
    }

    CHECK(queue_is_empty(&q));
    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-02  空队列 dequeue 返回 QUEUE_ERR_EMPTY                          */
/* ================================================================== */

static void tc02_dequeue_empty(void)
{
    Queue q;
    queue_new(&q);

    void *out = NULL;
    CHECK_EQ(queue_dequeue(&q, &out), QUEUE_ERR_EMPTY);

    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-03  peek 不改变 len                                              */
/* ================================================================== */

static void tc03_peek_len(void)
{
    Queue q;
    queue_new(&q);

    queue_enqueue(&q, make_int(10));
    queue_enqueue(&q, make_int(20));

    void *front = NULL, *back = NULL;
    CHECK_EQ(queue_peek_front(&q, &front), QUEUE_OK);
    CHECK_EQ(queue_peek_back(&q, &back),  QUEUE_OK);
    CHECK_EQ(*(int *)front, 10);
    CHECK_EQ(*(int *)back,  20);
    CHECK_EQ((int)queue_len(&q), 2);  /* len 不变 */

    void *out = NULL;
    queue_dequeue(&q, &out); free(out);
    queue_dequeue(&q, &out); free(out);

    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-04  peek 空队列返回 QUEUE_ERR_EMPTY                              */
/* ================================================================== */

static void tc04_peek_empty(void)
{
    Queue q;
    queue_new(&q);

    void *out = NULL;
    CHECK_EQ(queue_peek_front(&q, &out), QUEUE_ERR_EMPTY);
    CHECK_EQ(queue_peek_back(&q, &out),  QUEUE_ERR_EMPTY);

    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-05  enqueue/dequeue 交替，len 始终准确                           */
/* ================================================================== */

static void tc05_interleave(void)
{
    Queue q;
    queue_new(&q);

    queue_enqueue(&q, make_int(1));
    queue_enqueue(&q, make_int(2));
    CHECK_EQ((int)queue_len(&q), 2);

    void *out = NULL;
    queue_dequeue(&q, &out); free(out);
    CHECK_EQ((int)queue_len(&q), 1);

    queue_enqueue(&q, make_int(3));
    queue_enqueue(&q, make_int(4));
    CHECK_EQ((int)queue_len(&q), 3);

    while (!queue_is_empty(&q)) {
        queue_dequeue(&q, &out);
        free(out);
    }
    CHECK_EQ((int)queue_len(&q), 0);

    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-06  foreach 遍历顺序（FIFO）                                     */
/* ================================================================== */

static void tc06_foreach(void)
{
    Queue q;
    queue_new(&q);

    for (int i = 1; i <= 4; i++)
        queue_enqueue(&q, make_int(i * 10));

    g_foreach_idx = 0;
    queue_foreach(&q, collect_int);

    CHECK_EQ(g_foreach_idx, 4);
    CHECK_EQ(g_foreach_buf[0], 10);
    CHECK_EQ(g_foreach_buf[1], 20);
    CHECK_EQ(g_foreach_buf[2], 30);
    CHECK_EQ(g_foreach_buf[3], 40);

    void *out = NULL;
    while (!queue_is_empty(&q)) {
        queue_dequeue(&q, &out);
        free(out);
    }

    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-07  托管模式 clear：free_val 调用次数 == len                     */
/* ================================================================== */

static void tc07_managed_clear(void)
{
    Queue q;
    queue_new_managed(&q, count_free, NULL);
    g_free_count = 0;

    for (int i = 0; i < 5; i++)
        queue_enqueue(&q, make_int(i));

    queue_clear(&q);
    CHECK_EQ(g_free_count, 5);
    CHECK(queue_is_empty(&q));

    queue_destroy(&q);
}

/* ================================================================== */
/*  TC-08  托管模式 destroy：free_val 调用次数 == len                   */
/* ================================================================== */

static void tc08_managed_destroy(void)
{
    Queue q;
    queue_new_managed(&q, count_free, NULL);
    g_free_count = 0;

    for (int i = 0; i < 3; i++)
        queue_enqueue(&q, make_int(i));

    queue_destroy(&q);
    CHECK_EQ(g_free_count, 3);
}

/* ================================================================== */
/*  TC-09  strerror 覆盖                                                */
/* ================================================================== */

static void tc09_strerror(void)
{
    CHECK(queue_strerror(QUEUE_OK)        != NULL);
    CHECK(queue_strerror(QUEUE_ERR_EMPTY) != NULL);
    CHECK(queue_strerror(QUEUE_ERR_ALLOC) != NULL);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main(void)
{
    printf("=== Queue ===\n");
    tc01_fifo();
    tc02_dequeue_empty();
    tc03_peek_len();
    tc04_peek_empty();
    tc05_interleave();
    tc06_foreach();
    tc07_managed_clear();
    tc08_managed_destroy();
    tc09_strerror();

    printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

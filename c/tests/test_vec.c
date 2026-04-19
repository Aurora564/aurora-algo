/**
 * test_vec.c — C Vec 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_vec tests/test_vec.c src/Vec.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_vec
 *   gcc -fsanitize=address,undefined ... && ./tests/test_vec
 */

#include "devc.h"
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

typedef struct { char *str; } StrElem;

static void str_free(void *p)
{
    StrElem *e = (StrElem *)p;
    free(e->str);
    g_free_count++;
}

static void *str_clone(const void *p)
{
    const StrElem *src = (const StrElem *)p;
    StrElem *dst = malloc(sizeof(StrElem));
    if (!dst) return NULL;
    dst->str = malloc(strlen(src->str) + 1);
    if (!dst->str) { free(dst); return NULL; }
    strcpy(dst->str, src->str);
    return dst;
}

/* ------------------------------------------------------------------ */
/*  TC-01  push 直到触发扩容，验证所有元素值正确                         */
/* ------------------------------------------------------------------ */
static void tc01_push_grow(void)
{
    puts("TC-01: push until grow");
    Vec v;
    CHECK_EQ(vec_new_with_cap(&v, sizeof(int), 2), VEC_OK);

    for (int i = 0; i < 10; i++)
        CHECK_EQ(vec_push(&v, &i), VEC_OK);

    CHECK_EQ(vec_len(&v), 10u);
    for (size_t i = 0; i < 10; i++) {
        int val = -1;
        CHECK_EQ(vec_get(&v, i, &val), VEC_OK);
        CHECK_EQ(val, (int)i);
    }
    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-02  pop 至空，最后一次返回 ERR_EMPTY                             */
/* ------------------------------------------------------------------ */
static void tc02_pop_empty(void)
{
    puts("TC-02: pop to empty");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    int x = 42;
    CHECK_EQ(vec_push(&v, &x), VEC_OK);
    int out = 0;
    CHECK_EQ(vec_pop(&v, &out), VEC_OK);
    CHECK_EQ(out, 42);
    CHECK_EQ(vec_pop(&v, &out), VEC_ERR_EMPTY);
    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-03/04  get 边界                                                  */
/* ------------------------------------------------------------------ */
static void tc03_04_get_bounds(void)
{
    puts("TC-03/04: get bounds");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    for (int i = 0; i < 5; i++)
        vec_push(&v, &i);

    int val = -1;
    CHECK_EQ(vec_get(&v, 0, &val), VEC_OK);
    CHECK_EQ(val, 0);
    CHECK_EQ(vec_get(&v, 4, &val), VEC_OK);
    CHECK_EQ(val, 4);
    CHECK_EQ(vec_get(&v, 5, &val), VEC_ERR_OOB);   /* TC-04 */
    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-05  insert(0, x) 后原有元素整体后移一位                          */
/* ------------------------------------------------------------------ */
static void tc05_insert_head(void)
{
    puts("TC-05: insert at head");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    for (int i = 1; i <= 3; i++) vec_push(&v, &i); /* [1,2,3] */
    int x = 0;
    CHECK_EQ(vec_insert(&v, 0, &x), VEC_OK);        /* [0,1,2,3] */
    CHECK_EQ(vec_len(&v), 4u);

    for (size_t i = 0; i < 4; i++) {
        int val = -1;
        vec_get(&v, i, &val);
        CHECK_EQ(val, (int)i);
    }
    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-06  remove(0) 后原有元素整体前移一位                             */
/* ------------------------------------------------------------------ */
static void tc06_remove_head(void)
{
    puts("TC-06: remove at head");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    for (int i = 0; i < 4; i++) vec_push(&v, &i);  /* [0,1,2,3] */
    int out = -1;
    CHECK_EQ(vec_remove(&v, 0, &out), VEC_OK);      /* [1,2,3] */
    CHECK_EQ(out, 0);
    CHECK_EQ(vec_len(&v), 3u);

    for (size_t i = 0; i < 3; i++) {
        int val = -1;
        vec_get(&v, i, &val);
        CHECK_EQ(val, (int)(i + 1));
    }
    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-07  swap_remove(i) 后末尾元素出现在 i 位置                        */
/* ------------------------------------------------------------------ */
static void tc07_swap_remove(void)
{
    puts("TC-07: swap_remove");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    for (int i = 0; i < 5; i++) vec_push(&v, &i);  /* [0,1,2,3,4] */
    int out = -1;
    CHECK_EQ(vec_swap_remove(&v, 1, &out), VEC_OK); /* removes 1, 4 fills pos 1 */
    CHECK_EQ(out, 1);
    CHECK_EQ(vec_len(&v), 4u);

    int pos1 = -1;
    vec_get(&v, 1, &pos1);
    CHECK_EQ(pos1, 4); /* 末尾元素 4 填补到 index=1 */
    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-08  vec_copy 后修改副本，源数组不受影响                           */
/* ------------------------------------------------------------------ */
static void tc08_copy_independence(void)
{
    puts("TC-08: copy independence (POD)");
    Vec src, dst;
    CHECK_EQ(vec_new(&src, sizeof(int)), VEC_OK);

    for (int i = 0; i < 3; i++) vec_push(&src, &i);
    CHECK_EQ(vec_copy(&dst, &src), VEC_OK);

    int newval = 99;
    CHECK_EQ(vec_set(&dst, 0, &newval), VEC_OK);

    int src0 = -1;
    vec_get(&src, 0, &src0);
    CHECK_EQ(src0, 0); /* 源不受影响 */

    vec_destroy(&src);
    vec_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/*  TC-09  vec_clone 后修改副本内部字段，源数组不受影响                   */
/* ------------------------------------------------------------------ */
static void tc09_clone_independence(void)
{
    puts("TC-09: clone independence (heap element)");
    Vec src;
    VecConfig cfg = {
        sizeof(StrElem), 4, NULL, str_clone, str_free
    };
    CHECK_EQ(vec_new_config(&src, &cfg), VEC_OK);

    StrElem e;
    e.str = malloc(8); strcpy(e.str, "hello");
    CHECK_EQ(vec_push(&src, &e), VEC_OK);

    Vec dst;
    CHECK_EQ(vec_clone(&dst, &src), VEC_OK);

    /* 修改副本 */
    StrElem *dp = (StrElem *)dst.data;
    strcpy(dp[0].str, "world");

    /* 源不受影响 */
    StrElem *sp = (StrElem *)src.data;
    CHECK(strcmp(sp[0].str, "hello") == 0);

    vec_destroy(&src);
    vec_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/*  TC-10  move 后源数组 len == 0 且 data == NULL                        */
/* ------------------------------------------------------------------ */
static void tc10_move(void)
{
    puts("TC-10: move");
    Vec src, dst;
    CHECK_EQ(vec_new(&src, sizeof(int)), VEC_OK);

    int x = 7;
    vec_push(&src, &x);
    vec_move(&dst, &src);

    CHECK(src.data == NULL);
    CHECK_EQ(src.len, 0u);
    CHECK_EQ(vec_len(&dst), 1u);

    int val = -1;
    vec_get(&dst, 0, &val);
    CHECK_EQ(val, 7);
    vec_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/*  TC-11  托管模式 clear：free_fn 被调用恰好 len 次                     */
/* ------------------------------------------------------------------ */
static void tc11_managed_clear(void)
{
    puts("TC-11: managed clear");
    Vec v;
    VecConfig cfg = { sizeof(StrElem), 4, NULL, str_clone, str_free };
    CHECK_EQ(vec_new_config(&v, &cfg), VEC_OK);

    for (int i = 0; i < 5; i++) {
        StrElem e;
        e.str = malloc(4);
        sprintf(e.str, "%d", i);
        vec_push(&v, &e);
    }

    g_free_count = 0;
    vec_clear(&v);
    CHECK_EQ(g_free_count, 5);
    CHECK_EQ(vec_len(&v), 0u);
    CHECK(vec_cap(&v) >= 5u); /* 容量不变 */

    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  TC-12  托管模式 destroy：free_fn 被调用 len 次                       */
/* ------------------------------------------------------------------ */
static void tc12_managed_destroy(void)
{
    puts("TC-12: managed destroy");
    Vec v;
    VecConfig cfg = { sizeof(StrElem), 4, NULL, str_clone, str_free };
    CHECK_EQ(vec_new_config(&v, &cfg), VEC_OK);

    for (int i = 0; i < 3; i++) {
        StrElem e;
        e.str = malloc(4);
        sprintf(e.str, "%d", i);
        vec_push(&v, &e);
    }

    g_free_count = 0;
    vec_destroy(&v);
    CHECK_EQ(g_free_count, 3);
    CHECK(v.data == NULL);
}

/* ------------------------------------------------------------------ */
/*  TC-13  自定义扩容策略（×1.5），验证扩容后容量符合预期                  */
/* ------------------------------------------------------------------ */

static size_t grow_x15(size_t cap)
{
    return cap + cap / 2 + 1; /* 约 ×1.5，保证严格递增 */
}

static void tc13_custom_grow(void)
{
    puts("TC-13: custom grow x1.5");
    Vec v;
    VecConfig cfg = { sizeof(int), 2, grow_x15, NULL, NULL };
    CHECK_EQ(vec_new_config(&v, &cfg), VEC_OK);
    CHECK_EQ(vec_cap(&v), 2u);

    /* 推入第 3 个元素，触发扩容：grow_x15(2) = 4 */
    for (int i = 0; i < 3; i++) vec_push(&v, &i);
    CHECK(vec_cap(&v) >= 3u);

    /* 验证容量由 grow_x15 驱动（不是 ×2 的 4） */
    size_t expected = grow_x15(2); /* 4 */
    CHECK_EQ(vec_cap(&v), expected);

    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  额外：reserve 与 shrink_to_fit                                      */
/* ------------------------------------------------------------------ */
static void tc_reserve_shrink(void)
{
    puts("TC-extra: reserve & shrink_to_fit");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    CHECK_EQ(vec_reserve(&v, 100), VEC_OK);
    CHECK(vec_cap(&v) >= 100u);

    for (int i = 0; i < 5; i++) vec_push(&v, &i);
    CHECK_EQ(vec_shrink_to_fit(&v), VEC_OK);
    CHECK_EQ(vec_cap(&v), 5u);

    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  额外：extend 与 slice                                               */
/* ------------------------------------------------------------------ */
static void tc_extend_slice(void)
{
    puts("TC-extra: extend & slice");
    Vec v;
    CHECK_EQ(vec_new(&v, sizeof(int)), VEC_OK);

    int src[] = {10, 20, 30};
    CHECK_EQ(vec_extend(&v, src, 3), VEC_OK);
    CHECK_EQ(vec_len(&v), 3u);

    VecSlice sl;
    CHECK_EQ(vec_slice(&v, 1, 3, &sl), VEC_OK);
    CHECK_EQ(sl.len, 2u);
    int *p = (int *)sl.ptr;
    CHECK_EQ(p[0], 20);
    CHECK_EQ(p[1], 30);

    vec_destroy(&v);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    puts("=== Vec C Tests ===");
    tc01_push_grow();
    tc02_pop_empty();
    tc03_04_get_bounds();
    tc05_insert_head();
    tc06_remove_head();
    tc07_swap_remove();
    tc08_copy_independence();
    tc09_clone_independence();
    tc10_move();
    tc11_managed_clear();
    tc12_managed_destroy();
    tc13_custom_grow();
    tc_reserve_shrink();
    tc_extend_slice();

    printf("\n=== Result: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

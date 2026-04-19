#include "DList.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Minimal test harness                                                 */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(expr)                                                     \
    do {                                                                \
        if (expr) { g_pass++; }                                        \
        else {                                                          \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #expr);                         \
            g_fail++;                                                   \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b)                                                  \
    do {                                                                \
        if ((a) == (b)) { g_pass++; }                                  \
        else {                                                          \
            fprintf(stderr, "FAIL  %s:%d  %s == %s  (%d != %d)\n",   \
                    __FILE__, __LINE__, #a, #b, (int)(a), (int)(b));  \
            g_fail++;                                                   \
        }                                                               \
    } while (0)

/* ------------------------------------------------------------------ */
/* Value helpers                                                        */
/* ------------------------------------------------------------------ */

static void int_free(void *val) { free(val); }

static void *int_clone(const void *val)
{
    int *p = (int *)malloc(sizeof(int));
    if (p) *p = *(const int *)val;
    return p;
}

static void *make_int(int v)
{
    int *p = (int *)malloc(sizeof(int));
    if (p) *p = v;
    return p;
}

static int val_of(void *p) { return *(int *)p; }

/* ------------------------------------------------------------------ */
/* TC-01  Sentinel invariant on empty list                              */
/* ------------------------------------------------------------------ */

static void tc01_sentinel_invariant(void)
{
    DList l;
    CHECK_EQ(dlist_new(&l), DL_OK);
    CHECK(l.dummy != NULL);
    CHECK(l.dummy->prev == l.dummy);
    CHECK(l.dummy->next == l.dummy);
    CHECK_EQ((int)l.len, 0);
    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-02  Push front / back and peek                                    */
/* ------------------------------------------------------------------ */

static void tc02_push_peek(void)
{
    DList l;
    dlist_new(&l);

    dlist_push_back(&l, make_int(1));
    dlist_push_back(&l, make_int(2));
    dlist_push_front(&l, make_int(0));
    CHECK_EQ((int)dlist_len(&l), 3);

    void *v;
    dlist_peek_front(&l, &v); CHECK_EQ(val_of(v), 0);
    dlist_peek_back(&l, &v);  CHECK_EQ(val_of(v), 2);

    /* sentinel still circular */
    CHECK(l.dummy->next->prev == l.dummy);
    CHECK(l.dummy->prev->next == l.dummy);

    dlist_destroy(&l);
    /* no free_fn → intentional leak for test simplicity */
    /* (values were allocated but not freed here)        */
}

/* ------------------------------------------------------------------ */
/* TC-03  Pop front / back FIFO / LIFO semantics                        */
/* ------------------------------------------------------------------ */

static void tc03_pop(void)
{
    DList l;
    dlist_new_managed(&l, int_free, int_clone);

    for (int i = 0; i < 5; i++) dlist_push_back(&l, make_int(i));
    CHECK_EQ((int)dlist_len(&l), 5);

    void *v;
    /* pop front gives 0,1,2 */
    dlist_pop_front(&l, &v); CHECK_EQ(val_of(v), 0); int_free(v);
    dlist_pop_front(&l, &v); CHECK_EQ(val_of(v), 1); int_free(v);
    /* pop back gives 4,3 */
    dlist_pop_back(&l, &v);  CHECK_EQ(val_of(v), 4); int_free(v);
    dlist_pop_back(&l, &v);  CHECK_EQ(val_of(v), 3); int_free(v);

    CHECK_EQ((int)dlist_len(&l), 1);
    dlist_destroy(&l); /* frees remaining node (val=2) */
}

/* ------------------------------------------------------------------ */
/* TC-04  get() bidirectional O(n/2) access                             */
/* ------------------------------------------------------------------ */

static void tc04_get(void)
{
    DList l;
    dlist_new(&l);
    for (int i = 0; i < 10; i++) dlist_push_back(&l, make_int(i));

    void *v;
    for (int i = 0; i < 10; i++) {
        dlist_get(&l, (size_t)i, &v);
        CHECK_EQ(val_of(v), i);
    }

    CHECK_EQ(dlist_get(&l, 10, &v), DL_ERR_OOB);
    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-05  insert_before / insert_after                                  */
/* ------------------------------------------------------------------ */

static void tc05_insert(void)
{
    DList l;
    dlist_new(&l);
    dlist_push_back(&l, make_int(1));
    dlist_push_back(&l, make_int(3));

    /* find node with value 3 and insert 2 before it */
    DLNode *n3 = l.dummy->next->next; /* second node */
    dlist_insert_before(&l, n3, make_int(2));
    CHECK_EQ((int)dlist_len(&l), 3);

    void *v;
    dlist_get(&l, 1, &v); CHECK_EQ(val_of(v), 2);
    dlist_get(&l, 2, &v); CHECK_EQ(val_of(v), 3);

    /* insert 4 after the last node */
    dlist_insert_after(&l, l.dummy->prev, make_int(4));
    dlist_peek_back(&l, &v); CHECK_EQ(val_of(v), 4);

    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-06  remove()                                                      */
/* ------------------------------------------------------------------ */

static void tc06_remove(void)
{
    DList l;
    dlist_new(&l);
    for (int i = 0; i < 5; i++) dlist_push_back(&l, make_int(i));

    /* remove the middle node (index 2, val=2) */
    DLNode *mid = l.dummy->next->next->next;
    void *v;
    dlist_remove(&l, mid, &v);
    CHECK_EQ(val_of(v), 2);
    int_free(v);
    CHECK_EQ((int)dlist_len(&l), 4);

    /* remaining: 0,1,3,4 */
    dlist_get(&l, 2, &v); CHECK_EQ(val_of(v), 3);
    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-07  find()                                                        */
/* ------------------------------------------------------------------ */

static int pred_gt3(const void *val) { return *(const int *)val > 3; }

static void tc07_find(void)
{
    DList l;
    dlist_new(&l);
    for (int i = 0; i < 6; i++) dlist_push_back(&l, make_int(i));

    DLNode *n = dlist_find(&l, pred_gt3);
    CHECK(n != NULL);
    CHECK_EQ(val_of(n->val), 4);

    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-08  reverse()                                                     */
/* ------------------------------------------------------------------ */

static void tc08_reverse(void)
{
    DList l;
    dlist_new(&l);
    for (int i = 0; i < 5; i++) dlist_push_back(&l, make_int(i));

    dlist_reverse(&l);
    CHECK_EQ((int)dlist_len(&l), 5);

    /* sentinel must stay consistent */
    CHECK(l.dummy->next->prev == l.dummy);
    CHECK(l.dummy->prev->next == l.dummy);

    void *v;
    for (int i = 0; i < 5; i++) {
        dlist_get(&l, (size_t)i, &v);
        CHECK_EQ(val_of(v), 4 - i);
    }
    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-09  splice() O(1)                                                 */
/* ------------------------------------------------------------------ */

static void tc09_splice(void)
{
    DList dst, src;
    dlist_new(&dst);
    dlist_new(&src);

    for (int i = 0; i < 3; i++) dlist_push_back(&dst, make_int(i));   /* 0,1,2 */
    for (int i = 10; i < 13; i++) dlist_push_back(&src, make_int(i)); /* 10,11,12 */

    /* splice src before index-1 node of dst (before val=1) */
    DLNode *pos = dst.dummy->next->next; /* val=1 */
    DListError err = dlist_splice(&dst, pos, &src);
    CHECK_EQ(err, DL_OK);
    CHECK_EQ((int)dlist_len(&dst), 6);
    CHECK_EQ((int)dlist_len(&src), 0);

    /* expected order: 0,10,11,12,1,2 */
    void *v;
    int expected[] = {0, 10, 11, 12, 1, 2};
    for (int i = 0; i < 6; i++) {
        dlist_get(&dst, (size_t)i, &v);
        CHECK_EQ(val_of(v), expected[i]);
    }

    /* src sentinel still valid (empty circular) */
    CHECK(src.dummy->next == src.dummy);
    CHECK(src.dummy->prev == src.dummy);

    dlist_destroy(&dst);
    dlist_destroy(&src);
}

/* ------------------------------------------------------------------ */
/* TC-10  clone() deep copy                                             */
/* ------------------------------------------------------------------ */

static void tc10_clone(void)
{
    DList src, dst;
    dlist_new_managed(&src, int_free, int_clone);
    for (int i = 0; i < 4; i++) dlist_push_back(&src, make_int(i));

    DListError err = dlist_clone(&dst, &src);
    CHECK_EQ(err, DL_OK);
    CHECK_EQ((int)dlist_len(&dst), 4);

    /* mutate src; dst must be independent */
    void *v;
    dlist_pop_front(&src, &v); int_free(v);

    void *dv;
    dlist_get(&dst, 0, &dv);
    CHECK_EQ(val_of(dv), 0); /* dst unchanged */

    dlist_destroy(&src);
    dlist_destroy(&dst);
}

/* ------------------------------------------------------------------ */
/* TC-11  foreach / foreach_rev                                         */
/* ------------------------------------------------------------------ */

static int g_sum = 0;
static void acc(void *val) { g_sum += *(int *)val; }

static void tc11_foreach(void)
{
    DList l;
    dlist_new(&l);
    for (int i = 1; i <= 5; i++) dlist_push_back(&l, make_int(i));

    g_sum = 0;
    dlist_foreach(&l, acc);
    CHECK_EQ(g_sum, 15);

    g_sum = 0;
    dlist_foreach_rev(&l, acc);
    CHECK_EQ(g_sum, 15);

    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* TC-12  pop on empty list                                             */
/* ------------------------------------------------------------------ */

static void tc12_empty_errors(void)
{
    DList l;
    dlist_new(&l);

    void *v;
    CHECK_EQ(dlist_pop_front(&l, &v), DL_ERR_EMPTY);
    CHECK_EQ(dlist_pop_back(&l, &v),  DL_ERR_EMPTY);
    CHECK_EQ(dlist_peek_front(&l, &v), DL_ERR_EMPTY);
    CHECK_EQ(dlist_peek_back(&l, &v),  DL_ERR_EMPTY);
    CHECK_EQ(dlist_get(&l, 0, &v),     DL_ERR_OOB);

    dlist_destroy(&l);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    tc01_sentinel_invariant();
    tc02_push_peek();
    tc03_pop();
    tc04_get();
    tc05_insert();
    tc06_remove();
    tc07_find();
    tc08_reverse();
    tc09_splice();
    tc10_clone();
    tc11_foreach();
    tc12_empty_errors();

    printf("DList C:  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

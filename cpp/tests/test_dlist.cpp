#include "DList.hpp"

#include <cstdio>
#include <string>

using namespace algo;

/* ------------------------------------------------------------------ */
/* Minimal test harness                                                 */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (expr) { g_pass++; }                                           \
        else {                                                             \
            std::fprintf(stderr, "FAIL  %s:%d  %s\n",                    \
                         __FILE__, __LINE__, #expr);                       \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        if ((a) == (b)) { g_pass++; }                                     \
        else {                                                             \
            std::fprintf(stderr, "FAIL  %s:%d  (%s)==(%s)\n",            \
                         __FILE__, __LINE__, #a, #b);                     \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

/* Verify push succeeds and bump pass counter. */
#define PUSH_OK(list, val) CHECK((list).push_back(val).has_value())
#define PUSHF_OK(list, val) CHECK((list).push_front(val).has_value())

/* ------------------------------------------------------------------ */
/* TC-01  Default construction — empty list                             */
/* ------------------------------------------------------------------ */

static void tc01_empty(void)
{
    DList<int> l;
    CHECK(l.is_empty());
    CHECK_EQ(l.size(), 0u);
    CHECK(!l.pop_front().has_value());
    CHECK(!l.pop_back().has_value());
    CHECK(l.pop_front().error() == DListError::Empty);
}

/* ------------------------------------------------------------------ */
/* TC-02  Push front / back, peek                                       */
/* ------------------------------------------------------------------ */

static void tc02_push_peek(void)
{
    DList<int> l;
    PUSH_OK(l, 1);
    PUSH_OK(l, 2);
    PUSHF_OK(l, 0);

    CHECK_EQ(l.size(), 3u);
    CHECK_EQ(l.peek_front().value().get(), 0);
    CHECK_EQ(l.peek_back().value().get(),  2);
}

/* ------------------------------------------------------------------ */
/* TC-03  Pop front / back                                              */
/* ------------------------------------------------------------------ */

static void tc03_pop(void)
{
    DList<int> l;
    for (int i = 0; i < 5; i++) PUSH_OK(l, i);

    CHECK_EQ(l.pop_front().value(), 0);
    CHECK_EQ(l.pop_front().value(), 1);
    CHECK_EQ(l.pop_back().value(),  4);
    CHECK_EQ(l.pop_back().value(),  3);
    CHECK_EQ(l.size(), 1u);
}

/* ------------------------------------------------------------------ */
/* TC-04  get() bidirectional                                           */
/* ------------------------------------------------------------------ */

static void tc04_get(void)
{
    DList<int> l;
    for (int i = 0; i < 10; i++) PUSH_OK(l, i);

    for (int i = 0; i < 10; i++) {
        auto r = l.get((size_t)i);
        CHECK(r.has_value());
        CHECK_EQ(r.value().get(), i);
    }
    CHECK(!l.get(10).has_value());
    CHECK(l.get(10).error() == DListError::OutOfBound);
}

/* ------------------------------------------------------------------ */
/* TC-05  Bidirectional iterator                                        */
/* ------------------------------------------------------------------ */

static void tc05_iterator(void)
{
    DList<int> l;
    for (int i = 0; i < 5; i++) PUSH_OK(l, i);

    int k = 0;
    for (int v : l) CHECK_EQ(v, k++);

    k = 4;
    for (auto it = l.rbegin(); it != l.rend(); ++it) CHECK_EQ(*it, k--);
}

/* ------------------------------------------------------------------ */
/* TC-06  reverse()                                                     */
/* ------------------------------------------------------------------ */

static void tc06_reverse(void)
{
    DList<int> l;
    for (int i = 0; i < 5; i++) PUSH_OK(l, i);
    l.reverse();

    int k = 4;
    for (int v : l) CHECK_EQ(v, k--);
    CHECK_EQ(l.size(), 5u);
}

/* ------------------------------------------------------------------ */
/* TC-07  splice() O(1)                                                 */
/* ------------------------------------------------------------------ */

static void tc07_splice(void)
{
    DList<int> dst, src;
    for (int i = 0; i < 3; i++) PUSH_OK(dst, i);        /* 0,1,2   */
    for (int i = 10; i < 13; i++) PUSH_OK(src, i);      /* 10,11,12 */

    /* splice src before index 1 (val=1) */
    CHECK(dst.splice_at(1, src).has_value());
    CHECK_EQ(dst.size(), 6u);
    CHECK_EQ(src.size(), 0u);

    int expected[] = {0, 10, 11, 12, 1, 2};
    int k = 0;
    for (int v : dst) CHECK_EQ(v, expected[k++]);
}

/* ------------------------------------------------------------------ */
/* TC-08  Copy constructor (deep copy)                                  */
/* ------------------------------------------------------------------ */

static void tc08_copy(void)
{
    DList<int> a;
    for (int i = 0; i < 4; i++) PUSH_OK(a, i);

    DList<int> b(a);
    CHECK_EQ(b.size(), 4u);

    CHECK(a.pop_front().has_value()); /* discard value; just verify success */
    CHECK_EQ(b.peek_front().value().get(), 0);
}

/* ------------------------------------------------------------------ */
/* TC-09  Move constructor                                              */
/* ------------------------------------------------------------------ */

static void tc09_move(void)
{
    DList<int> a;
    for (int i = 0; i < 4; i++) PUSH_OK(a, i);

    DList<int> b(std::move(a));
    CHECK_EQ(b.size(), 4u);
    CHECK_EQ(b.peek_front().value().get(), 0);
}

/* ------------------------------------------------------------------ */
/* TC-10  Copy-and-swap assignment                                      */
/* ------------------------------------------------------------------ */

static void tc10_assign(void)
{
    DList<int> a, b;
    for (int i = 0; i < 3; i++) PUSH_OK(a, i);
    for (int i = 10; i < 13; i++) PUSH_OK(b, i);

    b = a;
    CHECK_EQ(b.size(), 3u);
    CHECK_EQ(b.peek_front().value().get(), 0);
}

/* ------------------------------------------------------------------ */
/* TC-11  Works with non-trivial type (std::string)                    */
/* ------------------------------------------------------------------ */

static void tc11_string(void)
{
    DList<std::string> l;
    CHECK(l.push_back("hello").has_value());
    CHECK(l.push_back("world").has_value());
    CHECK(l.push_front("start").has_value());

    CHECK_EQ(l.size(), 3u);
    CHECK(l.peek_front().value().get() == "start");
    CHECK(l.peek_back().value().get()  == "world");

    CHECK(l.pop_front().value() == "start");
}

/* ------------------------------------------------------------------ */
/* TC-12  clear()                                                       */
/* ------------------------------------------------------------------ */

static void tc12_clear(void)
{
    DList<int> l;
    for (int i = 0; i < 5; i++) PUSH_OK(l, i);
    l.clear();
    CHECK(l.is_empty());
    CHECK_EQ(l.size(), 0u);

    PUSH_OK(l, 99);
    CHECK_EQ(l.peek_front().value().get(), 99);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void)
{
    tc01_empty();
    tc02_push_peek();
    tc03_pop();
    tc04_get();
    tc05_iterator();
    tc06_reverse();
    tc07_splice();
    tc08_copy();
    tc09_move();
    tc10_assign();
    tc11_string();
    tc12_clear();

    std::printf("DList C++:  %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

/**
 * test_stack.cpp — C++ Stack 功能测试
 *
 * 编译：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_stack tests/test_stack.cpp
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_stack
 *   g++ -fsanitize=address,undefined ... && ./tests/test_stack
 */

#include "Stack.hpp"

#include <cstdio>
#include <string>

using namespace algo;

/* ------------------------------------------------------------------ */
/*  测试工具宏                                                          */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

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

/* ================================================================== */
/*  ArrayStack 测试用例                                                 */
/* ================================================================== */

/* TC-01  LIFO 顺序 */
static void atc01_lifo()
{
    ArrayStack<int> s;
    for (int i = 1; i <= 5; i++)
        CHECK(s.push(i).has_value());

    CHECK_EQ(s.len(), 5u);

    for (int i = 5; i >= 1; i--) {
        auto r = s.pop();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }

    CHECK(s.is_empty());
}

/* TC-02  pop 空栈返回 Empty */
static void atc02_pop_empty()
{
    ArrayStack<int> s;
    auto r = s.pop();
    CHECK(!r.has_value());
    CHECK(r.error() == StackError::Empty);
}

/* TC-03  peek 不改变 len */
static void atc03_peek_len()
{
    ArrayStack<int> s;
    (void)s.push(42);

    auto r = s.peek();
    CHECK(r.has_value());
    CHECK_EQ(r->get(), 42);
    CHECK_EQ(s.len(), 1u);

}

/* TC-04  触发扩容后所有元素仍正确 */
static void atc04_grow()
{
    ArrayStack<int> s(2);   /* 初始容量 2 */
    for (int i = 0; i < 16; i++)
        s.push(i);

    CHECK_EQ(s.len(), 16u);

    for (int i = 15; i >= 0; i--) {
        auto r = s.pop();
        CHECK_EQ(*r, i);
    }
}

/* TC-05  reserve 后不触发扩容 */
static void atc05_reserve()
{
    ArrayStack<int> s;
    CHECK(s.reserve(64).has_value());
    size_t cap = s.len();   /* len == 0；实际检验是 push 不出错 */
    (void)cap;

    for (int i = 0; i < 64; i++)
        CHECK(s.push(i).has_value());

    CHECK_EQ(s.len(), 64u);
}

/* TC-06  clear */
static void atc06_clear()
{
    ArrayStack<int> s;
    for (int i = 0; i < 8; i++)
        s.push(i);

    s.clear();
    CHECK(s.is_empty());
    CHECK_EQ(s.len(), 0u);
}

/* TC-07  shrink_to_fit */
static void atc07_shrink()
{
    ArrayStack<int> s(32);
    s.push(1);
    CHECK(s.shrink_to_fit().has_value());
    CHECK_EQ(s.len(), 1u);
}

/* TC-08  拷贝：修改副本不影响原栈 */
static void atc08_copy()
{
    ArrayStack<int> orig;
    orig.push(1);
    orig.push(2);

    ArrayStack<int> copy = orig;
    copy.push(99);

    CHECK_EQ(orig.len(), 2u);
    CHECK_EQ(copy.len(), 3u);

    auto top = orig.peek();
    CHECK_EQ(top->get(), 2);
}

/* TC-09  移动后原栈为空 */
static void atc09_move()
{
    ArrayStack<int> s;
    s.push(10);
    s.push(20);

    ArrayStack<int> moved = std::move(s);

    CHECK(s.is_empty());
    CHECK_EQ(moved.len(), 2u);
    CHECK_EQ(moved.pop().value(), 20);
}

/* TC-10  string 元素 */
static void atc10_string()
{
    ArrayStack<std::string> s;
    s.push("hello");
    s.push("world");

    CHECK_EQ(s.pop().value(), "world");
    CHECK_EQ(s.pop().value(), "hello");
    CHECK(s.is_empty());
}

/* ================================================================== */
/*  ListStack 测试用例                                                  */
/* ================================================================== */

/* TC-01  LIFO 顺序 */
static void ltc01_lifo()
{
    ListStack<int> s;
    for (int i = 1; i <= 5; i++)
        CHECK(s.push(i).has_value());

    CHECK_EQ(s.len(), 5u);

    for (int i = 5; i >= 1; i--) {
        auto r = s.pop();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }

    CHECK(s.is_empty());
}

/* TC-02  pop 空栈返回 Empty */
static void ltc02_pop_empty()
{
    ListStack<int> s;
    auto r = s.pop();
    CHECK(!r.has_value());
    CHECK(r.error() == StackError::Empty);
}

/* TC-03  peek 不改变 len */
static void ltc03_peek_len()
{
    ListStack<int> s;
    s.push(99);

    auto r = s.peek();
    CHECK(r.has_value());
    CHECK_EQ(r->get(), 99);
    CHECK_EQ(s.len(), 1u);
}

/* TC-04  push/pop 交替，len 始终准确 */
static void ltc04_interleave()
{
    ListStack<int> s;
    s.push(1);
    s.push(2);
    CHECK_EQ(s.len(), 2u);

    s.pop();
    CHECK_EQ(s.len(), 1u);

    s.push(3);
    s.push(4);
    CHECK_EQ(s.len(), 3u);

    while (!s.is_empty()) s.pop();
    CHECK_EQ(s.len(), 0u);
}

/* TC-05  clear */
static void ltc05_clear()
{
    ListStack<int> s;
    for (int i = 0; i < 5; i++)
        s.push(i);

    s.clear();
    CHECK(s.is_empty());
    CHECK_EQ(s.len(), 0u);
}

/* TC-06  拷贝：修改副本不影响原栈 */
static void ltc06_copy()
{
    ListStack<int> orig;
    orig.push(1);
    orig.push(2);

    ListStack<int> copy = orig;
    copy.push(99);

    CHECK_EQ(orig.len(), 2u);
    CHECK_EQ(copy.len(), 3u);

    CHECK_EQ(orig.peek()->get(), 2);
}

/* TC-07  移动后原栈为空 */
static void ltc07_move()
{
    ListStack<int> s;
    s.push(10);
    s.push(20);

    ListStack<int> moved = std::move(s);

    CHECK(s.is_empty());
    CHECK_EQ(moved.len(), 2u);
    CHECK_EQ(moved.pop().value(), 20);
}

/* TC-08  string 元素（RAII 验证） */
static void ltc08_string()
{
    ListStack<std::string> s;
    s.push("alpha");
    s.push("beta");

    CHECK_EQ(s.pop().value(), "beta");
    CHECK_EQ(s.pop().value(), "alpha");
    CHECK(s.is_empty());
}

/* ================================================================== */
/*  等价性测试                                                          */
/* ================================================================== */

static void tc_equivalence()
{
    ArrayStack<int> as;
    ListStack<int>  ls;

    for (int i = 1; i <= 10; i++) {
        as.push(i);
        ls.push(i);
    }

    for (int i = 10; i >= 1; i--) {
        auto av = as.pop().value();
        auto lv = ls.pop().value();
        CHECK_EQ(av, lv);
        CHECK_EQ(av, i);
    }

    CHECK(as.is_empty());
    CHECK(ls.is_empty());
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main()
{
    std::printf("=== ArrayStack ===\n");
    atc01_lifo();
    atc02_pop_empty();
    atc03_peek_len();
    atc04_grow();
    atc05_reserve();
    atc06_clear();
    atc07_shrink();
    atc08_copy();
    atc09_move();
    atc10_string();

    std::printf("=== ListStack ===\n");
    ltc01_lifo();
    ltc02_pop_empty();
    ltc03_peek_len();
    ltc04_interleave();
    ltc05_clear();
    ltc06_copy();
    ltc07_move();
    ltc08_string();

    std::printf("=== Equivalence ===\n");
    tc_equivalence();

    std::printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

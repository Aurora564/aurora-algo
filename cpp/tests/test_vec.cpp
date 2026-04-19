/**
 * test_vec.cpp — C++ Vec 功能测试
 *
 * 编译（C++23）：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -fsanitize=address,undefined \
 *       -o tests/test_vec tests/test_vec.cpp
 */

#include "Vec.hpp"
#include <cassert>
#include <cstdio>
#include <string>

/* ------------------------------------------------------------------ */
/*  测试工具                                                            */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                    \
    do {                                                               \
        if (expr) {                                                    \
            std::printf("  PASS  %s\n", #expr);                       \
            ++g_pass;                                                  \
        } else {                                                       \
            std::printf("  FAIL  %s  (%s:%d)\n",                      \
                        #expr, __FILE__, __LINE__);                    \
            ++g_fail;                                                  \
        }                                                              \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

/* ------------------------------------------------------------------ */
/*  TC-01  push 直到扩容，所有元素值正确                                 */
/* ------------------------------------------------------------------ */
static void tc01_push_grow()
{
    std::puts("TC-01: push until grow");
    Vec<int> v(2);
    for (int i = 0; i < 10; i++) CHECK(v.push(i));

    CHECK_EQ(v.len(), 10u);
    for (size_t i = 0; i < 10; i++) {
        auto r = v.get(i);
        CHECK(r.has_value());
        CHECK_EQ(r.value().get(), (int)i);
    }
}

/* ------------------------------------------------------------------ */
/*  TC-02  pop 至空返回 Empty                                           */
/* ------------------------------------------------------------------ */
static void tc02_pop_empty()
{
    std::puts("TC-02: pop to empty");
    Vec<int> v;
    CHECK(v.push(42));
    auto r = v.pop();
    CHECK(r.has_value());
    CHECK_EQ(*r, 42);

    auto r2 = v.pop();
    CHECK(!r2.has_value());
    CHECK(r2.error() == VecError::Empty);
}

/* ------------------------------------------------------------------ */
/*  TC-03/04  get 边界                                                  */
/* ------------------------------------------------------------------ */
static void tc03_04_get_bounds()
{
    std::puts("TC-03/04: get bounds");
    Vec<int> v;
    for (int i = 0; i < 5; i++) v.push(i);

    CHECK_EQ(v.get(0)->get(), 0);
    CHECK_EQ(v.get(4)->get(), 4);
    CHECK(!v.get(5).has_value());
    CHECK(v.get(5).error() == VecError::OutOfBounds);
}

/* ------------------------------------------------------------------ */
/*  TC-05  insert(0, x) 原元素后移                                      */
/* ------------------------------------------------------------------ */
static void tc05_insert_head()
{
    std::puts("TC-05: insert at head");
    Vec<int> v;
    for (int i = 1; i <= 3; i++) v.push(i);    // [1,2,3]
    CHECK(v.insert(0, 0));                       // [0,1,2,3]
    CHECK_EQ(v.len(), 4u);
    for (size_t i = 0; i < 4; i++)
        CHECK_EQ(v.get(i)->get(), (int)i);
}

/* ------------------------------------------------------------------ */
/*  TC-06  remove(0) 原元素前移                                         */
/* ------------------------------------------------------------------ */
static void tc06_remove_head()
{
    std::puts("TC-06: remove at head");
    Vec<int> v;
    for (int i = 0; i < 4; i++) v.push(i);     // [0,1,2,3]
    auto r = v.remove(0);
    CHECK(r.has_value());
    CHECK_EQ(*r, 0);
    CHECK_EQ(v.len(), 3u);
    for (size_t i = 0; i < 3; i++)
        CHECK_EQ(v.get(i)->get(), (int)(i + 1));
}

/* ------------------------------------------------------------------ */
/*  TC-07  swap_remove：末尾元素填补 i                                  */
/* ------------------------------------------------------------------ */
static void tc07_swap_remove()
{
    std::puts("TC-07: swap_remove");
    Vec<int> v;
    for (int i = 0; i < 5; i++) v.push(i);     // [0,1,2,3,4]
    auto r = v.swap_remove(1);
    CHECK(r.has_value());
    CHECK_EQ(*r, 1);
    CHECK_EQ(v.len(), 4u);
    CHECK_EQ(v.get(1)->get(), 4);
}

/* ------------------------------------------------------------------ */
/*  TC-08  拷贝构造后修改副本，源不受影响                                */
/* ------------------------------------------------------------------ */
static void tc08_copy_independence()
{
    std::puts("TC-08: copy independence");
    Vec<int> src;
    for (int i = 0; i < 3; i++) src.push(i);

    Vec<int> dst(src);           // 深拷贝（调用 T 的拷贝构造）
    CHECK(dst.set(0, 99));
    CHECK_EQ(src.get(0)->get(), 0);
}

/* ------------------------------------------------------------------ */
/*  TC-09  移动构造后源为空                                              */
/* ------------------------------------------------------------------ */
static void tc09_move()
{
    std::puts("TC-09: move constructor");
    Vec<int> src;
    for (int i = 0; i < 3; i++) src.push(i);

    Vec<int> dst(std::move(src));
    CHECK_EQ(src.len(), 0u);
    CHECK_EQ(dst.len(), 3u);
    CHECK_EQ(dst.get(2)->get(), 2);
}

/* ------------------------------------------------------------------ */
/*  TC-10  Rule of Five：赋值运算符（copy-and-swap）                    */
/* ------------------------------------------------------------------ */
static void tc10_copy_assign()
{
    std::puts("TC-10: copy assign (copy-and-swap)");
    Vec<int> a, b;
    for (int i = 0; i < 5; i++) a.push(i);
    b = a;
    CHECK_EQ(b.len(), 5u);
    b.set(0, 99);
    CHECK_EQ(a.get(0)->get(), 0); // 源不受影响
}

/* ------------------------------------------------------------------ */
/*  TC-11  reserve & shrink_to_fit                                      */
/* ------------------------------------------------------------------ */
static void tc11_reserve_shrink()
{
    std::puts("TC-11: reserve & shrink_to_fit");
    Vec<int> v;
    CHECK(v.reserve(50));
    CHECK(v.cap() >= 50u);
    for (int i = 0; i < 5; i++) v.push(i);
    CHECK(v.shrink_to_fit());
    CHECK_EQ(v.cap(), 5u);
}

/* ------------------------------------------------------------------ */
/*  TC-12  clear：len == 0，cap 不变                                    */
/* ------------------------------------------------------------------ */
static void tc12_clear()
{
    std::puts("TC-12: clear");
    Vec<std::string> v;
    v.push("hello");
    v.push("world");
    size_t old_cap = v.cap();
    v.clear();
    CHECK_EQ(v.len(), 0u);
    CHECK_EQ(v.cap(), old_cap);
}

/* ------------------------------------------------------------------ */
/*  TC-13  自定义扩容策略 ×1.5                                          */
/* ------------------------------------------------------------------ */
static void tc13_custom_grow()
{
    std::puts("TC-13: custom grow x1.5");
    auto grow15 = [](size_t cap) -> size_t {
        return cap + cap / 2 + 1;
    };
    Vec<int> v(2, grow15);
    CHECK_EQ(v.cap(), 2u);
    for (int i = 0; i < 3; i++) v.push(i);
    CHECK(v.cap() == grow15(2));
}

/* ------------------------------------------------------------------ */
/*  额外：extend & as_slice & foreach                                   */
/* ------------------------------------------------------------------ */
static void tc_extra()
{
    std::puts("TC-extra: extend / as_slice / foreach");
    Vec<int> v;
    int src[] = {1, 2, 3};
    CHECK(v.extend(src, 3));
    CHECK_EQ(v.len(), 3u);

    int sum = 0;
    v.foreach([&](int &x) { sum += x; });
    CHECK_EQ(sum, 6);

    const int *sl = v.as_slice();
    CHECK_EQ(sl[0], 1);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main()
{
    std::puts("=== Vec C++ Tests ===");
    tc01_push_grow();
    tc02_pop_empty();
    tc03_04_get_bounds();
    tc05_insert_head();
    tc06_remove_head();
    tc07_swap_remove();
    tc08_copy_independence();
    tc09_move();
    tc10_copy_assign();
    tc11_reserve_shrink();
    tc12_clear();
    tc13_custom_grow();
    tc_extra();

    std::printf("\n=== Result: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

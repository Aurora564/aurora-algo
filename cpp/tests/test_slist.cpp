/**
 * test_slist.cpp — C++ SList 功能测试
 *
 * 编译（C++23）：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -fsanitize=address,undefined \
 *       -o tests/test_slist tests/test_slist.cpp
 */

#include "SList.hpp"
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
/*  TC-01  push_back N 个元素，pop_front 验证 FIFO 顺序               */
/* ------------------------------------------------------------------ */
static void tc01_fifo()
{
    std::puts("TC-01: push_back + pop_front FIFO");
    SList<int> list;

    for (int i = 0; i < 5; i++) CHECK(list.push_back(i));

    CHECK_EQ(list.len(), 5u);
    for (int i = 0; i < 5; i++) {
        auto r = list.pop_front();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }
    CHECK_EQ(list.len(), 0u);
}

/* ------------------------------------------------------------------ */
/*  TC-02  push_front N 个元素，pop_front 验证 LIFO 顺序              */
/* ------------------------------------------------------------------ */
static void tc02_lifo()
{
    std::puts("TC-02: push_front + pop_front LIFO");
    SList<int> list;

    for (int i = 0; i < 5; i++) CHECK(list.push_front(i)); /* 链表: 4→3→2→1→0 */

    for (int i = 4; i >= 0; i--) {
        auto r = list.pop_front();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }
}

/* ------------------------------------------------------------------ */
/*  TC-03  pop_front / pop_back 空链表返回 Empty                      */
/* ------------------------------------------------------------------ */
static void tc03_pop_empty()
{
    std::puts("TC-03: pop empty");
    SList<int> list;

    auto r1 = list.pop_front();
    CHECK(!r1.has_value());
    CHECK(r1.error() == SListError::Empty);

    auto r2 = list.pop_back();
    CHECK(!r2.has_value());
    CHECK(r2.error() == SListError::Empty);

    auto r3 = list.peek_front();
    CHECK(!r3.has_value());
    CHECK(r3.error() == SListError::Empty);

    auto r4 = list.peek_back();
    CHECK(!r4.has_value());
    CHECK(r4.error() == SListError::Empty);
}

/* ------------------------------------------------------------------ */
/*  TC-04  get / peek 边界                                             */
/* ------------------------------------------------------------------ */
static void tc04_get_peek()
{
    std::puts("TC-04: get & peek bounds");
    SList<int> list;
    for (int i = 10; i <= 30; i += 10) CHECK(list.push_back(i)); /* [10, 20, 30] */

    CHECK_EQ(list.get(0)->get(), 10);
    CHECK_EQ(list.get(2)->get(), 30);
    CHECK(!list.get(3).has_value());
    CHECK(list.get(3).error() == SListError::OutOfBounds);

    CHECK_EQ(list.peek_front()->get(), 10);
    CHECK_EQ(list.peek_back()->get(),  30);
}

/* ------------------------------------------------------------------ */
/*  TC-05  pop_back：正确返回尾部值                                    */
/* ------------------------------------------------------------------ */
static void tc05_pop_back()
{
    std::puts("TC-05: pop_back");
    SList<int> list;
    for (int i = 1; i <= 3; i++) CHECK(list.push_back(i));    /* [1, 2, 3] */

    auto r = list.pop_back();
    CHECK(r.has_value());
    CHECK_EQ(*r, 3);
    CHECK_EQ(list.len(), 2u);

    /* 弹至只剩一个节点 */
    CHECK(list.pop_back().has_value());
    r = list.pop_back();
    CHECK(r.has_value());
    CHECK_EQ(*r, 1);
    CHECK(list.is_empty());

    /* 空链表 pop_back */
    r = list.pop_back();
    CHECK(!r.has_value());
    CHECK(r.error() == SListError::Empty);
}

/* ------------------------------------------------------------------ */
/*  TC-06  insert_after / remove_after                                 */
/* ------------------------------------------------------------------ */
static void tc06_insert_remove_after()
{
    std::puts("TC-06: insert_after & remove_after");
    SList<int> list;
    CHECK(list.push_back(1));
    CHECK(list.push_back(3));      /* [1, 3] */

    /* insert_after(head, 2) → [1, 2, 3] */
    auto *head = list.head();
    CHECK(list.insert_after(head, 2));
    CHECK_EQ(list.len(), 3u);
    CHECK_EQ(list.get(1)->get(), 2);

    /* remove_after(head) → 移除值 2 → [1, 3] */
    auto r = list.remove_after(head);
    CHECK(r.has_value());
    CHECK_EQ(*r, 2);
    CHECK_EQ(list.len(), 2u);

    /* remove_after(tail) → OutOfBounds */
    auto r2 = list.remove_after(list.tail());
    CHECK(!r2.has_value());
    CHECK(r2.error() == SListError::OutOfBounds);

    /* insert_after(nullptr) 等价于 push_front */
    CHECK(list.insert_after(nullptr, 0));   /* [0, 1, 3] */
    CHECK_EQ(list.get(0)->get(), 0);
}

/* ------------------------------------------------------------------ */
/*  TC-07  reverse                                                     */
/* ------------------------------------------------------------------ */
static void tc07_reverse()
{
    std::puts("TC-07: reverse");
    SList<int> list;
    for (int i = 1; i <= 5; i++) CHECK(list.push_back(i)); /* [1,2,3,4,5] */

    list.reverse(); /* [5,4,3,2,1] */

    for (size_t i = 0; i < 5; i++)
        CHECK_EQ(list.get(i)->get(), (int)(5 - i));

    CHECK_EQ(list.peek_back()->get(), 1); /* tail 正确 */

    /* 空链表 reverse 不崩溃 */
    SList<int> empty;
    empty.reverse();
    CHECK(empty.is_empty());

    /* 单节点 reverse */
    SList<int> single;
    CHECK(single.push_back(42));
    single.reverse();
    CHECK_EQ(single.peek_front()->get(), 42);
}

/* ------------------------------------------------------------------ */
/*  TC-08  find                                                        */
/* ------------------------------------------------------------------ */
static void tc08_find()
{
    std::puts("TC-08: find");
    SList<int> list;
    for (int i = 10; i <= 50; i += 10) CHECK(list.push_back(i)); /* [10,20,30,40,50] */

    auto *node = list.find([](const int &v) { return v == 30; });
    CHECK(node != nullptr);
    CHECK_EQ(node->val, 30);

    auto *not_found = list.find([](const int &v) { return v == 99; });
    CHECK(not_found == nullptr);
}

/* ------------------------------------------------------------------ */
/*  TC-09  拷贝构造：深拷贝，修改副本不影响原链表                        */
/* ------------------------------------------------------------------ */
static void tc09_copy_independence()
{
    std::puts("TC-09: copy independence");
    SList<std::string> src;
    CHECK(src.push_back("hello"));
    CHECK(src.push_back("world"));

    SList<std::string> dst(src);
    dst.get(0)->get() = "changed";

    CHECK_EQ(src.get(0)->get(), "hello");   /* 原链表不受影响 */
    CHECK_EQ(dst.len(), src.len());
}

/* ------------------------------------------------------------------ */
/*  TC-10  移动构造：src 变空，dst 包含所有节点                         */
/* ------------------------------------------------------------------ */
static void tc10_move()
{
    std::puts("TC-10: move constructor");
    SList<int> src;
    CHECK(src.push_back(1));
    CHECK(src.push_back(2));
    CHECK(src.push_back(3));

    SList<int> dst(std::move(src));
    CHECK_EQ(src.len(), 0u);
    CHECK(src.is_empty());
    CHECK_EQ(dst.len(), 3u);
    CHECK_EQ(dst.get(2)->get(), 3);
}

/* ------------------------------------------------------------------ */
/*  TC-11  copy-and-swap 赋值                                          */
/* ------------------------------------------------------------------ */
static void tc11_copy_assign()
{
    std::puts("TC-11: copy assign");
    SList<int> a, b;
    for (int i = 0; i < 5; i++) CHECK(a.push_back(i));

    b = a;
    b.get(0)->get() = 99;
    CHECK_EQ(a.get(0)->get(), 0); /* 源不受影响 */
    CHECK_EQ(b.len(), 5u);
}

/* ------------------------------------------------------------------ */
/*  TC-12  clear：len == 0                                             */
/* ------------------------------------------------------------------ */
static void tc12_clear()
{
    std::puts("TC-12: clear");
    SList<int> list;
    for (int i = 0; i < 5; i++) CHECK(list.push_back(i));
    list.clear();
    CHECK_EQ(list.len(), 0u);
    CHECK(list.is_empty());
    CHECK(list.peek_front().error() == SListError::Empty);
}

/* ------------------------------------------------------------------ */
/*  TC-13  range-based for                                             */
/* ------------------------------------------------------------------ */
static void tc13_range_for()
{
    std::puts("TC-13: range-based for");
    SList<int> list;
    for (int i = 1; i <= 4; i++) CHECK(list.push_back(i));

    int sum = 0;
    for (int v : list) sum += v;
    CHECK_EQ(sum, 10);
}

/* ------------------------------------------------------------------ */
/*  TC-14  foreach                                                     */
/* ------------------------------------------------------------------ */
static void tc14_foreach()
{
    std::puts("TC-14: foreach");
    SList<int> list;
    for (int i = 1; i <= 4; i++) CHECK(list.push_back(i));

    int product = 1;
    list.foreach([&](const int &v) { product *= v; });
    CHECK_EQ(product, 24);
}

/* ------------------------------------------------------------------ */
/*  TC-15  move 赋值                                                    */
/* ------------------------------------------------------------------ */
static void tc15_move_assign()
{
    std::puts("TC-15: move assign");
    SList<int> a, b;
    for (int i = 0; i < 3; i++) CHECK(a.push_back(i));
    b = std::move(a);
    CHECK(a.is_empty());
    CHECK_EQ(b.len(), 3u);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main()
{
    std::puts("=== SList C++ Tests ===");
    tc01_fifo();
    tc02_lifo();
    tc03_pop_empty();
    tc04_get_peek();
    tc05_pop_back();
    tc06_insert_remove_after();
    tc07_reverse();
    tc08_find();
    tc09_copy_independence();
    tc10_move();
    tc11_copy_assign();
    tc12_clear();
    tc13_range_for();
    tc14_foreach();
    tc15_move_assign();

    std::printf("\n=== Result: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

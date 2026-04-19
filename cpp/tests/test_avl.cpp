/**
 * test_avl.cpp — C++ AVL 功能测试
 *
 * 编译：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_avl tests/test_avl.cpp
 */

#include "AVL.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace algo;

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                       \
    do {                                                                  \
        if (expr) {                                                       \
            std::printf("  PASS  %s\n", #expr);                          \
            ++g_pass;                                                     \
        } else {                                                          \
            std::printf("  FAIL  %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_fail;                                                     \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

/* ------------------------------------------------------------------ */
/*  TC-01  空树 search → nullopt                                       */
/* ------------------------------------------------------------------ */
static void tc01_empty_search()
{
    std::puts("\n[TC-01] empty search");
    AVL<int, std::string> t;
    CHECK(!t.search(1).has_value());
    CHECK(t.is_empty());
}

/* ------------------------------------------------------------------ */
/*  TC-02  insert 5 无序键，in_order 严格升序                          */
/* ------------------------------------------------------------------ */
static void tc02_in_order_ascending()
{
    std::puts("\n[TC-02] in_order ascending");
    AVL<int, int> t;
    for (int k : {3, 1, 4, 5, 2}) t.insert(k, k);

    std::vector<int> got;
    t.in_order([&](const int& k, const int&) { got.push_back(k); });
    std::vector<int> want = {1, 2, 3, 4, 5};
    CHECK(got == want);
}

/* ------------------------------------------------------------------ */
/*  TC-03  search 存在键                                               */
/* ------------------------------------------------------------------ */
static void tc03_search_found()
{
    std::puts("\n[TC-03] search found");
    AVL<int, int> t;
    t.insert(42, 99);
    auto v = t.search(42);
    CHECK(v.has_value());
    CHECK_EQ(v->get(), 99);
}

/* ------------------------------------------------------------------ */
/*  TC-04  search 不存在键                                             */
/* ------------------------------------------------------------------ */
static void tc04_search_missing()
{
    std::puts("\n[TC-04] search missing");
    AVL<int, int> t;
    t.insert(1, 1);
    CHECK(!t.search(99).has_value());
}

/* ------------------------------------------------------------------ */
/*  TC-05  delete 叶节点                                               */
/* ------------------------------------------------------------------ */
static void tc05_delete_leaf()
{
    std::puts("\n[TC-05] delete leaf");
    AVL<int, int> t;
    for (int k : {5, 3, 7}) t.insert(k, k);
    CHECK(t.remove(3));
    CHECK_EQ(t.len(), 2u);
    CHECK(t.is_balanced());
    CHECK(t.is_valid_bst());
    CHECK(!t.contains(3));
}

/* ------------------------------------------------------------------ */
/*  TC-06  delete 单子节点                                             */
/* ------------------------------------------------------------------ */
static void tc06_delete_one_child()
{
    std::puts("\n[TC-06] delete one child");
    AVL<int, int> t;
    for (int k : {5, 3, 7, 6}) t.insert(k, k);
    CHECK(t.remove(7));
    CHECK_EQ(t.len(), 3u);
    CHECK(t.is_balanced());
    CHECK(t.is_valid_bst());
    CHECK(!t.contains(7));
}

/* ------------------------------------------------------------------ */
/*  TC-07  delete 双子节点                                             */
/* ------------------------------------------------------------------ */
static void tc07_delete_two_children()
{
    std::puts("\n[TC-07] delete two children");
    AVL<int, int> t;
    for (int k : {5, 3, 7, 1, 4, 6, 9}) t.insert(k, k);
    CHECK(t.remove(5));
    CHECK_EQ(t.len(), 6u);
    CHECK(t.is_balanced());
    CHECK(t.is_valid_bst());

    std::vector<int> got;
    t.in_order([&](const int& k, const int&) { got.push_back(k); });
    CHECK((got == std::vector<int>{1, 3, 4, 6, 7, 9}));
}

/* ------------------------------------------------------------------ */
/*  TC-08  delete 根节点                                               */
/* ------------------------------------------------------------------ */
static void tc08_delete_root()
{
    std::puts("\n[TC-08] delete root");
    {
        AVL<int, int> t;
        t.insert(1, 1);
        t.remove(1);
        CHECK(t.is_empty());
        CHECK_EQ(t.len(), 0u);
    }
    {
        AVL<int, int> t;
        t.insert(5, 5); t.insert(3, 3);
        t.remove(5);
        CHECK_EQ(t.len(), 1u);
        CHECK(t.is_balanced());
        CHECK(t.is_valid_bst());
    }
    {
        AVL<int, int> t;
        for (int k : {5, 2, 8}) t.insert(k, k);
        t.remove(5);
        CHECK_EQ(t.len(), 2u);
        CHECK(t.is_balanced());
        CHECK(t.is_valid_bst());
    }
}

/* ------------------------------------------------------------------ */
/*  TC-09  重复 insert → len 不变，value 更新                          */
/* ------------------------------------------------------------------ */
static void tc09_duplicate_insert()
{
    std::puts("\n[TC-09] duplicate insert");
    AVL<int, int> t;
    t.insert(1, 10);
    t.insert(1, 20);
    CHECK_EQ(t.len(), 1u);
    CHECK_EQ(t.search(1)->get(), 20);
}

/* ------------------------------------------------------------------ */
/*  TC-10  min / max                                                   */
/* ------------------------------------------------------------------ */
static void tc10_min_max()
{
    std::puts("\n[TC-10] min / max");
    AVL<int, int> t;
    for (int k : {5, 3, 7, 1, 9}) t.insert(k, k);

    auto mn = t.min();
    CHECK(mn.has_value());
    CHECK_EQ(mn->first, 1);

    auto mx = t.max();
    CHECK(mx.has_value());
    CHECK_EQ(mx->first, 9);

    AVL<int, int> empty;
    CHECK(!empty.min().has_value());
    CHECK(!empty.max().has_value());
}

/* ------------------------------------------------------------------ */
/*  TC-11  顺序插入 1..1000，height ≤ 22，树保持平衡                  */
/* ------------------------------------------------------------------ */
static void tc11_sequential_insert()
{
    std::puts("\n[TC-11] sequential insert stays balanced");
    AVL<int, int> t;
    for (int i = 1; i <= 1000; ++i) t.insert(i, i);

    CHECK_EQ(t.len(), 1000u);
    CHECK(t.height() <= 22u);
    CHECK(t.is_balanced());
    CHECK(t.is_valid_bst());
}

/* ------------------------------------------------------------------ */
/*  TC-12  混合 insert/delete，不变式保持                              */
/* ------------------------------------------------------------------ */
static void tc12_mixed_ops()
{
    std::puts("\n[TC-12] mixed insert/delete invariants");
    AVL<int, int> t;
    for (int k : {10, 5, 15, 3, 7, 12, 20, 1, 4, 6, 8, 11, 13, 18, 25}) {
        t.insert(k, k);
        CHECK(t.is_balanced());
        CHECK(t.is_valid_bst());
    }
    for (int k : {5, 15, 1, 20, 10}) {
        t.remove(k);
        CHECK(t.is_balanced());
        CHECK(t.is_valid_bst());
    }
}

/* ------------------------------------------------------------------ */
/*  TC-13  copy constructor keeps invariants                           */
/* ------------------------------------------------------------------ */
static void tc13_copy()
{
    std::puts("\n[TC-13] copy constructor");
    AVL<int, int> t;
    for (int k : {5, 3, 7, 1, 9}) t.insert(k, k);
    AVL<int, int> t2(t);
    CHECK_EQ(t2.len(), t.len());
    CHECK(t2.is_balanced());
    CHECK(t2.is_valid_bst());
    t2.remove(5);
    CHECK(t.contains(5));   /* original unaffected */
}

/* ------------------------------------------------------------------ */
/*  TC-14  delete 不存在键 → false，len 不变                          */
/* ------------------------------------------------------------------ */
static void tc14_delete_missing()
{
    std::puts("\n[TC-14] delete missing");
    AVL<int, int> t;
    t.insert(1, 1);
    CHECK(!t.remove(99));
    CHECK_EQ(t.len(), 1u);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int main()
{
    tc01_empty_search();
    tc02_in_order_ascending();
    tc03_search_found();
    tc04_search_missing();
    tc05_delete_leaf();
    tc06_delete_one_child();
    tc07_delete_two_children();
    tc08_delete_root();
    tc09_duplicate_insert();
    tc10_min_max();
    tc11_sequential_insert();
    tc12_mixed_ops();
    tc13_copy();
    tc14_delete_missing();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

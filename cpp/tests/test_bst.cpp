/**
 * test_bst.cpp — C++ BST 功能测试
 *
 * 编译（C++23）：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -fsanitize=address,undefined \
 *       -o tests/test_bst tests/test_bst.cpp
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_bst
 */

#include "BST.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>

using namespace algo;

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
/*  TC-01  空树 search → nullopt                                        */
/* ------------------------------------------------------------------ */
static void tc01_empty_search()
{
    std::puts("\n[TC-01] empty search");
    BST<int, int> t;
    CHECK(!t.search(1).has_value());
    CHECK(t.is_empty());
}

/* ------------------------------------------------------------------ */
/*  TC-02  insert 5 无序键，in_order 严格升序                           */
/* ------------------------------------------------------------------ */
static void tc02_in_order_ascending()
{
    std::puts("\n[TC-02] in_order ascending");
    BST<int, int> t;
    for (int k : {3, 1, 4, 5, 2}) t.insert(k, k);

    std::vector<int> keys;
    t.in_order([&](const int& k, int&) { keys.push_back(k); });

    CHECK_EQ(keys.size(), 5u);
    CHECK(std::is_sorted(keys.begin(), keys.end()));
    bool strict = true;
    for (std::size_t i = 1; i < keys.size(); i++)
        if (keys[i] <= keys[i-1]) { strict = false; break; }
    CHECK(strict);
}

/* ------------------------------------------------------------------ */
/*  TC-03  search 存在键 → has_value + 正确 value                       */
/* ------------------------------------------------------------------ */
static void tc03_search_found()
{
    std::puts("\n[TC-03] search found");
    BST<int, int> t;
    t.insert(42, 99);
    auto r = t.search(42);
    CHECK(r.has_value());
    CHECK_EQ(r->get(), 99);
}

/* ------------------------------------------------------------------ */
/*  TC-04  search 不存在键 → nullopt                                    */
/* ------------------------------------------------------------------ */
static void tc04_search_missing()
{
    std::puts("\n[TC-04] search missing");
    BST<int, int> t;
    t.insert(1, 1);
    CHECK(!t.search(99).has_value());
}

/* ------------------------------------------------------------------ */
/*  TC-05  delete 叶节点                                                */
/* ------------------------------------------------------------------ */
static void tc05_delete_leaf()
{
    std::puts("\n[TC-05] delete leaf");
    BST<int, int> t;
    for (int k : {5, 3, 7}) t.insert(k, k);
    CHECK(t.remove(3));
    CHECK_EQ(t.len(), 2u);
    CHECK(t.is_valid());
    CHECK(!t.contains(3));
}

/* ------------------------------------------------------------------ */
/*  TC-06  delete 单子节点                                              */
/* ------------------------------------------------------------------ */
static void tc06_delete_one_child()
{
    std::puts("\n[TC-06] delete one child");
    BST<int, int> t;
    for (int k : {5, 3, 7, 6}) t.insert(k, k);
    CHECK(t.remove(7));
    CHECK_EQ(t.len(), 3u);
    CHECK(t.is_valid());
    CHECK(!t.contains(7));
}

/* ------------------------------------------------------------------ */
/*  TC-07  delete 双子节点                                              */
/* ------------------------------------------------------------------ */
static void tc07_delete_two_children()
{
    std::puts("\n[TC-07] delete two children");
    BST<int, int> t;
    for (int k : {5, 3, 7, 1, 4, 6, 9}) t.insert(k, k);
    CHECK(t.remove(5));
    CHECK_EQ(t.len(), 6u);
    CHECK(t.is_valid());

    std::vector<int> keys;
    t.in_order([&](const int& k, int&) { keys.push_back(k); });
    std::vector<int> want = {1, 3, 4, 6, 7, 9};
    CHECK(keys == want);
}

/* ------------------------------------------------------------------ */
/*  TC-08  delete 根节点                                                */
/* ------------------------------------------------------------------ */
static void tc08_delete_root()
{
    std::puts("\n[TC-08] delete root");
    /* 单节点 */
    {
        BST<int, int> t;
        t.insert(1, 1);
        t.remove(1);
        CHECK(t.is_empty());
    }
    /* 只有左子 */
    {
        BST<int, int> t;
        t.insert(5, 5); t.insert(3, 3);
        t.remove(5);
        CHECK_EQ(t.len(), 1u);
        CHECK(t.is_valid());
    }
    /* 双子 */
    {
        BST<int, int> t;
        for (int k : {5, 2, 8}) t.insert(k, k);
        t.remove(5);
        CHECK_EQ(t.len(), 2u);
        CHECK(t.is_valid());
    }
}

/* ------------------------------------------------------------------ */
/*  TC-09  重复 insert → len 不变，value 更新                           */
/* ------------------------------------------------------------------ */
static void tc09_duplicate_insert()
{
    std::puts("\n[TC-09] duplicate insert");
    BST<int, int> t;
    t.insert(1, 10);
    t.insert(1, 20);
    CHECK_EQ(t.len(), 1u);
    auto r = t.search(1);
    CHECK(r.has_value());
    CHECK_EQ(r->get(), 20);
}

/* ------------------------------------------------------------------ */
/*  TC-10  min / max                                                    */
/* ------------------------------------------------------------------ */
static void tc10_min_max()
{
    std::puts("\n[TC-10] min / max");
    BST<int, int> t;
    for (int k : {5, 3, 7, 1, 9}) t.insert(k, k);

    auto mn = t.min();
    CHECK(mn.has_value());
    CHECK_EQ(mn->first, 1);

    auto mx = t.max();
    CHECK(mx.has_value());
    CHECK_EQ(mx->first, 9);

    BST<int, int> empty;
    CHECK(!empty.min().has_value());
    CHECK(!empty.max().has_value());
}

/* ------------------------------------------------------------------ */
/*  TC-11  successor / predecessor 边界                                 */
/* ------------------------------------------------------------------ */
static void tc11_succ_pred()
{
    std::puts("\n[TC-11] successor / predecessor");
    BST<int, int> t;
    for (int k : {2, 1, 3}) t.insert(k, k);

    CHECK(!t.successor(3).has_value());
    CHECK(!t.predecessor(1).has_value());

    auto s = t.successor(1);
    CHECK(s.has_value());
    CHECK_EQ(s->first, 2);

    auto p = t.predecessor(3);
    CHECK(p.has_value());
    CHECK_EQ(p->first, 2);
}

/* ------------------------------------------------------------------ */
/*  TC-12  顺序插入 1..500，height 退化，中序仍正确                      */
/* ------------------------------------------------------------------ */
static void tc12_degenerate()
{
    std::puts("\n[TC-12] degenerate (sequential insert)");
    BST<int, int> t;
    for (int i = 1; i <= 500; i++) t.insert(i, i);

    CHECK(t.height() >= 250u);
    CHECK_EQ(t.len(), 500u);

    std::vector<int> keys;
    keys.reserve(500);
    t.in_order([&](const int& k, int&) { keys.push_back(k); });
    CHECK(std::is_sorted(keys.begin(), keys.end()));
    bool strict = keys.size() == 500;
    for (std::size_t i = 1; strict && i < keys.size(); i++)
        if (keys[i] <= keys[i-1]) strict = false;
    CHECK(strict);
}

/* ------------------------------------------------------------------ */
/*  TC-13  拷贝构造独立性                                               */
/* ------------------------------------------------------------------ */
static void tc13_copy_independence()
{
    std::puts("\n[TC-13] copy independence");
    BST<int, int> a;
    for (int k : {3, 1, 5}) a.insert(k, k);

    BST<int, int> b = a;
    b.remove(3);

    CHECK(a.contains(3));
    CHECK(!b.contains(3));
    CHECK_EQ(a.len(), 3u);
    CHECK_EQ(b.len(), 2u);
    CHECK(a.is_valid());
    CHECK(b.is_valid());
}

/* ------------------------------------------------------------------ */
/*  TC-14  level_order → BFS 顺序                                       */
/* ------------------------------------------------------------------ */
static void tc14_level_order()
{
    std::puts("\n[TC-14] level_order BFS");
    BST<int, int> t;
    for (int k : {4, 2, 6, 1, 3, 5, 7}) t.insert(k, k);

    std::vector<int> keys;
    t.level_order([&](const int& k, int&) { keys.push_back(k); });

    std::vector<int> want = {4, 2, 6, 1, 3, 5, 7};
    CHECK(keys == want);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
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
    tc11_succ_pred();
    tc12_degenerate();
    tc13_copy_independence();
    tc14_level_order();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

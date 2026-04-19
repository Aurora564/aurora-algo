/**
 * test_queue.cpp — C++ Queue 功能测试
 *
 * 编译：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_queue tests/test_queue.cpp
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_queue
 *   g++ -fsanitize=address,undefined ... && ./tests/test_queue
 */

#include "Queue.hpp"

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
/*  Queue<int> 测试用例                                                  */
/* ================================================================== */

/* TC-01  FIFO 顺序 */
static void tc01_fifo()
{
    std::printf("=== TC-01  FIFO 顺序 ===\n");
    Queue<int> q;
    for (int i = 1; i <= 5; i++)
        CHECK(q.enqueue(i).has_value());

    CHECK_EQ(q.size(), 5u);
    CHECK(!q.is_empty());

    for (int i = 1; i <= 5; i++) {
        auto r = q.dequeue();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }
    CHECK(q.is_empty());
}

/* TC-02  空队列 dequeue 返回 Empty */
static void tc02_dequeue_empty()
{
    std::printf("=== TC-02  dequeue 空队列 ===\n");
    Queue<int> q;
    auto r = q.dequeue();
    CHECK(!r.has_value());
    CHECK(r.error() == QueueError::Empty);
}

/* TC-03  空队列 peek 返回 Empty */
static void tc03_peek_empty()
{
    std::printf("=== TC-03  peek 空队列 ===\n");
    Queue<int> q;
    CHECK(!q.peek_front().has_value());
    CHECK(q.peek_front().error() == QueueError::Empty);
    CHECK(!q.peek_back().has_value());
    CHECK(q.peek_back().error() == QueueError::Empty);
}

/* TC-04  peek 不移除元素 */
static void tc04_peek_no_remove()
{
    std::printf("=== TC-04  peek 不移除元素 ===\n");
    Queue<int> q;
    (void)q.enqueue(10);
    (void)q.enqueue(20);
    (void)q.enqueue(30);

    auto pf = q.peek_front();
    CHECK(pf.has_value());
    CHECK_EQ(pf->get(), 10);

    auto pb = q.peek_back();
    CHECK(pb.has_value());
    CHECK_EQ(pb->get(), 30);

    /* 再次 peek，值不变 */
    CHECK_EQ(q.peek_front()->get(), 10);
    CHECK_EQ(q.size(), 3u);
}

/* TC-05  单元素入队出队 */
static void tc05_single()
{
    std::printf("=== TC-05  单元素 ===\n");
    Queue<int> q;
    (void)q.enqueue(42);
    CHECK_EQ(q.size(), 1u);
    CHECK_EQ(q.peek_front()->get(), 42);
    CHECK_EQ(q.peek_back()->get(), 42);

    auto r = q.dequeue();
    CHECK(r.has_value());
    CHECK_EQ(*r, 42);
    CHECK(q.is_empty());
}

/* TC-06  交替入队出队 */
static void tc06_interleaved()
{
    std::printf("=== TC-06  交替入队出队 ===\n");
    Queue<int> q;
    (void)q.enqueue(1);
    (void)q.enqueue(2);
    CHECK_EQ(*q.dequeue(), 1);
    (void)q.enqueue(3);
    CHECK_EQ(*q.dequeue(), 2);
    (void)q.enqueue(4);
    (void)q.enqueue(5);
    CHECK_EQ(*q.dequeue(), 3);
    CHECK_EQ(*q.dequeue(), 4);
    CHECK_EQ(*q.dequeue(), 5);
    CHECK(q.is_empty());
}

/* TC-07  clear */
static void tc07_clear()
{
    std::printf("=== TC-07  clear ===\n");
    Queue<int> q;
    for (int i = 0; i < 10; i++)
        q.enqueue(i);
    CHECK_EQ(q.size(), 10u);
    q.clear();
    CHECK(q.is_empty());
    CHECK_EQ(q.size(), 0u);
    /* clear 后可重新使用 */
    (void)q.enqueue(99);
    CHECK_EQ(*q.dequeue(), 99);
}

/* TC-08  range-for 迭代（按 FIFO 顺序） */
static void tc08_range_for()
{
    std::printf("=== TC-08  range-for 迭代 ===\n");
    Queue<int> q;
    for (int i = 1; i <= 4; i++)
        q.enqueue(i);

    int expected = 1;
    for (int v : q) {
        CHECK_EQ(v, expected++);
    }
    CHECK_EQ(expected, 5);
    /* 迭代不消耗元素 */
    CHECK_EQ(q.size(), 4u);
}

/* TC-09  移动语义入队 */
static void tc09_move_enqueue()
{
    std::printf("=== TC-09  移动语义入队 ===\n");
    Queue<std::string> q;
    std::string s = "hello";
    (void)q.enqueue(std::move(s));
    CHECK(s.empty());   /* 移动后源串应为空（实现定义，但 libstdc++ 保证）*/
    CHECK_EQ(q.dequeue().value(), std::string("hello"));
}

/* TC-10  字符串元素 */
static void tc10_string_elements()
{
    std::printf("=== TC-10  字符串元素 ===\n");
    Queue<std::string> q;
    (void)q.enqueue(std::string("alpha"));
    (void)q.enqueue(std::string("beta"));
    (void)q.enqueue(std::string("gamma"));

    CHECK_EQ(q.peek_front()->get(), std::string("alpha"));
    CHECK_EQ(q.peek_back()->get(),  std::string("gamma"));

    CHECK_EQ(q.dequeue().value(), std::string("alpha"));
    CHECK_EQ(q.dequeue().value(), std::string("beta"));
    CHECK_EQ(q.dequeue().value(), std::string("gamma"));
    CHECK(q.is_empty());
}

/* TC-11  拷贝语义 */
static void tc11_copy()
{
    std::printf("=== TC-11  拷贝语义 ===\n");
    Queue<int> q1;
    (void)q1.enqueue(1);
    (void)q1.enqueue(2);
    (void)q1.enqueue(3);

    Queue<int> q2 = q1;   /* 拷贝构造 */
    CHECK_EQ(q2.size(), 3u);

    /* 修改 q2 不影响 q1 */
    (void)q2.enqueue(4);
    CHECK_EQ(q1.size(), 3u);
    CHECK_EQ(q2.size(), 4u);

    /* q1 值不变 */
    CHECK_EQ(*q1.dequeue(), 1);
    CHECK_EQ(*q1.dequeue(), 2);
    CHECK_EQ(*q1.dequeue(), 3);
}

/* TC-12  移动构造 */
static void tc12_move_ctor()
{
    std::printf("=== TC-12  移动构造 ===\n");
    Queue<int> q1;
    (void)q1.enqueue(10);
    (void)q1.enqueue(20);

    Queue<int> q2 = std::move(q1);
    CHECK(q1.is_empty());
    CHECK_EQ(q2.size(), 2u);
    CHECK_EQ(*q2.dequeue(), 10);
    CHECK_EQ(*q2.dequeue(), 20);
}

/* TC-13  swap */
static void tc13_swap()
{
    std::printf("=== TC-13  swap ===\n");
    Queue<int> a, b;
    (void)a.enqueue(1); a.enqueue(2);
    (void)b.enqueue(10); b.enqueue(20); b.enqueue(30);

    swap(a, b);
    CHECK_EQ(a.size(), 3u);
    CHECK_EQ(b.size(), 2u);
    CHECK_EQ(*a.dequeue(), 10);
    CHECK_EQ(*b.dequeue(), 1);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main()
{
    tc01_fifo();
    tc02_dequeue_empty();
    tc03_peek_empty();
    tc04_peek_no_remove();
    tc05_single();
    tc06_interleaved();
    tc07_clear();
    tc08_range_for();
    tc09_move_enqueue();
    tc10_string_elements();
    tc11_copy();
    tc12_move_ctor();
    tc13_swap();

    std::printf("\n结果：%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

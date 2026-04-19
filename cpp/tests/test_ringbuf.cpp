/**
 * test_ringbuf.cpp — C++ RingBuf 功能测试
 *
 * 编译：
 *   cd aurora-algo/cpp
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_ringbuf tests/test_ringbuf.cpp
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_ringbuf
 */

#include "RingBuf.hpp"

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
/*  定长缓冲区测试                                                       */
/* ================================================================== */

/* TC-01  FIFO 顺序 */
static void tc01_fifo_fixed()
{
    std::printf("=== TC-01  FIFO（定长）===\n");
    RingBuf<int> rb(4);
    for (int i = 1; i <= 4; i++)
        CHECK(rb.enqueue(i).has_value());

    CHECK_EQ(rb.size(), 4u);
    CHECK(rb.is_full());

    for (int i = 1; i <= 4; i++) {
        auto r = rb.dequeue();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }
    CHECK(rb.is_empty());
}

/* TC-02  定长满时返回 Full */
static void tc02_fixed_full()
{
    std::printf("=== TC-02  定长满 ===\n");
    RingBuf<int> rb(2);
    CHECK(rb.enqueue(1).has_value());
    CHECK(rb.enqueue(2).has_value());
    auto r = rb.enqueue(3);
    CHECK(!r.has_value());
    CHECK(r.error() == RingBufError::Full);
    CHECK_EQ(rb.size(), 2u);
}

/* TC-03  空缓冲区 dequeue 返回 Empty */
static void tc03_dequeue_empty()
{
    std::printf("=== TC-03  dequeue 空 ===\n");
    RingBuf<int> rb(4);
    auto r = rb.dequeue();
    CHECK(!r.has_value());
    CHECK(r.error() == RingBufError::Empty);
}

/* TC-04  peek 不移除元素 */
static void tc04_peek()
{
    std::printf("=== TC-04  peek ===\n");
    RingBuf<int> rb(4);
    (void)rb.enqueue(10);
    (void)rb.enqueue(20);
    (void)rb.enqueue(30);


    auto pf = rb.peek_front();
    CHECK(pf.has_value());
    CHECK_EQ(pf->get(), 10);

    auto pb = rb.peek_back();
    CHECK(pb.has_value());
    CHECK_EQ(pb->get(), 30);

    CHECK_EQ(rb.size(), 3u);
}

/* TC-05  交替入队出队（环绕测试） */
static void tc05_wraparound()
{
    std::printf("=== TC-05  环绕（wrap-around）===\n");
    RingBuf<int> rb(4);
    /* 填满 */
    for (int i = 1; i <= 4; i++) (void)rb.enqueue(i);
    /* 消费 2 */
    CHECK_EQ(*rb.dequeue(), 1);
    CHECK_EQ(*rb.dequeue(), 2);
    /* 再入队 2（head 已推进，发生环绕） */
    CHECK(rb.enqueue(5).has_value());
    CHECK(rb.enqueue(6).has_value());
    /* 检验顺序 3,4,5,6 */
    CHECK_EQ(*rb.dequeue(), 3);
    CHECK_EQ(*rb.dequeue(), 4);
    CHECK_EQ(*rb.dequeue(), 5);
    CHECK_EQ(*rb.dequeue(), 6);
    CHECK(rb.is_empty());
}

/* TC-06  get 随机访问 */
static void tc06_get()
{
    std::printf("=== TC-06  get 随机访问 ===\n");
    RingBuf<int> rb(8);
    for (int i = 0; i < 5; i++) (void)rb.enqueue(i * 10);

    for (size_t i = 0; i < 5; i++) {
        auto r = rb.get(i);
        CHECK(r.has_value());
        CHECK_EQ(r->get(), (int)(i * 10));
    }

    auto oob = rb.get(5);
    CHECK(!oob.has_value());
    CHECK(oob.error() == RingBufError::OutOfBound);
}

/* TC-07  clear */
static void tc07_clear()
{
    std::printf("=== TC-07  clear ===\n");
    RingBuf<int> rb(4);
    (void)rb.enqueue(1); (void)rb.enqueue(2); (void)rb.enqueue(3);
    rb.clear();
    CHECK(rb.is_empty());
    CHECK_EQ(rb.size(), 0u);
    /* clear 后可重新使用 */
    (void)rb.enqueue(99);
    CHECK_EQ(*rb.dequeue(), 99);
}

/* ================================================================== */
/*  自增长缓冲区测试                                                    */
/* ================================================================== */

/* TC-08  自增长基本 FIFO */
static void tc08_growable_fifo()
{
    std::printf("=== TC-08  自增长 FIFO ===\n");
    RingBuf<int> rb(growable, 2);
    for (int i = 1; i <= 10; i++)
        CHECK(rb.enqueue(i).has_value());

    CHECK_EQ(rb.size(), 10u);

    for (int i = 1; i <= 10; i++) {
        auto r = rb.dequeue();
        CHECK(r.has_value());
        CHECK_EQ(*r, i);
    }
    CHECK(rb.is_empty());
}

/* TC-09  自增长扩容后环绕仍正确 */
static void tc09_growable_grow_wraparound()
{
    std::printf("=== TC-09  自增长环绕扩容 ===\n");
    RingBuf<int> rb(growable, 2);  /* cap 向上取到 2 */
    (void)rb.enqueue(1);
    (void)rb.enqueue(2);
    CHECK_EQ(*rb.dequeue(), 1); /* head 推进 */
    (void)rb.enqueue(3);        /* 此时 head=1, len=2, cap=2 → 满，触发扩容 */
    (void)rb.enqueue(4);        /* 扩容后可继续入队 */

    CHECK_EQ(*rb.dequeue(), 2);
    CHECK_EQ(*rb.dequeue(), 3);
    CHECK_EQ(*rb.dequeue(), 4);
    CHECK(rb.is_empty());
}

/* TC-10  reserve（仅自增长） */
static void tc10_reserve()
{
    std::printf("=== TC-10  reserve ===\n");
    RingBuf<int> rb(growable, 1);
    CHECK(rb.reserve(100).has_value());
    CHECK(rb.capacity() >= 100u);

    /* 定长缓冲区调 reserve 返回 BadConfig */
    RingBuf<int> fixed(4);
    auto r = fixed.reserve(10);
    CHECK(!r.has_value());
    CHECK(r.error() == RingBufError::BadConfig);
}

/* ================================================================== */
/*  Rule of Five 测试                                                  */
/* ================================================================== */

/* TC-11  拷贝构造（定长） */
static void tc11_copy()
{
    std::printf("=== TC-11  拷贝构造 ===\n");
    RingBuf<int> rb1(4);
    (void)rb1.enqueue(1); (void)rb1.enqueue(2); (void)rb1.enqueue(3);
    /* 先消费一个，制造非零 head */
    CHECK_EQ(*rb1.dequeue(), 1);

    RingBuf<int> rb2 = rb1;  /* 拷贝构造，linearized */
    CHECK_EQ(rb2.size(), 2u);

    /* 修改 rb2 不影响 rb1 */
    (void)rb2.enqueue(99);
    CHECK_EQ(rb1.size(), 2u);
    CHECK_EQ(rb2.size(), 3u);

    /* rb1 值正确 */
    CHECK_EQ(*rb1.dequeue(), 2);
    CHECK_EQ(*rb1.dequeue(), 3);
    CHECK(rb1.is_empty());
}

/* TC-12  移动构造 */
static void tc12_move()
{
    std::printf("=== TC-12  移动构造 ===\n");
    RingBuf<int> rb1(4);
    (void)rb1.enqueue(10); (void)rb1.enqueue(20);

    RingBuf<int> rb2 = std::move(rb1);
    CHECK(rb1.is_empty());
    CHECK_EQ(rb2.size(), 2u);
    CHECK_EQ(*rb2.dequeue(), 10);
    CHECK_EQ(*rb2.dequeue(), 20);
}

/* TC-13  swap */
static void tc13_swap()
{
    std::printf("=== TC-13  swap ===\n");
    RingBuf<int> a(4), b(4);
    (void)a.enqueue(1); (void)a.enqueue(2);
    (void)b.enqueue(100);

    swap(a, b);
    CHECK_EQ(a.size(), 1u);
    CHECK_EQ(b.size(), 2u);
    CHECK_EQ(*a.dequeue(), 100);
    CHECK_EQ(*b.dequeue(), 1);
    CHECK_EQ(*b.dequeue(), 2);
}

/* TC-14  字符串元素 */
static void tc14_strings()
{
    std::printf("=== TC-14  字符串元素 ===\n");
    RingBuf<std::string> rb(growable, 2);
    (void)rb.enqueue(std::string("hello"));
    (void)rb.enqueue(std::string("world"));
    (void)rb.enqueue(std::string("!"));

    CHECK_EQ(rb.peek_front()->get(), std::string("hello"));
    CHECK_EQ(rb.peek_back()->get(),  std::string("!"));

    CHECK_EQ(rb.dequeue().value(), std::string("hello"));
    CHECK_EQ(rb.dequeue().value(), std::string("world"));
    CHECK_EQ(rb.dequeue().value(), std::string("!"));
    CHECK(rb.is_empty());
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main()
{
    tc01_fifo_fixed();
    tc02_fixed_full();
    tc03_dequeue_empty();
    tc04_peek();
    tc05_wraparound();
    tc06_get();
    tc07_clear();
    tc08_growable_fifo();
    tc09_growable_grow_wraparound();
    tc10_reserve();
    tc11_copy();
    tc12_move();
    tc13_swap();
    tc14_strings();

    std::printf("\n结果：%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

/**
 * test_str.cpp — Str UTF-8 字符串测试
 *
 * 编译：
 *   g++ -std=c++23 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_str tests/test_str.cpp && ./tests/test_str
 */

#include "Str.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using algo::Str;
using algo::StrError;

/* ------------------------------------------------------------------ */
/*  小型测试框架（与其他 test_*.cpp 保持一致）                          */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(expr)                                                   \
    do {                                                              \
        if (expr) { ++g_pass; }                                       \
        else {                                                        \
            ++g_fail;                                                 \
            std::fprintf(stderr, "FAIL %s:%d  %s\n",                 \
                         __FILE__, __LINE__, #expr);                  \
        }                                                             \
    } while (0)

/* ------------------------------------------------------------------ */
/*  TC-01  空字符串构造                                                 */
/* ------------------------------------------------------------------ */
static void tc01_empty_construction()
{
    Str s;
    CHECK(s.is_empty());
    CHECK(s.len_bytes() == 0);
    CHECK(s.len_chars() == 0);
    CHECK(s.capacity()  == 0);
    CHECK(s.as_string_view().empty());
}

/* ------------------------------------------------------------------ */
/*  TC-02  from() ASCII                                                */
/* ------------------------------------------------------------------ */
static void tc02_from_ascii()
{
    auto r = Str::from("hello");
    CHECK(r.has_value());
    CHECK(r->len_bytes() == 5);
    CHECK(r->len_chars() == 5);
    CHECK(!r->is_empty());
    CHECK(r->as_string_view() == "hello");
}

/* ------------------------------------------------------------------ */
/*  TC-03  from() 多字节 UTF-8                                         */
/* ------------------------------------------------------------------ */
static void tc03_from_utf8_multibyte()
{
    /* "你好世界" — 每个汉字 3 字节，共 12 字节 */
    auto r = Str::from("你好世界");
    CHECK(r.has_value());
    CHECK(r->len_bytes() == 12);
    CHECK(r->len_chars() == 4);
}

/* ------------------------------------------------------------------ */
/*  TC-04  from() 非法 UTF-8                                           */
/* ------------------------------------------------------------------ */
static void tc04_invalid_utf8()
{
    /* 单个续字节 0x80 — 非法首字节 */
    const char bad[] = {'\x80', '\0'};
    auto r = Str::from(std::string_view(bad, 1));
    CHECK(!r.has_value());
    CHECK(r.error() == StrError::InvalidUtf8);

    /* 截断序列：只给 2 字节中的 1 字节 */
    const char trunc[] = {'\xC3', '\0'};
    auto r2 = Str::from(std::string_view(trunc, 1));
    CHECK(!r2.has_value());
    CHECK(r2.error() == StrError::InvalidUtf8);
}

/* ------------------------------------------------------------------ */
/*  TC-05  len_bytes vs len_chars                                      */
/* ------------------------------------------------------------------ */
static void tc05_len_bytes_vs_chars()
{
    /* "A日B" — 1 + 3 + 1 = 5 字节，3 个码点 */
    auto r = Str::from("A日B");
    CHECK(r.has_value());
    CHECK(r->len_bytes() == 5);
    CHECK(r->len_chars() == 3);
}

/* ------------------------------------------------------------------ */
/*  TC-06  get_char                                                    */
/* ------------------------------------------------------------------ */
static void tc06_get_char()
{
    auto r = Str::from("A日B");
    CHECK(r.has_value());

    auto c0 = r->get_char(0);
    CHECK(c0.has_value() && *c0 == U'A');

    auto c1 = r->get_char(1);
    CHECK(c1.has_value() && *c1 == U'日');

    auto c2 = r->get_char(2);
    CHECK(c2.has_value() && *c2 == U'B');

    /* 越界 */
    auto c3 = r->get_char(3);
    CHECK(!c3.has_value() && c3.error() == StrError::OutOfBound);
}

/* ------------------------------------------------------------------ */
/*  TC-07  byte_offset 及末尾哨兵                                      */
/* ------------------------------------------------------------------ */
static void tc07_byte_offset()
{
    /* "A日B": A@0, 日@1(3 bytes), B@4 */
    auto r = Str::from("A日B");
    CHECK(r.has_value());

    auto o0 = r->byte_offset(0);
    CHECK(o0.has_value() && *o0 == 0);

    auto o1 = r->byte_offset(1);
    CHECK(o1.has_value() && *o1 == 1);

    auto o2 = r->byte_offset(2);
    CHECK(o2.has_value() && *o2 == 4);

    /* 哨兵：char_i == len_chars() == 3 → len_bytes() == 5 */
    auto sentinel = r->byte_offset(3);
    CHECK(sentinel.has_value() && *sentinel == 5);

    /* 越界 */
    auto oob = r->byte_offset(4);
    CHECK(!oob.has_value() && oob.error() == StrError::OutOfBound);
}

/* ------------------------------------------------------------------ */
/*  TC-08  slice                                                       */
/* ------------------------------------------------------------------ */
static void tc08_slice()
{
    auto r = Str::from("A日B");
    CHECK(r.has_value());

    /* [0, 1) → "A" */
    auto s0 = r->slice(0, 1);
    CHECK(s0.has_value() && *s0 == "A");

    /* [1, 4) → "日" */
    auto s1 = r->slice(1, 4);
    CHECK(s1.has_value() && *s1 == "日");

    /* [4, 5) → "B" */
    auto s2 = r->slice(4, 5);
    CHECK(s2.has_value() && *s2 == "B");

    /* BadBoundary: byte 2 是续字节 */
    auto bad = r->slice(2, 4);
    CHECK(!bad.has_value() && bad.error() == StrError::BadBoundary);

    /* OutOfBound: byte_end > len */
    auto oob = r->slice(0, 6);
    CHECK(!oob.has_value() && oob.error() == StrError::OutOfBound);
}

/* ------------------------------------------------------------------ */
/*  TC-09  append / append_char                                        */
/* ------------------------------------------------------------------ */
static void tc09_append()
{
    auto r = Str::from("hello");
    CHECK(r.has_value());

    auto e1 = r->append(" world");
    CHECK(e1.has_value());
    CHECK(r->as_string_view() == "hello world");
    CHECK(r->len_bytes() == 11);

    /* append_char: U+4E16 = 世 */
    r->append_char(0x4E16u);
    CHECK(r->len_bytes() == 14);
    CHECK(r->len_chars() == 12);

    /* append 非法 UTF-8 → error */
    const char bad[] = {'\x80'};
    auto e2 = r->append(std::string_view(bad, 1));
    CHECK(!e2.has_value() && e2.error() == StrError::InvalidUtf8);
    /* 原字符串未被修改 */
    CHECK(r->len_bytes() == 14);
}

/* ------------------------------------------------------------------ */
/*  TC-10  insert / remove_range                                       */
/* ------------------------------------------------------------------ */
static void tc10_insert_remove()
{
    auto r = Str::from("helo");
    CHECK(r.has_value());

    /* 在 byte 3 ("o" 前)插入 "l" */
    auto ei = r->insert(3, "l");
    CHECK(ei.has_value());
    CHECK(r->as_string_view() == "hello");

    /* remove_range [1, 3) → "hlo" */
    auto er = r->remove_range(1, 3);
    CHECK(er.has_value());
    CHECK(r->as_string_view() == "hlo");

    /* BadBoundary: 在多字节字符中间插入 */
    auto r2 = Str::from("A日B");
    CHECK(r2.has_value());
    auto ebad = r2->insert(2, "X");  /* byte 2 是续字节 */
    CHECK(!ebad.has_value() && ebad.error() == StrError::BadBoundary);
}

/* ------------------------------------------------------------------ */
/*  TC-11  find / contains / starts_with / ends_with / trim           */
/* ------------------------------------------------------------------ */
static void tc11_search()
{
    auto r = Str::from("  hello world  ");
    CHECK(r.has_value());

    auto [pos, found] = r->find("world");
    CHECK(found && pos == 8);

    auto [_, notfound] = r->find("xyz");
    CHECK(!notfound);

    CHECK(r->contains("hello"));
    CHECK(!r->contains("bye"));

    CHECK(r->starts_with("  hello"));
    CHECK(!r->starts_with("hello"));

    CHECK(r->ends_with("world  "));
    CHECK(!r->ends_with("world"));

    Str trimmed = r->trim();
    CHECK(trimmed.as_string_view() == "hello world");
}

/* ------------------------------------------------------------------ */
/*  TC-12  to_upper / to_lower                                        */
/* ------------------------------------------------------------------ */
static void tc12_case()
{
    auto r = Str::from("Hello World 你好");
    CHECK(r.has_value());

    r->to_upper();
    CHECK(r->starts_with("HELLO WORLD "));

    r->to_lower();
    CHECK(r->starts_with("hello world "));

    /* 非 ASCII 字节不受影响（字节数不变） */
    CHECK(r->len_bytes() == 18);  /* "hello world " = 12, "你好" = 6 */
}

/* ------------------------------------------------------------------ */
/*  TC-13  compare / hash                                              */
/* ------------------------------------------------------------------ */
static void tc13_compare_hash()
{
    auto a = Str::from("abc");
    auto b = Str::from("abc");
    auto c = Str::from("abd");
    CHECK(a.has_value() && b.has_value() && c.has_value());

    CHECK(*a == *b);
    CHECK(a->compare(*b) == 0);
    CHECK(a->compare(*c) < 0);
    CHECK(c->compare(*a) > 0);

    /* 相同内容，相同哈希 */
    CHECK(a->hash() == b->hash());

    /* 不同内容，哈希应不同（概率极高） */
    CHECK(a->hash() != c->hash());
}

/* ------------------------------------------------------------------ */
/*  TC-14  拷贝构造 / 移动构造 / swap                                  */
/* ------------------------------------------------------------------ */
static void tc14_copy_move_swap()
{
    auto r = Str::from("hello 世界");
    CHECK(r.has_value());

    /* 拷贝构造 */
    Str copy = *r;
    CHECK(copy.as_string_view() == r->as_string_view());
    CHECK(copy.len_bytes() == r->len_bytes());
    /* 深拷贝：修改原，副本不变 */
    (void)r->append("!");
    CHECK(copy.as_string_view() == "hello 世界");

    /* 移动构造 */
    Str moved = std::move(copy);
    CHECK(moved.as_string_view() == "hello 世界");
    CHECK(copy.is_empty());  /* copy 已被清空 */

    /* swap */
    auto x = Str::from("AAA");
    auto y = Str::from("BBB");
    CHECK(x.has_value() && y.has_value());
    swap(*x, *y);
    CHECK(x->as_string_view() == "BBB");
    CHECK(y->as_string_view() == "AAA");
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int main()
{
    tc01_empty_construction();
    tc02_from_ascii();
    tc03_from_utf8_multibyte();
    tc04_invalid_utf8();
    tc05_len_bytes_vs_chars();
    tc06_get_char();
    tc07_byte_offset();
    tc08_slice();
    tc09_append();
    tc10_insert_remove();
    tc11_search();
    tc12_case();
    tc13_compare_hash();
    tc14_copy_move_swap();

    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

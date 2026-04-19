/**
 * test_str.c — C Str (动态 UTF-8 字符串) 功能测试
 *
 * 编译：
 *   cd aurora-algo/c
 *   gcc -std=c99 -O0 -g -Wall -Wextra -I include \
 *       -o tests/test_str tests/test_str.c src/Str.c
 *
 * 内存检查：
 *   valgrind --leak-check=full ./tests/test_str
 *   gcc -fsanitize=address,undefined ... && ./tests/test_str
 */

#include "Str.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/*  测试工具宏                                                          */
/* ------------------------------------------------------------------ */

static int g_pass = 0, g_fail = 0;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (expr) {                                                        \
            g_pass++;                                                      \
        } else {                                                           \
            fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr); \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        if ((a) == (b)) {                                                  \
            g_pass++;                                                      \
        } else {                                                           \
            fprintf(stderr, "FAIL  %s:%d  %s == %s  (%d != %d)\n",       \
                    __FILE__, __LINE__, #a, #b, (int)(a), (int)(b));      \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

/* ================================================================== */
/*  TC-01  空字符串                                                     */
/* ================================================================== */

static void tc01_empty(void)
{
    Str s;
    CHECK_EQ(str_new(&s), STR_OK);
    CHECK(str_is_empty(&s));
    CHECK_EQ((int)str_len_bytes(&s), 0);
    CHECK_EQ((int)str_len_chars(&s), 0);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-02  from_cstr / len_bytes / len_chars                           */
/* ================================================================== */

static void tc02_from_cstr_ascii(void)
{
    Str s;
    CHECK_EQ(str_from_cstr(&s, "hello"), STR_OK);
    CHECK_EQ((int)str_len_bytes(&s), 5);
    CHECK_EQ((int)str_len_chars(&s), 5);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-03  多字节 UTF-8（中文）                                         */
/* ================================================================== */

static void tc03_multibyte(void)
{
    /* "你好" — 2 chars, 6 bytes (each 3 bytes in UTF-8) */
    Str s;
    CHECK_EQ(str_from_cstr(&s, "\xE4\xBD\xA0\xE5\xA5\xBD"), STR_OK);
    CHECK_EQ((int)str_len_bytes(&s), 6);
    CHECK_EQ((int)str_len_chars(&s), 2);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-04  无效 UTF-8 被拒绝                                            */
/* ================================================================== */

static void tc04_invalid_utf8(void)
{
    Str s;
    /* 0xFF is never valid in UTF-8 */
    CHECK_EQ(str_from_cstr(&s, "\xFF"), STR_ERR_INVALID_UTF8);

    /* Lone continuation byte */
    CHECK_EQ(str_from_cstr(&s, "\x80"), STR_ERR_INVALID_UTF8);

    /* Overlong 2-byte encoding of U+0000 */
    CHECK_EQ(str_from_cstr(&s, "\xC0\x80"), STR_ERR_INVALID_UTF8);

    /* Surrogate U+D800 */
    CHECK_EQ(str_from_cstr(&s, "\xED\xA0\x80"), STR_ERR_INVALID_UTF8);
}

/* ================================================================== */
/*  TC-05  get_char                                                     */
/* ================================================================== */

static void tc05_get_char(void)
{
    Str s;
    /* "A©B" — U+0041, U+00A9 (2 bytes), U+0042 */
    str_from_cstr(&s, "A\xC2\xA9""B");

    uint32_t cp;
    CHECK_EQ(str_get_char(&s, 0, &cp), STR_OK);
    CHECK_EQ((int)cp, 0x41);   /* 'A' */
    CHECK_EQ(str_get_char(&s, 1, &cp), STR_OK);
    CHECK_EQ((int)cp, 0xA9);   /* '©' */
    CHECK_EQ(str_get_char(&s, 2, &cp), STR_OK);
    CHECK_EQ((int)cp, 0x42);   /* 'B' */
    CHECK_EQ(str_get_char(&s, 3, &cp), STR_ERR_OOB);

    str_destroy(&s);
}

/* ================================================================== */
/*  TC-06  byte_offset                                                  */
/* ================================================================== */

static void tc06_byte_offset(void)
{
    Str s;
    str_from_cstr(&s, "A\xC2\xA9""B");  /* 4 bytes, 3 chars */

    size_t off;
    CHECK_EQ(str_byte_offset(&s, 0, &off), STR_OK); CHECK_EQ((int)off, 0);
    CHECK_EQ(str_byte_offset(&s, 1, &off), STR_OK); CHECK_EQ((int)off, 1);
    CHECK_EQ(str_byte_offset(&s, 2, &off), STR_OK); CHECK_EQ((int)off, 3);
    CHECK_EQ(str_byte_offset(&s, 3, &off), STR_OK); CHECK_EQ((int)off, 4);
    CHECK_EQ(str_byte_offset(&s, 4, &off), STR_ERR_OOB);

    str_destroy(&s);
}

/* ================================================================== */
/*  TC-07  slice                                                        */
/* ================================================================== */

static void tc07_slice(void)
{
    Str s;
    str_from_cstr(&s, "hello world");

    Str sub;
    CHECK_EQ(str_slice(&s, 6, 11, &sub), STR_OK);
    const char *cs = str_as_cstr(&sub);
    CHECK(strcmp(cs, "world") == 0);
    str_destroy(&sub);

    /* bad boundary: in "A©B" (A=0x41, ©=0xC2 0xA9, B=0x42)           *
     * byte 2 (0xA9) is a continuation byte → bad boundary.           */
    Str s2;
    str_from_cstr(&s2, "A\xC2\xA9""B");
    Str sub2;
    CHECK_EQ(str_slice(&s2, 2, 4, &sub2), STR_ERR_BAD_BOUNDARY);
    str_destroy(&s2);

    str_destroy(&s);
}

/* ================================================================== */
/*  TC-08  append / append_cstr / append_char                          */
/* ================================================================== */

static void tc08_append(void)
{
    Str s;
    str_new(&s);
    CHECK_EQ(str_append_cstr(&s, "foo"), STR_OK);
    CHECK_EQ(str_append_cstr(&s, "bar"), STR_OK);
    CHECK_EQ((int)str_len_bytes(&s), 6);
    const char *cs = str_as_cstr(&s);
    CHECK(strcmp(cs, "foobar") == 0);

    /* append_char: '!' = U+0021 */
    CHECK_EQ(str_append_char(&s, 0x21), STR_OK);
    cs = str_as_cstr(&s);
    CHECK(strcmp(cs, "foobar!") == 0);

    str_destroy(&s);
}

/* ================================================================== */
/*  TC-09  insert                                                       */
/* ================================================================== */

static void tc09_insert(void)
{
    Str s;
    str_from_cstr(&s, "ac");
    /* insert 'b' at rune index 1 → "abc" */
    CHECK_EQ(str_insert(&s, 1, 'b'), STR_OK);
    const char *cs = str_as_cstr(&s);
    CHECK(strcmp(cs, "abc") == 0);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-10  remove_range                                                 */
/* ================================================================== */

static void tc10_remove_range(void)
{
    Str s;
    str_from_cstr(&s, "hello world");
    CHECK_EQ(str_remove_range(&s, 5, 11), STR_OK);
    const char *cs = str_as_cstr(&s);
    CHECK(strcmp(cs, "hello") == 0);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-11  clear / reserve / shrink_to_fit                             */
/* ================================================================== */

static void tc11_storage(void)
{
    Str s;
    str_new(&s);
    CHECK_EQ(str_reserve(&s, 64), STR_OK);
    CHECK((int)str_cap(&s) >= 64);

    str_append_cstr(&s, "hello");
    str_clear(&s);
    CHECK_EQ((int)str_len_bytes(&s), 0);
    CHECK((int)str_cap(&s) >= 64);  /* cap preserved */

    /* shrink after re-populating */
    str_append_cstr(&s, "hi");
    CHECK_EQ(str_shrink_to_fit(&s), STR_OK);
    CHECK_EQ((int)str_cap(&s), 2);

    str_destroy(&s);
}

/* ================================================================== */
/*  TC-12  to_upper / to_lower                                         */
/* ================================================================== */

static void tc12_case(void)
{
    Str s;
    str_from_cstr(&s, "Hello World");

    str_to_upper(&s);
    const char *cs = str_as_cstr(&s);
    CHECK(strcmp(cs, "HELLO WORLD") == 0);

    str_to_lower(&s);
    cs = str_as_cstr(&s);
    CHECK(strcmp(cs, "hello world") == 0);

    str_destroy(&s);
}

/* ================================================================== */
/*  TC-13  find / contains / starts_with / ends_with                  */
/* ================================================================== */

static void tc13_search(void)
{
    Str s;
    str_from_cstr(&s, "hello world");

    Str needle;
    str_from_cstr(&needle, "world");

    CHECK_EQ((int)str_find(&s, &needle), 6);
    CHECK(str_contains(&s, &needle));

    Str prefix;
    str_from_cstr(&prefix, "hello");
    CHECK(str_starts_with(&s, &prefix));

    CHECK(str_ends_with(&s, &needle));

    str_destroy(&needle);
    str_destroy(&prefix);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-14  trim                                                         */
/* ================================================================== */

static void tc14_trim(void)
{
    Str s;
    str_from_cstr(&s, "  hello  ");

    Str trimmed;
    CHECK_EQ(str_trim(&s, &trimmed), STR_OK);
    const char *cs = str_as_cstr(&trimmed);
    CHECK(strcmp(cs, "hello") == 0);

    str_destroy(&trimmed);
    str_destroy(&s);
}

/* ================================================================== */
/*  TC-15  equal / compare                                              */
/* ================================================================== */

static void tc15_compare(void)
{
    Str a, b, c;
    str_from_cstr(&a, "apple");
    str_from_cstr(&b, "apple");
    str_from_cstr(&c, "banana");

    CHECK(str_equal(&a, &b));
    CHECK(!str_equal(&a, &c));
    CHECK(str_compare(&a, &b) == 0);
    CHECK(str_compare(&a, &c) < 0);
    CHECK(str_compare(&c, &a) > 0);

    str_destroy(&a);
    str_destroy(&b);
    str_destroy(&c);
}

/* ================================================================== */
/*  TC-16  hash 相同内容产生相同哈希                                    */
/* ================================================================== */

static void tc16_hash(void)
{
    Str a, b, c;
    str_from_cstr(&a, "hello");
    str_from_cstr(&b, "hello");
    str_from_cstr(&c, "world");

    CHECK(str_hash(&a) == str_hash(&b));
    CHECK(str_hash(&a) != str_hash(&c));

    str_destroy(&a);
    str_destroy(&b);
    str_destroy(&c);
}

/* ================================================================== */
/*  TC-17  strerror 覆盖                                               */
/* ================================================================== */

static void tc17_strerror(void)
{
    CHECK(str_strerror(STR_OK)               != NULL);
    CHECK(str_strerror(STR_ERR_INVALID_UTF8) != NULL);
    CHECK(str_strerror(STR_ERR_OOB)          != NULL);
    CHECK(str_strerror(STR_ERR_BAD_BOUNDARY) != NULL);
    CHECK(str_strerror(STR_ERR_ALLOC)        != NULL);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */

int main(void)
{
    printf("=== Str ===\n");
    tc01_empty();
    tc02_from_cstr_ascii();
    tc03_multibyte();
    tc04_invalid_utf8();
    tc05_get_char();
    tc06_byte_offset();
    tc07_slice();
    tc08_append();
    tc09_insert();
    tc10_remove_range();
    tc11_storage();
    tc12_case();
    tc13_search();
    tc14_trim();
    tc15_compare();
    tc16_hash();
    tc17_strerror();

    printf("\nResult: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

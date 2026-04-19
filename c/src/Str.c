#include "Str.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* UTF-8 internal helpers                                               */
/* ------------------------------------------------------------------ */

/* Returns non-zero if b is the start of a rune (not a continuation). */
static int utf8_rune_start(unsigned char b)
{
    return (b & 0xC0) != 0x80;
}

/* Decode one rune from buf[0..buf_len).                               *
 * On success: fills *cp and returns byte length (1-4).               *
 * On error: returns 0 (invalid sequence, surrogate, or > 0x10FFFF).  */
static size_t utf8_decode_one(const unsigned char *buf, size_t buf_len,
                               uint32_t *cp)
{
    if (buf_len == 0) return 0;

    unsigned char b0 = buf[0];

    if (b0 < 0x80) {
        *cp = b0;
        return 1;
    }
    if (b0 < 0xC0) return 0;  /* continuation byte at start */

    size_t   seq_len;
    uint32_t min_cp;

    if (b0 < 0xE0) {
        seq_len = 2; min_cp = 0x80;
        *cp = b0 & 0x1F;
    } else if (b0 < 0xF0) {
        seq_len = 3; min_cp = 0x800;
        *cp = b0 & 0x0F;
    } else if (b0 < 0xF8) {
        seq_len = 4; min_cp = 0x10000;
        *cp = b0 & 0x07;
    } else {
        return 0;
    }

    if (buf_len < seq_len) return 0;

    for (size_t i = 1; i < seq_len; i++) {
        unsigned char bi = buf[i];
        if ((bi & 0xC0) != 0x80) return 0;
        *cp = (*cp << 6) | (bi & 0x3F);
    }

    if (*cp < min_cp)                   return 0;  /* overlong         */
    if (*cp >= 0xD800 && *cp <= 0xDFFF) return 0;  /* surrogate        */
    if (*cp > 0x10FFFF)                 return 0;  /* out of range     */

    return seq_len;
}

/* Encode cp into buf (must have ≥ 4 bytes). Returns byte length.     *
 * Returns 0 for invalid code points.                                  */
static size_t utf8_encode_one(uint32_t cp, unsigned char *buf)
{
    if (cp < 0x80) {
        buf[0] = (unsigned char)cp;
        return 1;
    }
    if (cp < 0x800) {
        buf[0] = (unsigned char)(0xC0 | (cp >> 6));
        buf[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
    if (cp < 0x10000) {
        buf[0] = (unsigned char)(0xE0 | (cp >> 12));
        buf[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        buf[0] = (unsigned char)(0xF0 | (cp >> 18));
        buf[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (unsigned char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

/* Returns non-zero if the byte range [data, data+n) is valid UTF-8.  */
static int utf8_validate(const unsigned char *data, size_t n)
{
    size_t i = 0;
    while (i < n) {
        uint32_t cp;
        size_t   step = utf8_decode_one(data + i, n - i, &cp);
        if (step == 0) return 0;
        i += step;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Internal buffer helpers                                              */
/* ------------------------------------------------------------------ */

static StrError str_ensure(Str *s, size_t need)
{
    if (need <= s->cap) return STR_OK;

    size_t new_cap = s->cap == 0 ? 16 : s->cap;
    while (new_cap < need) new_cap *= 2;

    unsigned char *p = (unsigned char *)realloc(s->data, new_cap);
    if (!p) return STR_ERR_ALLOC;

    s->data = p;
    s->cap  = new_cap;
    return STR_OK;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

StrError str_new(Str *s)
{
    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;
    return STR_OK;
}

StrError str_from_cstr(Str *s, const char *cstr)
{
    size_t n = strlen(cstr);
    if (!utf8_validate((const unsigned char *)cstr, n))
        return STR_ERR_INVALID_UTF8;

    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;

    StrError e = str_ensure(s, n);
    if (e != STR_OK) return e;

    memcpy(s->data, cstr, n);
    s->len = n;
    return STR_OK;
}

StrError str_from_bytes(Str *s, const unsigned char *bytes, size_t n)
{
    if (!utf8_validate(bytes, n)) return STR_ERR_INVALID_UTF8;

    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;

    StrError e = str_ensure(s, n);
    if (e != STR_OK) return e;

    memcpy(s->data, bytes, n);
    s->len = n;
    return STR_OK;
}

void str_destroy(Str *s)
{
    free(s->data);
    s->data = NULL;
    s->len  = 0;
    s->cap  = 0;
}

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t str_len_bytes(const Str *s) { return s->len; }
size_t str_cap(const Str *s)       { return s->cap; }
int    str_is_empty(const Str *s)  { return s->len == 0; }

size_t str_len_chars(const Str *s)
{
    size_t count = 0, i = 0;
    while (i < s->len) {
        if (utf8_rune_start(s->data[i])) count++;
        i++;
    }
    return count;
}

const char *str_as_cstr(Str *s)
{
    /* Ensure there is room for a NUL terminator without changing len. */
    if (str_ensure(s, s->len + 1) != STR_OK) return NULL;
    s->data[s->len] = '\0';
    return (const char *)s->data;
}

/* ------------------------------------------------------------------ */
/* Access                                                               */
/* ------------------------------------------------------------------ */

StrError str_get_char(const Str *s, size_t i, uint32_t *cp)
{
    size_t pos = 0, idx = 0;
    while (pos < s->len) {
        uint32_t c;
        size_t   step = utf8_decode_one(s->data + pos, s->len - pos, &c);
        if (step == 0) return STR_ERR_INVALID_UTF8;
        if (idx == i) { *cp = c; return STR_OK; }
        idx++;
        pos += step;
    }
    return STR_ERR_OOB;
}

StrError str_byte_offset(const Str *s, size_t i, size_t *offset)
{
    size_t pos = 0, idx = 0;
    while (pos < s->len) {
        if (idx == i) { *offset = pos; return STR_OK; }
        uint32_t cp;
        size_t step = utf8_decode_one(s->data + pos, s->len - pos, &cp);
        if (step == 0) return STR_ERR_INVALID_UTF8;
        idx++;
        pos += step;
    }
    if (idx == i) { *offset = pos; return STR_OK; }
    return STR_ERR_OOB;
}

StrError str_slice(const Str *s, size_t from_byte, size_t to_byte, Str *dst)
{
    if (from_byte > s->len || to_byte > s->len || from_byte > to_byte)
        return STR_ERR_OOB;
    if (from_byte < s->len && !utf8_rune_start(s->data[from_byte]))
        return STR_ERR_BAD_BOUNDARY;
    if (to_byte < s->len && !utf8_rune_start(s->data[to_byte]))
        return STR_ERR_BAD_BOUNDARY;

    return str_from_bytes(dst, s->data + from_byte, to_byte - from_byte);
}

/* ------------------------------------------------------------------ */
/* Modification                                                         */
/* ------------------------------------------------------------------ */

StrError str_append(Str *s, const Str *other)
{
    StrError e = str_ensure(s, s->len + other->len);
    if (e != STR_OK) return e;
    memcpy(s->data + s->len, other->data, other->len);
    s->len += other->len;
    return STR_OK;
}

StrError str_append_cstr(Str *s, const char *cstr)
{
    size_t n = strlen(cstr);
    if (!utf8_validate((const unsigned char *)cstr, n))
        return STR_ERR_INVALID_UTF8;
    StrError e = str_ensure(s, s->len + n);
    if (e != STR_OK) return e;
    memcpy(s->data + s->len, cstr, n);
    s->len += n;
    return STR_OK;
}

StrError str_append_char(Str *s, uint32_t cp)
{
    unsigned char buf[4];
    size_t n = utf8_encode_one(cp, buf);
    if (n == 0) return STR_ERR_INVALID_UTF8;
    StrError e = str_ensure(s, s->len + n);
    if (e != STR_OK) return e;
    memcpy(s->data + s->len, buf, n);
    s->len += n;
    return STR_OK;
}

StrError str_insert(Str *s, size_t i, uint32_t cp)
{
    size_t offset;
    StrError e = str_byte_offset(s, i, &offset);
    if (e != STR_OK) return e;

    unsigned char buf[4];
    size_t n = utf8_encode_one(cp, buf);
    if (n == 0) return STR_ERR_INVALID_UTF8;

    e = str_ensure(s, s->len + n);
    if (e != STR_OK) return e;

    memmove(s->data + offset + n, s->data + offset, s->len - offset);
    memcpy(s->data + offset, buf, n);
    s->len += n;
    return STR_OK;
}

StrError str_remove_range(Str *s, size_t from, size_t to)
{
    if (from > to) return STR_ERR_OOB;

    size_t from_byte, to_byte;
    StrError e = str_byte_offset(s, from, &from_byte);
    if (e != STR_OK) return e;
    e = str_byte_offset(s, to, &to_byte);
    if (e != STR_OK) return e;

    size_t remove_bytes = to_byte - from_byte;
    memmove(s->data + from_byte, s->data + to_byte, s->len - to_byte);
    s->len -= remove_bytes;
    return STR_OK;
}

void str_clear(Str *s)
{
    s->len = 0;
}

StrError str_reserve(Str *s, size_t additional)
{
    return str_ensure(s, s->len + additional);
}

StrError str_shrink_to_fit(Str *s)
{
    if (s->len == s->cap) return STR_OK;
    if (s->len == 0) {
        free(s->data);
        s->data = NULL;
        s->cap  = 0;
        return STR_OK;
    }
    unsigned char *p = (unsigned char *)realloc(s->data, s->len);
    if (!p) return STR_ERR_ALLOC;
    s->data = p;
    s->cap  = s->len;
    return STR_OK;
}

void str_to_upper(Str *s)
{
    for (size_t i = 0; i < s->len; i++)
        if (s->data[i] < 0x80)
            s->data[i] = (unsigned char)toupper(s->data[i]);
}

void str_to_lower(Str *s)
{
    for (size_t i = 0; i < s->len; i++)
        if (s->data[i] < 0x80)
            s->data[i] = (unsigned char)tolower(s->data[i]);
}

/* ------------------------------------------------------------------ */
/* Search                                                               */
/* ------------------------------------------------------------------ */

size_t str_find(const Str *s, const Str *needle)
{
    if (needle->len == 0) return 0;
    if (needle->len > s->len) return (size_t)-1;

    for (size_t i = 0; i <= s->len - needle->len; i++) {
        if (s->data[i] == needle->data[0] &&
            memcmp(s->data + i, needle->data, needle->len) == 0)
            return i;
    }
    return (size_t)-1;
}

size_t str_find_cstr(const Str *s, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return 0;
    if (nlen > s->len) return (size_t)-1;

    for (size_t i = 0; i <= s->len - nlen; i++) {
        if (memcmp(s->data + i, needle, nlen) == 0)
            return i;
    }
    return (size_t)-1;
}

int str_contains(const Str *s, const Str *needle)
{
    return str_find(s, needle) != (size_t)-1;
}

int str_starts_with(const Str *s, const Str *prefix)
{
    if (prefix->len > s->len) return 0;
    return memcmp(s->data, prefix->data, prefix->len) == 0;
}

int str_ends_with(const Str *s, const Str *suffix)
{
    if (suffix->len > s->len) return 0;
    return memcmp(s->data + s->len - suffix->len,
                  suffix->data, suffix->len) == 0;
}

StrError str_trim(const Str *s, Str *dst)
{
    size_t lo = 0;
    while (lo < s->len && s->data[lo] < 0x80 && isspace(s->data[lo]))
        lo++;

    size_t hi = s->len;
    while (hi > lo && s->data[hi - 1] < 0x80 && isspace(s->data[hi - 1]))
        hi--;

    return str_from_bytes(dst, s->data + lo, hi - lo);
}

/* ------------------------------------------------------------------ */
/* Comparison                                                           */
/* ------------------------------------------------------------------ */

int str_equal(const Str *a, const Str *b)
{
    return a->len == b->len && memcmp(a->data, b->data, a->len) == 0;
}

int str_compare(const Str *a, const Str *b)
{
    size_t min_len = a->len < b->len ? a->len : b->len;
    int    cmp     = memcmp(a->data, b->data, min_len);
    if (cmp != 0) return cmp;
    if (a->len < b->len) return -1;
    if (a->len > b->len) return  1;
    return 0;
}

uint64_t str_hash(const Str *s)
{
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < s->len; i++) {
        h ^= (uint64_t)s->data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                          */
/* ------------------------------------------------------------------ */

const char *str_strerror(StrError err)
{
    switch (err) {
    case STR_OK:               return "ok";
    case STR_ERR_INVALID_UTF8: return "invalid UTF-8 sequence";
    case STR_ERR_OOB:          return "index out of bounds";
    case STR_ERR_BAD_BOUNDARY: return "byte offset not on rune boundary";
    case STR_ERR_ALLOC:        return "memory allocation failed";
    default:                   return "unknown error";
    }
}

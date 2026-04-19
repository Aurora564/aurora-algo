#ifndef STR_H
#define STR_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Error codes                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    STR_OK               = 0,
    STR_ERR_INVALID_UTF8 = 1,  /* invalid byte sequence                */
    STR_ERR_OOB          = 2,  /* character index out of bounds        */
    STR_ERR_BAD_BOUNDARY = 3,  /* byte offset not on rune boundary     */
    STR_ERR_ALLOC        = 4,  /* memory allocation failed             */
} StrError;

/* ------------------------------------------------------------------ */
/* Str                                                                  */
/*                                                                      */
/* UTF-8 dynamic string backed by a heap byte buffer.                  */
/* All positions exposed in the API are Unicode code-point (rune)      */
/* indices unless the function name says "bytes".                       */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char *data;   /* heap buffer; NOT NUL-terminated by default */
    size_t         len;    /* used bytes                                  */
    size_t         cap;    /* allocated bytes                             */
} Str;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                            */
/* ------------------------------------------------------------------ */

StrError    str_new(Str *s);
StrError    str_from_cstr(Str *s, const char *cstr);
StrError    str_from_bytes(Str *s, const unsigned char *bytes, size_t n);
void        str_destroy(Str *s);

/* ------------------------------------------------------------------ */
/* Queries                                                              */
/* ------------------------------------------------------------------ */

size_t      str_len_bytes(const Str *s);
size_t      str_len_chars(const Str *s);   /* O(n) rune count          */
size_t      str_cap(const Str *s);
int         str_is_empty(const Str *s);

/* Return a NUL-terminated view (appends '\0' temporarily; caller must *
 * not modify the buffer while the pointer is live).                   */
const char *str_as_cstr(Str *s);

/* ------------------------------------------------------------------ */
/* Access                                                               */
/* ------------------------------------------------------------------ */

/* Write the Unicode code point at rune index i into *cp.              *
 * Returns STR_ERR_OOB if i >= len_chars.                              */
StrError    str_get_char(const Str *s, size_t i, uint32_t *cp);

/* Byte offset of rune index i.                                        *
 * Returns STR_ERR_OOB if i > len_chars (i == len_chars returns len). */
StrError    str_byte_offset(const Str *s, size_t i, size_t *offset);

/* Copy bytes [from_byte, to_byte) into dst (new allocation).          *
 * Returns STR_ERR_BAD_BOUNDARY if either offset is not on a rune      *
 * start, STR_ERR_OOB if offsets are out of range.                    */
StrError    str_slice(const Str *s, size_t from_byte, size_t to_byte,
                      Str *dst);

/* ------------------------------------------------------------------ */
/* Modification                                                         */
/* ------------------------------------------------------------------ */

StrError    str_append(Str *s, const Str *other);
StrError    str_append_cstr(Str *s, const char *cstr);
StrError    str_append_char(Str *s, uint32_t cp);

/* Insert rune cp at rune index i. */
StrError    str_insert(Str *s, size_t i, uint32_t cp);

/* Remove runes [from, to). */
StrError    str_remove_range(Str *s, size_t from, size_t to);

void        str_clear(Str *s);
StrError    str_reserve(Str *s, size_t additional);
StrError    str_shrink_to_fit(Str *s);

/* In-place ASCII case conversion (non-ASCII bytes are untouched). */
void        str_to_upper(Str *s);
void        str_to_lower(Str *s);

/* ------------------------------------------------------------------ */
/* Search                                                               */
/* ------------------------------------------------------------------ */

/* Returns byte offset of first occurrence of needle, or (size_t)-1.  */
size_t      str_find(const Str *s, const Str *needle);
size_t      str_find_cstr(const Str *s, const char *needle);

int         str_contains(const Str *s, const Str *needle);
int         str_starts_with(const Str *s, const Str *prefix);
int         str_ends_with(const Str *s, const Str *suffix);

/* Returns a new Str with leading/trailing ASCII whitespace removed.   */
StrError    str_trim(const Str *s, Str *dst);

/* ------------------------------------------------------------------ */
/* Comparison                                                           */
/* ------------------------------------------------------------------ */

int         str_equal(const Str *a, const Str *b);
int         str_compare(const Str *a, const Str *b);  /* like strcmp   */

/* FNV-1a 64-bit hash of the raw bytes. */
uint64_t    str_hash(const Str *s);

/* ------------------------------------------------------------------ */
/* Diagnostics                                                          */
/* ------------------------------------------------------------------ */

const char *str_strerror(StrError err);

#endif /* STR_H */

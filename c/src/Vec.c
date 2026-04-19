#include "devc.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------ */
/*  内部工具                                                            */
/* ------------------------------------------------------------------ */

static size_t default_grow(size_t cap)
{
    return cap == 0 ? 4 : cap * 2;
}

/*
 * 将容量扩充到至少能容纳 needed 个元素。
 * 分配失败时原数组状态不变（强异常安全）。
 */
static VecError ensure_cap(Vec *v, size_t needed)
{
    if (needed <= v->cap) return VEC_OK;

    size_t new_cap = v->cap == 0 ? 4 : v->cap;
    while (new_cap < needed)
        new_cap = v->grow(new_cap);

    /* malloc + memcpy + free —— 保证分配失败时原 data 不变 */
    void *new_data = malloc(new_cap * v->elem_size);
    if (!new_data) return VEC_ERR_ALLOC;

    if (v->data && v->len > 0)
        memcpy(new_data, v->data, v->len * v->elem_size);

    free(v->data);
    v->data = new_data;
    v->cap  = new_cap;
    return VEC_OK;
}

/* ------------------------------------------------------------------ */
/*  生命周期                                                            */
/* ------------------------------------------------------------------ */

VecError vec_new(Vec *v, size_t elem_size)
{
    VecConfig cfg = { elem_size, 0, NULL, NULL, NULL };
    return vec_new_config(v, &cfg);
}

VecError vec_new_with_cap(Vec *v, size_t elem_size, size_t init_cap)
{
    VecConfig cfg = { elem_size, init_cap, NULL, NULL, NULL };
    return vec_new_config(v, &cfg);
}

VecError vec_new_config(Vec *v, const VecConfig *cfg)
{
    if (!v || !cfg || cfg->elem_size == 0)
        return VEC_ERR_BAD_CONFIG;

    /* 验证自定义扩容函数的合法性 */
    if (cfg->grow && cfg->grow(4) <= 4)
        return VEC_ERR_BAD_CONFIG;

    size_t init_cap  = cfg->init_cap == 0 ? 4 : cfg->init_cap;
    vec_grow_fn grow = cfg->grow ? cfg->grow : default_grow;

    void *data = malloc(init_cap * cfg->elem_size);
    if (!data) return VEC_ERR_ALLOC;

    v->data      = data;
    v->len       = 0;
    v->cap       = init_cap;
    v->elem_size = cfg->elem_size;
    v->grow      = grow;
    v->clone     = cfg->clone;
    v->free_elem = cfg->free_elem;
    return VEC_OK;
}

void vec_destroy(Vec *v)
{
    if (!v) return;
    if (v->free_elem) {
        for (size_t i = 0; i < v->len; i++)
            v->free_elem((char *)v->data + i * v->elem_size);
    }
    free(v->data);
    v->data = NULL;
    v->len  = 0;
    v->cap  = 0;
}

void vec_move(Vec *dst, Vec *src)
{
    *dst      = *src;
    src->data = NULL;
    src->len  = 0;
    src->cap  = 0;
}

VecError vec_copy(Vec *dst, const Vec *src)
{
    /* 浅拷贝：元素逐字节复制，dst 不继承 free_elem（避免双重释放） */
    VecConfig cfg = {
        src->elem_size, src->cap,
        src->grow, src->clone, NULL
    };
    VecError err = vec_new_config(dst, &cfg);
    if (err != VEC_OK) return err;

    if (src->len > 0)
        memcpy(dst->data, src->data, src->len * src->elem_size);
    dst->len = src->len;
    return VEC_OK;
}

VecError vec_clone(Vec *dst, const Vec *src)
{
    assert(src->clone != NULL); /* 编程错误：未提供 clone_fn */
    if (!src->clone) return VEC_ERR_NO_CLONE_FN;

    VecConfig cfg = {
        src->elem_size, src->cap,
        src->grow, src->clone, src->free_elem
    };
    VecError err = vec_new_config(dst, &cfg);
    if (err != VEC_OK) return err;

    for (size_t i = 0; i < src->len; i++) {
        /* clone_fn 返回堆上新副本，memcpy 入缓冲区后释放外壳 */
        void *cloned = src->clone((const char *)src->data + i * src->elem_size);
        if (!cloned) {
            /* 回滚：释放已写入的元素，再释放缓冲区 */
            if (dst->free_elem) {
                for (size_t j = 0; j < i; j++)
                    dst->free_elem((char *)dst->data + j * dst->elem_size);
            }
            free(dst->data);
            dst->data = NULL;
            dst->len = dst->cap = 0;
            return VEC_ERR_ALLOC;
        }
        memcpy((char *)dst->data + i * dst->elem_size, cloned, src->elem_size);
        free(cloned);
    }
    dst->len = src->len;
    return VEC_OK;
}

/* ------------------------------------------------------------------ */
/*  基本读写                                                            */
/* ------------------------------------------------------------------ */

VecError vec_push(Vec *v, const void *elem)
{
    VecError err = ensure_cap(v, v->len + 1);
    if (err != VEC_OK) return err;
    memcpy((char *)v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return VEC_OK;
}

VecError vec_pop(Vec *v, void *out)
{
    if (v->len == 0) return VEC_ERR_EMPTY;
    v->len--;
    if (out)
        memcpy(out, (char *)v->data + v->len * v->elem_size, v->elem_size);
    /* 托管模式：所有权转给调用者，不调用 free_elem */
    return VEC_OK;
}

VecError vec_get(const Vec *v, size_t i, void *out)
{
    if (i >= v->len) return VEC_ERR_OOB;
    memcpy(out, (const char *)v->data + i * v->elem_size, v->elem_size);
    return VEC_OK;
}

VecError vec_set(Vec *v, size_t i, const void *elem)
{
    if (i >= v->len) return VEC_ERR_OOB;
    char *slot = (char *)v->data + i * v->elem_size;
    if (v->free_elem) v->free_elem(slot); /* 释放旧元素内部资源 */
    memcpy(slot, elem, v->elem_size);
    return VEC_OK;
}

VecError vec_insert(Vec *v, size_t i, const void *elem)
{
    if (i > v->len) return VEC_ERR_OOB;
    VecError err = ensure_cap(v, v->len + 1);
    if (err != VEC_OK) return err;

    if (i < v->len)
        memmove((char *)v->data + (i + 1) * v->elem_size,
                (char *)v->data + i       * v->elem_size,
                (v->len - i) * v->elem_size);

    memcpy((char *)v->data + i * v->elem_size, elem, v->elem_size);
    v->len++;
    return VEC_OK;
}

VecError vec_remove(Vec *v, size_t i, void *out)
{
    if (i >= v->len) return VEC_ERR_OOB;
    char *slot = (char *)v->data + i * v->elem_size;
    if (out) memcpy(out, slot, v->elem_size);
    /* 所有权转给调用者，不调用 free_elem */

    if (i < v->len - 1)
        memmove(slot, slot + v->elem_size, (v->len - i - 1) * v->elem_size);
    v->len--;
    return VEC_OK;
}

VecError vec_swap_remove(Vec *v, size_t i, void *out)
{
    if (i >= v->len) return VEC_ERR_OOB;
    char *slot = (char *)v->data + i * v->elem_size;
    if (out) memcpy(out, slot, v->elem_size);
    v->len--;
    /* 末尾元素填补 i（当 i 恰好是末尾时无需复制） */
    if (i < v->len)
        memcpy(slot, (char *)v->data + v->len * v->elem_size, v->elem_size);
    return VEC_OK;
}

/* ------------------------------------------------------------------ */
/*  容量管理                                                            */
/* ------------------------------------------------------------------ */

size_t vec_len(const Vec *v)      { return v->len; }
size_t vec_cap(const Vec *v)      { return v->cap; }
int    vec_is_empty(const Vec *v) { return v->len == 0; }

VecError vec_reserve(Vec *v, size_t additional)
{
    return ensure_cap(v, v->len + additional);
}

VecError vec_shrink_to_fit(Vec *v)
{
    if (v->len == v->cap) return VEC_OK;

    if (v->len == 0) {
        free(v->data);
        v->data = NULL;
        v->cap  = 0;
        return VEC_OK;
    }

    void *new_data = malloc(v->len * v->elem_size);
    if (!new_data) return VEC_ERR_ALLOC;
    memcpy(new_data, v->data, v->len * v->elem_size);
    free(v->data);
    v->data = new_data;
    v->cap  = v->len;
    return VEC_OK;
}

void vec_clear(Vec *v)
{
    if (v->free_elem) {
        for (size_t i = 0; i < v->len; i++)
            v->free_elem((char *)v->data + i * v->elem_size);
    }
    v->len = 0;
    /* 容量不变 */
}

/* ------------------------------------------------------------------ */
/*  批量操作                                                            */
/* ------------------------------------------------------------------ */

VecError vec_extend(Vec *v, const void *src, size_t n)
{
    if (n == 0) return VEC_OK;
    VecError err = ensure_cap(v, v->len + n);
    if (err != VEC_OK) return err;
    memcpy((char *)v->data + v->len * v->elem_size, src, n * v->elem_size);
    v->len += n;
    return VEC_OK;
}

VecError vec_extend_clone(Vec *v, const void *src, size_t n)
{
    assert(v->clone != NULL);
    if (!v->clone) return VEC_ERR_NO_CLONE_FN;
    if (n == 0) return VEC_OK;

    VecError err = ensure_cap(v, v->len + n);
    if (err != VEC_OK) return err;

    size_t base = v->len;
    for (size_t i = 0; i < n; i++) {
        void *cloned = v->clone((const char *)src + i * v->elem_size);
        if (!cloned) {
            /* 回滚：释放本次已克隆的元素 */
            if (v->free_elem) {
                for (size_t j = 0; j < i; j++)
                    v->free_elem((char *)v->data + (base + j) * v->elem_size);
            }
            return VEC_ERR_ALLOC;
        }
        memcpy((char *)v->data + (base + i) * v->elem_size, cloned, v->elem_size);
        free(cloned);
    }
    v->len += n;
    return VEC_OK;
}

VecError vec_slice(const Vec *v, size_t start, size_t end, VecSlice *out)
{
    if (start > end || end > v->len) return VEC_ERR_OOB;
    out->ptr       = (char *)v->data + start * v->elem_size;
    out->len       = end - start;
    out->elem_size = v->elem_size;
    return VEC_OK;
}

void vec_foreach(const Vec *v,
                 void (*fn)(void *elem, void *user_data),
                 void *user_data)
{
    for (size_t i = 0; i < v->len; i++)
        fn((char *)v->data + i * v->elem_size, user_data);
}

/* ------------------------------------------------------------------ */
/*  错误信息                                                            */
/* ------------------------------------------------------------------ */

const char *vec_strerror(VecError err)
{
    switch (err) {
    case VEC_OK:              return "ok";
    case VEC_ERR_OOB:         return "index out of bounds";
    case VEC_ERR_EMPTY:       return "vector is empty";
    case VEC_ERR_ALLOC:       return "memory allocation failed";
    case VEC_ERR_BAD_CONFIG:  return "invalid configuration";
    case VEC_ERR_NO_CLONE_FN: return "clone_fn not provided";
    default:                  return "unknown error";
    }
}

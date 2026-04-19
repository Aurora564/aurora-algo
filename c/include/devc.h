/**
 * devc.h - Dynamic Vector (99)
 *
 * A type-erased, owning dynamic array with configuration:
 *  - growth strategy (default X 2)
 *  - element clone hook (enable deep-copy)
 *  - element free hook (enbales owned/managed elements)
 *
 * Ownership model
 * ---------------
 *   The Vector unconditionally owns its backing buffer (data).
 *   Element Ownership is opt-in:
 *      - free_fn == NULL -> unmanaged (caller owns element resourses)
 *      - free_fn != NULL -> manage (vector calls free_fn on eviction)
 *
 * push() always memcpy-s the element into the buffer.
 *        In managed mode the caller retains the original; the vector 
 *        holds an independent bitwise cope of the struct. If the
 *        element contains pointers and you want the vector to own 
 *        those too, provide a clone_fn and call vec_push_clone().
 *
 * pop() / remove() / swap_remove()
 *        transfer element ownship to the caller (out parameter).
 *
 */

#ifndef VEC_H
#define VEC_H

#include <stddef.h>

/**********************************************************
* 错误码
********************************************************* */ 
typedef enum {
    VEC_OK          = 0,
    VEC_ERR_OOB,            /* 索引越界          */
    VEC_ERR_EMPTY,          /* 对空数组操作       */
    VEC_ERR_ALLOC,          /* 内存分配失败      */
    VEC_ERR_BAD_CONFIG,     /* 非法初始化配置     */
    VEC_ERR_NO_CLONE_FN     /* clone 但未提供钩子 */
} VecError;

/*--------------------------------------------------------
* 钩子函数类型
*---------------------------------------------------------*/
typedef size_t  (*vec_grow_fn)(size_t cap);     /* 扩容策略： cap -> new_cap */
typedef void *  (*vec_clone_fn)(const void *);  /* 深拷贝单个元素，返回堆上新副本*/
typedef void    (*vec_free_fn)(void *);         /* 释放单个元素内部元素 */

/*-------------------------------------------------------
* 初始化配置
*-------------------------------------------------------*/
typedef struct {
    size_t elem_size;       /* 必填                      */
    size_t init_cap;        /* 必填， 0 -> 默认 4         */
    vec_grow_fn grow;       /* 选填， NULL -> 2           */
    vec_clone_fn clone;     /* 选填： NULL -> 不支持深拷贝 */
    vec_free_fn free_elem;  /* 选填， NULL -> 不托管元素资源*/
} VecConfig;

/* ------------------------------------------------------
 * 核心数据结构
 * -----------------------------------------------------*/
typedef struct {
    void        *data;
    size_t      len;
    size_t      cap;
    size_t      elem_size;
    vec_grow_fn grow;
    vec_clone_fn clone;
    vec_free_fn free_elem;
} Vec;

/* ----------------------------------------------------
 * 借用试图（slice）
 * 调用者持有试图期间，不得对原数组执行任何可能触发
 * 重新分配的操作（push/insert/reserve 等）
 * ---------------------------------------------------*/
typedef struct {
    void *ptr;
    size_t len;
    size_t elem_size;
} VecSlice;


/* =====================================================
 * API 
 * ====================================================*/ 

/*-- 生命周期 ----------------------------------------*/ 

/** 创建空数组， 不托管元素资源 */ 
VecError vec_new(Vec *v, size_t elem_size);

/** 创建并预分配 init_cap 容量 */ 
VecError vec_new_with_cap(Vec *v, size_t elem_size, size_t init_cap);

/** 完整配置初始化 */ 
VecError vec_new_config(Vec *v, const VecConfig *cfg);

/** 销毁， 托管模式下逐元素调用 free_elem, 然后释放缓冲区 */
void vec_destroy(Vec *v);

/** O(1) 转移所有权；转移后 src->data == NULL, len/cap == 0 */ 
void vec_move(Vec *dst, Vec *src);

/** 浅拷贝： dst 获取独立缓冲区，元素逐字节复制 */ 
VecError vec_copy(Vec *dst, const Vec *src);

/** 深拷贝： 逐元素调用clone_fn;需要src->clone != NULL; */ 
VecError vec_clone(Vec *dst,const Vec *src);


/*-- 基本读写 ----------------------------------------*/ 

/** 追加到末尾， 必要时扩容；均摊 O(1) */ 
VecError vec_push(Vec *v, const void *elem);

/** 移除并返回末尾元素到 out;托管模式下所有权转给调用者 */ 
VecError vec_pop(Vec *v, void *out);

/** 读取索引 i 处元素到 out； O(1) */
VecError vec_get(const Vec *v, size_t i, void *out);

/** 覆写索引 i 处元素；托管模式下先调用旧元素 free_elem */
VecError vec_set(Vec *v, size_t i, const void *elem);

/** 在i 处插入， 后续元素后移； O(n) */ 
VecError vec_insert(Vec *v, size_t i, const void *out);


/** 移除 i 处元素到out， 后续元素前移；所有权转给调用者；O(n); */
VecError vec_remove(Vec *v, size_t i, void *out);

/** 移除 i 处元素到out，末尾元素填补（不保序) */ 
VecError vec_swap_remove(Vec *v, size_t i, void *out);


/* 容量管理 ------------------------------------------*/ 

size_t vec_len(const Vec *v);
size_t vec_cap(const Vec *v);
int vec_is_empty(const Vec *v);

/** 确保至少还有 additional 个空槽位 */ 
VecError vec_reserve(Vec *v, size_t additional);

/** 容量缩减至 len */ 
VecError vec_shrink_to_fit(Vec *v);

/** 移除所有元素：托管模式下调用各元素 free_elem；容量不变 */
void vec_clear(Vec *v);


/*---批量操作 ---------------------------------------*/ 

/** 追加 n 个元素（浅拷贝） */ 
VecError vec_extend(Vec *v, const void *src, size_t n);

/** 追加 n个元素（深拷贝，需要clone_fn）*/ 
VecError vec_extend_clone(Vec *v, const void *src, size_t n);

/** 返回 [start, end) 的借用试图：O(1) */ 
VecError vec_slice(const Vec *v, size_t start, size_t end, VecSlice *out);

/** 对每个元素调用 fn(elem, user_data) */ 
void vec_foreach(const Vec *v, void (*fn)(void *elem, void *user_data), void *user_data);

/*----错误信息 ---------------------------*/ 
const char *vec_strerror(VecError err);


#endif 

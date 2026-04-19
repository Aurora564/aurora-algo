/**
 * SList.h — Singly Linked List (C99)
 *
 * 结构设计：
 *  - 哨兵头节点（dummy head）：始终存在，val == NULL，消除头部插入的特判。
 *  - tail 指针：指向最后一个真实节点（或 dummy，当链表为空时），O(1) push_back。
 *  - len 计数器：O(1) 查询长度。
 *
 * 所有权模型：
 *  - 节点本身（SLNode）始终由链表管理。
 *  - 节点值（val）默认不托管：链表只存储指针，不干涉指针指向的内存。
 *  - 托管模式：初始化时提供 free_fn，链表在节点移除或销毁时调用它。
 *
 * pop_front / pop_back / remove_after：
 *  - 将 val 所有权转移给调用者（通过 out 参数），不调用 free_fn。
 *
 * pop_back：O(n)，需从头遍历找前驱——单链表固有缺陷。
 */

#ifndef SLIST_H
#define SLIST_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    SL_OK            = 0,
    SL_ERR_EMPTY,        /* 对空链表 pop/peek    */
    SL_ERR_OOB,          /* 索引或节点操作越界   */
    SL_ERR_ALLOC,        /* 节点内存分配失败     */
    SL_ERR_NO_CLONE_FN   /* clone 但未提供钩子   */
} SListError;

/* ------------------------------------------------------------------ */
/*  钩子函数类型                                                        */
/* ------------------------------------------------------------------ */

typedef void  (*sl_free_fn)(void *val);          /* 释放单个值的内部资源 */
typedef void *(*sl_clone_fn)(const void *val);   /* 深拷贝单个值，返回堆上新副本 */

/* ------------------------------------------------------------------ */
/*  核心数据结构                                                        */
/* ------------------------------------------------------------------ */

typedef struct SLNode {
    void          *val;    /* 指向外部值；托管模式下链表拥有此指针 */
    struct SLNode *next;
} SLNode;

typedef struct {
    SLNode     *dummy;     /* 哨兵头节点；dummy->val == NULL，永不为 NULL */
    SLNode     *tail;      /* 末尾真实节点；空链表时 tail == dummy */
    size_t      len;       /* 真实节点数量 */
    sl_free_fn  free_val;  /* NULL = 不托管 */
    sl_clone_fn clone_val; /* NULL = 不支持深拷贝 */
} SList;

/* =====================================================================
 * API
 * ==================================================================== */

/*-- 生命周期 ----------------------------------------------------------*/

/** 创建空链表，不托管节点值资源 */
SListError slist_new(SList *list);

/** 创建链表，注入值的生命周期钩子 */
SListError slist_new_managed(SList *list, sl_free_fn free_val, sl_clone_fn clone_val);

/** 销毁链表；托管模式下逐节点调用 free_val，然后释放所有节点和哨兵 */
void slist_destroy(SList *list);

/** O(1) 转移所有权；转移后 src->dummy == NULL，src->len == 0 */
void slist_move(SList *dst, SList *src);

/** 浅拷贝：dst 获得独立节点链，val 指针逐个复制（不拷贝指向内容）*/
SListError slist_copy(SList *dst, const SList *src);

/** 深拷贝：逐节点调用 clone_val；需要 src->clone_val != NULL */
SListError slist_clone(SList *dst, const SList *src);

/*-- 基本读写 ----------------------------------------------------------*/

/** 在头部插入 val；O(1) */
SListError slist_push_front(SList *list, void *val);

/** 在尾部插入 val；O(1) */
SListError slist_push_back(SList *list, void *val);

/** 移除头部节点，val 写入 *out（可为 NULL）；O(1) */
SListError slist_pop_front(SList *list, void **out);

/** 移除尾部节点，val 写入 *out（可为 NULL）；O(n) */
SListError slist_pop_back(SList *list, void **out);

/** 查看头部 val，不移除；O(1) */
SListError slist_peek_front(const SList *list, void **out);

/** 查看尾部 val，不移除；O(1) */
SListError slist_peek_back(const SList *list, void **out);

/** 在 node 之后插入 val；node 必须属于该链表；O(1) */
SListError slist_insert_after(SList *list, SLNode *node, void *val);

/** 移除 node 之后的节点，val 写入 *out；若 node 为尾节点返回 SL_ERR_OOB；O(1) */
SListError slist_remove_after(SList *list, SLNode *node, void **out);

/** 返回索引 i 处节点的 val（从0开始）；O(n) */
SListError slist_get(const SList *list, size_t i, void **out);

/** 返回第一个满足 pred 的节点；未找到返回 NULL；O(n) */
SLNode *slist_find(const SList *list, int (*pred)(const void *val));

/*-- 容量 -------------------------------------------------------------*/

size_t slist_len(const SList *list);
int    slist_is_empty(const SList *list);

/** 移除所有节点；托管模式下调用各节点 free_val */
void slist_clear(SList *list);

/*-- 遍历 -------------------------------------------------------------*/

/** 对每个节点的 val 调用 fn(val, user_data) */
void slist_foreach(const SList *list,
                   void (*fn)(void *val, void *user_data),
                   void *user_data);

/** 原地反转链表；O(n) */
void slist_reverse(SList *list);

/*-- 错误信息 ---------------------------------------------------------*/

const char *slist_strerror(SListError err);

#endif /* SLIST_H */

# Doubly Linked List 设计文档

> 适用语言：C (C99+) · C++ (C++17+) · Go (1.18+)
> 文档版本：1.0

---

## 目录

1. [核心语义](#一核心语义)
2. [关键语义决策](#二关键语义决策)
3. [接口设计](#三接口设计)
4. [异常处理](#四异常处理)
5. [语言实现](#五语言实现)
6. [功能测试](#六功能测试)
7. [基准测试](#七基准测试)

---

## 一、核心语义

Doubly Linked List 是每个节点同时持有**前驱指针和后继指针**的双向链表，提供以下核心保证：

- **双向遍历**：可从任意节点向前或向后遍历。
- **O(1) 插入/删除**：给定节点指针时，插入其前后或删除该节点均为 O(1)。
- **O(1) 双端操作**：`push_front`、`push_back`、`pop_front`、`pop_back` 全部 O(1)。
- **所有权明确**：链表对所有节点拥有唯一所有权，值的所有权由注入的钩子函数决定。

与单链表的核心区别：

| 维度 | Singly Linked List | Doubly Linked List |
|------|-------------------|-------------------|
| 每节点指针数 | 1 (`next`) | 2 (`prev` + `next`) |
| `pop_back` | O(n) | O(1) |
| 给定节点删除 | O(n)（需找前驱）| O(1) |
| 内存开销 | 低 | 较高（多一个指针）|

适用场景：需要双端 O(1) 操作、给定节点 O(1) 删除（如 LRU Cache）、双向遍历。

---

## 二、关键语义决策

### 2.1 哨兵节点方案：单哨兵循环 vs 双哨兵

**决策：使用单哨兵循环链表（Circular Dummy Head）。**

两种主流方案对比：

| 方案 | 结构 | 边界处理 |
|------|------|---------|
| 双哨兵（head_sentinel + tail_sentinel）| 线性，两端各一虚拟节点 | 简单，插入/删除无特判 |
| 单哨兵循环（dummy.prev = tail, dummy.next = head）| 循环，一个虚拟节点兼任头尾 | 最简，所有操作统一 |

选择单哨兵循环方案：
- 只需一个哨兵节点，空间开销更小。
- `dummy.prev` 始终指向尾节点，`dummy.next` 始终指向头节点。
- 插入到 `dummy` 之前 = 尾部追加；插入到 `dummy` 之后 = 头部插入。
- 所有插入/删除操作代码完全统一，无空指针边界判断。

```
空链表：  dummy ←→ dummy  (dummy.prev = dummy, dummy.next = dummy)
单节点：  dummy ←→ node ←→ dummy
多节点：  dummy ←→ n1 ←→ n2 ←→ ... ←→ nk ←→ dummy
```

### 2.2 是否维护独立的 `len` 计数器？

**决策：维护 `len` 计数器（O(1) 查询长度）。**

遍历计算长度为 O(n)，对调用者不友好。`len` 计数器在所有修改操作中维护，成本极低。

### 2.3 节点暴露策略

**决策：暴露不透明的节点指针（opaque node pointer）用于游标操作。**

双链表的核心价值之一是"给定节点 O(1) 删除"，这要求调用者能持有节点的引用。设计权衡：

- **暴露裸节点指针**（C）：高效，调用者负责类型安全，不得访问 `prev`/`next`。
- **迭代器/句柄**（C++）：封装节点指针，提供安全的间接访问接口。
- **节点引用**（Go）：返回 `*Node`，`Val` 字段可读写，`prev`/`next` 为私有。

核心契约：调用者持有的节点句柄在节点被链表删除后立即失效，后续使用属于未定义行为。

### 2.4 迭代器失效规则

与单链表相同：
- 插入操作不使任何已有迭代器失效。
- 删除操作仅使被删除节点的迭代器失效。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new()` | 创建空链表（含循环哨兵节点）| O(1) |
| `new_managed(free_fn, clone_fn)` | 注入值钩子 | O(1) |
| `destroy(list)` | 销毁；托管模式逐节点调用 `free_fn` | O(n) |
| `move(dst, src)` | 转移所有权 | O(1) |
| `copy(dst, src)` | 浅拷贝各节点值 | O(n) |
| `clone(dst, src)` | 深拷贝，需 `clone_fn` | O(n) |

### 3.2 基本读写

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `push_front(list, val)` | 头部插入 | O(1) |
| `push_back(list, val)` | 尾部插入 | O(1) |
| `pop_front(list)` | 移除并返回头部元素 | O(1) |
| `pop_back(list)` | 移除并返回尾部元素 | O(1) |
| `peek_front(list)` | 返回头部元素引用 | O(1) |
| `peek_back(list)` | 返回尾部元素引用 | O(1) |
| `insert_before(list, node, val)` | 在 node 前插入 | O(1) |
| `insert_after(list, node, val)` | 在 node 后插入 | O(1) |
| `remove(list, node)` | 移除指定节点 | O(1) |
| `get(list, i)` | 按索引查找（从近端遍历）| O(n) |
| `find(list, pred)` | 返回第一个满足 pred 的节点 | O(n) |

> `get(i)` 优化：若 `i < len/2` 从头遍历，否则从尾遍历，均摊仍 O(n) 但常数减半。

### 3.3 容量查询

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(list)` | 节点数量 | O(1) |
| `is_empty(list)` | 是否为空 | O(1) |
| `clear(list)` | 移除所有节点，容量（N/A，无缓冲区）不变 | O(n) |

### 3.4 遍历

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `foreach(list, fn)` | 正向遍历每个节点 | O(n) |
| `foreach_rev(list, fn)` | 反向遍历每个节点 | O(n) |
| `reverse(list)` | 原地反转（交换所有节点的 prev/next）| O(n) |
| `splice(dst, pos, src)` | 将 src 整体接入 dst 的 pos 之后 | O(1) |

> `splice` 是双链表的特有高效操作，可在 O(1) 内将一个完整链表插入另一个链表的任意位置（需更新两端哨兵指针）。

---

## 四、异常处理

与单链表相同，参见 `2-singly-linked-list.md` 第四节。额外补充：

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_DANGLING_NODE` | `remove`/`insert_*` 传入不属于该链表的节点 | Debug: `assert`；Release: UB |

节点归属验证在 Debug 构建中通过在每个节点中存储链表指针回指实现（增加一个 `owner` 字段），Release 构建中省略以节省内存。

---

## 五、语言实现

### 5.1 C

```c
typedef void  (*free_fn_t)(void *val);
typedef void* (*clone_fn_t)(const void *val);

typedef struct DLNode {
    void           *val;
    struct DLNode  *prev;
    struct DLNode  *next;
} DLNode;

typedef struct {
    DLNode     *dummy;     // 循环哨兵节点
    size_t      len;
    free_fn_t   free_val;
    clone_fn_t  clone_val;
} DList;

typedef enum {
    DL_OK = 0, DL_ERR_EMPTY, DL_ERR_OOB, DL_ERR_ALLOC
} DListError;

/* 内部辅助（不暴露）*/
static void dl_link_before(DLNode *ref, DLNode *node);  // 在 ref 前插入 node
static void dl_unlink(DLNode *node);                     // 摘除 node（不释放）

/* 生命周期 */
DListError dlist_new(DList *list);
DListError dlist_new_managed(DList *list, free_fn_t free_val, clone_fn_t clone_val);
void       dlist_destroy(DList *list);
void       dlist_move(DList *dst, DList *src);
DListError dlist_copy(DList *dst, const DList *src);
DListError dlist_clone(DList *dst, const DList *src);

/* 读写 */
DListError dlist_push_front(DList *list, void *val);
DListError dlist_push_back(DList *list, void *val);
DListError dlist_pop_front(DList *list, void **out);
DListError dlist_pop_back(DList *list, void **out);
DListError dlist_peek_front(const DList *list, void **out);
DListError dlist_peek_back(const DList *list, void **out);
DListError dlist_insert_before(DList *list, DLNode *node, void *val);
DListError dlist_insert_after(DList *list, DLNode *node, void *val);
DListError dlist_remove(DList *list, DLNode *node, void **out);
DListError dlist_get(const DList *list, size_t i, void **out);
DLNode*    dlist_find(const DList *list, int (*pred)(const void *val));

/* 容量 */
size_t     dlist_len(const DList *list);
int        dlist_is_empty(const DList *list);
void       dlist_clear(DList *list);

/* 遍历 */
void       dlist_foreach(DList *list, void (*fn)(void *val));
void       dlist_foreach_rev(DList *list, void (*fn)(void *val));
void       dlist_reverse(DList *list);
DListError dlist_splice(DList *dst, DLNode *pos, DList *src);
```

> **C 实现注意**：`dl_link_before(ref, node)` 是所有插入操作的基础原语：`push_back` = `dl_link_before(dummy, node)`，`push_front` = `dl_link_before(dummy->next, node)`。循环结构意味着哨兵节点的 `prev`/`next` 在空链表时均指向自身，消除了所有 NULL 判断。

### 5.2 C++

```cpp
template<typename T>
class DList {
public:
    struct Node {
        T     val;
        Node *prev = nullptr;
        Node *next = nullptr;
    };

    struct Iterator {
        Node *ptr;
        T& operator*()  { return ptr->val; }
        Iterator& operator++() { ptr = ptr->next; return *this; }
        Iterator& operator--() { ptr = ptr->prev; return *this; }
        bool operator==(const Iterator& o) const { return ptr == o.ptr; }
    };

    DList();
    DList(const DList& other);
    DList(DList&& other) noexcept;
    DList& operator=(DList other) noexcept;
    ~DList();

    DListError push_front(const T& val);
    DListError push_front(T&& val);
    DListError push_back(const T& val);
    DListError push_back(T&& val);
    std::expected<T, DListError> pop_front();
    std::expected<T, DListError> pop_back();
    std::expected<std::reference_wrapper<T>, DListError> peek_front();
    std::expected<std::reference_wrapper<T>, DListError> peek_back();

    DListError           insert_before(Iterator pos, T val);
    DListError           insert_after(Iterator pos, T val);
    std::expected<T, DListError> erase(Iterator pos);

    std::expected<std::reference_wrapper<T>, DListError> get(size_t i);
    Iterator find(std::function<bool(const T&)> pred);

    size_t len()      const noexcept;
    bool   is_empty() const noexcept;
    void   clear();
    void   reverse() noexcept;
    void   splice(Iterator pos, DList& other);  // 将 other 全部接入 pos 之后，O(1)

    Iterator begin() { return {dummy_->next}; }
    Iterator end()   { return {dummy_};        }
    // rbegin/rend 利用 prev 指针实现反向迭代器

private:
    Node  *dummy_ = nullptr;
    size_t len_   = 0;

    void link_before(Node *ref, Node *node) noexcept;
    void unlink(Node *node) noexcept;
};
```

> **C++ 实现注意**：`splice` 操作只需重接 4 个指针，无需遍历，是 `std::list::splice` 相同语义的手动实现。`Iterator` 支持双向遍历（`operator++` / `operator--`）。析构时从 `dummy_->next` 遍历直到回到 `dummy_`，避免访问 NULL。

### 5.3 Go

```go
type DListNode[T any] struct {
    Val  T
    prev *DListNode[T]
    next *DListNode[T]
}

type DList[T any] struct {
    dummy *DListNode[T]  // 循环哨兵
    len   int
}

func NewDList[T any]() *DList[T]

func (l *DList[T]) PushFront(val T)
func (l *DList[T]) PushBack(val T)
func (l *DList[T]) PopFront() (T, error)
func (l *DList[T]) PopBack() (T, error)
func (l *DList[T]) PeekFront() (T, error)
func (l *DList[T]) PeekBack() (T, error)

func (l *DList[T]) InsertBefore(node *DListNode[T], val T)
func (l *DList[T]) InsertAfter(node *DListNode[T], val T)
func (l *DList[T]) Remove(node *DListNode[T]) T

func (l *DList[T]) Get(i int) (T, error)
func (l *DList[T]) Find(pred func(T) bool) *DListNode[T]

func (l *DList[T]) Len() int
func (l *DList[T]) IsEmpty() bool
func (l *DList[T]) Clear()
func (l *DList[T]) Reverse()
func (l *DList[T]) Splice(pos *DListNode[T], other *DList[T])

func (l *DList[T]) ForEach(fn func(T))
func (l *DList[T]) ForEachRev(fn func(T))
func (l *DList[T]) All() iter.Seq[T]       // Go 1.23+
func (l *DList[T]) Backward() iter.Seq[T]  // Go 1.23+

// 内部原语（unexported）
func (l *DList[T]) linkBefore(ref, node *DListNode[T])
func (l *DList[T]) unlink(node *DListNode[T])
```

> **Go 实现注意**：`dummy` 节点在空链表时 `dummy.prev = dummy`，`dummy.next = dummy`。`Remove` 直接返回 `T` 而非 `(T, error)`，因为传入有效节点是调用者契约，Go 惯例对编程错误使用 panic 而非 error。

---

## 六、功能测试

### 6.1 测试分类

| 类别 | 覆盖内容 |
|------|---------|
| 正常路径 | 全部双端操作的基础语义 |
| 边界条件 | 空链表、单节点、两节点时所有操作 |
| 哨兵完整性 | 每次操作后验证 `dummy.next.prev == dummy` 且 `dummy.prev.next == dummy` |
| 给定节点操作 | insert_before/after、remove 给定节点 |
| 双向遍历 | foreach 与 foreach_rev 遍历顺序互为逆序 |
| splice | splice 后 src 变空，dst 节点顺序正确 |
| get 优化 | 前半段从头遍历，后半段从尾遍历 |
| 所有权 | 与单链表相同 |

### 6.2 关键测试用例

```
TC-01  push_back N 个元素，pop_back 顺序为 push 顺序的逆序（LIFO from back）
TC-02  pop_front 空链表返回 ERR_EMPTY，pop_back 同理
TC-03  insert_before(node) 后 node.prev 为新节点
TC-04  insert_after(node) 后 node.next 为新节点
TC-05  remove(node) 后 len 减 1，node 的前驱后继相互连接
TC-06  循环哨兵完整性：操作后 dummy.prev.next == dummy（invariant check）
TC-07  reverse 后原顺序逆转，哨兵仍正确
TC-08  splice(pos, src)：src 变空，dst 在 pos 后多出 src 的所有节点
TC-09  get(i) 对 i < len/2 从头查，对 i >= len/2 从尾查
TC-10  双向遍历：foreach 结果与 foreach_rev 结果互逆
```

---

## 七、基准测试

| 场景 | 目的 |
|------|------|
| push_front/push_back N | 双端插入吞吐量 |
| pop_front/pop_back N | 双端删除对比（均 O(1)）|
| 中间删除（给定节点）| 体现 O(1) 任意位置删除的优势 |
| splice N 节点 | 验证 O(1) 接链 |
| 顺序遍历 vs Vector | 量化链表内存不连续的缓存代价 |
| 反向遍历 | 双向遍历的对称性验证 |

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` | 单链表 `pop_back`（O(n)）|
| C++ | Google Benchmark | `std::list<T>` |
| Go | `testing.B` | `container/list` |

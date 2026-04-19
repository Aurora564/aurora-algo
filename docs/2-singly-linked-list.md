# Singly Linked List 设计文档

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

Singly Linked List 是一个管理**通过单向指针链接的节点序列**的容器，提供以下核心保证：

- **链式性**：每个节点持有指向下一节点的指针，无法 O(1) 随机访问。
- **同质性**：所有节点存储相同类型的值，节点大小在初始化时确定。
- **动态性**：每次插入/删除均按需分配/释放节点，无容量上限（受限于堆内存）。
- **所有权明确**：链表对所有节点拥有唯一所有权；节点内值的资源所有权由初始化时注入的钩子函数决定。

与 Vector 的核心区别：

| 维度 | Vector | Singly Linked List |
|------|--------|-------------------|
| 内存布局 | 连续 | 分散（每节点独立分配）|
| 随机访问 | O(1) | O(n) |
| 头部插入 | O(n) | O(1) |
| 缓存友好性 | 高 | 低 |
| 内存开销 | 低（无指针） | 高（每节点一个指针）|

适用场景：频繁头部插入/删除、不需要随机访问、链表长度变化剧烈时。

---

## 二、关键语义决策

### 2.1 是否使用头哨兵节点？

**决策：使用哨兵头节点（dummy head）。**

哨兵节点是一个不存储有效数据的虚拟节点，始终作为链表的第一个节点存在。

优势：
- 消除"链表为空"的特判，所有插入/删除操作逻辑统一。
- `insert_after(sentinel, x)` 即为头部插入，无需特殊处理 `head == NULL` 的分支。

代价：额外分配一个节点的内存（可忽略）。

> 注意：`tail` 指针直接指向最后一个真实节点（或哨兵节点，当链表为空时）。这保证了 O(1) 尾部追加，同时保持头部插入的 O(1)。

### 2.2 是否维护 `tail` 指针？

**决策：维护 `tail` 指针。**

不维护时，`push_back` 需要遍历到末尾，为 O(n)。维护 `tail` 后：
- `push_back`：O(1)
- `push_front`：O(1)
- `pop_front`：O(1)
- `pop_back`：O(n)（单链表无法反向遍历，需从头找前驱）

`pop_back` 的 O(n) 代价是单链表的固有缺陷；需要 O(1) `pop_back` 时应使用双链表。

### 2.3 节点内存管理策略

**决策：每个节点独立 `malloc`/`free`，不使用节点池。**

节点池（arena allocator）可以减少 `malloc` 次数，提升缓存局部性，但大幅增加实现复杂度，且在节点频繁删除场景下碎片管理困难。基础实现使用朴素策略，性能敏感场景可在此接口之上替换分配器。

### 2.4 节点值的所有权语义

与 Vector 一致：
- **不托管模式（默认）**：链表只负责节点本身的内存，不干涉节点值内部资源。
- **托管模式**：初始化时注入 `free_fn`，在节点被移除/链表销毁时调用。

`pop`/`remove` 返回值时：将值所有权转移给调用者，**不调用** `free_fn`。

### 2.5 迭代器失效规则

- 插入操作：不使任何已有迭代器失效（链表不重新分配内存）。
- 删除操作：仅使指向被删除节点的迭代器失效，其余迭代器仍然有效。

这是链表相比 Vector 的关键优势：插入/删除不会使其他位置的迭代器失效。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new()` | 创建空链表（含哨兵节点），不托管元素资源 | O(1) |
| `new_with_free(free_fn)` | 创建链表，注入元素释放钩子 | O(1) |
| `destroy(list)` | 销毁；托管模式逐节点调用 `free_fn` | O(n) |
| `move(dst, src)` | 转移所有权，src 进入已移动状态 | O(1) |
| `copy(dst, src)` | 浅拷贝每个节点值，创建独立链表 | O(n) |
| `clone(dst, src)` | 深拷贝，需 `clone_fn != NULL` | O(n) |

### 3.2 基本读写

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `push_front(list, val)` | 在头部插入 | O(1) |
| `push_back(list, val)` | 在尾部插入 | O(1) |
| `pop_front(list)` | 移除并返回头部元素 | O(1) |
| `pop_back(list)` | 移除并返回尾部元素 | O(n) |
| `peek_front(list)` | 返回头部元素引用（不移除）| O(1) |
| `peek_back(list)` | 返回尾部元素引用（不移除）| O(1) |
| `insert_after(node, val)` | 在 node 之后插入 | O(1) |
| `remove_after(node)` | 移除 node 之后的节点 | O(1) |
| `get(list, i)` | 返回索引 i 处元素（遍历）| O(n) |
| `find(list, pred)` | 返回第一个满足 pred 的节点 | O(n) |

> `insert_after` / `remove_after` 是单链表的自然操作原语。直接操作节点指针时需通过迭代器/游标 API 暴露，不直接暴露裸指针（C 除外）。

### 3.3 容量查询

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(list)` | 当前节点数 | O(1)（维护计数器）|
| `is_empty(list)` | 是否为空 | O(1) |
| `clear(list)` | 移除所有节点，托管模式调用各节点 `free_fn` | O(n) |

### 3.4 遍历

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `foreach(list, fn)` | 对每个节点值调用 `fn` | O(n) |
| `iter(list)` | 返回迭代器（Go: channel 或 iter.Seq）| — |
| `reverse(list)` | 原地反转链表 | O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_EMPTY` | 对空链表 pop/peek | 返回错误码 |
| `ERR_OUT_OF_BOUNDS` | `get(i)` 且 `i >= len` | 返回错误码 |
| `ERR_ALLOC` | 节点分配失败 | 返回错误码，链表状态不变 |
| `ERR_NO_CLONE_FN` | 调用 clone 但未提供 `clone_fn` | `assert`（编程错误）|

### 4.2 内存分配失败的状态保证

所有插入操作在节点分配失败时，链表保持与调用前完全相同的状态（强异常安全）：节点分配与链接操作原子化——先分配，失败则直接返回，链表结构不被修改。

---

## 五、语言实现

### 5.1 C

```c
typedef void  (*free_fn_t)(void *val);
typedef void* (*clone_fn_t)(const void *val);

typedef struct SLNode {
    void           *val;       // 拥有或借用，取决于 free_fn
    struct SLNode  *next;
} SLNode;

typedef struct {
    SLNode     *head;          // 指向哨兵节点
    SLNode     *tail;          // 指向最后一个真实节点（或哨兵）
    size_t      len;
    free_fn_t   free_val;      // NULL 表示不托管
    clone_fn_t  clone_val;     // NULL 表示不支持深拷贝
} SList;

typedef enum {
    SL_OK = 0, SL_ERR_EMPTY, SL_ERR_OOB, SL_ERR_ALLOC
} SListError;

/* 生命周期 */
SListError slist_new(SList *list);
SListError slist_new_managed(SList *list, free_fn_t free_val, clone_fn_t clone_val);
void       slist_destroy(SList *list);
void       slist_move(SList *dst, SList *src);
SListError slist_copy(SList *dst, const SList *src);
SListError slist_clone(SList *dst, const SList *src);

/* 读写 */
SListError slist_push_front(SList *list, void *val);
SListError slist_push_back(SList *list, void *val);
SListError slist_pop_front(SList *list, void **out);
SListError slist_pop_back(SList *list, void **out);
SListError slist_peek_front(const SList *list, void **out);
SListError slist_peek_back(const SList *list, void **out);
SListError slist_insert_after(SList *list, SLNode *node, void *val);
SListError slist_remove_after(SList *list, SLNode *node, void **out);
SListError slist_get(const SList *list, size_t i, void **out);
SLNode*    slist_find(const SList *list, int (*pred)(const void *val));

/* 容量 */
size_t     slist_len(const SList *list);
int        slist_is_empty(const SList *list);
void       slist_clear(SList *list);

/* 遍历 */
void       slist_foreach(SList *list, void (*fn)(void *val));
void       slist_reverse(SList *list);
```

> **C 实现注意**：节点值通过 `void *` 存储，由调用者负责类型安全。哨兵节点的 `val` 字段始终为 `NULL`，`free_val` 不对哨兵调用。`tail` 在空链表时指向哨兵节点（`head`）。

### 5.2 C++

```cpp
template<typename T>
class SList {
public:
    struct Node {
        T     val;
        Node *next = nullptr;
    };

    SList() = default;
    SList(const SList& other);
    SList(SList&& other) noexcept;
    SList& operator=(SList other) noexcept;
    ~SList();

    VecError push_front(const T& val);
    VecError push_front(T&& val);
    VecError push_back(const T& val);
    VecError push_back(T&& val);
    std::expected<T, SListError> pop_front();
    std::expected<T, SListError> pop_back();
    std::expected<std::reference_wrapper<T>, SListError>       peek_front();
    std::expected<std::reference_wrapper<const T>, SListError> peek_front() const;
    std::expected<std::reference_wrapper<T>, SListError>       peek_back();

    SListError insert_after(Node *node, T val);
    std::expected<T, SListError> remove_after(Node *node);

    std::expected<std::reference_wrapper<T>, SListError> get(size_t i);
    Node* find(std::function<bool(const T&)> pred);

    size_t len()      const noexcept;
    bool   is_empty() const noexcept;
    void   clear();
    void   reverse() noexcept;

    // 范围遍历支持（C++20 range-based for）
    struct Iterator { /* ... */ };
    Iterator begin();
    Iterator end();

private:
    Node  *dummy_ = nullptr;   // 哨兵节点（new 时分配，不构造 T）
    Node  *tail_  = nullptr;   // 指向最后一个真实节点或 dummy_
    size_t len_   = 0;
};
```

> **C++ 实现注意**：哨兵节点使用 `operator new(sizeof(Node))` 分配但不调用 `T` 的构造函数（`std::allocator_traits` 或 placement new 控制），避免对 T 有默认构造函数的要求。移动构造/赋值必须 `noexcept`，析构须处理 `dummy_ == nullptr` 的情况（已移走状态）。

### 5.3 Go

```go
type SListNode[T any] struct {
    Val  T
    next *SListNode[T]
}

type SList[T any] struct {
    dummy *SListNode[T]   // 哨兵节点
    tail  *SListNode[T]   // 指向最后一个真实节点或 dummy
    len   int
}

type SListError int
const (
    ErrEmpty       SListError = iota + 1
    ErrOutOfBounds
    ErrAlloc
)

func NewSList[T any]() *SList[T]

func (l *SList[T]) PushFront(val T)
func (l *SList[T]) PushBack(val T)
func (l *SList[T]) PopFront() (T, error)
func (l *SList[T]) PopBack() (T, error)
func (l *SList[T]) PeekFront() (T, error)
func (l *SList[T]) PeekBack() (T, error)
func (l *SList[T]) InsertAfter(node *SListNode[T], val T)
func (l *SList[T]) RemoveAfter(node *SListNode[T]) (T, error)
func (l *SList[T]) Get(i int) (T, error)
func (l *SList[T]) Find(pred func(T) bool) *SListNode[T]

func (l *SList[T]) Len() int
func (l *SList[T]) IsEmpty() bool
func (l *SList[T]) Clear()
func (l *SList[T]) Reverse()
func (l *SList[T]) ForEach(fn func(T))

// Go 1.23+ iter.Seq 支持
func (l *SList[T]) All() iter.Seq[T]
```

> **Go 实现注意**：哨兵节点的 `Val` 字段为 T 的零值，不应被外部访问。`tail` 空链表时指向 `dummy`。由于 Go 有 GC，`PopFront`/`PopBack` 只需解链，不需手动释放节点。

---

## 六、功能测试

### 6.1 测试分类

| 类别 | 覆盖内容 |
|------|---------|
| 正常路径 | push_front/push_back/pop_front/pop_back 基础语义 |
| 边界条件 | 空链表、单节点、连续 push 后 pop 至空 |
| 错误路径 | 空链表 pop/peek 返回错误，越界 get |
| 哨兵节点 | 插入第一个元素后 tail 指向正确，删除最后一个元素后 tail 回到哨兵 |
| 所有权语义 | 托管模式 clear/destroy 的 free_fn 调用次数，clone 独立性 |
| 反转 | 反转后顺序正确，单节点/空链表反转安全 |
| 迭代 | foreach 遍历顺序，修改值的回调 |

### 6.2 关键测试用例

```
TC-01  push_back N 个元素，顺序 pop_front，验证 FIFO 顺序
TC-02  push_front N 个元素，顺序 pop_front，验证 LIFO 顺序
TC-03  pop_front/pop_back 空链表返回 ERR_EMPTY
TC-04  get(0) = peek_front，get(len-1) = peek_back
TC-05  get(len) 返回 ERR_OUT_OF_BOUNDS
TC-06  insert_after(tail_node, x)：等价于 push_back
TC-07  remove_after(dummy, ...) 等价于 pop_front
TC-08  reverse 后 pop_front 顺序与 reverse 前 pop_back 一致
TC-09  托管模式 destroy：free_fn 调用恰好 len 次
TC-10  clone 后修改克隆链表，原链表不受影响
TC-11  len 在连续 push/pop 后保持准确
TC-12  空链表 reverse 不崩溃，单节点 reverse 返回自身
```

---

## 七、基准测试

### 7.1 测试场景

| 场景 | 目的 |
|------|------|
| push_front N vs Vector insert(0) | 量化头部插入的 O(1) vs O(n) 差距 |
| push_back N（有 tail 指针）| 尾部追加吞吐量 |
| pop_front N | 队列模式出队性能 |
| pop_back N | 量化无前驱指针的 O(n) 代价 |
| 顺序 get 遍历 vs Vector 遍历 | 缓存不友好的实际代价 |
| find（命中率 50%）| 链表线性搜索基准 |

### 7.2 对比基线

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` 手写计时 | 朴素数组（`push_front` = `memmove`）|
| C++ | Google Benchmark | `std::forward_list<T>` / `std::list<T>` |
| Go | `testing.B` | 原生 `[]T`（头部插入）/ `container/list` |

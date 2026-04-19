# Stack 设计文档

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

Stack（栈）是一个遵循 **LIFO（后进先出）** 语义的线性容器。本文档同时涵盖两种实现：

- **数组版（ArrayStack）**：基于动态数组（复用 Vec），连续内存，缓存友好。
- **链表版（ListStack）**：基于单链表，每次 push/pop 动态分配/释放节点。

栈只暴露受限接口：

| 操作 | 语义 |
|------|------|
| `push(x)` | 将 x 压入栈顶 |
| `pop()` | 弹出栈顶元素并返回 |
| `peek()` | 返回栈顶元素引用（不移除）|
| `is_empty()` | 是否为空 |
| `len()` | 当前元素数量 |
| `clear()` | 清空所有元素 |

刻意**不提供**随机访问、迭代器或任意插入接口——违反 LIFO 语义的操作不应出现在栈的公开接口中。

---

## 二、关键语义决策

### 2.1 数组版 vs 链表版的选择依据

| 维度 | 数组版 | 链表版 |
|------|--------|--------|
| push/pop 均摊 | O(1)，偶发扩容 | O(1)，每次 malloc/free |
| 内存布局 | 连续，缓存友好 | 分散，缓存不友好 |
| 内存开销 | 低（无指针）| 高（每节点一指针）|
| 扩容抖动 | 存在（可 `reserve` 规避）| 无 |
| 最大容量限制 | 受堆内存限制（需扩容）| 受堆内存限制（逐节点）|
| 迭代器失效 | push 可能触发扩容使旧指针失效 | 不会失效 |

**推荐选择**：大多数场景选数组版；需要严格无抖动实时响应或节点指针需长期有效时选链表版。

### 2.2 数组版底层：复用 Vec 还是独立实现？

**决策：以 Vec 为底层存储，在其上封装 Stack 接口。**

优势：
- 不重复实现扩容逻辑、内存管理和内存安全保证。
- Stack 只是 Vec 的语义限制，不是独立的数据结构。
- 封装层零开销（内联后等价于直接调用 Vec 操作）。

实现方式：数组版 Stack 持有一个 `Vec` 字段，`push` 委托 `vec_push`，`pop` 委托 `vec_pop`，不暴露 Vec 的其他接口。

### 2.3 链表版底层：复用 SList 还是独立实现？

**决策：独立实现简化版单链表，不复用完整 SList。**

原因：
- Stack 的链表版只需要 `push_front`（压栈）和 `pop_front`（弹栈），无需 `tail` 指针、`get`、`find` 等功能。
- 独立实现的节点结构更轻量（无 `tail` 字段，无 `clone_fn`）。
- 避免将完整 SList 的复杂性引入 Stack 模块。

### 2.4 `pop` 空栈处理

**决策：返回 `(value, error)`，与 Vec 保持一致。**

`peek` 对空栈同样返回错误而非 panic/崩溃，调用者负责在 `push` 之前保证栈不为空（或处理 `ERR_EMPTY`）。

### 2.5 元素所有权语义

数组版：继承自 Vec，支持托管模式。
链表版：与 SList 一致，通过 `free_fn` 控制。

---

## 三、接口设计

### 3.1 统一接口（两种实现共享）

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new()` | 创建空栈 | O(1) |
| `new_with_cap(n)` | 预分配容量（仅数组版有意义）| O(1) |
| `destroy(s)` | 销毁栈 | O(n) |
| `push(s, val)` | 压入栈顶 | 均摊 O(1) |
| `pop(s)` | 弹出栈顶 | O(1) |
| `peek(s)` | 查看栈顶（不移除）| O(1) |
| `len(s)` | 当前元素数量 | O(1) |
| `is_empty(s)` | 是否为空 | O(1) |
| `clear(s)` | 清空 | O(n) |

### 3.2 数组版额外接口

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `reserve(s, n)` | 预分配 n 个额外槽位（规避扩容抖动）| O(n) |
| `shrink_to_fit(s)` | 释放多余容量 | O(n) |

---

## 四、异常处理

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_EMPTY` | `pop`/`peek` 时栈为空 | 返回错误码 |
| `ERR_ALLOC` | 内存分配失败 | 返回错误码，栈状态不变 |

数组版的 `push` 失败时底层 Vec 保证强异常安全（状态回滚），链表版 `push` 失败时节点分配失败则直接返回，链表结构不变。

---

## 五、语言实现

### 5.1 C（数组版）

```c
#include "Vec.h"

typedef struct {
    Vec base;    // 底层 Vec，栈顶 = base.data[base.len - 1]
} ArrayStack;

typedef enum {
    STACK_OK = 0, STACK_ERR_EMPTY, STACK_ERR_ALLOC
} StackError;

StackError astack_new(ArrayStack *s);
StackError astack_new_with_cap(ArrayStack *s, size_t cap);
void       astack_destroy(ArrayStack *s);
StackError astack_push(ArrayStack *s, const void *val);
StackError astack_pop(ArrayStack *s, void *out);
StackError astack_peek(const ArrayStack *s, void **out);
size_t     astack_len(const ArrayStack *s);
int        astack_is_empty(const ArrayStack *s);
void       astack_clear(ArrayStack *s);
StackError astack_reserve(ArrayStack *s, size_t additional);
```

### 5.2 C（链表版）

```c
typedef struct StackNode {
    void            *val;
    struct StackNode *next;
} StackNode;

typedef struct {
    StackNode  *top;      // 栈顶节点（无哨兵）
    size_t      len;
    free_fn_t   free_val;
} ListStack;

StackError lstack_new(ListStack *s);
StackError lstack_new_managed(ListStack *s, free_fn_t free_val);
void       lstack_destroy(ListStack *s);
StackError lstack_push(ListStack *s, void *val);
StackError lstack_pop(ListStack *s, void **out);
StackError lstack_peek(const ListStack *s, void **out);
size_t     lstack_len(const ListStack *s);
int        lstack_is_empty(const ListStack *s);
void       lstack_clear(ListStack *s);
```

> **链表版 C 注意**：无哨兵节点，空栈时 `top == NULL`，push 时新节点的 `next` 指向旧 `top`，弹出时更新 `top = top->next`。

### 5.3 C++（模板，两版）

```cpp
// 数组版
template<typename T>
class ArrayStack {
public:
    explicit ArrayStack(size_t init_cap = 4);
    ArrayStack(const ArrayStack&);
    ArrayStack(ArrayStack&&) noexcept;
    ArrayStack& operator=(ArrayStack) noexcept;
    ~ArrayStack() = default;

    StackError push(const T& val);
    StackError push(T&& val);
    std::expected<T, StackError> pop();
    std::expected<std::reference_wrapper<T>, StackError>       peek();
    std::expected<std::reference_wrapper<const T>, StackError> peek() const;

    size_t len()      const noexcept;
    bool   is_empty() const noexcept;
    void   clear();
    StackError reserve(size_t additional);
    StackError shrink_to_fit();

private:
    Vec<T> base_;
};

// 链表版
template<typename T>
class ListStack {
public:
    ListStack() = default;
    ListStack(const ListStack&);
    ListStack(ListStack&&) noexcept;
    ListStack& operator=(ListStack) noexcept;
    ~ListStack();

    StackError push(const T& val);
    StackError push(T&& val);
    std::expected<T, StackError> pop();
    std::expected<std::reference_wrapper<T>, StackError>       peek();
    std::expected<std::reference_wrapper<const T>, StackError> peek() const;

    size_t len()      const noexcept;
    bool   is_empty() const noexcept;
    void   clear();

private:
    struct Node { T val; Node *next = nullptr; };
    Node  *top_ = nullptr;
    size_t len_ = 0;
};
```

### 5.4 Go（泛型，两版）

```go
// 数组版（基于 Vec）
type ArrayStack[T any] struct {
    base Vec[T]
}

func NewArrayStack[T any](opts ...Option) *ArrayStack[T]
func (s *ArrayStack[T]) Push(val T) error
func (s *ArrayStack[T]) Pop() (T, error)
func (s *ArrayStack[T]) Peek() (T, error)
func (s *ArrayStack[T]) Len() int
func (s *ArrayStack[T]) IsEmpty() bool
func (s *ArrayStack[T]) Clear()
func (s *ArrayStack[T]) Reserve(additional int) error

// 链表版
type stackNode[T any] struct {
    val  T
    next *stackNode[T]
}

type ListStack[T any] struct {
    top *stackNode[T]
    len int
}

func NewListStack[T any]() *ListStack[T]
func (s *ListStack[T]) Push(val T)
func (s *ListStack[T]) Pop() (T, error)
func (s *ListStack[T]) Peek() (T, error)
func (s *ListStack[T]) Len() int
func (s *ListStack[T]) IsEmpty() bool
func (s *ListStack[T]) Clear()
```

> **Go 链表版注意**：`Push` 不返回 error（Go 内存不足会 panic，不返回 error），与 Go 惯例一致。`stackNode` 为私有类型，不暴露节点结构。

---

## 六、功能测试

### 6.1 关键测试用例

```
TC-01  push N 个元素，pop 顺序为 push 顺序的逆序（LIFO 验证）
TC-02  pop 空栈返回 ERR_EMPTY
TC-03  peek 不改变 len
TC-04  数组版：push 直到触发扩容，所有元素仍正确
TC-05  数组版：reserve(N) 后连续 push N 个元素不触发扩容
TC-06  链表版：push/pop 交替操作，len 始终准确
TC-07  两种实现的行为等价性：相同操作序列产生相同结果
TC-08  托管模式 clear：free_fn 调用恰好 len 次
TC-09  destroy 后不能再使用（C：置 NULL；C++：移动后析构）
TC-10  copy/clone：修改副本不影响原栈
```

### 6.2 数组版 vs 链表版等价性测试

对随机操作序列（push/pop 各 50%），两种实现的输出序列完全相同，且最终状态（len、top 值）一致。

---

## 七、基准测试

| 场景 | 目的 |
|------|------|
| push N（数组版，无预分配）| 均摊 O(1) + 扩容开销 |
| push N（数组版，已 reserve）| 无扩容时的纯 push 性能上限 |
| push N（链表版）| 每次 malloc 的实际代价 |
| pop N（两版）| 数组版 O(1) vs 链表版 O(1) + free |
| 混合 push/pop（各 50%）| 模拟真实使用，对比内存分配器开销 |

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` | 朴素数组（固定大小）|
| C++ | Google Benchmark | `std::stack<T, std::vector<T>>` / `std::stack<T, std::list<T>>` |
| Go | `testing.B` | 原生 `[]T` 切片模拟栈 |

# Queue 设计文档

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

Queue（队列）是遵循 **FIFO（先进先出）** 语义的线性容器：从尾部入队（enqueue），从头部出队（dequeue）。本文档涵盖**链表版**实现（环形缓冲区队列见 `6-ring-buffer.md`）。

核心保证：
- `enqueue`（入队）：O(1)，追加到尾部。
- `dequeue`（出队）：O(1)，从头部移除。
- `peek_front`：O(1)，查看队首不移除。
- 不提供随机访问和中间插入——违反 FIFO 语义。

---

## 二、关键语义决策

### 2.1 底层结构：链表 vs 环形缓冲区

| 维度 | 链表队列（本文档）| 环形缓冲区队列（下一篇）|
|------|-----------------|----------------------|
| 内存布局 | 分散节点 | 连续数组 |
| enqueue/dequeue | O(1)，每次 malloc/free | O(1)，无分配 |
| 无界扩容 | 自然支持 | 需要整体 realloc |
| 缓存友好性 | 低 | 高 |
| 适用场景 | 无界，对内存局部性不敏感 | 有界或容量可预知，高性能 |

### 2.2 底层链表方案

**决策：复用双链表（DList）的 `push_back` + `pop_front` 实现链表队列。**

理由：
- 双链表天然支持 O(1) 双端操作，与队列语义完美匹配。
- 单链表也可实现（`push_back` 需维护 `tail` 指针），但复用双链表代码路径更清晰。
- 若不依赖已有 DList 实现，可使用极简版单链表（仅含 `head`、`tail`、`len`）。

> 若性能是首要目标，请使用环形缓冲区队列；若不限容量且代码简洁性优先，使用链表队列。

### 2.3 `dequeue` 空队列处理

**决策：返回 `(value, error)`。**

与栈、Vector 设计一致，不 panic，调用者决定如何处理空队列。

### 2.4 元素所有权语义

与 DList 一致，支持不托管和托管两种模式，通过初始化时注入 `free_fn` 控制。

### 2.5 并发安全

本文档实现**非并发安全**。Go 版本若需在 goroutine 间共享队列，应使用 channel 或在外部加 `sync.Mutex`。并发安全版本（基于 channel 或无锁队列）留待高级结构阶段实现。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new()` | 创建空队列 | O(1) |
| `new_managed(free_fn)` | 注入元素释放钩子 | O(1) |
| `destroy(q)` | 销毁；托管模式逐节点调用 `free_fn` | O(n) |
| `move(dst, src)` | 转移所有权 | O(1) |
| `clone(dst, src)` | 深拷贝，需 `clone_fn` | O(n) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `enqueue(q, val)` | 入队（追加到尾部）| O(1) |
| `dequeue(q)` | 出队（移除并返回头部）| O(1) |
| `peek_front(q)` | 查看队首元素引用 | O(1) |
| `peek_back(q)` | 查看队尾元素引用 | O(1) |
| `len(q)` | 队列长度 | O(1) |
| `is_empty(q)` | 是否为空 | O(1) |
| `clear(q)` | 清空队列 | O(n) |

### 3.3 扩展操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `enqueue_batch(q, vals, n)` | 批量入队 | O(n) |
| `foreach(q, fn)` | 从队首到队尾遍历（不出队）| O(n) |

---

## 四、异常处理

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_EMPTY` | `dequeue`/`peek_front`/`peek_back` 时队列为空 | 返回错误码 |
| `ERR_ALLOC` | 节点分配失败 | 返回错误码，队列状态不变 |

---

## 五、语言实现

### 5.1 C

```c
#include "doubly_linked_list.h"

typedef struct {
    DList base;
} Queue;

typedef enum {
    Q_OK = 0, Q_ERR_EMPTY, Q_ERR_ALLOC
} QueueError;

QueueError queue_new(Queue *q);
QueueError queue_new_managed(Queue *q, free_fn_t free_val);
void       queue_destroy(Queue *q);
void       queue_move(Queue *dst, Queue *src);

QueueError queue_enqueue(Queue *q, void *val);
QueueError queue_dequeue(Queue *q, void **out);
QueueError queue_peek_front(const Queue *q, void **out);
QueueError queue_peek_back(const Queue *q, void **out);

size_t     queue_len(const Queue *q);
int        queue_is_empty(const Queue *q);
void       queue_clear(Queue *q);
void       queue_foreach(const Queue *q, void (*fn)(const void *val));
```

> **C 实现注意**：Queue 完全是 DList 的语义封装，`enqueue` 委托 `dlist_push_back`，`dequeue` 委托 `dlist_pop_front`。不暴露 DList 的随机访问或中间插入接口。

### 5.2 C++

```cpp
template<typename T>
class Queue {
public:
    Queue() = default;
    Queue(const Queue&);
    Queue(Queue&&) noexcept;
    Queue& operator=(Queue) noexcept;
    ~Queue() = default;

    QueueError enqueue(const T& val);
    QueueError enqueue(T&& val);
    std::expected<T, QueueError> dequeue();
    std::expected<std::reference_wrapper<T>, QueueError>       peek_front();
    std::expected<std::reference_wrapper<const T>, QueueError> peek_front() const;
    std::expected<std::reference_wrapper<T>, QueueError>       peek_back();

    size_t len()      const noexcept;
    bool   is_empty() const noexcept;
    void   clear();
    void   foreach(std::function<void(const T&)> fn) const;

private:
    DList<T> base_;
};
```

### 5.3 Go

```go
type Queue[T any] struct {
    base DList[T]
}

func NewQueue[T any]() *Queue[T]

func (q *Queue[T]) Enqueue(val T)
func (q *Queue[T]) Dequeue() (T, error)
func (q *Queue[T]) PeekFront() (T, error)
func (q *Queue[T]) PeekBack() (T, error)

func (q *Queue[T]) Len() int
func (q *Queue[T]) IsEmpty() bool
func (q *Queue[T]) Clear()
func (q *Queue[T]) ForEach(fn func(T))
func (q *Queue[T]) All() iter.Seq[T]   // Go 1.23+
```

> **Go 实现注意**：`Enqueue` 不返回 error（同链表版 Stack 的 `Push`）。Go channel 可以完全替代 Queue 的并发场景，本实现专注于单线程、算法学习用途。

---

## 六、功能测试

### 6.1 关键测试用例

```
TC-01  enqueue N 个元素，dequeue 顺序与入队顺序完全相同（FIFO 验证）
TC-02  dequeue 空队列返回 ERR_EMPTY
TC-03  peek_front 不改变 len 和队列内容
TC-04  enqueue 1 个元素，peek_front == peek_back（单元素队列）
TC-05  交替 enqueue/dequeue，len 始终准确
TC-06  enqueue_batch 后批量 dequeue，顺序正确
TC-07  clear 后 len == 0，再次 enqueue 正常工作
TC-08  托管模式 clear：free_fn 调用恰好 len 次
TC-09  move 后 src 为空，dst 包含原有所有元素
TC-10  clone 后修改副本不影响原队列
```

---

## 七、基准测试

| 场景 | 目的 |
|------|------|
| enqueue N | 入队吞吐量（每次 malloc 代价）|
| dequeue N | 出队吞吐量（每次 free 代价）|
| enqueue N + dequeue N（交替）| 生产者-消费者模式基准 |
| 链表队列 vs 环形缓冲区队列 | 内存分配代价 vs 连续内存代价对比 |

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` | 固定大小数组队列（手动维护头尾指针）|
| C++ | Google Benchmark | `std::queue<T, std::deque<T>>` |
| Go | `testing.B` | `chan T`（有缓冲 channel）/ `container/list` |

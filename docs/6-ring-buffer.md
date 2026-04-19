# Ring Buffer（循环队列）设计文档

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

Ring Buffer（环形缓冲区）是基于**固定大小连续数组**实现的 FIFO 队列，通过头尾指针在数组上循环移动来复用空间，无需移动元素或重新分配内存。

核心保证：
- **无分配的 enqueue/dequeue**：在容量范围内，入队和出队均不涉及堆内存操作，O(1) 且无抖动。
- **连续内存**：数据存储在单块连续数组中，缓存友好。
- **可扩容变体**：满时可选择报错（有界模式）或自动扩容（无界模式）。

与链表队列对比：

| 维度 | 链表队列 | Ring Buffer |
|------|---------|------------|
| enqueue/dequeue 均摊 | O(1) + malloc/free | O(1)，无分配 |
| 满时行为 | 自动扩容 | 可配置：报错或扩容 |
| 内存布局 | 分散 | 连续 |
| 缓存友好性 | 低 | 高 |
| 内存开销 | 高（每节点一指针）| 低（仅两个整数索引）|
| 适用场景 | 无界，内存不敏感 | 有界或高吞吐量场景 |

适用场景：I/O 缓冲区、日志系统、生产者-消费者管道、固定窗口滑动计算。

---

## 二、关键语义决策

### 2.1 区分满与空：额外计数器 vs 留一个空槽

环形缓冲区的经典难题：`head == tail` 既可表示"空"又可表示"满"。两种主流解法：

**方案 A：留一个空槽（waste one slot）**
- 容量为 N 的数组只存 N-1 个元素。
- 空：`head == tail`；满：`(tail + 1) % N == head`。
- 优点：无额外字段，逻辑简洁。
- 缺点：浪费一个槽位，且对调用者暴露了"容量 = 分配大小 - 1"的怪异语义。

**方案 B：维护 `len` 计数器（本实现采用）**
- 空：`len == 0`；满：`len == cap`。
- 优点：语义清晰，`len()`/`cap()` 直接返回准确值，不浪费槽位。
- 缺点：每次 enqueue/dequeue 需维护计数器（成本极低）。

**决策：使用方案 B（维护 `len` 计数器）。**

### 2.2 满时策略：有界 vs 可扩容

**决策：支持两种模式，通过初始化配置选择。**

| 模式 | 满时行为 | 适用场景 |
|------|---------|---------|
| 有界（`BOUNDED`）| 返回 `ERR_FULL` | 固定大小缓冲区，实时系统 |
| 可扩容（`GROWABLE`）| 自动 ×2 扩容（需 realloc + 数据重排）| 通用队列，容量难以预知时 |

扩容时的数据重排：由于数据在数组中可能跨越边界（`tail < head`），realloc 后需将"绕回"部分搬移到新数组的正确位置。实现方式：

```
扩容前（cap=8, head=5, tail=3）：
  [D E F G _ _ A B C] → 逻辑顺序：A B C D E F G
                  ^tail       ^head

扩容后（cap=16）：
  [A B C D E F G _ _ _ _ _ _ _ _ _]
   ^head                ^tail
```

重排策略：两次 `memcpy`，将 `[head..cap-1]` 复制到新缓冲区的 `[0..old_cap-head-1]`，再将 `[0..tail-1]` 复制到紧随其后的位置，最后更新 `head = 0`，`tail = old_len`。

### 2.3 索引计算：取模 vs 位掩码

当容量始终为 2 的幂时，`(i + 1) % cap` 可替换为 `(i + 1) & (cap - 1)`，避免除法指令。

**决策：可扩容模式下强制容量为 2 的幂；有界模式下容量可为任意值（使用取模）。**

### 2.4 元素所有权语义

与 Vec 一致：默认不托管，可通过 `free_fn` 注入托管语义。扩容时对已有元素执行 `memcpy`（浅移动），不调用 `clone_fn`。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new(elem_size, cap)` | 创建有界环形缓冲区 | O(1) |
| `new_growable(elem_size, init_cap)` | 创建可扩容环形缓冲区 | O(1) |
| `new_config(cfg)` | 注入扩容策略和元素钩子 | O(1) |
| `destroy(rb)` | 销毁；托管模式逐元素调用 `free_fn` | O(n) |
| `move(dst, src)` | 转移所有权 | O(1) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `enqueue(rb, val)` | 入队（追加到尾部）| O(1)，满时有界返回 ERR_FULL |
| `dequeue(rb)` | 出队（移除并返回头部）| O(1) |
| `peek_front(rb)` | 查看队首元素引用 | O(1) |
| `peek_back(rb)` | 查看队尾元素引用 | O(1) |
| `get(rb, i)` | 按逻辑索引访问（0 = 队首）| O(1) |

### 3.3 容量管理

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(rb)` | 当前元素数量 | O(1) |
| `cap(rb)` | 缓冲区总容量 | O(1) |
| `is_empty(rb)` | 是否为空 | O(1) |
| `is_full(rb)` | 是否已满（仅有界模式有意义）| O(1) |
| `clear(rb)` | 清空，head/tail 重置为 0 | O(n)（托管模式）|
| `reserve(rb, n)` | 可扩容模式：确保至少 n 个空余槽位 | O(n) |

### 3.4 批量操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `enqueue_batch(rb, src, n)` | 批量入队 n 个元素 | O(n) |
| `dequeue_batch(rb, dst, n)` | 批量出队 n 个元素 | O(n) |
| `as_slices(rb)` | 返回两段连续切片（因绕回可能有两段）| O(1) |

> `as_slices` 返回 `(first, second)` 两个切片，逻辑顺序为 `first ++ second`。当数据未跨越数组边界时 `second` 为空。此 API 对 I/O 缓冲场景（如 `writev`）非常有用。

---

## 四、异常处理

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_EMPTY` | `dequeue`/`peek` 时缓冲区为空 | 返回错误码 |
| `ERR_FULL` | 有界模式下 `enqueue` 时已满 | 返回错误码 |
| `ERR_ALLOC` | 可扩容模式扩容失败 | 返回错误码，状态回滚 |
| `ERR_OUT_OF_BOUNDS` | `get(i)` 且 `i >= len` | 返回错误码 |
| `ERR_INVALID_CONFIG` | 初始容量为 0 | 初始化时检测，返回 NULL/error |

扩容时的强异常安全：先分配新缓冲区，成功后再执行数据重排和指针切换，失败则释放新缓冲区并返回 `ERR_ALLOC`。

---

## 五、语言实现

### 5.1 C

```c
typedef void  (*free_fn_t)(void *elem);

typedef enum {
    RB_BOUNDED = 0,
    RB_GROWABLE
} RBMode;

typedef struct {
    void      *data;
    size_t     head;        // 队首元素的物理索引
    size_t     tail;        // 下一个入队位置的物理索引
    size_t     len;         // 当前元素数量
    size_t     cap;         // 缓冲区总容量（GROWABLE 模式下始终为 2 的幂）
    size_t     elem_size;
    RBMode     mode;
    free_fn_t  free_elem;   // NULL = 不托管
} RingBuf;

typedef enum {
    RB_OK = 0, RB_ERR_EMPTY, RB_ERR_FULL, RB_ERR_ALLOC,
    RB_ERR_OOB, RB_ERR_INVALID_CONFIG
} RBError;

/* 内部索引计算（inline）*/
/* BOUNDED: (i + 1) % cap   */
/* GROWABLE: (i + 1) & (cap - 1) */

/* 生命周期 */
RBError ringbuf_new(RingBuf *rb, size_t elem_size, size_t cap);
RBError ringbuf_new_growable(RingBuf *rb, size_t elem_size, size_t init_cap);
void    ringbuf_destroy(RingBuf *rb);
void    ringbuf_move(RingBuf *dst, RingBuf *src);

/* 核心操作 */
RBError ringbuf_enqueue(RingBuf *rb, const void *elem);
RBError ringbuf_dequeue(RingBuf *rb, void *out);
RBError ringbuf_peek_front(const RingBuf *rb, void **out);
RBError ringbuf_peek_back(const RingBuf *rb, void **out);
RBError ringbuf_get(const RingBuf *rb, size_t i, void *out);

/* 容量 */
size_t  ringbuf_len(const RingBuf *rb);
size_t  ringbuf_cap(const RingBuf *rb);
int     ringbuf_is_empty(const RingBuf *rb);
int     ringbuf_is_full(const RingBuf *rb);
void    ringbuf_clear(RingBuf *rb);
RBError ringbuf_reserve(RingBuf *rb, size_t additional);

/* 批量 */
RBError ringbuf_enqueue_batch(RingBuf *rb, const void *src, size_t n);
RBError ringbuf_dequeue_batch(RingBuf *rb, void *dst, size_t n);
void    ringbuf_as_slices(const RingBuf *rb,
                          void **first,  size_t *first_len,
                          void **second, size_t *second_len);
```

> **C 实现注意**：`head` 和 `tail` 均为物理索引，元素的物理地址为 `data + ((head + i) % cap) * elem_size`。扩容时分配新缓冲区，调用两次 `memcpy` 重排数据后，`head = 0`，`tail = old_len`。

### 5.2 C++

```cpp
template<typename T>
class RingBuf {
public:
    enum class Mode { Bounded, Growable };

    explicit RingBuf(size_t cap, Mode mode = Mode::Bounded);
    RingBuf(const RingBuf&);
    RingBuf(RingBuf&&) noexcept;
    RingBuf& operator=(RingBuf) noexcept;
    ~RingBuf();

    RBError enqueue(const T& elem);
    RBError enqueue(T&& elem);
    std::expected<T, RBError> dequeue();
    std::expected<std::reference_wrapper<T>, RBError>       peek_front();
    std::expected<std::reference_wrapper<const T>, RBError> peek_front() const;
    std::expected<std::reference_wrapper<T>, RBError>       peek_back();
    std::expected<std::reference_wrapper<T>, RBError>       get(size_t i);

    size_t len()      const noexcept;
    size_t cap()      const noexcept;
    bool   is_empty() const noexcept;
    bool   is_full()  const noexcept;
    void   clear();
    RBError reserve(size_t additional);

    // 返回两段连续 span（C++20 std::span）
    std::pair<std::span<T>, std::span<T>> as_slices() noexcept;

private:
    T      *data_  = nullptr;
    size_t  head_  = 0;
    size_t  tail_  = 0;
    size_t  len_   = 0;
    size_t  cap_   = 0;
    Mode    mode_;

    size_t  phys(size_t logical) const noexcept;
    RBError grow();
};
```

> **C++ 实现注意**：`enqueue(T&&)` 使用 placement new 在 `data_[tail_]` 就地构造，避免多余拷贝。`dequeue()` 显式调用 `data_[head_].~T()` 析构后移动返回。`Growable` 模式容量始终保持 2 的幂。

### 5.3 Go

```go
type RingBuf[T any] struct {
    data    []T
    head    int
    tail    int
    len     int
    growable bool
}

type RBError int
const (
    RBErrEmpty          RBError = iota + 1
    RBErrFull
    RBErrOutOfBounds
    RBErrInvalidConfig
)

func NewRingBuf[T any](cap int) *RingBuf[T]              // 有界模式
func NewGrowableRingBuf[T any](initCap int) *RingBuf[T]  // 可扩容模式

func (rb *RingBuf[T]) Enqueue(elem T) error
func (rb *RingBuf[T]) Dequeue() (T, error)
func (rb *RingBuf[T]) PeekFront() (T, error)
func (rb *RingBuf[T]) PeekBack() (T, error)
func (rb *RingBuf[T]) Get(i int) (T, error)

func (rb *RingBuf[T]) Len() int
func (rb *RingBuf[T]) Cap() int
func (rb *RingBuf[T]) IsEmpty() bool
func (rb *RingBuf[T]) IsFull() bool
func (rb *RingBuf[T]) Clear()
func (rb *RingBuf[T]) Reserve(additional int) error

// 返回两段 slice，逻辑上连续
func (rb *RingBuf[T]) AsSlices() (first, second []T)

// 内部
func (rb *RingBuf[T]) phys(logical int) int
func (rb *RingBuf[T]) grow() error
```

> **Go 实现注意**：Go 中 `[]T` 本身即连续数组，`data` 长度始终等于容量。`phys(i) = (head + i) % len(data)`。扩容时创建新切片，两次 `copy` 重排数据，更新 `head = 0`，`tail = old_len`。有界模式下 `Enqueue` 满时返回 `RBErrFull`；可扩容模式下触发 `grow`。

---

## 六、功能测试

### 6.1 测试分类

| 类别 | 覆盖内容 |
|------|---------|
| 正常路径 | enqueue/dequeue 基础 FIFO 语义 |
| 边界条件 | 空缓冲区、满缓冲区、容量 1 |
| 绕回行为 | 数据跨越数组边界时的正确性 |
| 有界满时 | 返回 ERR_FULL，缓冲区状态不变 |
| 可扩容 | 满时正确扩容，数据重排后顺序不变 |
| 随机访问 | get(i) 物理索引计算正确性 |
| as_slices | 未绕回时 second 为空，绕回时两段拼接等于逻辑顺序 |
| 批量操作 | enqueue_batch/dequeue_batch 与逐个操作结果一致 |

### 6.2 关键测试用例

```
TC-01  enqueue 填满（cap=8），dequeue 全部，验证 FIFO 顺序
TC-02  有界模式满时 enqueue 返回 ERR_FULL，len 不变
TC-03  enqueue 4 个，dequeue 3 个，再 enqueue 5 个（触发绕回），验证顺序正确
TC-04  可扩容模式：enqueue 超过 init_cap，容量翻倍，数据正确
TC-05  get(0) == peek_front，get(len-1) == peek_back
TC-06  get(len) 返回 ERR_OUT_OF_BOUNDS
TC-07  as_slices 绕回情况：first ++ second == 完整逻辑序列
TC-08  enqueue_batch(n) 等价于逐个 enqueue n 次
TC-09  clear 后 head=0, tail=0, len=0，再次 enqueue 正常
TC-10  托管模式 clear：free_fn 调用恰好 len 次
TC-11  可扩容扩容失败：状态回滚，原数据完整
TC-12  容量 1 的缓冲区：enqueue 1 个，满；dequeue 1 个，空
```

---

## 七、基准测试

### 7.1 测试场景

| 场景 | 目的 |
|------|------|
| enqueue + dequeue N（有界，不触发满）| 无分配 FIFO 的峰值吞吐量 |
| 可扩容：push 直到触发 K 次扩容 | 量化扩容重排代价 |
| Ring Buffer vs 链表队列（相同 N）| 内存分配代价 vs 连续内存优势 |
| get(i) 随机访问 | 与 Vector 随机访问对比 |
| as_slices + 单次 memcpy | 批量 I/O 场景的零拷贝模拟 |
| 批量 enqueue_batch vs 逐个 enqueue | SIMD/prefetch 友好性 |

### 7.2 基准工具与参考基线

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` | 链表队列（相同操作数）|
| C++ | Google Benchmark | `std::queue<T, std::deque<T>>` / `boost::circular_buffer` |
| Go | `testing.B` | `chan T`（有缓冲 channel）|

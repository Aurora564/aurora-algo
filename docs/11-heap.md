# Heap（二叉堆）设计文档

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

二叉堆（Binary Heap）是一棵**完全二叉树**，满足**堆序性质**：

- **最大堆**：每个节点的值 ≥ 其所有后代节点的值；根节点持有全局最大值。
- **最小堆**：每个节点的值 ≤ 其所有后代节点的值；根节点持有全局最小值。

完全二叉树可以高效地用**数组**表示（无需指针），下标关系：

```
节点 i 的父节点：(i - 1) / 2
节点 i 的左子节点：2*i + 1
节点 i 的右子节点：2*i + 2
```

| 操作 | 复杂度 |
|------|--------|
| `push`（插入） | O(log n) |
| `top`（查看堆顶） | O(1) |
| `pop`（删除堆顶） | O(log n) |
| `heapify`（从数组原地建堆） | **O(n)**（比逐个 push 更优） |
| `heap_sort` | O(n log n)，原地，空间 O(1) |

本文档实现**最大堆**为主，通过注入比较函数可支持最小堆或自定义优先级。

---

## 二、关键语义决策

### 2.1 数组存储 vs 树节点

**决策：使用动态数组存储，不使用指针节点。**

完全二叉树的数组下标映射天然满足堆的结构需求，优点显著：

| 维度 | 数组存储（本实现） | 指针节点 |
|------|----------------|---------|
| 缓存局部性 | 优（连续内存） | 差（指针跳转） |
| 额外内存 | 无（仅值本身） | 3 个指针/节点 |
| 实现复杂度 | 简单（下标计算） | 复杂（指针维护） |
| 随机访问 | O(1) | O(log n) |

底层动态数组复用本项目的 `Vec` 实现（C/C++/Go 均已实现）。

### 2.2 最大堆 vs 最小堆：注入比较函数

**决策：通过注入比较函数统一最大/最小堆，而非两套实现。**

| 语言 | 策略 |
|------|------|
| C | `cmp_fn(a, b) → int`，`> 0` 表示 a 优先于 b |
| C++ | 模板参数 `Compare`，默认 `std::less<T>`（最小堆）；传入 `std::greater<T>` 得最大堆 |
| Go | 泛型约束 `cmp.Ordered`；提供 `MaxHeap[K]` 和 `MinHeap[K]` 类型别名，或传入 `less` 函数 |

> **注意 C++ 约定**：标准库 `std::priority_queue` 默认是最大堆（`std::less`）——`less` 意味着"父 > 子"（less 方向的元素被下沉）。本实现与标准库保持一致。

### 2.3 `heapify`（建堆）算法

**决策：使用 Floyd 建堆算法（O(n)），而非逐个 push（O(n log n)）。**

```
Floyd 建堆：从最后一个非叶节点 (n/2 - 1) 开始，逆序对每个节点执行 sift_down
```

数学证明：O(n) 总操作量（各层节点数 × 该层最大下沉高度之和，收敛到 O(n)）。

### 2.4 `heap_sort`：原地排序

**决策：`heap_sort` 在原数组上操作，空间 O(1)，不分配额外内存。**

```
heap_sort(array):
    1. heapify(array)         → 建最大堆
    2. for i from n-1 to 1:
           swap(array[0], array[i])   → 将最大值移到末尾
           sift_down(array, 0, i)     → 对缩小后的堆修复
```

结果为升序排列。若需降序，使用最小堆执行同样操作。

### 2.5 `decrease_key` / `increase_key`（可选）

这两个操作是 Dijkstra 等算法的关键：修改元素优先级后上浮或下沉。

**决策：本实现提供 `update(i, new_val)` 操作，调用 `sift_up` 或 `sift_down` 恢复堆序。**

调用方负责提供正确的下标 `i`；若需通过键查找下标，需额外维护索引（Indexed Heap，超出本文档范围）。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new(cmp_fn)` | 创建空堆 | O(1) |
| `from(array, cmp_fn)` | 从数组建堆（Floyd 算法） | O(n) |
| `destroy(h)` | 释放内部数组 | O(1) |
| `clone(h)` | 深拷贝 | O(n) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `push(val)` | 插入元素 | O(log n) |
| `top()` | 查看堆顶（不弹出）| O(1) |
| `pop()` | 弹出堆顶，返回其值 | O(log n) |
| `update(i, new_val)` | 修改下标 i 处的值并恢复堆序 | O(log n) |

### 3.3 批量与排序

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `heapify(array)` | 原地建堆 | O(n) |
| `heap_sort(array)` | 原地堆排序（升序） | O(n log n) |

### 3.4 统计

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(h)` | 元素数量 | O(1) |
| `is_empty(h)` | 是否为空 | O(1) |
| `capacity(h)` | 当前内部数组容量 | O(1) |
| `is_valid_heap(h)` | 验证堆序性质（测试辅助）| O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_EMPTY` | 对空堆调用 `top` / `pop` | 返回错误码 |
| `ERR_OUT_OF_BOUND` | `update(i, v)` 时 i ≥ len | 返回错误码 |
| `ERR_ALLOC` | `push` 时扩容失败 | 返回错误码，堆状态不变 |

### 4.2 不变式保证

每次 `push` / `pop` / `update` 操作后：

```
is_valid_heap(h) == true
len(h) == prev_len ± 1  （push+1，pop-1，update 不变）
```

---

## 五、语言实现

### 5.1 C

```c
typedef int (*heap_cmp_fn)(const void *a, const void *b);
/* 返回值 > 0 表示 a 的优先级高于 b（a 应更靠近堆顶） */

typedef enum {
    HEAP_OK        = 0,
    HEAP_EMPTY     = -1,
    HEAP_ALLOC_ERR = -2,
    HEAP_OUT_OF_BOUND = -3,
} HeapError;

typedef struct {
    void       **data;     /* 元素指针数组（底层 Vec 展开） */
    size_t       len;
    size_t       cap;
    heap_cmp_fn  cmp;
} Heap;

/* 生命周期 */
Heap      *heap_new(heap_cmp_fn cmp);
Heap      *heap_from(void **array, size_t n, heap_cmp_fn cmp);  /* Floyd 建堆 */
void       heap_destroy(Heap *h);

/* 核心操作 */
HeapError  heap_push(Heap *h, void *val);
HeapError  heap_top(const Heap *h, void **out);
HeapError  heap_pop(Heap *h, void **out);
HeapError  heap_update(Heap *h, size_t i, void *new_val);

/* 排序（原地） */
void       heap_sort(void **array, size_t n, heap_cmp_fn cmp);

/* 统计 */
size_t     heap_len(const Heap *h);
int        heap_is_empty(const Heap *h);
int        heap_is_valid(const Heap *h);

/* 内部：上浮 / 下沉 */
static void sift_up(Heap *h, size_t i);
static void sift_down(Heap *h, size_t i, size_t heap_size);
```

### 5.2 C++

```cpp
namespace algo {

// 默认 Compare = std::less<T> → 最大堆（父 >= 子，less 表示被下沉方向）
// 传入 std::greater<T>         → 最小堆
template <typename T, typename Compare = std::less<T>>
class Heap {
public:
    enum class Error { Empty, AllocFail, OutOfBound };

    explicit Heap(Compare cmp = {}) : cmp_(std::move(cmp)) {}

    // 从范围建堆（Floyd O(n)）
    template <std::ranges::input_range R>
    explicit Heap(R&& range, Compare cmp = {});

    /* 核心操作 */
    [[nodiscard]] std::expected<void, Error> push(T val);

    [[nodiscard]] std::expected<std::reference_wrapper<const T>, Error>
        top() const noexcept;

    [[nodiscard]] std::expected<T, Error> pop();

    [[nodiscard]] std::expected<void, Error>
        update(std::size_t i, T new_val);

    /* 统计 */
    [[nodiscard]] std::size_t size()     const noexcept { return data_.size(); }
    [[nodiscard]] bool        empty()    const noexcept { return data_.empty(); }
    [[nodiscard]] bool        is_valid() const noexcept;

    /* 静态：原地堆排序 */
    static void heap_sort(std::span<T> arr, Compare cmp = {});

private:
    std::vector<T> data_;
    [[no_unique_address]] Compare cmp_;

    void sift_up(std::size_t i);
    void sift_down(std::size_t i, std::size_t heap_size);
};

// 便捷别名
template <typename T> using MaxHeap = Heap<T, std::less<T>>;
template <typename T> using MinHeap = Heap<T, std::greater<T>>;

} // namespace algo
```

### 5.3 Go

```go
package linear

import "cmp"

// Heap 二叉堆；less(a, b) 返回 true 时 a 优先级更高
// less = func(a, b T) bool { return a > b } → 最大堆
// less = func(a, b T) bool { return a < b } → 最小堆
type Heap[T any] struct {
    data []T
    less func(a, b T) bool
}

// 构造
func NewHeap[T any](less func(T, T) bool) *Heap[T]

// 从切片建堆（Floyd O(n)）
func HeapFrom[T any](data []T, less func(T, T) bool) *Heap[T]

// 核心操作
func (h *Heap[T]) Push(val T)
func (h *Heap[T]) Top() (T, bool)     // (value, ok)
func (h *Heap[T]) Pop() (T, bool)     // (value, ok)
func (h *Heap[T]) Update(i int, newVal T) bool

// 原地堆排序
func HeapSort[T cmp.Ordered](arr []T)
func HeapSortFunc[T any](arr []T, less func(T, T) bool)

// 统计
func (h *Heap[T]) Len()     int
func (h *Heap[T]) Empty()   bool
func (h *Heap[T]) IsValid() bool

// 便捷构造：有序类型直接使用
func NewMaxHeap[T cmp.Ordered]() *Heap[T]
func NewMinHeap[T cmp.Ordered]() *Heap[T]

// 内部
func (h *Heap[T]) siftUp(i int)
func (h *Heap[T]) siftDown(i, heapSize int)
```

---

## 六、功能测试

### TC-01  空堆基本属性

```
h := NewMaxHeap[int]()
ASSERT h.Len() == 0
ASSERT h.Empty() == true
ASSERT h.IsValid() == true
top, ok := h.Top()
ASSERT !ok
```

### TC-02  单元素 push / pop

```
h.Push(42)
ASSERT h.Len() == 1
top, _ := h.Top()
ASSERT top == 42
val, _ := h.Pop()
ASSERT val == 42
ASSERT h.Empty() == true
```

### TC-03  最大堆性质验证

```
for _, v := range [3, 1, 4, 1, 5, 9, 2, 6]:
    h.Push(v)
ASSERT h.IsValid() == true
// 依次弹出应为降序
expected := [9, 6, 5, 4, 3, 2, 1, 1]
for _, e := range expected:
    v, _ := h.Pop()
    ASSERT v == e
```

### TC-04  最小堆性质验证

```
h := NewMinHeap[int]()
for _, v := range [3, 1, 4, 1, 5, 9, 2, 6]:
    h.Push(v)
expected := [1, 1, 2, 3, 4, 5, 6, 9]  // 升序
for _, e := range expected:
    v, _ := h.Pop()
    ASSERT v == e
```

### TC-05  heapify（Floyd O(n) 建堆）

```
arr := [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5]
h := HeapFrom(arr, maxLess)
ASSERT h.Len() == 11
ASSERT h.IsValid() == true
top, _ := h.Top()
ASSERT top == 9
```

### TC-06  heap_sort（升序）

```
arr := [5, 3, 8, 1, 9, 2, 7, 4, 6]
HeapSort(arr)
ASSERT arr == [1, 2, 3, 4, 5, 6, 7, 8, 9]
```

### TC-07  heap_sort 边界：空数组 / 单元素 / 全相等

```
HeapSort([]) — 无 panic
HeapSort([42]) → [42]
HeapSort([3, 3, 3]) → [3, 3, 3]
```

### TC-08  heap_sort 正确性（随机）

```
生成 1000 个随机整数，HeapSort 后与 sort.Ints 结果对比
ASSERT 两者相同
```

### TC-09  update 操作

```
h := HeapFrom([1, 3, 5, 7, 9], maxLess)
// 将下标 4 处的值（9 所在位置取决于建堆结果）
h.Update(0, 0)   // 降低堆顶优先级
ASSERT h.IsValid() == true
```

### TC-10  大批量 push / pop

```
for i in [0..9999]:
    h.Push(rand.Int())
ASSERT h.Len() == 10000
ASSERT h.IsValid() == true
prev := MaxInt
for !h.Empty():
    v, _ := h.Pop()
    ASSERT v <= prev
    prev = v
```

### TC-11  扩容行为

```
h := NewMaxHeap[int]()
// 初始 cap 为 0，push 触发多次扩容
for i in [0..1023]:
    h.Push(i)
ASSERT h.Len() == 1024
ASSERT h.IsValid() == true
```

### TC-12  与 std::priority_queue 行为对比

```
C++ 测试：std::priority_queue<int> pq vs algo::MaxHeap<int>
相同输入序列，弹出顺序完全一致
```

---

## 七、基准测试

### 基准一：push / pop 吞吐量

```
n = 10_000 / 100_000 / 1_000_000
对比：algo::Heap vs std::priority_queue（C++）/ heap 包（Go）
测量：每次 push + pop 的平均 ns
```

### 基准二：heapify vs n 次 push

```
相同数据集，两种方式建堆
验证 Floyd O(n) 相比 n×push O(n log n) 的实际加速比
n = 1_000 / 100_000 / 1_000_000
```

### 基准三：heap_sort vs std::sort

```
n = 100_000 个随机整数
对比 ns/element：heap_sort vs std::sort（C++ introsort）
heap_sort 空间 O(1) vs introsort 的递归栈
```

### 基准四：缓存局部性（数组 vs 链表堆）

```
同规模数据，数组堆 vs 理论指针堆的 cache miss 对比
使用 perf / pprof 测量 L1/L2 cache miss rate
```

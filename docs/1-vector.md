# Dynamic Vector 设计文档

# Dynamic Vector 设计文档

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

Dynamic Vector 是一个管理**连续内存中同质元素序列**的容器，提供以下核心保证：

- **连续性**：所有元素在内存中连续排列，支持 O(1) 随机访问。
- **同质性**：所有元素类型相同，元素大小在初始化时确定，运行时不可更改。
- **动态性**：容量不足时自动扩容，调用者无需手动管理缓冲区。
- **所有权明确**：数组对底层缓冲区拥有唯一所有权；对元素内部资源的所有权由初始化时注入的语义模式决定。

---

## 二、关键语义决策

### 2.1 `get` 越界如何处理？

**决策：返回 `(value, error)`，不约束调用者行为。**

越界访问属于**可预期的运行时错误**，而非不可恢复的程序错误。因此：

- 不使用 `assert` / `panic` 强制终止进程（调用者可能希望优雅降级）。
- 不使用异常抛出（C 不支持，且强制调用者编写 try/catch）。
- 返回 `(value, error)`，将决策权交还调用者，符合"库不应替调用者做策略决定"的原则。

各语言对应形式：

| 语言 | 返回形式 |
|------|---------|
| C | `int vec_get(const Vec *v, size_t i, void *out)` → 返回错误码，`out` 填充结果 |
| C++ | `std::expected<T&, VecError> get(size_t i)` |
| Go | `func (v *Vec[T]) Get(i int) (T, error)` |

### 2.2 `pop` 空数组怎么处理？

**决策：返回 `(value, error)`，不约束调用者行为。**

理由与 `get` 越界一致。返回的 `value` 在错误情况下为零值（Go）或未定义（C/C++，调用者不应读取）。错误类型应明确区分 `ERR_OUT_OF_BOUNDS` 与 `ERR_EMPTY`，便于调用者精确处理。

### 2.3 扩容策略：固定还是可配置？

**决策：同时支持两种方式，通过缺省参数模式兼容。**

| 维度 | 固定 ×2 | 可配置 |
|------|--------|--------|
| API 复杂度 | 低 | 高 |
| 灵活性 | 低 | 高 |
| 均摊复杂度 | O(1) | 取决于策略 |
| 适用场景 | 通用 | 内存/性能敏感场景 |

实现方式：默认使用 ×2 策略；初始化时可注入自定义 `grow_fn(current_cap) → new_cap`。增长函数必须满足 `new_cap > current_cap`，否则视为非法配置（`ERR_INVALID_CONFIG`）。

### 2.4 元素所有权语义？

**决策：支持不托管与托管两种模式，通过初始化时注入钩子函数区分。**

所有权的核心问题：**数组和调用者，谁负责元素内部资源的生死？**

**不托管模式（默认）**：数组只管自身缓冲区，不介入元素内部资源。适用于 POD 类型。

**托管模式**：数组在元素移除或销毁时，调用注入的 `free_fn` 钩子。适用于元素含堆分配字段的场景。

拷贝行为分两种：
- **浅拷贝**（`memcpy`）：适用于 POD，副本与源共享元素内部指针，需调用者自行管理。
- **深拷贝**（逐元素调用 `clone_fn`）：生成完全独立的副本，要求 `clone_fn != NULL`。

移动语义三种语言均以 O(1) 代价完成——转移后源数组 `data` 置空，`len` / `cap` 置零，对已移动的源数组调用任何操作属于编程错误。

### 2.5 借用视图的安全约束

`slice` / `AsSlice` 返回对内部缓冲区的**借用视图**，不转移所有权。

> ⚠️ **调用者契约**：持有视图期间，不得对原数组执行任何可能触发重新分配的操作（`push`、`insert`、`reserve` 等）。违反此约束将导致视图指向已释放的内存，属于未定义行为。C / C++ 在 Debug 构建下通过版本号（generation counter）检测此类违规；Go 版本无法静态强制，依赖调用者自律。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new(elem_size)` | 创建空数组，不托管元素资源 | O(1) |
| `new_with_config(cfg)` | 创建数组，注入扩容策略与元素钩子 | O(1) |
| `new_with_cap(n)` | 创建并预分配 n 个元素容量 | O(1) |
| `destroy(v)` | 销毁；托管模式下逐元素调用 `free_fn` | O(n) |
| `move(dst, src)` | 转移所有权，src 进入已移动状态 | O(1) |
| `copy(dst, src)` | 浅拷贝，dst 获得独立缓冲区 | O(n) |
| `clone(dst, src)` | 深拷贝，需 `clone_fn != NULL` | O(n) |

初始化配置结构：

```
VecConfig {
    elem_size : usize                  // 必填
    init_cap  : usize                  // 选填，默认 4
    grow_fn   : fn(usize) → usize      // 选填，默认 ×2
    clone_fn  : fn(*elem) → *elem      // 选填
    free_fn   : fn(*elem)              // 选填
}
```

### 3.2 基本读写

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `push(v, elem)` | 追加到末尾，必要时扩容 | 均摊 O(1) |
| `pop(v)` | 移除并返回末尾元素 | O(1) |
| `get(v, i)` | 返回索引 i 处元素 | O(1) |
| `set(v, i, elem)` | 设置索引 i 处元素 | O(1) |
| `insert(v, i, elem)` | 在 i 处插入，后续元素后移 | O(n) |
| `remove(v, i)` | 移除 i 处元素，后续元素前移 | O(n) |
| `swap_remove(v, i)` | 移除 i，末尾元素填补（不保序） | O(1) |

`pop` / `remove` 的所有权语义：托管模式下将元素所有权转移给调用者，**不调用** `free_fn`（所有权已转出）。

### 3.3 容量管理

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(v)` | 当前元素数量 | O(1) |
| `cap(v)` | 当前缓冲区容量 | O(1) |
| `is_empty(v)` | 是否为空 | O(1) |
| `reserve(v, n)` | 确保至少 n 个空余槽位 | O(n) |
| `shrink_to_fit(v)` | 容量收缩至 len | O(n) |
| `clear(v)` | 移除所有元素，托管模式调用各元素 `free_fn`，容量不变 | O(n) |

### 3.4 批量操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `extend(v, src, n)` | 追加 n 个元素（浅拷贝） | O(n) |
| `extend_clone(v, src, n)` | 追加 n 个元素（深拷贝，需 `clone_fn`） | O(n) |
| `slice(v, start, end)` | 返回 `[start, end)` 的借用视图，见 [2.5 节](#25-借用视图的安全约束) | O(1) |
| `foreach(v, fn)` | 对每个元素调用 `fn` | O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_OUT_OF_BOUNDS` | 索引 ≥ len | 返回错误码 |
| `ERR_EMPTY` | 对空数组 pop | 返回错误码 |
| `ERR_ALLOC` | 内存分配失败 | 返回错误码，数组状态回滚 |
| `ERR_INVALID_CONFIG` | `grow_fn` 返回值 ≤ current_cap | 初始化时检测，返回 NULL/error |
| `ERR_NO_CLONE_FN` | 调用 clone 但未提供 `clone_fn` | `assert`（编程错误） |

### 4.2 错误处理原则

**可恢复错误**（无效参数、资源不足）：返回错误值，不修改数组状态，调用者决定如何响应。

**不可恢复错误**（违反接口契约的编程错误）：Debug 构建用 `assert` 尽早暴露，Release 构建行为未定义，调用者有义务不违反契约。

### 4.3 内存分配失败的状态保证

所有可能触发内存分配的操作在分配失败时必须保证数组处于**与调用前相同的状态**（强异常安全）：

```
push 失败时的执行顺序：
  1. 尝试分配新缓冲区
  2. 失败 → 返回 ERR_ALLOC，原 data/len/cap 不变
  3. 成功 → 复制旧数据 → 追加新元素 → 释放旧缓冲区 → 更新指针
```

---

## 五、语言实现

### 5.1 C

```c
typedef size_t (*grow_fn_t)(size_t cap);
typedef void*  (*clone_fn_t)(const void *src);
typedef void   (*free_fn_t)(void *elem);

typedef struct {
    void        *data;
    size_t       len;
    size_t       cap;
    size_t       elem_size;
    grow_fn_t    grow;
    clone_fn_t   clone;
    free_fn_t    free_elem;
} Vec;

typedef enum {
    VEC_OK = 0, VEC_ERR_OOB, VEC_ERR_EMPTY, VEC_ERR_ALLOC, VEC_ERR_BAD_CONFIG
} VecError;

/* 生命周期 */
VecError vec_new(Vec *v, size_t elem_size);
VecError vec_new_with_cap(Vec *v, size_t elem_size, size_t init_cap);
VecError vec_new_config(Vec *v, size_t elem_size, size_t init_cap,
                        grow_fn_t grow, clone_fn_t clone, free_fn_t free_elem);
void     vec_destroy(Vec *v);
void     vec_move(Vec *dst, Vec *src);
VecError vec_copy(Vec *dst, const Vec *src);
VecError vec_clone(Vec *dst, const Vec *src);

/* 基本读写 */
VecError vec_push(Vec *v, const void *elem);
VecError vec_pop(Vec *v, void *out);
VecError vec_get(const Vec *v, size_t i, void *out);
VecError vec_set(Vec *v, size_t i, const void *elem);
VecError vec_insert(Vec *v, size_t i, const void *elem);
VecError vec_remove(Vec *v, size_t i, void *out);
VecError vec_swap_remove(Vec *v, size_t i, void *out);

/* 容量管理 */
size_t   vec_len(const Vec *v);
size_t   vec_cap(const Vec *v);
int      vec_is_empty(const Vec *v);
VecError vec_reserve(Vec *v, size_t additional);
VecError vec_shrink_to_fit(Vec *v);
void     vec_clear(Vec *v);
```

> **C 实现注意**：泛型通过 `void *` + `elem_size` 实现，所有元素读写均经由 `memcpy`，不支持含 VLA 或柔性数组成员的元素类型。`vec_move` 后调用者有责任不再使用 `src`，C 语言无法在编译期强制。

### 5.2 C++

```cpp
template<typename T>
class Vec {
public:
    using GrowFn = std::function<size_t(size_t)>;

    explicit Vec(size_t init_cap = 4, GrowFn grow = default_grow);
    Vec(const Vec& other);              // 深拷贝（调用 T 的拷贝构造）
    Vec(Vec&& other) noexcept;          // 移动构造
    Vec& operator=(Vec other) noexcept; // copy-and-swap
    ~Vec();

    VecError push(const T& elem);
    VecError push(T&& elem);
    std::expected<T, VecError>                               pop();
    std::expected<std::reference_wrapper<T>, VecError>       get(size_t i);
    std::expected<std::reference_wrapper<const T>, VecError> get(size_t i) const;
    VecError set(size_t i, T elem);
    VecError insert(size_t i, T elem);
    std::expected<T, VecError> remove(size_t i);
    std::expected<T, VecError> swap_remove(size_t i);

    size_t   len()      const noexcept;
    size_t   cap()      const noexcept;
    bool     is_empty() const noexcept;
    VecError reserve(size_t additional);
    VecError shrink_to_fit();
    void     clear();

private:
    T      *data_ = nullptr;
    size_t  len_  = 0;
    size_t  cap_  = 0;
    GrowFn  grow_;
    static size_t default_grow(size_t cap) { return cap == 0 ? 4 : cap * 2; }
};
```

> **C++ 实现注意**：遵循 Rule of Five；移动构造与移动赋值必须标记 `noexcept`，否则 STL 容器无法利用移动优化。析构函数对 `data_ == nullptr` 安全。使用 `std::expected` 需 C++23，C++17 可替换为 `std::optional` + 独立错误参数或第三方 `tl::expected`。

### 5.3 Go

```go
type GrowFn func(cap int) int

type Vec[T any] struct {
    data   []T
    growFn GrowFn
}

type VecError int
const (
    ErrOutOfBounds VecError = iota + 1
    ErrEmpty
    ErrAlloc
)

func New[T any](opts ...Option) *Vec[T]
func (v *Vec[T]) Clone(cloneFn func(T) T) *Vec[T]

func (v *Vec[T]) Push(elem T) error
func (v *Vec[T]) Pop() (T, error)
func (v *Vec[T]) Get(i int) (T, error)
func (v *Vec[T]) Set(i int, elem T) error
func (v *Vec[T]) Insert(i int, elem T) error
func (v *Vec[T]) Remove(i int) (T, error)
func (v *Vec[T]) SwapRemove(i int) (T, error)

func (v *Vec[T]) Len() int
func (v *Vec[T]) Cap() int
func (v *Vec[T]) IsEmpty() bool
func (v *Vec[T]) Reserve(additional int) error
func (v *Vec[T]) ShrinkToFit()
func (v *Vec[T]) Clear()
func (v *Vec[T]) AsSlice() []T  // 借用视图，见 2.5 节
```

> **Go 实现注意**：底层直接使用 `[]T`，`append` 在扩容时会分配新底层数组并复制，`AsSlice` 返回的 slice header 持有旧数组指针，扩容后将静默失效。`ErrAlloc` 在 Go 中通常不会触发（OOM 会直接 panic），保留此错误码是为了与 C/C++ 接口对齐，实现上可在 `recover` 中捕获 OOM panic 并转换。

---

## 六、功能测试

### 6.1 测试分类

| 类别 | 覆盖内容 |
|------|---------|
| 正常路径 | push/pop/get/set 的基础语义 |
| 边界条件 | 空数组、单元素、容量恰好满时 push、索引为 0 和 len-1 |
| 错误路径 | 越界访问、空数组 pop、配置非法 |
| 扩容行为 | 扩容后元素完整保留、容量按策略增长 |
| 所有权语义 | 拷贝/克隆/移动后的独立性验证、`free_fn` 调用次数 |
| 内存安全 | Valgrind（C）/ ASan（C++）验证无泄漏、无悬空指针 |

### 6.2 关键测试用例

```
TC-01  push 直到触发扩容，验证所有元素值正确
TC-02  pop 至空，最后一次返回 ERR_EMPTY
TC-03  get(0) 和 get(len-1) 返回正确值
TC-04  get(len) 返回 ERR_OUT_OF_BOUNDS
TC-05  insert(0, x) 后原有元素整体后移一位
TC-06  remove(0) 后原有元素整体前移一位
TC-07  swap_remove(i) 后末尾元素出现在 i 位置，len 减 1
TC-08  copy 后修改副本，源数组不受影响（POD 类型）
TC-09  clone 后修改副本内部字段，源数组不受影响（含指针类型）
TC-10  move 后源数组 len == 0 且 data == NULL
TC-11  托管模式 clear：free_fn 被调用恰好 len 次
TC-12  托管模式 destroy：free_fn 被调用 len 次后 data 被释放
TC-13  自定义扩容策略（×1.5），验证扩容后容量符合预期
TC-14  内存分配失败模拟：push 返回 ERR_ALLOC，数组状态不变
       └─ C/C++：通过替换 malloc 为故障注入版本实现
       └─ Go：通过 recover 捕获 OOM panic，此用例验证状态回滚逻辑
```

### 6.3 测试工具

| 语言 | 单元测试框架 | 内存检查 |
|------|------------|---------|
| C | Unity 或 CMocka | Valgrind / AddressSanitizer |
| C++ | Google Test | AddressSanitizer / UBSan |
| Go | `testing` 标准库 | `go test -race` |

---

## 七、基准测试

### 7.1 测试场景

| 场景 | 目的 |
|------|------|
| 顺序 push N 个元素 | 均摊 push 吞吐量，扩容开销 |
| 预分配后顺序 push | 量化 `reserve` 的收益 |
| 顺序 pop N 个元素 | pop 吞吐量 |
| 随机 get | 缓存未命中对随机访问的影响 |
| 顺序 get（遍历） | 连续内存访问峰值 |
| `insert(0, x)` N 次 | 头部插入的 O(n) 移动开销 |
| 扩容策略对比（×2 vs ×1.5） | 内存占用 vs 扩容次数权衡 |
| 深拷贝 vs 浅拷贝（含指针元素） | `clone_fn` 调用开销 |

### 7.2 基准指标

| 指标 | 说明 |
|------|------|
| 吞吐量（ops/s） | 每秒完成的操作数 |
| 延迟（ns/op） | 单次操作平均时间 |
| 内存峰值（bytes） | 操作过程中最大内存占用 |
| 扩容次数 | push N 个元素期间触发的扩容总次数 |

### 7.3 基准工具与参考基线

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` 手写计时 | 朴素 `realloc` 实现 |
| C++ | Google Benchmark | `std::vector<T>` |
| Go | `testing.B`（`go test -bench`） | 原生 `[]T` append |
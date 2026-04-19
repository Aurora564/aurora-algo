# Binary Search Tree 设计文档

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

Binary Search Tree（BST）是一棵满足**搜索性质**的二叉树：

- 每个节点存储一个键值对 `(key, value)`。
- 对任意节点 `n`：左子树中所有键 `< n.key`，右子树中所有键 `> n.key`。
- **不允许重复键**：重复插入视为更新 `value`，不产生重复节点。

BST 提供以下核心保证：

| 操作 | 平均复杂度 | 最坏复杂度（退化为链表）|
|------|-----------|----------------------|
| `insert` | O(log n) | O(n) |
| `search` | O(log n) | O(n) |
| `delete` | O(log n) | O(n) |
| `min` / `max` | O(log n) | O(n) |
| `in_order` 遍历 | O(n) | O(n) |

本文档实现的 BST **不自平衡**（无旋转），最坏情况退化为 O(n)。需要平衡保证的场景请参考 [AVL 树文档](9-avl-tree.md) 或 [红黑树文档](10-red-black-tree.md)。

---

## 二、关键语义决策

### 2.1 节点所有权模型

**决策：树独占所有节点；销毁树时递归释放全部节点。**

三语言所有权策略如下：

| 语言 | 节点分配 | 释放策略 |
|------|---------|---------|
| C | `malloc` 逐节点分配 | `bst_destroy` 递归 `free` |
| C++ | `new` 逐节点分配，`unique_ptr` 管理子指针 | 析构函数级联释放 |
| Go | `new(bstNode[K, V])` | GC 自动回收 |

C++ 使用 `std::unique_ptr<Node>` 管理左右子节点，析构时自动递归释放，无需手写 `destroy`。

> ⚠️ **C 语言深度限制**：极深树（>10 000 层）递归释放可能栈溢出。生产 C 代码建议改用迭代后序遍历释放，本文档实现采用递归，优先清晰性。

### 2.2 删除节点的三种 case

删除是 BST 中最复杂的操作，存在三种情形：

```
Case 1  叶节点（无子节点）
        → 直接删除，父节点对应子指针置 NULL

Case 2  只有一个子节点
        → 用唯一子节点替换被删除节点

Case 3  有两个子节点
        → 找中序后继（右子树最小值），
          将后继的 key/value 复制到当前节点，
          然后在右子树中删除后继节点
          （后继最多有一个右子节点，退化为 Case 1 或 2）
```

**决策：选择中序后继（而非中序前驱）替换，理由是实现上略简单——后继只需一路向左走，无需维护父指针。**

### 2.3 中序遍历 API：回调 vs 迭代器

**决策：提供回调（visitor 函数）风格，同时在 Go 版本提供 channel/迭代器风格。**

| 风格 | 优点 | 缺点 |
|------|------|------|
| 回调 `foreach(fn)` | 无需额外分配，适合 C | 无法在遍历中途 break |
| 迭代器 / range | 可被 for-range 消费（Go）| C 中实现复杂，需显式栈 |
| channel（Go） | 最惯用，支持并发消费 | 每次遍历启动 goroutine，有调度开销 |

Go 版本提供两种：`InOrder(fn func(K, V))` 回调形式 + `InOrderChan() <-chan KV[K, V]` channel 形式。

### 2.4 键的比较：类型参数 vs 注入比较函数

| 语言 | 策略 | 实现形式 |
|------|------|---------|
| C | 注入 `cmp_fn(a, b) → int` | 初始化时传入函数指针 |
| C++ | 模板参数 `Compare`，默认 `std::less<K>` | `BST<K, V, Compare = std::less<K>>` |
| Go | 泛型约束 `cmp.Ordered`，使用 `cmp.Compare` | 支持所有内置有序类型；自定义类型可传入比较函数 |

### 2.5 `search` 的返回语义：指针 vs 拷贝 vs optional

**决策：返回值的可选形式（不返回裸指针），避免悬空引用风险。**

| 语言 | 返回形式 |
|------|---------|
| C | `int bst_search(const BST *t, Key key, Value *out)` → 返回 0/1，结果填入 `out` |
| C++ | `std::expected<std::reference_wrapper<V>, BSTError> search(const K& key)` |
| Go | `func (t *BST[K, V]) Search(key K) (V, bool)` |

C++ 返回引用包装而非拷贝，允许调用者原地修改 value；若不需要修改，可通过 `const` 重载返回 `const` 引用。

### 2.6 是否支持重复键

**决策：不支持，重复插入视为 upsert（更新 value）。**

支持重复键会使删除操作语义复杂化（删除哪一个？），且大多数有序映射语义本就不允许重复键。有序多重映射场景建议使用 multiset 变体（超出本文档范围）。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new(cmp_fn)` | 创建空 BST | O(1) |
| `destroy(t)` | 销毁所有节点 | O(n) |
| `clone(t)` | 深拷贝整棵树 | O(n) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `insert(key, value)` | 插入；键已存在则更新 value | O(h) |
| `search(key)` | 查找键，返回 value 或 not-found | O(h) |
| `delete(key)` | 删除节点（三 case 策略） | O(h) |
| `contains(key)` | 是否包含键 | O(h) |
| `min()` | 返回最小键的节点 | O(h) |
| `max()` | 返回最大键的节点 | O(h) |
| `successor(key)` | 中序后继节点 | O(h) |
| `predecessor(key)` | 中序前驱节点 | O(h) |

*h = 树高；平均 O(log n)，最坏 O(n)。*

### 3.3 遍历

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `in_order(fn)` | 中序遍历（升序），对每个节点调用 fn | O(n) |
| `pre_order(fn)` | 前序遍历 | O(n) |
| `post_order(fn)` | 后序遍历 | O(n) |
| `level_order(fn)` | 层序遍历（BFS，需辅助队列）| O(n) |

### 3.4 统计与调试

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(t)` | 节点总数 | O(1) |
| `height(t)` | 树高（空树为 0） | O(n) |
| `is_empty(t)` | 是否为空 | O(1) |
| `is_valid_bst(t)` | 验证搜索性质（测试辅助）| O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_NOT_FOUND` | `search` / `delete` 键不存在 | 返回错误码 |
| `ERR_ALLOC` | 节点内存分配失败 | 返回错误码，树状态不变 |
| `ERR_EMPTY` | 对空树调用 `min` / `max` | 返回错误码 |

### 4.2 不变式保证

每次 `insert` / `delete` 操作完成后，树必须满足 BST 搜索性质。可通过 `is_valid_bst` 在测试中断言：

```
post-insert:  is_valid_bst(t) == true
post-delete:  is_valid_bst(t) == true
              len(t) == prev_len - 1  （若键存在）
```

---

## 五、语言实现

### 5.1 C

```c
typedef int  (*bst_cmp_fn)(const void *a, const void *b);
typedef void (*bst_visit_fn)(const void *key, void *value, void *user_data);

typedef struct BSTNode {
    void           *key;
    void           *value;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

typedef struct {
    BSTNode    *root;
    size_t      len;
    bst_cmp_fn  cmp;
} BST;

typedef enum {
    BST_OK = 0,
    BST_ERR_NOT_FOUND,
    BST_ERR_ALLOC,
    BST_ERR_EMPTY,
} BSTError;

/* 生命周期 */
BSTError bst_new(BST *t, bst_cmp_fn cmp);
void     bst_destroy(BST *t);
BSTError bst_clone(BST *dst, const BST *src, bst_cmp_fn key_clone,
                   bst_cmp_fn val_clone);

/* 核心操作 */
BSTError bst_insert(BST *t, void *key, void *value);
BSTError bst_search(const BST *t, const void *key, void **value_out);
BSTError bst_delete(BST *t, const void *key);
int      bst_contains(const BST *t, const void *key);
BSTError bst_min(const BST *t, void **key_out, void **value_out);
BSTError bst_max(const BST *t, void **key_out, void **value_out);

/* 遍历 */
void bst_in_order(const BST *t, bst_visit_fn fn, void *user_data);
void bst_pre_order(const BST *t, bst_visit_fn fn, void *user_data);
void bst_post_order(const BST *t, bst_visit_fn fn, void *user_data);
void bst_level_order(const BST *t, bst_visit_fn fn, void *user_data);

/* 统计 */
size_t bst_len(const BST *t);
size_t bst_height(const BST *t);
int    bst_is_empty(const BST *t);
int    bst_is_valid(const BST *t);  /* 测试辅助 */
```

> **C 实现注意**：键和值均为 `void *`，不拥有所指向内存；调用者负责键和值的生命周期。`bst_delete` 仅断开节点指针并 `free` 节点本身，不释放键/值内存。需要托管键值资源时，在初始化时额外注入 `key_free_fn` / `val_free_fn`（本文档简化版本省略）。

### 5.2 C++

```cpp
namespace algo {

template<typename K, typename V, typename Compare = std::less<K>>
class BST {
public:
    BST() = default;
    explicit BST(Compare cmp);
    BST(const BST& other);               // 深拷贝
    BST(BST&& other) noexcept;           // 移动
    BST& operator=(BST other) noexcept;  // copy-and-swap
    ~BST() = default;                    // unique_ptr 自动释放

    /* 核心操作 */
    std::expected<void, BSTError> insert(K key, V value);
    std::expected<std::reference_wrapper<V>, BSTError>       search(const K& key);
    std::expected<std::reference_wrapper<const V>, BSTError> search(const K& key) const;
    std::expected<void, BSTError> remove(const K& key);
    bool contains(const K& key) const noexcept;
    std::expected<std::pair<const K&, V&>, BSTError> min();
    std::expected<std::pair<const K&, V&>, BSTError> max();

    /* 遍历 */
    void in_order(std::invocable<const K&, V&> auto fn);
    void pre_order(std::invocable<const K&, V&> auto fn);
    void post_order(std::invocable<const K&, V&> auto fn);
    void level_order(std::invocable<const K&, V&> auto fn);

    /* 统计 */
    size_t len()      const noexcept;
    size_t height()   const noexcept;
    bool   is_empty() const noexcept;
    bool   is_valid() const noexcept;  /* 测试辅助 */

    friend void swap(BST& a, BST& b) noexcept;

private:
    struct Node {
        K                        key;
        V                        value;
        std::unique_ptr<Node>    left;
        std::unique_ptr<Node>    right;

        Node(K k, V v) : key(std::move(k)), value(std::move(v)) {}
    };

    std::unique_ptr<Node> root_;
    size_t                len_  = 0;
    Compare               cmp_;

    /* 递归实现辅助 */
    static std::expected<void, BSTError>
    insert_node(std::unique_ptr<Node>& node, K key, V value,
                const Compare& cmp, size_t& len);

    static std::expected<void, BSTError>
    remove_node(std::unique_ptr<Node>& node, const K& key,
                const Compare& cmp, size_t& len);

    static Node* find_min(Node* node) noexcept;
    static size_t height_of(const Node* node) noexcept;
    static bool   is_valid_range(const Node* node, const K* lo, const K* hi,
                                 const Compare& cmp) noexcept;
};

enum class BSTError { NotFound, Alloc, Empty };

} /* namespace algo */
```

> **C++ 实现注意**：`std::unique_ptr<Node>` 管理子节点，析构时自动递归释放——对于极深的退化树，此递归析构可能栈溢出（同 C 的注意事项）。`level_order` 需要内部维护 `std::queue<Node*>`，是唯一需要辅助数据结构的遍历。C++20 `std::invocable` concept 使 `in_order` 等遍历接口接受任意可调用对象（lambda、函数指针、仿函数）。

### 5.3 Go

```go
package linear

import "cmp"

type BSTV[K cmp.Ordered, V any] struct {
    root *bstNode[K, V]
    len  int
}

type bstNode[K cmp.Ordered, V any] struct {
    key   K
    value V
    left  *bstNode[K, V]
    right *bstNode[K, V]
}

type KV[K, V any] struct {
    Key   K
    Value V
}

/* 工厂 */
func NewBST[K cmp.Ordered, V any]() *BSTV[K, V]

/* 核心操作 */
func (t *BSTV[K, V]) Insert(key K, value V)
func (t *BSTV[K, V]) Search(key K) (V, bool)
func (t *BSTV[K, V]) Delete(key K) bool
func (t *BSTV[K, V]) Contains(key K) bool
func (t *BSTV[K, V]) Min() (K, V, bool)
func (t *BSTV[K, V]) Max() (K, V, bool)
func (t *BSTV[K, V]) Successor(key K) (K, V, bool)
func (t *BSTV[K, V]) Predecessor(key K) (K, V, bool)

/* 遍历 */
func (t *BSTV[K, V]) InOrder(fn func(K, V))
func (t *BSTV[K, V]) PreOrder(fn func(K, V))
func (t *BSTV[K, V]) PostOrder(fn func(K, V))
func (t *BSTV[K, V]) LevelOrder(fn func(K, V))

/* channel 风格（惰性，逐节点发送） */
func (t *BSTV[K, V]) InOrderChan() <-chan KV[K, V]

/* 统计 */
func (t *BSTV[K, V]) Len()     int
func (t *BSTV[K, V]) Height()  int
func (t *BSTV[K, V]) IsEmpty() bool
func (t *BSTV[K, V]) IsValid() bool  /* 测试辅助 */
```

> **Go 实现注意**：`cmp.Ordered` 约束覆盖所有内置有序类型（整数、浮点、字符串），无需注入比较函数；如需自定义类型，可通过另一版本 `BSTCustom[K any, V any]` 接受 `func(a, b K) int` 参数。`InOrderChan` 在内部 goroutine 中执行遍历，调用者读完 channel 后 goroutine 自然退出；若提前停止读取，需关闭 done channel（可通过 context.Context 传入）。

---

## 六、功能测试

### 6.1 测试分类

| 类别 | 覆盖内容 |
|------|---------|
| 正常路径 | insert / search / delete 基础语义 |
| 边界条件 | 空树、单节点、删除根节点、连续删到空树 |
| 删除三 case | 叶节点、单子节点、双子节点（验证中序后继替换） |
| 重复插入 | 键已存在时 value 更新，len 不变 |
| 搜索性质 | 每次操作后 `is_valid_bst` 断言 |
| 遍历顺序 | 中序遍历结果严格升序 |
| 极端有序输入 | 正序 / 逆序插入 N 个元素，退化为链表，操作仍正确 |
| 克隆独立性 | 克隆后修改原树，副本不受影响 |

### 6.2 关键测试用例

```
TC-01  空树 search → ERR_NOT_FOUND / false
TC-02  insert 5 个无序键，in_order 返回严格升序序列
TC-03  search 存在的键 → 返回正确 value
TC-04  search 不存在的键 → ERR_NOT_FOUND
TC-05  delete 叶节点 → len 减 1，is_valid_bst == true
TC-06  delete 单子节点 → len 减 1，is_valid_bst == true
TC-07  delete 双子节点 → len 减 1，is_valid_bst == true，中序序列正确
TC-08  delete 根节点（各种子情形）
TC-09  重复 insert 同一键 → len 不变，value 更新
TC-10  min() / max() 返回正确键值
TC-11  successor / predecessor 边界：最大键无后继，最小键无前驱
TC-12  顺序插入 1…1000，height ≈ 1000（验证退化），in_order 仍正确
TC-13  clone 后两棵树互相独立（修改一棵不影响另一棵）
TC-14  level_order 遍历顺序：根 → 左子 → 右子 → 孙…（BFS 验证）
```

### 6.3 测试工具

| 语言 | 单元测试框架 | 内存检查 |
|------|------------|---------|
| C | 自制 CHECK 宏（与线性结构一致）| Valgrind / AddressSanitizer |
| C++ | 自制 CHECK 宏（与线性结构一致）| AddressSanitizer / UBSan |
| Go | `testing` 标准库 + 表驱动测试 | `go test -race` |

---

## 七、基准测试

### 7.1 测试场景

| 场景 | 目的 |
|------|------|
| 随机插入 N 个键 | 平均 O(log n) 插入吞吐量 |
| 顺序插入 N 个键 | 退化 O(n) 对比 |
| 随机 search N 次 | 查找吞吐量（命中 vs 未命中）|
| 随机 delete N/2 个键 | 删除吞吐量 |
| in_order 遍历 | 完整遍历吞吐量 |
| BST vs `std::map` / Go `map` | 与标准库有序映射的差距量化 |

### 7.2 基准指标

| 指标 | 说明 |
|------|------|
| 吞吐量（ops/s） | 每秒完成的操作数 |
| 延迟（ns/op） | 单次操作平均时间 |
| 树高 | 插入 N 个随机键后实测高度 vs 期望 2 ln N |
| 内存占用（bytes/node） | 节点本身 + 指针开销 |

### 7.3 基准工具与参考基线

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` 手写计时 | 线性扫描有序数组 |
| C++ | Google Benchmark | `std::map<K, V>`（红黑树）|
| Go | `testing.B`（`go test -bench`）| 原生 `map[K]V`（哈希表）|

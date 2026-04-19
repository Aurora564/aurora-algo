# Red-Black Tree 设计文档

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

红黑树（Red-Black Tree）是一种自平衡二叉搜索树，通过对每个节点着色（红/黑）并维护以下五条性质来保证树高在 O(log n) 以内：

| 性质 | 描述 |
|------|------|
| **P1** | 每个节点是红色或黑色 |
| **P2** | 根节点是黑色 |
| **P3** | 叶节点（NIL 哨兵）是黑色 |
| **P4** | 红色节点的两个子节点必须是黑色（不存在连续红节点） |
| **P5** | 从任一节点到其所有后代叶节点的路径上，经过的黑色节点数量相同（**黑高** black-height 一致） |

由 P4 和 P5 可推导出：最长路径 ≤ 2 × 最短路径，树高 ≤ 2 * log₂(n+1)。

| 操作 | 平均 | 最坏 |
|------|------|------|
| `insert` | O(log n) | O(log n) |
| `search` | O(log n) | O(log n) |
| `delete` | O(log n) | O(log n) |
| 遍历 | O(n) | O(n) |

**与 AVL 树的对比：**

| 维度 | AVL 树 | 红黑树 |
|------|--------|--------|
| 平衡严格程度 | 严格（BF ≤ 1） | 宽松（黑高一致） |
| 最大树高 | ≈ 1.44 log n | ≤ 2 log n |
| insert 旋转次数 | ≤ 2 | ≤ 2 |
| delete 旋转次数 | ≤ O(log n) | ≤ 3（恒定） |
| 适用场景 | 读多写少 | 读写均衡（Linux 内核、Java TreeMap） |

---

## 二、关键语义决策

### 2.1 NIL 哨兵节点

**决策：使用单个共享的黑色 NIL 哨兵节点，而非 `NULL`。**

| 方案 | 优点 | 缺点 |
|------|------|------|
| `NULL` 表示叶节点 | 无额外分配 | 旋转和修复代码需要大量 NULL 检查 |
| **共享 NIL 哨兵（本实现）** | 消除 NULL 检查，代码更简洁 | 需要全局/树级别的哨兵对象 |

NIL 节点固定为黑色（满足 P3），`nil->left = nil->right = nil->parent = nil`。

```
TreeNode NIL_SENTINEL = {
    .color  = BLACK,
    .left   = &NIL_SENTINEL,
    .right  = &NIL_SENTINEL,
    .parent = &NIL_SENTINEL,
};
```

> C 实现使用全局 `static` NIL；C++ 使用 `inline static` 或 `thread_local`；Go 使用包级变量（注意泛型实例化需独立 NIL）。

### 2.2 父指针

**决策：红黑树节点存储父指针 `parent`。**

红黑树的插入修复（fixup）和删除修复需要访问兄弟节点，而兄弟节点需通过父指针获取。与 AVL 树（无父指针，通过递归回溯修复）不同，红黑树采用迭代修复，**必须维护父指针**。

```
struct RBNode {
    void    *key, *value;
    Color    color;
    RBNode  *parent;
    RBNode  *left, *right;
};
```

### 2.3 插入修复（Insert Fixup）

插入后新节点着为**红色**，可能违反 P4（父子均红）。修复分三种情形（相对于祖父节点的叔节点颜色）：

```
Case 1  叔节点为红色
        → 父和叔染黑，祖父染红，继续向上处理祖父

Case 2  叔节点为黑色，新节点是父的"内侧"子（LR 或 RL）
        → 先旋转父节点，转化为 Case 3

Case 3  叔节点为黑色，新节点是父的"外侧"子（LL 或 RR）
        → 旋转祖父，交换父/祖父颜色，修复完成
```

插入修复最多执行 **2 次旋转**。

### 2.4 删除修复（Delete Fixup）

删除是红黑树最复杂的部分。删除策略：先用 BST 方式删除节点，若被删节点（或其后继）为黑色，则可能违反 P5，需修复"双黑"节点。修复分 4 种情形（基于兄弟节点颜色及其子节点颜色）：

```
Case 1  兄弟节点为红色
        → 旋转父节点，交换父/兄颜色，转化为 Case 2/3/4

Case 2  兄弟节点为黑色，兄弟两子均黑
        → 兄弟染红，双黑上移至父节点

Case 3  兄弟节点为黑色，兄弟"近侧"子红、"远侧"子黑
        → 旋转兄弟，交换兄/近子颜色，转化为 Case 4

Case 4  兄弟节点为黑色，兄弟"远侧"子红
        → 旋转父节点，父颜色赋给兄弟，父和远子染黑，修复完成
```

删除修复最多执行 **3 次旋转**（恒定上界，优于 AVL 的 O(log n) 次）。

### 2.5 与 BST / AVL 的接口一致性

红黑树对外接口语义与 BST / AVL 文档完全一致：
- 不允许重复键，重复插入视为 upsert
- 键比较策略：C 注入函数指针，C++ 模板 `Compare`，Go `cmp.Ordered`
- 返回语义：C `(value_out, error_code)`，C++ `std::expected`，Go `(V, bool)`

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new(cmp_fn)` | 创建空红黑树 | O(1) |
| `destroy(t)` | 销毁所有节点 | O(n) |
| `clone(t)` | 深拷贝整棵树（含颜色） | O(n) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `insert(key, value)` | 插入并修复红黑性质 | O(log n) |
| `search(key)` | 查找键 | O(log n) |
| `delete(key)` | 删除并修复红黑性质 | O(log n) |
| `contains(key)` | 是否包含键 | O(log n) |
| `min()` | 返回最小键 | O(log n) |
| `max()` | 返回最大键 | O(log n) |

### 3.3 遍历

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `in_order(fn)` | 中序遍历（升序） | O(n) |
| `pre_order(fn)` | 前序遍历 | O(n) |
| `level_order(fn)` | 层序遍历 | O(n) |

### 3.4 统计与调试

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(t)` | 节点总数 | O(1) |
| `height(t)` | 树高 | O(n)（需遍历，无缓存） |
| `black_height(t)` | 根的黑高 | O(log n) |
| `is_empty(t)` | 是否为空 | O(1) |
| `is_valid_rb(t)` | 验证五条性质（测试辅助） | O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_NOT_FOUND` | `search` / `delete` 键不存在 | 返回错误码，树状态不变 |
| `ERR_ALLOC` | 节点内存分配失败 | 返回错误码，树状态不变 |
| `ERR_EMPTY` | 对空树调用 `min` / `max` | 返回错误码 |

### 4.2 不变式保证

每次操作后，树必须满足全部五条红黑性质：

```
post-insert:  is_valid_rb(t) == true
post-delete:  is_valid_rb(t) == true
              len(t) == prev_len - 1  （若键存在）
```

`is_valid_rb` 的验证内容：

```
1. 根节点为黑色
2. 没有连续红节点
3. 所有路径黑高相等
4. BST 搜索性质成立
```

---

## 五、语言实现

### 5.1 C

```c
typedef enum { RB_RED = 0, RB_BLACK = 1 } RBColor;
typedef int  (*rb_cmp_fn)(const void *a, const void *b);
typedef void (*rb_visit_fn)(const void *key, void *value, void *user_data);

typedef enum {
    RB_OK        = 0,
    RB_NOT_FOUND = -1,
    RB_ALLOC_ERR = -2,
    RB_EMPTY     = -3,
} RBError;

typedef struct RBNode {
    void           *key;
    void           *value;
    RBColor         color;
    struct RBNode  *parent;
    struct RBNode  *left;
    struct RBNode  *right;
} RBNode;

typedef struct {
    RBNode    *root;
    RBNode    *nil;       /* 共享 NIL 哨兵 */
    size_t     len;
    rb_cmp_fn  cmp;
} RBTree;

/* 生命周期 */
RBTree  *rb_new(rb_cmp_fn cmp);
void     rb_destroy(RBTree *t);

/* 核心操作 */
RBError  rb_insert(RBTree *t, void *key, void *value);
RBError  rb_search(const RBTree *t, const void *key, void **value_out);
RBError  rb_delete(RBTree *t, const void *key);
int      rb_contains(const RBTree *t, const void *key);

/* 极值 */
RBError  rb_min(const RBTree *t, void **key_out, void **value_out);
RBError  rb_max(const RBTree *t, void **key_out, void **value_out);

/* 遍历 */
void     rb_in_order(const RBTree *t, rb_visit_fn fn, void *user_data);

/* 统计 */
size_t   rb_len(const RBTree *t);
int      rb_is_empty(const RBTree *t);
int      rb_black_height(const RBTree *t);

/* 测试辅助 */
int      rb_is_valid(const RBTree *t);

/* 内部：旋转 */
static void rb_rotate_left(RBTree *t, RBNode *n);
static void rb_rotate_right(RBTree *t, RBNode *n);
static void rb_insert_fixup(RBTree *t, RBNode *n);
static void rb_delete_fixup(RBTree *t, RBNode *n);
static void rb_transplant(RBTree *t, RBNode *u, RBNode *v);
```

### 5.2 C++

```cpp
namespace algo {

template <typename K, typename V, typename Compare = std::less<K>>
class RBTree {
public:
    enum class Error { NotFound, AllocFail, Empty };

    RBTree();
    ~RBTree() = default;
    RBTree(const RBTree& other);
    RBTree(RBTree&&) noexcept = default;
    RBTree& operator=(RBTree other) noexcept;

    /* 核心操作 */
    [[nodiscard]] std::expected<void, Error>
        insert(K key, V value);

    [[nodiscard]] std::expected<std::reference_wrapper<V>, Error>
        search(const K& key);

    [[nodiscard]] std::expected<void, Error>
        remove(const K& key);

    bool contains(const K& key) const;

    /* 极值 */
    [[nodiscard]] std::expected<std::pair<const K&, V&>, Error> min();
    [[nodiscard]] std::expected<std::pair<const K&, V&>, Error> max();

    /* 遍历 */
    template <std::invocable<const K&, V&> Fn>
    void in_order(Fn&& fn);

    /* 统计 */
    [[nodiscard]] std::size_t size()         const noexcept { return len_; }
    [[nodiscard]] bool        empty()        const noexcept;
    [[nodiscard]] int         black_height() const noexcept;

    /* 测试辅助 */
    bool is_valid_rb() const;

    friend void swap(RBTree& a, RBTree& b) noexcept;

private:
    enum class Color : uint8_t { Red, Black };

    struct Node {
        K      key;
        V      value;
        Color  color{Color::Red};
        Node  *parent{nullptr};
        Node  *left{nullptr};
        Node  *right{nullptr};
    };

    Node*  nil_;          /* 指向共享 NIL 哨兵 */
    Node*  root_;
    std::size_t len_{0};
    [[no_unique_address]] Compare cmp_{};

    /* 旋转与修复 */
    void rotate_left(Node* n);
    void rotate_right(Node* n);
    void insert_fixup(Node* n);
    void delete_fixup(Node* n);
    void transplant(Node* u, Node* v);
    Node* find_min(Node* n) const noexcept;
};

} // namespace algo
```

> **实现说明**：C++ 中 NIL 哨兵为 `Node` 类型的堆对象（在构造函数中分配），由 `RBTree` 独占管理。拷贝构造时复制整棵树但共用新的 NIL 哨兵。

### 5.3 Go

```go
package linear

import "cmp"

type rbColor bool

const (
    rbRed   rbColor = false
    rbBlack rbColor = true
)

// RBTree 红黑树，键类型需满足 cmp.Ordered 约束
type RBTree[K cmp.Ordered, V any] struct {
    nil_ *rbNode[K, V]   // NIL 哨兵（指针，泛型实例各自独立）
    root *rbNode[K, V]
    len  int
}

type rbNode[K cmp.Ordered, V any] struct {
    key    K
    value  V
    color  rbColor
    parent *rbNode[K, V]
    left   *rbNode[K, V]
    right  *rbNode[K, V]
}

// 构造
func NewRBTree[K cmp.Ordered, V any]() *RBTree[K, V]

// 核心操作
func (t *RBTree[K, V]) Insert(key K, value V)
func (t *RBTree[K, V]) Search(key K) (V, bool)
func (t *RBTree[K, V]) Delete(key K) bool
func (t *RBTree[K, V]) Contains(key K) bool

// 极值
func (t *RBTree[K, V]) Min() (K, V, bool)
func (t *RBTree[K, V]) Max() (K, V, bool)

// 遍历
func (t *RBTree[K, V]) InOrder(fn func(K, V))
func (t *RBTree[K, V]) InOrderChan() <-chan KV[K, V]

// 统计
func (t *RBTree[K, V]) Len()         int
func (t *RBTree[K, V]) Empty()       bool
func (t *RBTree[K, V]) BlackHeight() int

// 测试辅助
func (t *RBTree[K, V]) IsValidRB() bool

// 内部（非导出）
func (t *RBTree[K, V]) rotateLeft(n *rbNode[K, V])
func (t *RBTree[K, V]) rotateRight(n *rbNode[K, V])
func (t *RBTree[K, V]) insertFixup(n *rbNode[K, V])
func (t *RBTree[K, V]) deleteFixup(n *rbNode[K, V])
func (t *RBTree[K, V]) transplant(u, v *rbNode[K, V])
```

> **Go 泛型 NIL 哨兵**：由于 Go 泛型每个类型实例化独立，无法使用全局 NIL。`NewRBTree` 在堆上分配一个黑色哨兵节点，所有叶子指向此哨兵。

---

## 六、功能测试

### TC-01  空树基本属性

```
t := NewRBTree[int, string]()
ASSERT t.Len() == 0
ASSERT t.Empty() == true
ASSERT t.IsValidRB() == true
ASSERT t.BlackHeight() == 0
```

### TC-02  单节点插入

```
t.Insert(10, "ten")
ASSERT t.Len() == 1
ASSERT root 为黑色（P2）
ASSERT t.IsValidRB() == true
```

### TC-03  连续插入验证五条性质

```
for _, k := range [10, 20, 30, 15, 5, 25, 1]:
    t.Insert(k, _)
    ASSERT t.IsValidRB() == true
中序输出: [1, 5, 10, 15, 20, 25, 30]
```

### TC-04  顺序插入（最坏 BST 退化场景）

```
for i in [1..1000]:
    t.Insert(i, _)
ASSERT t.IsValidRB() == true
ASSERT t.Len() == 1000
ASSERT t.BlackHeight() 一致（通过 IsValidRB 验证）
树高上界: t.Height() <= 2 * ceil(log2(1001))  // ≈ 20
```

### TC-05  删除叶节点（红色）

```
插入 [10, 5, 20]，删除 5
（5 若为红叶，直接删除，无需修复）
ASSERT t.IsValidRB() == true
ASSERT t.Len() == 2
```

### TC-06  删除黑叶节点（触发修复）

```
构造一棵需要 delete fixup 的树，删除黑色叶节点
ASSERT t.IsValidRB() == true
```

### TC-07  删除根节点

```
t.Insert(42, _)
t.Delete(42)
ASSERT t.Len() == 0
ASSERT t.IsValidRB() == true
```

### TC-08  四种修复 case 覆盖

```
通过精心构造的键序列，分别触发 delete fixup 的：
- Case 1: 兄弟为红
- Case 2: 兄弟为黑，两子均黑
- Case 3: 兄弟为黑，近侧子红
- Case 4: 兄弟为黑，远侧子红
每种 case 执行后: ASSERT t.IsValidRB() == true
```

### TC-09  批量随机插入删除

```
循环 100 次：
    插入 1000 个随机键
    删除 500 个随机键
    ASSERT t.IsValidRB() == true
```

### TC-10  upsert 语义

```
t.Insert(7, "a")
t.Insert(7, "b")
ASSERT t.Len() == 1
v, ok := t.Search(7)
ASSERT ok && v == "b"
ASSERT t.IsValidRB() == true
```

### TC-11  min / max

```
t.Insert([8, 3, 12, 1, 9, 5]...)
ASSERT t.Min() == 1
ASSERT t.Max() == 12
对空树: ASSERT min/max 返回 not-found
```

### TC-12  中序遍历有序性

```
插入 500 个随机键
in_order 收集所有键，ASSERT 严格递增
```

### TC-13  黑高一致性（插入 / 删除各阶段）

```
每次 insert/delete 后调用 BlackHeight
验证同一层的所有路径黑高相等（通过 IsValidRB 内部检验）
```

### TC-14  大规模压力测试

```
插入 100_000 个随机键，删除全部
ASSERT t.Len() == 0
ASSERT t.IsValidRB() == true（空树也满足五条性质）
```

---

## 七、基准测试

### 基准一：插入吞吐量

```
n = 10_000 / 100_000 / 1_000_000
顺序插入 vs 随机插入
对比：RBTree vs AVL Tree vs BST vs std::map / Go map
```

### 基准二：search 吞吐量

```
预填充 n = 100_000
连续 search 1_000_000 次（命中率 50%）
对比：RBTree vs AVL vs std::map / Go map
```

### 基准三：delete 旋转次数统计

```
预填充 n = 100_000，删除全部键
记录每次 delete 触发的旋转次数
ASSERT 平均旋转次数 ≤ 3（理论上界）
对比 AVL delete：平均旋转次数 ≤ O(log n)
```

### 基准四：读多写少 vs 写多读少

```
操作比例 9:1 读写 → 对比 RBTree vs AVL
操作比例 1:9 读写 → 对比 RBTree vs AVL
验证 AVL 在读多场景更优（树更矮），RBTree 在写多场景更优（删除旋转更少）
```

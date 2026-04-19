# AVL Tree 设计文档

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

AVL 树是第一种自平衡二叉搜索树，由 Adelson-Velsky 和 Landis 于 1962 年提出。它在 BST 搜索性质之上附加了**平衡条件**：

> 任意节点的左右子树高度之差（**平衡因子 BF**）的绝对值不超过 1。
>
> `BF(n) = height(n.left) - height(n.right) ∈ {-1, 0, +1}`

通过在 `insert` / `delete` 后执行**旋转**恢复平衡，AVL 树在最坏情况下仍保证 O(log n) 的高度上界。

| 操作 | 平均 | 最坏 |
|------|------|------|
| `insert` | O(log n) | O(log n) |
| `search` | O(log n) | O(log n) |
| `delete` | O(log n) | O(log n) |
| `min` / `max` | O(log n) | O(log n) |
| 遍历 | O(n) | O(n) |

与普通 BST 相比，AVL 树用额外的旋转开销（≤ 2 次旋转/插入，≤ O(log n) 次旋转/删除）换取最坏情况下的性能保证。

---

## 二、关键语义决策

### 2.1 平衡因子存储方式

**决策：在每个节点中缓存子树高度（`int8_t height`），动态计算平衡因子。**

| 方案 | 优点 | 缺点 |
|------|------|------|
| 存储平衡因子 BF（-1/0/+1） | 节省 1 字节/节点 | 旋转时需要复杂的 BF 更新逻辑 |
| **存储高度（本实现）** | 旋转后直接重算高度，逻辑清晰 | 多占 1 字节，但旋转代码更简单 |

```
height(NULL) = 0
height(node) = 1 + max(height(node.left), height(node.right))
BF(node)     = height(node.left) - height(node.right)
```

### 2.2 四种旋转

AVL 树有四种旋转情形，所有旋转均为 O(1)：

```
失衡情形            触发条件           修复方式
-----------------------------------------------------------------------
LL（左-左）         BF(n) = +2，       右旋 n
                   BF(n.left) >= 0
LR（左-右）         BF(n) = +2，       左旋 n.left，再右旋 n
                   BF(n.left) < 0
RR（右-右）         BF(n) = -2，       左旋 n
                   BF(n.right) <= 0
RL（右-左）         BF(n) = -2，       右旋 n.right，再左旋 n
                   BF(n.right) > 0
```

**右旋示意（LL case）：**

```
    n              x
   / \            / \
  x   T3   →    T1   n
 / \                 / \
T1  T2              T2  T3
```

**左旋示意（RR case）：**

```
  n                y
 / \              / \
T1  y      →     n  T3
   / \          / \
  T2  T3       T1  T2
```

### 2.3 插入后的回溯路径

**决策：插入采用递归实现，每层返回时更新高度并检查平衡因子。**

递归自底向上天然符合 AVL 的回溯需求：

```
avl_insert(node, key, value):
    1. 按 BST 规则递归插入
    2. 更新 node.height
    3. 计算 BF(node)
    4. 如果 BF ∈ {-2, +2}，执行对应旋转
    5. 返回（可能已旋转的）子树根
```

插入后**最多执行 1 次旋转**（单旋或双旋），此后所有祖先节点的高度不再变化。

### 2.4 删除后的回溯路径

**决策：删除与 BST 三 case 策略相同，但删除后需沿路径向上逐层 rebalance。**

删除后**可能需要 O(log n) 次旋转**（每一层都可能失衡），与插入不同。回溯逻辑与插入相同，递归返回时检查并修复。

### 2.5 所有权模型

与 BST 文档一致：

| 语言 | 策略 |
|------|------|
| C | `malloc` + 递归 `free`（`avl_destroy`） |
| C++ | `std::unique_ptr<Node>` 级联析构 |
| Go | GC 自动回收 |

### 2.6 与 BST 文档的关系

AVL 树是 BST 的超集：所有 BST 接口（`search`、遍历、`min`/`max` 等）语义不变，仅在 `insert` / `delete` 后增加平衡修复步骤。接口形式与 BST 文档保持一致，键比较策略相同（C 注入函数指针，C++ 模板 `Compare`，Go `cmp.Ordered`）。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new(cmp_fn)` | 创建空 AVL 树 | O(1) |
| `destroy(t)` | 销毁所有节点 | O(n) |
| `clone(t)` | 深拷贝整棵树 | O(n) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `insert(key, value)` | 插入；键已存在则更新 value；自动 rebalance | O(log n) |
| `search(key)` | 查找键，返回 value 或 not-found | O(log n) |
| `delete(key)` | 删除节点；自动 rebalance | O(log n) |
| `contains(key)` | 是否包含键 | O(log n) |
| `min()` | 返回最小键 | O(log n) |
| `max()` | 返回最大键 | O(log n) |

### 3.3 遍历

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `in_order(fn)` | 中序遍历（升序） | O(n) |
| `pre_order(fn)` | 前序遍历 | O(n) |
| `post_order(fn)` | 后序遍历 | O(n) |
| `level_order(fn)` | 层序遍历（BFS） | O(n) |

### 3.4 统计与调试

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(t)` | 节点总数 | O(1) |
| `height(t)` | 树高（空树为 0） | O(1)（直接读根节点 height 字段） |
| `is_empty(t)` | 是否为空 | O(1) |
| `is_balanced(t)` | 验证全树平衡因子合法（测试辅助） | O(n) |
| `is_valid_bst(t)` | 验证 BST 搜索性质（测试辅助） | O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_NOT_FOUND` | `search` / `delete` 键不存在 | 返回错误码，树状态不变 |
| `ERR_ALLOC` | 节点内存分配失败 | 返回错误码，树状态不变 |
| `ERR_EMPTY` | 对空树调用 `min` / `max` | 返回错误码 |

### 4.2 不变式保证

每次 `insert` / `delete` 后，树必须同时满足：

```
1. BST 搜索性质：is_valid_bst(t) == true
2. AVL 平衡性质：is_balanced(t) == true
3. 高度上界：height(t) <= 1.44 * log2(len(t) + 2)
```

测试断言：

```
post-insert:  is_balanced(t) && is_valid_bst(t)
post-delete:  is_balanced(t) && is_valid_bst(t)
              len(t) == prev_len - 1  （若键存在）
```

---

## 五、语言实现

### 5.1 C

```c
typedef int  (*avl_cmp_fn)(const void *a, const void *b);
typedef void (*avl_visit_fn)(const void *key, void *value, void *user_data);

typedef enum {
    AVL_OK        = 0,
    AVL_NOT_FOUND = -1,
    AVL_ALLOC_ERR = -2,
    AVL_EMPTY     = -3,
} AVLError;

typedef struct AVLNode {
    void           *key;
    void           *value;
    int8_t          height;   /* 子树高度，空节点视为 0 */
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

typedef struct {
    AVLNode    *root;
    size_t      len;
    avl_cmp_fn  cmp;
} AVL;

/* 生命周期 */
AVL      *avl_new(avl_cmp_fn cmp);
void      avl_destroy(AVL *t);

/* 核心操作 */
AVLError  avl_insert(AVL *t, void *key, void *value);
AVLError  avl_search(const AVL *t, const void *key, void **value_out);
AVLError  avl_delete(AVL *t, const void *key);
int       avl_contains(const AVL *t, const void *key);

/* 极值 */
AVLError  avl_min(const AVL *t, void **key_out, void **value_out);
AVLError  avl_max(const AVL *t, void **key_out, void **value_out);

/* 遍历 */
void      avl_in_order(const AVL *t, avl_visit_fn fn, void *user_data);

/* 统计 */
size_t    avl_len(const AVL *t);
int       avl_height(const AVL *t);
int       avl_is_empty(const AVL *t);

/* 测试辅助 */
int       avl_is_balanced(const AVL *t);
int       avl_is_valid_bst(const AVL *t);
```

**旋转辅助函数（内部）：**

```c
/* 返回新子树根 */
static AVLNode *rotate_right(AVLNode *n);
static AVLNode *rotate_left(AVLNode *n);
static AVLNode *rebalance(AVLNode *n);          /* 检测并执行旋转 */
static int8_t   node_height(const AVLNode *n);  /* NULL 安全 */
static void     update_height(AVLNode *n);
```

### 5.2 C++

```cpp
namespace algo {

template <typename K, typename V, typename Compare = std::less<K>>
class AVL {
public:
    enum class Error { NotFound, AllocFail, Empty };

    AVL() = default;
    ~AVL() = default;
    AVL(const AVL& other);
    AVL(AVL&&) noexcept = default;
    AVL& operator=(AVL other) noexcept;

    /* 核心操作 */
    [[nodiscard]] std::expected<void, Error>
        insert(K key, V value);

    [[nodiscard]] std::expected<std::reference_wrapper<V>, Error>
        search(const K& key);

    [[nodiscard]] std::expected<std::reference_wrapper<const V>, Error>
        search(const K& key) const;

    [[nodiscard]] std::expected<void, Error>
        remove(const K& key);

    bool contains(const K& key) const;

    /* 极值 */
    [[nodiscard]] std::expected<std::pair<const K&, V&>, Error> min();
    [[nodiscard]] std::expected<std::pair<const K&, V&>, Error> max();

    /* 遍历 */
    template <std::invocable<const K&, V&> Fn>
    void in_order(Fn&& fn);

    template <std::invocable<const K&, const V&> Fn>
    void in_order(Fn&& fn) const;

    /* 统计 */
    [[nodiscard]] std::size_t size()   const noexcept { return len_; }
    [[nodiscard]] int         height() const noexcept;
    [[nodiscard]] bool        empty()  const noexcept { return root_ == nullptr; }

    /* 测试辅助 */
    bool is_balanced()  const;
    bool is_valid_bst() const;

    friend void swap(AVL& a, AVL& b) noexcept {
        using std::swap;
        swap(a.root_, b.root_);
        swap(a.len_,  b.len_);
    }

private:
    struct Node {
        K                    key;
        V                    value;
        int8_t               height{1};
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    std::unique_ptr<Node> root_;
    std::size_t           len_{0};
    [[no_unique_address]] Compare cmp_{};

    /* 内部旋转与平衡 */
    static int8_t              node_height(const std::unique_ptr<Node>& n) noexcept;
    static void                update_height(Node* n) noexcept;
    static int                 balance_factor(const Node* n) noexcept;
    static std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> n);
    static std::unique_ptr<Node> rotate_left(std::unique_ptr<Node>  n);
    static std::unique_ptr<Node> rebalance(std::unique_ptr<Node> n);

    std::unique_ptr<Node> insert_node(std::unique_ptr<Node> n,
                                      K key, V value, bool& updated);
    std::unique_ptr<Node> remove_node(std::unique_ptr<Node> n,
                                      const K& key, bool& removed);
    static const Node*    find_min(const Node* n) noexcept;
};

} // namespace algo
```

### 5.3 Go

```go
package linear

import "cmp"

// AVL 树，键类型需满足 cmp.Ordered 约束
type AVL[K cmp.Ordered, V any] struct {
    root *avlNode[K, V]
    len  int
}

type avlNode[K cmp.Ordered, V any] struct {
    key    K
    value  V
    height int8
    left   *avlNode[K, V]
    right  *avlNode[K, V]
}

// 核心操作
func (t *AVL[K, V]) Insert(key K, value V)
func (t *AVL[K, V]) Search(key K) (V, bool)
func (t *AVL[K, V]) Delete(key K) bool
func (t *AVL[K, V]) Contains(key K) bool

// 极值
func (t *AVL[K, V]) Min() (K, V, bool)
func (t *AVL[K, V]) Max() (K, V, bool)

// 遍历
func (t *AVL[K, V]) InOrder(fn func(K, V))
func (t *AVL[K, V]) InOrderChan() <-chan KV[K, V]  // KV 复用 BST 文档定义

// 统计
func (t *AVL[K, V]) Len()    int
func (t *AVL[K, V]) Height() int
func (t *AVL[K, V]) Empty()  bool

// 测试辅助
func (t *AVL[K, V]) IsBalanced()  bool
func (t *AVL[K, V]) IsValidBST()  bool

// 内部辅助（非导出）
func nodeHeight[K cmp.Ordered, V any](n *avlNode[K, V]) int8
func updateHeight[K cmp.Ordered, V any](n *avlNode[K, V])
func balanceFactor[K cmp.Ordered, V any](n *avlNode[K, V]) int
func rotateRight[K cmp.Ordered, V any](n *avlNode[K, V]) *avlNode[K, V]
func rotateLeft[K cmp.Ordered, V any](n *avlNode[K, V])  *avlNode[K, V]
func rebalance[K cmp.Ordered, V any](n *avlNode[K, V])   *avlNode[K, V]
```

---

## 六、功能测试

### TC-01  空树基本属性

```
avl := AVL{}
ASSERT avl.Len() == 0
ASSERT avl.Height() == 0
ASSERT avl.Empty() == true
ASSERT avl.IsBalanced() == true
```

### TC-02  单节点插入

```
avl.Insert(10, "ten")
ASSERT avl.Len() == 1
ASSERT avl.Height() == 1
ASSERT avl.IsBalanced() == true
ASSERT avl.IsValidBST() == true
```

### TC-03  LL 旋转（右旋）

```
// 顺序插入 30 → 20 → 10，触发 LL 失衡
avl.Insert(30, _); avl.Insert(20, _); avl.Insert(10, _)
ASSERT avl.Height() == 2   // 平衡后树高为 2，而非退化的 3
ASSERT avl.IsBalanced() == true
ASSERT avl.IsValidBST() == true
中序输出: [10, 20, 30]
```

### TC-04  RR 旋转（左旋）

```
// 顺序插入 10 → 20 → 30，触发 RR 失衡
avl.Insert(10, _); avl.Insert(20, _); avl.Insert(30, _)
ASSERT avl.Height() == 2
ASSERT avl.IsBalanced() == true
```

### TC-05  LR 旋转（左旋 + 右旋）

```
// 插入 30 → 10 → 20，触发 LR 失衡
avl.Insert(30, _); avl.Insert(10, _); avl.Insert(20, _)
ASSERT avl.Height() == 2
ASSERT avl.IsBalanced() == true
中序输出: [10, 20, 30]
```

### TC-06  RL 旋转（右旋 + 左旋）

```
// 插入 10 → 30 → 20，触发 RL 失衡
avl.Insert(10, _); avl.Insert(30, _); avl.Insert(20, _)
ASSERT avl.Height() == 2
ASSERT avl.IsBalanced() == true
```

### TC-07  大批量顺序插入后保持平衡

```
for i in [1..1000]:
    avl.Insert(i, i)
ASSERT avl.Len() == 1000
ASSERT avl.Height() <= ceil(1.44 * log2(1001))  // ≈ 14
ASSERT avl.IsBalanced() == true
ASSERT avl.IsValidBST() == true
```

### TC-08  随机插入后保持平衡

```
顺序随机打乱后插入 500 个键
ASSERT avl.IsBalanced() == true
ASSERT avl.IsValidBST() == true
```

### TC-09  删除叶节点后平衡

```
插入 [10, 5, 20, 3]，删除 3（叶节点）
ASSERT avl.IsBalanced() == true && avl.Len() == 3
```

### TC-10  删除单子节点后平衡

```
插入 [10, 5, 20, 4]，删除 5（单子节点）
ASSERT avl.IsBalanced() == true && avl.Len() == 3
```

### TC-11  删除双子节点后平衡（触发旋转）

```
构建：
       20
      /  \
    10    30
   /  \
  5   15
删除 10（双子节点，后继为 15）
ASSERT 中序输出: [5, 15, 20, 30]
ASSERT avl.IsBalanced() == true
```

### TC-12  批量删除后保持平衡

```
插入 1000 个随机键，然后依次删除所有偶数键（500 次）
每次删除后: ASSERT avl.IsBalanced() && avl.IsValidBST()
```

### TC-13  upsert 语义

```
avl.Insert(42, "first")
avl.Insert(42, "second")
ASSERT avl.Len() == 1
v, ok := avl.Search(42)
ASSERT ok && v == "second"
```

### TC-14  min / max / 边界

```
插入 [3, 1, 4, 1, 5, 9, 2, 6]（去重后 7 个）
ASSERT avl.Min() == 1
ASSERT avl.Max() == 9
对空树调用 Min() → ERR_EMPTY / (zero, false)
```

---

## 七、基准测试

### 基准一：顺序 vs 随机插入

```
n = 10_000 / 100_000 / 1_000_000
1. 顺序插入 [0..n-1]
2. 随机打乱后插入
对比：insert 吞吐量（ops/sec）/ 最终树高
```

普通 BST 顺序插入退化为链表（高度 = n），AVL 树两种模式高度均应在 1.44 * log2(n) 以内。

### 基准二：search 吞吐量

```
预填充 n = 100_000 个键
连续 search 1_000_000 次（随机键，命中率 50%）
对比：AVL 树 vs 标准库（std::map / Go map）
```

### 基准三：delete 开销

```
预填充 n = 100_000 个键
依次删除全部键（顺序 / 逆序 / 随机）
记录每次 delete 触发的旋转次数（平均值应 ≤ O(log n)）
```

### 基准四：内存开销

```
n = 1_000_000 个节点
每个 AVLNode 额外 1 字节 height vs 普通 BSTNode
实际内存对比（含 malloc 元数据）
```

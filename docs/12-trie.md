# Trie（前缀树）设计文档

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

Trie（发音同 "try"，又称前缀树或字典树）是一种**多叉树**结构，专为字符串集合的高效存储与检索设计。

核心特性：

- 根节点不存储字符；每个节点代表一个字符，从根到叶的路径对应一个字符串。
- 所有具有相同前缀的字符串共享前缀路径。
- 标记节点（`is_end`）区分完整单词与路径中间节点。

```
插入 ["app", "apple", "apply", "apt"] 后的树形结构：

root
└── a
    └── p
        ├── p (is_end: false)
        │   ├── l
        │   │   ├── e (is_end: true)  ← "apple"
        │   │   └── y (is_end: true)  ← "apply"
        │   └── (is_end: true)        ← "app"
        └── t (is_end: true)          ← "apt"
```

| 操作 | 复杂度（k = 键字符数，n = 节点数）|
|------|----------------------------------|
| `insert(key)` | O(k) |
| `search(key)` | O(k) |
| `starts_with(prefix)` | O(k) |
| `delete(key)` | O(k) |
| `all_with_prefix(prefix)` | O(k + 输出数量) |

Trie 不依赖键的比较，时间复杂度与键集合大小 n 无关，仅与键长 k 相关。

---

## 二、关键语义决策

### 2.1 子节点存储：数组 vs 哈希表

**决策：使用固定大小 256 元素数组（字节级 Trie），每个槽对应一个 ASCII/UTF-8 字节值。**

| 方案 | 查找复杂度 | 内存（每节点）| 适用场景 |
|------|-----------|-------------|---------|
| **256 元素数组** | O(1)（直接下标） | 256 × 8B = 2 KB | ASCII/字节流，内存充裕 |
| 哈希表 | O(1) 平均 | 动态，仅存实际子节点 | 稀疏字符集（Unicode 全集）|
| 有序数组 + 二分查找 | O(log 子节点数) | 紧凑 | 字符集中等，需有序遍历 |

本实现选择 256 元素指针数组，适合字节流 key（UTF-8 字符串、二进制前缀索引）。每节点 `children[256]` 中大量为 NULL，但查找无分支，缓存友好。

> 若键为 Unicode 码点（可达 1 114 112 种），应改用哈希表方案（超出本文档范围）。

### 2.2 键的语义：字节序列 vs 字符串

**决策：键类型为字节切片（`const uint8_t *` / `std::span<uint8_t>` / `[]byte`），同时提供字符串便利接口。**

这使 Trie 可用于任意二进制前缀索引（如 IP 地址前缀、文件路径前缀），不局限于文本。

### 2.3 节点值：纯集合 vs 键值映射

**决策：支持键值对映射（`insert(key, value)`），`is_end == true` 的节点存储关联值。**

纯集合语义（仅判断 key 是否存在）是键值映射的子集，通过将 value 设为 `void*` 或 `struct{}`（Go）统一支持。

### 2.4 `delete` 语义：标记删除 vs 物理删除

**决策：物理删除——删除节点后，若该节点无子节点且非端点，则递归删除，释放内存。**

| 方案 | 内存 | 实现复杂度 |
|------|------|-----------|
| 标记删除（仅清除 is_end） | 节点常驻内存，空间浪费 | 简单 |
| **物理删除（本实现）** | 无用节点立即释放 | 稍复杂（需后序检查并删除空路径） |

删除算法：

```
delete(node, key, depth):
    if depth == len(key):
        node.is_end = false
        node.value  = nil
        return node.children 全为 NULL  // 可以删除
    c = key[depth]
    should_delete = delete(node.children[c], key, depth+1)
    if should_delete:
        free(node.children[c])
        node.children[c] = NULL
    return !node.is_end && node.children 全为 NULL
```

### 2.5 Go 版本：并发安全变体

**决策：提供标准 `Trie`（非并发）和 `SyncTrie`（读写锁保护）两种实现。**

`SyncTrie` 使用 `sync.RWMutex`：

- `search` / `starts_with` / `all_with_prefix` → 读锁（并发读）
- `insert` / `delete` → 写锁（互斥写）

这是 Go 版本独有的功能（README 备注："含并发安全版（Go）"）。

### 2.6 `all_with_prefix` 返回语义

**决策：返回所有以给定前缀开头的键的有序切片（按字典序），而非惰性迭代器。**

对于小数据集，切片更简单。Go 版本额外提供 channel 变体 `AllWithPrefixChan`，支持惰性消费超大结果集。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new()` | 创建空 Trie | O(1) |
| `destroy(t)` | 释放所有节点 | O(n) |

### 3.2 核心操作

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `insert(key, value)` | 插入键值对；键已存在则更新 value | O(k) |
| `search(key)` | 精确查找，返回 value 或 not-found | O(k) |
| `contains(key)` | 是否包含完整键 | O(k) |
| `delete(key)` | 删除键，物理清除空路径 | O(k) |
| `starts_with(prefix)` | 是否存在以 prefix 开头的键 | O(k) |

### 3.3 前缀查询

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `all_with_prefix(prefix)` | 返回所有以 prefix 开头的键（字典序） | O(k + 输出) |
| `count_with_prefix(prefix)` | 统计以 prefix 开头的键数量 | O(k + 匹配数) |
| `longest_prefix(key)` | 返回 key 在 Trie 中最长的前缀匹配 | O(k) |

### 3.4 统计

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len(t)` | 完整键数量 | O(1)（缓存计数） |
| `is_empty(t)` | 是否为空 | O(1) |
| `node_count(t)` | 内部节点总数（内存压力指标）| O(n) |

---

## 四、异常处理

### 4.1 错误分类

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_NOT_FOUND` | `search` / `delete` 键不存在 | 返回错误码 |
| `ERR_ALLOC` | 节点内存分配失败 | 返回错误码，树状态不变（已插入部分路径不回滚，但 is_end 不置位）|
| `ERR_EMPTY_KEY` | 插入空键 | 可选：允许空键（is_end 置位于根）；或返回错误 |

> **空键决策**：本实现允许空键（`insert("", value)` 将根节点的 `is_end` 置位，类似空字符串词条）。

### 4.2 不变式保证

```
post-insert:  contains(key) == true
              len(t) 增加 1（若键不存在）或不变（upsert）
post-delete:  contains(key) == false
              所有无子且非端点的路径节点已被释放
```

---

## 五、语言实现

### 5.1 C

```c
#define TRIE_CHILDREN 256

typedef void (*trie_visit_fn)(const char *key, void *value, void *user_data);

typedef enum {
    TRIE_OK        = 0,
    TRIE_NOT_FOUND = -1,
    TRIE_ALLOC_ERR = -2,
} TrieError;

typedef struct TrieNode {
    struct TrieNode *children[TRIE_CHILDREN];
    void            *value;
    int              is_end;
} TrieNode;

typedef struct {
    TrieNode *root;
    size_t    len;
} Trie;

/* 生命周期 */
Trie      *trie_new(void);
void       trie_destroy(Trie *t);

/* 核心操作 */
TrieError  trie_insert(Trie *t, const char *key, void *value);
TrieError  trie_search(const Trie *t, const char *key, void **value_out);
int        trie_contains(const Trie *t, const char *key);
TrieError  trie_delete(Trie *t, const char *key);
int        trie_starts_with(const Trie *t, const char *prefix);

/* 前缀查询 */
/* 结果填入调用方分配的 char** keys 数组；返回实际写入数量 */
size_t     trie_all_with_prefix(const Trie *t, const char *prefix,
                                char **keys_out, size_t max_out);
size_t     trie_count_with_prefix(const Trie *t, const char *prefix);

/* 统计 */
size_t     trie_len(const Trie *t);
int        trie_is_empty(const Trie *t);

/* 内部辅助 */
static TrieNode *trie_node_new(void);
static int       trie_delete_rec(TrieNode *node, const char *key, size_t depth);
static void      trie_collect(TrieNode *node, char *buf, size_t depth,
                              char **out, size_t *count, size_t max);
```

### 5.2 C++

```cpp
namespace algo {

template <typename V = std::monostate>  /* 默认 monostate = 纯集合语义 */
class Trie {
public:
    enum class Error { NotFound, AllocFail };

    Trie() : root_(std::make_unique<Node>()), len_(0) {}

    /* 核心操作 */
    [[nodiscard]] std::expected<void, Error>
        insert(std::string_view key, V value = {});

    [[nodiscard]] std::expected<std::reference_wrapper<V>, Error>
        search(std::string_view key);

    [[nodiscard]] std::expected<std::reference_wrapper<const V>, Error>
        search(std::string_view key) const;

    bool contains(std::string_view key) const noexcept;

    [[nodiscard]] std::expected<void, Error>
        remove(std::string_view key);

    bool starts_with(std::string_view prefix) const noexcept;

    /* 前缀查询 */
    [[nodiscard]] std::vector<std::string>
        all_with_prefix(std::string_view prefix) const;

    [[nodiscard]] std::size_t
        count_with_prefix(std::string_view prefix) const noexcept;

    [[nodiscard]] std::string
        longest_prefix(std::string_view key) const;

    /* 统计 */
    [[nodiscard]] std::size_t size()     const noexcept { return len_; }
    [[nodiscard]] bool        empty()    const noexcept { return len_ == 0; }

private:
    static constexpr std::size_t ALPHA = 256;

    struct Node {
        std::array<std::unique_ptr<Node>, ALPHA> children{};
        std::optional<V>                          value;
        bool                                      is_end{false};
    };

    std::unique_ptr<Node> root_;
    std::size_t           len_{0};

    /* 返回 true 表示该节点可以被父节点释放 */
    bool remove_rec(Node* node, std::string_view key, std::size_t depth);
    void collect(const Node* node, std::string& buf,
                 std::vector<std::string>& out) const;
    const Node* find_prefix_node(std::string_view prefix) const noexcept;
};

} // namespace algo
```

### 5.3 Go（标准版 + 并发安全版）

```go
package linear

import "sync"

const trieAlpha = 256

// TrieNode 内部节点
type trieNode[V any] struct {
    children [trieAlpha]*trieNode[V]
    value    V
    isEnd    bool
}

// ---- 标准 Trie（非并发安全）----

type Trie[V any] struct {
    root *trieNode[V]
    len  int
}

func NewTrie[V any]() *Trie[V]

func (t *Trie[V]) Insert(key string, value V)
func (t *Trie[V]) Search(key string) (V, bool)
func (t *Trie[V]) Contains(key string) bool
func (t *Trie[V]) Delete(key string) bool
func (t *Trie[V]) StartsWith(prefix string) bool

func (t *Trie[V]) AllWithPrefix(prefix string) []string
func (t *Trie[V]) CountWithPrefix(prefix string) int
func (t *Trie[V]) LongestPrefix(key string) string

func (t *Trie[V]) Len()   int
func (t *Trie[V]) Empty() bool

// ---- SyncTrie（读写锁并发安全版）----

type SyncTrie[V any] struct {
    mu   sync.RWMutex
    trie Trie[V]
}

func NewSyncTrie[V any]() *SyncTrie[V]

// 写操作：互斥锁
func (t *SyncTrie[V]) Insert(key string, value V)
func (t *SyncTrie[V]) Delete(key string) bool

// 读操作：读锁（允许并发读）
func (t *SyncTrie[V]) Search(key string)     (V, bool)
func (t *SyncTrie[V]) Contains(key string)   bool
func (t *SyncTrie[V]) StartsWith(prefix string) bool
func (t *SyncTrie[V]) AllWithPrefix(prefix string) []string

// channel 变体（惰性枚举，适合大前缀结果集）
func (t *SyncTrie[V]) AllWithPrefixChan(prefix string) <-chan string

func (t *SyncTrie[V]) Len() int
```

---

## 六、功能测试

### TC-01  空 Trie 基本属性

```
t := NewTrie[string]()
ASSERT t.Len() == 0
ASSERT t.Empty() == true
ASSERT !t.Contains("hello")
ASSERT !t.StartsWith("")  // 空前缀，视实现而定
```

### TC-02  单词插入与精确查找

```
t.Insert("hello", "world")
ASSERT t.Contains("hello") == true
ASSERT t.Contains("hell")  == false
ASSERT t.Contains("helloo") == false
v, ok := t.Search("hello")
ASSERT ok && v == "world"
```

### TC-03  前缀共享（内存共享路径）

```
t.Insert("app", "application")
t.Insert("apple", "the fruit")
t.Insert("apply", "to apply")
ASSERT t.Len() == 3
ASSERT t.StartsWith("app") == true
ASSERT t.StartsWith("apt") == false
```

### TC-04  all_with_prefix

```
t.Insert([{"app", _}, {"apple", _}, {"apply", _}, {"apt", _}, {"banana", _}]...)
result := t.AllWithPrefix("app")
ASSERT result == ["app", "apple", "apply"]  // 字典序
result2 := t.AllWithPrefix("ba")
ASSERT result2 == ["banana"]
ASSERT len(t.AllWithPrefix("xyz")) == 0
```

### TC-05  delete（物理删除验证）

```
t.Insert("apple", _); t.Insert("app", _)
t.Delete("apple")
ASSERT !t.Contains("apple")
ASSERT t.Contains("app")  // "app" 节点不受影响
ASSERT t.Len() == 1
```

### TC-06  delete 末尾空路径清理

```
t.Insert("abc", _)
t.Delete("abc")
ASSERT t.Len() == 0
ASSERT t.Empty() == true
// "a", "b", "c" 路径节点应已被释放（valgrind/AddressSanitizer 无泄漏）
```

### TC-07  upsert 语义

```
t.Insert("key", "v1")
t.Insert("key", "v2")
ASSERT t.Len() == 1
v, _ := t.Search("key")
ASSERT v == "v2"
```

### TC-08  longest_prefix

```
t.Insert(["int", "interface", "internal"]...)
ASSERT t.LongestPrefix("interface{}")   == "interface"
ASSERT t.LongestPrefix("intern")        == "int"       // "intern" 不是完整 key
ASSERT t.LongestPrefix("xyz")           == ""
```

### TC-09  count_with_prefix

```
插入 ["go", "golang", "gone", "good", "bad"]
ASSERT t.CountWithPrefix("go")  == 3
ASSERT t.CountWithPrefix("goo") == 1
ASSERT t.CountWithPrefix("bad") == 1
ASSERT t.CountWithPrefix("b")   == 1
ASSERT t.CountWithPrefix("x")   == 0
```

### TC-10  空键

```
t.Insert("", "empty key value")
ASSERT t.Contains("") == true
v, ok := t.Search("")
ASSERT ok && v == "empty key value"
t.Delete("")
ASSERT !t.Contains("")
```

### TC-11  二进制键（非文本）

```
key := []byte{0x00, 0xFF, 0x80}
t.Insert(string(key), "binary")
ASSERT t.Contains(string(key)) == true
```

### TC-12  并发安全版（Go SyncTrie）

```
st := NewSyncTrie[int]()
// 100 goroutine 并发写
for i in [0..99]:
    go func(i int) { st.Insert(strconv.Itoa(i), i) }()
等待全部写完成
ASSERT st.Len() == 100

// 100 goroutine 并发读
for i in [0..99]:
    go func(i int) {
        v, ok := st.Search(strconv.Itoa(i))
        ASSERT ok && v == i
    }()
// 无数据竞争（运行 go test -race）
```

### TC-13  大词典插入（性能健康性）

```
加载 /usr/share/dict/words（约 235 000 个英文单词）
所有单词 Insert
ASSERT t.Len() == 单词总数
ASSERT t.Contains("hello") == true
ASSERT !t.Contains("xyznotaword")
```

### TC-14  AllWithPrefixChan（Go 惰性枚举）

```
插入大量以 "go" 开头的键
ch := st.AllWithPrefixChan("go")
count := 0
for key := range ch:
    count++
ASSERT count == t.CountWithPrefix("go")
```

---

## 七、基准测试

### 基准一：insert 吞吐量

```
词典：/usr/share/dict/words（~235 000 词）
对比：Trie vs std::unordered_map<string, V> / Go map[string]V
```

Trie 在前缀密集场景（大量共享前缀）下内存占用远低于哈希表，insert 速度也因路径共享而更快。

### 基准二：search 吞吐量

```
预填充 100 000 个键
连续 search 1 000 000 次（50% 命中率）
对比：Trie vs hash map
```

### 基准三：all_with_prefix 查询

```
预填充全量英文词典
对不同长度前缀（1 字符 / 3 字符 / 5 字符）执行 AllWithPrefix
测量：平均延迟 × 返回条目数
```

### 基准四：并发读吞吐量（Go SyncTrie）

```
16 goroutine 并发读（RLock 不互斥）
1 goroutine 并发写（WLock 互斥）
对比：SyncTrie vs mutex 全锁版本
验证 RWMutex 在读多写少场景的实际加速比
```

### 基准五：内存占用对比

```
n = 100 000 个随机字符串（平均长度 8 字节）
Trie 节点数（node_count）× 节点大小
vs hash map 内部存储 + string 拷贝
```

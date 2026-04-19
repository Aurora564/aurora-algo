# Dynamic String 设计文档

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

Dynamic String 是一个管理**可变长度 UTF-8 字节序列**的容器，在动态数组（Vec）的基础上增加字符串特有语义：

- **UTF-8 原生**：以 UTF-8 字节序列存储，所有以"字符"为单位的操作均以 Unicode 码点（code point）计数，而非字节数。
- **空终止可选**：内部存储不强制维护 `\0` 终止符，但提供 `as_cstr()` 接口按需生成（C 互操作场景）。
- **动态增长**：与 Vec 相同的扩容策略，默认 ×2。
- **值语义**：字符串拥有自身的缓冲区，赋值/拷贝创建独立副本。

与 `Vec<u8>` 的核心区别：字符串在 `Vec<u8>` 基础上额外保证内容始终是合法的 UTF-8 序列；所有修改接口在修改前验证输入的 UTF-8 合法性。

---

## 二、关键语义决策

### 2.1 字节索引 vs 码点索引

字符串操作的核心歧义：`get(i)` 的 `i` 是**字节偏移**还是**码点索引**？

| 方案 | 优点 | 缺点 |
|------|------|------|
| 字节偏移 | O(1) 访问 | 多字节字符易被截断，破坏 UTF-8 |
| 码点索引 | 直观，安全 | O(n) 访问（需逐字节扫描）|

**决策：主接口使用码点索引；提供 `byte_offset(char_i)` 辅助函数将码点索引转换为字节偏移。**

对于性能敏感场景，调用者可先用 `byte_offset` 缓存偏移量，再使用字节偏移直接操作缓冲区。

### 2.2 字符串切片的生命周期

`slice(start, end)` 返回对内部缓冲区的**借用视图**（只读），语义与 Vec 的 `AsSlice` 相同：

> ⚠️ 持有切片期间，不得对原字符串执行任何可能触发重新分配的操作。

此外，切片的字节边界必须落在合法的 UTF-8 码点边界上；否则属于编程错误（Debug: assert，Release: UB）。

### 2.3 Small String Optimization（SSO）分析

SSO 是 C++ `std::string` 的重要优化：短字符串（通常 ≤ 15 字节）直接存储在结构体内部，不进行堆分配。

**本实现的 SSO 决策：C++ 版本实现 SSO；C 和 Go 版本不实现。**

| 语言 | SSO 决策 | 理由 |
|------|---------|------|
| C | 不实现 | `void *` 泛型结构难以内嵌固定大小 union，复杂度远超收益 |
| C++ | 实现（内联存储 15 字节）| 语言特性天然支持，`union` + 标志位实现简洁 |
| Go | 不实现 | GC 管理内存，短字符串分配代价低；Go 的逃逸分析会将小对象留在栈上 |

SSO 结构布局（C++，64 位系统）：

```
struct StrSSO {
    union {
        struct {                  // 堆模式（len > 15）
            char   *data;         // 8 bytes
            size_t  len;          // 8 bytes
            size_t  cap;          // 8 bytes
        } heap;
        struct {                  // SSO 模式（len <= 15）
            char    buf[15];      // 15 bytes
            uint8_t len;          // 1 byte（最高位为 0 表示 SSO 模式）
        } sso;
    };
};  // sizeof == 24 bytes（与 std::string 相同）
```

SSO 标志位：通过 `sso.len` 的最高位区分模式（0 = SSO，1 = 堆）。当字符串从 SSO 模式增长超过 15 字节时，自动迁移到堆模式。

### 2.4 编码验证策略

**决策：在边界处验证，内部操作信任已验证的数据。**

- `append(str)`、`insert(i, str)` 等接口：验证输入字节序列是否为合法 UTF-8，非法则返回 `ERR_INVALID_UTF8`。
- 字节级操作（`append_bytes`、`set_byte`）：不验证，调用者保证 UTF-8 合法性，属于 unsafe 接口。
- 内部存储始终假定为合法 UTF-8，无需重复验证。

### 2.5 字符串比较与哈希

字符串比较提供两种语义：
- **字节序比较**（`cmp_bytes`）：按字节值比较，O(n)，与 `memcmp` 等价。
- **码点顺序比较**（`cmp_chars`）：按 Unicode 码点值比较（对 UTF-8 而言，字节序与码点序一致，因此与字节序比较结果相同，但明确传达语义意图）。
- **不实现**语言级别的 locale 感知排序（如 `strcoll`），该功能属于国际化库范畴。

哈希函数：提供 `hash(str)` 接口（FNV-1a 或 xxHash），用于哈希表中以字符串为键的场景。

---

## 三、接口设计

### 3.1 生命周期

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `new()` | 创建空字符串 | O(1) |
| `from_cstr(s)` | 从 C 字符串创建（复制，验证 UTF-8）| O(n) |
| `from_bytes(bytes, len)` | 从字节序列创建（复制，验证 UTF-8）| O(n) |
| `with_cap(n)` | 预分配 n 字节容量 | O(1) |
| `destroy(s)` | 销毁（C 语言需显式调用）| O(1) |
| `move(dst, src)` | 转移所有权 | O(1) |
| `clone(dst, src)` | 深拷贝 | O(n) |

### 3.2 查询

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `len_bytes(s)` | 字节长度 | O(1) |
| `len_chars(s)` | 码点数量（需遍历）| O(n) |
| `cap(s)` | 缓冲区字节容量 | O(1) |
| `is_empty(s)` | 是否为空 | O(1) |
| `as_bytes(s)` | 返回只读字节切片 | O(1) |
| `as_cstr(s)` | 返回 `\0` 终止的 C 字符串（按需追加 `\0`）| O(1) 均摊 |
| `get_char(s, i)` | 返回第 i 个码点（码点索引，遍历）| O(n) |
| `byte_offset(s, char_i)` | 第 char_i 个码点的字节偏移 | O(n) |
| `slice(s, byte_start, byte_end)` | 返回字节范围的借用视图 | O(1) |

### 3.3 修改

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `append_str(s, other)` | 追加另一字符串 | 均摊 O(n) |
| `append_cstr(s, cstr)` | 追加 C 字符串 | 均摊 O(n) |
| `append_char(s, codepoint)` | 追加单个 Unicode 码点（编码为 UTF-8）| O(1) |
| `insert_str(s, byte_pos, other)` | 在字节偏移处插入（需落在码点边界）| O(n) |
| `remove_range(s, byte_start, byte_end)` | 移除字节范围（需落在码点边界）| O(n) |
| `clear(s)` | 清空（len 置 0，容量不变）| O(1) |
| `reserve(s, n)` | 确保至少 n 字节的额外容量 | O(n) |
| `shrink_to_fit(s)` | 收缩容量至 len | O(n) |
| `to_uppercase(s)` | 原地转大写（ASCII 范围）| O(n) |
| `to_lowercase(s)` | 原地转小写（ASCII 范围）| O(n) |

### 3.4 搜索与分割

| 操作 | 语义 | 复杂度 |
|------|------|--------|
| `find(s, pattern)` | 返回第一次出现的字节偏移 | O(n·m)，可优化为 KMP |
| `contains(s, pattern)` | 是否包含子串 | O(n·m) |
| `starts_with(s, prefix)` | 前缀匹配 | O(m) |
| `ends_with(s, suffix)` | 后缀匹配 | O(m) |
| `split(s, delim)` | 按分隔符分割，返回子串视图数组 | O(n) |
| `trim(s)` | 移除首尾 ASCII 空白 | O(n) |

---

## 四、异常处理

| 错误类型 | 触发条件 | 处理方式 |
|---------|---------|---------|
| `ERR_INVALID_UTF8` | 输入字节序列不是合法 UTF-8 | 返回错误码 |
| `ERR_ALLOC` | 内存分配失败 | 返回错误码，字符串状态回滚 |
| `ERR_OUT_OF_BOUNDS` | 码点索引超出范围 | 返回错误码 |
| `ERR_BAD_BOUNDARY` | 切片/插入边界不在码点边界上 | Debug: assert；Release: ERR_BAD_BOUNDARY |

`as_cstr()` 的空终止符维护：字符串内部记录"是否已追加 `\0`"的标志位，避免每次调用都重写。任何修改操作会清除此标志。

---

## 五、语言实现

### 5.1 C

```c
typedef struct {
    char   *data;       // UTF-8 字节，不含 \0
    size_t  len;        // 字节长度
    size_t  cap;        // 缓冲区容量（data 实际分配 cap+1 字节以容纳 \0）
    int     cstr_valid; // as_cstr() 的 \0 是否已追加
} Str;

typedef enum {
    STR_OK = 0, STR_ERR_INVALID_UTF8, STR_ERR_ALLOC,
    STR_ERR_OOB, STR_ERR_BAD_BOUNDARY
} StrError;

/* 生命周期 */
StrError str_new(Str *s);
StrError str_from_cstr(Str *s, const char *cstr);
StrError str_from_bytes(Str *s, const char *bytes, size_t len);
StrError str_with_cap(Str *s, size_t cap);
void     str_destroy(Str *s);
void     str_move(Str *dst, Str *src);
StrError str_clone(Str *dst, const Str *src);

/* 查询 */
size_t      str_len_bytes(const Str *s);
size_t      str_len_chars(const Str *s);       // O(n) 遍历
size_t      str_cap(const Str *s);
int         str_is_empty(const Str *s);
const char* str_as_bytes(const Str *s);
const char* str_as_cstr(Str *s);               // 按需追加 \0
StrError    str_get_char(const Str *s, size_t char_i, uint32_t *out_codepoint);
StrError    str_byte_offset(const Str *s, size_t char_i, size_t *out_offset);
const char* str_slice(const Str *s, size_t byte_start, size_t byte_end);

/* 修改 */
StrError str_append_str(Str *s, const Str *other);
StrError str_append_cstr(Str *s, const char *cstr);
StrError str_append_char(Str *s, uint32_t codepoint);
StrError str_insert_str(Str *s, size_t byte_pos, const Str *other);
StrError str_remove_range(Str *s, size_t byte_start, size_t byte_end);
void     str_clear(Str *s);
StrError str_reserve(Str *s, size_t additional);
StrError str_shrink_to_fit(Str *s);
void     str_to_uppercase(Str *s);
void     str_to_lowercase(Str *s);

/* 搜索 */
int      str_find(const Str *s, const char *pattern, size_t *out_offset);
int      str_contains(const Str *s, const char *pattern);
int      str_starts_with(const Str *s, const char *prefix);
int      str_ends_with(const Str *s, const char *suffix);
void     str_trim(Str *s);

/* 比较与哈希 */
int      str_cmp(const Str *a, const Str *b);   // 字节序，返回 -1/0/1
int      str_eq(const Str *a, const Str *b);
uint64_t str_hash(const Str *s);                // FNV-1a
```

> **C 实现注意**：分配 `cap + 1` 字节，始终保留最后一个字节给 `\0`，但 `len` 不计入此字节。`cstr_valid` 标志避免 `as_cstr` 重复写入。UTF-8 验证函数独立实现，遵循 WHATWG Encoding 规范的字节序列规则。

### 5.2 C++（含 SSO）

```cpp
class Str {
public:
    Str() noexcept = default;
    explicit Str(const char* cstr);           // 验证 UTF-8，抛出或返回 expected
    explicit Str(std::string_view sv);
    Str(const Str&);
    Str(Str&&) noexcept;
    Str& operator=(Str) noexcept;
    ~Str();

    // 查询
    size_t      len_bytes() const noexcept;
    size_t      len_chars() const;            // O(n)
    size_t      cap()       const noexcept;
    bool        is_empty()  const noexcept;
    std::string_view as_sv() const noexcept;  // 零拷贝视图
    const char* as_cstr();                    // 按需追加 \0，使 SSO buf 也 \0 终止
    std::expected<uint32_t, StrError> get_char(size_t char_i) const;
    std::string_view slice(size_t byte_start, size_t byte_end) const;

    // 修改
    StrError append(std::string_view sv);
    StrError append_char(uint32_t codepoint);
    StrError insert(size_t byte_pos, std::string_view sv);
    StrError remove_range(size_t byte_start, size_t byte_end);
    void     clear() noexcept;
    StrError reserve(size_t additional);
    void     shrink_to_fit();
    void     to_uppercase();
    void     to_lowercase();

    // 搜索
    std::optional<size_t> find(std::string_view pattern) const;
    bool   contains(std::string_view pattern)   const;
    bool   starts_with(std::string_view prefix) const;
    bool   ends_with(std::string_view suffix)   const;
    Str    trim() const;

    // 比较
    std::strong_ordering operator<=>(const Str&) const noexcept;
    bool operator==(const Str&) const noexcept;

    // 哈希支持（std::hash 特化）
    size_t hash() const noexcept;

private:
    // SSO 布局（24 bytes total，同 std::string）
    static constexpr size_t SSO_CAP = 15;
    union {
        struct { char *ptr; size_t len; size_t cap; } heap_;
        struct { char buf[SSO_CAP]; uint8_t len; }    sso_;
    };

    bool   is_sso()  const noexcept { return !(sso_.len & 0x80); }
    char*  data_ptr()      noexcept { return is_sso() ? sso_.buf : heap_.ptr; }
    size_t data_len() const noexcept;
    void   migrate_to_heap(size_t new_cap);
};

// std::hash 特化，使 Str 可用于 std::unordered_map
template<> struct std::hash<Str> {
    size_t operator()(const Str& s) const noexcept { return s.hash(); }
};
```

> **C++ SSO 实现注意**：`sso_.len` 最高位为 0 表示 SSO 模式；最高位为 1 表示堆模式（此时 `heap_.len` 存储真实长度，通过 `& 0x7F...` 掩码读取，或直接用 `heap_` 字段）。`as_cstr()` 在 SSO 模式下直接写 `sso_.buf[sso_.len] = '\0'`（buf 有 15+1=16 字节，最后一字节恰好可放 `\0`）。

### 5.3 Go

```go
// Go 的 string 是不可变的，Str 提供可变版本
type Str struct {
    data []byte   // UTF-8 字节，不包含 \0
}

type StrError int
const (
    ErrInvalidUTF8 StrError = iota + 1
    ErrOutOfBounds
    ErrBadBoundary
)

func NewStr() *Str
func StrFromString(s string) (*Str, error)  // 验证 UTF-8（Go string 已保证，此处为对称接口）
func StrFromBytes(b []byte) (*Str, error)   // 验证 UTF-8

func (s *Str) LenBytes() int
func (s *Str) LenChars() int               // O(n)，utf8.RuneCountInString
func (s *Str) Cap() int
func (s *Str) IsEmpty() bool
func (s *Str) AsString() string            // 零拷贝（Go string header 共享底层数组）
func (s *Str) AsBytes() []byte             // 借用视图
func (s *Str) GetChar(charI int) (rune, error)
func (s *Str) ByteOffset(charI int) (int, error)
func (s *Str) Slice(byteStart, byteEnd int) (string, error)

func (s *Str) Append(other string) error
func (s *Str) AppendChar(r rune)
func (s *Str) Insert(bytePos int, other string) error
func (s *Str) RemoveRange(byteStart, byteEnd int) error
func (s *Str) Clear()
func (s *Str) Reserve(additional int)
func (s *Str) ShrinkToFit()
func (s *Str) ToUpper()
func (s *Str) ToLower()

func (s *Str) Find(pattern string) (int, bool)
func (s *Str) Contains(pattern string) bool
func (s *Str) StartsWith(prefix string) bool
func (s *Str) EndsWith(suffix string) bool
func (s *Str) Trim() *Str
func (s *Str) Split(delim string) []string

func (s *Str) Equal(other *Str) bool
func (s *Str) Compare(other *Str) int    // -1/0/1
func (s *Str) Hash() uint64              // FNV-1a
```

> **Go 实现注意**：`AsString()` 通过 `unsafe.String` 或类型转换零拷贝返回 `string`（共享底层 `[]byte`）——持有返回值期间不应修改 `Str`。`AppendChar` 使用 `utf8.AppendRune` 编码码点。Go 的 `strings` 标准库已实现大部分搜索功能，`Str` 可直接委托。

---

## 六、功能测试

### 6.1 测试分类

| 类别 | 覆盖内容 |
|------|---------|
| 正常路径 | 基础创建、追加、查询 |
| UTF-8 验证 | 合法/非法多字节序列，截断序列 |
| 码点边界 | get_char、slice 在多字节字符处的正确性 |
| SSO 迁移（C++）| 字符串从 SSO 增长到堆模式后数据正确 |
| 修改操作 | insert、remove_range 在多字节字符边界处正确 |
| 搜索 | find 跨边界模式，contains/starts_with/ends_with |
| 比较与哈希 | 等价字符串哈希值相同，比较结果正确 |
| 内存安全 | Valgrind/ASan 验证无泄漏（尤其 SSO 迁移路径）|

### 6.2 关键测试用例

```
TC-01  from_cstr("hello") len_bytes=5, len_chars=5
TC-02  from_cstr("你好") len_bytes=6, len_chars=2
TC-03  非法 UTF-8 字节序列 → ERR_INVALID_UTF8
TC-04  get_char(1) on "你好" 返回 '好'（U+597D）
TC-05  slice(0, 3) on "你好" 返回 "你"（3 字节）
TC-06  slice(0, 2) on "你好" → ERR_BAD_BOUNDARY（截断多字节字符）
TC-07  C++：append 使 SSO 字符串超过 15 字节，迁移到堆后数据完整
TC-08  C++：SSO 字符串 as_cstr() 正确追加 \0
TC-09  append_char(U+1F600) 追加 4 字节 emoji，len_bytes += 4，len_chars += 1
TC-10  trim("  hello  ") == "hello"
TC-11  find("abcabc", "bc") 返回 1（字节偏移）
TC-12  hash(s1) == hash(s2) 当 s1 == s2（相同内容不同实例）
TC-13  扩容后 as_cstr() 返回正确的 \0 终止字符串
TC-14  clear() 后 len_bytes=0，再 append 正常工作
```

---

## 七、基准测试

### 7.1 测试场景

| 场景 | 目的 |
|------|------|
| append N 个短字符串（ASCII）| 均摊 append 吞吐量 |
| append N 个短字符串（3 字节 CJK 字符）| 多字节字符的 UTF-8 编码代价 |
| C++ SSO：append 15 字节内 vs 超出 | 量化 SSO 迁移代价 |
| len_chars（N 字节字符串）| O(n) 遍历的实际代价 |
| find（朴素 vs KMP）| 模式匹配算法对比（KMP 留待字符串算法章节）|
| 哈希 N 个字符串 | FNV-1a 吞吐量 |

### 7.2 基准工具与参考基线

| 语言 | 工具 | 对比基线 |
|------|------|---------|
| C | `clock_gettime` | `strcat` + `realloc` 朴素实现 |
| C++ | Google Benchmark | `std::string`（含 SSO）|
| Go | `testing.B` | `strings.Builder` / 原生 `string` 拼接 |

#pragma once
/**
 * Str.hpp — UTF-8 可变字符串（C++23）
 *
 * 以堆上 char 缓冲区存储 UTF-8 字节序列。
 * 所有修改接口在系统边界处验证输入的 UTF-8 合法性。
 * 错误以 std::expected<T, StrError> 形式返回，不抛出异常。
 *
 * 两种构造方式：
 *   Str empty;                            // 空字符串
 *   auto r = Str::from("hello 世界");     // 验证 UTF-8，返回 expected
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <new>
#include <string_view>
#include <utility>

namespace algo {

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

enum class StrError {
    InvalidUtf8,  /* 非法 UTF-8 字节序列       */
    OutOfBound,   /* 码点索引或字节偏移超出范围  */
    BadBoundary,  /* 字节位置未落在码点边界上    */
    Alloc,        /* 内存分配失败              */
};

/* ================================================================== */
/*  Str                                                                */
/* ================================================================== */

class Str {
public:
    /* ---- 构造 / 析构 -------------------------------------------- */

    Str() = default;

    /** 工厂：从 string_view 构造，验证 UTF-8 */
    [[nodiscard]] static std::expected<Str, StrError>
    from(std::string_view sv)
    {
        if (!utf8_valid(sv.data(), sv.size()))
            return std::unexpected(StrError::InvalidUtf8);
        Str s;
        if (!sv.empty()) {
            if (auto e = s.grow_to(sv.size()); !e) return std::unexpected(e.error());
            std::memcpy(s.data_, sv.data(), sv.size());
            s.len_ = sv.size();
        }
        return s;
    }

    /* 深拷贝 */
    Str(const Str& other)
        : len_(other.len_), cap_(other.len_)
    {
        if (len_ > 0) {
            data_ = static_cast<char*>(::operator new(cap_));
            std::memcpy(data_, other.data_, len_);
        }
    }

    /* 移动构造 */
    Str(Str&& other) noexcept
        : data_(other.data_), len_(other.len_), cap_(other.cap_)
    {
        other.data_ = nullptr;
        other.len_  = 0;
        other.cap_  = 0;
    }

    /* copy-and-swap 统一赋值 */
    Str& operator=(Str other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    ~Str()
    {
        ::operator delete(data_);
        data_ = nullptr;
        len_  = 0;
        cap_  = 0;
    }

    /* ---- 查询 -------------------------------------------------- */

    [[nodiscard]] size_t len_bytes() const noexcept { return len_; }
    [[nodiscard]] size_t capacity()  const noexcept { return cap_; }
    [[nodiscard]] bool   is_empty()  const noexcept { return len_ == 0; }

    [[nodiscard]] std::string_view as_string_view() const noexcept
    {
        return {data_, len_};
    }

    /** Unicode 码点数量；O(n) */
    [[nodiscard]] size_t len_chars() const noexcept
    {
        size_t count = 0;
        const char* p   = data_;
        const char* end = data_ + len_;
        while (p < end) {
            int n = utf8_seq_len(static_cast<unsigned char>(*p));
            p += (n > 0 ? n : 1);
            ++count;
        }
        return count;
    }

    /* ---- 访问 -------------------------------------------------- */

    /** 返回第 char_i 个 Unicode 码点；O(n) */
    [[nodiscard]] std::expected<uint32_t, StrError>
    get_char(size_t char_i) const
    {
        size_t i = 0;
        const char* p   = data_;
        const char* end = data_ + len_;
        while (p < end) {
            int n = utf8_seq_len(static_cast<unsigned char>(*p));
            if (n <= 0) n = 1;
            if (i == char_i) return utf8_decode(p);
            p += n;
            ++i;
        }
        return std::unexpected(StrError::OutOfBound);
    }

    /** 第 char_i 个码点的字节偏移（char_i == len_chars() 时返回 len_bytes()） */
    [[nodiscard]] std::expected<size_t, StrError>
    byte_offset(size_t char_i) const
    {
        size_t i = 0;
        const char* p   = data_;
        const char* end = data_ + len_;
        while (p < end) {
            if (i == char_i) return static_cast<size_t>(p - data_);
            int n = utf8_seq_len(static_cast<unsigned char>(*p));
            if (n <= 0) n = 1;
            p += n;
            ++i;
        }
        if (i == char_i) return len_;   /* 末尾哨兵 */
        return std::unexpected(StrError::OutOfBound);
    }

    /** 字节范围 [byte_start, byte_end) 的视图；O(1) */
    [[nodiscard]] std::expected<std::string_view, StrError>
    slice(size_t byte_start, size_t byte_end) const
    {
        if (byte_start > len_ || byte_end > len_ || byte_start > byte_end)
            return std::unexpected(StrError::OutOfBound);
        if (byte_start > 0 && !is_rune_start(data_[byte_start]))
            return std::unexpected(StrError::BadBoundary);
        if (byte_end < len_ && !is_rune_start(data_[byte_end]))
            return std::unexpected(StrError::BadBoundary);
        return std::string_view{data_ + byte_start, byte_end - byte_start};
    }

    /* ---- 修改 -------------------------------------------------- */

    /** 追加 string_view（验证 UTF-8） */
    [[nodiscard]] std::expected<void, StrError>
    append(std::string_view sv)
    {
        if (!utf8_valid(sv.data(), sv.size()))
            return std::unexpected(StrError::InvalidUtf8);
        if (auto e = grow_to(len_ + sv.size()); !e) return e;
        std::memcpy(data_ + len_, sv.data(), sv.size());
        len_ += sv.size();
        return {};
    }

    /** 追加 Unicode 码点（编码为 UTF-8） */
    void append_char(uint32_t cp)
    {
        char buf[4];
        int n = utf8_encode(cp, buf);
        (void)grow_to(len_ + static_cast<size_t>(n));
        std::memcpy(data_ + len_, buf, static_cast<size_t>(n));
        len_ += static_cast<size_t>(n);
    }

    /** 在字节偏移 byte_pos 处插入（验证 UTF-8，验证边界） */
    [[nodiscard]] std::expected<void, StrError>
    insert(size_t byte_pos, std::string_view sv)
    {
        if (byte_pos > len_) return std::unexpected(StrError::OutOfBound);
        if (byte_pos > 0 && byte_pos < len_ && !is_rune_start(data_[byte_pos]))
            return std::unexpected(StrError::BadBoundary);
        if (!utf8_valid(sv.data(), sv.size()))
            return std::unexpected(StrError::InvalidUtf8);
        size_t new_len = len_ + sv.size();
        if (auto e = grow_to(new_len); !e) return e;
        std::memmove(data_ + byte_pos + sv.size(),
                     data_ + byte_pos,
                     len_ - byte_pos);
        std::memcpy(data_ + byte_pos, sv.data(), sv.size());
        len_ = new_len;
        return {};
    }

    /** 移除字节范围 [byte_start, byte_end) */
    [[nodiscard]] std::expected<void, StrError>
    remove_range(size_t byte_start, size_t byte_end)
    {
        if (byte_start > len_ || byte_end > len_ || byte_start > byte_end)
            return std::unexpected(StrError::OutOfBound);
        if (byte_start > 0 && !is_rune_start(data_[byte_start]))
            return std::unexpected(StrError::BadBoundary);
        if (byte_end < len_ && !is_rune_start(data_[byte_end]))
            return std::unexpected(StrError::BadBoundary);
        std::memmove(data_ + byte_start, data_ + byte_end, len_ - byte_end);
        len_ -= (byte_end - byte_start);
        return {};
    }

    /** 清空（len 归零，容量不变） */
    void clear() noexcept { len_ = 0; }

    /** 预留额外 additional 字节容量 */
    [[nodiscard]] std::expected<void, StrError>
    reserve(size_t additional)
    {
        return grow_to(len_ + additional);
    }

    /** 将容量收缩至 len */
    void shrink_to_fit()
    {
        if (cap_ <= len_) return;
        if (len_ == 0) {
            ::operator delete(data_);
            data_ = nullptr;
            cap_  = 0;
            return;
        }
        char* nd = static_cast<char*>(::operator new(len_, std::nothrow));
        if (!nd) return;
        std::memcpy(nd, data_, len_);
        ::operator delete(data_);
        data_ = nd;
        cap_  = len_;
    }

    /** 原地 ASCII 大写（非 ASCII 字节不变） */
    void to_upper() noexcept
    {
        for (size_t i = 0; i < len_; ++i)
            if (data_[i] >= 'a' && data_[i] <= 'z')
                data_[i] = static_cast<char>(data_[i] ^ 0x20);
    }

    /** 原地 ASCII 小写 */
    void to_lower() noexcept
    {
        for (size_t i = 0; i < len_; ++i)
            if (data_[i] >= 'A' && data_[i] <= 'Z')
                data_[i] = static_cast<char>(data_[i] | 0x20);
    }

    /* ---- 搜索 -------------------------------------------------- */

    /** 返回 pattern 第一次出现的字节偏移；second == false 表示未找到 */
    [[nodiscard]] std::pair<size_t, bool>
    find(std::string_view pattern) const noexcept
    {
        if (pattern.empty()) return {0, true};
        if (pattern.size() > len_) return {0, false};
        const char* it = std::search(data_, data_ + len_,
                                     pattern.data(),
                                     pattern.data() + pattern.size());
        if (it == data_ + len_) return {0, false};
        return {static_cast<size_t>(it - data_), true};
    }

    [[nodiscard]] bool contains(std::string_view pattern) const noexcept
    {
        return find(pattern).second;
    }

    [[nodiscard]] bool starts_with(std::string_view prefix) const noexcept
    {
        if (prefix.size() > len_) return false;
        return std::memcmp(data_, prefix.data(), prefix.size()) == 0;
    }

    [[nodiscard]] bool ends_with(std::string_view suffix) const noexcept
    {
        if (suffix.size() > len_) return false;
        return std::memcmp(data_ + len_ - suffix.size(),
                           suffix.data(), suffix.size()) == 0;
    }

    /** 返回首尾 ASCII 空白已去除的新 Str */
    [[nodiscard]] Str trim() const
    {
        size_t lo = 0;
        while (lo < len_ && is_ascii_space(data_[lo])) ++lo;
        size_t hi = len_;
        while (hi > lo && is_ascii_space(data_[hi - 1])) --hi;
        Str out;
        if (hi > lo) {
            (void)out.grow_to(hi - lo);
            std::memcpy(out.data_, data_ + lo, hi - lo);
            out.len_ = hi - lo;
        }
        return out;
    }

    /* ---- 比较与哈希 -------------------------------------------- */

    [[nodiscard]] bool operator==(const Str& other) const noexcept
    {
        return len_ == other.len_ &&
               (len_ == 0 || std::memcmp(data_, other.data_, len_) == 0);
    }

    /** 按字节序比较；返回负、零或正（类似 memcmp / strcmp） */
    [[nodiscard]] int compare(const Str& other) const noexcept
    {
        size_t min_len = len_ < other.len_ ? len_ : other.len_;
        int r = (min_len == 0) ? 0 : std::memcmp(data_, other.data_, min_len);
        if (r != 0) return r;
        if (len_ < other.len_) return -1;
        if (len_ > other.len_) return  1;
        return 0;
    }

    /** FNV-1a 64-bit 哈希 */
    [[nodiscard]] uint64_t hash() const noexcept
    {
        static constexpr uint64_t kOffset = 14695981039346656037ULL;
        static constexpr uint64_t kPrime  = 1099511628211ULL;
        uint64_t h = kOffset;
        for (size_t i = 0; i < len_; ++i) {
            h ^= static_cast<uint64_t>(static_cast<unsigned char>(data_[i]));
            h *= kPrime;
        }
        return h;
    }

    /* ---- ADL swap --------------------------------------------- */
    friend void swap(Str& a, Str& b) noexcept
    {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.len_,  b.len_);
        swap(a.cap_,  b.cap_);
    }

private:
    char*  data_ = nullptr;
    size_t len_  = 0;
    size_t cap_  = 0;

    /* ---- UTF-8 内部工具 --------------------------------------- */

    /** 以 c 开头的 UTF-8 序列字节数；续字节或非法首字节返回 -1 */
    static int utf8_seq_len(unsigned char c) noexcept
    {
        if (c < 0x80) return 1;
        if (c < 0xC0) return -1;  /* continuation byte */
        if (c < 0xE0) return 2;
        if (c < 0xF0) return 3;
        if (c < 0xF8) return 4;
        return -1;
    }

    /** 验证 [p, p+n) 是否为合法 UTF-8 */
    static bool utf8_valid(const char* p, size_t n) noexcept
    {
        const auto* u = reinterpret_cast<const unsigned char*>(p);
        size_t i = 0;
        while (i < n) {
            int len = utf8_seq_len(u[i]);
            if (len < 0 || static_cast<size_t>(len) > n - i) return false;
            for (int k = 1; k < len; ++k)
                if ((u[i + k] & 0xC0) != 0x80) return false;
            uint32_t cp = utf8_decode(reinterpret_cast<const char*>(u + i));
            if (len == 2 && cp < 0x80u)     return false;  /* overlong */
            if (len == 3 && cp < 0x800u)    return false;
            if (len == 4 && cp < 0x10000u)  return false;
            if (cp >= 0xD800u && cp <= 0xDFFFu) return false;  /* surrogate */
            if (cp > 0x10FFFFu) return false;
            i += static_cast<size_t>(len);
        }
        return true;
    }

    /** 解码 UTF-8 序列首字符（调用前已验证） */
    static uint32_t utf8_decode(const char* p) noexcept
    {
        const auto* u = reinterpret_cast<const unsigned char*>(p);
        if (u[0] < 0x80) return u[0];
        if (u[0] < 0xE0) return ((u[0] & 0x1Fu) << 6)  |  (u[1] & 0x3Fu);
        if (u[0] < 0xF0) return ((u[0] & 0x0Fu) << 12) | ((u[1] & 0x3Fu) << 6) | (u[2] & 0x3Fu);
        return ((u[0] & 0x07u) << 18) | ((u[1] & 0x3Fu) << 12) |
               ((u[2] & 0x3Fu) << 6)  |  (u[3] & 0x3Fu);
    }

    /** 编码 Unicode 码点为 UTF-8；写入 out，返回字节数 */
    static int utf8_encode(uint32_t cp, char* out) noexcept
    {
        auto* u = reinterpret_cast<unsigned char*>(out);
        if (cp < 0x80u) {
            u[0] = static_cast<unsigned char>(cp);
            return 1;
        }
        if (cp < 0x800u) {
            u[0] = static_cast<unsigned char>(0xC0u | (cp >> 6));
            u[1] = static_cast<unsigned char>(0x80u | (cp & 0x3Fu));
            return 2;
        }
        if (cp < 0x10000u) {
            u[0] = static_cast<unsigned char>(0xE0u | (cp >> 12));
            u[1] = static_cast<unsigned char>(0x80u | ((cp >> 6) & 0x3Fu));
            u[2] = static_cast<unsigned char>(0x80u | (cp & 0x3Fu));
            return 3;
        }
        u[0] = static_cast<unsigned char>(0xF0u | (cp >> 18));
        u[1] = static_cast<unsigned char>(0x80u | ((cp >> 12) & 0x3Fu));
        u[2] = static_cast<unsigned char>(0x80u | ((cp >> 6)  & 0x3Fu));
        u[3] = static_cast<unsigned char>(0x80u | (cp & 0x3Fu));
        return 4;
    }

    /** 字节 c 是否为 UTF-8 序列首字节（非续字节） */
    static bool is_rune_start(char c) noexcept
    {
        return (static_cast<unsigned char>(c) & 0xC0u) != 0x80u;
    }

    /** ASCII 空白 */
    static bool is_ascii_space(char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
               c == '\f' || c == '\v';
    }

    /** 确保 cap_ >= needed；不足则 2 倍扩容 */
    std::expected<void, StrError> grow_to(size_t needed)
    {
        if (cap_ >= needed) return {};
        size_t new_cap = (cap_ == 0) ? 8 : cap_ * 2;
        if (new_cap < needed) new_cap = needed;
        char* nd = static_cast<char*>(::operator new(new_cap, std::nothrow));
        if (!nd) return std::unexpected(StrError::Alloc);
        if (len_ > 0) std::memcpy(nd, data_, len_);
        ::operator delete(data_);
        data_ = nd;
        cap_  = new_cap;
        return {};
    }
};

} /* namespace algo */

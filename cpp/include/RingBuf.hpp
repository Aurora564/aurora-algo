#pragma once
/**
 * RingBuf.hpp — 环形缓冲区（C++23）
 *
 * 定长或自增长的先进先出循环缓冲区，直接按值存储 T。
 * 错误以 std::expected<T, RingBufError> 形式返回，不抛出异常。
 *
 * 两种构造方式：
 *   RingBuf<int> fixed(8);                    // 定长，容量恰好为 8
 *   RingBuf<int> grow(algo::growable, 4);     // 自增长，初始容量向上取 2 的幂
 */

#include <cstddef>
#include <expected>
#include <functional>
#include <new>
#include <utility>

namespace algo {

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

enum class RingBufError {
    Empty,      /* dequeue / peek 时缓冲区为空  */
    Full,       /* 定长缓冲区已满               */
    OutOfBound, /* get(i) 超出有效范围           */
    Alloc,      /* 内存分配失败                 */
    BadConfig,  /* 对定长缓冲区调用 reserve      */
};

/* ------------------------------------------------------------------ */
/*  growable 标签（区分两种构造方式）                                   */
/* ------------------------------------------------------------------ */

struct growable_t { explicit growable_t() = default; };
inline constexpr growable_t growable{};

/* ================================================================== */
/*  RingBuf<T>                                                         */
/* ================================================================== */

template<typename T>
class RingBuf {
public:
    /* ---- 构造 / 析构 -------------------------------------------- */

    /** 定长缓冲区：容量恰好为 cap */
    explicit RingBuf(size_t cap)
        : cap_(cap), growable_(false)
    {
        if (cap_ > 0)
            data_ = static_cast<T *>(::operator new(cap_ * sizeof(T)));
    }

    /** 自增长缓冲区：初始容量向上取整到 2 的幂 */
    RingBuf(growable_t, size_t init_cap)
        : growable_(true)
    {
        if (init_cap == 0) init_cap = 1;
        cap_  = next_pow2(init_cap);
        data_ = static_cast<T *>(::operator new(cap_ * sizeof(T)));
    }

    /* 深拷贝：线性化复制，head 归零 */
    RingBuf(const RingBuf &other)
        : len_(other.len_), cap_(other.cap_),
          head_(0), growable_(other.growable_)
    {
        if (cap_ > 0)
            data_ = static_cast<T *>(::operator new(cap_ * sizeof(T)));
        for (size_t i = 0; i < len_; ++i)
            new (data_ + i) T(other.data_[other.phys(i)]);
    }

    /* 移动构造 noexcept */
    RingBuf(RingBuf &&other) noexcept
        : data_(other.data_), len_(other.len_),
          cap_(other.cap_),   head_(other.head_),
          growable_(other.growable_)
    {
        other.data_ = nullptr;
        other.len_  = 0;
        other.cap_  = 0;
        other.head_ = 0;
    }

    /* copy-and-swap 统一赋值 */
    RingBuf &operator=(RingBuf other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    ~RingBuf() { destroy_all(); }

    /* ---- 核心操作 ----------------------------------------------- */

    /** 将 val 入队（拷贝）；定长满时返回 Full，自增长时自动扩容 */
    [[nodiscard]] std::expected<void, RingBufError> enqueue(const T &val)
    {
        if (len_ == cap_) {
            if (!growable_) return std::unexpected(RingBufError::Full);
            if (auto e = grow(); !e) return e;
        }
        new (data_ + tail_slot()) T(val);
        ++len_;
        return {};
    }

    /** 将 val 入队（移动）；定长满时返回 Full，自增长时自动扩容 */
    [[nodiscard]] std::expected<void, RingBufError> enqueue(T &&val)
    {
        if (len_ == cap_) {
            if (!growable_) return std::unexpected(RingBufError::Full);
            if (auto e = grow(); !e) return e;
        }
        new (data_ + tail_slot()) T(std::move(val));
        ++len_;
        return {};
    }

    /** 移除并返回队首元素；O(1) */
    [[nodiscard]] std::expected<T, RingBufError> dequeue()
    {
        if (len_ == 0) return std::unexpected(RingBufError::Empty);
        T val = std::move(data_[head_]);
        data_[head_].~T();
        head_ = wrap(head_ + 1);
        --len_;
        return val;
    }

    /* ---- Peek / 随机访问 --------------------------------------- */

    /** 查看队首元素（不移除）；O(1) */
    [[nodiscard]] std::expected<std::reference_wrapper<const T>, RingBufError>
    peek_front() const
    {
        if (len_ == 0) return std::unexpected(RingBufError::Empty);
        return std::cref(data_[head_]);
    }

    /** 查看队尾元素（不移除）；O(1) */
    [[nodiscard]] std::expected<std::reference_wrapper<const T>, RingBufError>
    peek_back() const
    {
        if (len_ == 0) return std::unexpected(RingBufError::Empty);
        return std::cref(data_[phys(len_ - 1)]);
    }

    /** 按逻辑下标随机访问（0 = 最旧）；O(1) */
    [[nodiscard]] std::expected<std::reference_wrapper<const T>, RingBufError>
    get(size_t i) const
    {
        if (i >= len_) return std::unexpected(RingBufError::OutOfBound);
        return std::cref(data_[phys(i)]);
    }

    [[nodiscard]] std::expected<std::reference_wrapper<T>, RingBufError>
    get(size_t i)
    {
        if (i >= len_) return std::unexpected(RingBufError::OutOfBound);
        return std::ref(data_[phys(i)]);
    }

    /* ---- 查询 -------------------------------------------------- */

    [[nodiscard]] size_t size()     const noexcept { return len_; }
    [[nodiscard]] size_t capacity() const noexcept { return cap_; }
    [[nodiscard]] bool   is_empty() const noexcept { return len_ == 0; }
    [[nodiscard]] bool   is_full()  const noexcept { return len_ == cap_; }

    /* ---- 容量管理 ----------------------------------------------- */

    /** 清空所有元素；保留已分配的内存 */
    void clear()
    {
        for (size_t i = 0; i < len_; ++i)
            data_[phys(i)].~T();
        len_  = 0;
        head_ = 0;
    }

    /** 预留至少 additional 个额外槽（仅自增长模式）*/
    [[nodiscard]] std::expected<void, RingBufError> reserve(size_t additional)
    {
        if (!growable_) return std::unexpected(RingBufError::BadConfig);
        size_t needed = len_ + additional;
        while (cap_ < needed) {
            if (auto e = grow(); !e) return e;
        }
        return {};
    }

    /* ---- ADL swap ---------------------------------------------- */
    friend void swap(RingBuf &a, RingBuf &b) noexcept
    {
        using std::swap;
        swap(a.data_,     b.data_);
        swap(a.len_,      b.len_);
        swap(a.cap_,      b.cap_);
        swap(a.head_,     b.head_);
        swap(a.growable_, b.growable_);
    }

private:
    T     *data_     = nullptr;
    size_t len_      = 0;
    size_t cap_      = 0;
    size_t head_     = 0;   /* 最旧元素的物理下标 */
    bool   growable_ = false;

    /* 逻辑下标 i → 物理下标
     * 自增长：cap 是 2 的幂，用位运算加速 */
    size_t phys(size_t i) const noexcept
    {
        if (growable_) return (head_ + i) & (cap_ - 1);
        return cap_ == 0 ? 0 : (head_ + i) % cap_;
    }

    /* 下一个写入槽的物理下标 */
    size_t tail_slot() const noexcept { return phys(len_); }

    /* 物理下标加 1（循环） */
    size_t wrap(size_t idx) const noexcept
    {
        if (growable_) return idx & (cap_ - 1);
        return cap_ == 0 ? 0 : idx % cap_;
    }

    /* 将容量加倍并线性化排列 */
    std::expected<void, RingBufError> grow()
    {
        size_t new_cap = cap_ == 0 ? 2 : cap_ * 2;
        T *new_data = static_cast<T *>(
            ::operator new(new_cap * sizeof(T), std::nothrow));
        if (!new_data) return std::unexpected(RingBufError::Alloc);

        /* 线性化：逻辑 0..len-1 → new_data[0..len-1] */
        for (size_t i = 0; i < len_; ++i) {
            new (new_data + i) T(std::move(data_[phys(i)]));
            data_[phys(i)].~T();
        }
        ::operator delete(data_);
        data_ = new_data;
        cap_  = new_cap;
        head_ = 0;
        return {};
    }

    void destroy_all()
    {
        for (size_t i = 0; i < len_; ++i)
            data_[phys(i)].~T();
        ::operator delete(data_);
        data_ = nullptr;
        len_  = 0;
        cap_  = 0;
        head_ = 0;
    }

    static size_t next_pow2(size_t n)
    {
        if (n == 0) return 1;
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }
};

} /* namespace algo */

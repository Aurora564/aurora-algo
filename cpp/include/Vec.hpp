#pragma once
/**
 * Vec.hpp  — Dynamic Vector (C++23)
 *
 * 设计要点：
 *  - Rule of Five：拷贝构造/赋值深拷贝（调用 T 的拷贝构造），
 *    移动构造/赋值 noexcept，析构安全处理 nullptr。
 *  - 错误以 std::expected<T, VecError> 形式返回，不抛出异常。
 *  - 扩容策略可在构造时注入，默认 ×2。
 *  - 使用 placement new / 显式析构管理原始内存，避免 T 的默认构造。
 */

#include <cstddef>
#include <cstring>
#include <cassert>
#include <functional>
#include <expected>
#include <new>
#include <utility>

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

enum class VecError {
    OutOfBounds,
    Empty,
    Alloc,
    BadConfig,
};

/* ------------------------------------------------------------------ */
/*  Vec<T>                                                              */
/* ------------------------------------------------------------------ */

template<typename T>
class Vec {
public:
    using GrowFn = std::function<size_t(size_t)>;

    /* ---- 构造 / 析构 -------------------------------------------- */

    explicit Vec(size_t init_cap = 4, GrowFn grow = default_grow)
        : grow_(std::move(grow))
    {
        if (init_cap > 0) {
            data_ = static_cast<T *>(::operator new(init_cap * sizeof(T)));
            cap_  = init_cap;
        }
    }

    /* 深拷贝：调用 T 的拷贝构造 */
    Vec(const Vec &other) : grow_(other.grow_)
    {
        if (other.cap_ > 0) {
            data_ = static_cast<T *>(::operator new(other.cap_ * sizeof(T)));
            cap_  = other.cap_;
        }
        for (size_t i = 0; i < other.len_; i++)
            new (data_ + i) T(other.data_[i]);
        len_ = other.len_;
    }

    /* 移动构造 noexcept */
    Vec(Vec &&other) noexcept
        : data_(other.data_), len_(other.len_),
          cap_(other.cap_),   grow_(std::move(other.grow_))
    {
        other.data_ = nullptr;
        other.len_  = 0;
        other.cap_  = 0;
    }

    /* copy-and-swap 统一赋值 */
    Vec &operator=(Vec other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    ~Vec() { destroy_all(); }

    /* ---- 基本读写 ----------------------------------------------- */

    std::expected<void, VecError> push(const T &elem)
    {
        if (auto e = ensure_cap(len_ + 1); !e) return e;
        new (data_ + len_) T(elem);
        ++len_;
        return {};
    }

    std::expected<void, VecError> push(T &&elem)
    {
        if (auto e = ensure_cap(len_ + 1); !e) return e;
        new (data_ + len_) T(std::move(elem));
        ++len_;
        return {};
    }

    std::expected<T, VecError> pop()
    {
        if (len_ == 0) return std::unexpected(VecError::Empty);
        --len_;
        T val = std::move(data_[len_]);
        data_[len_].~T();
        return val;
    }

    std::expected<std::reference_wrapper<T>, VecError> get(size_t i)
    {
        if (i >= len_) return std::unexpected(VecError::OutOfBounds);
        return std::ref(data_[i]);
    }

    std::expected<std::reference_wrapper<const T>, VecError> get(size_t i) const
    {
        if (i >= len_) return std::unexpected(VecError::OutOfBounds);
        return std::cref(data_[i]);
    }

    std::expected<void, VecError> set(size_t i, T elem)
    {
        if (i >= len_) return std::unexpected(VecError::OutOfBounds);
        data_[i] = std::move(elem);
        return {};
    }

    std::expected<void, VecError> insert(size_t i, T elem)
    {
        if (i > len_) return std::unexpected(VecError::OutOfBounds);
        if (auto e = ensure_cap(len_ + 1); !e) return e;

        /* 末尾构造一个临时占位，然后向右移动 */
        if (i < len_) {
            new (data_ + len_) T(std::move(data_[len_ - 1]));
            for (size_t j = len_ - 1; j > i; --j)
                data_[j] = std::move(data_[j - 1]);
            data_[i] = std::move(elem);
        } else {
            new (data_ + len_) T(std::move(elem));
        }
        ++len_;
        return {};
    }

    std::expected<T, VecError> remove(size_t i)
    {
        if (i >= len_) return std::unexpected(VecError::OutOfBounds);
        T val = std::move(data_[i]);
        for (size_t j = i; j < len_ - 1; ++j)
            data_[j] = std::move(data_[j + 1]);
        data_[len_ - 1].~T();
        --len_;
        return val;
    }

    std::expected<T, VecError> swap_remove(size_t i)
    {
        if (i >= len_) return std::unexpected(VecError::OutOfBounds);
        T val = std::move(data_[i]);
        --len_;
        if (i < len_) {
            data_[i] = std::move(data_[len_]);
        }
        data_[len_].~T();
        return val;
    }

    /* ---- 容量管理 ----------------------------------------------- */

    size_t len()      const noexcept { return len_; }
    size_t cap()      const noexcept { return cap_; }
    bool   is_empty() const noexcept { return len_ == 0; }

    std::expected<void, VecError> reserve(size_t additional)
    {
        return ensure_cap(len_ + additional);
    }

    std::expected<void, VecError> shrink_to_fit()
    {
        if (len_ == cap_) return {};
        if (len_ == 0) {
            ::operator delete(data_);
            data_ = nullptr;
            cap_  = 0;
            return {};
        }
        T *new_data = static_cast<T *>(::operator new(len_ * sizeof(T)));
        if (!new_data) return std::unexpected(VecError::Alloc);
        for (size_t i = 0; i < len_; i++) {
            new (new_data + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        ::operator delete(data_);
        data_ = new_data;
        cap_  = len_;
        return {};
    }

    void clear()
    {
        for (size_t i = 0; i < len_; i++)
            data_[i].~T();
        len_ = 0;
    }

    /* ---- 批量操作 ----------------------------------------------- */

    /** 追加 n 个元素（拷贝） */
    std::expected<void, VecError> extend(const T *src, size_t n)
    {
        if (n == 0) return {};
        if (auto e = ensure_cap(len_ + n); !e) return e;
        for (size_t i = 0; i < n; i++)
            new (data_ + len_ + i) T(src[i]);
        len_ += n;
        return {};
    }

    /** 借用视图：调用者持有期间不得触发重新分配 */
    T       *as_slice()       noexcept { return data_; }
    const T *as_slice() const noexcept { return data_; }

    void foreach(std::function<void(T &)> fn)
    {
        for (size_t i = 0; i < len_; i++)
            fn(data_[i]);
    }

    /* ---- ADL swap ----------------------------------------------- */
    friend void swap(Vec &a, Vec &b) noexcept
    {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.len_,  b.len_);
        swap(a.cap_,  b.cap_);
        swap(a.grow_, b.grow_);
    }

private:
    T      *data_ = nullptr;
    size_t  len_  = 0;
    size_t  cap_  = 0;
    GrowFn  grow_;

    static size_t default_grow(size_t cap) { return cap == 0 ? 4 : cap * 2; }

    std::expected<void, VecError> ensure_cap(size_t needed)
    {
        if (needed <= cap_) return {};

        size_t new_cap = cap_ == 0 ? 4 : cap_;
        while (new_cap < needed)
            new_cap = grow_(new_cap);

        T *new_data = static_cast<T *>(::operator new(new_cap * sizeof(T), std::nothrow));
        if (!new_data) return std::unexpected(VecError::Alloc);

        for (size_t i = 0; i < len_; i++) {
            new (new_data + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        ::operator delete(data_);
        data_ = new_data;
        cap_  = new_cap;
        return {};
    }

    void destroy_all()
    {
        for (size_t i = 0; i < len_; i++)
            data_[i].~T();
        ::operator delete(data_);
        data_ = nullptr;
        len_  = 0;
        cap_  = 0;
    }
};

#pragma once
/**
 * Stack.hpp — 栈（C++23）
 *
 * 提供两种模板实现：
 *  - ArrayStack<T>：基于 Vec<T>，连续内存，缓存友好。
 *  - ListStack<T> ：基于轻量节点单链表，每次 push/pop 动态分配/释放。
 *
 * 错误以 std::expected<T, StackError> 形式返回，不抛出异常。
 */

#include "Vec.hpp"

#include <expected>
#include <functional>
#include <utility>

namespace algo {

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

enum class StackError {
    Empty,  /* pop/peek 时栈为空     */
    Alloc,  /* 内存分配失败          */
};

/* ================================================================== */
/*  ArrayStack<T>  — 基于 Vec<T>                                       */
/* ================================================================== */

template<typename T>
class ArrayStack {
public:
    /* ---- 构造 / 析构 -------------------------------------------- */

    explicit ArrayStack(size_t init_cap = 4) : base_(init_cap) {}

    /* Rule of Five：依赖 Vec<T> 的语义，声明 = default 即可 */
    ArrayStack(const ArrayStack &)            = default;
    ArrayStack(ArrayStack &&) noexcept        = default;
    ArrayStack &operator=(const ArrayStack &) = default;
    ArrayStack &operator=(ArrayStack &&) noexcept = default;
    ~ArrayStack()                             = default;

    /* ---- 核心操作 ----------------------------------------------- */

    /** 将 val 压入栈顶（拷贝）；均摊 O(1) */
    std::expected<void, StackError> push(const T &val)
    {
        auto r = base_.push(val);
        if (!r) return std::unexpected(StackError::Alloc);
        return {};
    }

    /** 将 val 压入栈顶（移动）；均摊 O(1) */
    std::expected<void, StackError> push(T &&val)
    {
        auto r = base_.push(std::move(val));
        if (!r) return std::unexpected(StackError::Alloc);
        return {};
    }

    /** 弹出栈顶元素（移动语义）；O(1) */
    std::expected<T, StackError> pop()
    {
        auto r = base_.pop();
        if (!r) return std::unexpected(StackError::Empty);
        return std::move(*r);
    }

    /** 返回栈顶元素的可变引用（不移除）；O(1) */
    std::expected<std::reference_wrapper<T>, StackError> peek()
    {
        if (base_.is_empty()) return std::unexpected(StackError::Empty);
        auto r = base_.get(base_.len() - 1);
        return r.value();  /* get 不会失败，已确认非空 */
    }

    /** 返回栈顶元素的常量引用（不移除）；O(1) */
    std::expected<std::reference_wrapper<const T>, StackError> peek() const
    {
        if (base_.is_empty()) return std::unexpected(StackError::Empty);
        auto r = base_.get(base_.len() - 1);
        return r.value();
    }

    /* ---- 查询 -------------------------------------------------- */

    size_t len()      const noexcept { return base_.len(); }
    bool   is_empty() const noexcept { return base_.is_empty(); }

    /* ---- 容量管理 ----------------------------------------------- */

    void clear() { base_.clear(); }

    /** 预留至少 additional 个额外槽位 */
    std::expected<void, StackError> reserve(size_t additional)
    {
        auto r = base_.reserve(additional);
        if (!r) return std::unexpected(StackError::Alloc);
        return {};
    }

    /** 容量收缩至当前长度 */
    std::expected<void, StackError> shrink_to_fit()
    {
        auto r = base_.shrink_to_fit();
        if (!r) return std::unexpected(StackError::Alloc);
        return {};
    }

private:
    Vec<T> base_;
};

/* ================================================================== */
/*  ListStack<T>  — 轻量节点单链表                                      */
/* ================================================================== */

template<typename T>
class ListStack {
public:
    /* ---- 构造 / 析构 -------------------------------------------- */

    ListStack() = default;

    /* 深拷贝：逐节点复制 */
    ListStack(const ListStack &other) { copy_from(other); }

    /* 移动构造：O(1) */
    ListStack(ListStack &&other) noexcept
        : top_(other.top_), len_(other.len_)
    {
        other.top_ = nullptr;
        other.len_ = 0;
    }

    /* copy-and-swap 统一赋值 */
    ListStack &operator=(ListStack other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    ~ListStack() { clear(); }

    /* ---- 核心操作 ----------------------------------------------- */

    /** 将 val 压入栈顶（拷贝）；O(1) */
    std::expected<void, StackError> push(const T &val)
    {
        Node *node = new (std::nothrow) Node{val, top_};
        if (!node) return std::unexpected(StackError::Alloc);
        top_ = node;
        ++len_;
        return {};
    }

    /** 将 val 压入栈顶（移动）；O(1) */
    std::expected<void, StackError> push(T &&val)
    {
        Node *node = new (std::nothrow) Node{std::move(val), top_};
        if (!node) return std::unexpected(StackError::Alloc);
        top_ = node;
        ++len_;
        return {};
    }

    /** 弹出栈顶元素；O(1) */
    std::expected<T, StackError> pop()
    {
        if (!top_) return std::unexpected(StackError::Empty);
        Node *node = top_;
        top_       = node->next;
        --len_;
        T val = std::move(node->val);
        delete node;
        return val;
    }

    /** 返回栈顶元素的可变引用（不移除）；O(1) */
    std::expected<std::reference_wrapper<T>, StackError> peek()
    {
        if (!top_) return std::unexpected(StackError::Empty);
        return std::ref(top_->val);
    }

    /** 返回栈顶元素的常量引用（不移除）；O(1) */
    std::expected<std::reference_wrapper<const T>, StackError> peek() const
    {
        if (!top_) return std::unexpected(StackError::Empty);
        return std::cref(top_->val);
    }

    /* ---- 查询 -------------------------------------------------- */

    size_t len()      const noexcept { return len_; }
    bool   is_empty() const noexcept { return top_ == nullptr; }

    /* ---- 容量管理 ----------------------------------------------- */

    void clear()
    {
        while (top_) {
            Node *next = top_->next;
            delete top_;
            top_ = next;
        }
        len_ = 0;
    }

    /* ---- ADL swap ---------------------------------------------- */
    friend void swap(ListStack &a, ListStack &b) noexcept
    {
        using std::swap;
        swap(a.top_, b.top_);
        swap(a.len_, b.len_);
    }

private:
    struct Node {
        T     val;
        Node *next = nullptr;
    };

    Node  *top_ = nullptr;
    size_t len_ = 0;

    /* 深拷贝辅助：按逆序重建链表保持相同顺序 */
    void copy_from(const ListStack &other)
    {
        if (!other.top_) return;

        /* 收集 other 的所有值（从栈顶到栈底）*/
        Node *cur    = other.top_;
        Node *prev   = nullptr;
        Node *new_top = nullptr;

        while (cur) {
            Node *node = new Node{cur->val, nullptr};
            node->next = new_top;
            new_top    = node;
            cur        = cur->next;
        }

        /* new_top 现在是逆序的（原来的栈底在最前），再翻转一次 */
        cur = new_top;
        new_top = nullptr;
        prev = nullptr;
        while (cur) {
            Node *next = cur->next;
            cur->next  = prev;
            prev       = cur;
            cur        = next;
        }
        top_ = prev;
        len_ = other.len_;
    }
};

} /* namespace algo */

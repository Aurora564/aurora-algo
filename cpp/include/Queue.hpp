#pragma once
/**
 * Queue.hpp — 先进先出队列（C++23）
 *
 * 基于 DList<T> 的薄封装：enqueue = push_back，dequeue = pop_front。
 * 错误以 std::expected<T, QueueError> 形式返回，不抛出异常。
 */

#include "DList.hpp"

namespace algo {

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

enum class QueueError {
    Empty,  /* dequeue / peek 时队列为空 */
    Alloc,  /* 内存分配失败              */
};

/* ================================================================== */
/*  Queue<T>                                                           */
/* ================================================================== */

template<typename T>
class Queue {
public:
    /* ---- 构造 / 析构 -------------------------------------------- */

    Queue() = default;

    /* Rule of Five：依赖 DList<T> 的语义 */
    Queue(const Queue &)            = default;
    Queue(Queue &&) noexcept        = default;
    Queue &operator=(const Queue &) = default;
    Queue &operator=(Queue &&) noexcept = default;
    ~Queue()                        = default;

    /* ---- 核心操作 ----------------------------------------------- */

    /** 将 val 入队（拷贝）；均摊 O(1) */
    [[nodiscard]] std::expected<void, QueueError> enqueue(const T &val)
    {
        auto r = base_.push_back(val);
        if (!r) return std::unexpected(QueueError::Alloc);
        return {};
    }

    /** 将 val 入队（移动）；均摊 O(1) */
    [[nodiscard]] std::expected<void, QueueError> enqueue(T &&val)
    {
        auto r = base_.push_back(std::move(val));
        if (!r) return std::unexpected(QueueError::Alloc);
        return {};
    }

    /** 移除并返回队首元素；O(1) */
    [[nodiscard]] std::expected<T, QueueError> dequeue()
    {
        auto r = base_.pop_front();
        if (!r) return std::unexpected(QueueError::Empty);
        return std::move(*r);
    }

    /* ---- Peek -------------------------------------------------- */

    /** 查看队首元素（不移除）；O(1) */
    [[nodiscard]] std::expected<std::reference_wrapper<const T>, QueueError>
    peek_front() const
    {
        auto r = base_.peek_front();
        if (!r) return std::unexpected(QueueError::Empty);
        return *r;
    }

    /** 查看队尾元素（不移除）；O(1) */
    [[nodiscard]] std::expected<std::reference_wrapper<const T>, QueueError>
    peek_back() const
    {
        auto r = base_.peek_back();
        if (!r) return std::unexpected(QueueError::Empty);
        return *r;
    }

    /* ---- 查询 -------------------------------------------------- */

    [[nodiscard]] size_t size()     const noexcept { return base_.size(); }
    [[nodiscard]] bool   is_empty() const noexcept { return base_.is_empty(); }

    /* ---- 迭代器（转发 DList）------------------------------------ */

    auto begin() const noexcept { return base_.begin(); }
    auto end()   const noexcept { return base_.end(); }

    /* ---- 容量管理 ----------------------------------------------- */

    void clear() { base_.clear(); }

    /* ---- ADL swap ---------------------------------------------- */
    friend void swap(Queue &a, Queue &b) noexcept
    {
        using std::swap;
        swap(a.base_, b.base_);
    }

private:
    DList<T> base_;
};

} /* namespace algo */

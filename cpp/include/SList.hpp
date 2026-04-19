#pragma once
/**
 * SList.hpp — Singly Linked List (C++23)
 *
 * 设计要点：
 *  - 无哨兵节点：head_ / tail_ 为真实节点指针，nullptr 表示空链表。
 *    （C++ 有类型系统，不需要哨兵消除 NULL 特判；代码更直观。）
 *  - 值语义：Node 内联存储 T，深拷贝调用 T 的拷贝构造。
 *  - Rule of Five：拷贝构造深拷贝，移动构造/赋值 noexcept。
 *  - 错误以 std::expected<T, SListError> 形式返回，不抛出异常。
 *  - push/pop 提供 const T& 和 T&& 两种重载。
 */

#include <cstddef>
#include <cassert>
#include <functional>
#include <expected>
#include <utility>

/* ------------------------------------------------------------------ */
/*  错误码                                                              */
/* ------------------------------------------------------------------ */

enum class SListError {
    Empty,
    OutOfBounds,
    Alloc,
    NoCloneFn,
};

/* ------------------------------------------------------------------ */
/*  SList<T>                                                            */
/* ------------------------------------------------------------------ */

template<typename T>
class SList {
public:
    /* ---- 节点 ------------------------------------------------------ */

    struct Node {
        T     val;
        Node *next = nullptr;

        explicit Node(const T &v, Node *n = nullptr) : val(v),             next(n) {}
        explicit Node(T &&v,      Node *n = nullptr) : val(std::move(v)),  next(n) {}
    };

    /* ---- 构造 / 析构 ----------------------------------------------- */

    SList() = default;

    /* 深拷贝：逐节点调用 T 的拷贝构造 */
    SList(const SList &other)
    {
        for (Node *curr = other.head_; curr; curr = curr->next)
            push_back_nocheck(curr->val);
    }

    /* 移动构造 noexcept */
    SList(SList &&other) noexcept
        : head_(other.head_), tail_(other.tail_), len_(other.len_)
    {
        other.head_ = other.tail_ = nullptr;
        other.len_  = 0;
    }

    /* copy-and-swap 统一赋值 */
    SList &operator=(SList other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    ~SList() { clear(); }

    /* ---- 基本读写 -------------------------------------------------- */

    std::expected<void, SListError> push_front(const T &val)
    {
        Node *node = new(std::nothrow) Node(val, head_);
        if (!node) return std::unexpected(SListError::Alloc);
        if (!head_) tail_ = node;
        head_ = node;
        ++len_;
        return {};
    }

    std::expected<void, SListError> push_front(T &&val)
    {
        Node *node = new(std::nothrow) Node(std::move(val), head_);
        if (!node) return std::unexpected(SListError::Alloc);
        if (!head_) tail_ = node;
        head_ = node;
        ++len_;
        return {};
    }

    std::expected<void, SListError> push_back(const T &val)
    {
        Node *node = new(std::nothrow) Node(val);
        if (!node) return std::unexpected(SListError::Alloc);
        if (tail_) tail_->next = node; else head_ = node;
        tail_ = node;
        ++len_;
        return {};
    }

    std::expected<void, SListError> push_back(T &&val)
    {
        Node *node = new(std::nothrow) Node(std::move(val));
        if (!node) return std::unexpected(SListError::Alloc);
        if (tail_) tail_->next = node; else head_ = node;
        tail_ = node;
        ++len_;
        return {};
    }

    std::expected<T, SListError> pop_front()
    {
        if (!head_) return std::unexpected(SListError::Empty);
        Node *node = head_;
        T val      = std::move(node->val);
        head_      = node->next;
        if (!head_) tail_ = nullptr;
        delete node;
        --len_;
        return val;
    }

    std::expected<T, SListError> pop_back()
    {
        if (!head_) return std::unexpected(SListError::Empty);

        if (head_ == tail_) {       /* 只有一个节点 */
            T val = std::move(head_->val);
            delete head_;
            head_ = tail_ = nullptr;
            --len_;
            return val;
        }

        /* 找 tail 的前驱 */
        Node *prev = head_;
        while (prev->next != tail_) prev = prev->next;

        T val = std::move(tail_->val);
        delete tail_;
        tail_       = prev;
        tail_->next = nullptr;
        --len_;
        return val;
    }

    std::expected<std::reference_wrapper<T>, SListError> peek_front()
    {
        if (!head_) return std::unexpected(SListError::Empty);
        return std::ref(head_->val);
    }

    std::expected<std::reference_wrapper<const T>, SListError> peek_front() const
    {
        if (!head_) return std::unexpected(SListError::Empty);
        return std::cref(head_->val);
    }

    std::expected<std::reference_wrapper<T>, SListError> peek_back()
    {
        if (!tail_) return std::unexpected(SListError::Empty);
        return std::ref(tail_->val);
    }

    std::expected<std::reference_wrapper<const T>, SListError> peek_back() const
    {
        if (!tail_) return std::unexpected(SListError::Empty);
        return std::cref(tail_->val);
    }

    /**
     * 在 node 之后插入 val。
     * node == nullptr 表示插入到链表头部（等价于 push_front）。
     */
    std::expected<void, SListError> insert_after(Node *node, const T &val)
    {
        if (!node) return push_front(val);
        Node *new_node = new(std::nothrow) Node(val, node->next);
        if (!new_node) return std::unexpected(SListError::Alloc);
        node->next = new_node;
        if (node == tail_) tail_ = new_node;
        ++len_;
        return {};
    }

    std::expected<void, SListError> insert_after(Node *node, T &&val)
    {
        if (!node) return push_front(std::move(val));
        Node *new_node = new(std::nothrow) Node(std::move(val), node->next);
        if (!new_node) return std::unexpected(SListError::Alloc);
        node->next = new_node;
        if (node == tail_) tail_ = new_node;
        ++len_;
        return {};
    }

    /**
     * 移除 node 之后的节点，返回其值。
     * node == nullptr 表示移除头节点（等价于 pop_front）。
     * 若 node 为尾节点（无后继）返回 OutOfBounds。
     */
    std::expected<T, SListError> remove_after(Node *node)
    {
        if (!node) return pop_front();
        if (!node->next) return std::unexpected(SListError::OutOfBounds);

        Node *target = node->next;
        T val        = std::move(target->val);
        node->next   = target->next;
        if (target == tail_) tail_ = node;
        delete target;
        --len_;
        return val;
    }

    /** 按逻辑索引访问（0 = 头节点）；O(n) */
    std::expected<std::reference_wrapper<T>, SListError> get(size_t i)
    {
        if (i >= len_) return std::unexpected(SListError::OutOfBounds);
        Node *curr = head_;
        for (size_t j = 0; j < i; ++j) curr = curr->next;
        return std::ref(curr->val);
    }

    std::expected<std::reference_wrapper<const T>, SListError> get(size_t i) const
    {
        if (i >= len_) return std::unexpected(SListError::OutOfBounds);
        Node *curr = head_;
        for (size_t j = 0; j < i; ++j) curr = curr->next;
        return std::cref(curr->val);
    }

    /** 返回第一个满足 pred 的节点；未找到返回 nullptr */
    Node *find(std::function<bool(const T &)> pred) const
    {
        for (Node *curr = head_; curr; curr = curr->next)
            if (pred(curr->val)) return curr;
        return nullptr;
    }

    /* ---- 容量 ------------------------------------------------------ */

    size_t len()      const noexcept { return len_; }
    bool   is_empty() const noexcept { return len_ == 0; }

    void clear()
    {
        Node *curr = head_;
        while (curr) {
            Node *next = curr->next;
            delete curr;
            curr = next;
        }
        head_ = tail_ = nullptr;
        len_  = 0;
    }

    /* ---- 遍历 ------------------------------------------------------ */

    void foreach(std::function<void(T &)> fn)
    {
        for (Node *curr = head_; curr; curr = curr->next)
            fn(curr->val);
    }

    void foreach(std::function<void(const T &)> fn) const
    {
        for (Node *curr = head_; curr; curr = curr->next)
            fn(curr->val);
    }

    void reverse() noexcept
    {
        if (len_ <= 1) return;
        tail_       = head_;    /* 当前头节点将成为新尾节点 */
        Node *prev  = nullptr;
        Node *curr  = head_;
        while (curr) {
            Node *next = curr->next;
            curr->next = prev;
            prev       = curr;
            curr       = next;
        }
        head_            = prev;
        tail_->next      = nullptr;
    }

    /* ---- 迭代器（range-based for 支持）----------------------------- */

    struct Iterator {
        Node *ptr;
        T           &operator*()       { return ptr->val; }
        Iterator    &operator++()      { ptr = ptr->next; return *this; }
        bool         operator==(const Iterator &o) const { return ptr == o.ptr; }
        bool         operator!=(const Iterator &o) const { return ptr != o.ptr; }
    };

    Iterator begin() { return {head_}; }
    Iterator end()   { return {nullptr}; }

    struct ConstIterator {
        const Node *ptr;
        const T     &operator*()               { return ptr->val; }
        ConstIterator &operator++()            { ptr = ptr->next; return *this; }
        bool          operator==(const ConstIterator &o) const { return ptr == o.ptr; }
        bool          operator!=(const ConstIterator &o) const { return ptr != o.ptr; }
    };

    ConstIterator begin() const { return {head_}; }
    ConstIterator end()   const { return {nullptr}; }

    /* ---- ADL swap -------------------------------------------------- */

    friend void swap(SList &a, SList &b) noexcept
    {
        using std::swap;
        swap(a.head_, b.head_);
        swap(a.tail_, b.tail_);
        swap(a.len_,  b.len_);
    }

    /* ---- 暴露 head 供游标操作（返回真实第一节点）--------------------- */

    Node *head() noexcept { return head_; }
    Node *tail() noexcept { return tail_; }

private:
    Node  *head_ = nullptr;
    Node  *tail_ = nullptr;
    size_t len_  = 0;

    /* 供拷贝构造使用的无错误检查版本（拷贝构造失败直接抛异常）*/
    void push_back_nocheck(const T &val)
    {
        Node *node = new Node(val);
        if (tail_) tail_->next = node; else head_ = node;
        tail_ = node;
        ++len_;
    }
};

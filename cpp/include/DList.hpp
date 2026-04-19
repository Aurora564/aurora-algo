#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <iterator>
#include <utility>

namespace algo {

/* ------------------------------------------------------------------ */
/* Error type                                                           */
/* ------------------------------------------------------------------ */

enum class DListError {
    Empty     = 1,
    OutOfBound = 2,
    AllocFail  = 3,
};

/* ------------------------------------------------------------------ */
/* DList<T>                                                             */
/* ------------------------------------------------------------------ */

template <typename T>
class DList {
    struct Node {
        T     val;
        Node *prev{nullptr};
        Node *next{nullptr};

        explicit Node(T v) : val(std::move(v)) {}
        /* Sentinel node — no value */
        Node() = default;
    };

    Node  *dummy_{nullptr};
    size_t len_{0};

    /* ---- internal primitives -------------------------------------- */

    /* Insert node immediately before ref. */
    static void link_before(Node *ref, Node *node) noexcept
    {
        node->prev      = ref->prev;
        node->next      = ref;
        ref->prev->next = node;
        ref->prev       = node;
    }

    /* Detach node from its neighbours.  Does not free. */
    static void unlink(Node *node) noexcept
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    /* Allocate a new data node and link it before ref. */
    [[nodiscard]] std::expected<void, DListError>
    emplace_before(Node *ref, T val)
    {
        Node *n = new (std::nothrow) Node(std::move(val));
        if (!n) return std::unexpected(DListError::AllocFail);
        link_before(ref, n);
        ++len_;
        return {};
    }

    /* Deep-copy all nodes from other into this (already-initialised) list. */
    [[nodiscard]] std::expected<void, DListError>
    copy_nodes(const DList &other)
    {
        for (Node *c = other.dummy_->next; c != other.dummy_; c = c->next) {
            auto r = emplace_before(dummy_, c->val);
            if (!r) return r;
        }
        return {};
    }

public:
    /* ---- construction --------------------------------------------- */

    DList()
    {
        dummy_ = new (std::nothrow) Node();
        if (dummy_) {
            dummy_->prev = dummy_;
            dummy_->next = dummy_;
        }
    }

    ~DList() { clear(); delete dummy_; }

    /* Rule of Five */

    DList(const DList &other) : DList()
    {
        if (dummy_) (void)copy_nodes(other);
    }

    DList(DList &&other) noexcept
        : dummy_(other.dummy_), len_(other.len_)
    {
        other.dummy_ = nullptr;
        other.len_   = 0;
    }

    DList &operator=(DList other) noexcept   /* copy-and-swap */
    {
        swap(*this, other);
        return *this;
    }

    friend void swap(DList &a, DList &b) noexcept
    {
        using std::swap;
        swap(a.dummy_, b.dummy_);
        swap(a.len_,   b.len_);
    }

    /* ---- capacity ------------------------------------------------- */

    [[nodiscard]] size_t size()     const noexcept { return len_; }
    [[nodiscard]] bool   is_empty() const noexcept { return len_ == 0; }

    /* ---- push / pop ----------------------------------------------- */

    [[nodiscard]] std::expected<void, DListError> push_front(T val)
    {
        return emplace_before(dummy_->next, std::move(val));
    }

    [[nodiscard]] std::expected<void, DListError> push_back(T val)
    {
        return emplace_before(dummy_, std::move(val));
    }

    [[nodiscard]] std::expected<T, DListError> pop_front()
    {
        if (len_ == 0) return std::unexpected(DListError::Empty);
        Node *n = dummy_->next;
        T val   = std::move(n->val);
        unlink(n);
        delete n;
        --len_;
        return val;
    }

    [[nodiscard]] std::expected<T, DListError> pop_back()
    {
        if (len_ == 0) return std::unexpected(DListError::Empty);
        Node *n = dummy_->prev;
        T val   = std::move(n->val);
        unlink(n);
        delete n;
        --len_;
        return val;
    }

    /* ---- peek ----------------------------------------------------- */

    [[nodiscard]] std::expected<std::reference_wrapper<const T>, DListError>
    peek_front() const
    {
        if (len_ == 0) return std::unexpected(DListError::Empty);
        return std::cref(dummy_->next->val);
    }

    [[nodiscard]] std::expected<std::reference_wrapper<const T>, DListError>
    peek_back() const
    {
        if (len_ == 0) return std::unexpected(DListError::Empty);
        return std::cref(dummy_->prev->val);
    }

    /* ---- random access -------------------------------------------- */

    /* Bidirectional O(n/2): i < len/2 → forward, else backward. */
    [[nodiscard]] std::expected<std::reference_wrapper<T>, DListError>
    get(size_t i)
    {
        if (i >= len_) return std::unexpected(DListError::OutOfBound);
        Node *curr;
        if (i < len_ / 2) {
            curr = dummy_->next;
            for (size_t k = 0; k < i; ++k) curr = curr->next;
        } else {
            curr = dummy_->prev;
            for (size_t k = len_ - 1; k > i; --k) curr = curr->prev;
        }
        return std::ref(curr->val);
    }

    /* ---- bidirectional iterator ----------------------------------- */

    class Iterator {
        friend class DList;
        Node *ptr_;
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = T *;
        using reference         = T &;

        explicit Iterator(Node *p) noexcept : ptr_(p) {}

        reference  operator*()  const noexcept { return ptr_->val; }
        pointer    operator->() const noexcept { return &ptr_->val; }

        Iterator &operator++() noexcept { ptr_ = ptr_->next; return *this; }
        Iterator  operator++(int) noexcept { auto t = *this; ++*this; return t; }

        Iterator &operator--() noexcept { ptr_ = ptr_->prev; return *this; }
        Iterator  operator--(int) noexcept { auto t = *this; --*this; return t; }

        bool operator==(const Iterator &o) const noexcept { return ptr_ == o.ptr_; }
        bool operator!=(const Iterator &o) const noexcept { return ptr_ != o.ptr_; }
    };

    Iterator begin() const noexcept { return Iterator(dummy_->next); }
    Iterator end()   const noexcept { return Iterator(dummy_); }

    std::reverse_iterator<Iterator> rbegin() const noexcept
    { return std::make_reverse_iterator(end()); }
    std::reverse_iterator<Iterator> rend() const noexcept
    { return std::make_reverse_iterator(begin()); }

    /* ---- reverse in-place ---------------------------------------- */

    void reverse() noexcept
    {
        Node *curr = dummy_;
        do {
            std::swap(curr->prev, curr->next);
            curr = curr->prev; /* was curr->next before swap */
        } while (curr != dummy_);
    }

    /* ---- clear ----------------------------------------------------- */

    void clear() noexcept
    {
        if (!dummy_) return;
        Node *curr = dummy_->next;
        while (curr != dummy_) {
            Node *nxt = curr->next;
            delete curr;
            curr = nxt;
        }
        dummy_->prev = dummy_;
        dummy_->next = dummy_;
        len_         = 0;
    }

    /* ---- splice ---------------------------------------------------- */

    /* Move all nodes from src before pos (which belongs to *this).    *
     * pos == end() appends. src becomes empty in O(1).                */
    void splice(Iterator pos, DList &src) noexcept
    {
        if (src.len_ == 0) return;

        Node *p = pos.ptr_;
        Node *src_first = src.dummy_->next;
        Node *src_last  = src.dummy_->prev;

        /* Detach src chain */
        src.dummy_->next = src.dummy_;
        src.dummy_->prev = src.dummy_;

        /* Wire before p */
        src_first->prev = p->prev;
        src_last->next  = p;
        p->prev->next   = src_first;
        p->prev         = src_last;

        len_     += src.len_;
        src.len_  = 0;
    }

    /* Convenience: splice src before node at index i. */
    [[nodiscard]] std::expected<void, DListError>
    splice_at(size_t i, DList &src)
    {
        if (i > len_) return std::unexpected(DListError::OutOfBound);
        Node *pos;
        if (i == len_) {
            pos = dummy_;
        } else if (i < len_ / 2) {
            pos = dummy_->next;
            for (size_t k = 0; k < i; ++k) pos = pos->next;
        } else {
            pos = dummy_->prev;
            for (size_t k = len_ - 1; k > i; --k) pos = pos->prev;
        }
        splice(Iterator(pos), src);
        return {};
    }
};

} // namespace algo

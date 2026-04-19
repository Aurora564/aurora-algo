/**
 * BST.hpp — Binary Search Tree (C++23, header-only)
 *
 * 设计：
 *  - std::unique_ptr<Node> 管理子节点，析构自动递归释放。
 *  - Compare 模板参数默认 std::less<K>，支持自定义比较。
 *  - 重复插入视为 upsert（更新 value），len 不变。
 *  - 删除采用中序后继替换（双子节点 case）。
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

namespace algo {

enum class BSTError { NotFound, Alloc, Empty };

template<typename K, typename V, typename Compare = std::less<K>>
class BST {
public:
    BST() = default;
    explicit BST(Compare cmp) : cmp_(std::move(cmp)) {}

    BST(const BST& other) : len_(other.len_), cmp_(other.cmp_) {
        root_ = clone_node(other.root_.get());
    }
    BST(BST&& other) noexcept = default;
    BST& operator=(BST other) noexcept {
        swap(*this, other);
        return *this;
    }
    ~BST() = default;

    friend void swap(BST& a, BST& b) noexcept {
        using std::swap;
        swap(a.root_, b.root_);
        swap(a.len_,  b.len_);
        swap(a.cmp_,  b.cmp_);
    }

    /* ---------------------------------------------------------------- */
    /*  核心操作                                                         */
    /* ---------------------------------------------------------------- */

    void insert(K key, V value) {
        insert_node(root_, std::move(key), std::move(value));
    }

    std::optional<std::reference_wrapper<V>> search(const K& key) {
        Node* n = find_node(root_.get(), key);
        if (!n) return std::nullopt;
        return std::ref(n->value);
    }

    std::optional<std::reference_wrapper<const V>> search(const K& key) const {
        const Node* n = find_node(root_.get(), key);
        if (!n) return std::nullopt;
        return std::cref(n->value);
    }

    bool remove(const K& key) {
        bool deleted = false;
        remove_node(root_, key, deleted);
        if (deleted) --len_;
        return deleted;
    }

    bool contains(const K& key) const noexcept {
        return find_node(root_.get(), key) != nullptr;
    }

    std::optional<std::pair<const K&, V&>> min() {
        if (!root_) return std::nullopt;
        Node* n = min_node(root_.get());
        return std::pair<const K&, V&>{n->key, n->value};
    }

    std::optional<std::pair<const K&, V&>> max() {
        if (!root_) return std::nullopt;
        Node* n = max_node(root_.get());
        return std::pair<const K&, V&>{n->key, n->value};
    }

    std::optional<std::pair<const K&, V&>> successor(const K& key) {
        Node* succ = nullptr;
        Node* n    = root_.get();
        while (n) {
            if (cmp_(key, n->key)) {
                succ = n;
                n    = n->left.get();
            } else if (cmp_(n->key, key)) {
                n = n->right.get();
            } else {
                if (n->right) {
                    Node* s = min_node(n->right.get());
                    return std::pair<const K&, V&>{s->key, s->value};
                }
                if (succ)
                    return std::pair<const K&, V&>{succ->key, succ->value};
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<std::pair<const K&, V&>> predecessor(const K& key) {
        Node* pred = nullptr;
        Node* n    = root_.get();
        while (n) {
            if (cmp_(n->key, key)) {
                pred = n;
                n    = n->right.get();
            } else if (cmp_(key, n->key)) {
                n = n->left.get();
            } else {
                if (n->left) {
                    Node* p = max_node(n->left.get());
                    return std::pair<const K&, V&>{p->key, p->value};
                }
                if (pred)
                    return std::pair<const K&, V&>{pred->key, pred->value};
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    /* ---------------------------------------------------------------- */
    /*  遍历                                                             */
    /* ---------------------------------------------------------------- */

    void in_order(auto fn) { in_order_rec(root_.get(), fn); }
    void pre_order(auto fn) { pre_order_rec(root_.get(), fn); }
    void post_order(auto fn) { post_order_rec(root_.get(), fn); }

    void level_order(auto fn) {
        if (!root_) return;
        std::queue<Node*> q;
        q.push(root_.get());
        while (!q.empty()) {
            Node* n = q.front(); q.pop();
            fn(n->key, n->value);
            if (n->left)  q.push(n->left.get());
            if (n->right) q.push(n->right.get());
        }
    }

    /* ---------------------------------------------------------------- */
    /*  统计                                                             */
    /* ---------------------------------------------------------------- */

    std::size_t len()      const noexcept { return len_; }
    bool        is_empty() const noexcept { return len_ == 0; }
    std::size_t height()   const noexcept { return height_of(root_.get()); }

    bool is_valid() const noexcept {
        return is_valid_range(root_.get(), nullptr, nullptr);
    }

private:
    struct Node {
        K                     key;
        V                     value;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        Node(K k, V v) : key(std::move(k)), value(std::move(v)) {}
    };

    std::unique_ptr<Node> root_;
    std::size_t           len_  = 0;
    Compare               cmp_;

    /* ---- helpers --------------------------------------------------- */

    Node* find_node(Node* n, const K& key) const noexcept {
        while (n) {
            if      (cmp_(key, n->key)) n = n->left.get();
            else if (cmp_(n->key, key)) n = n->right.get();
            else                        return n;
        }
        return nullptr;
    }

    void insert_node(std::unique_ptr<Node>& n, K key, V value) {
        if (!n) {
            n = std::make_unique<Node>(std::move(key), std::move(value));
            ++len_;
            return;
        }
        if      (cmp_(key, n->key)) insert_node(n->left,  std::move(key), std::move(value));
        else if (cmp_(n->key, key)) insert_node(n->right, std::move(key), std::move(value));
        else                        n->value = std::move(value);  /* upsert */
    }

    void remove_node(std::unique_ptr<Node>& n, const K& key, bool& deleted) {
        if (!n) return;
        if      (cmp_(key, n->key)) { remove_node(n->left,  key, deleted); }
        else if (cmp_(n->key, key)) { remove_node(n->right, key, deleted); }
        else {
            deleted = true;
            if (!n->left) {
                n = std::move(n->right);
            } else if (!n->right) {
                n = std::move(n->left);
            } else {
                Node* succ = min_node(n->right.get());
                n->key   = succ->key;
                n->value = std::move(succ->value);
                bool dummy = false;
                remove_node(n->right, n->key, dummy);
            }
        }
    }

    static Node* min_node(Node* n) noexcept {
        while (n->left) n = n->left.get();
        return n;
    }

    static Node* max_node(Node* n) noexcept {
        while (n->right) n = n->right.get();
        return n;
    }

    static std::size_t height_of(const Node* n) noexcept {
        if (!n) return 0;
        return std::max(height_of(n->left.get()), height_of(n->right.get())) + 1;
    }

    bool is_valid_range(const Node* n, const K* lo, const K* hi) const noexcept {
        if (!n) return true;
        if (lo && !cmp_(*lo, n->key)) return false;
        if (hi && !cmp_(n->key, *hi)) return false;
        return is_valid_range(n->left.get(),  lo,      &n->key) &&
               is_valid_range(n->right.get(), &n->key, hi);
    }

    static std::unique_ptr<Node> clone_node(const Node* src) {
        if (!src) return nullptr;
        auto n    = std::make_unique<Node>(src->key, src->value);
        n->left   = clone_node(src->left.get());
        n->right  = clone_node(src->right.get());
        return n;
    }

    static void in_order_rec(Node* n, auto& fn) {
        if (!n) return;
        in_order_rec(n->left.get(), fn);
        fn(n->key, n->value);
        in_order_rec(n->right.get(), fn);
    }

    static void pre_order_rec(Node* n, auto& fn) {
        if (!n) return;
        fn(n->key, n->value);
        pre_order_rec(n->left.get(), fn);
        pre_order_rec(n->right.get(), fn);
    }

    static void post_order_rec(Node* n, auto& fn) {
        if (!n) return;
        post_order_rec(n->left.get(), fn);
        post_order_rec(n->right.get(), fn);
        fn(n->key, n->value);
    }
};

} // namespace algo

/**
 * AVL.hpp — Self-balancing AVL Tree (C++23, header-only)
 *
 * 设计：
 *  - std::unique_ptr<Node> 管理子节点，析构自动递归释放。
 *  - Compare 模板参数默认 std::less<K>，支持自定义比较。
 *  - 重复插入视为 upsert（更新 value），len 不变。
 *  - 删除采用中序后继替换（双子节点 case）。
 *  - 每个节点缓存 int8_t height_，空节点高度为 0。
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

namespace algo {

template<typename K, typename V, typename Compare = std::less<K>>
class AVL {
public:
    AVL() = default;
    explicit AVL(Compare cmp) : cmp_(std::move(cmp)) {}

    AVL(const AVL& other) : len_(other.len_), cmp_(other.cmp_) {
        root_ = clone_node(other.root_.get());
    }
    AVL(AVL&& other) noexcept = default;
    AVL& operator=(AVL other) noexcept {
        swap(*this, other);
        return *this;
    }
    ~AVL() = default;

    friend void swap(AVL& a, AVL& b) noexcept {
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
        root_ = remove_node(std::move(root_), key, deleted);
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

    /* ---------------------------------------------------------------- */
    /*  遍历                                                             */
    /* ---------------------------------------------------------------- */

    void in_order(auto fn) const { in_order_rec(root_.get(), fn); }
    void level_order(auto fn) const {
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
    /*  统计 / 测试辅助                                                  */
    /* ---------------------------------------------------------------- */

    std::size_t len()      const noexcept { return len_; }
    bool        is_empty() const noexcept { return len_ == 0; }
    std::size_t height()   const noexcept { return static_cast<std::size_t>(node_height(root_.get())); }

    bool is_balanced()  const noexcept { return is_balanced_rec(root_.get()); }
    bool is_valid_bst() const noexcept { return is_valid_range(root_.get(), nullptr, nullptr); }

private:
    struct Node {
        K                     key;
        V                     value;
        int8_t                height_ = 1;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        Node(K k, V v) : key(std::move(k)), value(std::move(v)) {}
    };

    std::unique_ptr<Node> root_;
    std::size_t           len_ = 0;
    Compare               cmp_;

    /* ---- height helpers ------------------------------------------- */

    static int8_t node_height(const Node* n) noexcept {
        return n ? n->height_ : 0;
    }

    static void update_height(Node* n) noexcept {
        int8_t l = node_height(n->left.get());
        int8_t r = node_height(n->right.get());
        n->height_ = static_cast<int8_t>((l > r ? l : r) + 1);
    }

    static int balance_factor(const Node* n) noexcept {
        return static_cast<int>(node_height(n->left.get())) -
               static_cast<int>(node_height(n->right.get()));
    }

    /* ---- rotations ------------------------------------------------- */

    static std::unique_ptr<Node> rotate_right(std::unique_ptr<Node> n) {
        auto x    = std::move(n->left);
        n->left   = std::move(x->right);
        update_height(n.get());
        x->right  = std::move(n);
        update_height(x.get());
        return x;
    }

    static std::unique_ptr<Node> rotate_left(std::unique_ptr<Node> n) {
        auto y    = std::move(n->right);
        n->right  = std::move(y->left);
        update_height(n.get());
        y->left   = std::move(n);
        update_height(y.get());
        return y;
    }

    static std::unique_ptr<Node> rebalance(std::unique_ptr<Node> n) {
        update_height(n.get());
        int bf = balance_factor(n.get());
        if (bf == 2) {
            if (balance_factor(n->left.get()) < 0)
                n->left = rotate_left(std::move(n->left));  /* LR */
            return rotate_right(std::move(n));               /* LL */
        }
        if (bf == -2) {
            if (balance_factor(n->right.get()) > 0)
                n->right = rotate_right(std::move(n->right)); /* RL */
            return rotate_left(std::move(n));                  /* RR */
        }
        return n;
    }

    /* ---- search ---------------------------------------------------- */

    Node* find_node(Node* n, const K& key) const noexcept {
        while (n) {
            if      (cmp_(key, n->key)) n = n->left.get();
            else if (cmp_(n->key, key)) n = n->right.get();
            else                        return n;
        }
        return nullptr;
    }

    /* ---- insert ---------------------------------------------------- */

    void insert_node(std::unique_ptr<Node>& n, K key, V value) {
        if (!n) {
            n = std::make_unique<Node>(std::move(key), std::move(value));
            ++len_;
            return;
        }
        if      (cmp_(key, n->key)) insert_node(n->left,  std::move(key), std::move(value));
        else if (cmp_(n->key, key)) insert_node(n->right, std::move(key), std::move(value));
        else                        { n->value = std::move(value); return; }  /* upsert */
        n = rebalance(std::move(n));
    }

    /* ---- delete ---------------------------------------------------- */

    static Node* min_node(Node* n) noexcept {
        while (n->left) n = n->left.get();
        return n;
    }

    static Node* max_node(Node* n) noexcept {
        while (n->right) n = n->right.get();
        return n;
    }

    std::unique_ptr<Node> remove_node(std::unique_ptr<Node> n,
                                      const K& key, bool& deleted) {
        if (!n) return nullptr;
        if (cmp_(key, n->key)) {
            n->left  = remove_node(std::move(n->left),  key, deleted);
        } else if (cmp_(n->key, key)) {
            n->right = remove_node(std::move(n->right), key, deleted);
        } else {
            deleted = true;
            if (!n->left)  return std::move(n->right);
            if (!n->right) return std::move(n->left);
            Node* succ = min_node(n->right.get());
            n->key   = succ->key;
            n->value = std::move(succ->value);
            bool dummy = false;
            n->right = remove_node(std::move(n->right), n->key, dummy);
        }
        return rebalance(std::move(n));
    }

    /* ---- invariant helpers ---------------------------------------- */

    static bool is_balanced_rec(const Node* n) noexcept {
        if (!n) return true;
        int bf = balance_factor(n);
        if (bf < -1 || bf > 1) return false;
        return is_balanced_rec(n->left.get()) && is_balanced_rec(n->right.get());
    }

    bool is_valid_range(const Node* n, const K* lo, const K* hi) const noexcept {
        if (!n) return true;
        if (lo && !cmp_(*lo, n->key)) return false;
        if (hi && !cmp_(n->key, *hi)) return false;
        return is_valid_range(n->left.get(),  lo,      &n->key) &&
               is_valid_range(n->right.get(), &n->key, hi);
    }

    /* ---- copy ----------------------------------------------------- */

    static std::unique_ptr<Node> clone_node(const Node* src) {
        if (!src) return nullptr;
        auto n     = std::make_unique<Node>(src->key, src->value);
        n->height_ = src->height_;
        n->left    = clone_node(src->left.get());
        n->right   = clone_node(src->right.get());
        return n;
    }

    /* ---- traversal ------------------------------------------------ */

    static void in_order_rec(const Node* n, auto& fn) {
        if (!n) return;
        in_order_rec(n->left.get(), fn);
        fn(n->key, n->value);
        in_order_rec(n->right.get(), fn);
    }
};

} // namespace algo

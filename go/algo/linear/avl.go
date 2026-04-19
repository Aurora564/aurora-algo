package linear

import "cmp"

type avlNode[K cmp.Ordered, V any] struct {
	key    K
	value  V
	height int8
	left   *avlNode[K, V]
	right  *avlNode[K, V]
}

// AVL is a self-balancing binary search tree.
type AVL[K cmp.Ordered, V any] struct {
	root *avlNode[K, V]
	len  int
}

/* ---- height helpers ------------------------------------------------- */

func avlHeight[K cmp.Ordered, V any](n *avlNode[K, V]) int8 {
	if n == nil {
		return 0
	}
	return n.height
}

func avlUpdateHeight[K cmp.Ordered, V any](n *avlNode[K, V]) {
	l, r := avlHeight(n.left), avlHeight(n.right)
	if l > r {
		n.height = l + 1
	} else {
		n.height = r + 1
	}
}

func avlBF[K cmp.Ordered, V any](n *avlNode[K, V]) int {
	return int(avlHeight(n.left)) - int(avlHeight(n.right))
}

/* ---- rotations ------------------------------------------------------- */

func avlRotateRight[K cmp.Ordered, V any](n *avlNode[K, V]) *avlNode[K, V] {
	x := n.left
	n.left = x.right
	x.right = n
	avlUpdateHeight(n)
	avlUpdateHeight(x)
	return x
}

func avlRotateLeft[K cmp.Ordered, V any](n *avlNode[K, V]) *avlNode[K, V] {
	y := n.right
	n.right = y.left
	y.left = n
	avlUpdateHeight(n)
	avlUpdateHeight(y)
	return y
}

func avlRebalance[K cmp.Ordered, V any](n *avlNode[K, V]) *avlNode[K, V] {
	avlUpdateHeight(n)
	bf := avlBF(n)
	if bf == 2 {
		if avlBF(n.left) < 0 {
			n.left = avlRotateLeft(n.left) // LR
		}
		return avlRotateRight(n) // LL
	}
	if bf == -2 {
		if avlBF(n.right) > 0 {
			n.right = avlRotateRight(n.right) // RL
		}
		return avlRotateLeft(n) // RR
	}
	return n
}

/* ---- insert ---------------------------------------------------------- */

func avlInsert[K cmp.Ordered, V any](n *avlNode[K, V], key K, value V) (*avlNode[K, V], bool) {
	if n == nil {
		return &avlNode[K, V]{key: key, value: value, height: 1}, true
	}
	c := cmp.Compare(key, n.key)
	inserted := false
	switch {
	case c < 0:
		n.left, inserted = avlInsert(n.left, key, value)
	case c > 0:
		n.right, inserted = avlInsert(n.right, key, value)
	default:
		n.value = value // upsert
	}
	return avlRebalance(n), inserted
}

// Insert adds or updates a key-value pair and rebalances.
func (t *AVL[K, V]) Insert(key K, value V) {
	var inserted bool
	t.root, inserted = avlInsert(t.root, key, value)
	if inserted {
		t.len++
	}
}

/* ---- search ---------------------------------------------------------- */

// Search returns the value and true if the key exists.
func (t *AVL[K, V]) Search(key K) (V, bool) {
	n := t.root
	for n != nil {
		c := cmp.Compare(key, n.key)
		switch {
		case c < 0:
			n = n.left
		case c > 0:
			n = n.right
		default:
			return n.value, true
		}
	}
	var zero V
	return zero, false
}

/* ---- delete ---------------------------------------------------------- */

func avlMinNode[K cmp.Ordered, V any](n *avlNode[K, V]) *avlNode[K, V] {
	for n.left != nil {
		n = n.left
	}
	return n
}

func avlDelete[K cmp.Ordered, V any](n *avlNode[K, V], key K) (*avlNode[K, V], bool) {
	if n == nil {
		return nil, false
	}
	c := cmp.Compare(key, n.key)
	deleted := false
	switch {
	case c < 0:
		n.left, deleted = avlDelete(n.left, key)
	case c > 0:
		n.right, deleted = avlDelete(n.right, key)
	default:
		deleted = true
		if n.left == nil {
			return n.right, true
		}
		if n.right == nil {
			return n.left, true
		}
		succ := avlMinNode(n.right)
		n.key = succ.key
		n.value = succ.value
		n.right, _ = avlDelete(n.right, succ.key)
	}
	if !deleted {
		return n, false
	}
	return avlRebalance(n), true
}

// Delete removes a key and rebalances. Returns true if the key was present.
func (t *AVL[K, V]) Delete(key K) bool {
	var deleted bool
	t.root, deleted = avlDelete(t.root, key)
	if deleted {
		t.len--
	}
	return deleted
}

// Contains reports whether the key is present.
func (t *AVL[K, V]) Contains(key K) bool {
	_, ok := t.Search(key)
	return ok
}

/* ---- min / max ------------------------------------------------------- */

// Min returns the minimum key and its value. ok is false for an empty tree.
func (t *AVL[K, V]) Min() (k K, v V, ok bool) {
	if t.root == nil {
		return
	}
	n := avlMinNode(t.root)
	return n.key, n.value, true
}

// Max returns the maximum key and its value. ok is false for an empty tree.
func (t *AVL[K, V]) Max() (k K, v V, ok bool) {
	if t.root == nil {
		return
	}
	n := t.root
	for n.right != nil {
		n = n.right
	}
	return n.key, n.value, true
}

/* ---- traversal ------------------------------------------------------- */

func avlInOrder[K cmp.Ordered, V any](n *avlNode[K, V], fn func(K, V)) {
	if n == nil {
		return
	}
	avlInOrder(n.left, fn)
	fn(n.key, n.value)
	avlInOrder(n.right, fn)
}

// InOrder calls fn for each node in ascending key order.
func (t *AVL[K, V]) InOrder(fn func(K, V)) {
	avlInOrder(t.root, fn)
}

// InOrderChan returns a channel that yields KV pairs in ascending key order.
func (t *AVL[K, V]) InOrderChan() <-chan KV[K, V] {
	ch := make(chan KV[K, V])
	go func() {
		t.InOrder(func(k K, v V) { ch <- KV[K, V]{k, v} })
		close(ch)
	}()
	return ch
}

/* ---- stats ----------------------------------------------------------- */

// Len returns the number of nodes.
func (t *AVL[K, V]) Len() int { return t.len }

// Height returns the tree height (0 for empty).
func (t *AVL[K, V]) Height() int { return int(avlHeight(t.root)) }

// Empty reports whether the tree has no nodes.
func (t *AVL[K, V]) Empty() bool { return t.len == 0 }

/* ---- invariant checks (test helpers) --------------------------------- */

func avlIsBalanced[K cmp.Ordered, V any](n *avlNode[K, V]) bool {
	if n == nil {
		return true
	}
	bf := avlBF(n)
	if bf < -1 || bf > 1 {
		return false
	}
	return avlIsBalanced(n.left) && avlIsBalanced(n.right)
}

// IsBalanced verifies every node has |BF| ≤ 1.
func (t *AVL[K, V]) IsBalanced() bool { return avlIsBalanced(t.root) }

func avlIsValidBST[K cmp.Ordered, V any](n *avlNode[K, V], lo, hi *K) bool {
	if n == nil {
		return true
	}
	if lo != nil && cmp.Compare(n.key, *lo) <= 0 {
		return false
	}
	if hi != nil && cmp.Compare(n.key, *hi) >= 0 {
		return false
	}
	return avlIsValidBST(n.left, lo, &n.key) && avlIsValidBST(n.right, &n.key, hi)
}

// IsValidBST verifies the BST search property.
func (t *AVL[K, V]) IsValidBST() bool { return avlIsValidBST(t.root, nil, nil) }

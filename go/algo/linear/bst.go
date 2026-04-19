package linear

import "cmp"

// KV holds a key-value pair yielded by BST traversal.
type KV[K, V any] struct {
	Key   K
	Value V
}

// ------------------------------------------------------------------ //
//  Internal node                                                       //
// ------------------------------------------------------------------ //

type bstNode[K cmp.Ordered, V any] struct {
	key   K
	value V
	left  *bstNode[K, V]
	right *bstNode[K, V]
}

// ------------------------------------------------------------------ //
//  BST                                                                 //
// ------------------------------------------------------------------ //

// BST is a non-self-balancing binary search tree.
// Keys must satisfy cmp.Ordered. Duplicate keys update the value (upsert).
type BST[K cmp.Ordered, V any] struct {
	root *bstNode[K, V]
	len  int
}

// NewBST returns an empty BST.
func NewBST[K cmp.Ordered, V any]() *BST[K, V] {
	return &BST[K, V]{}
}

// ------------------------------------------------------------------ //
//  Core operations                                                     //
// ------------------------------------------------------------------ //

// Insert inserts key → value, or updates value if key already exists.
func (t *BST[K, V]) Insert(key K, value V) {
	t.root = bstInsert(t.root, key, value, &t.len)
}

func bstInsert[K cmp.Ordered, V any](n *bstNode[K, V], key K, value V, lenPtr *int) *bstNode[K, V] {
	if n == nil {
		*lenPtr++
		return &bstNode[K, V]{key: key, value: value}
	}
	switch cmp.Compare(key, n.key) {
	case -1:
		n.left = bstInsert(n.left, key, value, lenPtr)
	case 1:
		n.right = bstInsert(n.right, key, value, lenPtr)
	default:
		n.value = value
	}
	return n
}

// Search returns the value for key, and true if found.
func (t *BST[K, V]) Search(key K) (V, bool) {
	n := t.root
	for n != nil {
		switch cmp.Compare(key, n.key) {
		case -1:
			n = n.left
		case 1:
			n = n.right
		default:
			return n.value, true
		}
	}
	var zero V
	return zero, false
}

// Delete removes the node with key. Returns true if the key was present.
func (t *BST[K, V]) Delete(key K) bool {
	var deleted bool
	t.root, deleted = bstDelete(t.root, key)
	if deleted {
		t.len--
	}
	return deleted
}

func bstDelete[K cmp.Ordered, V any](n *bstNode[K, V], key K) (*bstNode[K, V], bool) {
	if n == nil {
		return nil, false
	}
	var deleted bool
	switch cmp.Compare(key, n.key) {
	case -1:
		n.left, deleted = bstDelete(n.left, key)
	case 1:
		n.right, deleted = bstDelete(n.right, key)
	default:
		deleted = true
		if n.left == nil {
			return n.right, true
		}
		if n.right == nil {
			return n.left, true
		}
		// Two children: replace with in-order successor (min of right subtree).
		succ := bstMinNode(n.right)
		n.key, n.value = succ.key, succ.value
		n.right, _ = bstDelete(n.right, succ.key)
	}
	return n, deleted
}

// Contains returns true if key exists in the tree.
func (t *BST[K, V]) Contains(key K) bool {
	_, ok := t.Search(key)
	return ok
}

// Min returns the minimum key-value pair and true, or zero values and false for an empty tree.
func (t *BST[K, V]) Min() (K, V, bool) {
	if t.root == nil {
		var zk K
		var zv V
		return zk, zv, false
	}
	n := bstMinNode(t.root)
	return n.key, n.value, true
}

// Max returns the maximum key-value pair and true, or zero values and false for an empty tree.
func (t *BST[K, V]) Max() (K, V, bool) {
	if t.root == nil {
		var zk K
		var zv V
		return zk, zv, false
	}
	n := bstMaxNode(t.root)
	return n.key, n.value, true
}

// Successor returns the in-order successor of key.
func (t *BST[K, V]) Successor(key K) (K, V, bool) {
	var succ *bstNode[K, V]
	n := t.root
	for n != nil {
		switch cmp.Compare(key, n.key) {
		case -1:
			succ = n
			n = n.left
		case 1:
			n = n.right
		default:
			if n.right != nil {
				s := bstMinNode(n.right)
				return s.key, s.value, true
			}
			if succ != nil {
				return succ.key, succ.value, true
			}
			var zk K
			var zv V
			return zk, zv, false
		}
	}
	var zk K
	var zv V
	return zk, zv, false
}

// Predecessor returns the in-order predecessor of key.
func (t *BST[K, V]) Predecessor(key K) (K, V, bool) {
	var pred *bstNode[K, V]
	n := t.root
	for n != nil {
		switch cmp.Compare(key, n.key) {
		case 1:
			pred = n
			n = n.right
		case -1:
			n = n.left
		default:
			if n.left != nil {
				p := bstMaxNode(n.left)
				return p.key, p.value, true
			}
			if pred != nil {
				return pred.key, pred.value, true
			}
			var zk K
			var zv V
			return zk, zv, false
		}
	}
	var zk K
	var zv V
	return zk, zv, false
}

// ------------------------------------------------------------------ //
//  Traversal                                                           //
// ------------------------------------------------------------------ //

// InOrder visits nodes in ascending key order.
func (t *BST[K, V]) InOrder(fn func(K, V)) { bstInOrder(t.root, fn) }

func bstInOrder[K cmp.Ordered, V any](n *bstNode[K, V], fn func(K, V)) {
	if n == nil {
		return
	}
	bstInOrder(n.left, fn)
	fn(n.key, n.value)
	bstInOrder(n.right, fn)
}

// PreOrder visits nodes in pre-order (root, left, right).
func (t *BST[K, V]) PreOrder(fn func(K, V)) { bstPreOrder(t.root, fn) }

func bstPreOrder[K cmp.Ordered, V any](n *bstNode[K, V], fn func(K, V)) {
	if n == nil {
		return
	}
	fn(n.key, n.value)
	bstPreOrder(n.left, fn)
	bstPreOrder(n.right, fn)
}

// PostOrder visits nodes in post-order (left, right, root).
func (t *BST[K, V]) PostOrder(fn func(K, V)) { bstPostOrder(t.root, fn) }

func bstPostOrder[K cmp.Ordered, V any](n *bstNode[K, V], fn func(K, V)) {
	if n == nil {
		return
	}
	bstPostOrder(n.left, fn)
	bstPostOrder(n.right, fn)
	fn(n.key, n.value)
}

// LevelOrder visits nodes in BFS order (level by level).
func (t *BST[K, V]) LevelOrder(fn func(K, V)) {
	if t.root == nil {
		return
	}
	queue := []*bstNode[K, V]{t.root}
	for len(queue) > 0 {
		n := queue[0]
		queue = queue[1:]
		fn(n.key, n.value)
		if n.left != nil {
			queue = append(queue, n.left)
		}
		if n.right != nil {
			queue = append(queue, n.right)
		}
	}
}

// InOrderChan returns a channel that yields KV pairs in ascending key order.
// The goroutine exits when all nodes are sent or the returned channel is drained.
func (t *BST[K, V]) InOrderChan() <-chan KV[K, V] {
	ch := make(chan KV[K, V])
	go func() {
		defer close(ch)
		bstInOrderChan(t.root, ch)
	}()
	return ch
}

func bstInOrderChan[K cmp.Ordered, V any](n *bstNode[K, V], ch chan<- KV[K, V]) {
	if n == nil {
		return
	}
	bstInOrderChan(n.left, ch)
	ch <- KV[K, V]{Key: n.key, Value: n.value}
	bstInOrderChan(n.right, ch)
}

// ------------------------------------------------------------------ //
//  Statistics                                                          //
// ------------------------------------------------------------------ //

// Len returns the number of nodes.
func (t *BST[K, V]) Len() int { return t.len }

// IsEmpty returns true if the tree is empty.
func (t *BST[K, V]) IsEmpty() bool { return t.len == 0 }

// Height returns the tree height (0 for empty tree).
func (t *BST[K, V]) Height() int { return bstHeight(t.root) }

func bstHeight[K cmp.Ordered, V any](n *bstNode[K, V]) int {
	if n == nil {
		return 0
	}
	l, r := bstHeight(n.left), bstHeight(n.right)
	if l > r {
		return l + 1
	}
	return r + 1
}

// IsValid verifies the BST invariant — for use in tests.
func (t *BST[K, V]) IsValid() bool {
	return bstIsValid[K, V](t.root, nil, nil)
}

func bstIsValid[K cmp.Ordered, V any](n *bstNode[K, V], lo, hi *K) bool {
	if n == nil {
		return true
	}
	if lo != nil && cmp.Compare(n.key, *lo) <= 0 {
		return false
	}
	if hi != nil && cmp.Compare(n.key, *hi) >= 0 {
		return false
	}
	return bstIsValid(n.left, lo, &n.key) && bstIsValid(n.right, &n.key, hi)
}

// ------------------------------------------------------------------ //
//  Internal helpers                                                    //
// ------------------------------------------------------------------ //

func bstMinNode[K cmp.Ordered, V any](n *bstNode[K, V]) *bstNode[K, V] {
	for n.left != nil {
		n = n.left
	}
	return n
}

func bstMaxNode[K cmp.Ordered, V any](n *bstNode[K, V]) *bstNode[K, V] {
	for n.right != nil {
		n = n.right
	}
	return n
}

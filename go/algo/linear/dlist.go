package linear

import "iter"

// DListError is the sentinel error type for DList operations.
type DListError int

const (
	DLOk       DListError = 0
	DLErrEmpty DListError = 1
	DLErrOOB   DListError = 2
)

func (e DListError) Error() string {
	switch e {
	case DLErrEmpty:
		return "dlist: empty"
	case DLErrOOB:
		return "dlist: index out of bounds"
	default:
		return "dlist: unknown error"
	}
}

// DListNode is a doubly-linked node.  Val is exported for external cursors;
// prev and next are unexported to preserve invariants.
type DListNode[T any] struct {
	Val  T
	prev *DListNode[T]
	next *DListNode[T]
}

// DList is a doubly-linked list with a single circular sentinel (dummy head).
//
// Invariant (empty):   dummy.prev == dummy && dummy.next == dummy
// Invariant (non-empty): dummy.next == head, dummy.prev == tail
type DList[T any] struct {
	dummy *DListNode[T]
	len   int
}

// NewDList returns an empty DList.
func NewDList[T any]() *DList[T] {
	d := new(DListNode[T])
	d.prev = d
	d.next = d
	return &DList[T]{dummy: d}
}

// ------------------------------------------------------------------ //
// Internal primitives                                                  //
// ------------------------------------------------------------------ //

// linkBefore inserts node immediately before ref, wiring all 4 pointers.
func linkBefore[T any](ref, node *DListNode[T]) {
	node.prev = ref.prev
	node.next = ref
	ref.prev.next = node
	ref.prev = node
}

// unlink detaches node from its neighbours without freeing anything.
func unlink[T any](node *DListNode[T]) {
	node.prev.next = node.next
	node.next.prev = node.prev
}

// ------------------------------------------------------------------ //
// Capacity                                                             //
// ------------------------------------------------------------------ //

// Len returns the number of elements.
func (l *DList[T]) Len() int { return l.len }

// IsEmpty reports whether the list contains no elements.
func (l *DList[T]) IsEmpty() bool { return l.len == 0 }

// ------------------------------------------------------------------ //
// Push / pop                                                           //
// ------------------------------------------------------------------ //

// PushFront inserts val at the front of the list.
func (l *DList[T]) PushFront(val T) {
	n := &DListNode[T]{Val: val}
	linkBefore(l.dummy.next, n)
	l.len++
}

// PushBack appends val at the back of the list.
func (l *DList[T]) PushBack(val T) {
	n := &DListNode[T]{Val: val}
	linkBefore(l.dummy, n)
	l.len++
}

// PopFront removes and returns the front element.
func (l *DList[T]) PopFront() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, DLErrEmpty
	}
	n := l.dummy.next
	val := n.Val
	unlink(n)
	n.next = nil // assist GC
	n.prev = nil
	l.len--
	return val, nil
}

// PopBack removes and returns the back element.
func (l *DList[T]) PopBack() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, DLErrEmpty
	}
	n := l.dummy.prev
	val := n.Val
	unlink(n)
	n.next = nil
	n.prev = nil
	l.len--
	return val, nil
}

// ------------------------------------------------------------------ //
// Peek                                                                 //
// ------------------------------------------------------------------ //

// PeekFront returns the front value without removing it.
func (l *DList[T]) PeekFront() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, DLErrEmpty
	}
	return l.dummy.next.Val, nil
}

// PeekBack returns the back value without removing it.
func (l *DList[T]) PeekBack() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, DLErrEmpty
	}
	return l.dummy.prev.Val, nil
}

// ------------------------------------------------------------------ //
// Random access                                                        //
// ------------------------------------------------------------------ //

// Get returns the element at index i (0-based).
// Traverses from the nearer end: O(n/2).
func (l *DList[T]) Get(i int) (T, error) {
	if i < 0 || i >= l.len {
		var zero T
		return zero, DLErrOOB
	}
	var curr *DListNode[T]
	if i < l.len/2 {
		curr = l.dummy.next
		for k := 0; k < i; k++ {
			curr = curr.next
		}
	} else {
		curr = l.dummy.prev
		for k := l.len - 1; k > i; k-- {
			curr = curr.prev
		}
	}
	return curr.Val, nil
}

// ------------------------------------------------------------------ //
// Node-relative operations (for external cursor use)                  //
// ------------------------------------------------------------------ //

// InsertBefore inserts val immediately before node.
func (l *DList[T]) InsertBefore(node *DListNode[T], val T) {
	n := &DListNode[T]{Val: val}
	linkBefore(node, n)
	l.len++
}

// InsertAfter inserts val immediately after node.
func (l *DList[T]) InsertAfter(node *DListNode[T], val T) {
	n := &DListNode[T]{Val: val}
	linkBefore(node.next, n)
	l.len++
}

// Remove detaches node from the list and returns its value.
func (l *DList[T]) Remove(node *DListNode[T]) T {
	val := node.Val
	unlink(node)
	node.next = nil
	node.prev = nil
	l.len--
	return val
}

// Find returns the first node satisfying pred, or nil.
func (l *DList[T]) Find(pred func(T) bool) *DListNode[T] {
	for curr := l.dummy.next; curr != l.dummy; curr = curr.next {
		if pred(curr.Val) {
			return curr
		}
	}
	return nil
}

// Dummy returns the sentinel node (useful for cursor-based iteration).
func (l *DList[T]) Dummy() *DListNode[T] { return l.dummy }

// ------------------------------------------------------------------ //
// Iteration                                                            //
// ------------------------------------------------------------------ //

// ForEach calls fn on each element front-to-back.
func (l *DList[T]) ForEach(fn func(T)) {
	for curr := l.dummy.next; curr != l.dummy; curr = curr.next {
		fn(curr.Val)
	}
}

// ForEachRev calls fn on each element back-to-front.
func (l *DList[T]) ForEachRev(fn func(T)) {
	for curr := l.dummy.prev; curr != l.dummy; curr = curr.prev {
		fn(curr.Val)
	}
}

// All returns an iter.Seq[T] over elements front-to-back.
func (l *DList[T]) All() iter.Seq[T] {
	return func(yield func(T) bool) {
		for curr := l.dummy.next; curr != l.dummy; curr = curr.next {
			if !yield(curr.Val) {
				return
			}
		}
	}
}

// Backward returns an iter.Seq[T] over elements back-to-front.
func (l *DList[T]) Backward() iter.Seq[T] {
	return func(yield func(T) bool) {
		for curr := l.dummy.prev; curr != l.dummy; curr = curr.prev {
			if !yield(curr.Val) {
				return
			}
		}
	}
}

// ------------------------------------------------------------------ //
// Mutation                                                             //
// ------------------------------------------------------------------ //

// Clear removes all elements.
func (l *DList[T]) Clear() {
	curr := l.dummy.next
	for curr != l.dummy {
		nxt := curr.next
		curr.next = nil
		curr.prev = nil
		curr = nxt
	}
	l.dummy.prev = l.dummy
	l.dummy.next = l.dummy
	l.len = 0
}

// Reverse reverses the list in-place in O(n).
func (l *DList[T]) Reverse() {
	curr := l.dummy
	for {
		curr.prev, curr.next = curr.next, curr.prev
		curr = curr.prev // was curr.next before swap
		if curr == l.dummy {
			break
		}
	}
}

// Splice moves all elements from src into l immediately before pos.
// pos must be a node belonging to l (use l.Dummy() to append).
// src becomes empty after the call.  O(1).
func (l *DList[T]) Splice(pos *DListNode[T], src *DList[T]) {
	if src.len == 0 {
		return
	}
	srcFirst := src.dummy.next
	srcLast := src.dummy.prev

	// Detach src chain from its sentinel.
	src.dummy.next = src.dummy
	src.dummy.prev = src.dummy

	// Wire before pos.
	srcFirst.prev = pos.prev
	srcLast.next = pos
	pos.prev.next = srcFirst
	pos.prev = srcLast

	l.len += src.len
	src.len = 0
}

package linear

// ------------------------------------------------------------------ //
//  错误类型                                                            //
// ------------------------------------------------------------------ //

// SListError 是 SList 操作返回的错误类型。
type SListError int

const (
	SLErrEmpty       SListError = iota + 1 // 对空链表 pop/peek
	SLErrOutOfBounds                       // 索引越界
	SLErrAlloc                             // 内存分配失败（通常不触发）
)

func (e SListError) Error() string {
	switch e {
	case SLErrEmpty:
		return "list is empty"
	case SLErrOutOfBounds:
		return "index out of bounds"
	case SLErrAlloc:
		return "memory allocation failed"
	default:
		return "unknown slist error"
	}
}

// ------------------------------------------------------------------ //
//  节点与链表结构                                                      //
// ------------------------------------------------------------------ //

// SListNode 是单链表节点。Val 字段对外可读写；next 为私有。
type SListNode[T any] struct {
	Val  T
	next *SListNode[T]
}

// SList 是带哨兵头节点和 tail 指针的单向链表。
//
// 结构不变量：
//   - dummy 始终非 nil，dummy.Val 为 T 的零值（忽略）。
//   - 空链表时 tail == dummy。
//   - len 等于真实节点数量。
type SList[T any] struct {
	dummy *SListNode[T]
	tail  *SListNode[T]
	len   int
}

// NewSList 创建空单链表。
func NewSList[T any]() *SList[T] {
	dummy := &SListNode[T]{}
	return &SList[T]{dummy: dummy, tail: dummy}
}

// ------------------------------------------------------------------ //
//  基本读写                                                            //
// ------------------------------------------------------------------ //

// PushFront 在头部插入 val；O(1)。
func (l *SList[T]) PushFront(val T) {
	node := &SListNode[T]{Val: val, next: l.dummy.next}
	l.dummy.next = node
	if l.tail == l.dummy {
		l.tail = node
	}
	l.len++
}

// PushBack 在尾部追加 val；O(1)。
func (l *SList[T]) PushBack(val T) {
	node := &SListNode[T]{Val: val}
	l.tail.next = node
	l.tail = node
	l.len++
}

// PopFront 移除并返回头部元素；空链表返回 SLErrEmpty。O(1)。
func (l *SList[T]) PopFront() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, SLErrEmpty
	}
	node := l.dummy.next
	l.dummy.next = node.next
	if node == l.tail {
		l.tail = l.dummy
	}
	node.next = nil // 断链，辅助 GC
	l.len--
	return node.Val, nil
}

// PopBack 移除并返回尾部元素；空链表返回 SLErrEmpty。O(n)。
func (l *SList[T]) PopBack() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, SLErrEmpty
	}
	// 找 tail 的前驱
	prev := l.dummy
	for prev.next != l.tail {
		prev = prev.next
	}
	val := l.tail.Val
	prev.next = nil
	l.tail = prev
	l.len--
	return val, nil
}

// PeekFront 返回头部元素的副本，不移除；O(1)。
func (l *SList[T]) PeekFront() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, SLErrEmpty
	}
	return l.dummy.next.Val, nil
}

// PeekBack 返回尾部元素的副本，不移除；O(1)。
func (l *SList[T]) PeekBack() (T, error) {
	if l.len == 0 {
		var zero T
		return zero, SLErrEmpty
	}
	return l.tail.Val, nil
}

// InsertAfter 在 node 之后插入 val；node 必须属于该链表；O(1)。
// 传入 dummy 节点（通过 Dummy() 获取）等价于 PushFront。
func (l *SList[T]) InsertAfter(node *SListNode[T], val T) {
	new_node := &SListNode[T]{Val: val, next: node.next}
	node.next = new_node
	if node == l.tail {
		l.tail = new_node
	}
	l.len++
}

// RemoveAfter 移除 node 之后的节点，返回其值。
// 若 node 为尾节点（无后继），返回 SLErrOutOfBounds；O(1)。
func (l *SList[T]) RemoveAfter(node *SListNode[T]) (T, error) {
	if node.next == nil {
		var zero T
		return zero, SLErrOutOfBounds
	}
	target := node.next
	node.next = target.next
	if target == l.tail {
		l.tail = node
	}
	target.next = nil // 辅助 GC
	l.len--
	return target.Val, nil
}

// Get 返回逻辑索引 i 处元素的副本（0 = 头节点）；O(n)。
func (l *SList[T]) Get(i int) (T, error) {
	if i < 0 || i >= l.len {
		var zero T
		return zero, SLErrOutOfBounds
	}
	curr := l.dummy.next
	for range i {
		curr = curr.next
	}
	return curr.Val, nil
}

// Find 返回第一个满足 pred 的节点；未找到返回 nil；O(n)。
func (l *SList[T]) Find(pred func(T) bool) *SListNode[T] {
	for curr := l.dummy.next; curr != nil; curr = curr.next {
		if pred(curr.Val) {
			return curr
		}
	}
	return nil
}

// ------------------------------------------------------------------ //
//  容量管理                                                            //
// ------------------------------------------------------------------ //

// Len 返回当前节点数量；O(1)。
func (l *SList[T]) Len() int { return l.len }

// IsEmpty 返回链表是否为空。
func (l *SList[T]) IsEmpty() bool { return l.len == 0 }

// Clear 移除所有真实节点；O(n)。
func (l *SList[T]) Clear() {
	l.dummy.next = nil
	l.tail = l.dummy
	l.len = 0
}

// ------------------------------------------------------------------ //
//  遍历                                                                //
// ------------------------------------------------------------------ //

// ForEach 对每个节点的值调用 fn，从头到尾遍历；O(n)。
func (l *SList[T]) ForEach(fn func(T)) {
	for curr := l.dummy.next; curr != nil; curr = curr.next {
		fn(curr.Val)
	}
}

// Reverse 原地反转链表；O(n)。
func (l *SList[T]) Reverse() {
	if l.len <= 1 {
		return
	}
	newTail := l.dummy.next // 当前头节点将成为新尾节点
	var prev *SListNode[T]
	curr := l.dummy.next
	for curr != nil {
		next := curr.next
		curr.next = prev
		prev = curr
		curr = next
	}
	l.dummy.next = prev // prev 是原尾节点，成为新头节点
	newTail.next = nil
	l.tail = newTail
}

// ------------------------------------------------------------------ //
//  辅助：暴露哨兵节点（供 InsertAfter / RemoveAfter 游标操作使用）    //
// ------------------------------------------------------------------ //

// Dummy 返回哨兵节点指针。InsertAfter(Dummy(), val) 等价于 PushFront。
func (l *SList[T]) Dummy() *SListNode[T] { return l.dummy }

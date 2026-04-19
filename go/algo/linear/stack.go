package linear

// ------------------------------------------------------------------ //
//  错误类型                                                            //
// ------------------------------------------------------------------ //

// StackError 是 Stack 操作返回的错误类型。
type StackError int

const (
	StackErrEmpty StackError = iota + 1 // pop/peek 时栈为空
)

func (e StackError) Error() string {
	switch e {
	case StackErrEmpty:
		return "stack is empty"
	default:
		return "unknown stack error"
	}
}

// ================================================================== //
//  ArrayStack[T] — 基于 Vec[T]                                        //
// ================================================================== //

// ArrayStack 是基于动态数组的栈，连续内存，缓存友好。
// 栈顶为切片末尾，Push/Pop 均为均摊 O(1)。
type ArrayStack[T any] struct {
	base Vec[T]
}

// NewArrayStack 创建一个空的数组栈，可通过选项配置初始容量和扩容策略。
func NewArrayStack[T any](opts ...Option) *ArrayStack[T] {
	s := &ArrayStack[T]{}
	cfg := &vecConfig{initCap: 4, growFn: defaultGrow}
	for _, opt := range opts {
		opt(cfg)
	}
	s.base = Vec[T]{
		data:   make([]T, 0, cfg.initCap),
		growFn: cfg.growFn,
	}
	return s
}

// Push 将 val 压入栈顶；均摊 O(1)。
func (s *ArrayStack[T]) Push(val T) {
	s.base.Push(val) //nolint:errcheck // Vec.Push 永远不返回错误
}

// Pop 弹出栈顶元素；栈为空时返回 StackErrEmpty。O(1)。
func (s *ArrayStack[T]) Pop() (T, error) {
	val, err := s.base.Pop()
	if err == ErrEmpty {
		return val, StackErrEmpty
	}
	return val, nil
}

// Peek 返回栈顶元素（不移除）；栈为空时返回 StackErrEmpty。O(1)。
func (s *ArrayStack[T]) Peek() (T, error) {
	if s.base.IsEmpty() {
		var zero T
		return zero, StackErrEmpty
	}
	val, _ := s.base.Get(s.base.Len() - 1)
	return val, nil
}

// Len 返回栈中元素数量。
func (s *ArrayStack[T]) Len() int { return s.base.Len() }

// IsEmpty 返回栈是否为空。
func (s *ArrayStack[T]) IsEmpty() bool { return s.base.IsEmpty() }

// Clear 清空所有元素，容量不变。
func (s *ArrayStack[T]) Clear() { s.base.Clear() }

// Reserve 预留至少 additional 个额外槽位，避免扩容抖动。
func (s *ArrayStack[T]) Reserve(additional int) { s.base.Reserve(additional) } //nolint:errcheck

// ShrinkToFit 将底层容量收缩至当前元素数量。
func (s *ArrayStack[T]) ShrinkToFit() { s.base.ShrinkToFit() }

// ================================================================== //
//  ListStack[T] — 基于轻量单链表                                       //
// ================================================================== //

type stackNode[T any] struct {
	val  T
	next *stackNode[T]
}

// ListStack 是基于节点单链表的栈。
// 每次 Push/Pop 动态分配/释放节点，无需预分配，适合元素大小不定或
// 频繁清空重建的场景。
type ListStack[T any] struct {
	top *stackNode[T]
	len int
}

// NewListStack 创建一个空的链表栈。
func NewListStack[T any]() *ListStack[T] {
	return &ListStack[T]{}
}

// Push 将 val 压入栈顶；O(1)。
func (s *ListStack[T]) Push(val T) {
	s.top = &stackNode[T]{val: val, next: s.top}
	s.len++
}

// Pop 弹出栈顶元素；栈为空时返回 StackErrEmpty。O(1)。
func (s *ListStack[T]) Pop() (T, error) {
	if s.top == nil {
		var zero T
		return zero, StackErrEmpty
	}
	node := s.top
	s.top = node.next
	s.len--
	return node.val, nil
}

// Peek 返回栈顶元素（不移除）；栈为空时返回 StackErrEmpty。O(1)。
func (s *ListStack[T]) Peek() (T, error) {
	if s.top == nil {
		var zero T
		return zero, StackErrEmpty
	}
	return s.top.val, nil
}

// Len 返回栈中元素数量。
func (s *ListStack[T]) Len() int { return s.len }

// IsEmpty 返回栈是否为空。
func (s *ListStack[T]) IsEmpty() bool { return s.top == nil }

// Clear 清空所有节点。
func (s *ListStack[T]) Clear() {
	s.top = nil
	s.len = 0
}

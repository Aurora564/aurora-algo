package linear

import "iter"

// ------------------------------------------------------------------ //
//  错误类型                                                            //
// ------------------------------------------------------------------ //

// QueueError 是 Queue 操作返回的错误类型。
type QueueError int

const (
	QueueErrEmpty QueueError = iota + 1 // dequeue/peek 时队列为空
)

func (e QueueError) Error() string {
	switch e {
	case QueueErrEmpty:
		return "queue is empty"
	default:
		return "unknown queue error"
	}
}

// ================================================================== //
//  Queue[T] — 基于双链表（DList）的 FIFO 队列                          //
// ================================================================== //

// Queue 是基于双链表的队列，遵循 FIFO 语义：
// Enqueue 追加到尾部，Dequeue 从头部移除，两者均为 O(1)。
type Queue[T any] struct {
	base *DList[T]
}

// NewQueue 创建一个空队列。
func NewQueue[T any]() *Queue[T] {
	return &Queue[T]{base: NewDList[T]()}
}

// Enqueue 将 val 追加到队尾；O(1)。
func (q *Queue[T]) Enqueue(val T) {
	q.base.PushBack(val)
}

// Dequeue 移除并返回队首元素；队列为空时返回 QueueErrEmpty。O(1)。
func (q *Queue[T]) Dequeue() (T, error) {
	val, err := q.base.PopFront()
	if err == DLErrEmpty {
		return val, QueueErrEmpty
	}
	return val, nil
}

// PeekFront 返回队首元素（不移除）；队列为空时返回 QueueErrEmpty。O(1)。
func (q *Queue[T]) PeekFront() (T, error) {
	val, err := q.base.PeekFront()
	if err == DLErrEmpty {
		return val, QueueErrEmpty
	}
	return val, nil
}

// PeekBack 返回队尾元素（不移除）；队列为空时返回 QueueErrEmpty。O(1)。
func (q *Queue[T]) PeekBack() (T, error) {
	val, err := q.base.PeekBack()
	if err == DLErrEmpty {
		return val, QueueErrEmpty
	}
	return val, nil
}

// Len 返回队列中的元素数量。
func (q *Queue[T]) Len() int { return q.base.Len() }

// IsEmpty 报告队列是否为空。
func (q *Queue[T]) IsEmpty() bool { return q.base.IsEmpty() }

// Clear 清空所有元素。
func (q *Queue[T]) Clear() { q.base.Clear() }

// ForEach 从队首到队尾遍历所有元素，不出队。
func (q *Queue[T]) ForEach(fn func(T)) { q.base.ForEach(fn) }

// All 返回从队首到队尾的 iter.Seq[T]（Go 1.23+）。
func (q *Queue[T]) All() iter.Seq[T] { return q.base.All() }

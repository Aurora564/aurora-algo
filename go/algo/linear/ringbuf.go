package linear

// ------------------------------------------------------------------ //
//  错误类型                                                            //
// ------------------------------------------------------------------ //

// RBError 是 RingBuf 操作返回的错误类型。
type RBError int

const (
	RBErrEmpty         RBError = iota + 1 // dequeue/peek 时缓冲区为空
	RBErrFull                             // 有界模式 enqueue 时已满
	RBErrOutOfBounds                      // get(i) 且 i >= len
	RBErrInvalidConfig                    // 初始容量为 0
)

func (e RBError) Error() string {
	switch e {
	case RBErrEmpty:
		return "ring buffer is empty"
	case RBErrFull:
		return "ring buffer is full"
	case RBErrOutOfBounds:
		return "ring buffer index out of bounds"
	case RBErrInvalidConfig:
		return "ring buffer invalid config: cap must be > 0"
	default:
		return "unknown ring buffer error"
	}
}

// ================================================================== //
//  RingBuf[T] — 环形缓冲区                                            //
// ================================================================== //

// RingBuf 是基于连续数组的环形缓冲区，遵循 FIFO 语义。
//
//   - 有界模式（growable=false）：满时 Enqueue 返回 RBErrFull。
//   - 可扩容模式（growable=true）：满时自动 ×2 扩容，容量始终为 2 的幂。
//
// 使用 len 计数器区分空/满，不浪费一个槽位。
// head 指向队首元素的物理索引，tail 指向下一个入队位置的物理索引。
type RingBuf[T any] struct {
	data     []T
	head     int
	tail     int
	len      int
	growable bool
}

// NewRingBuf 创建容量为 cap 的有界环形缓冲区。
func NewRingBuf[T any](cap int) (*RingBuf[T], error) {
	if cap <= 0 {
		return nil, RBErrInvalidConfig
	}
	return &RingBuf[T]{data: make([]T, cap)}, nil
}

// NewGrowableRingBuf 创建初始容量为 initCap 的可扩容环形缓冲区。
// initCap 会向上取整到最近的 2 的幂。
func NewGrowableRingBuf[T any](initCap int) (*RingBuf[T], error) {
	if initCap <= 0 {
		return nil, RBErrInvalidConfig
	}
	cap := nextPow2(initCap)
	return &RingBuf[T]{data: make([]T, cap), growable: true}, nil
}

// nextPow2 返回 >= n 的最小 2 的幂（n > 0）。
func nextPow2(n int) int {
	p := 1
	for p < n {
		p <<= 1
	}
	return p
}

// ------------------------------------------------------------------ //
//  内部索引计算                                                        //
// ------------------------------------------------------------------ //

// phys 将逻辑索引（0 = 队首）映射到物理数组索引。
// 可扩容模式下容量为 2 的幂，使用位掩码；有界模式使用取模。
func (rb *RingBuf[T]) phys(logical int) int {
	cap := len(rb.data)
	if rb.growable {
		return (rb.head + logical) & (cap - 1)
	}
	return (rb.head + logical) % cap
}

// ------------------------------------------------------------------ //
//  扩容                                                                //
// ------------------------------------------------------------------ //

// grow 将容量翻倍，并将元素重排到新切片的 [0, old_len-1]。
func (rb *RingBuf[T]) grow() {
	oldCap := len(rb.data)
	newCap := oldCap * 2
	newData := make([]T, newCap)

	// 两次 copy 重排：[head..oldCap-1] 然后 [0..tail-1]
	firstLen := oldCap - rb.head
	copy(newData, rb.data[rb.head:])
	copy(newData[firstLen:], rb.data[:rb.tail])

	rb.data = newData
	rb.head = 0
	rb.tail = rb.len // old_len
}

// ------------------------------------------------------------------ //
//  核心操作                                                            //
// ------------------------------------------------------------------ //

// Enqueue 将 elem 追加到队尾。
// 有界模式满时返回 RBErrFull；可扩容模式满时自动扩容，始终返回 nil。
func (rb *RingBuf[T]) Enqueue(elem T) error {
	if rb.len == len(rb.data) {
		if !rb.growable {
			return RBErrFull
		}
		rb.grow()
	}
	rb.data[rb.tail] = elem
	if rb.growable {
		rb.tail = (rb.tail + 1) & (len(rb.data) - 1)
	} else {
		rb.tail = (rb.tail + 1) % len(rb.data)
	}
	rb.len++
	return nil
}

// Dequeue 移除并返回队首元素；缓冲区为空时返回 RBErrEmpty。O(1)。
func (rb *RingBuf[T]) Dequeue() (T, error) {
	if rb.len == 0 {
		var zero T
		return zero, RBErrEmpty
	}
	val := rb.data[rb.head]
	var zero T
	rb.data[rb.head] = zero // 释放引用，辅助 GC
	if rb.growable {
		rb.head = (rb.head + 1) & (len(rb.data) - 1)
	} else {
		rb.head = (rb.head + 1) % len(rb.data)
	}
	rb.len--
	return val, nil
}

// PeekFront 返回队首元素（不移除）；缓冲区为空时返回 RBErrEmpty。O(1)。
func (rb *RingBuf[T]) PeekFront() (T, error) {
	if rb.len == 0 {
		var zero T
		return zero, RBErrEmpty
	}
	return rb.data[rb.head], nil
}

// PeekBack 返回队尾元素（不移除）；缓冲区为空时返回 RBErrEmpty。O(1)。
func (rb *RingBuf[T]) PeekBack() (T, error) {
	if rb.len == 0 {
		var zero T
		return zero, RBErrEmpty
	}
	return rb.data[rb.phys(rb.len-1)], nil
}

// Get 按逻辑索引返回元素（0 = 队首）；i >= len 时返回 RBErrOutOfBounds。O(1)。
func (rb *RingBuf[T]) Get(i int) (T, error) {
	if i < 0 || i >= rb.len {
		var zero T
		return zero, RBErrOutOfBounds
	}
	return rb.data[rb.phys(i)], nil
}

// ------------------------------------------------------------------ //
//  容量管理                                                            //
// ------------------------------------------------------------------ //

// Len 返回当前元素数量。
func (rb *RingBuf[T]) Len() int { return rb.len }

// Cap 返回缓冲区总容量。
func (rb *RingBuf[T]) Cap() int { return len(rb.data) }

// IsEmpty 报告缓冲区是否为空。
func (rb *RingBuf[T]) IsEmpty() bool { return rb.len == 0 }

// IsFull 报告缓冲区是否已满（有界模式下有意义）。
func (rb *RingBuf[T]) IsFull() bool { return rb.len == len(rb.data) }

// Clear 清空所有元素，head/tail 重置为 0，容量不变。
func (rb *RingBuf[T]) Clear() {
	var zero T
	for i := range rb.data {
		rb.data[i] = zero
	}
	rb.head = 0
	rb.tail = 0
	rb.len = 0
}

// Reserve 确保至少有 additional 个额外的空余槽位（仅可扩容模式有效）。
// 有界模式下不扩容，若当前空余不足则返回 RBErrFull。
func (rb *RingBuf[T]) Reserve(additional int) error {
	avail := len(rb.data) - rb.len
	if avail >= additional {
		return nil
	}
	if !rb.growable {
		return RBErrFull
	}
	needed := rb.len + additional
	for len(rb.data) < needed {
		rb.grow()
	}
	return nil
}

// ------------------------------------------------------------------ //
//  批量操作                                                            //
// ------------------------------------------------------------------ //

// AsSlices 返回两段连续切片，逻辑上 first ++ second 即完整队列内容。
// 数据未跨越数组边界时 second 为空切片。O(1)。
func (rb *RingBuf[T]) AsSlices() (first, second []T) {
	if rb.len == 0 {
		return nil, nil
	}
	cap := len(rb.data)
	if rb.head < rb.tail {
		// 未绕回
		return rb.data[rb.head:rb.tail], nil
	}
	// 绕回：[head..cap-1] ++ [0..tail-1]
	return rb.data[rb.head:cap], rb.data[:rb.tail]
}

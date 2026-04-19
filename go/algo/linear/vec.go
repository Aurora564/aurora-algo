// Package linear 提供基础线性数据结构实现。
package linear

// ------------------------------------------------------------------ //
//  错误类型                                                            //
// ------------------------------------------------------------------ //

// VecError 是 Vec 操作返回的错误类型。
type VecError int

const (
	ErrOutOfBounds VecError = iota + 1 // 索引越界
	ErrEmpty                           // 对空数组操作
	ErrAlloc                           // 内存分配失败（通常不触发）
)

func (e VecError) Error() string {
	switch e {
	case ErrOutOfBounds:
		return "index out of bounds"
	case ErrEmpty:
		return "vector is empty"
	case ErrAlloc:
		return "memory allocation failed"
	default:
		return "unknown vec error"
	}
}

// ------------------------------------------------------------------ //
//  配置与选项                                                          //
// ------------------------------------------------------------------ //

// GrowFn 接收当前容量，返回扩容后的新容量；必须满足 new_cap > cap。
type GrowFn func(cap int) int

type vecConfig struct {
	initCap int
	growFn  GrowFn
}

// Option 是 New 的函数选项。
type Option func(*vecConfig)

// WithInitCap 设置初始容量（默认 4）。
func WithInitCap(n int) Option {
	return func(c *vecConfig) { c.initCap = n }
}

// WithGrowFn 注入自定义扩容策略（默认 ×2）。
func WithGrowFn(fn GrowFn) Option {
	return func(c *vecConfig) { c.growFn = fn }
}

func defaultGrow(cap int) int {
	if cap == 0 {
		return 4
	}
	return cap * 2
}

// ------------------------------------------------------------------ //
//  Vec[T]                                                              //
// ------------------------------------------------------------------ //

// Vec 是泛型动态数组，底层管理连续内存切片并支持可配置扩容策略。
//
// 借用视图安全约束：AsSlice 返回的切片在原数组发生任何扩容操作后将
// 静默失效，调用者有责任在持有视图期间不对原数组执行 Push/Insert/Reserve。
type Vec[T any] struct {
	data   []T
	growFn GrowFn
}

// New 创建一个空的 Vec[T]，可通过选项配置初始容量和扩容策略。
func New[T any](opts ...Option) *Vec[T] {
	cfg := &vecConfig{initCap: 4, growFn: defaultGrow}
	for _, opt := range opts {
		opt(cfg)
	}
	return &Vec[T]{
		data:   make([]T, 0, cfg.initCap),
		growFn: cfg.growFn,
	}
}

// ensureCap 确保底层切片至少有 needed 个元素的容量。
func (v *Vec[T]) ensureCap(needed int) {
	if cap(v.data) >= needed {
		return
	}
	newCap := cap(v.data)
	if newCap == 0 {
		newCap = 4
	}
	for newCap < needed {
		newCap = v.growFn(newCap)
	}
	newData := make([]T, len(v.data), newCap)
	copy(newData, v.data)
	v.data = newData
}

// ------------------------------------------------------------------ //
//  生命周期                                                            //
// ------------------------------------------------------------------ //

// Clone 使用 cloneFn 对每个元素深拷贝，返回完全独立的新 Vec。
func (v *Vec[T]) Clone(cloneFn func(T) T) *Vec[T] {
	dst := &Vec[T]{
		data:   make([]T, len(v.data), cap(v.data)),
		growFn: v.growFn,
	}
	for i, elem := range v.data {
		dst.data[i] = cloneFn(elem)
	}
	return dst
}

// ------------------------------------------------------------------ //
//  基本读写                                                            //
// ------------------------------------------------------------------ //

// Push 追加元素到末尾，必要时按扩容策略扩容；均摊 O(1)。
func (v *Vec[T]) Push(elem T) error {
	v.ensureCap(len(v.data) + 1)
	v.data = v.data[:len(v.data)+1]
	v.data[len(v.data)-1] = elem
	return nil
}

// Pop 移除并返回末尾元素；空数组返回 ErrEmpty。
func (v *Vec[T]) Pop() (T, error) {
	if len(v.data) == 0 {
		var zero T
		return zero, ErrEmpty
	}
	last := v.data[len(v.data)-1]
	var zero T
	v.data[len(v.data)-1] = zero // 清零，辅助 GC
	v.data = v.data[:len(v.data)-1]
	return last, nil
}

// Get 返回索引 i 处的元素副本；越界返回 ErrOutOfBounds。
func (v *Vec[T]) Get(i int) (T, error) {
	if i < 0 || i >= len(v.data) {
		var zero T
		return zero, ErrOutOfBounds
	}
	return v.data[i], nil
}

// Set 覆写索引 i 处的元素；越界返回 ErrOutOfBounds。
func (v *Vec[T]) Set(i int, elem T) error {
	if i < 0 || i >= len(v.data) {
		return ErrOutOfBounds
	}
	v.data[i] = elem
	return nil
}

// Insert 在索引 i 处插入元素，后续元素后移；O(n)。
// i == Len() 等价于 Push。
func (v *Vec[T]) Insert(i int, elem T) error {
	if i < 0 || i > len(v.data) {
		return ErrOutOfBounds
	}
	v.ensureCap(len(v.data) + 1)
	v.data = v.data[:len(v.data)+1]
	copy(v.data[i+1:], v.data[i:])
	v.data[i] = elem
	return nil
}

// Remove 移除索引 i 处的元素，后续元素前移；O(n)。
func (v *Vec[T]) Remove(i int) (T, error) {
	if i < 0 || i >= len(v.data) {
		var zero T
		return zero, ErrOutOfBounds
	}
	val := v.data[i]
	copy(v.data[i:], v.data[i+1:])
	var zero T
	v.data[len(v.data)-1] = zero
	v.data = v.data[:len(v.data)-1]
	return val, nil
}

// SwapRemove 移除索引 i 处的元素，用末尾元素填补（不保序）；O(1)。
func (v *Vec[T]) SwapRemove(i int) (T, error) {
	if i < 0 || i >= len(v.data) {
		var zero T
		return zero, ErrOutOfBounds
	}
	val := v.data[i]
	last := len(v.data) - 1
	v.data[i] = v.data[last]
	var zero T
	v.data[last] = zero
	v.data = v.data[:last]
	return val, nil
}

// ------------------------------------------------------------------ //
//  容量管理                                                            //
// ------------------------------------------------------------------ //

// Len 返回当前元素数量。
func (v *Vec[T]) Len() int { return len(v.data) }

// Cap 返回当前缓冲区容量。
func (v *Vec[T]) Cap() int { return cap(v.data) }

// IsEmpty 返回数组是否为空。
func (v *Vec[T]) IsEmpty() bool { return len(v.data) == 0 }

// Reserve 确保至少还有 additional 个空槽位。
func (v *Vec[T]) Reserve(additional int) error {
	v.ensureCap(len(v.data) + additional)
	return nil
}

// ShrinkToFit 将容量收缩至当前长度。
func (v *Vec[T]) ShrinkToFit() {
	if len(v.data) == cap(v.data) {
		return
	}
	newData := make([]T, len(v.data))
	copy(newData, v.data)
	v.data = newData
}

// Clear 移除所有元素，容量不变。
func (v *Vec[T]) Clear() {
	var zero T
	for i := range v.data {
		v.data[i] = zero
	}
	v.data = v.data[:0]
}

// ------------------------------------------------------------------ //
//  批量操作                                                            //
// ------------------------------------------------------------------ //

// Extend 将 src 切片中的所有元素追加到末尾（浅拷贝）。
func (v *Vec[T]) Extend(src []T) error {
	if len(src) == 0 {
		return nil
	}
	v.ensureCap(len(v.data) + len(src))
	n := len(v.data)
	v.data = v.data[:n+len(src)]
	copy(v.data[n:], src)
	return nil
}

// ExtendClone 将 src 中每个元素经 cloneFn 深拷贝后追加到末尾。
func (v *Vec[T]) ExtendClone(src []T, cloneFn func(T) T) error {
	if len(src) == 0 {
		return nil
	}
	v.ensureCap(len(v.data) + len(src))
	for _, elem := range src {
		v.data = append(v.data, cloneFn(elem))
	}
	return nil
}

// AsSlice 返回对内部缓冲区的借用视图（不转移所有权）。
//
// 警告：原数组发生扩容后，此切片将指向旧底层数组，静默失效。
func (v *Vec[T]) AsSlice() []T { return v.data }

// Foreach 对每个元素调用 fn。
func (v *Vec[T]) Foreach(fn func(T)) {
	for _, elem := range v.data {
		fn(elem)
	}
}

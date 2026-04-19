package linear

import (
	"testing"
)

// ------------------------------------------------------------------ //
//  TC-RB01  FIFO 基本正确性：填满后全部出队                              //
// ------------------------------------------------------------------ //

func TestRBTC01_BoundedFIFO(t *testing.T) {
	rb, err := NewRingBuf[int](8)
	if err != nil {
		t.Fatal(err)
	}
	for i := range 8 {
		if e := rb.Enqueue(i); e != nil {
			t.Fatalf("Enqueue(%d): %v", i, e)
		}
	}
	if !rb.IsFull() {
		t.Fatal("should be full")
	}
	for want := range 8 {
		got, e := rb.Dequeue()
		if e != nil || got != want {
			t.Fatalf("Dequeue() = (%d, %v), want (%d, nil)", got, e, want)
		}
	}
	if !rb.IsEmpty() {
		t.Fatal("should be empty after all dequeues")
	}
}

// ------------------------------------------------------------------ //
//  TC-RB02  有界模式满时返回 RBErrFull，len 不变                         //
// ------------------------------------------------------------------ //

func TestRBTC02_BoundedFull(t *testing.T) {
	rb, _ := NewRingBuf[int](4)
	for i := range 4 {
		_ = rb.Enqueue(i)
	}
	err := rb.Enqueue(99)
	if err != RBErrFull {
		t.Fatalf("Enqueue on full: got %v, want RBErrFull", err)
	}
	if rb.Len() != 4 {
		t.Fatalf("Len after failed Enqueue = %d, want 4", rb.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-RB03  绕回：enqueue 4，dequeue 3，再 enqueue 5，验证顺序            //
// ------------------------------------------------------------------ //

func TestRBTC03_Wraparound(t *testing.T) {
	rb, _ := NewRingBuf[int](8)
	for i := range 4 {
		_ = rb.Enqueue(i) // 0 1 2 3
	}
	for range 3 {
		_, _ = rb.Dequeue() // 消费 0 1 2
	}
	for i := range 5 {
		_ = rb.Enqueue(10 + i) // 10 11 12 13 14
	}
	// 逻辑顺序：3 10 11 12 13 14
	want := []int{3, 10, 11, 12, 13, 14}
	for _, w := range want {
		got, err := rb.Dequeue()
		if err != nil || got != w {
			t.Fatalf("Dequeue() = (%d, %v), want (%d, nil)", got, err, w)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-RB04  可扩容模式：超过 initCap 后容量翻倍，数据正确                  //
// ------------------------------------------------------------------ //

func TestRBTC04_GrowableGrows(t *testing.T) {
	rb, _ := NewGrowableRingBuf[int](4)
	const n = 16
	for i := range n {
		if err := rb.Enqueue(i); err != nil {
			t.Fatalf("Enqueue(%d): %v", i, err)
		}
	}
	if rb.Cap() < n {
		t.Fatalf("Cap = %d, want >= %d", rb.Cap(), n)
	}
	for want := range n {
		got, err := rb.Dequeue()
		if err != nil || got != want {
			t.Fatalf("Dequeue() = (%d, %v), want (%d, nil)", got, err, want)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-RB05  Get(0)==PeekFront，Get(len-1)==PeekBack                   //
// ------------------------------------------------------------------ //

func TestRBTC05_GetPeekConsistency(t *testing.T) {
	rb, _ := NewRingBuf[int](8)
	for i := range 5 {
		_ = rb.Enqueue(i * 2) // 0 2 4 6 8
	}

	front, _ := rb.PeekFront()
	back, _ := rb.PeekBack()
	g0, _ := rb.Get(0)
	gLast, _ := rb.Get(rb.Len() - 1)

	if g0 != front {
		t.Errorf("Get(0)=%d != PeekFront()=%d", g0, front)
	}
	if gLast != back {
		t.Errorf("Get(len-1)=%d != PeekBack()=%d", gLast, back)
	}
}

// ------------------------------------------------------------------ //
//  TC-RB06  Get(len) 返回 RBErrOutOfBounds                            //
// ------------------------------------------------------------------ //

func TestRBTC06_GetOutOfBounds(t *testing.T) {
	rb, _ := NewRingBuf[int](4)
	_ = rb.Enqueue(1)
	_, err := rb.Get(rb.Len())
	if err != RBErrOutOfBounds {
		t.Fatalf("Get(len): got %v, want RBErrOutOfBounds", err)
	}
	_, err = rb.Get(-1)
	if err != RBErrOutOfBounds {
		t.Fatalf("Get(-1): got %v, want RBErrOutOfBounds", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-RB07  AsSlices 绕回时两段拼接等于逻辑序列                          //
// ------------------------------------------------------------------ //

func TestRBTC07_AsSlicesWraparound(t *testing.T) {
	rb, _ := NewRingBuf[int](8)
	for i := range 6 {
		_ = rb.Enqueue(i) // 0..5
	}
	for range 4 {
		_, _ = rb.Dequeue() // 消费 0..3，head=4
	}
	for i := range 5 {
		_ = rb.Enqueue(10 + i) // 10..14，绕回
	}
	// 逻辑序列：4 5 10 11 12 13 14
	want := []int{4, 5, 10, 11, 12, 13, 14}

	first, second := rb.AsSlices()
	got := append(first, second...)
	if len(got) != len(want) {
		t.Fatalf("AsSlices len = %d, want %d", len(got), len(want))
	}
	for i, v := range got {
		if v != want[i] {
			t.Errorf("AsSlices[%d] = %d, want %d", i, v, want[i])
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-RB08  AsSlices 未绕回时 second 为 nil                            //
// ------------------------------------------------------------------ //

func TestRBTC08_AsSlicesNoWrap(t *testing.T) {
	rb, _ := NewRingBuf[int](8)
	for i := range 4 {
		_ = rb.Enqueue(i)
	}
	first, second := rb.AsSlices()
	if second != nil {
		t.Errorf("expected nil second slice, got len=%d", len(second))
	}
	if len(first) != 4 {
		t.Errorf("first len = %d, want 4", len(first))
	}
}

// ------------------------------------------------------------------ //
//  TC-RB09  Clear 后 len==0，head==0，tail==0，再次 Enqueue 正常         //
// ------------------------------------------------------------------ //

func TestRBTC09_Clear(t *testing.T) {
	rb, _ := NewRingBuf[int](8)
	for i := range 5 {
		_ = rb.Enqueue(i)
	}
	rb.Clear()
	if rb.Len() != 0 || rb.head != 0 || rb.tail != 0 {
		t.Fatalf("after Clear: len=%d head=%d tail=%d, want all 0", rb.Len(), rb.head, rb.tail)
	}
	_ = rb.Enqueue(77)
	got, _ := rb.Dequeue()
	if got != 77 {
		t.Errorf("Dequeue after Clear = %d, want 77", got)
	}
}

// ------------------------------------------------------------------ //
//  TC-RB10  Dequeue/Peek 空缓冲区返回 RBErrEmpty                       //
// ------------------------------------------------------------------ //

func TestRBTC10_EmptyErrors(t *testing.T) {
	rb, _ := NewRingBuf[int](4)
	if _, err := rb.Dequeue(); err != RBErrEmpty {
		t.Fatalf("Dequeue on empty: got %v, want RBErrEmpty", err)
	}
	if _, err := rb.PeekFront(); err != RBErrEmpty {
		t.Fatalf("PeekFront on empty: got %v, want RBErrEmpty", err)
	}
	if _, err := rb.PeekBack(); err != RBErrEmpty {
		t.Fatalf("PeekBack on empty: got %v, want RBErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-RB11  Reserve：可扩容模式确保空余槽位                              //
// ------------------------------------------------------------------ //

func TestRBTC11_Reserve(t *testing.T) {
	rb, _ := NewGrowableRingBuf[int](4)
	_ = rb.Enqueue(1)
	if err := rb.Reserve(10); err != nil {
		t.Fatalf("Reserve(10): %v", err)
	}
	if rb.Cap()-rb.Len() < 10 {
		t.Fatalf("after Reserve(10): avail=%d, want >= 10", rb.Cap()-rb.Len())
	}

	// 有界模式空余不足时返回 RBErrFull
	rb2, _ := NewRingBuf[int](4)
	if err := rb2.Reserve(5); err != RBErrFull {
		t.Fatalf("bounded Reserve(5) on cap=4: got %v, want RBErrFull", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-RB12  容量 1 的缓冲区：enqueue 1 个满；dequeue 1 个空               //
// ------------------------------------------------------------------ //

func TestRBTC12_CapOne(t *testing.T) {
	rb, _ := NewRingBuf[int](1)
	if err := rb.Enqueue(42); err != nil {
		t.Fatalf("Enqueue: %v", err)
	}
	if !rb.IsFull() {
		t.Fatal("should be full after 1 enqueue into cap-1 buffer")
	}
	if err := rb.Enqueue(0); err != RBErrFull {
		t.Fatalf("second Enqueue: got %v, want RBErrFull", err)
	}
	got, err := rb.Dequeue()
	if err != nil || got != 42 {
		t.Fatalf("Dequeue() = (%d, %v), want (42, nil)", got, err)
	}
	if !rb.IsEmpty() {
		t.Fatal("should be empty after dequeue")
	}
}

// ------------------------------------------------------------------ //
//  TC-RB13  NewRingBuf cap=0 / NewGrowableRingBuf initCap=0 返回错误    //
// ------------------------------------------------------------------ //

func TestRBTC13_InvalidConfig(t *testing.T) {
	if _, err := NewRingBuf[int](0); err != RBErrInvalidConfig {
		t.Fatalf("NewRingBuf(0): got %v, want RBErrInvalidConfig", err)
	}
	if _, err := NewGrowableRingBuf[int](0); err != RBErrInvalidConfig {
		t.Fatalf("NewGrowableRingBuf(0): got %v, want RBErrInvalidConfig", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-RB14  可扩容模式绕回后扩容，数据顺序仍正确                          //
// ------------------------------------------------------------------ //

func TestRBTC14_GrowableGrowAfterWrap(t *testing.T) {
	rb, _ := NewGrowableRingBuf[int](4) // cap=4
	for i := range 4 {
		_ = rb.Enqueue(i) // 0 1 2 3
	}
	for range 2 {
		_, _ = rb.Dequeue() // 消费 0 1，head=2
	}
	// 此时 head=2，tail=2（绕回临界）
	// 再 enqueue 3 个触发扩容（当前 len=2，cap=4，enqueue 3 个会填满后扩容）
	for i := range 3 {
		_ = rb.Enqueue(10 + i) // 10 11 12
	}
	// 逻辑序列：2 3 10 11 12
	want := []int{2, 3, 10, 11, 12}
	for _, w := range want {
		got, err := rb.Dequeue()
		if err != nil || got != w {
			t.Fatalf("Dequeue() = (%d, %v), want (%d, nil)", got, err, w)
		}
	}
}

package linear

import (
	"testing"
)

// ------------------------------------------------------------------ //
//  TC-01  PushBack + PopFront 验证 FIFO 顺序                          //
// ------------------------------------------------------------------ //

func TestSL01_FIFO(t *testing.T) {
	l := NewSList[int]()
	for i := range 5 {
		l.PushBack(i * 10)
	}
	if l.Len() != 5 {
		t.Fatalf("Len() = %d, want 5", l.Len())
	}
	for i := range 5 {
		got, err := l.PopFront()
		if err != nil {
			t.Fatalf("PopFront(): %v", err)
		}
		if got != i*10 {
			t.Errorf("PopFront() = %d, want %d", got, i*10)
		}
	}
	if l.Len() != 0 {
		t.Errorf("Len() = %d after drain, want 0", l.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-02  PushFront + PopFront 验证 LIFO 顺序                         //
// ------------------------------------------------------------------ //

func TestSL02_LIFO(t *testing.T) {
	l := NewSList[int]()
	for i := range 5 {
		l.PushFront(i) // 链表: 4→3→2→1→0
	}
	for i := 4; i >= 0; i-- {
		got, err := l.PopFront()
		if err != nil {
			t.Fatalf("PopFront(): %v", err)
		}
		if got != i {
			t.Errorf("PopFront() = %d, want %d", got, i)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-03  空链表 pop/peek 返回 SLErrEmpty                             //
// ------------------------------------------------------------------ //

func TestSL03_PopEmpty(t *testing.T) {
	l := NewSList[int]()

	if _, err := l.PopFront(); err != SLErrEmpty {
		t.Errorf("PopFront() error = %v, want SLErrEmpty", err)
	}
	if _, err := l.PopBack(); err != SLErrEmpty {
		t.Errorf("PopBack() error = %v, want SLErrEmpty", err)
	}
	if _, err := l.PeekFront(); err != SLErrEmpty {
		t.Errorf("PeekFront() error = %v, want SLErrEmpty", err)
	}
	if _, err := l.PeekBack(); err != SLErrEmpty {
		t.Errorf("PeekBack() error = %v, want SLErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-04  Get / Peek 边界                                             //
// ------------------------------------------------------------------ //

func TestSL04_GetPeek(t *testing.T) {
	l := NewSList[int]()
	l.PushBack(10)
	l.PushBack(20)
	l.PushBack(30)

	if v, err := l.Get(0); err != nil || v != 10 {
		t.Errorf("Get(0) = (%d, %v), want (10, nil)", v, err)
	}
	if v, err := l.Get(2); err != nil || v != 30 {
		t.Errorf("Get(2) = (%d, %v), want (30, nil)", v, err)
	}
	if _, err := l.Get(3); err != SLErrOutOfBounds {
		t.Errorf("Get(3) error = %v, want SLErrOutOfBounds", err)
	}
	if v, err := l.PeekFront(); err != nil || v != 10 {
		t.Errorf("PeekFront() = (%d, %v)", v, err)
	}
	if v, err := l.PeekBack(); err != nil || v != 30 {
		t.Errorf("PeekBack() = (%d, %v)", v, err)
	}
}

// ------------------------------------------------------------------ //
//  TC-05  PopBack：O(n) 遍历返回正确尾值                              //
// ------------------------------------------------------------------ //

func TestSL05_PopBack(t *testing.T) {
	l := NewSList[int]()
	l.PushBack(1)
	l.PushBack(2)
	l.PushBack(3)

	if v, err := l.PopBack(); err != nil || v != 3 {
		t.Errorf("PopBack() = (%d, %v), want (3, nil)", v, err)
	}
	if v, err := l.PopBack(); err != nil || v != 2 {
		t.Errorf("PopBack() = (%d, %v), want (2, nil)", v, err)
	}
	if v, err := l.PopBack(); err != nil || v != 1 {
		t.Errorf("PopBack() = (%d, %v), want (1, nil)", v, err)
	}
	if !l.IsEmpty() {
		t.Error("should be empty after 3 PopBack")
	}
	if l.dummy.next != nil || l.tail != l.dummy {
		t.Error("tail should point back to dummy after drain")
	}
}

// ------------------------------------------------------------------ //
//  TC-06  InsertAfter / RemoveAfter                                   //
// ------------------------------------------------------------------ //

func TestSL06_InsertRemoveAfter(t *testing.T) {
	l := NewSList[int]()
	l.PushBack(1)
	l.PushBack(3) // [1, 3]

	// InsertAfter(first, 2) → [1, 2, 3]
	first := l.dummy.next
	l.InsertAfter(first, 2)
	if l.Len() != 3 {
		t.Fatalf("Len() = %d, want 3", l.Len())
	}
	if v, _ := l.Get(1); v != 2 {
		t.Errorf("Get(1) = %d, want 2", v)
	}

	// RemoveAfter(first) → 移除 2 → [1, 3]
	v, err := l.RemoveAfter(first)
	if err != nil || v != 2 {
		t.Errorf("RemoveAfter(first) = (%d, %v), want (2, nil)", v, err)
	}
	if l.Len() != 2 {
		t.Errorf("Len() = %d, want 2", l.Len())
	}

	// RemoveAfter(tail) → SLErrOutOfBounds
	if _, err := l.RemoveAfter(l.tail); err != SLErrOutOfBounds {
		t.Errorf("RemoveAfter(tail) error = %v, want SLErrOutOfBounds", err)
	}

	// InsertAfter(dummy) 等价于 PushFront
	l.InsertAfter(l.Dummy(), 0) // [0, 1, 3]
	if v, _ := l.Get(0); v != 0 {
		t.Errorf("Get(0) = %d, want 0 after InsertAfter(dummy)", v)
	}
}

// ------------------------------------------------------------------ //
//  TC-07  Reverse 后顺序正确                                          //
// ------------------------------------------------------------------ //

func TestSL07_Reverse(t *testing.T) {
	l := NewSList[int]()
	for i := 1; i <= 5; i++ {
		l.PushBack(i) // [1,2,3,4,5]
	}
	l.Reverse() // [5,4,3,2,1]

	for i := range 5 {
		got, _ := l.Get(i)
		want := 5 - i
		if got != want {
			t.Errorf("Get(%d) = %d, want %d", i, got, want)
		}
	}
	if back, _ := l.PeekBack(); back != 1 {
		t.Errorf("PeekBack() = %d, want 1", back)
	}

	// 空链表 Reverse 不崩溃
	empty := NewSList[int]()
	empty.Reverse()

	// 单节点 Reverse
	single := NewSList[int]()
	single.PushBack(42)
	single.Reverse()
	if v, _ := single.PeekFront(); v != 42 {
		t.Errorf("single Reverse: PeekFront() = %d, want 42", v)
	}
}

// ------------------------------------------------------------------ //
//  TC-08  Find                                                        //
// ------------------------------------------------------------------ //

func TestSL08_Find(t *testing.T) {
	l := NewSList[int]()
	for _, v := range []int{10, 30, 50} {
		l.PushBack(v)
	}

	node := l.Find(func(v int) bool { return v == 30 })
	if node == nil || node.Val != 30 {
		t.Errorf("Find(30) = %v, want node with Val=30", node)
	}

	notFound := l.Find(func(v int) bool { return v == 99 })
	if notFound != nil {
		t.Errorf("Find(99) should return nil")
	}
}

// ------------------------------------------------------------------ //
//  TC-09  哨兵不变量：tail 始终正确                                   //
// ------------------------------------------------------------------ //

func TestSL09_SentinelInvariant(t *testing.T) {
	l := NewSList[int]()

	// 空链表：tail == dummy
	if l.tail != l.dummy {
		t.Error("empty list: tail should == dummy")
	}

	l.PushFront(1)
	if l.tail == l.dummy {
		t.Error("after PushFront: tail should != dummy")
	}

	l.PushBack(2)
	if v, _ := l.PeekBack(); v != 2 {
		t.Errorf("tail.Val = %d, want 2", l.tail.Val)
	}

	l.PopBack()
	if v, _ := l.PeekBack(); v != 1 {
		t.Errorf("after PopBack: tail.Val = %d, want 1", l.tail.Val)
	}

	l.PopFront()
	if l.tail != l.dummy {
		t.Error("after draining: tail should == dummy")
	}
}

// ------------------------------------------------------------------ //
//  TC-10  Clear                                                       //
// ------------------------------------------------------------------ //

func TestSL10_Clear(t *testing.T) {
	l := NewSList[int]()
	for i := range 5 {
		l.PushBack(i)
	}
	l.Clear()
	if l.Len() != 0 {
		t.Errorf("Len() = %d after Clear, want 0", l.Len())
	}
	if l.tail != l.dummy {
		t.Error("tail should == dummy after Clear")
	}
	// 清空后继续使用
	l.PushBack(99)
	if v, _ := l.PeekFront(); v != 99 {
		t.Errorf("PeekFront() = %d after Clear+Push, want 99", v)
	}
}

// ------------------------------------------------------------------ //
//  TC-11  ForEach 遍历顺序正确                                        //
// ------------------------------------------------------------------ //

func TestSL11_ForEach(t *testing.T) {
	l := NewSList[int]()
	for i := 1; i <= 4; i++ {
		l.PushBack(i)
	}
	sum := 0
	l.ForEach(func(v int) { sum += v })
	if sum != 10 {
		t.Errorf("ForEach sum = %d, want 10", sum)
	}
}

// ------------------------------------------------------------------ //
//  TC-12  单元素链表的各种边界操作                                     //
// ------------------------------------------------------------------ //

func TestSL12_SingleElement(t *testing.T) {
	l := NewSList[int]()
	l.PushBack(42)

	if v, _ := l.PeekFront(); v != 42 {
		t.Errorf("PeekFront() = %d, want 42", v)
	}
	if v, _ := l.PeekBack(); v != 42 {
		t.Errorf("PeekBack() = %d, want 42", v)
	}
	if l.dummy.next != l.tail {
		t.Error("single element: dummy.next should == tail")
	}

	v, _ := l.PopBack()
	if v != 42 || !l.IsEmpty() {
		t.Errorf("PopBack() single: got %d, empty=%v", v, l.IsEmpty())
	}
}

// ------------------------------------------------------------------ //
//  Benchmark                                                          //
// ------------------------------------------------------------------ //

func BenchmarkSListPushFront(b *testing.B) {
	for range b.N {
		l := NewSList[int]()
		for i := range 1000 {
			l.PushFront(i)
		}
	}
}

func BenchmarkSListPushBack(b *testing.B) {
	for range b.N {
		l := NewSList[int]()
		for i := range 1000 {
			l.PushBack(i)
		}
	}
}

func BenchmarkSListPopFront(b *testing.B) {
	for range b.N {
		l := NewSList[int]()
		for i := range 1000 {
			l.PushBack(i)
		}
		b.StartTimer()
		for !l.IsEmpty() {
			l.PopFront()
		}
		b.StopTimer()
	}
}

func BenchmarkSListPushFrontVsVecInsertHead(b *testing.B) {
	b.Run("SList/PushFront", func(b *testing.B) {
		for range b.N {
			l := NewSList[int]()
			for i := range 1000 {
				l.PushFront(i)
			}
		}
	})
	b.Run("Vec/InsertHead", func(b *testing.B) {
		for range b.N {
			v := New[int]()
			for i := range 1000 {
				v.Insert(0, i)
			}
		}
	})
}

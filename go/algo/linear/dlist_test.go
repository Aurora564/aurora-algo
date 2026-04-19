package linear

import (
	"slices"
	"testing"
)

// ------------------------------------------------------------------ //
// TC-01  Sentinel invariant on empty list                              //
// ------------------------------------------------------------------ //

func TestDL01_SentinelInvariant(t *testing.T) {
	l := NewDList[int]()
	if l.dummy == nil {
		t.Fatal("dummy is nil")
	}
	if l.dummy.prev != l.dummy || l.dummy.next != l.dummy {
		t.Fatal("sentinel not self-referential on empty list")
	}
	if l.Len() != 0 {
		t.Fatalf("want len=0, got %d", l.Len())
	}
	if !l.IsEmpty() {
		t.Fatal("IsEmpty should be true")
	}
}

// ------------------------------------------------------------------ //
// TC-02  Push front / back and peek                                    //
// ------------------------------------------------------------------ //

func TestDL02_PushPeek(t *testing.T) {
	l := NewDList[int]()
	l.PushBack(1)
	l.PushBack(2)
	l.PushFront(0)

	if l.Len() != 3 {
		t.Fatalf("want len=3, got %d", l.Len())
	}
	if v, _ := l.PeekFront(); v != 0 {
		t.Fatalf("front: want 0, got %d", v)
	}
	if v, _ := l.PeekBack(); v != 2 {
		t.Fatalf("back: want 2, got %d", v)
	}
	// sentinel must be consistent
	if l.dummy.next.prev != l.dummy {
		t.Fatal("dummy.next.prev != dummy")
	}
	if l.dummy.prev.next != l.dummy {
		t.Fatal("dummy.prev.next != dummy")
	}
}

// ------------------------------------------------------------------ //
// TC-03  Pop front / back                                              //
// ------------------------------------------------------------------ //

func TestDL03_Pop(t *testing.T) {
	l := NewDList[int]()
	for i := range 5 {
		l.PushBack(i)
	}

	if v, _ := l.PopFront(); v != 0 {
		t.Fatalf("want 0, got %d", v)
	}
	if v, _ := l.PopFront(); v != 1 {
		t.Fatalf("want 1, got %d", v)
	}
	if v, _ := l.PopBack(); v != 4 {
		t.Fatalf("want 4, got %d", v)
	}
	if v, _ := l.PopBack(); v != 3 {
		t.Fatalf("want 3, got %d", v)
	}
	if l.Len() != 1 {
		t.Fatalf("want len=1, got %d", l.Len())
	}
}

// ------------------------------------------------------------------ //
// TC-04  get() bidirectional                                           //
// ------------------------------------------------------------------ //

func TestDL04_Get(t *testing.T) {
	l := NewDList[int]()
	for i := range 10 {
		l.PushBack(i)
	}
	for i := range 10 {
		v, err := l.Get(i)
		if err != nil {
			t.Fatalf("Get(%d): unexpected error %v", i, err)
		}
		if v != i {
			t.Fatalf("Get(%d): want %d, got %d", i, i, v)
		}
	}
	if _, err := l.Get(10); err == nil {
		t.Fatal("Get(10) should return error")
	}
}

// ------------------------------------------------------------------ //
// TC-05  ForEach / ForEachRev                                          //
// ------------------------------------------------------------------ //

func TestDL05_ForEach(t *testing.T) {
	l := NewDList[int]()
	for i := 1; i <= 5; i++ {
		l.PushBack(i)
	}

	sum := 0
	l.ForEach(func(v int) { sum += v })
	if sum != 15 {
		t.Fatalf("ForEach sum: want 15, got %d", sum)
	}

	sum = 0
	l.ForEachRev(func(v int) { sum += v })
	if sum != 15 {
		t.Fatalf("ForEachRev sum: want 15, got %d", sum)
	}
}

// ------------------------------------------------------------------ //
// TC-06  All() / Backward() iter.Seq                                  //
// ------------------------------------------------------------------ //

func TestDL06_IterSeq(t *testing.T) {
	l := NewDList[int]()
	for i := range 5 {
		l.PushBack(i)
	}

	got := slices.Collect(l.All())
	want := []int{0, 1, 2, 3, 4}
	if !slices.Equal(got, want) {
		t.Fatalf("All(): want %v, got %v", want, got)
	}

	gotRev := slices.Collect(l.Backward())
	wantRev := []int{4, 3, 2, 1, 0}
	if !slices.Equal(gotRev, wantRev) {
		t.Fatalf("Backward(): want %v, got %v", wantRev, gotRev)
	}
}

// ------------------------------------------------------------------ //
// TC-07  Reverse()                                                     //
// ------------------------------------------------------------------ //

func TestDL07_Reverse(t *testing.T) {
	l := NewDList[int]()
	for i := range 5 {
		l.PushBack(i)
	}
	l.Reverse()

	if l.Len() != 5 {
		t.Fatalf("want len=5, got %d", l.Len())
	}
	// sentinel must still be consistent
	if l.dummy.next.prev != l.dummy || l.dummy.prev.next != l.dummy {
		t.Fatal("sentinel broken after Reverse")
	}
	for i := range 5 {
		v, _ := l.Get(i)
		if v != 4-i {
			t.Fatalf("Get(%d): want %d, got %d", i, 4-i, v)
		}
	}
}

// ------------------------------------------------------------------ //
// TC-08  InsertBefore / InsertAfter                                    //
// ------------------------------------------------------------------ //

func TestDL08_Insert(t *testing.T) {
	l := NewDList[int]()
	l.PushBack(1)
	l.PushBack(3)

	// find node with value 3 and insert 2 before it
	n3 := l.Find(func(v int) bool { return v == 3 })
	if n3 == nil {
		t.Fatal("Find(3) returned nil")
	}
	l.InsertBefore(n3, 2)
	if l.Len() != 3 {
		t.Fatalf("want len=3, got %d", l.Len())
	}
	v, _ := l.Get(1)
	if v != 2 {
		t.Fatalf("Get(1): want 2, got %d", v)
	}

	// insert 4 after the last data node
	l.InsertAfter(l.dummy.prev, 4)
	if bk, _ := l.PeekBack(); bk != 4 {
		t.Fatalf("PeekBack: want 4, got %d", bk)
	}
}

// ------------------------------------------------------------------ //
// TC-09  Remove()                                                      //
// ------------------------------------------------------------------ //

func TestDL09_Remove(t *testing.T) {
	l := NewDList[int]()
	for i := range 5 {
		l.PushBack(i)
	}
	mid := l.Find(func(v int) bool { return v == 2 })
	if mid == nil {
		t.Fatal("Find(2) returned nil")
	}
	removed := l.Remove(mid)
	if removed != 2 {
		t.Fatalf("Remove: want 2, got %d", removed)
	}
	if l.Len() != 4 {
		t.Fatalf("want len=4, got %d", l.Len())
	}
	v, _ := l.Get(2)
	if v != 3 {
		t.Fatalf("Get(2) after remove: want 3, got %d", v)
	}
}

// ------------------------------------------------------------------ //
// TC-10  Splice() O(1)                                                 //
// ------------------------------------------------------------------ //

func TestDL10_Splice(t *testing.T) {
	dst := NewDList[int]()
	src := NewDList[int]()

	for i := range 3 {
		dst.PushBack(i)
	} // 0,1,2
	for i := 10; i < 13; i++ {
		src.PushBack(i)
	} // 10,11,12

	// splice src before index-1 node of dst (val=1)
	pos := dst.dummy.next.next // node with val=1
	dst.Splice(pos, src)

	if dst.Len() != 6 {
		t.Fatalf("dst.Len: want 6, got %d", dst.Len())
	}
	if src.Len() != 0 {
		t.Fatalf("src.Len: want 0, got %d", src.Len())
	}

	// src sentinel must still be valid empty circular
	if src.dummy.next != src.dummy || src.dummy.prev != src.dummy {
		t.Fatal("src sentinel broken after Splice")
	}

	expected := []int{0, 10, 11, 12, 1, 2}
	got := slices.Collect(dst.All())
	if !slices.Equal(got, expected) {
		t.Fatalf("want %v, got %v", expected, got)
	}
}

// ------------------------------------------------------------------ //
// TC-11  Empty-list errors                                             //
// ------------------------------------------------------------------ //

func TestDL11_EmptyErrors(t *testing.T) {
	l := NewDList[int]()

	if _, err := l.PopFront(); err == nil {
		t.Fatal("PopFront on empty should error")
	}
	if _, err := l.PopBack(); err == nil {
		t.Fatal("PopBack on empty should error")
	}
	if _, err := l.PeekFront(); err == nil {
		t.Fatal("PeekFront on empty should error")
	}
	if _, err := l.PeekBack(); err == nil {
		t.Fatal("PeekBack on empty should error")
	}
	if _, err := l.Get(0); err == nil {
		t.Fatal("Get(0) on empty should error")
	}
}

// ------------------------------------------------------------------ //
// TC-12  Clear()                                                       //
// ------------------------------------------------------------------ //

func TestDL12_Clear(t *testing.T) {
	l := NewDList[int]()
	for i := range 5 {
		l.PushBack(i)
	}
	l.Clear()
	if !l.IsEmpty() {
		t.Fatal("IsEmpty should be true after Clear")
	}
	if l.dummy.next != l.dummy || l.dummy.prev != l.dummy {
		t.Fatal("sentinel broken after Clear")
	}
	// can still push after clear
	l.PushBack(99)
	if v, _ := l.PeekFront(); v != 99 {
		t.Fatalf("PeekFront after Clear+Push: want 99, got %d", v)
	}
}

// ------------------------------------------------------------------ //
// Benchmarks                                                           //
// ------------------------------------------------------------------ //

func BenchmarkDListPushBack(b *testing.B) {
	l := NewDList[int]()
	for i := range b.N {
		l.PushBack(i)
	}
}

func BenchmarkDListGet(b *testing.B) {
	l := NewDList[int]()
	for i := range 1000 {
		l.PushBack(i)
	}
	b.ResetTimer()
	for i := range b.N {
		_, _ = l.Get(i % 1000)
	}
}

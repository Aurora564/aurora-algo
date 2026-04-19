package linear

import (
	"fmt"
	"strings"
	"testing"
)

// ------------------------------------------------------------------ //
//  TC-01  Push 直到触发扩容，验证所有元素值正确                          //
// ------------------------------------------------------------------ //

func TestTC01_PushGrow(t *testing.T) {
	v := New[int](WithInitCap(2))
	for i := range 10 {
		if err := v.Push(i); err != nil {
			t.Fatalf("Push(%d): %v", i, err)
		}
	}
	if v.Len() != 10 {
		t.Fatalf("len = %d, want 10", v.Len())
	}
	for i := range 10 {
		got, err := v.Get(i)
		if err != nil {
			t.Fatalf("Get(%d): %v", i, err)
		}
		if got != i {
			t.Errorf("Get(%d) = %d, want %d", i, got, i)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-02  Pop 至空返回 ErrEmpty                                        //
// ------------------------------------------------------------------ //

func TestTC02_PopEmpty(t *testing.T) {
	v := New[int]()
	v.Push(42)

	val, err := v.Pop()
	if err != nil || val != 42 {
		t.Fatalf("Pop() = (%d, %v), want (42, nil)", val, err)
	}
	_, err = v.Pop()
	if err != ErrEmpty {
		t.Fatalf("empty Pop() error = %v, want ErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-03/04  Get 边界                                                  //
// ------------------------------------------------------------------ //

func TestTC03_04_GetBounds(t *testing.T) {
	v := New[int]()
	for i := range 5 {
		v.Push(i)
	}

	if got, _ := v.Get(0); got != 0 {
		t.Errorf("Get(0) = %d, want 0", got)
	}
	if got, _ := v.Get(4); got != 4 {
		t.Errorf("Get(4) = %d, want 4", got)
	}
	if _, err := v.Get(5); err != ErrOutOfBounds {
		t.Errorf("Get(5) error = %v, want ErrOutOfBounds", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-05  Insert(0, x) 后原有元素整体后移一位                            //
// ------------------------------------------------------------------ //

func TestTC05_InsertHead(t *testing.T) {
	v := New[int]()
	for i := 1; i <= 3; i++ {
		v.Push(i)
	} // [1,2,3]
	if err := v.Insert(0, 0); err != nil { // [0,1,2,3]
		t.Fatal(err)
	}
	if v.Len() != 4 {
		t.Fatalf("len = %d, want 4", v.Len())
	}
	for i := range 4 {
		got, _ := v.Get(i)
		if got != i {
			t.Errorf("Get(%d) = %d, want %d", i, got, i)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-06  Remove(0) 后原有元素整体前移一位                               //
// ------------------------------------------------------------------ //

func TestTC06_RemoveHead(t *testing.T) {
	v := New[int]()
	for i := range 4 {
		v.Push(i)
	} // [0,1,2,3]
	val, err := v.Remove(0)
	if err != nil || val != 0 {
		t.Fatalf("Remove(0) = (%d, %v)", val, err)
	}
	if v.Len() != 3 {
		t.Fatalf("len = %d, want 3", v.Len())
	}
	for i := range 3 {
		got, _ := v.Get(i)
		if got != i+1 {
			t.Errorf("Get(%d) = %d, want %d", i, got, i+1)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-07  SwapRemove：末尾元素出现在 i 位置                             //
// ------------------------------------------------------------------ //

func TestTC07_SwapRemove(t *testing.T) {
	v := New[int]()
	for i := range 5 {
		v.Push(i)
	} // [0,1,2,3,4]
	val, err := v.SwapRemove(1)
	if err != nil || val != 1 {
		t.Fatalf("SwapRemove(1) = (%d, %v)", val, err)
	}
	if v.Len() != 4 {
		t.Fatalf("len = %d, want 4", v.Len())
	}
	pos1, _ := v.Get(1)
	if pos1 != 4 {
		t.Errorf("Get(1) = %d, want 4 (tail element)", pos1)
	}
}

// ------------------------------------------------------------------ //
//  TC-08  Clone 后修改副本，源不受影响                                   //
// ------------------------------------------------------------------ //

func TestTC08_CloneIndependence(t *testing.T) {
	type Elem struct{ S string }
	v := New[Elem]()
	v.Push(Elem{"hello"})
	v.Push(Elem{"world"})

	cloneFn := func(e Elem) Elem { return Elem{strings.Clone(e.S)} }
	dst := v.Clone(cloneFn)

	// 修改副本
	dst.Set(0, Elem{"changed"})

	// 源不受影响
	src0, _ := v.Get(0)
	if src0.S != "hello" {
		t.Errorf("source[0] = %q, want %q", src0.S, "hello")
	}
}

// ------------------------------------------------------------------ //
//  TC-09  Reserve 确保空槽位；Cap 增长                                  //
// ------------------------------------------------------------------ //

func TestTC09_Reserve(t *testing.T) {
	v := New[int]()
	v.Reserve(100)
	if v.Cap() < 100 {
		t.Errorf("Cap() = %d, want >= 100", v.Cap())
	}
}

// ------------------------------------------------------------------ //
//  TC-10  ShrinkToFit：容量收缩至 Len                                  //
// ------------------------------------------------------------------ //

func TestTC10_ShrinkToFit(t *testing.T) {
	v := New[int](WithInitCap(64))
	for i := range 5 {
		v.Push(i)
	}
	v.ShrinkToFit()
	if v.Cap() != v.Len() {
		t.Errorf("Cap()=%d != Len()=%d after ShrinkToFit", v.Cap(), v.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-11  Clear：len == 0，cap 不变                                    //
// ------------------------------------------------------------------ //

func TestTC11_Clear(t *testing.T) {
	v := New[int]()
	for i := range 5 {
		v.Push(i)
	}
	oldCap := v.Cap()
	v.Clear()
	if v.Len() != 0 {
		t.Errorf("Len() = %d after Clear, want 0", v.Len())
	}
	if v.Cap() != oldCap {
		t.Errorf("Cap() changed: %d -> %d", oldCap, v.Cap())
	}
}

// ------------------------------------------------------------------ //
//  TC-12  自定义扩容策略 ×1.5                                          //
// ------------------------------------------------------------------ //

func TestTC12_CustomGrow(t *testing.T) {
	grow15 := func(cap int) int { return cap + cap/2 + 1 }
	v := New[int](WithInitCap(2), WithGrowFn(grow15))
	if v.Cap() != 2 {
		t.Fatalf("initial cap = %d, want 2", v.Cap())
	}
	// 推入第 3 个元素，应触发扩容
	for i := range 3 {
		v.Push(i)
	}
	expected := grow15(2) // 4
	if v.Cap() != expected {
		t.Errorf("Cap() = %d, want %d (x1.5 grow)", v.Cap(), expected)
	}
}

// ------------------------------------------------------------------ //
//  TC-13  Extend & Foreach                                             //
// ------------------------------------------------------------------ //

func TestTC13_ExtendForeach(t *testing.T) {
	v := New[int]()
	src := []int{10, 20, 30}
	v.Extend(src)
	if v.Len() != 3 {
		t.Fatalf("len = %d after Extend", v.Len())
	}

	sum := 0
	v.Foreach(func(x int) { sum += x })
	if sum != 60 {
		t.Errorf("sum = %d, want 60", sum)
	}
}

// ------------------------------------------------------------------ //
//  额外：Set 越界                                                       //
// ------------------------------------------------------------------ //

func TestSet_OOB(t *testing.T) {
	v := New[int]()
	v.Push(1)
	if err := v.Set(1, 99); err != ErrOutOfBounds {
		t.Errorf("Set(1) error = %v, want ErrOutOfBounds", err)
	}
}

// ------------------------------------------------------------------ //
//  额外：Insert 越界                                                    //
// ------------------------------------------------------------------ //

func TestInsert_OOB(t *testing.T) {
	v := New[int]()
	if err := v.Insert(1, 0); err != ErrOutOfBounds {
		t.Errorf("Insert(1) on empty error = %v, want ErrOutOfBounds", err)
	}
	// Insert at Len() is valid (equiv. Push)
	v.Push(10)
	if err := v.Insert(1, 20); err != nil {
		t.Errorf("Insert at len error = %v", err)
	}
}

// ------------------------------------------------------------------ //
//  额外：AsSlice 返回的切片与底层一致                                    //
// ------------------------------------------------------------------ //

func TestAsSlice(t *testing.T) {
	v := New[int]()
	for i := range 3 {
		v.Push(i)
	}
	sl := v.AsSlice()
	if len(sl) != 3 {
		t.Fatalf("len(AsSlice()) = %d, want 3", len(sl))
	}
	for i, x := range sl {
		if x != i {
			t.Errorf("slice[%d] = %d, want %d", i, x, i)
		}
	}
}

// ------------------------------------------------------------------ //
//  额外：IsEmpty                                                        //
// ------------------------------------------------------------------ //

func TestIsEmpty(t *testing.T) {
	v := New[int]()
	if !v.IsEmpty() {
		t.Error("new Vec should be empty")
	}
	v.Push(1)
	if v.IsEmpty() {
		t.Error("non-empty Vec reported empty")
	}
}

// ------------------------------------------------------------------ //
//  Benchmark                                                           //
// ------------------------------------------------------------------ //

func BenchmarkPushNoPrealloc(b *testing.B) {
	for range b.N {
		v := New[int]()
		for i := range 1000 {
			v.Push(i)
		}
	}
}

func BenchmarkPushWithReserve(b *testing.B) {
	for range b.N {
		v := New[int]()
		v.Reserve(1000)
		for i := range 1000 {
			v.Push(i)
		}
	}
}

func BenchmarkSequentialGet(b *testing.B) {
	v := New[int]()
	for i := range 1000 {
		v.Push(i)
	}
	b.ResetTimer()
	for range b.N {
		sum := 0
		for i := range 1000 {
			x, _ := v.Get(i)
			sum += x
		}
		_ = sum
	}
}

func BenchmarkInsertHead(b *testing.B) {
	for range b.N {
		v := New[int]()
		for i := range 100 {
			v.Insert(0, i)
		}
	}
}

func BenchmarkGrowX2vsX15(b *testing.B) {
	grow15 := func(cap int) int { return cap + cap/2 + 1 }
	b.Run("x2", func(b *testing.B) {
		for range b.N {
			v := New[int]()
			for i := range 10000 {
				v.Push(i)
			}
		}
	})
	b.Run("x1.5", func(b *testing.B) {
		for range b.N {
			v := New[int](WithGrowFn(grow15))
			for i := range 10000 {
				v.Push(i)
			}
		}
	})
	_ = fmt.Sprintf // avoid unused import
}

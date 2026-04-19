package linear

import "testing"

// ================================================================== //
//  ArrayStack 测试                                                    //
// ================================================================== //

// ------------------------------------------------------------------ //
//  TC-S01  Push / Pop 基本正确性                                       //
// ------------------------------------------------------------------ //

func TestSTC01_ArrayStack_PushPop(t *testing.T) {
	s := NewArrayStack[int]()

	for i := range 5 {
		s.Push(i)
	}
	if s.Len() != 5 {
		t.Fatalf("Len = %d, want 5", s.Len())
	}

	// 后进先出：弹出顺序应为 4 3 2 1 0
	for want := 4; want >= 0; want-- {
		got, err := s.Pop()
		if err != nil {
			t.Fatalf("Pop(): %v", err)
		}
		if got != want {
			t.Errorf("Pop() = %d, want %d", got, want)
		}
	}
	if !s.IsEmpty() {
		t.Fatal("stack should be empty after all pops")
	}
}

// ------------------------------------------------------------------ //
//  TC-S02  Pop 空栈返回 StackErrEmpty                                  //
// ------------------------------------------------------------------ //

func TestSTC02_ArrayStack_PopEmpty(t *testing.T) {
	s := NewArrayStack[int]()
	_, err := s.Pop()
	if err != StackErrEmpty {
		t.Fatalf("Pop() on empty stack: got %v, want StackErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S03  Peek 不移除栈顶                                             //
// ------------------------------------------------------------------ //

func TestSTC03_ArrayStack_Peek(t *testing.T) {
	s := NewArrayStack[string]()
	s.Push("a")
	s.Push("b")

	got, err := s.Peek()
	if err != nil || got != "b" {
		t.Fatalf("Peek() = (%q, %v), want (\"b\", nil)", got, err)
	}
	if s.Len() != 2 {
		t.Fatalf("Len after Peek = %d, want 2", s.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-S04  Peek 空栈返回 StackErrEmpty                                 //
// ------------------------------------------------------------------ //

func TestSTC04_ArrayStack_PeekEmpty(t *testing.T) {
	s := NewArrayStack[int]()
	_, err := s.Peek()
	if err != StackErrEmpty {
		t.Fatalf("Peek() on empty stack: got %v, want StackErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S05  扩容后数据仍正确（超过初始容量）                               //
// ------------------------------------------------------------------ //

func TestSTC05_ArrayStack_Grow(t *testing.T) {
	s := NewArrayStack[int](WithInitCap(2))
	const n = 64
	for i := range n {
		s.Push(i)
	}
	if s.Len() != n {
		t.Fatalf("Len = %d, want %d", s.Len(), n)
	}
	for want := n - 1; want >= 0; want-- {
		got, err := s.Pop()
		if err != nil || got != want {
			t.Fatalf("Pop() = (%d, %v), want (%d, nil)", got, err, want)
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-S06  Clear 后栈为空                                              //
// ------------------------------------------------------------------ //

func TestSTC06_ArrayStack_Clear(t *testing.T) {
	s := NewArrayStack[int]()
	s.Push(1)
	s.Push(2)
	s.Clear()

	if !s.IsEmpty() || s.Len() != 0 {
		t.Fatal("stack should be empty after Clear")
	}
	// Clear 后继续可用
	s.Push(99)
	got, _ := s.Pop()
	if got != 99 {
		t.Errorf("Pop after Clear = %d, want 99", got)
	}
}

// ------------------------------------------------------------------ //
//  TC-S07  Reserve + ShrinkToFit                                      //
// ------------------------------------------------------------------ //

func TestSTC07_ArrayStack_ReserveShrink(t *testing.T) {
	s := NewArrayStack[int]()
	s.Reserve(100)
	if s.base.Cap() < 100 {
		t.Fatalf("Cap after Reserve(100) = %d, want >= 100", s.base.Cap())
	}

	s.Push(1)
	s.ShrinkToFit()
	if s.base.Cap() != 1 {
		t.Fatalf("Cap after ShrinkToFit = %d, want 1", s.base.Cap())
	}
}

// ================================================================== //
//  ListStack 测试                                                     //
// ================================================================== //

// ------------------------------------------------------------------ //
//  TC-S08  Push / Pop 基本正确性                                       //
// ------------------------------------------------------------------ //

func TestSTC08_ListStack_PushPop(t *testing.T) {
	s := NewListStack[int]()

	for i := range 5 {
		s.Push(i)
	}
	if s.Len() != 5 {
		t.Fatalf("Len = %d, want 5", s.Len())
	}

	for want := 4; want >= 0; want-- {
		got, err := s.Pop()
		if err != nil || got != want {
			t.Fatalf("Pop() = (%d, %v), want (%d, nil)", got, err, want)
		}
	}
	if !s.IsEmpty() {
		t.Fatal("stack should be empty after all pops")
	}
}

// ------------------------------------------------------------------ //
//  TC-S09  Pop 空栈返回 StackErrEmpty                                  //
// ------------------------------------------------------------------ //

func TestSTC09_ListStack_PopEmpty(t *testing.T) {
	s := NewListStack[int]()
	_, err := s.Pop()
	if err != StackErrEmpty {
		t.Fatalf("Pop() on empty stack: got %v, want StackErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S10  Peek 不移除栈顶                                             //
// ------------------------------------------------------------------ //

func TestSTC10_ListStack_Peek(t *testing.T) {
	s := NewListStack[string]()
	s.Push("x")
	s.Push("y")

	got, err := s.Peek()
	if err != nil || got != "y" {
		t.Fatalf("Peek() = (%q, %v), want (\"y\", nil)", got, err)
	}
	if s.Len() != 2 {
		t.Fatalf("Len after Peek = %d, want 2", s.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-S11  Peek 空栈返回 StackErrEmpty                                 //
// ------------------------------------------------------------------ //

func TestSTC11_ListStack_PeekEmpty(t *testing.T) {
	s := NewListStack[float64]()
	_, err := s.Peek()
	if err != StackErrEmpty {
		t.Fatalf("Peek() on empty stack: got %v, want StackErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S12  Clear 后栈为空且仍可复用                                     //
// ------------------------------------------------------------------ //

func TestSTC12_ListStack_Clear(t *testing.T) {
	s := NewListStack[int]()
	s.Push(10)
	s.Push(20)
	s.Clear()

	if !s.IsEmpty() || s.Len() != 0 {
		t.Fatal("stack should be empty after Clear")
	}
	s.Push(42)
	got, _ := s.Pop()
	if got != 42 {
		t.Errorf("Pop after Clear = %d, want 42", got)
	}
}

// ------------------------------------------------------------------ //
//  TC-S13  泛型：结构体元素                                             //
// ------------------------------------------------------------------ //

func TestSTC13_ListStack_StructElem(t *testing.T) {
	type Point struct{ x, y int }
	s := NewListStack[Point]()
	s.Push(Point{1, 2})
	s.Push(Point{3, 4})

	got, err := s.Pop()
	if err != nil || got != (Point{3, 4}) {
		t.Fatalf("Pop() = (%v, %v), want ({3 4}, nil)", got, err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S14  两种实现行为一致性对比                                        //
// ------------------------------------------------------------------ //

func TestSTC14_Consistency(t *testing.T) {
	const n = 20
	as := NewArrayStack[int]()
	ls := NewListStack[int]()

	for i := range n {
		as.Push(i)
		ls.Push(i)
	}

	for range n {
		av, ae := as.Pop()
		lv, le := ls.Pop()
		if ae != le || av != lv {
			t.Fatalf("ArrayStack.Pop()=(%d,%v) ListStack.Pop()=(%d,%v) mismatch",
				av, ae, lv, le)
		}
	}
}

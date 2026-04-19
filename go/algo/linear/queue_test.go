package linear

import "testing"

// ------------------------------------------------------------------ //
//  TC-Q01  FIFO 基本正确性                                             //
// ------------------------------------------------------------------ //

func TestQTC01_FIFO(t *testing.T) {
	q := NewQueue[int]()
	const n = 5
	for i := range n {
		q.Enqueue(i)
	}
	if q.Len() != n {
		t.Fatalf("Len = %d, want %d", q.Len(), n)
	}

	// 先进先出：出队顺序应为 0 1 2 3 4
	for want := range n {
		got, err := q.Dequeue()
		if err != nil {
			t.Fatalf("Dequeue(): %v", err)
		}
		if got != want {
			t.Errorf("Dequeue() = %d, want %d", got, want)
		}
	}
	if !q.IsEmpty() {
		t.Fatal("queue should be empty after all dequeues")
	}
}

// ------------------------------------------------------------------ //
//  TC-Q02  Dequeue 空队列返回 QueueErrEmpty                            //
// ------------------------------------------------------------------ //

func TestQTC02_DequeueEmpty(t *testing.T) {
	q := NewQueue[int]()
	_, err := q.Dequeue()
	if err != QueueErrEmpty {
		t.Fatalf("Dequeue() on empty queue: got %v, want QueueErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-Q03  PeekFront 不改变 Len 和队列内容                              //
// ------------------------------------------------------------------ //

func TestQTC03_PeekFront(t *testing.T) {
	q := NewQueue[string]()
	q.Enqueue("a")
	q.Enqueue("b")

	got, err := q.PeekFront()
	if err != nil || got != "a" {
		t.Fatalf("PeekFront() = (%q, %v), want (\"a\", nil)", got, err)
	}
	if q.Len() != 2 {
		t.Fatalf("Len after PeekFront = %d, want 2", q.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-Q04  单元素队列：PeekFront == PeekBack                           //
// ------------------------------------------------------------------ //

func TestQTC04_SingleElement(t *testing.T) {
	q := NewQueue[int]()
	q.Enqueue(42)

	front, _ := q.PeekFront()
	back, _ := q.PeekBack()
	if front != back || front != 42 {
		t.Fatalf("single-element: front=%d back=%d, want both 42", front, back)
	}
}

// ------------------------------------------------------------------ //
//  TC-Q05  交替 Enqueue/Dequeue，Len 始终准确                          //
// ------------------------------------------------------------------ //

func TestQTC05_InterleavedEnqueueDequeue(t *testing.T) {
	q := NewQueue[int]()
	for i := range 10 {
		q.Enqueue(i)
		if q.Len() != 1 {
			t.Fatalf("Len after Enqueue = %d, want 1", q.Len())
		}
		got, err := q.Dequeue()
		if err != nil || got != i {
			t.Fatalf("Dequeue() = (%d, %v), want (%d, nil)", got, err, i)
		}
		if q.Len() != 0 {
			t.Fatalf("Len after Dequeue = %d, want 0", q.Len())
		}
	}
}

// ------------------------------------------------------------------ //
//  TC-Q06  PeekFront / PeekBack 空队列返回 QueueErrEmpty               //
// ------------------------------------------------------------------ //

func TestQTC06_PeekEmpty(t *testing.T) {
	q := NewQueue[int]()
	if _, err := q.PeekFront(); err != QueueErrEmpty {
		t.Fatalf("PeekFront() on empty: got %v, want QueueErrEmpty", err)
	}
	if _, err := q.PeekBack(); err != QueueErrEmpty {
		t.Fatalf("PeekBack() on empty: got %v, want QueueErrEmpty", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-Q07  Clear 后 Len == 0，再次 Enqueue 正常                        //
// ------------------------------------------------------------------ //

func TestQTC07_Clear(t *testing.T) {
	q := NewQueue[int]()
	for i := range 5 {
		q.Enqueue(i)
	}
	q.Clear()
	if !q.IsEmpty() || q.Len() != 0 {
		t.Fatal("queue should be empty after Clear")
	}
	q.Enqueue(99)
	got, _ := q.Dequeue()
	if got != 99 {
		t.Errorf("Dequeue after Clear = %d, want 99", got)
	}
}

// ------------------------------------------------------------------ //
//  TC-Q08  ForEach 遍历顺序从队首到队尾                                 //
// ------------------------------------------------------------------ //

func TestQTC08_ForEach(t *testing.T) {
	q := NewQueue[int]()
	for i := range 5 {
		q.Enqueue(i)
	}
	var got []int
	q.ForEach(func(v int) { got = append(got, v) })
	for i, v := range got {
		if v != i {
			t.Errorf("ForEach[%d] = %d, want %d", i, v, i)
		}
	}
	// ForEach 不出队
	if q.Len() != 5 {
		t.Fatalf("Len after ForEach = %d, want 5", q.Len())
	}
}

// ------------------------------------------------------------------ //
//  TC-Q09  All() 迭代器覆盖所有元素                                     //
// ------------------------------------------------------------------ //

func TestQTC09_All(t *testing.T) {
	q := NewQueue[int]()
	for i := range 4 {
		q.Enqueue(i * 10)
	}
	idx := 0
	for v := range q.All() {
		want := idx * 10
		if v != want {
			t.Errorf("All()[%d] = %d, want %d", idx, v, want)
		}
		idx++
	}
	if idx != 4 {
		t.Fatalf("All() yielded %d elements, want 4", idx)
	}
}

// ------------------------------------------------------------------ //
//  TC-Q10  泛型：结构体元素                                             //
// ------------------------------------------------------------------ //

func TestQTC10_StructElement(t *testing.T) {
	type Task struct{ id, priority int }
	q := NewQueue[Task]()
	q.Enqueue(Task{1, 5})
	q.Enqueue(Task{2, 3})

	got, err := q.Dequeue()
	if err != nil || got != (Task{1, 5}) {
		t.Fatalf("Dequeue() = (%v, %v), want ({1 5}, nil)", got, err)
	}
}

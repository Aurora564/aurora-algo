package linear

import (
	"slices"
	"testing"
)

// TC-01: empty tree search → false
func TestBST_EmptySearch(t *testing.T) {
	bst := NewBST[int, string]()
	_, ok := bst.Search(1)
	if ok {
		t.Fatal("expected false on empty tree")
	}
	if !bst.IsEmpty() {
		t.Fatal("expected IsEmpty true")
	}
}

// TC-02: insert 5 unordered keys → in-order returns strictly ascending
func TestBST_InOrderAscending(t *testing.T) {
	bst := NewBST[int, int]()
	keys := []int{3, 1, 4, 1, 5} // note duplicate 1
	for _, k := range keys {
		bst.Insert(k, k)
	}
	var got []int
	bst.InOrder(func(k, _ int) { got = append(got, k) })
	want := []int{1, 3, 4, 5}
	if !slices.Equal(got, want) {
		t.Fatalf("in-order = %v, want %v", got, want)
	}
}

// TC-03: search existing key → correct value
func TestBST_SearchFound(t *testing.T) {
	bst := NewBST[string, int]()
	bst.Insert("hello", 42)
	v, ok := bst.Search("hello")
	if !ok || v != 42 {
		t.Fatalf("got (%d, %v), want (42, true)", v, ok)
	}
}

// TC-04: search missing key → false
func TestBST_SearchMissing(t *testing.T) {
	bst := NewBST[int, int]()
	bst.Insert(1, 1)
	_, ok := bst.Search(99)
	if ok {
		t.Fatal("expected false for missing key")
	}
}

// TC-05: delete leaf → len decreases, tree valid
func TestBST_DeleteLeaf(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{5, 3, 7} {
		bst.Insert(k, k)
	}
	ok := bst.Delete(3)
	if !ok {
		t.Fatal("expected delete to return true")
	}
	if bst.Len() != 2 {
		t.Fatalf("len = %d, want 2", bst.Len())
	}
	if !bst.IsValid() {
		t.Fatal("BST invariant violated after leaf delete")
	}
}

// TC-06: delete node with one child → len decreases, tree valid
func TestBST_DeleteOneChild(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{5, 3, 7, 6} {
		bst.Insert(k, k)
	}
	ok := bst.Delete(7) // has left child 6
	if !ok {
		t.Fatal("expected delete to return true")
	}
	if bst.Len() != 3 {
		t.Fatalf("len = %d, want 3", bst.Len())
	}
	if !bst.IsValid() {
		t.Fatal("BST invariant violated after one-child delete")
	}
	if bst.Contains(7) {
		t.Fatal("deleted key still found")
	}
}

// TC-07: delete node with two children → len decreases, tree valid, in-order correct
func TestBST_DeleteTwoChildren(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{5, 3, 7, 1, 4, 6, 9} {
		bst.Insert(k, k)
	}
	ok := bst.Delete(5)
	if !ok {
		t.Fatal("expected delete to return true")
	}
	if bst.Len() != 6 {
		t.Fatalf("len = %d, want 6", bst.Len())
	}
	if !bst.IsValid() {
		t.Fatal("BST invariant violated after two-child delete")
	}
	var got []int
	bst.InOrder(func(k, _ int) { got = append(got, k) })
	want := []int{1, 3, 4, 6, 7, 9}
	if !slices.Equal(got, want) {
		t.Fatalf("in-order = %v, want %v", got, want)
	}
}

// TC-08: delete root in various configurations
func TestBST_DeleteRoot(t *testing.T) {
	// root with no children
	t.Run("no_children", func(t *testing.T) {
		bst := NewBST[int, int]()
		bst.Insert(1, 1)
		bst.Delete(1)
		if bst.Len() != 0 || !bst.IsEmpty() {
			t.Fatal("tree should be empty")
		}
	})
	// root with left child only
	t.Run("left_only", func(t *testing.T) {
		bst := NewBST[int, int]()
		bst.Insert(5, 5)
		bst.Insert(3, 3)
		bst.Delete(5)
		if bst.Len() != 1 || !bst.IsValid() {
			t.Fatal("invalid after root delete (left only)")
		}
	})
	// root with two children
	t.Run("two_children", func(t *testing.T) {
		bst := NewBST[int, int]()
		for _, k := range []int{5, 2, 8} {
			bst.Insert(k, k)
		}
		bst.Delete(5)
		if bst.Len() != 2 || !bst.IsValid() {
			t.Fatal("invalid after root delete (two children)")
		}
	})
}

// TC-09: duplicate insert → len unchanged, value updated
func TestBST_DuplicateInsert(t *testing.T) {
	bst := NewBST[int, string]()
	bst.Insert(1, "first")
	bst.Insert(1, "second")
	if bst.Len() != 1 {
		t.Fatalf("len = %d, want 1", bst.Len())
	}
	v, _ := bst.Search(1)
	if v != "second" {
		t.Fatalf("value = %q, want %q", v, "second")
	}
}

// TC-10: Min / Max return correct key-value
func TestBST_MinMax(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{5, 3, 7, 1, 9} {
		bst.Insert(k, k)
	}
	minK, minV, ok := bst.Min()
	if !ok || minK != 1 || minV != 1 {
		t.Fatalf("Min = (%d,%d,%v), want (1,1,true)", minK, minV, ok)
	}
	maxK, maxV, ok := bst.Max()
	if !ok || maxK != 9 || maxV != 9 {
		t.Fatalf("Max = (%d,%d,%v), want (9,9,true)", maxK, maxV, ok)
	}
	// empty tree
	empty := NewBST[int, int]()
	_, _, ok = empty.Min()
	if ok {
		t.Fatal("Min on empty tree should return false")
	}
}

// TC-11: successor / predecessor boundaries
func TestBST_SuccessorPredecessor(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{2, 1, 3} {
		bst.Insert(k, k)
	}
	// successor of max (3) → false
	_, _, ok := bst.Successor(3)
	if ok {
		t.Fatal("successor of max should not exist")
	}
	// predecessor of min (1) → false
	_, _, ok = bst.Predecessor(1)
	if ok {
		t.Fatal("predecessor of min should not exist")
	}
	// normal cases
	sk, _, ok := bst.Successor(1)
	if !ok || sk != 2 {
		t.Fatalf("Successor(1) = (%d, %v), want (2, true)", sk, ok)
	}
	pk, _, ok := bst.Predecessor(3)
	if !ok || pk != 2 {
		t.Fatalf("Predecessor(3) = (%d, %v), want (2, true)", pk, ok)
	}
}

// TC-12: sequential insert 1…1000 → height ≈ 1000, in-order still correct
func TestBST_Degenerate(t *testing.T) {
	bst := NewBST[int, int]()
	const n = 1000
	for i := 1; i <= n; i++ {
		bst.Insert(i, i)
	}
	h := bst.Height()
	if h < n/2 {
		t.Fatalf("height %d too small for degenerate tree", h)
	}
	if bst.Len() != n {
		t.Fatalf("len = %d, want %d", bst.Len(), n)
	}
	prev := 0
	bst.InOrder(func(k, _ int) {
		if k <= prev {
			t.Fatalf("in-order not ascending: %d after %d", k, prev)
		}
		prev = k
	})
}

// TC-13: BST has no built-in clone — verify independent subtrees by delete
func TestBST_Independence(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{5, 3, 7} {
		bst.Insert(k, k)
	}
	bst.Delete(3)
	if bst.Contains(3) {
		t.Fatal("deleted key still present")
	}
	if !bst.Contains(5) || !bst.Contains(7) {
		t.Fatal("unrelated keys removed")
	}
}

// TC-14: level-order traversal is BFS order
func TestBST_LevelOrder(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{4, 2, 6, 1, 3, 5, 7} {
		bst.Insert(k, k)
	}
	var got []int
	bst.LevelOrder(func(k, _ int) { got = append(got, k) })
	want := []int{4, 2, 6, 1, 3, 5, 7}
	if !slices.Equal(got, want) {
		t.Fatalf("level-order = %v, want %v", got, want)
	}
}

// InOrderChan test
func TestBST_InOrderChan(t *testing.T) {
	bst := NewBST[int, int]()
	for _, k := range []int{3, 1, 4, 1, 5, 9, 2, 6} {
		bst.Insert(k, k)
	}
	var got []int
	for kv := range bst.InOrderChan() {
		got = append(got, kv.Key)
	}
	if !slices.IsSorted(got) {
		t.Fatalf("InOrderChan not sorted: %v", got)
	}
}

// Delete missing key → returns false
func TestBST_DeleteMissing(t *testing.T) {
	bst := NewBST[int, int]()
	bst.Insert(1, 1)
	if bst.Delete(99) {
		t.Fatal("expected false for deleting missing key")
	}
	if bst.Len() != 1 {
		t.Fatal("len changed after failed delete")
	}
}

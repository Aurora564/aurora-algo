package linear

import (
	"slices"
	"testing"
)

// TC-01: empty tree search → false
func TestAVL_EmptySearch(t *testing.T) {
	var avl AVL[int, string]
	_, ok := avl.Search(1)
	if ok {
		t.Fatal("expected false on empty tree")
	}
	if !avl.Empty() {
		t.Fatal("expected Empty true")
	}
}

// TC-02: insert 5 unordered keys → in-order returns strictly ascending
func TestAVL_InOrderAscending(t *testing.T) {
	var avl AVL[int, int]
	for _, k := range []int{3, 1, 4, 5, 2} {
		avl.Insert(k, k)
	}
	var got []int
	avl.InOrder(func(k, _ int) { got = append(got, k) })
	want := []int{1, 2, 3, 4, 5}
	if !slices.Equal(got, want) {
		t.Fatalf("in-order = %v, want %v", got, want)
	}
}

// TC-03: search existing key → correct value
func TestAVL_SearchFound(t *testing.T) {
	var avl AVL[int, int]
	avl.Insert(42, 99)
	v, ok := avl.Search(42)
	if !ok || v != 99 {
		t.Fatalf("got (%d, %v), want (99, true)", v, ok)
	}
}

// TC-04: search missing key → false
func TestAVL_SearchMissing(t *testing.T) {
	var avl AVL[int, int]
	avl.Insert(1, 1)
	_, ok := avl.Search(99)
	if ok {
		t.Fatal("expected false for missing key")
	}
}

// TC-05: delete leaf → len decreases, tree balanced + valid
func TestAVL_DeleteLeaf(t *testing.T) {
	var avl AVL[int, int]
	for _, k := range []int{5, 3, 7} {
		avl.Insert(k, k)
	}
	ok := avl.Delete(3)
	if !ok {
		t.Fatal("expected delete to return true")
	}
	if avl.Len() != 2 {
		t.Fatalf("len = %d, want 2", avl.Len())
	}
	if !avl.IsBalanced() || !avl.IsValidBST() {
		t.Fatal("AVL invariant violated after leaf delete")
	}
	if avl.Contains(3) {
		t.Fatal("deleted key still found")
	}
}

// TC-06: delete node with one child → invariants hold
func TestAVL_DeleteOneChild(t *testing.T) {
	var avl AVL[int, int]
	for _, k := range []int{5, 3, 7, 6} {
		avl.Insert(k, k)
	}
	ok := avl.Delete(7)
	if !ok {
		t.Fatal("expected delete to return true")
	}
	if avl.Len() != 3 {
		t.Fatalf("len = %d, want 3", avl.Len())
	}
	if !avl.IsBalanced() || !avl.IsValidBST() {
		t.Fatal("AVL invariant violated after one-child delete")
	}
	if avl.Contains(7) {
		t.Fatal("deleted key still found")
	}
}

// TC-07: delete two-child node → in-order and invariants correct
func TestAVL_DeleteTwoChildren(t *testing.T) {
	var avl AVL[int, int]
	for _, k := range []int{5, 3, 7, 1, 4, 6, 9} {
		avl.Insert(k, k)
	}
	ok := avl.Delete(5)
	if !ok {
		t.Fatal("expected delete to return true")
	}
	if avl.Len() != 6 {
		t.Fatalf("len = %d, want 6", avl.Len())
	}
	if !avl.IsBalanced() || !avl.IsValidBST() {
		t.Fatal("AVL invariant violated after two-child delete")
	}
	var got []int
	avl.InOrder(func(k, _ int) { got = append(got, k) })
	want := []int{1, 3, 4, 6, 7, 9}
	if !slices.Equal(got, want) {
		t.Fatalf("in-order = %v, want %v", got, want)
	}
}

// TC-08: delete root
func TestAVL_DeleteRoot(t *testing.T) {
	t.Run("single_node", func(t *testing.T) {
		var avl AVL[int, int]
		avl.Insert(1, 1)
		avl.Delete(1)
		if avl.Len() != 0 || !avl.Empty() {
			t.Fatal("tree should be empty")
		}
	})
	t.Run("left_only", func(t *testing.T) {
		var avl AVL[int, int]
		avl.Insert(5, 5)
		avl.Insert(3, 3)
		avl.Delete(5)
		if avl.Len() != 1 || !avl.IsBalanced() || !avl.IsValidBST() {
			t.Fatal("invalid after root delete (left only)")
		}
	})
	t.Run("two_children", func(t *testing.T) {
		var avl AVL[int, int]
		for _, k := range []int{5, 2, 8} {
			avl.Insert(k, k)
		}
		avl.Delete(5)
		if avl.Len() != 2 || !avl.IsBalanced() || !avl.IsValidBST() {
			t.Fatal("invalid after root delete (two children)")
		}
	})
}

// TC-09: duplicate insert → len unchanged, value updated
func TestAVL_DuplicateInsert(t *testing.T) {
	var avl AVL[int, string]
	avl.Insert(1, "first")
	avl.Insert(1, "second")
	if avl.Len() != 1 {
		t.Fatalf("len = %d, want 1", avl.Len())
	}
	v, _ := avl.Search(1)
	if v != "second" {
		t.Fatalf("value = %q, want %q", v, "second")
	}
}

// TC-10: Min / Max
func TestAVL_MinMax(t *testing.T) {
	var avl AVL[int, int]
	for _, k := range []int{5, 3, 7, 1, 9} {
		avl.Insert(k, k)
	}
	minK, minV, ok := avl.Min()
	if !ok || minK != 1 || minV != 1 {
		t.Fatalf("Min = (%d,%d,%v), want (1,1,true)", minK, minV, ok)
	}
	maxK, maxV, ok := avl.Max()
	if !ok || maxK != 9 || maxV != 9 {
		t.Fatalf("Max = (%d,%d,%v), want (9,9,true)", maxK, maxV, ok)
	}
	var empty AVL[int, int]
	_, _, ok = empty.Min()
	if ok {
		t.Fatal("Min on empty tree should return false")
	}
	_, _, ok = empty.Max()
	if ok {
		t.Fatal("Max on empty tree should return false")
	}
}

// TC-11: sequential insert → height stays O(log n), tree balanced
func TestAVL_BalancedAfterSequentialInsert(t *testing.T) {
	var avl AVL[int, int]
	const n = 1000
	for i := 1; i <= n; i++ {
		avl.Insert(i, i)
	}
	if avl.Len() != n {
		t.Fatalf("len = %d, want %d", avl.Len(), n)
	}
	h := avl.Height()
	if h > 22 { // 1.44 * log2(1002) ≈ 14.4; be generous
		t.Fatalf("height %d too large for balanced tree of %d nodes", h, n)
	}
	if !avl.IsBalanced() {
		t.Fatal("tree not balanced after sequential inserts")
	}
	if !avl.IsValidBST() {
		t.Fatal("BST property violated after sequential inserts")
	}
}

// TC-12: random-order insert + delete, invariants hold throughout
func TestAVL_InvariantsAfterMixedOps(t *testing.T) {
	var avl AVL[int, int]
	keys := []int{10, 5, 15, 3, 7, 12, 20, 1, 4, 6, 8, 11, 13, 18, 25}
	for _, k := range keys {
		avl.Insert(k, k)
		if !avl.IsBalanced() || !avl.IsValidBST() {
			t.Fatalf("invariant violated after insert %d", k)
		}
	}
	deletes := []int{5, 15, 1, 20, 10}
	for _, k := range deletes {
		avl.Delete(k)
		if !avl.IsBalanced() || !avl.IsValidBST() {
			t.Fatalf("invariant violated after delete %d", k)
		}
	}
}

// TC-13: InOrderChan returns ascending order
func TestAVL_InOrderChan(t *testing.T) {
	var avl AVL[int, int]
	for _, k := range []int{3, 1, 4, 1, 5, 9, 2, 6} {
		avl.Insert(k, k)
	}
	var got []int
	for kv := range avl.InOrderChan() {
		got = append(got, kv.Key)
	}
	if !slices.IsSorted(got) {
		t.Fatalf("InOrderChan not sorted: %v", got)
	}
}

// TC-14: delete missing key → returns false, len unchanged
func TestAVL_DeleteMissing(t *testing.T) {
	var avl AVL[int, int]
	avl.Insert(1, 1)
	if avl.Delete(99) {
		t.Fatal("expected false for deleting missing key")
	}
	if avl.Len() != 1 {
		t.Fatal("len changed after failed delete")
	}
}

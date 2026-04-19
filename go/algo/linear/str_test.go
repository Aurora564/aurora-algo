package linear

import (
	"testing"
)

// ------------------------------------------------------------------ //
//  TC-S01  ASCII 字符串：len_bytes == len_chars == 5                   //
// ------------------------------------------------------------------ //

func TestSTC01_ASCIILength(t *testing.T) {
	s, err := StrFromString("hello")
	if err != nil {
		t.Fatal(err)
	}
	if s.LenBytes() != 5 {
		t.Errorf("LenBytes = %d, want 5", s.LenBytes())
	}
	if s.LenChars() != 5 {
		t.Errorf("LenChars = %d, want 5", s.LenChars())
	}
	if s.AsString() != "hello" {
		t.Errorf("AsString = %q, want %q", s.AsString(), "hello")
	}
}

// ------------------------------------------------------------------ //
//  TC-S02  中文字符串：len_bytes=6，len_chars=2                         //
// ------------------------------------------------------------------ //

func TestSTC02_CJKLength(t *testing.T) {
	s, err := StrFromString("你好")
	if err != nil {
		t.Fatal(err)
	}
	if s.LenBytes() != 6 {
		t.Errorf("LenBytes = %d, want 6", s.LenBytes())
	}
	if s.LenChars() != 2 {
		t.Errorf("LenChars = %d, want 2", s.LenChars())
	}
}

// ------------------------------------------------------------------ //
//  TC-S03  非法 UTF-8 → StrErrInvalidUTF8                                //
// ------------------------------------------------------------------ //

func TestSTC03_InvalidUTF8(t *testing.T) {
	invalidBytes := []byte{0xff, 0xfe, 0x00} // 非法 UTF-8
	_, err := StrFromBytes(invalidBytes)
	if err != StrErrInvalidUTF8 {
		t.Fatalf("StrFromBytes(invalid): got %v, want StrErrInvalidUTF8", err)
	}

	// Append 非法字节串也应该拒绝
	s := NewStr()
	appendErr := s.Append(string([]byte{0x80, 0x81})) // 孤立续字节
	if appendErr != StrErrInvalidUTF8 {
		t.Fatalf("Append(invalid): got %v, want StrErrInvalidUTF8", appendErr)
	}
}

// ------------------------------------------------------------------ //
//  TC-S04  GetChar(1) on "你好" 返回 '好'（U+597D）                     //
// ------------------------------------------------------------------ //

func TestSTC04_GetChar(t *testing.T) {
	s, _ := StrFromString("你好")
	r, err := s.GetChar(1)
	if err != nil {
		t.Fatalf("GetChar(1): %v", err)
	}
	if r != '好' {
		t.Errorf("GetChar(1) = %c (%U), want 好 (U+597D)", r, r)
	}

	// 越界
	_, err = s.GetChar(2)
	if err != StrErrOutOfBounds {
		t.Fatalf("GetChar(2) on 2-char string: got %v, want StrErrOutOfBounds", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S05  Slice(0, 3) on "你好" 返回 "你"                             //
// ------------------------------------------------------------------ //

func TestSTC05_SliceValid(t *testing.T) {
	s, _ := StrFromString("你好")
	got, err := s.Slice(0, 3)
	if err != nil {
		t.Fatalf("Slice(0,3): %v", err)
	}
	if got != "你" {
		t.Errorf("Slice(0,3) = %q, want %q", got, "你")
	}
}

// ------------------------------------------------------------------ //
//  TC-S06  Slice(0, 2) on "你好" → StrErrBadBoundary（截断多字节字符）     //
// ------------------------------------------------------------------ //

func TestSTC06_SliceBadBoundary(t *testing.T) {
	s, _ := StrFromString("你好")
	_, err := s.Slice(0, 2)
	if err != StrErrBadBoundary {
		t.Fatalf("Slice(0,2): got %v, want StrErrBadBoundary", err)
	}
}

// ------------------------------------------------------------------ //
//  TC-S07  Reserve 确保容量增长；Len 不变                               //
// ------------------------------------------------------------------ //

func TestSTC07_Reserve(t *testing.T) {
	s, _ := StrFromString("hi")
	before := s.LenBytes()
	s.Reserve(100)
	if s.LenBytes() != before {
		t.Errorf("LenBytes after Reserve = %d, want %d", s.LenBytes(), before)
	}
	if s.Cap() < before+100 {
		t.Errorf("Cap = %d, want >= %d", s.Cap(), before+100)
	}
	// 追加后内容仍正确
	_ = s.Append(" world")
	if s.AsString() != "hi world" {
		t.Errorf("after Reserve+Append: %q, want %q", s.AsString(), "hi world")
	}
}

// ------------------------------------------------------------------ //
//  TC-S08  AsString 零拷贝与多次 Append 后内容正确                      //
// ------------------------------------------------------------------ //

func TestSTC08_AsStringCorrectness(t *testing.T) {
	s := NewStr()
	parts := []string{"foo", "bar", "baz"}
	for _, p := range parts {
		if err := s.Append(p); err != nil {
			t.Fatalf("Append(%q): %v", p, err)
		}
	}
	want := "foobarbaz"
	if s.AsString() != want {
		t.Errorf("AsString = %q, want %q", s.AsString(), want)
	}
	if s.LenBytes() != len(want) {
		t.Errorf("LenBytes = %d, want %d", s.LenBytes(), len(want))
	}
}

// ------------------------------------------------------------------ //
//  TC-S09  AppendChar(U+1F600 😀)：len_bytes += 4，len_chars += 1      //
// ------------------------------------------------------------------ //

func TestSTC09_AppendCharEmoji(t *testing.T) {
	s, _ := StrFromString("hi")
	bytesBefore := s.LenBytes()
	charsBefore := s.LenChars()

	s.AppendChar(0x1F600) // 😀，4 字节 UTF-8

	if s.LenBytes() != bytesBefore+4 {
		t.Errorf("LenBytes = %d, want %d", s.LenBytes(), bytesBefore+4)
	}
	if s.LenChars() != charsBefore+1 {
		t.Errorf("LenChars = %d, want %d", s.LenChars(), charsBefore+1)
	}
	if s.AsString() != "hi😀" {
		t.Errorf("AsString = %q, want %q", s.AsString(), "hi😀")
	}
}

// ------------------------------------------------------------------ //
//  TC-S10  Trim 移除首尾空白                                           //
// ------------------------------------------------------------------ //

func TestSTC10_Trim(t *testing.T) {
	s, _ := StrFromString("  hello  ")
	trimmed := s.Trim()
	if trimmed.AsString() != "hello" {
		t.Errorf("Trim = %q, want %q", trimmed.AsString(), "hello")
	}
	// 原字符串不变
	if s.AsString() != "  hello  " {
		t.Errorf("original changed to %q", s.AsString())
	}
}

// ------------------------------------------------------------------ //
//  TC-S11  Find("abcabc", "bc") 返回字节偏移 1                         //
// ------------------------------------------------------------------ //

func TestSTC11_Find(t *testing.T) {
	s, _ := StrFromString("abcabc")
	offset, found := s.Find("bc")
	if !found {
		t.Fatal("Find(\"bc\"): not found")
	}
	if offset != 1 {
		t.Errorf("Find(\"bc\") = %d, want 1", offset)
	}

	_, found2 := s.Find("xyz")
	if found2 {
		t.Error("Find(\"xyz\"): should not be found")
	}

	if !s.Contains("abc") {
		t.Error("Contains(\"abc\") should be true")
	}
	if !s.StartsWith("ab") {
		t.Error("StartsWith(\"ab\") should be true")
	}
	if !s.EndsWith("bc") {
		t.Error("EndsWith(\"bc\") should be true")
	}
}

// ------------------------------------------------------------------ //
//  TC-S12  相同内容的两个实例 Hash 值相同                               //
// ------------------------------------------------------------------ //

func TestSTC12_HashEqual(t *testing.T) {
	s1, _ := StrFromString("aurora")
	s2, _ := StrFromString("aurora")

	if s1.Hash() != s2.Hash() {
		t.Errorf("Hash mismatch: %d != %d", s1.Hash(), s2.Hash())
	}
	if !s1.Equal(s2) {
		t.Error("Equal should be true for same content")
	}

	s3, _ := StrFromString("algo")
	if s1.Hash() == s3.Hash() {
		t.Log("hash collision (unlikely but possible)")
	}
	if s1.Equal(s3) {
		t.Error("Equal should be false for different content")
	}
}

// ------------------------------------------------------------------ //
//  TC-S13  扩容后 AsString 内容正确                                    //
// ------------------------------------------------------------------ //

func TestSTC13_GrowAndAsString(t *testing.T) {
	s := NewStr()
	const n = 50
	for i := range n {
		_ = s.Append("x")
		if s.LenBytes() != i+1 {
			t.Fatalf("LenBytes after %d appends = %d, want %d", i+1, s.LenBytes(), i+1)
		}
	}
	want := ""
	for range n {
		want += "x"
	}
	if s.AsString() != want {
		t.Errorf("AsString after growth: got len %d, want len %d", len(s.AsString()), len(want))
	}
}

// ------------------------------------------------------------------ //
//  TC-S14  Clear 后 len_bytes=0，再 Append 正常                        //
// ------------------------------------------------------------------ //

func TestSTC14_ClearAndReuse(t *testing.T) {
	s, _ := StrFromString("some content")
	s.Clear()
	if s.LenBytes() != 0 {
		t.Fatalf("LenBytes after Clear = %d, want 0", s.LenBytes())
	}
	if !s.IsEmpty() {
		t.Fatal("IsEmpty after Clear should be true")
	}
	if err := s.Append("new"); err != nil {
		t.Fatalf("Append after Clear: %v", err)
	}
	if s.AsString() != "new" {
		t.Errorf("AsString after Clear+Append = %q, want %q", s.AsString(), "new")
	}
}

// ------------------------------------------------------------------ //
//  附加：Insert / RemoveRange / Split / Compare / ShrinkToFit         //
// ------------------------------------------------------------------ //

func TestSTC_Insert(t *testing.T) {
	s, _ := StrFromString("helo")
	if err := s.Insert(3, "l"); err != nil {
		t.Fatalf("Insert: %v", err)
	}
	if s.AsString() != "hello" {
		t.Errorf("after Insert: %q, want %q", s.AsString(), "hello")
	}

	// 在多字节字符边界插入
	s2, _ := StrFromString("你世界")
	if err := s2.Insert(3, "好"); err != nil { // 字节偏移 3 是 "世" 的起点
		t.Fatalf("Insert CJK: %v", err)
	}
	if s2.AsString() != "你好世界" {
		t.Errorf("after CJK Insert: %q, want %q", s2.AsString(), "你好世界")
	}

	// 非码点边界插入 → StrErrBadBoundary
	s3, _ := StrFromString("你好")
	err := s3.Insert(1, "x") // 字节 1 是续字节
	if err != StrErrBadBoundary {
		t.Fatalf("Insert at bad boundary: got %v, want StrErrBadBoundary", err)
	}
}

func TestSTC_RemoveRange(t *testing.T) {
	s, _ := StrFromString("hello world")
	if err := s.RemoveRange(5, 11); err != nil {
		t.Fatalf("RemoveRange: %v", err)
	}
	if s.AsString() != "hello" {
		t.Errorf("after RemoveRange: %q, want %q", s.AsString(), "hello")
	}

	// 跨多字节字符删除
	s2, _ := StrFromString("你好世界")
	if err := s2.RemoveRange(3, 6); err != nil { // 删除 "好"
		t.Fatalf("RemoveRange CJK: %v", err)
	}
	if s2.AsString() != "你世界" {
		t.Errorf("after CJK RemoveRange: %q, want %q", s2.AsString(), "你世界")
	}

	// 非码点边界 → StrErrBadBoundary
	s3, _ := StrFromString("你好")
	err := s3.RemoveRange(1, 3)
	if err != StrErrBadBoundary {
		t.Fatalf("RemoveRange bad boundary: got %v, want StrErrBadBoundary", err)
	}
}

func TestSTC_Split(t *testing.T) {
	s, _ := StrFromString("a,b,c")
	parts := s.Split(",")
	if len(parts) != 3 || parts[0] != "a" || parts[1] != "b" || parts[2] != "c" {
		t.Errorf("Split = %v, want [a b c]", parts)
	}
}

func TestSTC_Compare(t *testing.T) {
	a, _ := StrFromString("apple")
	b, _ := StrFromString("banana")
	c, _ := StrFromString("apple")

	if a.Compare(b) != -1 {
		t.Errorf("Compare(apple, banana) = %d, want -1", a.Compare(b))
	}
	if b.Compare(a) != 1 {
		t.Errorf("Compare(banana, apple) = %d, want 1", b.Compare(a))
	}
	if a.Compare(c) != 0 {
		t.Errorf("Compare(apple, apple) = %d, want 0", a.Compare(c))
	}
}

func TestSTC_ToUpperLower(t *testing.T) {
	s, _ := StrFromString("Hello World")
	s.ToUpper()
	if s.AsString() != "HELLO WORLD" {
		t.Errorf("ToUpper = %q, want %q", s.AsString(), "HELLO WORLD")
	}
	s.ToLower()
	if s.AsString() != "hello world" {
		t.Errorf("ToLower = %q, want %q", s.AsString(), "hello world")
	}
}

func TestSTC_ShrinkToFit(t *testing.T) {
	s, _ := StrFromString("hello")
	s.Reserve(200)
	s.ShrinkToFit()
	if s.Cap() != s.LenBytes() {
		t.Errorf("after ShrinkToFit: Cap=%d, LenBytes=%d", s.Cap(), s.LenBytes())
	}
	if s.AsString() != "hello" {
		t.Errorf("ShrinkToFit changed content: %q", s.AsString())
	}
}

func TestSTC_ByteOffset(t *testing.T) {
	s, _ := StrFromString("你好世界") // 4 chars, 12 bytes
	offsets := []int{0, 3, 6, 9, 12}
	for i, want := range offsets {
		got, err := s.ByteOffset(i)
		if err != nil {
			t.Fatalf("ByteOffset(%d): %v", i, err)
		}
		if got != want {
			t.Errorf("ByteOffset(%d) = %d, want %d", i, got, want)
		}
	}
	_, err := s.ByteOffset(5)
	if err != StrErrOutOfBounds {
		t.Fatalf("ByteOffset(5) on 4-char string: got %v, want StrErrOutOfBounds", err)
	}
}

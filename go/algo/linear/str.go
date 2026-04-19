package linear

import (
	"bytes"
	"hash/fnv"
	"strings"
	"unicode/utf8"
	"unsafe"
)

// ------------------------------------------------------------------ //
//  错误类型                                                            //
// ------------------------------------------------------------------ //

// StrError 是 Str 操作返回的错误类型。
type StrError int

const (
	StrErrInvalidUTF8 StrError = iota + 1 // 输入字节序列不是合法 UTF-8
	StrErrOutOfBounds                     // 码点索引或字节偏移超出范围
	StrErrBadBoundary                     // 字节位置未落在码点边界上
)

func (e StrError) Error() string {
	switch e {
	case StrErrInvalidUTF8:
		return "str: invalid UTF-8 sequence"
	case StrErrOutOfBounds:
		return "str: index out of bounds"
	case StrErrBadBoundary:
		return "str: byte position not on a UTF-8 rune boundary"
	default:
		return "str: unknown error"
	}
}

// ================================================================== //
//  Str — 可变 UTF-8 字节缓冲区                                         //
// ================================================================== //

// Str 是 Go string 的可变版本，以 []byte 存储 UTF-8 字节序列。
// 所有修改接口在系统边界处验证输入的 UTF-8 合法性；内部存储始终假定为合法 UTF-8。
// Go 版本不实现 SSO（GC + 逃逸分析已处理短字符串的分配代价）。
type Str struct {
	data []byte
}

// ------------------------------------------------------------------ //
//  构造函数                                                            //
// ------------------------------------------------------------------ //

// NewStr 创建一个空字符串。
func NewStr() *Str {
	return &Str{}
}

// StrFromString 从 Go string 创建 Str（复制，验证 UTF-8）。
// Go string 保证是合法 UTF-8，此处验证为对称接口。
func StrFromString(s string) (*Str, error) {
	if !utf8.ValidString(s) {
		return nil, StrErrInvalidUTF8
	}
	b := make([]byte, len(s))
	copy(b, s)
	return &Str{data: b}, nil
}

// StrFromBytes 从字节切片创建 Str（复制，验证 UTF-8）。
func StrFromBytes(b []byte) (*Str, error) {
	if !utf8.Valid(b) {
		return nil, StrErrInvalidUTF8
	}
	cp := make([]byte, len(b))
	copy(cp, b)
	return &Str{data: cp}, nil
}

// ------------------------------------------------------------------ //
//  查询                                                                //
// ------------------------------------------------------------------ //

// LenBytes 返回字节长度。O(1)。
func (s *Str) LenBytes() int { return len(s.data) }

// LenChars 返回 Unicode 码点数量。O(n)。
func (s *Str) LenChars() int { return utf8.RuneCount(s.data) }

// Cap 返回底层缓冲区的字节容量。O(1)。
func (s *Str) Cap() int { return cap(s.data) }

// IsEmpty 报告字符串是否为空。
func (s *Str) IsEmpty() bool { return len(s.data) == 0 }

// AsString 零拷贝返回 string（共享底层 []byte）。
// 持有返回值期间，不应对 Str 执行任何可能触发重新分配的操作。
func (s *Str) AsString() string {
	if len(s.data) == 0 {
		return ""
	}
	return unsafe.String(&s.data[0], len(s.data))
}

// AsBytes 返回底层字节切片（借用视图）。
func (s *Str) AsBytes() []byte { return s.data }

// GetChar 返回第 charI 个 Unicode 码点（0 索引）。O(n)。
func (s *Str) GetChar(charI int) (rune, error) {
	if charI < 0 {
		return 0, StrErrOutOfBounds
	}
	i := 0
	for _, r := range s.AsString() {
		if i == charI {
			return r, nil
		}
		i++
	}
	return 0, StrErrOutOfBounds
}

// ByteOffset 返回第 charI 个码点的字节偏移（0 索引）。O(n)。
// charI 等于码点总数时，返回 LenBytes()（指向字符串末尾）。
func (s *Str) ByteOffset(charI int) (int, error) {
	if charI < 0 {
		return 0, StrErrOutOfBounds
	}
	offset := 0
	i := 0
	b := s.data
	for len(b) > 0 {
		if i == charI {
			return offset, nil
		}
		_, size := utf8.DecodeRune(b)
		offset += size
		b = b[size:]
		i++
	}
	if i == charI {
		return offset, nil // 指向末尾的合法偏移
	}
	return 0, StrErrOutOfBounds
}

// Slice 返回字节范围 [byteStart, byteEnd) 对应的子串。O(1)。
// 若边界不在码点边界上，返回 StrErrBadBoundary；超出范围返回 StrErrOutOfBounds。
func (s *Str) Slice(byteStart, byteEnd int) (string, error) {
	n := len(s.data)
	if byteStart < 0 || byteEnd > n || byteStart > byteEnd {
		return "", StrErrOutOfBounds
	}
	if byteStart > 0 && !utf8.RuneStart(s.data[byteStart]) {
		return "", StrErrBadBoundary
	}
	if byteEnd < n && !utf8.RuneStart(s.data[byteEnd]) {
		return "", StrErrBadBoundary
	}
	return string(s.data[byteStart:byteEnd]), nil
}

// ------------------------------------------------------------------ //
//  修改                                                                //
// ------------------------------------------------------------------ //

// Append 追加 UTF-8 字符串 other。验证 UTF-8 合法性。
func (s *Str) Append(other string) error {
	if !utf8.ValidString(other) {
		return StrErrInvalidUTF8
	}
	s.data = append(s.data, other...)
	return nil
}

// AppendChar 将 Unicode 码点 r 编码为 UTF-8 后追加。
func (s *Str) AppendChar(r rune) {
	s.data = utf8.AppendRune(s.data, r)
}

// Insert 在字节偏移 bytePos 处插入字符串 other。
// bytePos 必须落在码点边界上（0 和 LenBytes() 始终合法）。
func (s *Str) Insert(bytePos int, other string) error {
	n := len(s.data)
	if bytePos < 0 || bytePos > n {
		return StrErrOutOfBounds
	}
	if bytePos > 0 && bytePos < n && !utf8.RuneStart(s.data[bytePos]) {
		return StrErrBadBoundary
	}
	if !utf8.ValidString(other) {
		return StrErrInvalidUTF8
	}
	ins := []byte(other)
	result := make([]byte, 0, n+len(ins))
	result = append(result, s.data[:bytePos]...)
	result = append(result, ins...)
	result = append(result, s.data[bytePos:]...)
	s.data = result
	return nil
}

// RemoveRange 移除字节范围 [byteStart, byteEnd)。
// 边界必须落在码点边界上。
func (s *Str) RemoveRange(byteStart, byteEnd int) error {
	n := len(s.data)
	if byteStart < 0 || byteEnd > n || byteStart > byteEnd {
		return StrErrOutOfBounds
	}
	if byteStart > 0 && !utf8.RuneStart(s.data[byteStart]) {
		return StrErrBadBoundary
	}
	if byteEnd < n && !utf8.RuneStart(s.data[byteEnd]) {
		return StrErrBadBoundary
	}
	s.data = append(s.data[:byteStart], s.data[byteEnd:]...)
	return nil
}

// Clear 清空字符串（len 置 0，容量不变）。O(1)。
func (s *Str) Clear() { s.data = s.data[:0] }

// Reserve 确保至少有 additional 字节的额外容量。
func (s *Str) Reserve(additional int) {
	needed := len(s.data) + additional
	if cap(s.data) < needed {
		newData := make([]byte, len(s.data), needed)
		copy(newData, s.data)
		s.data = newData
	}
}

// ShrinkToFit 将容量收缩至当前长度。
func (s *Str) ShrinkToFit() {
	if cap(s.data) > len(s.data) {
		newData := make([]byte, len(s.data))
		copy(newData, s.data)
		s.data = newData
	}
}

// ToUpper 将 ASCII 字母原地转换为大写。O(n)。
func (s *Str) ToUpper() { s.data = bytes.ToUpper(s.data) }

// ToLower 将 ASCII 字母原地转换为小写。O(n)。
func (s *Str) ToLower() { s.data = bytes.ToLower(s.data) }

// ------------------------------------------------------------------ //
//  搜索与分割                                                          //
// ------------------------------------------------------------------ //

// Find 返回 pattern 第一次出现的字节偏移；未找到时返回 (0, false)。
func (s *Str) Find(pattern string) (int, bool) {
	i := strings.Index(s.AsString(), pattern)
	if i < 0 {
		return 0, false
	}
	return i, true
}

// Contains 报告 s 是否包含 pattern。
func (s *Str) Contains(pattern string) bool {
	return strings.Contains(s.AsString(), pattern)
}

// StartsWith 报告 s 是否以 prefix 开头。
func (s *Str) StartsWith(prefix string) bool {
	return strings.HasPrefix(s.AsString(), prefix)
}

// EndsWith 报告 s 是否以 suffix 结尾。
func (s *Str) EndsWith(suffix string) bool {
	return strings.HasSuffix(s.AsString(), suffix)
}

// Trim 返回移除首尾 ASCII 空白后的新 Str。
func (s *Str) Trim() *Str {
	trimmed := strings.TrimSpace(s.AsString())
	b := make([]byte, len(trimmed))
	copy(b, trimmed)
	return &Str{data: b}
}

// Split 按 delim 分割字符串，返回各部分的字符串切片。
func (s *Str) Split(delim string) []string {
	return strings.Split(s.AsString(), delim)
}

// ------------------------------------------------------------------ //
//  比较与哈希                                                          //
// ------------------------------------------------------------------ //

// Equal 报告 s 与 other 内容是否相同。
func (s *Str) Equal(other *Str) bool {
	return bytes.Equal(s.data, other.data)
}

// Compare 按字节序比较，返回 -1、0 或 1。
func (s *Str) Compare(other *Str) int {
	c := bytes.Compare(s.data, other.data)
	if c < 0 {
		return -1
	} else if c > 0 {
		return 1
	}
	return 0
}

// Hash 返回字符串内容的 FNV-1a 哈希值。
func (s *Str) Hash() uint64 {
	h := fnv.New64a()
	h.Write(s.data)
	return h.Sum64()
}

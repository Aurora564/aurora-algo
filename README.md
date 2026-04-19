# aurora-algo

> 用 C / C++ / Go 三语言同步实现的基础算法库，目标是真正理解数据结构的工程实现

---

## 📌 项目理念

刷题容易忘，这个项目的思路是：**从零手写每一个数据结构**，用三种语言分别实现，通过对比理解内存模型、工程抽象和并发思维的本质差异。

| 语言 | 核心训练目标 | 代表技术 |
|------|------------|---------|
| **C** | 内存模型、ABI、裸指针 | `malloc/realloc`、`void*`、宏泛型、函数指针 |
| **C++** | 工程抽象、零开销原则 | 模板、RAII、move 语义、concepts (C++20) |
| **Go** | 并发思维、接口设计 | goroutine、channel、泛型 (1.18+)、sync 包 |

---

## 🗂️ 目录结构

```
aurora-algo/
├── c/
│   ├── include/          # 头文件 (.h)
│   ├── src/              # 实现 (.c)
│   └── tests/
├── cpp/
│   ├── include/          # 模板头文件 (.hpp)
│   └── tests/
├── go/
│   └── algo/
│       ├── linear/
│       ├── tree/
│       ├── graph/
│       └── advanced/
├── benchmark/
├── docs/                 # 每周技术笔记
└── README.md
```

---

## 📦 实现进度

> ✅ 已完成 · 🚧 进行中 · ⬜ 待实现

### 阶段一：线性结构

| 结构 | C | C++ | Go | 备注 |
|------|:-:|:---:|:--:|------|
| 动态数组 Vector | ✅ | ✅ | ✅ | 含扩容 / shrink 策略 |
| 单链表 | ✅ | ✅ | ✅ | |
| 双链表 | ✅ | ✅ | ✅ | 含哨兵节点 |
| 栈（数组版） | ✅ | ✅ | ✅ | 含 Reserve / ShrinkToFit |
| 栈（链表版） | ✅ | ✅ | ✅ | |
| 队列 | ✅ | ✅ | ✅ | 链表版，复用 DList |
| 循环队列 | ✅ | ✅ | ✅ | Ring Buffer，有界 + 可扩容双模式 |
| 动态字符串 | ✅ | ✅ | ✅ | 含 SSO 分析 |

### 阶段二：树结构

| 结构 | C | C++ | Go | 备注 |
|------|:-:|:---:|:--:|------|
| 二叉树 | ⬜ | ⬜ | ⬜ | 递归 + 迭代遍历 |
| BST | ⬜ | ⬜ | ⬜ | 含 delete 三种 case |
| AVL 树 | ⬜ | ⬜ | ⬜ | 四种旋转 |
| 红黑树 | ⬜ | ⬜ | ⬜ | 五条性质 |
| 堆 | ⬜ | ⬜ | ⬜ | 最大堆 + heapsort |
| Trie | ⬜ | ⬜ | ⬜ | 含并发安全版（Go） |

### 阶段三：图结构

| 结构 / 算法 | C | C++ | Go | 备注 |
|------------|:-:|:---:|:--:|------|
| 邻接表 | ⬜ | ⬜ | ⬜ | |
| 邻接矩阵 | ⬜ | ⬜ | ⬜ | |
| BFS | ⬜ | ⬜ | ⬜ | |
| DFS | ⬜ | ⬜ | ⬜ | 递归 + 迭代 |
| Dijkstra | ⬜ | ⬜ | ⬜ | 最小堆优化 |
| 拓扑排序 | ⬜ | ⬜ | ⬜ | Kahn + DFS 两种 |
| 并查集 | ⬜ | ⬜ | ⬜ | 路径压缩 + 按秩合并 |

### 阶段四：高级结构

| 结构 / 算法 | C | C++ | Go | 备注 |
|------------|:-:|:---:|:--:|------|
| 线段树 | ⬜ | ⬜ | ⬜ | 含 lazy tag |
| 树状数组 | ⬜ | ⬜ | ⬜ | Fenwick Tree |
| KMP | ⬜ | ⬜ | ⬜ | |
| Aho-Corasick | ⬜ | ⬜ | ⬜ | 多模式匹配 |
| LRU Cache | ⬜ | ⬜ | ⬜ | 双向链表 + hash map |
| SkipList | ⬜ | ⬜ | ⬜ | |

---

## ⚡ 快速开始

### C — 编译并运行测试

```bash
cd c
gcc -O2 -Wall -o tests/test_vector tests/test_vector.c src/vector.c
./tests/test_vector
```

### C++ — 编译并运行测试

```bash
cd cpp
g++ -std=c++20 -O2 -Wall -o tests/test_vector tests/test_vector.cpp
./tests/test_vector
```

### Go — 单测 & Benchmark

```bash
cd go
go test ./algo/linear/...
go test -bench=. ./algo/linear/...
```

---

## 📖 学习笔记

每周一篇技术总结，见 [`docs/`](./docs/)

---

## 🗺️ 路线图

- **2026 Q1**：线性结构 + 树结构基础
- **2026 Q2**：树结构进阶 + 图结构 + 高级结构 → v1.0.0

---

## 🛠️ 开发规范

**提交前检查：**
- C/C++：`valgrind` / `AddressSanitizer` 无内存泄漏
- 所有语言：单测通过
- benchmark 数据变更需记录在 `benchmark/` 目录

**分支命名：**`feat/c-vector` · `feat/cpp-avl` · `feat/go-skiplist`

**commit 格式：**
```
feat(c): implement vector with shrink strategy
test(cpp): add edge cases for BST delete
bench: add vector vs list comparison
```

---

## 📚 参考资料

- 《C 程序设计语言》K&R 第二版
- 《C++ Templates》第二版
- CLRS《算法导论》第 12~13 章
- Linux kernel `lib/rbtree.c`
- Go 官方源码 `src/runtime/`
- Brendan Gregg《Systems Performance》第二版

---

## License

MIT
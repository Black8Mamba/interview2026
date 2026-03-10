# Linux内存管理学习文档

> 全面深入的Linux内核内存管理学习资料，适合嵌入式/Linux开发工程师面试准备。

## 文档目录

| 章节 | 主题 | 主要内容 |
|------|------|----------|
| [Chapter 1](./chapter1-概述与基础概念.md) | 概述与基础概念 | 虚拟内存原理、物理内存管理、页表基础、MMU工作原理 |
| [Chapter 2](./chapter2-页面分配器-Buddy.md) | 页面分配器(Buddy) | Buddy算法原理、分配API、GFP标志、CMA |
| [Chapter 3](./chapter3-Slab分配器.md) | Slab分配器 | Slab/Slub/Slob、kmem_cache、kmalloc、着色机制 |
| [Chapter 4](./chapter4-vmalloc与内存映射.md) | vmalloc与内存映射 | vmalloc原理、mmap系统调用、brk |
| [Chapter 5](./chapter5-常用内存类型分配及用途.md) | 常用内存类型分配及用途 | 内核/用户态分配API对比、选择指南 |
| [Chapter 6](./chapter6-缺页异常处理.md) | 缺页异常处理 | page fault流程、COW写时复制、swap机制 |
| [Chapter 7](./chapter7-内存回收与交换.md) | 内存回收与交换 | LRU算法、kswapd、页面回收策略、swap |
| [Chapter 8](./chapter8-内存优化技术.md) | 内存优化技术 | HugeTLB、透明大页、内存压缩、memcg |
| [Chapter 9](./chapter9-内存调试与工具.md) | 内存调试与工具 | /proc、slabtop、vmstat、perf、页面缓存 |
| [Chapter 10](./chapter10-面试常考题及答案剖析.md) | 面试常考题及答案剖析 | 20道高频面试题、答案详解、延伸问题 |

## 学习路线

推荐按照章节顺序学习：

1. **基础阶段** (Chapter 1-3)
   - 先学习内存管理基础概念
   - 掌握Buddy和Slab分配器原理

2. **进阶阶段** (Chapter 4-7)
   - 理解内存分配机制
   - 掌握缺页异常和内存回收

3. **实战阶段** (Chapter 8-10)
   - 学习性能优化技术
   - 掌握调试工具
   - 复习面试常考题

## 核心概念一览

### 内存分配器层次

```
用户态
  ├── malloc / free (glibc)
  │     ├── brk() (小块)
  │     └── mmap() (大块)
  │
内核态
  ├── 页面分配器 (Buddy System)
  │     └── alloc_pages / __get_free_pages
  │
  ├── Slab分配器
  │     ├── kmem_cache (对象缓存)
  │     └── kmalloc (小块内存)
  │
  └── vmalloc (大块非连续内存)
```

### 地址空间布局 (64位)

```
+------------------+ 0xFFFFFFFFFFFFFFFF
|     内核空间     |
+------------------+ 0xFFFF800000000000
|    vmalloc区    |
+------------------+
|   I/O映射区      |
+------------------+
|   固定映射区     |
+------------------+
|    内核镜像     |
+------------------+ 0xFFFF800000000000
|    用户空间     |
+------------------+ 0x00007FFFFFFFFFFF
|     栈(向下)    |
+------------------+
|    内存映射区   |
+------------------+
|    堆(向上)     |
+------------------+
|    BSS / 数据   |
+------------------+
|     代码段      |
+------------------+ 0x0000000000400000
```

## 相关文档

- [interview_linux.md](../interview_linux.md) - Linux面试知识点汇总
- [interview_linux_qa.md](../interview_linux_qa.md) - Linux面试问答

## 参与贡献

欢迎提交Issue和Pull Request完善内容。

## 参考资料

- Linux内核源码 (mm/)
- 《深入理解Linux内核》
- 《Linux内核设计与实现》
- kernel.org文档

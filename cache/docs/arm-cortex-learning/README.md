# ARM Cortex处理器学习文档

> 一份涵盖Cache、流水线、内存模型、内存屏障的系统性学习指南

---

## 文档概述

本系列文档采用"三层式"结构设计，适合不同层次的读者：

- **📖 基础层**：概念引入，适合入门读者
- **⚙️ 原理层**：机制详解，适合进阶读者
- **🔧 实战层**：优化技巧与案例，适合高级读者

---

## 目录结构

| 章节 | 文件 | 主题 |
|------|------|------|
| 第1章 | [chapter1-overview.md](./chapter1-overview.md) | ARM Cortex处理器概述 |
| 第2章 | [chapter2-cache.md](./chapter2-cache.md) | Cache体系结构 |
| 第3章 | [chapter3-tlb-mmu.md](./chapter3-tlb-mmu.md) | TLB与内存管理 |
| 第4章 | [chapter4-pipeline.md](./chapter4-pipeline.md) | 流水线技术 |
| 第5章 | [chapter5-memory-model.md](./chapter5-memory-model.md) | 内存模型 |
| 第6章 | [chapter6-memory-barriers.md](./chapter6-memory-barriers.md) | 内存屏障 |
| 第7章 | [chapter7-practice.md](./chapter7-practice.md) | 综合实战 |

---

## 推荐阅读顺序

### 入门读者（首次接触ARM架构）

```
第1章 → 第2章 → 第3章 → 基础层全部
```

### 进阶读者（需要深入理解）

```
第1章 → 第2章 → 第3章 → 第4章 → 第5章 → 原理层全部
```

### 高级读者（性能优化与实战）

```
全部按顺序阅读，重点关注实战层和第7章
```

---

## 术语表

| 英文 | 中文 | 首次出现章节 |
|------|------|-------------|
| ARM | Advanced RISC Machines | 第1章 |
| Cortex-A/R/M | ARM处理器系列 | 第1章 |
| DynamIQ | Arm多核技术 | 第1章 |
| Cache | 缓存 | 第2章 |
| MESI | 缓存一致性协议 | 第2章 |
| TLB | 转换后备缓冲 | 第3章 |
| MMU | 内存管理单元 | 第3章 |
| Pipeline | 流水线 | 第4章 |
| Superscalar | 超标量 | 第4章 |
| Out-of-Order | 乱序执行 | 第4章 |
| Memory Model | 内存模型 | 第5章 |
| Acquire/Release | 获取/释放语义 | 第5章 |
| LDREX/STREX | 独占加载/条件存储 | 第5章 |
| Memory Barrier | 内存屏障 | 第6章 |
| DMB/DSB/ISB | 数据屏障/同步屏障/指令屏障 | 第6章 |

---

## 配套资源

### 官方文档

- [Arm Architecture Reference Manual (ARM ARM)](https://developer.arm.com/documentation/ddi0487/latest)
- [Arm Cortex-A Series Programmer's Guide](https://developer.arm.com/documentation/den0013/latest)
- [Arm Cortex-A55 Software Optimization Guide](https://developer.arm.com/documentation/uan0015/latest)

### 性能分析工具

- Linux `perf`
- Arm Streamline
- Arm DS-5

### 社区资源

- [Arm Developer Forums](https://community.arm.com/)
- [Linux Kernel Documentation - Memory Barriers](https://www.kernel.org/doc/Documentation/memory-barriers.txt)

---

## 贡献指南

欢迎提交Issue或Pull Request来改进本文档。

---

## 许可证

本学习文档仅供学习交流使用。

# Linux 内核同步机制学习资料 - 设计方案

## 整体目标

- **目标读者**：兼顾面试求职 + 内核开发深入学习
- **内容深度**：详细 (~5万字)，包含概念 + API + 底层实现 + 面试题
- **输出格式**：Markdown 文件

---

## 目录结构

```
docs/
├── SUMMARY.md                    # 总目录索引
├── 01-atomic/
│   ├── README.md                 # 原子操作概述
│   ├── atomic_t.md               # 原子变量
│   ├── atomic-bitops.md          # 位操作
│   └── interview.md              # 面试题
├── 02-barrier/
│   ├── README.md                 # 内存屏障概述
│   ├── barrier-types.md          # 屏障类型
│   ├── compiler-barrier.md       # 编译乱序（新增）
│   └── interview.md              # 面试题
├── 03-lock/
│   ├── README.md                 # 锁机制概述
│   ├── spinlock.md               # 自旋锁
│   ├── mutex.md                  # 互斥锁
│   ├── semaphore.md              # 信号量
│   ├── rwlock.md                 # 读写锁
│   ├── seqlock.md                # 顺序锁
│   ├── deadlock-debug.md         # 死锁调试（新增）
│   └── interview.md              # 面试题（含死锁）
├── 04-rcu/
│   ├── README.md                 # RCU 概述
│   ├── rcu-basic.md              # 基础 RCU
│   ├── rcu-srcu.md               # SRCU
│   └── interview.md              # 面试题
├── 05-percpu/                     # 新增：Per-CPU 变量
│   ├── README.md                 # Per-CPU 概述
│   ├── percpu-basic.md           # 基本用法
│   └── interview.md              # 面试题
├── 06-futex/                      # 新增：Futex
│   ├── README.md                 # Futex 概述
│   ├── futex-principle.md        # 原理分析
│   └── interview.md              # 面试题
└── 07-advanced/
    ├── README.md                 # 高级主题概述
    ├── lockdep.md                # lockdep 锁依赖检测
    └── interview.md              # 面试题
```

---

## 同步机制覆盖范围

| 类别 | 内容 | 说明 |
|------|------|------|
| 原子操作 | atomic_t, atomic64_t, 位操作 | 基础同步原语 |
| 内存屏障 | 硬件内存屏障、编译乱序屏障 | 新增编译乱序 |
| 锁机制 | spinlock, mutex, semaphore, rwlock, seqlock | 核心锁类型 |
| **新增** | **死锁调试** | 调试方法与工具 |
| RCU | 经典 RCU, SRCU | 读写优化 |
| **新增** | **Per-CPU 变量** | CPU 本地数据 |
| **新增** | **Futex** | 用户态互斥锁 |
| 高级主题 | lockdep | 锁依赖检测 |

---

## 面试题覆盖检查表

| 章节 | 面试题文件 | 状态 |
|------|----------|------|
| 01-atomic | interview.md | 有 |
| 02-barrier | interview.md | 有 |
| 03-lock | interview.md | 有（含死锁调试） |
| 04-rcu | interview.md | 有 |
| 05-percpu | interview.md | 有 |
| 06-futex | interview.md | 有 |
| 07-advanced | interview.md | 有 |

---

## 死锁调试内容要点

- 死锁的必要条件（互斥、占有并等待、不可抢占、循环等待）
- 死锁的预防与避免策略
- 内核调试工具：
  - `lockdep`：锁依赖检测
  - `mutex-debug`：互斥锁调试
  - `spinlock-debug`：自旋锁调试
  - `/proc/locks`：查看当前锁状态
  - `/proc/lock_stat`：锁统计信息
- 死锁检测方法：
  - `crash` 工具分析内核转储
  - `sysrq-t`：打印当前锁信息
  - `perf lock`：锁性能分析
- 实战案例分析

---

## 新增内容要点

### 1. 编译乱序（Compiler Barrier）

- 编译器优化导致的乱序
- `barrier()` 和 `READ_ONCE()`/`WRITE_ONCE()`
- 与硬件内存屏障的区别

### 2. Per-CPU 变量

- `get_cpu_var()` / `put_cpu_var()`
- `this_cpu_read()` / `this_cpu_write()`
- 使用场景与优势

### 3. Futex（用户态快速互斥锁）

- 用户态快速互斥锁
- 内核态 vs 用户态
- `futex()` 系统调用原理
- 典型应用（pthread_mutex）

---

## 文件统计

| 章节 | 文件数 |
|------|--------|
| 原子操作 | 4 |
| 内存屏障 | 4 |
| 锁机制 | 8（含 deadlock-debug.md） |
| RCU | 4 |
| Per-CPU | 3 |
| Futex | 3 |
| 高级主题 | 3 |
| 总索引 | 1 |
| **总计** | **30** |

---

*设计方案版本：v1.1*
*更新日期：2026-03-10*

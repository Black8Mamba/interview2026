# Linux 内核同步机制学习资料 - 实施计划

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一套完整的 Linux 内核同步机制学习资料，包含原子操作、内存屏障、锁机制、RCU、Per-CPU、Futex 等，兼顾面试求职和内核开发深入学习

**Architecture:** 按机制类型分章节，每章包含概念介绍、API 接口、底层实现分析、常见面试题

**Tech Stack:** Markdown 文档

---

## 文件结构规划

```
docs/
├── SUMMARY.md                           # 总目录索引
├── 01-atomic/
│   ├── README.md                       # 原子操作概述
│   ├── atomic_t.md                     # 原子变量
│   ├── atomic-bitops.md                # 位操作
│   └── interview.md                    # 面试题
├── 02-barrier/
│   ├── README.md                       # 内存屏障概述
│   ├── barrier-types.md                # 屏障类型
│   ├── compiler-barrier.md              # 编译乱序
│   └── interview.md                     # 面试题
├── 03-lock/
│   ├── README.md                       # 锁机制概述
│   ├── spinlock.md                     # 自旋锁
│   ├── mutex.md                        # 互斥锁
│   ├── semaphore.md                    # 信号量
│   ├── rwlock.md                       # 读写锁
│   ├── seqlock.md                      # 顺序锁
│   ├── deadlock-debug.md                # 死锁调试（新增）
│   └── interview.md                    # 面试题（含死锁）
├── 04-rcu/
│   ├── README.md                       # RCU 概述
│   ├── rcu-basic.md                    # 基础 RCU
│   ├── rcu-srcu.md                     # SRCU
│   └── interview.md                    # 面试题
├── 05-percpu/
│   ├── README.md                       # Per-CPU 概述
│   ├── percpu-basic.md                 # 基本用法
│   └── interview.md                    # 面试题
├── 06-futex/
│   ├── README.md                       # Futex 概述
│   ├── futex-principle.md              # 原理分析
│   └── interview.md                    # 面试题
└── 07-advanced/
    ├── README.md                       # 高级主题概述
    ├── lockdep.md                      # lockdep 锁依赖检测
    └── interview.md                     # 面试题
```

---

## Chunk 1: 项目初始化与原子操作章节

### Task 1: 创建目录结构

**Files:**
- Create: `docs/01-atomic/README.md`
- Create: `docs/01-atomic/atomic_t.md`
- Create: `docs/01-atomic/atomic-bitops.md`
- Create: `docs/01-atomic/interview.md`

- [ ] **Step 1: 创建 01-atomic 目录和 README.md**

创建 `docs/01-atomic/README.md`，内容包含原子操作章节的概述：
- 什么是原子操作
- 原子操作的重要性
- 本章涵盖内容

- [ ] **Step 2: 创建 atomic_t.md**

创建原子变量详解文档，包含：
- atomic_t 数据类型
- 常用 API：`atomic_set`, `atomic_read`, `atomic_add`, `atomic_sub`, `atomic_inc`, `atomic_dec`, `atomic_inc_return` 等
- 使用场景与示例代码
- x86 底层实现分析

- [ ] **Step 3: 创建 atomic-bitops.md**

创建位操作文档，包含：
- `test_bit`, `set_bit`, `clear_bit`, `change_bit`
- `test_and_set_bit`, `test_and_clear_bit` 等
- 原子位操作的应用场景
- 底层实现分析

- [ ] **Step 4: 创建 interview.md**

创建原子操作面试题，包含：
- 什么是原子操作？为什么需要原子操作？
- atomic_t 和 atomic64_t 的区别
- 原子操作能够完全替代锁吗？
- 常见面试题及答案

- [ ] **Step 5: 提交 Chunk 1 第一部分**

```bash
git add docs/01-atomic/
git commit -m "docs: 添加原子操作章节 - 概述、atomic_t、位操作、面试题"
```

---

### Task 2: 创建内存屏障章节

**Files:**
- Create: `docs/02-barrier/README.md`
- Create: `docs/02-barrier/barrier-types.md`
- Create: `docs/02-barrier/compiler-barrier.md`
- Create: `docs/02-barrier/interview.md`

- [ ] **Step 1: 创建 02-barrier 目录和 README.md**

- [ ] **Step 2: 创建 barrier-types.md**

硬件内存屏障文档：
- 什么是内存屏障
- `mb()`, `rmb()`, `wmb()`, `smp_mb()`, `smp_rmb()`, `smp_wmb()`
- 各架构的实现差异
- 使用场景

- [ ] **Step 3: 创建 compiler-barrier.md**

编译乱序屏障：
- 编译器优化导致的乱序
- `barrier()` 宏
- `READ_ONCE()` / `WRITE_ONCE()`
- 与硬件内存屏障的区别

- [ ] **Step 4: 创建 interview.md**

内存屏障面试题：
- 什么是内存屏障？
- `smp_mb()` 和 `mb()` 的区别
- 编译乱序与硬件乱序的区别
- volatile 能替代内存屏障吗？

- [ ] **Step 5: 提交 Chunk 1 第二部分**

```bash
git add docs/02-barrier/
git commit -m "docs: 添加内存屏障章节 - 硬件屏障、编译乱序、面试题"
```

---

## Chunk 2: 锁机制章节

### Task 3: 创建锁机制概述与自旋锁

**Files:**
- Create: `docs/03-lock/README.md`
- Create: `docs/03-lock/spinlock.md`

- [ ] **Step 1: 创建 03-lock 目录和 README.md**

- [ ] **Step 2: 创建 spinlock.md**

自旋锁文档：
- 什么是自旋锁
- 适用场景（短临界区、不可睡眠）
- 常用 API：`spin_lock`, `spin_unlock`, `spin_lock_irq`, `spin_lock_irqsave` 等
- 代码示例
- x86 实现（TAS、CAS）
- 为什么自旋锁不能递归
- 自旋锁与中断上下文

- [ ] **Step 3: 提交**

```bash
git add docs/03-lock/
git commit -m "docs: 添加锁机制章节 - 概述与自旋锁"
```

---

### Task 4: 创建互斥锁与信号量

**Files:**
- Create: `docs/03-lock/mutex.md`
- Create: `docs/03-lock/semaphore.md`

- [ ] **Step 1: 创建 mutex.md**

互斥锁文档：
- 什么是 mutex
- 与自旋锁的区别
- 常用 API：`mutex_lock`, `mutex_unlock`, `mutex_trylock`, `mutex_lock_interruptible`
- 实现原理（睡锁）
- 使用场景

- [ ] **Step 2: 创建 semaphore.md**

信号量文档：
- 什么是信号量
- 计数信号量 vs 二值信号量
- 常用 API：`down`, `up`, `down_interruptible`, `down_trylock`
- 与 mutex 的区别

- [ ] **Step 3: 提交**

```bash
git add docs/03-lock/
git commit -m "docs: 添加互斥锁与信号量"
```

---

### Task 5: 创建读写锁、顺序锁与死锁调试

**Files:**
- Create: `docs/03-lock/rwlock.md`
- Create: `docs/03-lock/seqlock.md`
- Create: `docs/03-lock/deadlock-debug.md`

- [ ] **Step 1: 创建 rwlock.md**

读写锁文档：
- 读写锁的概念
- 读优先 vs 写优先
- 常用 API：`read_lock`, `read_unlock`, `write_lock`, `write_unlock`
- seqlock vs rwlock

- [ ] **Step 2: 创建 seqlock.md**

顺序锁文档：
- 什么是顺序锁
- `read_seqbegin`, `read_seqretry`
- 使用场景（读多写少）

- [ ] **Step 3: 创建 deadlock-debug.md**

死锁调试文档（新增）：
- 死锁的必要条件（互斥、占有并等待、不可抢占、循环等待）
- 死锁的预防与避免策略
- 内核调试工具：
  - `lockdep`：锁依赖检测，静态分析锁顺序
  - `mutex-debug`：互斥锁调试
  - `spinlock-debug`：自旋锁调试
  - `/proc/locks`：查看当前锁状态
  - `/proc/lock_stat`：锁统计信息
- 死锁检测方法：
  - 使用 `crash` 工具分析内核转储
  - `sysrq-t`：打印当前锁信息
  - `perf lock`：锁性能分析
- 实战案例分析

- [ ] **Step 4: 创建 lock 章节面试题**

**Files:**
- Create: `docs/03-lock/interview.md`

锁机制面试题：
- 自旋锁与互斥锁的区别
- 信号量与互斥锁的区别
- 读写锁的适用场景
- 自旋锁的实现原理
- 为什么自旋锁不能长时间持有
- 什么是死锁？死锁的必要条件是什么？
- 如何预防和避免死锁？
- 如何调试死锁问题？
- lockdep 的工作原理

- [ ] **Step 5: 提交**

```bash
git add docs/03-lock/
git commit -m "docs: 添加读写锁、顺序锁、死锁调试及面试题"
```

---

## Chunk 3: RCU 与 Per-CPU 章节

### Task 6: 创建 RCU 章节

**Files:**
- Create: `docs/04-rcu/README.md`
- Create: `docs/04-rcu/rcu-basic.md`
- Create: `docs/04-rcu/rcu-srcu.md`
- Create: `docs/04-rcu/interview.md`

- [ ] **Step 1: 创建 04-rcu 目录和 README.md**

- [ ] **Step 2: 创建 rcu-basic.md**

RCU 基础文档：
- 什么是 RCU（Read-Copy-Update）
- 核心思想（写时复制、宽限期）
- 常用 API：`rcu_read_lock`, `rcu_read_unlock`, `synchronize_rcu`, `call_rcu`
- 使用场景（读多写少）
- 宽限期（Grace Period）概念

- [ ] **Step 3: 创建 rcu-srcu.md**

SRCU 文档：
- 什么是 SRCU
- 与普通 RCU 的区别
- 使用场景

- [ ] **Step 4: 创建 interview.md**

RCU 面试题：
- RCU 的基本原理
- 什么是宽限期（Grace Period）？
- RCU 与读写锁的区别
- RCU 的优势与局限

- [ ] **Step 5: 提交**

```bash
git add docs/04-rcu/
git commit -m "docs: 添加 RCU 章节 - 基础、SRCU、面试题"
```

---

### Task 7: 创建 Per-CPU 章节

**Files:**
- Create: `docs/05-percpu/README.md`
- Create: `docs/05-percpu/percpu-basic.md`
- Create: `docs/05-percpu/interview.md`

- [ ] **Step 1: 创建 05-percpu 目录和 README.md**

- [ ] **Step 2: 创建 percpu-basic.md**

Per-CPU 变量文档：
- 什么是 Per-CPU 变量
- 优势（无锁、缓存友好）
- 常用 API：`get_cpu_var`, `put_cpu_var`, `this_cpu_read`, `this_cpu_write`
- 使用场景

- [ ] **Step 3: 创建 interview.md**

Per-CPU 面试题：
- Per-CPU 变量的优势
- 什么场景适合使用 Per-CPU 变量
- 使用 Per-CPU 变量需要注意什么

- [ ] **Step 4: 提交**

```bash
git add docs/05-percpu/
git commit -m "docs: 添加 Per-CPU 章节 - 基本用法、面试题"
```

---

## Chunk 4: Futex 与高级主题章节

### Task 8: 创建 Futex 章节

**Files:**
- Create: `docs/06-futex/README.md`
- Create: `docs/06-futex/futex-principle.md`
- Create: `docs/06-futex/interview.md`

- [ ] **Step 1: 创建 06-futex 目录和 README.md**

- [ ] **Step 2: 创建 futex-principle.md**

Futex 原理文档：
- 什么是 Futex（Fast Userspace Mutex）
- 用户态 vs 内核态
- `futex()` 系统调用
- 典型应用（pthread_mutex）
- 工作原理（用户态自旋 + 内核等待）

- [ ] **Step 3: 创建 interview.md**

Futex 面试题：
- Futex 的工作原理
- 为什么 Futex 比传统系统调用快
- Futex 与普通互斥锁的区别

- [ ] **Step 4: 提交**

```bash
git add docs/06-futex/
git commit -m "docs: 添加 Futex 章节 - 原理、面试题"
```

---

### Task 9: 创建高级主题章节

**Files:**
- Create: `docs/07-advanced/README.md`
- Create: `docs/07-advanced/lockdep.md`
- Create: `docs/07-advanced/interview.md`

- [ ] **Step 1: 创建 07-advanced 目录和 README.md**

- [ ] **Step 2: 创建 lockdep.md**

lockdep 文档：
- 什么是 lockdep
- 锁依赖检测
- 使用方法
- 典型用例

- [ ] **Step 3: 创建 interview.md**

高级主题面试题：
- lockdep 的作用
- 如何调试锁问题

- [ ] **Step 4: 提交**

```bash
git add docs/07-advanced/
git commit -m "docs: 添加高级主题章节 - lockdep、面试题"
```

---

## Chunk 5: 汇总与索引

### Task 10: 创建总目录索引

**Files:**
- Create: `docs/SUMMARY.md`

- [ ] **Step 1: 创建 SUMMARY.md**

总目录索引文档，链接到所有章节。

- [ ] **Step 2: 提交**

```bash
git add docs/SUMMARY.md
git commit -m "docs: 添加总目录索引 SUMMARY.md"
```

---

### Task 11: 最终检查与整理

- [ ] **Step 1: 检查所有文件是否完整**

检查每个章节是否包含：
- 概念介绍
- API 接口
- 底层实现分析
- 面试题

- [ ] **Step 2: 提交最终版本**

```bash
git add docs/
git commit -m "docs: 完成 Linux 内核同步机制学习资料 - 所有章节"
```

---

## 实施顺序

1. **Chunk 1**: 项目初始化 + 原子操作 + 内存屏障
2. **Chunk 2**: 锁机制（自旋锁、互斥锁、信号量、读写锁、顺序锁、死锁调试）
3. **Chunk 3**: RCU + Per-CPU
4. **Chunk 4**: Futex + 高级主题
5. **Chunk 5**: 总目录索引

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

## 新增内容：死锁调试

在 Task 5 中新增了 `deadlock-debug.md`，包含：
- 死锁的必要条件
- 预防与避免策略
- 内核调试工具（lockdep, mutex-debug, spinlock-debug, /proc/locks, /proc/lock_stat）
- 死锁检测方法（crash, sysrq-t, perf lock）
- 实战案例分析

---

## 预计文件数量

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

*实施计划版本：v1.1*
*创建日期：2026-03-10*

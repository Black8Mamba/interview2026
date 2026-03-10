# Linux 内核同步机制学习资料

## 目录

### 第一章：原子操作
- [概述](./01-atomic/README.md)
- [原子变量 atomic_t](./01-atomic/atomic_t.md)
- [位操作](./01-atomic/atomic-bitops.md)
- [面试题](./01-atomic/interview.md)

### 第二章：内存屏障
- [概述](./02-barrier/README.md)
- [硬件内存屏障](./02-barrier/barrier-types.md)
- [编译乱序屏障](./02-barrier/compiler-barrier.md)
- [面试题](./02-barrier/interview.md)

### 第三章：锁机制
- [概述](./03-lock/README.md)
- [自旋锁](./03-lock/spinlock.md)
- [互斥锁](./03-lock/mutex.md)
- [信号量](./03-lock/semaphore.md)
- [读写锁](./03-lock/rwlock.md)
- [顺序锁](./03-lock/seqlock.md)
- [死锁调试](./03-lock/deadlock-debug.md)
- [面试题](./03-lock/interview.md)

### 第四章：RCU
- [概述](./04-rcu/README.md)
- [RCU 基础](./04-rcu/rcu-basic.md)
- [SRCU](./04-rcu/rcu-srcu.md)
- [面试题](./04-rcu/interview.md)

### 第五章：Per-CPU
- [概述](./05-percpu/README.md)
- [Per-CPU 基础](./05-percpu/percpu-basic.md)
- [面试题](./05-percpu/interview.md)

### 第六章：Futex
- [概述](./06-futex/README.md)
- [Futex 原理](./06-futex/futex-principle.md)
- [面试题](./06-futex/interview.md)

### 第七章：高级主题
- [概述](./07-advanced/README.md)
- [lockdep](./07-advanced/lockdep.md)
- [面试题](./07-advanced/interview.md)

---

## 同步机制一览

| 机制 | 类型 | 适用场景 |
|------|------|----------|
| 原子操作 | 基础 | 计数器、标志位 |
| 内存屏障 | 基础 | 内存顺序控制 |
| 自旋锁 | 锁 | 短临界区、中断上下文 |
| 互斥锁 | 锁 | 长临界区 |
| 信号量 | 锁 | 资源池 |
| 读写锁 | 锁 | 读多写少 |
| 顺序锁 | 锁 | 读极多写极少 |
| RCU | 锁 | 读极多写极少 |
| Per-CPU | 无锁 | CPU 本地数据 |
| Futex | 用户态 | 高性能用户锁 |
| lockdep | 调试 | 死锁检测 |

---

*持续更新中...*

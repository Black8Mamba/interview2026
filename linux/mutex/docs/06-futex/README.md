# Futex (Fast Userspace Mutex)

## 概述

Futex（Fast Userspace Mutex）是 Linux 提供的用户态快速互斥机制。它是 pthread mutex 等用户态锁的底层实现，结合了用户态自旋和内核等待，在大多数情况下避免了系统调用开销。

## Futex 的核心思想

1. **用户态尝试**：先在用户态尝试获取锁（CAS）
2. **快速路径**：如果锁空闲，使用原子操作获取，无需进入内核
3. **慢速路径**：如果锁被占用，进入内核等待队列
4. **唤醒**：锁持有者释放时唤醒等待者

## 本章内容

- [Futex 原理](./futex-principle.md) - Futex 工作原理
- [面试题](./interview.md) - 常见面试题

---

*下一页：[Futex 原理](./futex-principle.md)*

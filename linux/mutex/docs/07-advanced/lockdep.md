# lockdep - 锁依赖检测

## 概述

lockdep 是 Linux 内核中的一个运行时锁正确性检查器，能够检测潜在的死锁问题。它通过分析锁获取的历史路径，构建锁依赖图，检测可能导致死锁的锁顺序。

## 主要功能

1. **锁顺序检测**：检测可能导致死锁的锁获取顺序
2. **递归锁检测**：检测同一锁的递归获取
3. **锁睡眠检测**：检测在持锁状态下睡眠
4. **锁释放错误检测**：检测未持有锁时的解锁操作

## 启用 lockdep

### 内核配置

```
Kernel hacking  --->
    [*] Lock Debugging (spinlocks, mutexes, etc.)
        [*] Runtime lock dependency checking
        [*] Locking strict self-testing
```

### 启动参数

```bash
# 启用 lockdep
bootargs: lockdep=1

# 启用锁自检
bootargs: lockdebug
```

## 使用方法

### 自动检测

lockdep 会自动检测以下问题：

```c
/* 1. 递归锁获取 - lockdep 会检测到 */
void recursive_func(void)
{
    spin_lock(&lock);
    recursive_func();  /* 死锁！*/
    spin_unlock(&lock);
}

/* 2. 锁顺序不一致 - lockdep 会检测到 */
void func_a(void)
{
    spin_lock(&lock_a);
    spin_lock(&lock_b);
    spin_unlock(&lock_b);
    spin_unlock(&lock_a);
}

void func_b(void)
{
    spin_lock(&lock_b);  /* 与 func_a 顺序相反 */
    spin_lock(&lock_a);
    spin_unlock(&lock_a);
    spin_unlock(&lock_b);
}

/* 3. 持锁睡眠 - lockdep 会检测到 */
void func(void)
{
    spin_lock(&lock);
    down(&semaphore);  /* 危险！在自旋锁内睡眠 */
    spin_unlock(&lock);
}
```

### 查看 lockdep 信息

```bash
# 查看锁依赖信息
cat /proc/lockdep

# 查看锁统计
cat /proc/lock_stat
```

## 原理

### 锁获取图

lockdep 维护一个锁获取的有向图：

```
节点：每把锁
边：锁获取的顺序

A -> B 表示：先获取 A，再获取 B

潜在死锁：A -> B -> C -> A (环路)
```

### 检测算法

1. **记录锁获取序列**：每次获取锁时，记录锁和调用点
2. **构建依赖图**：根据历史构建锁依赖图
3. **环路检测**：检测图中是否存在环路
4. **报告问题**：发现潜在死锁时输出警告

## 使用场景

1. **开发阶段**：在开发和测试阶段启用 lockdep
2. **调试**：发现死锁问题时启用
3. **代码审查**：通过 lockdep 输出验证代码正确性

## 注意事项

1. **性能开销**：lockdep 有显著的性能开销
2. **内存开销**：需要额外内存维护锁依赖图
3. **不覆盖所有场景**：只能检测已触发的锁顺序

---

*上一页：[高级主题概述](./README.md) | 下一页：[面试题](./interview.md)*

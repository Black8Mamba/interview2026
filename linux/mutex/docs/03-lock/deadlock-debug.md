# 死锁调试

## 概述

死锁是指两个或多个执行线程相互持有对方需要的资源，导致所有线程都无法继续执行的状态。在内核开发中，死锁是一个常见且棘手的问题。本节介绍死锁的必要条件、预防策略以及内核提供的调试工具。

## 死锁的必要条件

死锁的发生必须同时满足以下四个条件（也称为 Coffman 条件）：

### 1. 互斥条件

资源一次只能被一个线程持有。例如，一把锁同时只能被一个线程持有。

### 2. 占有并等待条件

线程在持有已有资源的同时，请求新的资源。例如：
```c
spin_lock(&lock_a);
/* 持有 lock_a，同时请求 lock_b */
spin_lock(&lock_b);
```

### 3. 不可抢占条件

资源不能被强制从持有线程中抢走，只能由线程主动释放。锁不能被抢占。

### 4. 循环等待条件

形成一个线程间的循环等待链：
- 线程 A 持有 lock_a，等待 lock_b
- 线程 B 持有 lock_b，等待 lock_a

## 死锁的预防与避免

### 1. 统一锁顺序

始终按固定顺序获取多把锁：

```c
/* 方法：始终先获取 lock_a，再获取 lock_b */
void func_a(void)
{
    spin_lock(&lock_a);
    spin_lock(&lock_b);
    /* 临界区 */
    spin_unlock(&lock_b);
    spin_unlock(&lock_a);
}

void func_b(void)
{
    /* 同样的顺序！避免循环等待 */
    spin_lock(&lock_a);
    spin_lock(&lock_b);
    /* 临界区 */
    spin_unlock(&lock_b);
    spin_unlock(&lock_a);
}
```

### 2. 使用 Trylock

使用 `trylock` 尝试获取锁，失败时释放已持有的锁：

```c
bool try_lock_both(spinlock_t *a, spinlock_t *b)
{
    spin_lock(a);
    if (spin_trylock(b)) {
        return true;
    }
    spin_unlock(a);
    return false;
}
```

### 3. 限制锁的持有时间

尽量减少锁持有时间，避免在持锁期间做耗时操作：

```c
/* 不好：持有锁期间做 I/O */
void process_bad(struct data *d)
{
    spin_lock(&lock);
    d->status = PROCESSING;
    write_to_disk(d->buffer);  /* I/O 操作，耗时 */
    spin_unlock(&lock);
}

/* 好：减少锁持有时间 */
void process_good(struct data *d)
{
    spin_lock(&lock);
    d->status = PROCESSING;
    copy_data(d->temp_buffer);  /* 只复制必要数据 */
    spin_unlock(&lock);

    write_to_disk(d->temp_buffer);  /* 锁外 I/O */
}
```

## 内核调试工具

### 1. Lockdep

`lockdep` 是内核中最强大的锁依赖检测工具，能够静态分析锁获取顺序，检测潜在的死锁。

#### 启用 lockdep

```bash
# 启动内核时添加参数
bootargs: nmi_watchdog=1 lockdep=1
```

或在内核配置中启用：
```
Kernel hacking  --->
    [*] Lock Debugging (spinlocks, mutexes, etc.)
        [*] Runtime lock dependency checking
```

#### 使用方法

```c
/* lockdep 会自动检测以下情况：
 * 1. 同一把锁的递归获取
 * 2. 不同的锁获取顺序导致的死锁
 * 3. 锁获取后睡眠（自旋锁）
 */

/* 示例：lockdep 会检测到以下死锁 */
void thread_1(void)
{
    spin_lock(&lock_a);
    spin_lock(&lock_b);  /* 可能检测到问题 */
    spin_unlock(&lock_b);
    spin_unlock(&lock_a);
}

void thread_2(void)
{
    spin_lock(&lock_b);
    spin_lock(&lock_a);  /* 与 thread_1 获取顺序相反！*/
    spin_unlock(&lock_a);
    spin_unlock(&lock_b);
}
```

#### 查看 lockdep 信息

```bash
# 查看锁依赖信息
cat /proc/lockdep

# 查看锁统计
cat /proc/lock_stat
```

### 2. Mutex Debug

内核提供了 mutex 调试模式，可以检测：

- 递归锁获取
- 未持有锁时解锁
- 重复解锁

#### 启用

```
Kernel hacking  --->
    [*] Debug Mutexes
```

### 3. Spinlock Debug

自旋锁调试可以检测：

- 递归获取自旋锁
- 持有自旋锁时睡眠
- 未正确初始化

#### 启用

```
Kernel hacking  --->
    [*] Debug Spinlocks
```

### 4. /proc/locks

查看当前系统中的锁状态：

```bash
cat /proc/locks
```

输出示例：
```
1: POSIX  ADVISORY  WRITE  1234 00:0c:23456 67890 0
2: FLOCK  ADVISORY  WRITE  1234 00:0c:23457 67891 0
```

### 5. /proc/lock_stat

查看锁的统计信息：

```bash
cat /proc/lock_stat
```

## 死锁检测方法

### 1. SysRq 键

使用 Magic SysRq 键打印锁信息：

```bash
# 在内核恐慌时打印锁信息
echo t > /proc/sysrq-trigger
# 或
Alt+SysRq+t

# 打印所有锁的信息
echo l > /proc/sysrq-trigger
```

### 2. perf lock

使用 perf 工具分析锁性能：

```bash
# 记录锁事件
perf lock record <command>

# 分析锁
perf lock report

# 示例
perf lock record ls
perf lock report
```

### 3. Crash 工具

分析内核转储文件：

```bash
crash> spinlocks
crash> mutexes
crash> bt  # 查看死锁线程的栈
```

### 4. ftrace

使用 ftrace 跟踪锁事件：

```bash
# 跟踪锁获取/释放
echo 'lock:*' > /sys/kernel/debug/tracing/set_event

# 跟踪自旋锁
echo 'spinlock:*' > /sys/kernel/debug/tracing/set_event
```

## 实战案例分析

### 案例 1：递归锁获取

**问题代码**：
```c
void recursive_function(struct node *n)
{
    spin_lock(&n->lock);
    if (n->child) {
        /* 递归调用又获取同一把锁 - 死锁！*/
        recursive_function(n->child);
    }
    spin_unlock(&n->lock);
}
```

**解决方法**：使用递归锁或重新设计数据结构

### 案例 2：锁顺序不一致

**问题代码**：
```c
/* 线程 A */
spin_lock(&lock_a);
process_A();
spin_lock(&lock_b);

/* 线程 B */
spin_lock(&lock_b);
process_B();
spin_lock(&lock_a);
```

**解决方法**：统一锁顺序

### 案例 3：中断上下文中持锁睡眠

**问题代码**：
```c
irqreturn_t handler(int irq, void *dev)
{
    spin_lock(&dev->lock);
    /* 在自旋锁保护的区域调用可能睡眠的函数 */
    down(&dev->semaphore);  /* 危险！自旋锁期间不能睡眠 */
    spin_unlock(&dev->lock);
}
```

**解决方法**：使用 `spin_lock_irqsave` 或重新设计同步方案

---

*上一页：[顺序锁](./seqlock.md) | 下一页：[面试题](./interview.md)*

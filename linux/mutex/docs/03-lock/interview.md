# 锁机制 - 面试题

## 自旋锁

### Q1: 自旋锁与互斥锁的区别是什么？

**答：**

| 特性 | 自旋锁 | 互斥锁 |
|------|--------|--------|
| 获取失败时 | 自旋等待（CPU 忙） | 睡眠等待（CPU 空闲） |
| 持有期间 | 不能睡眠 | 可以睡眠 |
| 使用上下文 | 中断/进程上下文 | 只能是进程上下文 |
| 性能开销 | 无上下文切换 | 有上下文切换 |
| 适用场景 | 短临界区 | 长临界区 |

---

### Q2: 自旋锁的实现原理是什么？

**答：**
在 x86 架构上，自旋锁使用 `xchg` 或 `cmpxchg` 指令实现：

```c
/* 简化的自旋锁实现 */
static inline void spin_lock(spinlock_t *lock)
{
    while (atomic_xchg(&lock->locked, 1) != 0) {
        while (atomic_read(&lock->locked))
            cpu_relax();  /* PAUSE 指令 */
    }
    smp_mb();
}
```

关键点：
1. `lock` 前缀确保原子性
2. `xchg` 指令自动带 lock 前缀
3. `pause` 指令减少自旋时的 CPU 功耗和缓存冲突

---

### Q3: 为什么自旋锁不能长时间持有？

**答：**
1. **CPU 浪费**：自旋期间 CPU 无法做其他工作
2. **其他线程饥饿**：等待锁的线程浪费 CPU
3. **中断处理延迟**：高优先级任务无法获得 CPU
4. **系统响应差**：持锁时间长会导致系统整体变慢

---

### Q4: 自旋锁为什么不能递归？

**答：**
递归获取自旋锁会导致死锁：

```c
/* 这会导致死锁！*/
void recursive(void)
{
    spin_lock(&lock);
    /* ... */
    recursive();  /* 再次尝试获取同一把锁 */
    spin_unlock(&lock);
}
```

原因：
- 第一次获取后，锁状态为 1
- 第二次获取时，`xchg` 返回 1，循环不退出
- 永远自旋，死锁！

---

### Q5: spin_lock_irqsave 和 spin_lock_irq 的区别？

**答：**

| 函数 | 中断状态 | 说明 |
|------|----------|------|
| `spin_lock_irq` | 禁止中断 | 保存当前中断状态后禁止 |
| `spin_lock_irqsave` | 保存并禁止 | 保存当前中断状态到 flags 后禁止 |

```c
/* spin_lock_irqsave - 推荐使用 */
unsigned long flags;
spin_lock_irqsave(&lock, flags);
/* 临界区 */
spin_unlock_irqrestore(&lock, flags);  /* 恢复原始中断状态 */

/* spin_lock_irq - 不保存状态 */
spin_lock_irq(&lock);
/* 临界区 */
spin_unlock_irq(&lock);  /* 直接恢复开状态，可能改变原始状态 */
```

---

## 互斥锁与信号量

### Q6: 信号量与互斥锁的区别？

**答：**

| 特性 | 信号量 | 互斥锁 |
|------|--------|--------|
| 计数 | 支持 > 1 | 只能是 1 |
| 持有者记录 | 不记录 | 记录 |
| 释放者 | 任意线程 | 必须是获取的线程 |
| 优先级继承 | 无 | 有（PI mutex） |
| 典型用途 | 资源池、生产者-消费者 | 互斥保护 |

---

### Q7: 互斥锁的实现原理？

**答：**
互斥锁是睡眠锁，主要实现流程：

1. **快速路径**：尝试原子将计数从 1 减为 0，成功则获取锁
2. **慢速路径**：计数为 0 时
   - 将当前任务加入等待队列
   - 设置状态为 `TASK_UNINTERRUPTIBLE`
   - 调用 `schedule()` 让出 CPU
   - 被唤醒后重新尝试获取

---

## 死锁

### Q8: 什么是死锁？死锁的必要条件是什么？

**答：**
死锁是指两个或多个执行线程相互持有对方需要的资源，导致所有线程都无法继续执行。

**四个必要条件**（Coffman 条件）：

1. **互斥条件**：资源一次只能被一个线程持有
2. **占有并等待**：线程持有已有资源的同时请求新资源
3. **不可抢占**：资源不能被强制抢走
4. **循环等待**：形成线程间的循环等待链

---

### Q9: 如何预防和避免死锁？

**答：**

**预防策略**（破坏死锁条件）：

1. **破坏互斥**：使用资源池化（不适用于锁）
2. **破坏占有并等待**：一次性获取所有需要的锁
3. **破坏不可抢占**：使用 trylock
4. **破坏循环等待**：统一锁获取顺序

**避免策略**：

1. **统一锁顺序**：
```c
/* 所有函数按相同顺序获取锁 */
void foo() { lock_a -> lock_b; }
void bar() { lock_a -> lock_b; }  /* 相同顺序 */
```

2. **使用 Trylock**：
```c
if (!spin_trylock(&a))
    return;
if (!spin_trylock(&b)) {
    spin_unlock(&a);
    return;
}
```

3. **减少锁持有时间**

---

### Q10: 如何调试死锁问题？

**答：**

1. **Lockdep**：
```bash
# 启用 lockdep
boot: nmi_watchdog=1 lockdep=1
# 查看信息
cat /proc/lockdep
cat /proc/lock_stat
```

2. **SysRq**：
```bash
echo t > /proc/sysrq-trigger  # 打印锁信息
```

3. **perf lock**：
```bash
perf lock record <command>
perf lock report
```

4. **ftrace**：
```bash
echo 'lock:*' > /sys/kernel/debug/tracing/set_event
```

---

## 读写锁与顺序锁

### Q11: 读写锁的适用场景？

**答：**
适用于"读多写少"的场景：

- 多个读者可以同时访问
- 写者需要独占访问
- 写操作不频繁

**示例**：
- 配置文件访问
- 系统状态查询
- 只读数据结构

---

### Q12: seqlock 与 rwlock 的区别？

**答：**

| 特性 | seqlock | rwlock |
|------|---------|--------|
| 读者阻塞写者 | 否 | 是（默认） |
| 写者阻塞读者 | 否 | 是 |
| 读者需要重试 | 是 | 否 |
| 写操作要求 | 必须非常快 | 无特殊要求 |
| 适用场景 | 读极多写极少，数据可容忍不一致 | 读多写少 |

---

## 进阶问题

### Q13: 什么情况下使用自旋锁而不是互斥锁？

**答：**

1. **中断处理函数**：中断上下文不能睡眠，只能用自旋锁
2. **临界区非常短**：几微秒以内，自旋开销小于睡眠开销
3. **持锁期间不能睡眠**：如在持有锁时调用 `down()`
4. **性能关键路径**：避免上下文切换开销

---

### Q14: 自旋锁如何与中断处理配合使用？

**答：**

在中断处理程序中需要配合使用：

```c
/* 正确用法 */
static spinlock_t dev_lock;

irqreturn_t handler(int irq, void *dev)
{
    unsigned long flags;
    spin_lock_irqsave(&dev_lock, flags);
    /* 安全访问共享数据 */
    spin_unlock_irqrestore(&dev_lock, flags);
    return IRQ_HANDLED;
}

void process_context(void)
{
    unsigned long flags;
    spin_lock_irqsave(&dev_lock, flags);
    /* 安全访问共享数据 */
    spin_unlock_irqrestore(&dev_lock, flags);
}
```

**原因**：
- `spin_lock_irqsave` 在获取锁前禁止中断
- 防止中断处理程序尝试获取同一把锁导致死锁

---

### Q15: lockdep 的工作原理？

**答：**
lockdep 是内核的静态锁依赖分析器：

1. **锁获取图**：维护所有锁的获取顺序图
2. **路径分析**：检测图中是否存在环路
3. **状态检测**：检测同一锁的递归获取

```c
/* lockdep 会检测以下死锁模式 */
void thread1() { lock(A); lock(B); ... }
void thread2() { lock(B); lock(A); ... }  /* 与 thread1 顺序相反 */
```

lockdep 会在内核启动时分析所有可能的锁获取路径，检测潜在死锁。

---

*上一页：[死锁调试](./deadlock-debug.md)*

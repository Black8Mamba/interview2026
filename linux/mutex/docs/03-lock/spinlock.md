# 自旋锁 (Spinlock)

## 概述

自旋锁是 Linux 内核中最基本的锁机制。其核心思想是：当锁不可用时，线程会持续循环检查锁的状态，直到获取到锁。由于自旋期间线程一直在 CPU 上运行（忙等待），因此自旋锁适用于短临界区，且持有锁期间不能睡眠的场景。

## 基本特性

1. **不可睡眠**：自旋锁持有期间不能调用可能导致睡眠的函数
2. **忙等待**：锁不可用时循环等待，不释放 CPU
3. **单递归**：同一线程不能递归获取自旋锁（会导致死锁）
4. **关闭抢占**：获取自旋锁时会关闭内核抢占

## 常用 API

### 基本操作

```c
/* 动态初始化 */
spinlock_t lock;
spin_lock_init(&lock);

/* 静态初始化 */
DEFINE_SPINLOCK(lock);
```

### 获取与释放

```c
/* 获取锁 - 如果不可用则自旋等待 */
spin_lock(&lock);
/* 临界区 */
spin_unlock(&lock);

/* 尝试获取锁 - 立即返回，不自旋 */
if (spin_trylock(&lock)) {
    /* 临界区 */
    spin_unlock(&lock);
} else {
    /* 获取失败，锁已被占用 */
}
```

### 带中断控制的锁

```c
/* 获取锁并禁止本地中断 */
spin_lock_irq(&lock);
/* 临界区 */
spin_unlock_irq(&lock);

/* 获取锁并保存中断状态后禁止中断 */
spin_lock_irqsave(&lock, flags);
/* 临界区 */
spin_unlock_irqrestore(&lock, flags);

/* 获取锁并禁止底部半部（软中断） */
spin_lock_bh(&lock);
/* 临界区 */
spin_unlock_bh(&lock);
```

### 完整的锁类型组合

| 函数 | 中断 | 底部半部 | 抢占 |
|------|------|----------|------|
| `spin_lock` | 不变 | 不变 | 禁止 |
| `spin_lock_irq` | 禁止 | 不变 | 禁止 |
| `spin_lock_irqsave` | 保存后禁止 | 不变 | 禁止 |
| `spin_lock_bh` | 不变 | 禁止 | 禁止 |
| `spin_lock_irqsave_bh` | 保存后禁止 | 禁止 | 禁止 |

## 使用示例

### 示例 1：基本使用

```c
static spinlock_t my_lock;
static int counter = 0;

static void increment(void)
{
    spin_lock(&my_lock);
    counter++;
    spin_unlock(&my_lock);
}

static int get_count(void)
{
    int ret;
    spin_lock(&my_lock);
    ret = counter;
    spin_unlock(&my_lock);
    return ret;
}
```

### 示例 2：中断上下文使用

```c
static spinlock_t dev_lock;
static unsigned long flags;

void interrupt_handler(int irq, void *dev_id)
{
    spin_lock_irqsave(&dev_lock, flags);
    /* 处理中断 - 安全访问共享数据 */
    spin_unlock_irqrestore(&dev_lock, flags);
}
```

### 示例 3：保护设备结构

```c
struct my_device {
    spinlock_t lock;
    struct list_head queue;
    bool running;
};

void start_device(struct my_device *dev)
{
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->running = true;
    spin_unlock_irqrestore(&dev->lock, flags);
}

void stop_device(struct my_device *dev)
{
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    dev->running = false;
    spin_unlock_irqrestore(&dev->lock, flags);
}

irqreturn_t device_interrupt(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    /* 处理中断 - 访问 dev->running 等 */
    spin_unlock_irqrestore(&dev->lock, flags);

    return IRQ_HANDLED;
}
```

## 底层实现 (x86)

### 基本原理

x86 上的自旋锁实现使用 `xchg` 或 `cmpxchg` 指令：

```c
/* 简化的自旋锁实现 */
static inline void spin_lock(spinlock_t *lock)
{
    while (atomic_xchg(&lock->locked, 1) != 0) {
        while (atomic_read(&lock->locked))
            cpu_relax();  /* PAUSE 指令 */
    }
    smp_mb();  /* 获取锁后确保内存顺序 */
}
```

### x86 实现细节

```asm
; spin_lock 的 x86 实现
1:  lock; xchg eax, [lock]
    test eax, eax
    jz  3f         ; 如果 eax=0（锁空闲），跳到 3
2:  pause         ; CPU 暂停，减少功耗和缓存冲突
    cmp byte [lock], 0
    jne 2b         ; 如果锁仍被占用，继续等待
    jmp 1b         ; 重新尝试获取
3:  ; 获取到锁
```

**关键点**：
1. `lock` 前缀确保原子性
2. `xchg` 指令自动带 lock 前缀
3. `pause` 指令是优化，减少自旋时的 CPU 功耗

### 禁止抢占

```c
/* spin_lock 会调用 preempt_disable() 禁止抢占 */
#define spin_lock(lock)           \
    do {                          \
        preempt_disable();        \
        _spin_lock(lock);         \
    } while (0)
```

## 为什么自旋锁不能递归

```c
/* 这会导致死锁！ */
void recursive_function(void)
{
    spin_lock(&lock);
    /* ... */
    if (some_condition) {
        recursive_function();  /* 尝试再次获取锁 - 死锁！ */
    }
    spin_unlock(&lock);
}
```

**原因**：
- 递归获取自旋锁会尝试将锁状态从"已占用"改为"已占用"
- 第一次获取成功后，锁状态为 1
- 第二次获取时，`xchg` 会返回 1，循环不会退出
- 死锁！

## 自旋锁与中断上下文

### 为什么需要关闭中断

```c
/* 不正确：中断处理程序可能死锁 */
void handler(void)
{
    spin_lock(&lock);  /* 可能永久自旋 */
}

void process(void)
{
    spin_lock_irqsave(&lock, flags);  /* 正确：禁止中断 */
}
```

**问题**：
1. 进程上下文获取锁
2. 中断发生，中断处理程序尝试获取同一把锁
3. 中断处理程序自旋等待
4. 进程无法运行来释放锁
5. **死锁**

**解决方案**：使用 `spin_lock_irqsave` 在获取锁前禁止中断

## 性能注意事项

1. **临界区要短**：自旋期间 CPU 无法做其他工作
2. **避免在锁内做耗时操作**：如文件系统 I/O
3. **减少锁竞争**：
   - 减小锁粒度
   - 使用 per-CPU 数据
4. **使用合适的自旋变体**：
   - `spin_trylock()` 适用于可以放弃的场景
   - 避免不必要的 `spin_lock_irqsave`

## 与其他锁的对比

| 特性 | 自旋锁 | 互斥锁 |
|------|--------|--------|
| 获取失败时 | 自旋等待 | 睡眠等待 |
| 持有期间 | 不能睡眠 | 可以睡眠 |
| 上下文 | 中断/进程 | 只能是进程 |
| 性能 | 低开销（适合短临界区） | 有上下文切换开销 |
| 递归 | 不可递归 | 不可递归（同一线程） |

---

*上一页：[锁机制概述](./README.md) | 下一页：[互斥锁](./mutex.md)*

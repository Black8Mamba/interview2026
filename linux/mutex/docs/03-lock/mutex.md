# 互斥锁 (Mutex)

## 概述

互斥锁（Mutex）是 Linux 内核中用于保护临界区的睡眠锁。与自旋锁不同，当互斥锁不可用时，获取锁的进程会进入睡眠状态，让出 CPU 供其他进程使用。这使得互斥锁适用于需要长时间持有锁或需要睡眠的场景。

## 基本特性

1. **睡眠等待**：锁不可用时，进程进入睡眠状态
2. **可睡眠**：持有互斥锁的进程可以睡眠
3. **递归限制**：同一线程不能递归获取同一互斥锁
4. **进程上下文**：只能在进程上下文中使用

## 常用 API

### 初始化

```c
/* 动态初始化 */
struct mutex lock;
mutex_init(&lock);

/* 静态初始化 */
DEFINE_MUTEX(lock);
```

### 获取与释放

```c
/* 获取锁 - 如果不可用则睡眠 */
mutex_lock(&lock);
/* 临界区 */
mutex_unlock(&lock);

/* 尝试获取锁 - 不会睡眠，立即返回 */
if (mutex_trylock(&lock)) {
    /* 获取成功 */
    mutex_unlock(&lock);
} else {
    /* 获取失败 */
}

/* 可中断获取 - 可以被信号唤醒 */
if (mutex_lock_interruptible(&lock)) {
    /* 被信号中断，返回 -EINTR */
}

/* 可超时获取 */
ret = mutex_lock_timeout(&lock, timeout);  /* timeout in jiffies */
```

### 状态查询

```c
/* 检查锁是否被锁定（不推荐，竞态条件） */
if (mutex_is_locked(&lock)) {
    /* 锁被占用 */
}
```

## 使用示例

### 示例 1：基本使用

```c
static DEFINE_MUTEX(device_mutex);
static struct device *current_device;

int open_device(struct device *dev)
{
    mutex_lock(&device_mutex);
    if (current_device) {
        mutex_unlock(&device_mutex);
        return -EBUSY;
    }
    current_device = dev;
    mutex_unlock(&device_mutex);
    return 0;
}

void close_device(void)
{
    mutex_lock(&device_mutex);
    current_device = NULL;
    mutex_unlock(&device_mutex);
}
```

### 示例 2：带引用计数的设备

```c
struct device {
    struct mutex lock;
    int refcount;
    void *private_data;
};

void get_device(struct device *dev)
{
    mutex_lock(&dev->lock);
    dev->refcount++;
    mutex_unlock(&dev->lock);
}

void put_device(struct device *dev)
{
    mutex_lock(&dev->lock);
    dev->refcount--;
    if (dev->refcount == 0) {
        /* 释放设备资源 */
        kfree(dev->private_data);
        kfree(dev);
        /* 注意：这里不能访问 dev 了 */
        return;
    }
    mutex_unlock(&dev->lock);
}
```

### 示例 3：可中断操作

```c
int do_something(struct resource *res)
{
    int ret;

    ret = mutex_lock_interruptible(&res->lock);
    if (ret)
        return -EINTR;  /* 被信号中断 */

    /* 临界区 */
    if (need_to_wait) {
        ret = wait_event_interruptible(res->waitq,
                                       condition);
        if (ret) {
            mutex_unlock(&res->lock);
            return -EINTR;
        }
    }

    /* 完成操作 */
    mutex_unlock(&res->lock);
    return 0;
}
```

## 底层实现

### 数据结构

```c
struct mutex {
    atomic_t                count;       /* 锁计数：1=可用，0=被占用，-1=被占用且有等待者 */
    spinlock_t              wait_lock;    /* 保护等待队列 */
    struct list_head       wait_list;    /* 等待队列 */
    struct task_struct     *owner;       /* 当前持有者 */
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
    void                    *saved_pc;   /* 调试用：持有者的 PC */
#endif
};
```

### 获取锁流程

```c
void __sched mutex_lock(struct mutex *lock)
{
    /* 快速路径：尝试原子获取 */
    if (atomic_dec_and_test(&lock->count)) {
        lock->owner = current;
        return;
    }

    /* 慢速路径：需要等待 */
    mutex_slowlock(lock);
}
```

### 睡眠与唤醒

1. 将当前任务加入等待队列
2. 设置任务状态为 `TASK_UNINTERRUPTIBLE` 或 `TASK_INTERRUPTIBLE`
3. 检查锁是否可用（避免伪唤醒）
4. 调用 `schedule()` 让出 CPU
5. 被唤醒后，重新尝试获取锁

## 死锁避免

### 1. 统一锁顺序

```c
/* 始终按固定顺序获取锁 */
void func_a(void)
{
    mutex_lock(&lock_a);
    mutex_lock(&lock_b);
    /* 操作 */
    mutex_unlock(&lock_b);
    mutex_unlock(&lock_a);
}

void func_b(void)
{
    /* 相同的顺序！避免死锁 */
    mutex_lock(&lock_a);
    mutex_lock(&lock_b);
    /* 操作 */
    mutex_unlock(&lock_b);
    mutex_unlock(&lock_a);
}
```

### 2. .trylock

```c
/* 尝试获取锁，避免死锁 */
bool try_lock_multiple(struct mutex *a, struct mutex *b)
{
    if (mutex_trylock(a)) {
        if (mutex_trylock(b)) {
            return true;
        }
        mutex_unlock(a);
    }
    return false;
}
```

## 与自旋锁对比

| 特性 | 自旋锁 | 互斥锁 |
|------|--------|--------|
| 获取失败时 | 自旋等待（CPU 忙） | 睡眠等待（CPU 释放） |
| 持有期间 | 不能睡眠 | 可以睡眠 |
| 使用上下文 | 中断/进程上下文 | 只能是进程上下文 |
| 性能 | 无上下文切换 | 有上下文切换开销 |
| 适用场景 | 短临界区 | 长临界区 |
| 递归获取 | 不可（会死锁） | 不可（会panic） |

## 使用场景选择

**使用自旋锁**：
- 临界区非常短（几微秒以内）
- 在中断上下文中使用
- 持有锁期间不能睡眠

**使用互斥锁**：
- 临界区较长
- 持有锁期间需要睡眠
- 频繁的上下文切换可接受

---

*上一页：[自旋锁](./spinlock.md) | 下一页：[信号量](./semaphore.md)*

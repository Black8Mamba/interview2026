# 信号量 (Semaphore)

## 概述

信号量是一种计数锁，由 Edsger Dijkstra 提出。信号量维护了一个计数器，表示可用资源的数量。当计数器大于 0 时，获取操作成功；当计数器为 0 时，获取操作会阻塞调用线程。

信号量与互斥锁的主要区别：
- 信号量可以持有计数 > 1（允许多个并发访问）
- 信号量不要求释放锁的线程必须是获取锁的线程
- 信号量是睡眠锁

## 常用 API

### 初始化

```c
/* 动态初始化 */
struct semaphore sem;
sema_init(&sem, count);  /* count: 初始计数 */

/* 二值信号量（互斥锁的替代） */
static DECLARE_SEMAPHORE_GENERIC(name, count);
#define DECLARE_MUTEX(name)  DECLARE_MUTEX_LOCKED(name)
#define init_MUTEX(sem)      sema_init(sem, 1)
#define init_MUTEX_LOCKED(sem) sema_init(sem, 0)
```

### 获取与释放

```c
/* 获取信号量 - 计数减 1，如果为 0 则睡眠 */
down(&sem);

/* 尝试获取 - 立即返回，不睡眠 */
if (down_trylock(&sem)) {
    /* 获取成功 */
} else {
    /* 获取失败 */
}

/* 可中断获取 - 可以被信号唤醒 */
if (down_interruptible(&sem)) {
    /* 被信号中断，返回 -EINTR */
}

/* 可中断且可kill的获取 */
down_killable(&sem);

/* 释放信号量 - 计数加 1，唤醒等待者 */
up(&sem);
```

### 超时获取

```c
/* 带超时的获取 */
ret = down_timeout(&sem, timeout);  /* timeout in jiffies */
/* 返回值：0 成功，-ETIME 超时 */
```

## 使用示例

### 示例 1：资源池

```c
/* 限制最多 N 个并发访问 */
#define MAX_CONCURRENT 10
static struct semaphore resource_sem;
static void *resource_pool[MAX_CONCURRENT];

static int init(void)
{
    sema_init(&resource_sem, MAX_CONCURRENT);
    /* 初始化资源池 */
}

static void *acquire_resource(void)
{
    down(&resource_sem);  /* 获取资源，计数减 1 */
    /* 分配具体资源 */
    return find_free_resource();
}

static void release_resource(void *resource)
{
    /* 释放资源 */
    free_resource(resource);
    up(&resource_sem);  /* 归还资源，计数加 1 */
}
```

### 示例 2：生产者-消费者

```c
#define BUFFER_SIZE 100
static struct semaphore produce_sem = __SEMAPHORE_INITIALIZER(produce_sem, BUFFER_SIZE);
static struct semaphore consume_sem = __SEMAPHORE_INITIALIZER(consume_sem, 0);
static spinlock_t buffer_lock;
static int buffer[BUFFER_SIZE];
static int write_pos, read_pos;

void produce(int data)
{
    down(&produce_sem);  /* 等待空位 */
    spin_lock(&buffer_lock);
    buffer[write_pos] = data;
    write_pos = (write_pos + 1) % BUFFER_SIZE;
    spin_unlock(&buffer_lock);
    up(&consume_sem);  /* 增加可用数据 */
}

int consume(void)
{
    down(&consume_sem);  /* 等待数据 */
    spin_lock(&buffer_lock);
    int data = buffer[read_pos];
    read_pos = (read_pos + 1) % BUFFER_SIZE;
    spin_unlock(&buffer_lock);
    up(&produce_sem);  /* 释放空位 */
    return data;
}
```

### 示例 3：简单的互斥

```c
/* 使用二值信号量作为互斥锁 */
static DECLARE_MUTEX(sem);

void critical_section(void)
{
    down(&sem);  /* 获取锁 */
    /* 临界区 */
    up(&sem);  /* 释放锁 */
}
```

## 底层实现

### 数据结构

```c
struct semaphore {
    raw_spinlock_t      lock;
    unsigned int        count;
    struct list_head   wait_list;
};
```

### 实现原理

```c
void down(struct semaphore *sem)
{
    unsigned long flags;

    raw_spin_lock_irqsave(&sem->lock, flags);
    if (sem->count > 0) {
        sem->count--;
        raw_spin_unlock_irqrestore(&sem->lock, flags);
    } else {
        /* 计数为 0，需要等待 */
        __down(sem);  /* 将任务加入等待队列并睡眠 */
    }
}

void up(struct semaphore *sem)
{
    unsigned long flags;
    struct task_struct *tsk;

    raw_spin_lock_irqsave(&sem->lock, flags);
    if (list_empty(&sem->wait_list)) {
        sem->count++;
    } else {
        /* 唤醒等待队列中的一个任务 */
        tsk = list_entry(sem->wait_list.next, struct task_struct, task_list);
        list_del_init(&tsk->task_list);
        wake_up_process(tsk);
        sem->count++;
    }
    raw_spin_unlock_irqrestore(&sem->lock, flags);
}
```

## 与互斥锁的区别

| 特性 | 信号量 | 互斥锁 |
|------|--------|--------|
| 计数 | 支持计数 > 1 | 只能是 1 |
| 持有者 | 不记录 | 记录当前持有者 |
| 释放者 | 任意线程 | 必须是获取的线程 |
| 递归 | 不支持 | 不支持 |
| 优先级继承 | 无 | 有（PI mutex） |
| 性能 | 稍慢 | 稍快 |

## 使用场景

### 信号量适合的场景

1. **资源池**：限制并发访问的资源数量
2. **生产者-消费者**：平衡生产者和消费者的速率
3. **简单的同步**：不需要互斥锁的严格性

### 互斥锁更适合的场景

1. **严格互斥**：只需要一个持有者
2. **需要优先级继承**：避免优先级反转
3. **调试方便**：互斥锁提供更多调试信息

## 注意事项

1. **不要在中断上下文使用**：信号量会睡眠
2. **平衡获取和释放**：确保 `up()` 总是被调用
3. **考虑使用 mutex**：如果是二值信号量，考虑使用 mutex
4. **避免死锁**：和多把信号量一起使用时注意顺序

---

*上一页：[互斥锁](./mutex.md) | 下一页：[读写锁](./rwlock.md)*

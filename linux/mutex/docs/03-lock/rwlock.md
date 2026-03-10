# 读写锁 (Rwlock)

## 概述

读写锁是一种改进的锁机制，允许多个读者同时访问共享资源，但写者是互斥的。这种锁适用于"读多写少"的场景，可以显著提高并发性能。

## 基本特性

1. **读并行**：多个读者可以同时持有读锁
2. **写互斥**：写者必须独占访问
3. **写优先**（默认）：写者可能饿死读者
4. **睡眠锁**：获取失败时会睡眠

## 常用 API

### 初始化

```c
/* 动态初始化 */
rwlock_t lock;
rwlock_init(&lock);

/* 静态初始化 */
DEFINE_RWLOCK(lock);
```

### 读锁

```c
/* 获取读锁 */
read_lock(&lock);
/* 临界区 - 只读 */
read_unlock(&lock);

/* 尝试获取读锁 */
if (read_trylock(&lock)) {
    read_unlock(&lock);
}

/* 带中断控制的读锁 */
read_lock_irq(&lock);
read_unlock_irq(&lock);

read_lock_irqsave(&lock, flags);
read_unlock_irqrestore(&lock, flags);
```

### 写锁

```c
/* 获取写锁 */
write_lock(&lock);
/* 临界区 - 读写 */
write_unlock(&lock);

/* 尝试获取写锁 */
if (write_trylock(&lock)) {
    write_unlock(&lock);
}

/* 带中断控制的写锁 */
write_lock_irq(&lock);
write_unlock_irq(&lock);

write_lock_irqsave(&lock, flags);
write_unlock_irqrestore(&lock, flags);
```

## 使用示例

### 示例 1：配置参数

```c
static DEFINE_RWLOCK(config_lock);
static struct config {
    int mode;
    int timeout;
    char name[32];
} current_config;

void update_config(struct config *new_config)
{
    write_lock(&config_lock);
    current_config.mode = new_config->mode;
    current_config.timeout = new_config->timeout;
    strcpy(current_config.name, new_config->name);
    write_unlock(&config_lock);
}

int read_config(struct config *out)
{
    int ret = -EFAULT;
    read_lock(&config_lock);
    out->mode = current_config.mode;
    out->timeout = current_config.timeout;
    strcpy(out->name, current_config.name);
    read_unlock(&config_lock);
    return 0;
}
```

### 示例 2：链表访问

```c
static DEFINE_RWLOCK(list_lock);
static struct list_head data_list = LIST_HEAD_INIT(data_list);

void add_item(struct item *new)
{
    write_lock(&list_lock);
    list_add_tail(&new->list, &data_list);
    write_unlock(&list_lock);
}

void remove_item(struct item *item)
{
    write_lock(&list_lock);
    list_del(&item->list);
    write_unlock(&list_lock);
}

void traverse_list(void (*func)(struct item *))
{
    struct item *item;

    read_lock(&list_lock);
    list_for_each_entry(item, &data_list, list) {
        func(item);  /* 只读操作 */
    }
    read_unlock(&list_lock);
}
```

## 底层实现

### 数据结构

```c
typedef struct {
    arch_rwlock_t raw_lock;
} rwlock_t;
```

### 实现原理

```c
/* 简化的读写锁实现 */
void read_lock(rwlock_t *lock)
{
    /* 尝试获取读锁 */
    while (atomic_read(&lock->count) < 0) {
        /* 有写者，等待 */
        cpu_relax();
    }
    atomic_dec(&lock->count);
}

void write_lock(rwlock_t *lock)
{
    /* 获取独占锁 */
    while (atomic_cmpxchg(&lock->count, 0, -1) != 0) {
        cpu_relax();
    }
}
```

### 写优先问题

默认的读写锁是写优先的：

```c
/* 写优先意味着：
 * 1. 写者等待时，新的读者无法获取锁
 * 2. 写者获取锁后，后续读者需要等待
 * 可能导致读者被写者饿死
 */
```

**解决**：
- 使用顺序锁（seqlock）
- 使用 RCU（Read-Copy-Update）

## 与普通锁的对比

| 特性 | 读写锁 | 互斥锁 |
|------|--------|--------|
| 并发读 | 支持 | 不支持 |
| 性能（读多） | 高 | 低 |
| 写性能 | 相同 | 相同 |
| 实现复杂度 | 较高 | 较低 |

## 注意事项

1. **读多写少才有效**：如果写频繁，读写锁优势不大
2. **避免长时间读锁**：持有读锁期间不要做耗时操作
3. **写锁粒度**：尽量减小写锁保护的区域
4. **写优先**：注意写者可能饿死读者

---

*上一页：[信号量](./semaphore.md) | 下一页：[顺序锁](./seqlock.md)*

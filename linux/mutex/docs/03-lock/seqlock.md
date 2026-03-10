# 顺序锁 (Seqlock)

## 概述

顺序锁（Seqlock）是一种特殊的锁机制，核心思想是通过"顺序计数器"来协调读者和写者。与读写锁不同，顺序锁的读者不会阻塞写者，写者也不会阻塞读者（除非发生冲突）。读者通过检查顺序号来检测是否与写者发生冲突。

顺序锁适用于以下场景：
- 读多写少
- 写操作非常快（通常只是简单赋值）
- 可以容忍偶尔读到不一致的数据

## 基本特性

1. **写者不阻塞读者**：读者可以随时获取锁（读取顺序号）
2. **读者不阻塞写者**：写者直接获取锁，无等待
3. **写者优先**：写者优先获取锁
4. **读者可能重试**：检测到冲突时需要重新读取

## 常用 API

### 初始化

```c
/* 静态初始化 */
seqlock_t lock;
seqlock_init(&lock);

/* 宏初始化 */
DEFINE_SEQLOCK(lock);
```

### 写锁

```c
/* 获取写锁 - 实际上是获取对顺序号的独占访问 */
write_seqlock(&lock);

/* 写临界区 */
write_sequnlock(&lock);

/* 尝试获取写锁 */
if (write_tryseqlock(&lock)) {
    write_sequnlock(&lock);
}
```

### 读锁

```c
/* 开始读取 - 返回当前的顺序号 */
unsigned int seq = read_seqbegin(&lock);

/* 读取共享数据 */

/* 检查是否与写者冲突 */
if (read_seqretry(&lock, seq)) {
    /* 发生冲突，需要重试 */
}
```

### 简化写法

```c
/* 自动处理重试的宏 */
do {
    seq = read_seqbegin(&lock);
    /* 读取共享数据 */
} while (read_seqretry(&lock, seq));
```

## 使用示例

### 示例 1：时间戳

```c
static seqlock_t jiffies_lock = DEFINE_SEQLOCK(jiffies_lock);
static u64 jiffies_64;

u64 get_jiffies_64(void)
{
    unsigned int seq;
    u64 ret;

    do {
        seq = read_seqbegin(&jiffies_lock);
        ret = jiffies_64;
    } while (read_seqretry(&jiffies_lock, seq));

    return ret;
}

void update_jiffies(void)
{
    write_seqlock(&jiffies_lock);
    jiffies_64++;
    write_sequnlock(&jiffies_lock);
}
```

### 示例 2：配置结构

```c
static seqlock_t config_lock = DEFINE_SEQLOCK(config_lock);
static struct config {
    int mode;
    int timeout;
} current_config = {0, 1000};

void update_config(int mode, int timeout)
{
    write_seqlock(&config_lock);
    current_config.mode = mode;
    current_config.timeout = timeout;
    write_sequnlock(&config_lock);
}

void read_config(int *mode, int *timeout)
{
    unsigned int seq;

    do {
        seq = read_seqbegin(&config_lock);
        *mode = current_config.mode;
        *timeout = current_config.timeout;
    } while (read_seqretry(&config_lock, seq));
}
```

### 示例 3：统计信息

```c
static seqlock_t stats_lock = DEFINE_SEQLOCK(stats_lock);
static struct {
    unsigned long rx_bytes;
    unsigned long tx_bytes;
    unsigned int rx_packets;
    unsigned int tx_packets;
} stats;

void update_stats(int direction, unsigned long bytes, unsigned int packets)
{
    write_seqlock(&stats_lock);
    if (direction) {
        stats.tx_bytes += bytes;
        stats.tx_packets += packets;
    } else {
        stats.rx_bytes += bytes;
        stats.rx_packets += packets;
    }
    write_sequnlock(&stats_lock);
}

void get_stats(struct stats *out)
{
    do {
        memcpy(out, &stats, sizeof(stats));
    } while (read_seqretry(&stats_lock, seq));
}
```

## 底层实现

### 数据结构

```c
typedef struct {
    unsigned sequence;
    spinlock_t lock;
} seqlock_t;
```

### 实现原理

```c
/* 写锁获取 */
void write_seqlock(seqlock_t *s)
{
    spin_lock(&s->lock);
    s->sequence++;  /* 顺序号加 1，表示写操作开始 */
}

/* 写锁释放 */
void write_sequnlock(seqlock_t *s)
{
    s->sequence++;  /* 顺序号再加 1，表示写操作完成 */
    spin_unlock(&s->lock);
}

/* 读开始 */
unsigned int read_seqbegin(seqlock_t *s)
{
    unsigned int seq = s->sequence;  /* 读取当前顺序号 */
    return seq & ~1;  /* 确保是偶数（未在写） */
}

/* 读重试检查 */
int read_seqretry(seqlock_t *s, unsigned int start)
{
    return s->sequence != start;  /* 顺序号变化说明有写操作 */
}
```

### 顺序号的含义

```
sequence = 0:  初始状态
sequence = 1:  写者获取锁，开始写入
sequence = 2:  写者释放锁，写入完成
sequence = 3:  写者获取锁，开始写入
sequence = 4:  写者释放锁，写入完成
...
```

- **偶数**：无写者持有锁
- **奇数**：写者持有锁

读者通过比较读取前后的顺序号判断是否发生冲突。

## 与读写锁对比

| 特性 | 顺序锁 | 读写锁 |
|------|--------|--------|
| 读者阻塞写者 | 否 | 是（默认） |
| 写者阻塞读者 | 否 | 是 |
| 读者需要重试 | 是 | 否 |
| 写操作复杂度 | 简单 | 复杂 |
| 适用场景 | 读极多写极少 | 读多写少 |

## 注意事项

1. **写操作必须快**：读者的重试开销与写操作时间成正比
2. **数据类型限制**：
   - 写者写入时，读者可能读到部分更新
   - 需要确保复合操作（如 struct 赋值）是原子的
3. **避免指针**：写者修改指针时，读者可能读到悬空指针
4. **性能**：在写频繁时性能可能不如读写锁

---

*上一页：[读写锁](./rwlock.md) | 下一页：[死锁调试](./deadlock-debug.md)*

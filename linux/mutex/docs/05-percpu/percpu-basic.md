# Per-CPU 基础

## 概述

Per-CPU 变量为系统中的每个 CPU 创建一个独立的数据副本。每个 CPU 只读写自己的副本，完全避免了锁的使用，同时具有很好的缓存局部性。

## 常用 API

### 定义

```c
/* 静态定义 */
DEFINE_PER_CPU(type, name);           /* 常见类型 */
DEFINE_PER_CPU(int, my_counter);      /* 整数计数器 */
DEFINE_PER_CPU(unsigned long, flags); /* 标志 */

/* 数组 */
DEFINE_PER_CPU(int[10], array);

/* 读写 */
int get_cpu_var(var);    /* 获取并禁止抢占 */
void put_cpu_var(var);   /* 恢复抢占 */

/* 简化的读写（需要手动禁止抢占） */
this_cpu_read(var);      /* 读取当前 CPU 的值 */
this_cpu_write(var, val); /* 写入当前 CPU 的值 */

/* 其他操作 */
this_cpu_inc(var);       /* 当前 CPU 变量加 1 */
this_cpu_dec(var);       /* 当前 CPU 变量减 1 */
this_cpu_add(var, val);  /* 当前 CPU 变量加 val */
this_cpu_xchg(var, val); /* 交换当前 CPU 变量的值 */
```

### 动态分配

```c
/* 动态分配 per-cpu 变量 */
void *alloc_percpu(type);          /* 分配 */
void *alloc_percpu_gfp(type, gfp); /* 带 GFP 标志 */

/* 释放 */
void free_percpu(void * __percpu);
```

### 访问其他 CPU 的数据

```c
/* 访问指定 CPU 的变量 - 很少使用 */
per_cpu(var, cpu_id);  /* 获取指定 CPU 的副本 */
```

## 使用示例

### 示例 1：CPU 本地计数器

```c
/* 每 CPU 一个计数器，统计每个 CPU 处理的事件数 */
DEFINE_PER_CPU(unsigned long, event_count);

void count_event(void)
{
    /* this_cpu_inc 自动处理 CPU 切换 */
    this_cpu_inc(event_count);
}

void get_total_events(void)
{
    unsigned long total = 0;
    int cpu;

    for_each_possible_cpu(cpu) {
        total += per_cpu(event_count, cpu);
    }
    printk("Total events: %lu\n", total);
}
```

### 示例 2：每 CPU 缓冲区

```c
DEFINE_PER_CPU(struct buffer, cpu_buffer);

void process_on_cpu(void *data)
{
    struct buffer *buf = this_cpu_ptr(&cpu_buffer);

    /* 每个 CPU 有自己的缓冲区，无需加锁 */
    push_buffer(buf, data);
    process_buffer(buf);
}
```

### 示例 3：统计数据收集

```c
DEFINE_PER_CPU(struct stats, cpu_stats);

void record_stat(int value)
{
    this_cpu_inc(cpu_stats.count);
    this_cpu_add(cpu_stats.sum, value);
}

struct stats get_global_stats(void)
{
    struct stats total = {0, 0};
    int cpu;

    for_each_possible_cpu(cpu) {
        struct stats *s = &per_cpu(cpu_stats, cpu);
        total.count += s->count;
        total.sum += s->sum;
    }
    return total;
}
```

### 示例 4：中断处理中的 Per-CPU

```c
DEFINE_PER_CPU(int, irq_nest_count);

void handle_irq(void)
{
    this_cpu_inc(irq_nest_count);
    /* 处理中断 */
    this_cpu_dec(irq_nest_count);
}
```

## 底层实现

### 内存布局

Per-CPU 变量的内存布局：

```
CPU 0: [per_cpu_var][per_cpu_var]...
CPU 1: [per_cpu_var][per_cpu_var]...
CPU 2: [per_cpu_var][per_cpu_var]...
...
```

每个 CPU 的数据在内存中是连续的，通过偏移量访问。

### 实现机制

```c
/* this_cpu_read 的实现 */
#define this_cpu_read(p) \
    ({ \
        typeof(p) __p = (p); \
        __raw_this_cpu_read(__p); \
    })

/* 底层实现 */
static inline void __raw_this_cpu_read(void *p)
{
    /* 直接读取当前 CPU 的数据 */
    return *this_cpu_ptr(p);
}
```

### 禁用抢占

```c
/* get_cpu_var 实现 */
#define get_cpu_var(var) \
    ({ \
        preempt_disable(); \
        this_cpu_ptr(var); \
    })

/* put_cpu_var 实现 */
#define put_cpu_var(var) \
    preempt_enable()
```

## 与锁对比

| 特性 | Per-CPU | 锁 |
|------|---------|-----|
| 并发性能 | 极佳 | 有开销 |
| 实现复杂度 | 低 | 高 |
| CPU 通信 | 需要额外机制 | 天然支持 |
| 内存使用 | N 份（N=CPU数） | 1份 |

## 使用场景

1. **CPU 本地统计**：如每 CPU 事件计数
2. **缓存友好的数据结构**：如每 CPU 缓存
3. **避免竞争的数据**：如每个 CPU 独立处理的任务
4. **中断处理**：如中断统计、嵌套计数

## 注意事项

1. **CPU 数量变化**：在 CPU 热插拔时需要特殊处理
2. **内存占用**：每个变量占用 N 份内存（N=CPU数）
3. **初始化**：Per-CPU 变量需要正确初始化
4. **访问效率**：避免在 per-cpu 变量间频繁切换

---

*上一页：[Per-CPU 概述](./README.md) | 下一页：[面试题](./interview.md)*

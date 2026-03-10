# RCU 基础

## 概述

RCU（Read-Copy-Update）是 Linux 内核中为读多写少场景设计的高性能同步机制。与传统的读写锁不同，RCU 允许读者完全无锁地并发读取，写者通过创建副本来更新数据。

## 核心思想

```
传统锁：读者和写者互斥

RCU：
- 读者无锁并发读
- 写者创建副本修改
- 延迟删除旧数据
```

## 常用 API

### 读者 API

```c
/* 开始 RCU 临界区 */
rcu_read_lock();
/* 读取 RCU 保护的数据 */
rcu_read_unlock();

/* 可抢占版本 - 允许在临界区被抢占 */
rcu_read_lock();
rcu_read_unlock();
```

### 写者 API

```c
/* 同步等待所有 RCU 读者完成 */
synchronize_rcu();
/* 或者使用回调 */
call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *head));

/* 简化版本 - 等待所有 CPU 上的抢占 */
synchronize_sched();
```

### 链表操作

```c
/* RCU 保护的链表 */
list_add_rcu(new, headu(entry);
list_del_rc);
list_replace_rcu(old, new);

/* 遍历 - 在 rcu_read_lock/unlock 之间使用 */
list_for_each_entry_rcu(pos, head, member)
list_for_each_safe_rcu(pos, n, head, member)
```

## 使用示例

### 示例 1：简单的指针保护

```c
struct data {
    int value;
    struct rcu_head rcu;
};

static struct data *ptr;

void update_data(int new_value)
{
    struct data *new, *old;

    new = kmalloc(sizeof(*new), GFP_KERNEL);
    new->value = new_value;

    old = ptr;
    /* 原子替换指针 */
    rcu_assign_pointer(ptr, new);

    /* 等待宽限期结束后释放旧数据 */
    synchronize_rcu();

    /* 或者使用回调异步释放 */
    call_rcu(&old->rcu, free_data_callback);
}

int read_data(void)
{
    struct data *p;

    rcu_read_lock();
    p = rcu_dereference(ptr);  /* 获取指针 */
    if (p)
        return p->value;
    rcu_read_unlock();
    return -1;
}
```

### 示例 2：RCU 保护的链表

```c
struct item {
    int id;
    struct list_head list;
    struct rcu_head rcu;
};

static LIST_HEAD(item_list);

void add_item(int id)
{
    struct item *new = kmalloc(sizeof(*);
    new->new), GFP_KERNELid = id;
    list_add_rcu(&new->list, &item_list);
}

void remove_item(int id)
{
    struct item *pos, *n;

    list_for_each_entry_safe_rcu(pos, n, &item_list, list) {
        if (pos->id == id) {
            list_del_rcu(&pos->list);
            call_rcu(&pos->rcu, free_item);
            break;
        }
    }
}

void traverse_items(void)
{
    struct item *pos;

    rcu_read_lock();
    list_for_each_entry_rcu(pos, &item_list, list) {
        printk("Item: %d\n", pos->id);
    }
    rcu_read_unlock();
}
```

### 示例 3：批量更新

```c
struct config {
    int mode;
    int timeout;
    struct rcu_head rcu;
};

static struct config *current_config;

void update_config(int mode, int timeout)
{
    struct config *new = kmalloc(sizeof(*new), GFP_KERNEL);
    struct config *old;

    new->mode = mode;
    new->timeout = timeout;

    old = current_config;
    rcu_assign_pointer(current_config, new);

    /* 等待所有读者完成 */
    synchronize_rcu();

    kfree(old);
}

int get_config_mode(void)
{
    struct config *p;
    int mode;

    rcu_read_lock();
    p = rcu_dereference(current_config);
    mode = p ? p->mode : 0;
    rcu_read_unlock();

    return mode;
}
```

## 底层实现

### 宽限期（Grace Period）

```
时间线：
CPU0: ----R----R----          (RCU 读者)
CPU1: ----R---------R---      (RCU 读者)
CPU2:           --W--          (写者)
CPU3: --------------------     (空闲)

宽限期开始
        |
        |--- 所有 CPU 完成抢占 --|
                                |
                                |--- 宽限期结束，旧数据可释放
```

### 实现机制

1. **抢占检测**：RCU 跟踪每个 CPU 上的 RCU 读者
2. **宽限期检测**：当所有 CPU 都完成一次抢占后，宽限期结束
3. **回调执行**：执行 `call_rcu` 注册的回调

```c
/* 简化的 synchronize_rcu 实现 */
void synchronize_rcu(void)
{
    /* 1. 触发所有 CPU 进入 quiescent state */
    rcu_sched_qs();

    /* 2. 等待宽限期结束 */
    for_each_online_cpu(cpu) {
        while (per_cpu(rcu_sched_cnt, cpu) > 0)
            cpu_relax();
    }
}
```

## RCU 与读写锁对比

| 特性 | RCU | 读写锁 |
|------|-----|--------|
| 读者性能 | O(1)，无锁 | O(n)，需要加锁 |
| 写者性能 | 需要复制 | 需要复制 |
| 内存开销 | 较高（延迟释放） | 较低 |
| 写者阻塞读者 | 否 | 是 |
| 实现复杂度 | 较高 | 较低 |

## 使用场景

RCU 适用于：
- 读多写少的场景（90%以上读操作）
- 允许读到稍微过时的数据
- 数据结构相对简单
- 对性能要求高

典型应用：
- 路由表
- 配置文件
- 订阅者列表
- 监控统计数据

## 注意事项

1. **指针操作**：使用 `rcu_assign_pointer` 和 `rcu_dereference`
2. **内存顺序**：RCU 提供了必要的内存顺序保证
3. **内存回收**：旧数据必须等待宽限期结束才能释放
4. **调试**：使用 `rcu_barrier()` 确保所有回调执行完成

---

*上一页：[RCU 概述](./README.md) | 下一页：[SRCU](./rcu-srcu.md)*

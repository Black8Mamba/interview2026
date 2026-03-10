# SRCU (Sleepable RCU)

## 概述

SRCU（Sleepable RCU）是 RCU 的一种变体，允许在 RCU 临界区内睡眠。这使得 SRCU 可以用于那些需要在临界区内调用可能导致睡眠的操作的场景。

## 与普通 RCU 的区别

| 特性 | RCU | SRCU |
|------|-----|------|
| 临界区睡眠 | 禁止 | 允许 |
| 使用场景 | 进程/软中断上下文 | 任何上下文 |
| 性能开销 | 较低 | 较高 |
| 内存占用 | 较低 | 每个 SRCU 需要额外内存 |

## 常用 API

### 初始化

```c
/* 静态初始化 */
static DEFINE_SRCU(srcu);

/* 动态初始化 */
struct srcu_struct srcu;
srcu_init(&srcu);
```

### 读者 API

```c
/* 在 SRCU 临界区内可以睡眠 */
int idx;
idx = srcu_read_lock(&srcu);
/* 读取保护的数据 - 可以睡眠 */
srcu_read_unlock(&srcu, idx);
```

### 写者 API

```c
/* 同步等待 - 等待所有 SRCU 读者完成 */
synchronize_srcu(&srcu);

/* 异步回调 */
srcu_call_rcu(&srcu, &head, callback);
```

## 使用示例

### 示例 1：文件系统中的 SRCU

```c
static DEFINE_SRCU(vfs_srcu);

struct mount_point {
    const char *path;
    struct srcu_struct srcu;
};

static struct mount_point *current_mount;

void set_mount(const char *path)
{
    struct mount_point *new, *old;

    new = kmalloc(sizeof(*new), GFP_KERNEL);
    new->path = kstrdup(path, GFP_KERNEL);
    srcu_init(&new->srcu);

    old = current_mount;
    rcu_assign_pointer(current_mount, new);

    /* 等待所有读者完成 */
    synchronize_srcu(old ? &old->srcu : NULL);

    if (old)
        kfree(old->path);
    kfree(old);
}

const char *get_mount_path(void)
{
    int idx;
    const char *path;

    /* 可以在临界区内睡眠 */
    idx = srcu_read_lock(&current_mount->srcu);
    path = current_mount->path;
    /* 做其他可能睡眠的操作 */
    srcu_read_unlock(&current_mount->srcu, idx);

    return path;
}
```

### 示例 2：需要睡眠的数据处理

```c
DEFINE_SRCU(task_srcu);

struct task_data {
    int id;
    void (*process)(void *);
    struct rcu_head rcu;
};

static LIST_HEAD(task_list);

void add_task(struct task_data *task)
{
    list_add_rcu(&task->list, &task_list);
}

void process_tasks(void)
{
    struct task_data *task;

    rcu_read_lock();
    list_for_each_entry_rcu(task, &task_list, list) {
        /* 这里可以调用可能睡眠的函数 */
        if (task->process)
            task->process(task);
    }
    rcu_read_unlock();
}
```

## 实现原理

### 数据结构

```c
struct srcu_struct {
    int completed;  /* 宽限期计数器 */
    struct mutex mutex;  /* 保护 per-CPU 计数 */
    int *per_cpu_ref;
};
```

### 宽限期检测

SRCU 使用不同的宽限期检测机制：

1. 每个 SRCU 有独立的计数器
2. `srcu_read_lock()` 返回一个索引
3. `srcu_read_unlock()` 使用该索引
4. `synchronize_srcu()` 等待所有索引对应的计数归零

## 使用场景

SRCU 适用于：
- 需要在 RCU 临界区内睡眠的场景
- 多个独立的数据结构需要各自的 RCU 语义
- 不适合使用普通 RCU 的场景

## 注意事项

1. **性能开销**：SRCU 比普通 RCU 慢，不适合高性能场景
2. **内存**：每个 SRCU 结构需要额外的内存
3. **正确性**：确保 `srcu_read_lock/unlock` 配对使用

---

*上一页：[RCU 基础](./rcu-basic.md) | 下一页：[面试题](./interview.md)*

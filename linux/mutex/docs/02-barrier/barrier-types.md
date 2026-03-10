# 硬件内存屏障

## 概述

硬件内存屏障是 CPU 提供的指令，用于确保在多核处理器环境下内存访问的顺序性。现代 CPU 为了提高性能会进行指令乱序执行和 Store Buffering，内存屏障可以强制 CPU 按照程序顺序完成内存操作。

## 内存屏障的类型

### 1. 完整内存屏障（Full Memory Barrier）

`mb()` - 阻止所有类型的内存重排

```c
/* 完整内存屏障 - 阻止所有内存操作重排 */
mb();
```

**保证**：
- 所有在 mb() 之前的内存操作都在 mb() 之后的内存操作之前完成
- 适用于需要严格顺序的场景

### 2. 读内存屏障（Read Memory Barrier）

`rmb()` - 仅阻止读操作重排

```c
/* 读内存屏障 - 只阻止读操作重排 */
rmb();
```

**保证**：
- 所有在 rmb() 之前的读操作都在 rmb() 之后的读操作之前完成
- 写操作可能仍会被重排

### 3. 写内存屏障（Write Memory Barrier）

`wmb()` - 仅阻止写操作重排

```c
/* 写内存屏障 - 只阻止写操作重排 */
wmb();
```

**保证**：
- 所有在 wmb() 之前的写操作都在 wmb() 之后的写操作之前完成
- 读操作可能仍会被重排

## SMP 屏障

在内核中，通常使用 SMP（对称多处理）版本的屏障：

| 函数 | 描述 |
|------|------|
| `smp_mb()` | SMP 完整内存屏障 |
| `smp_rmb()` | SMP 读内存屏障 |
| `smp_wmb()` | SMP 写内存屏障 |

### SMP vs 非 SMP

```c
/* SMP 版本 - 多处理器 */
smp_mb();    /* 在 SMP 系统上完整屏障，在 UP 上可能为空 */

/* 非 SMP 版本 - 通用 */
mb();        /* 始终是完整屏障 */
```

**优化策略**：
- 在 UP（单处理器）系统上，这些屏障可以是空操作
- 在 SMP 系统上，需要完整的 barrier 指令

## 使用示例

### 示例 1：自旋锁中的内存顺序

```c
typedef struct {
    spinlock_t lock;
    int data;
    bool ready;
} shared_t;

void producer(shared_t *s)
{
    spin_lock(&s->lock);
    s->data = 42;    /* 写入数据 */
    s->ready = true; /* 设置标志 */
    spin_unlock(&s->lock);
}

void consumer(shared_t *s)
{
    while (!s->ready) {
        cpu_relax(); /* 自旋等待 */
    }
    /* 需要屏障确保读到 s->ready=true 时，s->data 已被写入 */
    smp_mb();
    print(s->data);
}
```

### 示例 2：无锁队列

```c
struct node {
    void *data;
    struct node *next;
};

void enqueue(struct node **head, struct node *new_node)
{
    new_node->next = *head;
    /* 内存屏障确保 next 指针在 data 写入之后 */
    smp_wmb();
    *head = new_node;  /* 更新头指针 */
}

void *dequeue(struct node **head)
{
    struct node *node = *head;
    if (!node)
        return NULL;
    /* 内存屏障确保读取到新头指针后，数据已正确 */
    smp_mb();
    *head = node->next;
    return node->data;
}
```

### 示例 3：设备驱动中的 MMIO

```c
void write_command(u32 cmd)
{
    /* 写命令寄存器 */
    writel(cmd, CTRL_REG);
    /* 确保命令写入完成后再发送 */
    wmb();
    /* 读状态寄存器确认 */
    while (!(readl(STATUS_REG) & CMD_DONE))
        ;
}
```

## x86 架构实现

### x86 的内存模型

x86 处理器提供 **TSO（Total Store Order）** 内存模型，这意味着：
- 读不会重排到写之前
- 写不会重排到读之前
- 写会按程序顺序执行

但以下情况仍可能发生：
- 读可以重排到写之后
- 写缓存可能被后续读绕过

### x86 内存屏障实现

```asm
; smp_mb() 在 x86 上的实现
lock; addl $0,0(%esp)

; 说明：
; lock 前缀锁定总线/缓存
; addl $0,0(%esp) 是一个空操作，但 lock 前缀强制内存顺序
```

```asm
; smp_wmb() 在 x86 上通常是空操作
; 因为 x86 的写操作不会重排

; smp_rmb() 在 x86 上也是空操作
; 因为 x86 的读操作不会重排到写之前
```

### SFENCE 和 MFENCE

```asm
; 写屏障 - 确保所有写操作 flush 到内存
sfence

; 读屏障 - 确保所有读操作完成
lfence

; 完整屏障 - 确保所有内存操作完成
mfence
```

Linux 内核的 `smp_mb()` 在某些架构上会使用这些指令。

## ARM/ARM64 架构

ARM 架构使用更弱的内存模型，允许更多类型的重排：

```c
/* ARM64 内存屏障实现 */
#define smp_mb()  dsb(ish)
#define smp_rmb() dsb(ishld)
#define smp_wmb() dsb(ishst)
```

`dsb`（Data Synchronization Barrier）指令强制等待所有数据访问完成。

## 锁与内存屏障的关系

获取锁时隐含了适当的内存屏障：

```c
/* spin_lock 包含必要的内存屏障 */
spin_lock(&lock);
/* - 阻止了锁获取前的内存操作重排到锁获取之后 - */

/* 临界区代码 */

/* spin_unlock 包含必要的内存屏障 */
spin_unlock(&lock);
/* - 阻止了临界区的操作重排到锁释放之后 - */
```

## 注意事项

1. **不要滥用**：内存屏障有性能开销，只在必要时使用
2. **理解内存模型**：不同架构有不同的内存模型
3. **配合原子操作**：原子操作 + 内存屏障 = 完整的顺序保证
4. **调试困难**：内存顺序问题通常难以复现和调试

---

*上一页：[内存屏障概述](./README.md) | 下一页：[编译乱序屏障](./compiler-barrier.md)*

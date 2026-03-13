# 第3章 中断处理流程

本章将深入讲解Linux内核中断处理的完整流程，从硬件中断发生到内核处理函数的调用过程，以及中断上下文、softirq、tasklet、workqueue和中断线程化等重要概念。这些知识是理解Linux中断子系统的核心，适合驱动开发者和系统性能工程师深入学习。

## 3.1 硬件中断处理流程

### 3.1.1 整体流程概述

从硬件外设触发中断到Linux内核处理函数被调用的完整流程涉及多个层次的协作。以下是整个处理流程的概述：

```
外设触发中断
    |
    v
CPU异常向量（硬件跳转）
    |
    v
内核中断入口（entry.S）
    |
    v
异常处理分发（handle_arch_exception）
    |
    v
查找irq_desc（通过irq_domain映射）
    |
    v
调用handle_irq流处理函数
    |
    v
遍历action链表，调用handler
    |
    v
返回IRQ_HANDLED/IRQ_NONE
```

### 3.1.2 从异常到中断处理

在ARM64架构中，当外设触发中断时，CPU会跳转到预定义的异常向量表。以下是中断入口的核心处理流程：

```c
// arch/arm64/kernel/entry.S
.macro    kernel_entry, el, regsize = 64
    // 保存现场：保存通用寄存器和PSTATE
    stp    x0, x1, [sp, #S_X0]
    stp    x2, x3, [sp, #S_X2]
    // ... 保存更多寄存器

    // 获取当前任务的pt_regs
    adr    x25, current_task
    ldr    x26, [x25, #TSK_TI_FLAGS]

    // 检查栈指针
    tbnz    x26, #TIF_SIGPENDING, ret_to_user
.endm

// 中断入口点
el1_interrupt:
    kernel_entry 1
    mov    x0, sp
    bl    handle_arch_exception
    b    ret_to_kernel
```

### 3.1.3 handle_irq 函数调用链

`handle_irq`是中断流处理的核心函数，它根据中断的触发类型选择不同的处理策略。以GIC控制器为例，主要使用`handle_fasteoi_irq`：

```c
// kernel/irq/chip.c
void handle_fasteoi_irq(struct irq_desc *desc)
{
    struct irq_chip *chip = desc->irq_data.chip;

    raw_spin_lock(&desc->lock);

    // 屏蔽当前中断
    if (irqd_irq_inprogress(&desc->irq_data)) {
        raw_spin_unlock(&desc->lock);
        return;
    }

    irqd_set(&desc->irq_data, IRQD_IRQ_INPROGRESS);

    // 确认中断（对于边沿触发）
    chip->irq_ack(&desc->irq_data);

    // 遍历action链表，调用处理函数
    desc->action_ret = handle_irq_event(desc);

    // 发送EOI信号
    chip->irq_eoi(&desc->irq_data);

    irqd_clear(&desc->irq_data, IRQD_IRQ_INPROGRESS);
    raw_spin_unlock(&desc->lock);
}
```

`handle_irq_event`函数负责遍历并执行所有注册的中断处理函数：

```c
// kernel/irq/handle.c
irqreturn_t handle_irq_event(struct irq_desc *desc)
{
    irqreturn_t retval = IRQ_NONE;
    struct irqaction *action;

    // 遍历action链表
    for_each_action_of_desc(desc, action) {
        irqreturn_t res;

        // 调用处理函数
        res = action->handler(irq, action->dev_id);
        if (res == IRQ_HANDLED)
            irq_stats(desc->irq_data.irq, IRQS_HANDOVER_TO_DAEMON) = 1;

        retval |= res;
    }

    return retval;
}
```

### 3.1.4 中断处理函数返回值

中断处理函数必须返回特定的枚举值，表示中断是否被正确处理：

```c
// include/linux/irqreturn.h
enum irqreturn {
    IRQ_NONE        = (0 << 0),    // 中断不是由本设备产生
    IRQ_HANDLED     = (1 << 0),    // 中断已由本设备处理
    IRQ_WAKE_THREAD = (1 << 1),    // 唤醒中断处理线程
};

typedef enum irqreturn irqreturn_t;

// 使用示例
static irqreturn_t my_device_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    u32 status = readl(dev->regs + STATUS_REG);

    if (!(status & DEVICE_IRQ_PENDING))
        return IRQ_NONE;  // 不是本设备的中断

    // 清除中断标志
    writel(status, dev->regs + STATUS_REG);

    // 处理中断
    dev->handled = true;

    return IRQ_HANDLED;
}
```

## 3.2 中断上下文

### 3.2.1 上下文概念

Linux内核中存在两种不同的执行上下文：进程上下文和中断上下文。理解它们的区别对于编写正确的驱动代码至关重要。

**进程上下文**是进程在执行系统调用或用户空间代码时的状态。在这种上下文中，代码可以访问用户空间内存，可以调用阻塞函数（如`sleep_on`），并且`current`宏指向当前运行的进程。

**中断上下文**是中断处理程序执行时的状态。此时没有对应的进程，`current`宏仍然指向被中断的进程，但不等于真正运行在该进程的上下文中。中断上下文的特点包括：

- 不能访问用户空间虚拟内存
- 不能调用可能阻塞的函数
- 不能进行上下文切换
- 执行时间应该尽可能短

### 3.2.2 上下文判断函数

Linux内核提供了多个宏和函数用于判断当前执行上下文：

```c
// include/linux/preempt.h
// 判断是否在中断上下文中（包括硬件中断和软中断）
#define in_interrupt()    (preempt_count() & (NMI_MASK | HARDIRQ_MASK | SOFTIRQ_MASK))

// 判断是否在硬件中断处理中
#define in_irq()          (hardirq_count())

// 判断是否在软中断上下文中
#define in_softirq()      (softirq_count())

// 判断是否在NMI上下文中
#define in_nmi()          (nmi_count())
```

`preempt_count`是理解中断上下文的关键，它是一个整数值，按位编码不同类型的信息：

```c
// include/linux/preempt.h
/*
 * preempt_count的位域布局：
 * [0-7]   : 抢占计数（preempt counter）
 * [8-15]  : 软中断计数（softirq counter）
 * [16-23] : 硬件中断计数（hardirq counter）
 * [24-31] : NMI计数（NMI counter）
 */
#define PREEMPT_BITS    8
#define SOFTIRQ_BITS   8
#define HARDIRQ_BITS   8
#define NMI_BITS       8

#define PREEMPT_SHIFT  0
#define SOFTIRQ_SHIFT  (PREEMPT_SHIFT + PREEMPT_BITS)
#define HARDIRQ_SHIFT  (SOFTIRQ_SHIFT + SOFTIRQ_BITS)
#define NMI_SHIFT      (HARDIRQ_SHIFT + HARDIRQ_BITS)

#define HARDIRQ_MASK   ((1UL << HARDIRQ_BITS) - 1)
#define SOFTIRQ_MASK   ((1UL << SOFTIRQ_BITS) - 1)
#define NMI_MASK       ((1UL << NMI_BITS) - 1)
```

### 3.2.3 实践示例

以下示例展示如何在驱动代码中正确判断上下文：

```c
// 在中断处理函数中（中断上下文）
static irqreturn_t my_device_isr(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* 安全打印：在中断上下文中可以使用printk */
    dev_info(&dev->pci_dev->dev, "In interrupt context\n");

    /* 正确做法：不能调用可能阻塞的函数 */
    // down(&dev->sem);  /* 错误：可能阻塞 */

    /* 正确做法：只做最基本的处理，其余工作交给软中断或工作队列 */
    tasklet_schedule(&dev->tasklet);

    return IRQ_HANDLED;
}

// 在进程上下文中（通过workqueue）
static void my_device_work(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);

    /* 确认在进程上下文中 */
    WARN_ON(in_interrupt());

    /* 可以调用阻塞函数 */
    down(&dev->sem);
    /* 处理中断后的工作... */
    up(&dev->sem);
}
```

## 3.3 softirq 机制

### 3.3.1 softirq 简介

softirq（软中断）是Linux内核实现延迟处理的重要机制。它在硬件中断处理完成后执行，用于处理那些需要较快响应但又不能在硬件中断处理函数中完成的耗时工作。softirq在硬件中断的上下文中被触发，但在软中断上下文中执行。

softirq与硬件中断的主要区别：
- 硬件中断会屏蔽同类中断，softirq不会
- 硬件中断在CPU屏蔽中断后不再响应，softirq可以被延迟执行
- 硬件中断处理时间应该尽可能短，softirq可以处理相对耗时的任务

### 3.3.2 softirq 定义与优先级

Linux内核预定义了多个softirq类型，它们具有不同的优先级：

```c
// include/linux/interrupt.h
enum {
    HI_SOFTIRQ = 0,        // 最高优先级，用于tasklet
    TIMER_SOFTIRQ,        // 定时器软中断
    NET_TX_SOFTIRQ,       // 网络发送
    NET_RX_SOFTIRQ,       // 网络接收
    BLOCK_SOFTIRQ,        // 块设备
    IRQ_POLL_SOFTIRQ,     // IRQ轮询
    TASKLET_SOFTIRQ,      // tasklet（优先级较低）
    SCHED_SOFTIRQ,        // 调度软中断
    HRTIMER_SOFTIRQ,      // 高精度定时器
    RCU_SOFTIRQ,          // RCU回调
    NR_SOFTIRQS
};
```

### 3.3.3 softirq_action 结构

每个注册的softirq由`softirq_action`结构表示：

```c
// include/linux/interrupt.h
struct softirq_action {
    void    (*action)(struct softirq_action *);
};
```

系统中有一个全局的softirq_action数组：

```c
// kernel/softirq.c
static struct softirq_action softirq_vec[NR_SOFTIRQS];
```

### 3.3.4 触发 softirq

驱动可以通过多种方式触发softirq：

```c
// 方式1：触发指定类型的softirq（会在下次软中断处理时执行）
void raise_softirq(unsigned int nr);

// 方式2：触发softirq并立即处理（当前上下文）
void raise_softirq_irqoff(unsigned int nr);

// 使用示例：驱动中触发NET_RX_SOFTIRQ
void my_network_rx_notify(struct my_device *dev)
{
    /* 通知内核有网络数据包到达 */
    raise_softirq(NET_RX_SOFTIRQ);
}
```

`raise_softirq_irqoff`是更高效的版本，因为它不需要额外的检查：

```c
// kernel/softirq.c
void raise_softirq_irqoff(unsigned int nr)
{
    __raise_softirq_irqoff(nr);

    /* 如果不在中断上下文中，可能需要唤醒ksoftirqd线程 */
    if (!in_interrupt())
        wakeup_softirqd();
}

void __raise_softirq_irqoff(unsigned int nr)
{
    trace_softirq_raise(nr);
    or_softirq_pending(1UL << nr);
}
```

### 3.3.5 softirq 处理流程

软中断在以下时机被处理：
1. 从硬件中断返回时
2. 在`__do_softirq`函数中
3. 由ksoftirqd内核线程处理（当软中断较多时）

```c
// kernel/softirq.c
asmlinkage __visible void __do_softirq(void)
{
    unsigned long pending;
    unsigned int max_restart = MAX_SOFTIRQ_RESTART;
    struct softirq_action *h;
    bool in_hardirq;

    /* 保存当前中断状态并屏蔽软中断 */
    pending = local_softirq_pending();
    __local_bh_disable_ip(_RET_IP_, SOFTIRQ_OFFSET);

    in_hardirq = lockdep_hardirq_context();

restart:
    /* 重置待处理的软中断 */
    set_softirq_pending(0);

    local_irq_enable();

    /* 遍历并处理所有pending的softirq */
    h = softirq_vec;
    while (pending) {
        unsigned int softirq_nr = __builtin_ctz(pending);
        pending &= ~(1UL << softirq_nr);

        if (in_hardirq)
            trace_hardirq_exit();

        /* 执行softirq处理函数 */
        h->action(h);
    }

    local_irq_disable();

    /* 检查是否有新的softirq到达 */
    pending = local_softirq_pending();
    if (pending) {
        if (--max_restart)
            goto restart;
    }

    wakeup_softirqd();
}
```

## 3.4 tasklet 与 workqueue

### 3.4.1 tasklet 机制

tasklet是建立在softirq之上的高级接口，专门用于驱动中的延迟处理。它的实现基于HI_SOFTIRQ和TASKLET_SOFTIRQ这两种softirq。

```c
// include/linux/interrupt.h
struct tasklet_struct {
    struct tasklet_struct *next;      // 链表指针
    unsigned long state;             // 状态
    atomic_t count;                  // 引用计数
    void (*func)(unsigned long);     // 处理函数
    unsigned long data;              // 传递给函数的参数
};
```

**tasklet的特点**：
- 每个CPU上串行执行：同一个tasklet不会同时在多个CPU上运行
- 调度简单：使用`tasklet_schedule`即可
- 不能睡眠：运行在软中断上下文

```c
// 使用示例：定义和使用tasklet
struct my_device {
    struct tasklet_struct tlet;
    /* 其他成员 */
};

void my_device_tasklet_handler(unsigned long data)
{
    struct my_device *dev = (struct my_device *)data;

    /* 这里处理中断后的工作 */
    dev_info(&dev->pci_dev->dev, "Tasklet running\n");

    /* 读取和处理数据 */
}

static int my_driver_probe(struct pci_dev *pci_dev,
                           const struct pci_device_id *id)
{
    struct my_device *dev;
    int ret;

    dev = devm_kzalloc(&pci_dev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    /* 初始化tasklet */
    tasklet_init(&dev->tlet, my_device_tasklet_handler,
                 (unsigned long)dev);

    /* 请求中断 */
    ret = request_irq(pci_dev->irq, my_device_isr,
                      IRQF_SHARED, "my_driver", dev);
    if (ret)
        goto err_free;

    return 0;

err_free:
    tasklet_kill(&dev->tlet);
    return ret;
}

static irqreturn_t my_device_isr(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* 在中断处理函数中调度tasklet */
    tasklet_schedule(&dev->tlet);

    return IRQ_HANDLED;
}

static void my_driver_remove(struct pci_dev *pci_dev)
{
    struct my_device *dev = pci_get_drvdata(pci_dev);

    free_irq(pci_dev->irq, dev);
    tasklet_kill(&dev->tlet);
}
```

### 3.4.2 workqueue 机制

workqueue是另一种延迟处理机制，与tasklet的主要区别在于：**workqueue在进程上下文执行**，可以睡眠和阻塞。

```c
// 使用示例：定义和使用workqueue
struct my_device {
    struct work_struct work;
    /* 其他成员 */
};

void my_device_work_handler(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);

    /* 确认在进程上下文中 */
    WARN_ON(in_interrupt());

    /* 可以调用可能阻塞的函数 */
    msleep(100);

    dev_info(&dev->pci_dev->dev, "Workqueue running\n");
}

static int my_driver_probe(struct pci_dev *pci_dev,
                           const struct pci_device_id *id)
{
    struct my_device *dev;

    dev = devm_kzalloc(&pci_dev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    /* 初始化work */
    INIT_WORK(&dev->work, my_device_work_handler);

    /* 请求中断 */
    return request_irq(pci_dev->irq, my_device_isr,
                      IRQF_SHARED, "my_driver", dev);
}

static irqreturn_t my_device_isr(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* 调度work到系统workqueue */
    schedule_work(&dev->work);

    return IRQ_HANDLED;
}
```

### 3.4.3 tasklet 与 workqueue 对比

| 特性 | tasklet | workqueue |
|------|---------|------------|
| 执行上下文 | 软中断上下文 | 进程上下文 |
| 是否可睡眠 | 不可睡眠 | 可以睡眠 |
| 调度方式 | tasklet_schedule | schedule_work |
| 并行性 | 同一tasklet不能并行 | 同一work可以并行（默认） |
| CPU亲和性 | 在当前CPU执行 | 系统调度决定 |
| 性能开销 | 较低 | 较高 |
| 使用场景 | 简单的延迟处理 | 需要阻塞的延迟处理 |

## 3.5 中断线程化

### 3.5.1 为什么需要中断线程化

传统的中断处理在硬件中断上下文中执行，这意味着：

- 不能睡眠或阻塞
- 高优先级中断可能长时间阻塞低优先级任务
- 实时性难以保证

中断线程化（Threaded Interrupts）将中断处理分为两部分：
1. **硬件处理函数（handler）**：在中断上下文执行，做最基本的处理
2. **线程处理函数（thread_fn）**：在独立内核线程中执行，可以睡眠

### 3.5.2 request_threaded_irq

使用`request_threaded_irq`注册线程化中断：

```c
// include/linux/interrupt.h
static inline int __must_check
request_threaded_irq(unsigned int irq, irq_handler_t handler,
                    irq_handler_t thread_fn,
                    unsigned long flags, const char *name, void *dev);

// 参数说明：
// irq         : 中断号
// handler     : 硬件中断处理函数（必须实现，不能为NULL）
// thread_fn   : 线程处理函数（可为空，为NULL时不创建线程）
// flags       : 中断标志
// name        : 中断名称（/proc/interrupts显示）
// dev         : 设备标识（共享中断时用于区分）
```

### 3.5.3 使用示例

```c
// 驱动中的线程化中断示例
static irqreturn_t my_device_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    u32 status = readl(dev->regs + STATUS_REG);

    if (!(status & DEVICE_IRQ_PENDING))
        return IRQ_NONE;  // 不是本设备的中断

    /* 在硬件处理函数中清除中断标志，禁止递归 */
    writel(status, dev->regs + STATUS_REG);

    /* 如果有线程化处理函数，返回IRQ_WAKE_THREAD来唤醒线程 */
    if (dev->use_threaded)
        return IRQ_WAKE_THREAD;

    return IRQ_HANDLED;
}

static irqreturn_t my_device_thread_fn(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    struct sk_buff *skb;

    /* 线程化处理函数中可以安全地睡眠和阻塞 */
    dev_info(&dev->pci_dev->dev, "Threaded IRQ handling\n");

    /* 处理接收数据（可能需要等待硬件） */
    while ((skb = my_device_rx(dev)) != NULL) {
        netif_rx(skb);
    }

    return IRQ_HANDLED;
}

static int my_driver_probe(struct pci_dev *pci_dev,
                           const struct pci_device_id *id)
{
    struct my_device *dev;
    int ret;

    dev = devm_kzalloc(&pci_dev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->use_threaded = true;

    /* 使用request_threaded_irq注册线程化中断 */
    ret = request_threaded_irq(pci_dev->irq,
                               my_device_handler,       // 硬件处理
                               my_device_thread_fn,     // 线程处理
                               IRQF_SHARED | IRQF_ONESHOT,
                               "my_driver", dev);
    if (ret) {
        dev_err(&pci_dev->dev, "Failed to request IRQ %d\n", pci_dev->irq);
        return ret;
    }

    return 0;
}
```

### 3.5.4 实时性保障

中断线程化的优势在于提供更好的实时性保证：

1. **优先级继承**：中断线程使用`SCHED_FIFO`或`SCHED_RR`调度，可以设置为高优先级
2. **可抢占**：线程化的中断处理可以被其他高优先级任务抢占
3. **避免饥饿**：不会因为长中断处理阻塞其他中断

```c
// 设置中断线程优先级
static int my_driver_probe(struct pci_dev *pci_dev,
                           const struct pci_device_id *id)
{
    struct my_device *dev;
    int ret;

    /* ... 分配设备结构 ... */

    ret = request_threaded_irq(pci_dev->irq,
                               my_device_handler,
                               my_device_thread_fn,
                               IRQF_SHARED, "my_driver", dev);
    if (ret)
        return ret;

    /* 获取并设置中断线程的优先级 */
    if (dev->irq_desc->thread)
        sched_set_fifo(dev->irq_desc->thread);

    return 0;
}
```

### 3.5.5 IRQ_POLL标志

对于需要轮询的中断设备，可以使用`IRQF_ONESHOT | IRQF_NO_SUSPEND | IRQF_NO_THREAD`组合，但这会导致中断处理延迟。另一种方案是使用`IRQ_POLL`标志：

```c
/* 使用IRQ_POLL进行中断轮询 */
static int my_poll(struct napi_struct *napi, int budget)
{
    struct my_device *dev = container_of(napi, struct my_device, napi);
    int work_done = 0;

    /* 处理数据 */
    work_done = my_device_poll(dev, budget);

    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        enable_irq(dev->irq);
    }

    return work_done;
}

static irqreturn_t my_device_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* 禁用中断，开始轮询 */
    disable_irq_nosync(irq);
    napi_schedule(&dev->napi);

    return IRQ_HANDLED;
}
```

---

## 本章面试题

### 1. 描述Linux中断处理的完整流程

**参考答案**：硬件外设触发中断后，CPU跳转到异常向量表，然后进入内核中断入口保存现场。接着通过中断号查找对应的irq_desc结构，调用handle_irq流处理函数（如handle_fasteoi_irq）。在流处理函数中确认中断、屏蔽中断、遍历action链表调用各处理函数，最后发送EOI信号完成中断处理。

### 2. in_interrupt()、in_irq()和in_softirq()的区别是什么？

**参考答案**：`in_interrupt()`检查是否在任何中断上下文中（包括硬件中断和软中断），通过检测preempt_count的HARDIRQ_MASK和SOFTIRQ_MASK位。`in_irq()`仅检测是否在硬件中断处理中，检查HARDIRQ_MASK位。`in_softirq()`仅检测是否在软中断上下文中，检查SOFTIRQ_MASK位。

### 3. 什么是softirq？它与硬件中断有什么区别？

**参考答案**：softirq是Linux内核用于延迟处理的机制，在硬件中断处理完成后执行。区别包括：硬件中断执行时会屏蔽同类中断，softirq不会；硬件中断处理时间应该尽可能短，softirq可以处理相对耗时的任务；softirq在软中断上下文执行，可以进行更复杂的处理但仍不能睡眠。

### 4. tasklet和workqueue有什么区别？分别在什么场景下使用？

**参考答案**：tasklet运行在软中断上下文，不能睡眠，适用于简单的延迟处理；workqueue运行在进程上下文，可以睡眠，适用于需要阻塞或耗时较长的处理。tasklet性能开销低，workqueue灵活性高。

### 5. 什么是中断线程化？使用request_threaded_irq的好处是什么？

**参考答案**：中断线程化将中断处理分为硬件处理函数（在中断上下文）和线程处理函数（在独立内核线程）。使用request_threaded_irq的好处包括：提供更好的实时性保证，支持优先级设置，可以睡眠和阻塞，避免长时间阻塞硬件中断。

### 6. 在中断处理函数中返回IRQ_WAKE_THREAD的意义是什么？

**参考答案**：返回IRQ_WAKE_THREAD表示告诉内核唤醒该中断对应的处理线程（thread_fn）。这用于线程化中断场景：硬件处理函数只做必要的快速处理，然后返回IRQ_WAKE_THREAD来触发线程化处理函数执行更复杂的任务。

### 7. 为什么中断处理函数应该尽可能短？

**参考答案**：因为中断处理期间会屏蔽当前中断线（对于电平触发），长时间处理会导致：其他设备中断响应延迟；可能丢失中断；系统实时性下降；影响系统整体性能。所以应该将耗时操作放到softirq、tasklet、workqueue或中断线程中处理。

# 第2章 中断框架核心

本章将深入讲解Linux内核中断子系统的核心数据结构，包括irq_desc结构体、irq_chip接口、irqdomain原理以及中断号映射机制。这些是理解Linux中断处理框架的关键，适合驱动开发者和性能工程师深入学习。

## 2.1 irq_desc 结构

### 2.1.1 结构定义

`struct irq_desc`是Linux中断子系统的核心数据结构，每个硬件中断号对应一个`irq_desc`实例。在Linux 6.x中，该结构体的定义位于`include/linux/irq.h`头文件中。

```c
// include/linux/irq.h
struct irq_desc {
    struct irq_common_data    irq_common_data;
    struct irq_data           irq_data;           // 中断数据
    irq_flow_handler_t        handle_irq;         // 中断流处理函数
    struct irqaction         *action;             // 中断处理动作链
    unsigned int              status_use_accessors; // 中断状态
    unsigned int              core_internal_state__do_not_mess_with_it;
    unsigned int              depth;              // 中断嵌套深度
    unsigned int              wake_depth;         // 唤醒深度
    atomic_t                  unhandled;          // 未处理计数
    struct task_struct       *thread;             // 中断线程（线程化中断）
    struct task_struct       *secondary;          // 次级中断线程
    irq_preflow_handler_t     preflow_handler;    // 前置处理函数
    irq_postflow_handler_t    postflow_handler;   // 后置处理函数
    struct irq_desc          *parent;             // 父中断描述符（级联）
    const char                *name;              // 中断名称
} ____cacheline_internodealigned_in_smp;
```

### 2.1.2 主要成员详解

**irq_data**成员封装了中断的硬件相关信息，是与中断控制器交互的接口：

```c
// include/linux/irq.h
struct irq_data {
    u32                     mask;              // 中断掩码
    unsigned int            irq;               // Linux中断号
    unsigned long           hwirq;             // 硬件中断号
    struct irq_chip        *chip;              // 中断控制器芯片
    void                   *chip_data;         // 芯片私有数据
    struct irq_domain      *domain;            // 所属irqdomain
    struct irq_common_data *common;
    struct irq_desc        *desc;              // 指向父描述符
};
```

**handle_irq**是中断流处理函数，根据中断触发类型（边沿触发、电平触发）选择不同的处理策略：

- `handle_edge_irq`：边沿触发中断处理
- `handle_level_irq`：电平触发中断处理
- `handle_simple_irq`：简单中断处理
- `handle_percpu_irq`：每CPU中断处理
- `handle_fasteoi_irq`：快速EOI中断处理（GIC常用）

**action**链表保存该中断的所有处理函数（irqaction结构体），支持多个驱动共享同一个中断号：

```c
// include/linux/interrupt.h
struct irqaction {
    irq_handler_t           handler;            // 中断处理函数
    unsigned long           flags;              // 标志（IRQF_SHARED等）
    const char             *name;               // 设备名称
    void                   *dev_id;            // 设备标识
    void                   *secondary;         // 次级处理函数
    struct irqaction       *next;               // 下一个处理函数
    int                     thread_fn;          // 线程化处理函数
    struct task_struct     *thread;             // 处理线程
    unsigned int            irq;                // 中断号
    unsigned int            num_threads;        // 线程化数量
    unsigned int            percpu_dev_id: 1;   // 每CPU设备ID
    struct irqaction       *secondary_new;     // 新的次级处理函数
};
```

### 2.1.3 堆栈管理

Linux内核的中断堆栈管理涉及两个关键概念：

**中断栈（Interrupt Stack）**：在ARM64架构中，每个CPU核心有两个堆栈指针（SP_EL0和SP_EL1）。当进入中断时，CPU从SP_EL0切换到SP_EL1，使用内核堆栈。对于中断处理，Linux使用专门的中断堆栈：

```c
// arch/arm64/include/asm/stacktrace.h
struct stack_info {
    enum {
        STACK_TYPE_KERNEL,
        STACK_TYPE_IRQ,
        STACK_TYPE_SOFTIRQ,
        STACK_TYPE_NMI,
        STACK_TYPE_ENTRY,
    } type;
    void *begin;
    void *end;
};
```

**栈溢出检测**：Linux内核配置了`CONFIG_DEBUG_STACKOVERFLOW`时，会在中断入口检查栈使用情况：

```c
// arch/arm64/kernel/entry-common.c
static void irqentry_exit_to_user_mode(struct pt_regs *regs)
{
    if (IS_ENABLED(CONFIG_DEBUG_STACKOVERFLOW)) {
        debug_check_irqstack();
    }
    // ... 其他处理
}
```

**内核栈大小**：在ARM64上，默认内核栈大小为16KB，其中约2KB保留用于中断处理。当栈使用超过阈值时会触发警告。

## 2.2 irq_chip 接口

### 2.2.1 结构定义

`struct irq_chip`定义了中断控制器的抽象接口，是Linux内核与硬件中断控制器交互的核心结构。不同的中断控制器（如GIC、ARMv7-PIC）只需要实现各自的chip操作即可。

```c
// include/linux/irq.h
struct irq_chip {
    const char        *name;                // 芯片名称
    struct device     *parent_device;       // 父设备
    const int        *irq_mask;             // 屏蔽中断回调
    const int        *irq_unmask;           // 解除屏蔽回调
    const int        *irq_enable;           // 使能中断
    const int        *irq_disable;          // 禁用中断
    const int        *irq_ack;              // 响应中断（边沿触发）
    const int        *irq_mask_ack;         // 屏蔽并响应中断
    const int        *irq_unmask_norequest; // 解除屏蔽（不检查状态）
    const int        *irq_eoi;              // 发送EOI（结束中断）
    const int        *irq_set_type;         // 设置触发类型
    const int        *irq_set_affinity;    // 设置CPU亲和性
    const int        *irq_retrigger;       // 重新触发中断
    const int        *irq_set_wake;         // 设置唤醒功能
    const int        *irq_read_chip;       // 读取芯片状态
    const int        *irq_bus_lock;        // 总线锁
    const int        *irq_bus_sync_unlock; // 总线同步解锁
    unsigned long     flags;                // 芯片标志
};
```

### 2.2.2 关键回调函数

**irq_enable / irq_disable**：使能和禁用中断

```c
// 典型实现示例（GIC）
static void gic_enable_irq(struct irq_data *d)
{
    if (gic_arch_extn.irq_enable)
        gic_arch_extn.irq_enable(d);
    else
        gic_unmask_irq(d);  // 清除屏蔽
}

static void gic_disable_irq(struct irq_data *d)
{
    if (gic_arch_extn.irq_disable)
        gic_arch_extn.irq_disable(d);
    else
        gic_mask_irq(d);    // 屏蔽中断
}
```

**irq_mask / irq_unmask**：屏蔽和解除屏蔽中断，是最常用的操作

```c
static void gic_mask_irq(struct irq_data *d)
{
    writel_relaxed(gic_dist_base(d), GIC_DIST_ENABLE_CLEAR + (gic_irq(d) / 32) * 4,
                   1UL << (gic_irq(d) % 32));
}
```

**irq_eoi**：End of Interrupt信号，通知中断控制器中断处理完成

```c
static void gic_eoi_irq(struct irq_data *d)
{
    writel_relaxed(gic_irq(d), gic_cpu_base(d) + GIC_CPU_EOI);
}
```

**irq_set_type**：配置中断触发类型

```c
// 触发类型定义
enum {
    IRQ_TYPE_NONE          = 0x00000000,
    IRQ_TYPE_EDGE_RISING   = 0x00000001,  // 上升沿
    IRQ_TYPE_EDGE_FALLING  = 0x00000002,  // 下降沿
    IRQ_TYPE_EDGE_BOTH     = 0x00000003,  // 双边沿
    IRQ_TYPE_LEVEL_HIGH    = 0x00000004,  // 高电平
    IRQ_TYPE_LEVEL_LOW     = 0x00000008,  // 低电平
    IRQ_TYPE_LEVEL_MASK    = 0x0000000C,
};
```

**irq_set_affinity**：设置中断亲和性，决定哪个CPU处理该中断

```c
static int gic_set_affinity(struct irq_data *d, const struct cpumask *mask_val,
                            bool force)
{
    unsigned int cpu;
    u32 val, mask;

    if (!force)
        cpu = cpumask_any_and(mask_val, cpu_online_mask);
    else
        cpu = cpumask_first(mask_val);

    // ... 配置目标CPU
    writel_relaxed(val, base + GIC_DIST_TARGET + gic_irq(irq) * 4);
}
```

## 2.3 irqdomain 原理

### 2.3.1 域概念

**irqdomain（中断域）** 是Linux 2.6内核引入的概念，用于管理硬件中断号（hwirq）与Linux中断号（irq）之间的映射关系。在设备树（Device Tree）系统中，每个中断控制器节点对应一个irqdomain。

irqdomain的核心作用：
1. **动态分配Linux中断号**：从irqdomain的线性空间或树状空间中分配
2. **维护映射关系**：保存hwirq到irq的映射表
3. **支持级联**：处理多级中断控制器的嵌套

### 2.3.2 映射机制

irqdomain支持多种映射方式，通过`irq_domain_ops`中的回调函数实现：

```c
// include/linux/irqdomain.h
struct irq_domain_ops {
    int (*match)(struct irq_domain *d, struct device_node *node,
                 enum irq_domain_bus_token bus_token);
    int (*map)(struct irq_domain *d, unsigned int virq, irq_hw_number_t hwirq);
    void (*unmap)(struct irq_domain *d, unsigned int virq);
    int (*xlate)(struct irq_domain *d, struct device_node *node,
                 const u32 *intspec, unsigned int intsize,
                 unsigned long *out_hwirq, unsigned int *out_type);
};
```

**线性映射（Linear Domain）**：适用于中断数量固定的场景，如GIC的SPI中断

```c
// 创建线性映射irqdomain
struct irq_domain *irq_domain_add_linear(struct device_node *of_node,
                                          unsigned int size,
                                          const struct irq_domain_ops *ops,
                                          void *host_data);

// 示例：GIC驱动的域创建
static int gic_irq_domain_map(struct irq_domain *d, unsigned int irq,
                               irq_hw_number_t hwirq)
{
    struct irq_chip *chip = d->host_data;

    if (hwirq < 32) {
        // SGI和PPI
        irq_set_percpu_devid(irq);
        irq_domain_set_info(d, irq, hwirq, chip, d->host_data,
                            handle_percpu_irq, NULL, NULL);
    } else {
        // SPI
        irq_domain_set_info(d, irq, hwirq, chip, d->host_data,
                            handle_fasteoi_irq, NULL, NULL);
    }

    return 0;
}
```

**树状映射（Tree Domain）**：适用于中断号不连续或数量较大的场景

```c
// 创建树状映射irqdomain
struct irq_domain *irq_domain_add_tree(struct device_node *of_node,
                                        const struct irq_domain_ops *ops,
                                        void *host_data);

// 使用radix树管理hwirq到virq的映射
static unsigned int irq_find_mapping(struct irq_domain *domain,
                                      irq_hw_number_t hwirq)
{
    struct radix_tree_iter iter;
    void **slot;

    if (domain == NULL)
        return 0;

    radix_tree_for_each_slot(slot, &domain->revmap_tree, &iter, 0) {
        if (iter.index == hwirq)
            return (unsigned int)(long)*slot;
    }

    return 0;
}
```

**重映射域（Re-mapping Domain）**：用于需要动态映射的场景，如PCIe MSI中断

```c
// 创建重映射域
struct irq_domain *irq_domain_create_simple(struct fwnode_handle *fwnode,
                                              unsigned int size,
                                              unsigned int first_irq,
                                              const struct irq_domain_ops *ops,
                                              void *host_data);
```

### 2.3.3 层次结构

在ARM64系统中，irqdomain呈现多级层次结构：

```
+------------------+
|   Root Domain    |  (GIC - 硬件中断号0-1019)
+------------------+
         |
         v (级联)
+------------------+
|  Secondary PIC   |  (如嵌入式控制器)
+------------------+
         |
         v (级联)
+------------------+
|   GPIO Domain    |  (GPIO引脚中断)
+------------------+
```

**层次结构示例**：

```c
// GIC作为根domain
gic_domain = irq_domain_add_linear(NULL, 1020, &gic_irq_domain_ops, gic);

// 级联GPIO控制器
gpio_domain = irq_domain_add_tree(gpio_node, &gpio_irq_domain_ops, gpio_chip);

// GPIO域映射：hwirq 0-31 -> virq动态分配
// 通过父domain的xlate解析设备树的interrupts属性
```

## 2.4 中断号映射

### 2.4.1 映射流程

从硬件中断到Linux中断的完整映射流程如下：

```
硬件中断发生
    |
    v
GIC硬件中断号(hwirq)
    |
    v
通过hwirq查找对应irq_domain
    |
    v
调用irq_domain->ops->xlate()解析设备树
    |
    v
irq_domain->ops->map()创建映射
    |
    v
返回Linux中断号(irq/virq)
    |
    v
注册irqaction到irq_desc
```

### 2.4.2 设备树解析

设备树中通过`interrupts`和`interrupt-parent`属性描述外设中断：

```dts
// 设备树示例
&uart0 {
    compatible = "ns16550a";
    reg = <0x01c28000 0x100>;
    interrupts = <GIC_SPI 65 IRQ_TYPE_LEVEL_HIGH>;
    interrupt-parent = <&gic>;
};
```

解析过程：

```c
// kernel/irq/irqdomain.c
static int irq_domain_xlate_onecell(struct irq_domain *d,
                                     struct device_node *node,
                                     const u32 *intspec, unsigned int intsize,
                                     irq_hw_number_t *out_hwirq,
                                     unsigned int *out_type)
{
    if (WARN_ON(intsize < 1))
        return -EINVAL;

    *out_hwirq = intspec[0];  // 硬件中断号
    if (intsize > 1)
        *out_type = intspec[1] & IRQ_TYPE_SENSE_MASK;
    else
        *out_type = IRQ_TYPE_NONE;

    return 0;
}
```

### 2.4.3 动态分配

对于需要动态分配Linux中断号的设备，使用`irq_create_mapping()`函数：

```c
// kernel/irq/irqdomain.c
unsigned int irq_create_mapping(struct irq_domain *domain,
                                irq_hw_number_t hwirq)
{
    unsigned int virq;

    /* 检查是否已有映射 */
    virq = irq_find_mapping(domain, hwirq);
    if (virq) {
        return virq;
    }

    /* 分配新的Linux中断号 */
    virq = irq_domain_alloc_descs(1, hwirq, 0, NUMA_NO_NODE, NULL);
    if (virq == 0)
        return 0;

    /* 创建映射 */
    if (irq_domain_associate(domain, virq, hwirq)) {
        irq_free_descs(virq, 1);
        return 0;
    }

    return virq;
}
```

### 2.4.4 中断请求与释放

驱动使用`request_irq()`和`free_irq()`管理中断：

```c
// 请求中断
int request_irq(unsigned int irq, irq_handler_t handler,
               unsigned long flags, const char *name, void *dev);

// 释放中断
void free_irq(unsigned int irq, void *dev_id);

// 使用示例
static int my_driver_probe(struct platform_device *pdev)
{
    int ret;
    unsigned int irq;

    /* 获取中断号（从设备树或资源）*/
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;

    /* 注册中断处理函数 */
    ret = request_irq(irq, my_irq_handler,
                      IRQF_SHARED | IRQF_ONESHOT,
                      "my_driver", my_device);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
        return ret;
    }

    return 0;
}

static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    if (!dev)
        return IRQ_NONE;

    /* 处理中断... */
    return IRQ_HANDLED;
}
```

---

## 本章面试题

### 1. 描述irq_desc结构体的主要成员及其作用

**参考答案**：`irq_desc`是每个Linux中断号对应的描述符，主要成员包括：`irq_data`（封装硬件相关信息如hwirq和chip）、`handle_irq`（中断流处理函数，根据触发类型选择不同策略）、`action`（irqaction链表，保存所有注册的处理函数）、`thread`（线程化中断的内核线程）、`status_use_accessors`（中断状态标志）、`depth`（中断嵌套深度）。

### 2. irq_chip的作用是什么？列举几个关键的回调函数

**参考答案**：`irq_chip`是Linux内核对中断控制器的抽象接口，使内核能够统一管理不同类型的中断控制器。关键回调包括：`irq_mask/unmask`（屏蔽/解除屏蔽）、`irq_enable/disable`（使能/禁用）、`irq_ack`（响应中断）、`irq_eoi`（发送结束中断信号）、`irq_set_type`（设置触发类型）、`irq_set_affinity`（设置CPU亲和性）。

### 3. 什么是irqdomain？它的主要作用是什么？

**参考答案**：irqdomain是Linux 2.6引入的中断号管理机制，用于管理硬件中断号（hwirq）与Linux中断号（irq）之间的映射关系。主要作用包括：动态分配Linux中断号、维护映射表、支持多级中断控制器的级联。irqdomain支持线性映射（hwirq连续）、树状映射（hwirq不连续）和重映射三种方式。

### 4. 描述从硬件中断发生到Linux中断处理函数被调用的完整流程

**参考答案**：流程为：外设触发中断 -> GIC接收并确定硬件中断号(hwirq) -> 通过hwirq查找对应irq_domain -> 调用domain的map()创建映射 -> 返回Linux中断号(virq) -> 根据中断号找到对应irq_desc -> 调用handle_irq流处理函数 -> 遍历action链表调用各irqaction的handler。

### 5. 在设备树中如何描述一个外设的中断？

**参考答案**：设备树中使用`interrupts`属性描述外设中断，使用`interrupt-parent`指定父中断控制器。格式为`<hwirq trigger_type>`，如`interrupts = <GIC_SPI 65 IRQ_TYPE_LEVEL_HIGH>`表示使用GIC的SPI中断65号，触发类型为高电平触发。

### 6. 什么是中断级联？irqdomain如何处理多级中断控制器？

**参考答案**：中断级联是指一个中断控制器作为另一个中断控制器的"外设"，例如GPIO控制器作为GIC的下级。每个中断控制器对应一个irqdomain，形成树状层次结构。当上级中断发生时，通过父domain解析并映射到下级domain的hwirq，最终找到实际的外设中断。

### 7. request_irq和devm_request_irq有什么区别？

**参考答案**：`request_irq`需要手动调用`free_irq`释放中断，适用于常规内存分配的中断处理场景。`devm_request_irq`是资源管理版本的函数，中断会在设备卸载时自动释放，无需手动调用free_irq，使用更方便但会占用设备生命周期内的中断资源。

### 8. Linux中断的handle_irq有哪些类型？分别适用于什么场景？

**参考答案**：主要类型包括：`handle_edge_irq`（边沿触发）、`handle_level_irq`（电平触发）、`handle_simple_irq`（无状态中断）、`handle_percpu_irq`（每CPU中断，如定时器）、`handle_fasteoi_irq`（使用快速EOI模式，GIC常用）。选择依据是硬件中断控制器的触发类型和处理模式。

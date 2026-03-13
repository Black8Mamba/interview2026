# 第5章 GIC中断控制器

本章将深入讲解ARM架构中最重要的中断控制器——GIC（Generic Interrupt Controller）。GIC是ARM公司设计的一种通用中断控制器，几乎所有现代ARM处理器都采用GIC来管理中断。作为Linux中断子系统的核心硬件基础，理解GIC的架构和编程接口对于驱动开发者至关重要。

## 5.1 GIC架构概述

### 5.1.1 GIC版本演进

GIC经历了多个版本的演进，不同版本在功能和架构上有显著差异：

**GICv1**：最初版本，仅支持基本的中断分发功能，已较少使用。

**GICv2**：在GICv1基础上增加了多项改进，支持最多8个CPU核心，定义了SGI、PPI、SPI三种中断类型。GICv2仍然采用传统的中断分发模型，所有中断通过单一的Distributor和CPU Interface进行管理。

**GICv3**：这是目前主流的版本，引入了革命性的变化。首先，GICv3支持更多的CPU核心（最多16个或更多），引入了Redistributor组件来分散处理器的负载。GICv3还引入了LPI（Locality-specific Peripheral Interrupts）支持，这是一种基于消息的中断机制，允许外设直接发送中断而不需要物理中断线。最重要的变化是，GICv3支持消息信号中断（MSI），可以通过内存写入触发中断，这对PCIe等现代总线非常重要。

**GICv4**：作为GICv3的增强版本，GICv4进一步优化了虚拟化支持。它允许hypervisor直接分发虚拟中断到虚拟机，减少了VM-Exit次数，提高了虚拟化性能。GICv4还增加了对虚拟LPI的支持。

```
+-------------------+     +-------------------+
|   GICv2 架构      |     |   GICv3/v4 架构   |
+-------------------+     +-------------------+
|   Distributor     |     |   Distributor     |
|   (全局)          |     |   (全局)          |
+--------+----------+     +--------+----------+
         |                       |
+--------+----------+     +------+-------+------+
|  CPU Interface    |     | Redistributor | ...
|  (每个CPU)        |     | (每个CPU)     |
+-------------------+     +---------------+
                                   |
                          +--------+--------+
                          |  CPU Interface  |
                          |  (每个CPU)       |
                          +------------------+
```

### 5.1.2 GIC核心组件

GIC中断控制器由多个核心组件构成，理解这些组件的职责对于掌握GIC至关重要：

**Distributor（分发器）**：这是GIC的中央枢纽，负责接收所有中断源的中断请求，根据中断优先级和目标CPU配置，将中断分发到相应的CPU Interface。Distributor还负责中断的使能/禁用、优先级排序和中断状态管理。

**CPU Interface（CPU接口）**：每个CPU核心都有一个独立的CPU Interface，它连接CPU核心和Distributor。CPU Interface负责向CPU核心发送中断请求信号（IRQ或FIQ），从CPU接收中断完成信号，并提供中断优先级掩码等功能。

**Redistributor（重新分发器）**：这是GICv3引入的新组件，位于Distributor和CPU Interface之间。Redistributor负责管理每个CPU核心特有的中断（SGI和PPI）以及LPI中断。通过引入Redistributor，GICv3实现了更好的可扩展性和负载分布。

**ITS（Interrupt Translation Service）**：GICv3引入的组件，专门用于处理LPI中断和MSI。ITS将设备发送的MSI翻译成GIC可以处理的LPI中断，并负责中断路由和优先级映射。

### 5.1.3 寄存器访问方式

GIC的寄存器访问根据版本和配置有所不同：

GICv2采用内存映射IO（MMIO）方式访问，所有寄存器都映射到固定的物理地址空间。Linux内核通过读取设备树中的"reg"属性来获取这些地址。

GICv3支持两种访问模式：传统模式仍然使用MMIO访问Distributor和CPU Interface；而Redistributor和ITS则通过系统寄存器（ICC_*_EL1、ICC_*_EL2、ICC_*_EL3）访问，这种方式在AArch64架构下更高效。

```c
// GICv2寄存器地址定义示例
#define GIC_DIST_BASE       0xFEE00000  // Distributor基地址
#define GIC_CPU_BASE        0xFEE01000  // CPU Interface基地址

// GICv3系统寄存器访问（ARM64）
#define ICC_IAR0_EL1        sys_reg(3, 0, 12, 12, 0)   // 中断应答寄存器
#define ICC_EOIR0_EL1       sys_reg(3, 0, 12, 12, 1)   // 中断结束寄存器
#define ICC_PMR_EL1         sys_reg(3, 0, 4, 6, 0)     // 优先级掩码寄存器
#define ICC_CTLR_EL1        sys_reg(3, 0, 12, 12, 4)   // 控制寄存器
```

### 5.1.4 GIC中断分组机制

ARM GIC支持中断分组机制，这是实现虚拟化和安全隔离的重要基础。在支持TrustZone安全扩展的ARM处理器上，GIC可以将中断分为不同的安全组：

**Group 0中断**：最高优先级中断，通常用于安全世界的固件或hypervisor。在ARM安全模型中，Group 0中断只能在安全状态（Secure World）下处理。

**Group 1中断**：分为安全（Secure）和非安全（Non-Secure）两个子组。Linux内核运行在非安全世界，处理Group 1非安全中断。

```c
/* GIC中断分组配置 */
struct gic_prio_bits {
    u8 prio_bits;      // 优先级位数
    u8 prio_mask;     // 优先级掩码
};

static const struct gic_prio_bits gic_prio_bits[] = {
    [0] = { .prio_bits = 8, .prio_mask = 0xff },
    [1] = { .prio_bits = 7, .prio_mask = 0x7f },
    [2] = { .prio_bits = 6, .prio_mask = 0x3f },
    /* ... 更多配置 ... */
};

/* 中断分组定义 */
enum gic_alliance_prio {
    GICD_INT_PRIORITY_MASKED     = 0x80,   // 屏蔽所有优先级
    GICD_INT_PRIORITY_QUIRKY    = 0x40,   // 特殊优先级
    GICD_INT_PRIORITY_HIGHEST   = 0x00,   // 最高优先级
    GICD_INT_PRIORITY_DEFAULT   = 0xa0,   // 默认优先级
};
```

### 5.1.5 GIC硬件实例

常见的GIC硬件实现包括：

**ARM GIC-400**：这是ARM官方推出的GICv2实现，广泛用于早期ARM64服务器和嵌入式平台。它支持最多8个CPU核心，提供GICv2接口。

**ARM GIC-500**：GICv3架构的商业实现，支持最多16个CPU核心，提供完整的GICv3特性，包括LPI支持和ITS。

**ARM GIC-600**：高性能GICv3实现，支持更多核心和更先进的中断虚拟化特性，是现代数据中心和高性能计算平台的主流选择。

**厂商定制实现**：如高通、华为海思等厂商基于ARM GIC架构的定制实现，通常在移动芯片中采用。

```c
/* 设备树中的GIC配置示例 */
gic: interrupt-controller@fee00000 {
    compatible = "arm,gic-400";
    #interrupt-cells = <3>;
    interrupt-controller;
    reg = <0x0 0xfee00000 0x0 0x10000>,   /* Distributor */
          <0x0 0xfee01000 0x0 0x20000>;   /* CPU Interface */
    interrupts = <GIC_PPI 9 IRQ_TYPE_LEVEL_HIGH>;
    phandle = <&gic>;
};

/* GICv3设备树配置 */
gic_its: msi-controller@fee20000 {
    compatible = "arm,gic-v3-its";
    msi-controller;
    reg = <0x0 0xfee20000 0x0 0x20000>;
    platform-msi-parent;
};

gic_v3: interrupt-controller@fee00000 {
    compatible = "arm,gic-v3";
    #interrupt-cells = <3>;
    interrupt-controller;
    reg = <0x0 0xfee00000 0x0 0x30000>,   /* Distributor */
          <0x0 0xfee40000 0x0 0x200000>,  /* Redistributor */
          <0x0 0xfee60000 0x0 0x100000>;  /* ITS */
    interrupts = <GIC_PPI 9 IRQ_TYPE_LEVEL_HIGH>;
};
```

## 5.2 Distributor分发器

Distributor是GIC的核心组件，理解其寄存器是进行GIC编程的基础。

### 5.2.1 GICD_CTLR - 控制寄存器

GICD_CTLR是Distributor的主控制寄存器，控制整个分发器的全局行为。

```c
// GICD_CTLR寄存器位定义
#define GICD_CTLR_ENABLE_G0     (1 << 0)   // 使能Group 0中断
#define GICD_CTLR_ENABLE_G1     (1 << 1)   // 使能Group 1中断（安全世界）
#define GICD_CTLR_ENABLE_G1NS   (1 << 2)   // 使能Group 1非安全中断
#define GICD_CTLR_ARE_S         (1 << 4)   // 亲和性路由使能（安全）
#define GICD_CTLR_ARE_NS        (1 << 5)   // 亲和性路由使能（非安全）
#define GICD_CTLR_DS            (1 << 6)   // 禁用安全分组
#define GICD_CTLR_E1NWF         (1 << 7)   // 使能非等待中断写入

// 配置GICD_CTLR
void gic_dist_config(void __iomem *base)
{
    u32 ctlr = readl_relaxed(base + GICD_CTLR);

    /* GICv2配置 */
    ctlr |= GICD_CTLR_ENABLE_G0;
    #ifdef CONFIG_ARM_GIC_V3
    ctlr |= GICD_CTLR_ENABLE_G1NS;
    #endif

    writel_relaxed(ctlr, base + GICD_CTLR);
}
```

### 5.2.2 GICD_ISENABLER - 中断使能寄存器

GICD_ISENABLERn寄存器用于使能中断。每个中断对应一个比特位，置1表示使能该中断。

```c
// 中断使能寄存器偏移计算
// GICD_ISENABLERn = GICD_ISENABLER + (中断号 / 32) * 4
#define GICD_ISENABLER         0x100
#define GICD_ISENABLERn(n)     (GICD_ISENABLER + ((n) / 32) * 4)

// 使能指定中断
void gic_enable_irq(void __iomem *base, unsigned int irq)
{
    unsigned int n = irq / 32;
    unsigned int mask = 1 << (irq % 32);
    u32 enable = readl_relaxed(base + GICD_ISENABLERn(n));

    enable |= mask;
    writel_relaxed(enable, base + GICD_ISENABLERn(n));
}

// 对应的禁用寄存器 GICD_ICENABLER
#define GICD_ICENABLER         0x180
#define GICD_ICENABLERn(n)     (GICD_ICENABLER + ((n) / 32) * 4)

void gic_disable_irq(void __iomem *base, unsigned int irq)
{
    unsigned int n = irq / 32;
    unsigned int mask = 1 << (irq % 32);
    u32 disable = readl_relaxed(base + GICD_ICENABLERn(n));

    disable |= mask;
    writel_relaxed(disable, base + GICD_ICENABLERn(n));
}
```

### 5.2.3 GICD_IPRIORITYR - 中断优先级寄存器

GICD_IPRIORITYRn寄存器设置每个中断的优先级。优先级值越低，优先级越高。

```c
// 优先级寄存器偏移计算（每个中断占8位或更多，取决于实现）
// GICD_IPRIORITYRn = GICD_IPRIORITYR + 中断号 * 优先级字节数
#define GICD_IPRIORITYR        0x400

// GICv2通常每中断占用8位
#define GICD_IPRIORITYRn(n)    (GICD_IPRIORITYR + (n) * 4)

// 设置中断优先级
void gic_set_priority(void __iomem *base, unsigned int irq, unsigned int priority)
{
    /* GICv2: 8位优先级，每中断4字节 */
    writel_relaxed(priority, base + GICD_IPRIORITYRn(irq));
}

// 读取中断优先级
unsigned int gic_get_priority(void __iomem *base, unsigned int irq)
{
    return readl_relaxed(base + GICD_IPRIORITYRn(irq));
}
```

**GICv2优先级特性**：
- 优先级字段宽度通常为8位（0-255）
- 某些实现只使用高5位或高6位
- 优先级数值越小，优先级越高
- 某些优先级值可能被保留或用于特殊目的

### 5.2.4 GICD_ITARGETSR - 中断目标CPU寄存器

GICD_ITARGETSRn寄存器配置中断的目标CPU。在GICv2中，每个SPI中断可以指定发送到哪个CPU核心。

```c
// 中断目标CPU寄存器偏移
#define GICD_ITARGETSR         0x800

// GICD_ITARGETRn = GICD_ITARGETSR + 中断号 * 1（每个中断1字节）
#define GICD_ITARGETSRn(n)     (GICD_ITARGETSR + (n))

/* CPU位掩码定义 */
#define CPU_MASK_CPU0           0x01
#define CPU_MASK_CPU1           0x02
#define CPU_MASK_CPU2           0x04
#define CPU_MASK_CPU3           0x08
#define CPU_MASK_CPU4           0x10
#define CPU_MASK_CPU5           0x20
#define CPU_MASK_CPU6           0x40
#define CPU_MASK_CPU7           0x80

// 设置中断目标CPU
void gic_set_target_cpu(void __iomem *base, unsigned int irq, unsigned int cpu_mask)
{
    /* 仅对SPI有效（中断号 >= 32） */
    if (irq < 32)
        return;

    writel_relaxed(cpu_mask, base + GICD_ITARGETSRn(irq));
}
```

**注意**：SGI和PPI中断的目标CPU由硬件根据中断号隐式确定，不能通过ITARGETSR配置。

### 5.2.5 GICD_ICFGR - 中断配置寄存器

GICD_ICFGRn寄存器配置中断的触发类型和是否使用电平敏感模式。

```c
// 中断配置寄存器
#define GICD_ICFGR             0xC00

// GICD_ICFGRn = GICD_ICFGR + (中断号 / 16) * 4
#define GICD_ICFGRn(n)         (GICD_ICFGR + ((n) / 16) * 4)

/* 配置位含义（每中断2位）：
 * Bit[1]: 1=电平敏感，0=边沿触发
 * Bit[0]: 1=1-N模型，0=N-N模型
 */
#define GICD_ICFGR_LEVEL       (1 << 1)   // 电平触发
#define GICD_ICFGR_EDGE        (0 << 1)   // 边沿触发
#define GICD_ICFGR_1N          (1 << 0)   // 1-N模型（仅一个CPU处理）
#define GICD_ICFGR_NN          (0 << 0)   // N-N模型（所有监听CPU都处理）

// 配置中断触发类型
void gic_set_config(void __iomem *base, unsigned int irq, unsigned int config)
{
    unsigned int n = irq / 16;
    unsigned int shift = (irq % 16) * 2;
    u32 icfgr = readl_relaxed(base + GICD_ICFGRn(n));

    icfgr &= ~(0x3 << shift);
    icfgr |= (config & 0x3) << shift;
    writel_relaxed(icfgr, base + GICD_ICFGRn(n));
}
```

## 5.3 CPU Interface

CPU Interface是CPU核心与GIC之间的桥梁，负责中断信号传递给CPU和处理中断完成通知。

### 5.3.1 GICC_CTLR - CPU Interface控制寄存器

GICC_CTLR控制CPU Interface的行为，包括优先级掩码、抢占设置等。

```c
// GICv2 GICC_CTLR寄存器位定义
#define GICC_CTLR_ENABLE           (1 << 0)   // 使能CPU Interface
#define GICC_CTLR_EOImode_NS       (1 << 1)   // 非安全EOI模式
#define GICC_CTLR_EOImode_S        (1 << 2)   // 安全EOI模式
#define GICC_CTLR_PMR_PRI_BIT      (1 << 3)   // 优先级位

// GICv3 ICC_CTLR_EL1位定义
#define ICC_CTLR_EL1_ENABLE_G0     (1 << 0)   // 使能Group 0
#define ICC_CTLR_EL1_ENABLE_G1     (1 << 1)   // 使能Group 1
#define ICC_CTLR_EL1_EOImode       (1 << 2)   // EOI模式位

// 配置CPU Interface
void gic_cpu_config(void __iomem *base)
{
    u32 ctlr;

    /* 设置优先级掩码，允许所有优先级中断 */
    writel_relaxed(0xff, base + GICC_PMR);

    /* 配置控制寄存器 */
    ctlr = readl_relaxed(base + GICC_CTLR);
    ctlr |= GICC_CTLR_ENABLE;
    #ifdef CONFIG_ARM_GIC_V3
    ctlr |= GICC_CTLR_EOImode_NS;
    #endif
    writel_relaxed(ctlr, base + GICC_CTLR);
}
```

### 5.3.2 GICC_IAR - 中断应答寄存器

当CPU响应中断时，通过读取GICC_IAR（Interrupt Acknowledge Register）获取中断号。

```c
// GICC_IAR寄存器
#define GICC_IAR                  0x0C

// 读取中断应答寄存器
static inline u32 gic_read_iar(void __iomem *cpu_base)
{
    return readl_relaxed(cpu_base + GICC_IAR);
}

/* 返回值解析：
 * [31:24] = 保留
 * [23:13] = CPUID（对于SGI）
 * [12:10] = 保留
 * [9:0]   = 中断号（0-1019 for SPI/PPI/SGI）
 *
 * 特殊值：
 * 1020 (0x3FC) = 无有效中断（spurious interrupt）
 */

// GICv3系统寄存器方式
static inline u64 gicv3_read_iar(void)
{
    return read_sysreg_s(ICC_IAR0_EL1);
}
```

### 5.3.3 GICC_EOIR - 中断结束寄存器

处理完中断后，CPU需要写入GICC_EOIR（End of Interrupt Register）来通知GIC当前中断处理完成。

```c
// GICC_EOIR寄存器
#define GICC_EOIR                0x10

// 写入EOI寄存器
static inline void gic_write_eoir(void __iomem *cpu_base, u32 eoir)
{
    writel_relaxed(eoir, cpu_base + GICC_EOIR);
}

/* EOI值通常就是之前读取的IAR值
 * 内核会保存中断号，处理完成后写入
 */

// GICv3系统寄存器方式
static inline void gicv3_write_eoir(u64 eoir)
{
    write_sysreg_s(eoir, ICC_EOIR0_EL1);
}

// 中断处理流程中的EOI
void handle_fasteoi_irq(struct irq_desc *desc)
{
    struct irq_chip *chip = desc->irq_data.chip;
    struct pt_regs *regs = get_irq_regs();
    u32 irqnr;

    /* 读取IAR获取中断号 */
    irqnr = chip->irq_read_retention_note(irq_data);
    if (irqnr >= 1020)
        return;  // spurious interrupt

    /* 处理中断 */
    handle_irq_event(regs);

    /* 发送EOI */
    chip->irq_eoi(&desc->irq_data);
}
```

### 5.3.4 优先级掩码寄存器PMR

GICC_PMR（Priority Mask Register）设置CPU的优先级阈值，低于该优先级的中断不会被此CPU接收。

```c
// GICC_PMR寄存器
#define GICC_PMR                 0x04

/* 优先级掩码示例：只接受优先级高于（数值小于）0xF0的中断 */
writel_relaxed(0xF0, cpu_base + GICC_PMR);

/* 实际使用场景：
 * 1. 内核启动时设置为0xFF（接受所有中断）
 * 2. 在中断处理关键路径中可能临时提高阈值
 * 3. 线程化中断中使用系统调用设置掩码
 */

// GICv3系统寄存器
static inline void gicv3_set_pmr(u64 pmr)
{
    write_sysreg_s(pmr, ICC_PMR_EL1);
}

static inline u64 gicv3_get_pmr(void)
{
    return read_sysreg_s(ICC_PMR_EL1);
}
```

### 5.3.5 二进制点寄存器BPR

二进制点寄存器（Binary Point Register）控制中断优先级的分组，用于决定中断抢占行为。

```c
// GICC_BPR寄存器
#define GICC_BPR                 0x08

/* BPR值含义：
 * BPR = 0: 不分组，优先级直接比较
 * BPR = 1: 高7位为组优先级，低1位为子优先级
 * BPR = 2: 高6位为组优先级，低2位为子优先级
 * ...
 *
 * 组优先级用于判断中断抢占
 * 子优先级在同组内决定处理顺序
 */

/* GICv3 BPR */
#define ICC_BPR0_EL1             sys_reg(3, 0, 12, 8, 0)
#define ICC_BPR1_EL1             sys_reg(3, 0, 12, 14, 0)

static void gic_set_bpr(struct gic_chip_data *gic, int cpu)
{
    void __iomem *cpu_base = per_cpu_ptr(gic->cpu_base, cpu);

    /* 设置BPR，使组优先级足够高以支持抢占 */
    writel_relaxed(4, cpu_base + GICC_BPR);
}
```

### 5.3.6 运行状态寄存器

GIC提供中断运行状态寄存器来追踪中断处理状态。

```c
// 中断运行状态寄存器
#define GICC_RPR                 0x0C    // 正在运行的中断优先级

// GICv3: ICC_RPR_EL1

// 获取当前正在运行的中断优先级
static inline u32 gic_read_rpr(void __iomem *cpu_base)
{
    return readl_relaxed(cpu_base + GICC_RPR);
}

// GICv3运行优先级解释
static inline u64 gicv3_read_rpr(void)
{
    return read_sysreg_s(ICC_RPR_EL1);
}

/* 运行优先级说明：
 * - 如果无中断运行，返回0xFF（GICv2）或0（GICv3）
 * - 数值越低表示优先级越高
 * - 可以用于判断当前CPU是否在处理中断
 */
```

## 5.4 中断类型

GIC定义了四种中断类型，每种类型有不同的特性和用途。

### 5.4.1 SGI（Software Generated Interrupt）

SGI是软件生成的中断，主要用于CPU之间的通信。

- **中断号**：0-15（共16个）
- **触发方式**：通过向GICD_SGIR寄存器写入来触发
- **目标**：可以指定一个或多个CPU作为目标

```c
// GICD_SGIR寄存器
#define GICD_SGIR                0x0F00

/* SGIR位定义：
 * [31:24] = 目标列表过滤器
 *          0b00 = 使用目标列表
 *          0b01 = 所有其他CPU
 *          0b10 = 仅当前CPU
 *          0b11 = 保留
 * [23:16] = 保留
 * [15:0]  = 目标CPUs位图
 */

// 触发SGI中断
void gic_send_sgi(void __iomem *dist_base, unsigned int sgi_id,
                  unsigned int cpu_mask)
{
    u32 sgi = (sgi_id & 0x0F) | (cpu_mask << 16);
    writel_relaxed(sgi, dist_base + GICD_SGIR);
}

/* 使用场景：
 * - CPU之间同步
 * - IPI（Inter-Processor Interrupt）实现
 * - 调度器唤醒其他CPU
 * - 热插拔通知
 */
```

### 5.4.2 PPI（Private Peripheral Interrupt）

PPI是每个CPU私有的外设中断，适用于需要每个CPU独立处理的中断。

- **中断号**：16-31（共16个）
- **特性**：中断号对于每个CPU是私有的
- **典型用途**：每CPU定时器、本地CPU计数器

```c
// 典型的PPI中断
#define TIMER_PPI                29    // 通用定时器中断
#define PMU_PPI                  30    // 性能监控单元中断

/* PPI配置示例 */
void gic_config_ppi(void __iomem *dist_base, unsigned int irq)
{
    /* PPI配置在GICD_ICFGR1中
     * 中断号16-31对应ICFGR1
     */
    u32 icfgr = readl_relaxed(dist_base + GICD_ICFGR1);

    /* 配置为边沿触发 */
    if (irq >= 16 && irq < 32) {
        unsigned int shift = ((irq - 16) % 16) * 2;
        icfgr &= ~(1 << (shift + 1));  // 边沿触发
        writel_relaxed(icfgr, dist_base + GICD_ICFGR1);
    }
}
```

### 5.4.3 SPI（Shared Peripheral Interrupt）

SPI是共享的外设中断，可以在多个CPU之间共享。

- **中断号**：32-1019（具体数量取决于实现，ARM GIC-400最多支持988个）
- **特性**：可配置目标CPU，使用ITARGETSR寄存器
- **典型用途**：外设中断，如UART、I2C、GPIO等

```c
// SPI中断配置
void gic_config_spi(void __iomem *dist_base, unsigned int irq,
                    unsigned int cpu_mask)
{
    /* 确保是SPI中断 */
    if (irq < 32 || irq > 1019)
        return;

    /* 配置目标CPU */
    writel_relaxed(cpu_mask, dist_base + GICD_ITARGETSRn(irq));

    /* 配置触发类型（电平或边沿） */
    /* ... */
}

/* 常见SPI中断号（设备树中定义）
 * 32-31 = SPI起始号
 */
```

### 5.4.4 LPI（Locality-specific Peripheral Interrupt）

LPI是GICv3引入的新中断类型，基于消息信号机制。

- **起始中断号**：8192（0x2000）
- **特性**：基于消息的中断，无需物理中断线
- **配置方式**：通过ITS（Interrupt Translation Service）配置
- **典型用途**：PCIeMSI、Message Signaled Interrupts

```c
// LPI配置需要通过ITS
// LPI不使用传统的enable/set位，而是通过ITS命令配置

/* LPI特性：
 * 1. 基于内存的配置表（Properties）
 * 2. 需要GICv3+硬件支持
 * 3. 支持更多中断号（理论上无限制）
 * 4. 更灵活的路由机制
 */

/* LPI配置在Linux中的实现 */
struct its_device {
    struct list_head entry;
    u32 device_id;
    struct irq_domain *domain;
    int nr_ites;
    struct its_ite *table;
};

int its_map_device(struct its_device *its_dev, u32 device_id,
                   unsigned int nr_ites)
{
    /* 分配中断目标表 */
    its_dev->table = kcalloc(nr_ites, sizeof(struct its_ite),
                             GFP_KERNEL);

    /* 配置ITS设备映射 */
    /* ... */
    return 0;
}
```

### 5.4.5 中断的生命周期

理解中断的生命周期对于正确使用GIC至关重要。每个中断从产生到完成经历多个状态：

**无效状态（Inactive）**：中断未激活或已完成处理。

**挂起状态（Pending）**：中断已产生但CPU尚未响应。

**激活状态（Active）**：CPU正在处理该中断。

**激活并挂起（Active and Pending）**：中断正在处理，同时又收到新的中断请求。

```c
/* 中断状态查询 */
static u32 gic_get_interrupt_status(void __iomem *dist_base, unsigned int irq)
{
    /* GICD_ISPENDR - 挂起状态 */
    u32 pend = readl_relaxed(dist_base + GICD_ISPENDRn(irq / 32));
    u32 pend_bit = 1 << (irq % 32);

    /* GICD_ISACTIVER - 激活状态 */
    u32 act = readl_relaxed(dist_base + GICD_ISACTIVERn(irq / 32));
    u32 act_bit = 1 << (irq % 32);

    /* 组合状态 */
    if (act & act_bit)
        return GIC_IRQ_STATUS_ACTIVE;
    if (pend & pend_bit)
        return GIC_IRQ_STATUS_PENDING;
    return GIC_IRQ_STATUS_INACTIVE;
}

#define GIC_IRQ_STATUS_INACTIVE    0
#define GIC_IRQ_STATUS_PENDING    1
#define GIC_IRQ_STATUS_ACTIVE     2
```

### 5.4.6 特殊中断号

GIC保留了一些特殊的中断号用于特定目的：

**Spurious中断（1020）**：当CPU读取IAR但没有有效中断时返回1020。这通常发生在中断信号抖动或硬件错误时。驱动程序和内核应忽略此中断号。

**伪中断处理**：

```c
/* 处理伪中断 */
static void handle_spurious_interrupt(unsigned int irq)
{
    /* 伪中断不应触发任何处理 */
    /* 记录统计信息 */
    atomic_inc(&irq_stat[irq].spurious_count);

    /* 可以在调试时打印警告 */
    #ifdef CONFIG_DEBUG_SHIRQ
    printk_ratelimited("Spurious interrupt on IRQ %d\n", irq);
    #endif
}
```

## 5.5 中断优先级与亲和性

### 5.5.1 优先级系统

GIC的优先级系统决定了中断的处理顺序和抢占行为。

```c
/* 优先级配置示例
 * GICv2: 8位优先级（0-255）
 * GICv3: 可配置位数（通常8位）
 *
 * 重要概念：
 * - 优先级数值越小，优先级越高
 * - 某些优先级值可能被保留
 * - 可通过分组实现不同安全等级
 */

// 配置中断优先级
static void gic_set_irq_priority(struct irq_desc *desc, unsigned int prio)
{
    struct irq_chip *chip = desc->irq_data.chip;
    void __iomem *dist_base = chip->parent_data;
    unsigned int irq = desc->irq_data.hwirq;

    if (prio > 255)
        prio = 255;

    writel_relaxed(prio, dist_base + GICD_IPRIORITYRn(irq));
}

/* 优先级分组（Priority Grouping）
 * 将优先级字段分为两部分：
 * - 组优先级：用于抢占判断
 * - 子优先级：同组内区分顺序
 *
 * 通过GICD_PGCR配置
 */
```

### 5.5.2 亲和性配置

中断亲和性决定了中断可以被哪些CPU处理。

```c
/* 设置中断亲和性（用户空间接口） */
int irq_set_affinity(unsigned int irq, const struct cpumask *mask)
{
    struct irq_desc *desc = irq_to_desc(irq);
    unsigned long flags;
    int ret;

    raw_spin_lock_irqsave(&desc->lock, flags);

    /* 检查是否支持亲和性设置 */
    if (!desc->irq_data.chip->irq_set_affinity) {
        ret = -EINVAL;
        goto out;
    }

    /* 调用chip回调 */
    ret = desc->irq_data.chip->irq_set_affinity(&desc->irq_data,
                                                 mask, false);
out:
    raw_spin_unlock_irqrestore(&desc->lock, flags);
    return ret;
}

/* 内核API（从进程上下文调用） */
static int gic_set_affinity(struct irq_data *d, const struct cpumask *mask_val,
                            bool force)
{
    void __iomem *dist_base = d->chip_data;
    unsigned int cpu = cpumask_first(mask_val);

    /* GICv2: 写入ITARGETSR */
    if (d->hwirq >= 32 && d->hwirq < 1020) {
        writel_relaxed(1 << cpu, dist_base + GICD_ITARGETSRn(d->hwirq));
    }

    irq_data_update_effective_affinity(d, cpumask_of(cpu));
    return IRQ_SET_MASK_OK;
}
```

### 5.5.3 IRQ Affinity在实际中的应用

```c
/* 用户空间设置中断亲和性
 * # echo 1 > /proc/irq/<irq_num>/smp_affinity
 * # echo 0-3 > /proc/irq/<irq_num>/smp_affinity_list
 */

/* 在驱动中请求中断并设置亲和性 */
int dev_driver_init(struct platform_device *dev)
{
    int irq = platform_get_irq(dev, 0);
    int ret;

    /* 请求中断 */
    ret = devm_request_threaded_irq(&dev->dev, irq, handler, thread_fn,
                                    IRQF_ONESHOT, dev_name(&dev->dev), data);
    if (ret)
        return ret;

    /* 设置亲和性（让中断在CPU0和CPU1上处理） */
    cpumask_t mask;
    cpumask_or(&mask, cpu_present_mask, NULL);
    ret = irq_set_affinity(irq, &mask);

    return ret;
}

/* NUMA亲和性
 * 中断应该路由到访问设备内存最快的CPU
 */
void set_numa_affinity(struct irq_desc *desc, struct device *dev)
{
    int node = dev_to_node(dev);
    cpumask_t mask;

    cpumask_of_node(&mask, node);
    irq_set_affinity(desc->irq_data.irq, &mask);
}
```

### 5.5.4 中断负载均衡

Linux内核提供了中断负载均衡机制，可以自动将中断分散到多个CPU上处理。

```c
/* 软中断负载均衡 */
static int irq_balance_move_one(int this_cpu, int cpu,
                               struct irq_desc *desc)
{
    struct irq_data *data = &desc->irq_data;
    unsigned long失衡阈值 = 10000;
    long imbalance;

    /* 计算当前CPU和目标CPU的负载差异 */
    imbalance = data->this_affinity_saturated -失衡阈值;
    if (imbalance <= 0)
        return 0;

    /* 尝试移动中断到负载较低的CPU */
    if (can_move_irq(data, cpu)) {
        data->affinity = cpumask_of(cpu);
        return 1;
    }

    return 0;
}

/* IRQ域负载均衡核心函数 */
void irq_do_affinity(struct irq_desc *desc, const struct cpumask *mask)
{
    struct irq_chip *chip = desc->irq_data.chip;

    /* 更新亲和性掩码 */
    cpumask_and(desc->irq_data.affinity, mask, cpu_online_mask);

    /* 重新配置硬件 */
    if (chip->irq_set_affinity)
        chip->irq_set_affinity(&desc->irq_data,
                               desc->irq_data.affinity, true);
}
```

### 5.5.5 实时性考虑

在实时系统中，中断优先级的配置直接影响系统的响应延迟。

```c
/* 实时系统的优先级配置 */
void realtime_irq_config(void)
{
    /* 设置高优先级定时器中断 */
    struct sched_param param = {
        .sched_priority = MAX_RT_PRIO - 1
    };

    /* 确保定时器中断具有最高优先级 */
    irq_set_priority(TIMER_IRQ, 0);  // 最高优先级

    /* 为实时任务分配专用CPU */
    cpumask_t rt_cpus;
    cpumask_parse("0-1", rt_cpus);  // CPU 0和1用于实时任务

    /* 设置IRQ亲和性到非实时CPU */
    cpumask_t non_rt_cpus;
    cpumask_complement(non_rt_cpus, rt_cpus);
    irq_set_affinity(IRQ_HOTPLUG, non_rt_cpus);
}
```

## 5.6 Linux内核GIC驱动实现

### 5.6.1 GIC驱动初始化

Linux内核的GIC驱动位于`drivers/irqchip/irq-gic.c`（GICv2）和`drivers/irqchip/irq-gic-v3.c`（GICv3）。

```c
// drivers/irqchip/irq-gic.c - GICv2驱动

static int __init gic_init_bases(struct gic_chip_data *gic,
                                 unsigned int irq_start,
                                 const char *name)
{
    u32 cpu;
    void __iomem *dist_base;
    void __iomem *cpu_base;

    /* 映射寄存器地址 */
    dist_base = ioremap(gic->dist_base, gic->dist_size);
    cpu_base = ioremap(gic->cpu_base, gic->cpu_size);

    gic->dist_base = dist_base;
    gic->cpu_base = cpu_base;

    /* 初始化Distributor */
    for (cpu = 0; cpu < NR_GIC_CPU_IF; cpu++) {
        void __iomem *base = gic_data_dist_base(gic);

        /* 禁用所有中断 */
        writel_relaxed(0xFFFFFFFF, base + GICD_ICENABLER0);

        /* 设置所有中断优先级为0 */
        for (int i = 0; i < NR_IRQS; i += 4)
            writel_relaxed(0xFFFFFFFF, base + GICD_IPRIORITYR + i);
    }

    /* 配置Distributor */
    gic_dist_config(dist_base);

    /* 配置CPU Interface */
    gic_cpu_config(cpu_base);

    /* 创建irqdomain */
    gic->domain = irq_domain_add_linear(node, NR_IRQS,
                                         &gic_irq_domain_ops,
                                         gic);

    /* 分配Linux中断号 */
    irq_set_probe_handler(irq_start + 29, handle_fasteoi_irq);

    pr_info("GIC: %s initialized\n", name);
    return 0;
}
```

### 5.6.2 GIC中断芯片描述

```c
// GIC芯片数据结构
struct gic_chip_data {
    union {
        struct gic_chip_data *single;
        struct {
            void __iomem *dist_base;
            void __iomem *cpu_base;
            u32 saved_spi_enable[DIV_ROUND_UP(1020, 32)];
            u32 saved_spi_conf[DIV_ROUND_UP(1020, 16)];
            u32 saved_spi_target[1020 / 4];
            u32 saved_spi_priority[1020 / 4];
        };
    };
    struct irq_domain *domain;
    u32 nr_irqs;
    u8 wakeup_irqs;
    u8 supports_internal_probe;
};

// 全局GIC数据
static struct gic_chip_data gic_data __read_mostly;

// GIC中断芯片操作
static struct irq_chip gic_chip = {
    .name                   = "GIC",
    .irq_mask               = gic_mask_irq,
    .irq_unmask             = gic_unmask_irq,
    .irq_eoi                = gic_eoi_irq,
    .irq_set_type           = gic_set_type,
    .irq_retrigger          = gic_retrigger,
    .irq_set_affinity       = gic_set_affinity,
    .irq_set_wake           = gic_set_wake,
    .irq_read_retention_note = gic_read_irq,
#ifdef CONFIG_SMP
    .flags                  = IRQCHIP_SKIP_SET_WAKE | IRQCHIP_MASK_ON_SUSPEND,
#else
    .flags                  = IRQCHIP_MASK_ON_SUSPEND,
#endif
};
```

### 5.6.3 中断屏蔽与使能

```c
// 屏蔽中断
static void gic_mask_irq(struct irq_data *d)
{
    void __iomem *base = gic_data_dist_base(&gic_data);
    unsigned int irq = d->hwirq;

    if (irq < 32) {
        /* PPI和SGI：使用ICENABLER */
        writel_relaxed(1 << irq, base + GICD_ICENABLER0);
    } else {
        /* SPI：使用ICENABLERn */
        writel_relaxed(1 << (irq % 32),
                       base + GICD_ICENABLERn(irq / 32));
    }
}

// 使能中断
static void gic_unmask_irq(struct irq_data *d)
{
    void __iomem *base = gic_data_dist_base(&gic_data);
    unsigned int irq = d->hwirq;

    if (irq < 32) {
        writel_relaxed(1 << irq, base + GICD_ISENABLER0);
    } else {
        writel_relaxed(1 << (irq % 32),
                       base + GICD_ISENABLERn(irq / 32));
    }
}
```

### 5.6.4 EOI处理与handle_fasteoi_irq

```c
// EOI（End of Interrupt）处理
static void gic_eoi_irq(struct irq_data *d)
{
    void __iomem *cpu_base = gic_data_cpu_base(&gic_data);
    u32 hwirq = d->hwirq;

    /* 写入EOIR，关闭当前中断优先级 */
    writel_relaxed(hwirq, cpu_base + GICC_EOIR);
}

/* handle_fasteoi_irq - 快速EOI中断处理函数
 * 这是GIC中断的标准处理函数
 */
void handle_fasteoi_irq(struct irq_desc *desc)
{
    struct irq_chip *chip = desc->irq_data.chip;
    struct pt_regs *regs;
    int ret;

    /* 检查是否嵌套 */
    if (handle_irq_event_prepare(desc))
        return;

    /* 增加中断统计 */
    kstat_incr_irqs_this_cpu(desc);

    /* 读取IAR，获取中断号 */
    ret = chip->irq_read_retention_note(irq_data);
    if (ret >= 1020)
        return;  // spurious interrupt

    /* 设置处理中状态 */
    handle_irq_event_prepare(desc);

    /* 执行中断处理 */
    handle_irq_event(regs);

    /* 发送EOI */
    chip->irq_eoi(&desc->irq_data);

    /* 检查是否需要重新调度 */
    handle_irq_event_finish(desc);
}
```

### 5.6.5 GICv3驱动特性

```c
// drivers/irqchip/irq-gic-v3.c - GICv3驱动

/* GICv3初始化 */
static int __init gic_init_dist(void)
{
    void __iomem *rdbase;

    /* 获取Redistributor基地址 */
    rdbase = gic_get_redist_base();

    /* 配置Redistributor */
    writel_relaxed(0, rdbase + GICR_WAKER);

    /* 使能Group 1非安全中断 */
    writel_relaxed(GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1NS,
                   gic_data.dist_base + GICD_CTLR);

    /* 配置CPU Interface（通过系统寄存器） */
    gicv3_configure_cpu();

    return 0;
}

/* GICv3 CPU Interface配置 */
static void gicv3_configure_cpu(void)
{
    u64 val;

    /* 设置优先级掩码 */
    val = read_sysreg_s(ICC_PMR_EL1);
    val = 0xFF;  // 允许所有优先级
    write_sysreg_s(val, ICC_PMR_EL1);

    /* 使能Group 1 */
    val = read_sysreg_s(ICC_CTLR_EL1);
    val |= ICC_CTLR_EL1_ENABLE_G1;
    val |= ICC_CTLR_EL1_EOImode;  // 降低优先级模式
    write_sysreg_s(val, ICC_CTLR_EL1);

    /* 设置VBAR_BRP和EOIBRP */
    write_sysreg_s(0, ICC_BPR0_EL1);
    write_sysreg_s(0, ICC_BPR1_EL1);
}

/* GICv3中断号范围 */
static int gic_irq_domain_translate(struct irq_domain *d,
                                     struct irq_fwspec *fwspec,
                                     unsigned long *hwirq,
                                     unsigned int *type)
{
    /* 支持两种格式：
     * 1. 单个cell: hwirq
     * 2. 两个cells: [0]=设备ID (MSI), [1]=eventID
     */
    if (fwspec->param_count == 1) {
        *hwirq = fwspec->param[0];
        *type = IRQ_TYPE_LEVEL_HIGH;
    } else if (fwspec->param_count == 2) {
        *hwirq = fwspec->param[1] + 8192;  // LPI起始号
        *type = IRQ_TYPE_EDGE_RISING;
    } else {
        return -EINVAL;
    }

    return 0;
}
```

### 5.6.7 GIC驱动中的中断处理流程

```c
/* GIC中断处理的完整流程 */
static void gic_handle_irq(struct pt_regs *regs)
{
    u32 irqstat;
    unsigned int irq;
    struct pt_regs *old_regs = set_irq_regs(regs);

    /* 重复读取直到没有更多中断 */
    do {
        /* 读取IAR获取中断号 */
        irqstat = gic_read_iar();
        irq = irqstat & 0x3FF;  // 取低10位

        /* 检查是否是spurious interrupt */
        if (irq >= 1020) {
            break;  // 无有效中断
        }

        /* 查找对应的Linux中断号 */
        struct irq_desc *desc = irq_to_desc(irq);
        if (!desc) {
            /* 未知中断，写入EOI并忽略 */
            gic_write_eoir(irqstat);
            continue;
        }

        /* 处理中断 */
        handle_irq(desc, regs);

    } while (1);

    /* 恢复寄存器 */
    set_irq_regs(old_regs);
}
```

### 5.6.8 中断挂起与激活状态管理

```c
/* 设置中断为挂起状态 */
static int gic_set_pending(struct irq_data *d)
{
    void __iomem *base = gic_data_dist_base(&gic_data);
    unsigned int irq = d->hwirq;

    if (irq < 32) {
        writel_relaxed(1 << irq, base + GICD_ISPENDR0);
    } else {
        writel_relaxed(1 << (irq % 32),
                       base + GICD_ISPENDRn(irq / 32));
    }

    return 0;
}

/* 清除激活状态 */
static int gic_clear_active(struct irq_data *d)
{
    void __iomem *base = gic_data_dist_base(&gic_data);
    unsigned int irq = d->hwirq;

    if (irq < 32) {
        writel_relaxed(1 << irq, base + GICD_ICPENDR0);
    } else {
        writel_relaxed(1 << (irq % 32),
                       base + GICD_ICPENDRn(irq / 32));
    }

    return 0;
}
```

### 5.6.9 中断类型配置函数

```c
/* 设置中断触发类型 */
static int gic_set_type(struct irq_data *d, unsigned int type)
{
    void __iomem *base = gic_data_dist_base(&gic_data);
    unsigned int irq = d->hwirq;
    u32 confreg;
    unsigned long flags;

    /* 只能是边沿或电平，不能同时 */
    if (type & IRQ_TYPE_SENSE_MASK) {
        if (type & (IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW))
            return -EINVAL;
        if (type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING))
            return -EINVAL;
    }

    raw_spin_lock_irqsave(&irq_desc[irq].lock, flags);

    /* 获取当前配置 */
    confreg = readl_relaxed(base + GICD_ICFGRn(irq));

    /* 清除原有配置 */
    if (irq < 32) {
        confreg &= ~(0x3 << ((irq % 16) * 2));
    } else {
        confreg &= ~(0x3 << ((irq % 16) * 2));
    }

    /* 设置新配置 */
    if (type & IRQ_TYPE_LEVEL_MASK) {
        confreg |= GICD_ICFGR_LEVEL << ((irq % 16) * 2);
    } else {
        confreg |= GICD_ICFGR_EDGE << ((irq % 16) * 2);
    }

    writel_relaxed(confreg, base + GICD_ICFGRn(irq));

    /* 更新Linux中断描述符 */
    irq_desc[irq].irq_data.chip_data = (void *)(type & IRQ_TYPE_SENSE_MASK);

    raw_spin_unlock_irqrestore(&irq_desc[irq].lock, flags);

    return 0;
}
```

### 5.6.10gic_chip_flags与中断标志

```c
// 中断标志定义（include/linux/irq.h）
#define IRQ_TYPE_NONE           0x00000000  // 无效类型
#define IRQ_TYPE_EDGE_RISING    0x00000001  // 上升沿触发
#define IRQ_TYPE_EDGE_FALLING   0x00000002  // 下降沿触发
#define IRQ_TYPE_EDGE_BOTH      (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_EDGE_FALLING)
#define IRQ_TYPE_LEVEL_HIGH     0x00000004  // 高电平触发
#define IRQ_TYPE_LEVEL_LOW      0x00000008  // 低电平触发
#define IRQ_TYPE_LEVEL_MASK     (IRQ_TYPE_LEVEL_HIGH | IRQ_TYPE_LEVEL_LOW)
#define IRQ_TYPE_SENSE_MASK     0x0000000f

/* GIC芯片标志 */
static struct irq_chip gic_chip = {
    .name           = "GICv3",
    .flags          = IRQCHIP_SKIP_SET_WAKE |  // 跳过设置唤醒
                      IRQCHIP_MASK_ON_SUSPEND, // 挂起时保持屏蔽
    /* ... */
};

/* 常用中断标志组合 */
IRQF_TRIGGER_RISING      = IRQF_TRIGGER_HIGH | IRQF_TRIGGER_RISING
IRQF_TRIGGER_HIGH        = (IRQ_TYPE_LEVEL_HIGH << IRQ_TYPE_SENSE_SHIFT)
IRQF_TRIGGER_LOW         = (IRQ_TYPE_LEVEL_LOW << IRQ_TYPE_SENSE_SHIFT)
IRQF_SHARED              = 0x00000080  // 共享中断
IRQF_PERCPU              = 0x00000400  // 每CPU中断
IRQF_ONESHOT             = 0x00004000  // 单次触发
```

### 5.6.11gic_desc结构与中断描述符

```c
/* GIC中断描述符扩展 */
struct gic_irq_desc {
    u32 hwirq;                    // 硬件中断号
    u32 status;                   // 中断状态
    u8 priority;                  // 中断优先级
    cpumask_t affinity;           // 亲和性掩码
    void *handler_data;           // 处理程序私有数据
};

/* 全局GIC描述符 */
static struct gic_chip_data {
    void __iomem *dist_base;      // Distributor基地址
    void __iomem *cpu_base;       // CPU Interface基地址
    struct irq_domain *domain;    // irqdomain
    u32 nr_irqs;                  // 支持的中断数量
    u32 max_irqs;                  // 最大中断号
    struct cpumask *cpumask;      // CPU掩码
    unsigned long *enabled_irqs;  // 已使能中断位图
} gic_data __read_mostly;

/* 初始化GIC描述符 */
static int __init gic_of_init(struct device_node *node,
                              struct gic_chip_data *gic)
{
    int ret;

    /* 从设备树获取寄存器地址 */
    ret = of_property_read_u32_array(node, "reg", reg, 2);
    if (ret)
        return ret;

    /* 映射寄存器 */
    gic->dist_base = ioremap(reg[0], reg[1]);
    gic->cpu_base = ioremap(reg[2], reg[3]);

    /* 初始化Distributor */
    gic_dist_init(gic);

    /* 初始化CPU Interface */
    gic_cpu_init(gic);

    /* 创建irqdomain */
    gic->domain = irq_domain_add_linear(node, gic->max_irqs,
                                        &gic_irq_domain_ops, gic);

    return 0;
}
```

---

## 本章面试题

### 1. GICv2和GICv3的主要区别是什么？

参考答案：GICv3相比GICv2有以下主要区别：1) 引入Redistributor组件，分散处理负载；2) 支持LPI（Locality-specific Peripheral Interrupt）中断，基于消息机制；3) 支持ITS（Interrupt Translation Service）处理MSI；4) 通过系统寄存器（ICC_*_EL1）访问更高效；5) 支持更多CPU核心；6) 支持更灵活的亲和性配置。GICv3是现代ARM服务器和移动设备的主流选择。

### 2. GIC的中断类型有哪些？它们有什么区别？

参考答案：GIC定义了四种中断类型：SGI（Software Generated Interrupt，0-15）用于CPU间通信，通过软件触发；PPI（Private Peripheral Interrupt，16-31）是每个CPU私有的中断，如定时器；SPI（Shared Peripheral Interrupt，32-1019）是共享外设中断，可配置目标CPU；LPI（Locality-specific Peripheral Interrupt，8192+）是GICv3引入的消息中断。SGI和PPI与特定CPU绑定，SPI可路由，LPI通过ITS配置。

### 3. 描述GIC Distributor的主要功能及其关键寄存器。

参考答案：Distributor是GIC的中央组件，负责：1) 接收所有中断请求；2) 优先级排序；3) 中断分发到目标CPU。关键寄存器包括：GICD_CTLR（控制寄存器，使能中断分组）、GICD_ISENABLER/GICD_ICENABLER（中断使能/禁用）、GICD_IPRIORITYR（设置中断优先级）、GICD_ITARGETSR（配置中断目标CPU）、GICD_ICFGR（配置触发类型）。

### 4. 什么是EOI（End of Interrupt）？为什么需要在中断处理完成后发送EOI？

参考答案：EOI是中断结束信号，CPU通过写入GICC_EOIR寄存器通知GIC当前中断处理完成。EOI的作用包括：1) 清除GIC中当前中断的激活状态，允许优先级更低的中断被处理；2) 更新中断优先级堆栈；3) 对于电平触发的中断，EOI后如果外设仍然保持有效信号，中断会重新触发。正确发送EOI是保证中断嵌套和优先级机制正常工作的关键。

### 5. 如何配置中断的亲和性（Affinity）？

参考答案：中断亲和性通过GICD_ITARGETSR寄存器配置（GICv2）或通过Redistributor配置（GICv3）。用户空间可通过`/proc/irq/<irq_num>/smp_affinity`文件设置位掩码。内核中使用irq_set_affinity()函数或驱动中通过irq_set_affinity_hint()设置。亲和性设置使得可以将中断集中在特定CPU上处理，提高缓存命中率或实现负载均衡。

### 6. 解释GIC的优先级系统，优先级掩码寄存器（PMR）的作用是什么？

参考答案：GIC使用优先级数值表示中断优先级，数值越小优先级越高。GICv2支持8位优先级（0-255），可通过GICD_PGCR分成组优先级和子优先级。CPU Interface的PMR寄存器设置优先级阈值，只有优先级高于PMR值（数值小于PMR）的中断才会被该CPU接收。这允许CPU临时屏蔽低优先级中断，处理更高优先级的任务。

### 7. handle_fasteoi_irq函数的工作流程是什么？

参考答案：handle_fasteoi_irq是GIC中断的标准处理函数，工作流程：1) 调用chip->irq_read_retention_note读取IAR获取中断号；2) 判断是否为有效中断（排除spurious interrupt）；3) 调用handle_irq_event执行中断处理；4) 调用chip->irq_eoi写入EOIR发送中断结束信号；5) 完成处理并检查是否需要重新调度。这个流程采用快速EOI模式，处理完成后立即发送EOI，而不是在中断处理函数返回后。

### 8. Linux内核GIC驱动中，gic_chip结构包含哪些关键回调函数？

参考答案：Linux GIC驱动的struct irq_chip包含关键回调：irq_mask（屏蔽中断）、irq_unmask（使能中断）、irq_eoi（发送EOI）、irq_set_type（设置触发类型）、irq_set_affinity（设置亲和性）、irq_retrigger（重新触发中断）、irq_set_wake（设置唤醒功能）。这些回调函数对应GIC硬件寄存器的操作，构成了Linux中断子系统与GIC硬件之间的桥梁。

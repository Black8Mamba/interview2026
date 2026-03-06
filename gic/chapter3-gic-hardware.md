# 第三章：SR6P3C4 GIC硬件架构

## 3.1 GIC硬件架构概述

SR6P3C4采用的GICv3架构由三个主要组件构成：**Distributor（分发器）**、**Redistributor（重分发器）**和**CPU Interface（CPU接口）**。每个组件承担不同的职责，共同完成中断的管理和分发。

### 硬件架构框图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SR6P3C4 GICv3 硬件架构                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                         GIC Distributor (GICD)                        │  │
│  │  地址: 0xFC000000                                                    │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────────────────┐│  │
│  │  │ GICD_CTLR │ │ GICD_TYPER│ │ GICD_IIDR│ │ GICD_IGROUPR[n]      ││  │
│  │  │  控制寄存器│ │ 类型寄存器 │ │ 实现ID   │ │ 中断分组              ││  │
│  │  └──────────┘ └──────────┘ └──────────┘ └─────────────────────────┘│  │
│  │  ┌────────────────────────────────┐ ┌─────────────────────────────┐ │  │
│  │  │ GICD_ISENABLER[n] / ICENABLER │ │ GICD_IPRIORITYR[n]         │ │  │
│  │  │ 中断使能/禁用                   │ │ 中断优先级                 │ │  │
│  │  └────────────────────────────────┘ └─────────────────────────────┘ │  │
│  │  ┌────────────────────────────────┐ ┌─────────────────────────────┐ │  │
│  │  │ GICD_IROUTER[n]                │ │ GICD_ICFGR[n]              │ │  │
│  │  │ 中断路由配置 (Affinity)        │ │ 中断触发类型配置            │ │  │
│  │  └────────────────────────────────┘ └─────────────────────────────┘ │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                      │                                        │
│                      ┌───────────────┼───────────────┐                       │
│                      │               │               │                        │
│                      ▼               ▼               ▼                        │
│  ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐ │
│  │  Redistributor #0    │  │  Redistributor #1   │  │  Redistributor #2   │ │
│  │  (Core 0)           │  │  (Core 1)           │  │  (Core 2)          │ │
│  │  0xFC010000        │  │  0xFC012000        │  │  0xFC014000        │ │
│  │ ┌───────────────┐  │  │ ┌───────────────┐  │  │ ┌───────────────┐   │ │
│  │ │ GICR_CTLR     │  │  │ │ GICR_CTLR     │  │  │ │ GICR_CTLR     │   │ │
│  │ │ GICR_TYPER    │  │  │ │ GICR_TYPER    │  │  │ │ GICR_TYPER    │   │ │
│  │ │ GICR_WAKER    │  │  │ │ GICR_WAKER    │  │  │ │ GICR_WAKER    │   │ │
│  │ └───────────────┘  │  │ └───────────────┘  │  │ └───────────────┘   │ │
│  │ ┌───────────────┐  │  │ ┌───────────────┐  │  │ ┌───────────────┐   │ │
│  │ │ SGI/PPI 寄存器│  │  │ │ SGI/PPI 寄存器│  │  │ │ SGI/PPI 寄存器│   │ │
│  │ │ (私有中断)    │  │  │ │ (私有中断)    │  │  │ │ (私有中断)    │   │ │
│  │ └───────────────┘  │  │ └───────────────┘  │  │ └───────────────┘   │ │
│  └─────────────────────┘  └─────────────────────┘  └─────────────────────┘ │
│              │                       │                       │               │
│              │                       │                       │               │
│              ▼                       ▼                       ▼               │
│  ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐ │
│  │   CPU Interface #0  │  │   CPU Interface #1  │  │   CPU Interface #2  │ │
│  │  (ICC_* 寄存器)     │  │  (ICC_* 寄存器)     │  │  (ICC_* 寄存器)     │ │
│  │ ┌───────────────┐   │  │ ┌───────────────┐   │  │ ┌───────────────┐   │ │
│  │ │ ICC_CTLR      │   │  │ │ ICC_CTLR      │   │  │ │ ICC_CTLR      │   │ │
│  │ │ ICC_PMR       │   │  │ │ ICC_PMR       │   │  │ │ ICC_PMR       │   │ │
│  │ │ ICC_BPR0/1    │   │  │ │ ICC_BPR0/1    │   │  │ │ ICC_BPR0/1    │   │ │
│  │ │ ICC_IAR/IAR  │   │  │ │ ICC_IAR/IAR   │   │  │ │ ICC_IAR/IAR   │   │ │
│  │ └───────────────┘   │  │ └───────────────┘   │  │ └───────────────┘   │ │
│  └─────────────────────┘  └─────────────────────┘  └─────────────────────┘ │
│              │                       │                       │               │
│              ▼                       ▼                       ▼               │
│         ┌────────┐              ┌────────┐              ┌────────┐          │
│         │ Core 0 │              │ Core 1 │              │ Core 2 │          │
│         │ Cortex │              │ Cortex │              │ Cortex │          │
│         │  -R52  │              │  -R52  │              │  -R52  │          │
│         └────────┘              └────────┘              └────────┘          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 地址映射

根据SR6P3C4的头文件定义，GIC寄存器的地址映射如下：

```c
// SR6P3C4 GIC地址定义
#define LS2_CORTEXR52_GICD_INTERFACE    (*(volatile struct ...) (0xFC000000U))

// Cluster 2 例子
#define LS2_CORTEXR52_GICR_CTRL_CPU0    (*(volatile struct ...) (0xFC000000U + 0x00100000U))
#define LS2_CORTEXR52_GICR_SGI_PPI_CPU0 (*(volatile struct ...) (0xFC000000U + 0x00110000U))
#define LS2_CORTEXR52_GICR_CTRL_CPU1    (*(volatile struct ...) (0xFC000000U + 0x00120000U))
#define LS2_CORTEXR52_GICR_SGI_PPI_CPU1 (*(volatile struct ...) (0xFC000000U + 0x00130000U))

// 实际使用的宏定义
#define CORTEXR52_GICR_CTRL_CPU0     LS2_CORTEXR52_GICR_CTRL_CPU0
#define CORTEXR52_GICR_SGI_PPI_CPU0  LS2_CORTEXR52_GICR_SGI_PPI_CPU0
#define CORTEXR52_GICR_CTRL_CPU1     LS2_CORTEXR52_GICR_CTRL_CPU1
#define CORTEXR52_GICR_SGI_PPI_CPU1  LS2_CORTEXR52_GICR_SGI_PPI_CPU1
#define CORTEXR52_GICD_INTERFACE      LS2_CORTEXR52_GICD_INTERFACE
```

---

## 3.2 GIC Distributor 寄存器组

Distributor是GIC的核心组件，负责管理所有SPI（共享外设中断）和SGI（软件生成中断）的中断配置和分发。

### 关键寄存器

| 寄存器 | 名称 | 功能描述 |
|--------|------|----------|
| GICD_CTLR | 控制寄存器 | 启用/禁用GIC，启用分组 |
| GICD_TYPER | 类型寄存器 | 获取GIC配置信息 |
| GICD_IIDR | 实现ID寄存器 | 识别GIC版本 |
| GICD_IGROUPR[n] | 分组寄存器 | 设置中断分组 |
| GICD_ISENABLER[n] | 使能寄存器 | 启用中断 |
| GICD_ICENABLER[n] | 禁用寄存器 | 禁用中断 |
| GICD_IPRIORITYR[n] | 优先级寄存器 | 设置中断优先级 |
| GICD_IROUTER[n] | 路由寄存器 | 配置中断目标核心 |
| GICD_ICFGR[n] | 配置寄存器 | 设置触发类型 |

### GICD_CTLR 详解

```c
// GICD_CTLR 寄存器结构
typedef struct {
    uint32_t ARE_S:1;       // [0]   Affinity Routing Enable
    uint32_t ARE_NS:1;      // [1]   Affinity Routing Enable (Non-secure)
    uint32_t EnableGrp0:1; // [2]   Enable Group 0 interrupts
    uint32_t EnableGrp1:1; // [3]   Enable Group 1 interrupts
    uint32_t RWP:1;         // [4]   Register Write Pending
    uint32_t reserved:27;
} GIC_DISTRIBUTER_CONTROL_MM_tag;
```

**关键字段说明：**

1. **ARE (Affinity Routing Enable)**
   - `ARE_S`: 安全世界的Affinity Routing使能
   - `ARE_NS`: 非安全世界的Affinity Routing使能
   - 启用后，GIC使用更灵活的路由机制

2. **EnableGrp0/EnableGrp1**
   - 启用Group 0和Group 1中断

3. **RWP (Register Write Pending)**
   - 指示之前的寄存器写入是否还在进行中
   - 写入关键寄存器后应等待此位清零

### SDK中的GICD配置

```c
// intc.c 中的GICD配置代码
static void gic_init(void)
{
    /* GICD configuration - At cluster level */
    /* each cluster can only see their own GIC */

    /* Affinity Routing Enable */
    GICD.GICD_CTLR.B.ARE = 1;
    while (GICD.GICD_CTLR.B.RWP != 0U) {
        ;  // 等待写入完成
    }

    /* Enable Group 0 interrupt : FIQ */
    GICD.GICD_CTLR.B.ENABLEGRP0 = 1;
    while (GICD.GICD_CTLR.B.RWP != 0U) {
        ;
    }

    /* Enable Group 1 interrupt : IRQ */
    GICD.GICD_CTLR.B.ENABLEGRP1 = 1;
    while (GICD.GICD_CTLR.B.RWP != 0U) {
        ;
    }
}
```

### GICD_TYPER 详解

```c
// GICD_TYPER 寄存器 - 只读，标识GIC能力
typedef struct {
    uint32_t ITLinesNumber:5;    // [0:4]  中断线数量 (实际SPI数 = (ITLinesNumber+1)*32)
    uint32_t CPUNumber:3;        // [5:7]  CPU核心数-1
    uint32_t SecExtn:1;          // [8]    安全扩展
    uint32_t reserved1:3;
    uint32_t Attr:4;              // [12:15] 实现属性
    uint32_t reserved2:1;
    uint32_t PRIBits:3;          // [17:19] 优先级位数-1 (0=8位, 1=16位...)
    uint32_t reserved3:1;
    uint32_t IDBITS:5;           // [21:25] 标识ID位数-1
    uint32_t reserved4:6;
} GIC_DISTRIBUTER_TYPE_MM_tag;

// SR6P3C4典型值:
// - ITLinesNumber = 20 (最大1024个SPI)
// - CPUNumber = 3 (4核)
// - PRIBits = 0 (8位优先级)
```

---

## 3.3 GIC Redistributor 寄存器组

Redistributor是GICv3新增的组件，每个CPU核心都有一个独立的Redistributor，负责管理该核心的私有中断（SGI和PPI）。

### 地址偏移

```c
// Redistributor 地址偏移 (相对于GIC基址)
#define GICR_CPU0_OFFSET   0x00100000  // Redistributor控制
#define GICR_SGI_PPI0     0x00110000  // SGI/PPI寄存器
#define GICR_CPU1_OFFSET  0x00120000
#define GICR_SGI_PPI1     0x00130000
```

### 关键寄存器

| 寄存器 | 名称 | 功能描述 |
|--------|------|----------|
| GICR_CTLR | 控制寄存器 | 重分发器控制 |
| GICR_TYPER | 类型寄存器 | 标识核心配置 |
| GICR_WAKER | 唤醒寄存器 | 控制核心休眠/唤醒 |
| GICR_IGROUPR0 | 分组寄存器 | SGI/PPI分组 |
| GICR_ISENABLER0 | 使能寄存器 | 启用SGI/PPI |
| GICR_IPRIORITYR[n] | 优先级寄存器 | SGI/PPI优先级 |
| GICR_ICFGR0/1 | 配置寄存器 | 触发类型 |

### GICR_WAKER 详解

```c
// GICR_WAKER 寄存器 - 控制核心休眠状态
typedef struct {
    uint32_t ProcessorSleep:1;    // [0]  核心休眠状态
    uint32_t ChildrenAsleep:1;    // [1]  子系统休眠状态
    uint32_t reserved:30;
} GIC_REDISTRIBUTER_WAKER_MM_tag;
```

**工作原理：**
- `ProcessorSleep = 1`: 核心进入休眠，GIC不再向此核心发送中断
- `ProcessorSleep = 0`: 核心唤醒
- 写入后需等待`ChildrenAsleep`清零，确保重分发器完全唤醒

### SDK中的Redistributor配置

```c
// intc.c 中的Redistributor初始化
static void gic_init(void)
{
    // ... GICD配置 ...

    /* GICR configuration depends on the core */
    switch (get_core_id()) {
      case 0U:
        GICR_CTRL_CPU0.GICR_WAKER.B.PROCESSORSLEEP = 0;
        while (GICR_CTRL_CPU0.GICR_WAKER.B.CHILDRENASLEEP != 0U) {
            ;  // 等待唤醒完成
        }
      break;
      case 1U:
        GICR_CTRL_CPU1.GICR_WAKER.B.PROCESSORSLEEP = 0;
        while (GICR_CTRL_CPU1.GICR_WAKER.B.CHILDRENASLEEP != 0U) {
            ;
        }
      break;
#ifdef CORTEXR52_GICR_CTRL_CPU2
      case 2U:
        GICR_CTRL_CPU2.GICR_WAKER.B.PROCESSORSLEEP = 0;
        while (GICR_CTRL_CPU2.GICR_WAKER.B.CHILDRENASLEEP != 0U) {
            ;
        }
      break;
#endif
#ifdef CORTEXR52_GICR_CTRL_CPU3
      case 3U:
        GICR_CTRL_CPU3.GICR_WAKER.B.PROCESSORSLEEP = 0;
        while (GICR_CTRL_CPU3.GICR_WAKER.B.CHILDRENASLEEP != 0U) {
            ;
        }
      break;
#endif
      default:
      break;
    }
}
```

### GICR_TYPER 详解

```c
// GICR_TYPER 寄存器 - 标识核心能力
typedef struct {
    uint32_t reserved1:8;
    uint32_t ProcessorNumber:4;  // [8:11]  核心号
    uint32_t reserved2:1;
    uint32_t DPGS:1;            // [13]    Direct Provision of GICv4
    uint32_t reserved3:1;
    uint32_t Last:1;            // [15]    最后一个Redistributor
    uint32_t ID:16;             // [16:31] 保留
} GIC_REDISTRIBUTER_TYPE_MM_tag;
```

---

## 3.4 CPU Interface 寄存器组

CPU Interface是连接GIC和CPU核心的桥梁，通过系统寄存器（ARM CP15协处理器）访问。

### 访问方式

```c
// CPU Interface 通过系统寄存器访问
// 在SDK中使用以下函数:

// 写系统寄存器
arm_write_sysreg(CP15_ICC_PMR, icc_pmr);      // 优先级掩码
arm_write_sysreg(CP15_ICC_IGRPEN0, 1);        // Group 0使能
arm_write_sysreg(CP15_ICC_IGRPEN1, 1);        // Group 1使能
arm_write_sysreg(CP15_ICC_BPR0, 0);           // Group 0优先级分组
arm_write_sysreg(CP15_ICC_BPR1, 1);           // Group 1优先级分组

// 读系统寄存器
arm_write_sysreg_64(CP15_ICC_SGI0R, icc_sgi); // 生成SGI
arm_write_sysreg_64(CP15_ICC_SGI1R, icc_sgi); // 生成SGI
```

### 关键寄存器 (通过CP15访问)

| 寄存器 | 名称 | 功能描述 |
|--------|------|----------|
| ICC_CTLR | 控制寄存器 | 中断处理模式配置 |
| ICC_PMR | 优先级掩码 | 过滤低于此优先级的中断 |
| ICC_BPR0/1 | 二进制点寄存器 | 优先级分组 |
| ICC_IAR | 中断确认寄存器 | 读取活动中断ID |
| ICC_EOIR | 中断结束寄存器 | 完成中断处理 |
| ICC_IGRPEN0/1 | 分组使能 | 启用中断组 |

### ICC_PMR 详解

```c
// 优先级掩码寄存器 (ICC_PMR)
// 8位字段，定义可以被当前核心接收的最低优先级

// SDK配置示例
icc_pmr = 0xF8;  // 允许所有优先级 (只使用高5位)
arm_write_sysreg(CP15_ICC_PMR, icc_pmr);

/*
 * 掩码值与优先级关系:
 * 0xFF - 只允许优先级0 (最高)
 * 0xF8 - 允许优先级0-7 (8级)
 * 0xE0 - 允许优先级0-15 (16级)
 * 0x00 - 允许所有优先级0-255
 */
```

### ICC_IGRPEN 详解

```c
// 分组使能寄存器

// 启用Group 0 -> FIQ
arm_write_sysreg(CP15_ICC_IGRPEN0, 1);

// 启用Group 1 -> IRQ
arm_write_sysreg(CP15_ICC_IGRPEN1, 1);

// 在安全系统中的典型配置:
// - 启动时: 两个都启用
// - 切换到安全模式: 只启用Group 0
// - 切换到非安全模式: 只启用Group 1
```

### SDK中完整的CPU Interface初始化

```c
// intc.c 中的CPU Interface配置
static void gic_init(void)
{
    uint32_t icc_pmr;

    // ... GICD和GICR配置 ...

    /* Kite core specific initialization */
    /* we configure the core to accept all priorities */
    icc_pmr = 0xF8;
    arm_write_sysreg(CP15_ICC_PMR, icc_pmr);

    /* Enable Group 0 interrupt : FIQ */
    arm_write_sysreg(CP15_ICC_IGRPEN0, 1);

    /* Enable Group 1 interrupt : IRQ */
    arm_write_sysreg(CP15_ICC_IGRPEN1, 1);

    /* Binary Point Register configuration */
    /* check as 2 minimum allowed as mentioned in Cortex-R52 TRM */
    arm_write_sysreg(CP15_ICC_BPR0, 0);
    arm_write_sysreg(CP15_ICC_BPR1, 1);

    (void)icc_pmr;
}
```

---

## 3.5 IBCM跨集群中断模块

### 什么是IBCM

**IBCM (Inter-Cluster Broadcast Module)** 是SR6P3C4特有的组件，用于在不同Cluster之间传递SGI中断。由于GIC的SGI只能路由到同一Cluster内的核心，跨Cluster通信需要通过IBCM实现。

### IBCM工作原理

```
┌─────────────────────────────────────────────────────────────────┐
│                    IBCM 跨集群中断机制                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Cluster 0                              Cluster 1             │
│  ┌─────────┐                           ┌─────────┐           │
│  │ Core 0   │◄───GIC SGI               │ Core 0   │           │
│  └─────────┘                           └────┬────┘           │
│  ┌─────────┐        ┌─────────┐             │                  │
│  │ Core 1   │◄───────┤        │             │                  │
│  └────┬────┘        │ IBCM   │             │                  │
│       │             │        │             │                  │
│       │             │        │             │                  │
│  ─────┴─────────────┴────────┴─────────────┴────────────────  │
│                         │                                        │
│              跨Cluster SGI请求                                   │
│                         │                                        │
│  ───────────────────────┴───────────────────────────────────── │
│                                                                 │
│  处理流程:                                                      │
│  1. Core 0 要向 Cluster 1 的 Core 0 发送中断                    │
│  2. 检测目标不在同一Cluster                                       │
│  3. 调用 ibcm_notify() 写入IBCM                                │
│  4. IBCM产生物理中断到目标Cluster                               │
│  5. 目标Cluster的GIC接收并分发到目标Core                        │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### SDK中的IBCM实现

```c
// ibcm.c 相关实现
// IBCM用于:
// - DME (Core 0 in Peripheral Cluster) - 最多4个SGI
// - DSPH (Core 1 in Peripheral Cluster) - 最多4个SGI
// - 其他Cluster - 最多8个SGI

// IBCM寄存器映射
#define IBCM_BASE_ADDR    0xFC04D000

void ibcm_init(void)
{
    // 初始化IBCM模块
}

void ibcm_notify(uint32_t value)
{
    // 参数格式: [cluster(4bits)][core(3bits)][int_id(8bits)]
    // 计算bit索引
    uint32_t cluster = (value >> 4) & 0xF;
    uint32_t core = (value >> 3) & 0x7;
    uint32_t int_id = value & 0xFF;

    // 写入IBCM触发寄存器
    // ... IBCM硬件操作
}
```

### 跨Cluster中断配置

```c
// 使用irq_notify()进行跨Cluster中断
void irq_notify(irq_cluster_t cluster, irq_core_t core, irq_id_t int_id)
{
    if (gic_is_sgi(int_id) != 0u) {
        if ((uint32_t)cluster == get_cluster_id()) {
            /* Same cluster, use GIC */
            gic_generate_sgi(cluster, core, int_id, IRQ_GROUP_NORMAL_IRQ, 0);
        } else {
            /* Different cluster, use IBCM */
            uint32_t value = ((uint32_t)cluster << 4UL) |
                            ((uint32_t)core << 3UL) |
                            (uint32_t)int_id;

            if (cluster == IRQ_CLUSTER_PER) {
                /* DME and DSPH can handle up to 4 software generated interrupts */
                if (int_id < IRQ_SGI_4_INT_NUMBER) {
                    ibcm_notify(value);
                }
            } else if (cluster < IRQ_CLUSTER_PER) {
                /* Cores in other clusters can handle up to 8 software generated interrupts */
                if (int_id < IRQ_SGI_8_INT_NUMBER) {
                    ibcm_notify(value);
                }
            }
        }
    }
}
```

---

## GIC硬件架构ASCII示意图

```
+=========================================================================+
|                    SR6P3C4 GICv3 硬件架构                              |
+=========================================================================+

                        外设中断源
                            |
        +-------------------+-------------------+
        |                   |                   |
        v                   v                   v
+---------------------+ +---------------------+ +---------------------+
|      Timer         | |      UART          | |       CAN         |
+---------------------+ +---------------------+ +---------------------+
        |                   |                   |
        +-------------------+-------------------+
                            |
                            v
+=========================================================================+
|                     GIC DISTRIBUTOR (GICD)                            |
|                     地址: 0xFC000000                                  |
+=========================================================================+
|                                                                         |
|  +-------------+  +-------------+  +-------------+  +-------------+ |
|  | GICD_CTLR   |  | GICD_TYPER  |  | GICD_IIDR   |  | GICD_IGROUPR| |
|  | 控制寄存器   |  | 类型寄存器   |  | 实现ID     |  | 中断分组   | |
|  +-------------+  +-------------+  +-------------+  +-------------+ |
|                                                                         |
|  +------------------------+  +------------------------+                  |
|  | GICD_ISENABLER[n]     |  | GICD_ICENABLER[n]    |                  |
|  | 中断使能寄存器        |  | 中断禁用寄存器        |                  |
|  +------------------------+  +------------------------+                  |
|                                                                         |
|  +------------------------+  +------------------------+                  |
|  | GICD_IPRIORITYR[n]    |  | GICD_IROUTER[n]       |                  |
|  | 中断优先级寄存器       |  | 中断路由寄存器(Affinity)|                  |
|  +------------------------+  +------------------------+                  |
|                                                                         |
|  +------------------------+                                            |
|  | GICD_ICFGR[n]         |                                            |
|  | 中断配置寄存器(触发类型) |                                            |
|  +------------------------+                                            |
+=========================================================================+
                            |
        +-------------------+-------------------+
        |                   |                   |
        v                   v                   v
+-------------------+ +-------------------+ +-------------------+
|  Redistributor    | |  Redistributor    | |  Redistributor    |
|  Core 0          | |  Core 1          | |  Core 2          |
|  0xFC010000      | |  0xFC012000      | |  0xFC014000      |
+-------------------+ +-------------------+ +-------------------+
        |                   |                   |
        v                   v                   v
+-------------------+ +-------------------+ +-------------------+
|  CPU Interface    | |  CPU Interface    | |  CPU Interface    |
|  (ICC_*寄存器)    | |  (ICC_*寄存器)    | |  (ICC_*寄存器)    |
|  通过CP15访问     | |  通过CP15访问     | |  通过CP15访问     |
+-------------------+ +-------------------+ +-------------------+
        |                   |                   |
        v                   v                   v
+-------------------+ +-------------------+ +-------------------+
|   Cortex-R52     | |   Cortex-R52     | |   Cortex-R52     |
|     Core 0       | |     Core 1       | |     Core 2       |
+-------------------+ +-------------------+ +-------------------+

+=========================================================================+
|                        地址映射表                                      |
+=========================================================================+
|  组件          |  地址范围            |  说明                       |
|----------------|---------------------|-----------------------------|
|  GICD          | 0xFC000000          | Distributor                 |
|  GICR CPU0     | 0xFC010000 ~ 0xFC011FFF | Redistributor Core 0  |
|  GICR CPU1     | 0xFC012000 ~ 0xFC013FFF | Redistributor Core 1  |
|  GICR CPU2     | 0xFC014000 ~ 0xFC015FFF | Redistributor Core 2  |
|  GICR CPU3     | 0xFC016000 ~ 0xFC017FFF | Redistributor Core 3  |
+=========================================================================+

+=========================================================================+
|                        IBCM 跨集群模块                                 |
+=========================================================================+

    Cluster 0                    Cluster 1
    +--------+                   +--------+
    | Core 0 |--- GIC SGI ----->| Core 0 |
    +--------+                   +--------+

    Cluster 0                    Cluster 1
    +--------+                   +--------+
    | Core 0 |<---- IBCM -------| Core 1 |
    +--------+    (跨Cluster)    +--------+

+=========================================================================+
```

---

## 本章小结

本章详细介绍了SR6P3C4的GIC硬件架构：

1. **GIC Distributor (GICD)**
   - 地址: 0xFC000000
   - 管理所有SPI中断
   - 关键寄存器: CTLR, TYPER, IGROUPR, ISENABLER, IROUTER等

2. **GIC Redistributor (GICR)**
   - 每个核心独立配置
   - 管理SGI和PPI私有中断
   - 关键寄存器: WAKER, IGROUPR0, ISENABLER0等

3. **CPU Interface (ICC_*)**
   - 通过CP15系统寄存器访问
   - 关键寄存器: PMR, BPR0/1, IGRPEN0/1

4. **IBCM跨集群模块**
   - 解决跨Cluster SGI通信
   - 支持DME/DSPH与其他Cluster间中断

---

## 下章预告

第四章将深入SDK源码，详细解析：
- `irq.h` API接口定义
- `irq_id.h` 中断号定义
- `intc.c` 核心驱动逐行解析

# 第二章：GICv3/v4核心概念 (下)

## 2.4 中断优先级机制

### 2.4.1 优先级概述

GIC使用优先级机制来决定当多个中断同时发生时哪个中断被优先处理。优先级数值越低，优先级越高（这与某些人的直觉相反）。

**关键特性**
- **优先级位数**: 8位 (256个级别)
- **有效优先级**: 高5位 (32个级别，由0xF8掩码决定)
- **优先级值**: 0（最高）到 255（最低）
- **默认值**: 通常为128

### 2.4.2 优先级寄存器

```c
// 优先级存储在8位寄存器中，每4个中断占用一个32位寄存器
// GICD_IPRIORITYR[n] 对应中断 (n*4) 到 (n*4+3)

// 优先级字段定义 (8位)
bit[7:5] = 优先级值的高3位
bit[4:0] = 保留

// SR6P3C4使用的掩码
priority_mask = 0xF8;  // 只使用高5位
```

### 2.4.3 优先级掩码与优先级分组

CPU Interface中的**优先级掩码寄存器 (ICC_PMR)** 决定了哪些优先级的中断可以被当前核心接收：

```c
// SDK中的配置
icc_pmr = 0xF8;  // 允许所有优先级 (0-248，实际有效5位)
arm_write_sysreg(CP15_ICC_PMR, icc_pmr);
```

**优先级掩码工作原理**

```
ICC_PMR = 0xF8 (允许所有优先级)
          │
          ▼
    ┌─────────────────────────────────────┐
    │  优先级0 (最高)  ──────────────✓   │
    │  优先级8              ─────────✓   │
    │  优先级16             ─────────✓   │
    │  优先级24             ─────────✓   │
    │  ...                 ─────────✓   │
    │  优先级248 (最低)    ─────────✓   │
    └─────────────────────────────────────┘

ICC_PMR = 0x40 (只允许优先级0-64)
          │
          ▼
    ┌─────────────────────────────────────┐
    │  优先级0-63      ──────────────✓   │
    │  优先级64-248    ──────────────✗   │ 被屏蔽
    └─────────────────────────────────────┘
```

### 2.4.4 SDK中的优先级配置

```c
// 设置SGI优先级
static void gic_configure_sgi(irq_id_t int_id, irq_group_t group,
                              irq_signal_t signal, uint32_t priority,
                              uint32_t core)
{
    GICR_t  *gicr;
    uint8_t *prio_regs;

    gicr = GICR_SGI_PPI(core);

    // 计算优先级字节 (只使用高5位)
    prio_regs = (uint8_t *)(&(gicr->GICR_IPRIORITYR[(uint32_t)int_id >> 2U]));
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));
}

// 设置PPI优先级
static void gic_configure_ppi(irq_id_t int_id, irq_group_t group,
                              irq_signal_t signal, uint32_t priority,
                              uint32_t core)
{
    uint8_t *prio_regs;
    GICR_t  *gicr;

    gicr = GICR_SGI_PPI(core);

    // 设置PPI优先级
    prio_regs = (uint8_t *)(&(gicr->GICR_IPRIORITYR[(uint32_t)int_id >> 2U]));
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));
}
```

---

## 2.5 中断分组与FIQ/IRQ映射

### 2.5.1 分组机制概述

GICv3支持将中断分为不同的组，这主要用于安全系统和虚拟化场景：

| 分组 | 名称 | 典型映射 | 用途 |
|------|------|----------|------|
| **Group 0** | 安全中断 | FIQ | 安全固件、关键安全功能 |
| **Group 1** | 非安全中断 | IRQ | 普通应用代码 |

### 2.5.2 分组配置

```c
// GIC Distributor中启用分组
GICD.GICD_CTLR.B.ENABLEGRP0 = 1;  // 启用Group 0
GICD.GICD_CTLR.B.ENABLEGRP1 = 1;  // 启用Group 1

// CPU Interface中启用分组处理
arm_write_sysreg(CP15_ICC_IGRPEN0, 1);  // 启用Group 0 → FIQ
arm_write_sysreg(CP15_ICC_IGRPEN1, 1);  // 启用Group 1 → IRQ
```

### 2.5.3 FIQ/IRQ映射关系

```
┌─────────────────────────────────────────────────────────────────┐
│                      中断分组与FIQ/IRQ映射                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   外设中断                                                        │
│       │                                                          │
│       ▼                                                          │
│   ┌───────────────┐                                              │
│   │   GIC Dist    │                                              │
│   │  (分组判断)    │                                              │
│   └───────┬───────┘                                              │
│           │                                                       │
│     ┌─────┴─────┐                                                │
│     │           │                                                │
│ Group 0      Group 1                                            │
│ (安全中断)    (非安全中断)                                       │
│     │           │                                                │
│     ▼           ▼                                                │
│   ┌─────┐    ┌─────┐                                           │
│   │ FIQ │    │ IRQ │                                           │
│   └─────┘    └─────┘                                           │
│     │           │                                                │
│     ▼           ▼                                                │
│  安全固件    普通应用                                             │
│  处理函数    处理函数                                             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.5.4 SDK中的分组设置

```c
// 设置SGI中断分组
static void gic_configure_sgi(irq_id_t int_id, irq_group_t group,
                              irq_signal_t signal, uint32_t priority,
                              uint32_t core)
{
    GICR_t  *gicr;

    gicr = GICR_SGI_PPI(core);

    // 设置分组 (0=Group 0=FIQ, 1=Group 1=IRQ)
    gicr->GICR_IGROUPR0.R =
        (gicr->GICR_IGROUPR0.R & ~(1UL << (uint32_t)int_id)) |
        ((uint32_t)group << (uint32_t)int_id);
}

// 设置SPI中断分组
static void gic_configure_spi(irq_id_t int_id, irq_group_t group,
                               irq_signal_t signal, uint32_t priority,
                               uint32_t core)
{
    GICD.GICD_IGROUPR[(uint32_t)int_id >> 5U].B.GROUP_STATUS_BIT =
        (GICD.GICD_IGROUPR[(uint32_t)int_id >> 5U].B.GROUP_STATUS_BIT &
         ~(1UL << ((uint32_t)int_id & 0x1fU))) |
        ((uint32_t)group << ((uint32_t)int_id & 0x1fU));
}
```

### 2.5.5 优先级分组 (BPR)

**二进制点寄存器 (BPR)** 定义了优先级字段的分割点，用于决定优先级比较和抢占的粒度：

```c
// SDK中的BPR配置
arm_write_sysreg(CP15_ICC_BPR0, 0);   // Group 0: 无分割，最精细优先级
arm_write_sysreg(CP15_ICC_BPR1, 1);   // Group 1: 1位分割
```

**BPR对优先级比较的影响**

```
BPR = 0 (Group 0):
  优先级字段: [7:5][4:3][2:0]
               │  │   │
               │  │   └── 8个亚优先级
               │  └────── 4个优先级组
               └───────── 8个主优先级

BPR = 3 (Group 1):
  优先级字段: [7:5][4:3][2:0]
                    │   │
                    │   └── 被合并(忽略)
                    └─────── 8个优先级组
```

---

## 2.6 Affinity Routing多核路由机制

### 2.6.1 什么是Affinity Routing

**Affinity Routing** 是GICv3引入的强大特性，允许根据CPU核心的亲和性（affinity）来路由中断。它使用一个层次化的亲和性值来标识系统中的每个核心：

```
Affinity格式: affinity[3:0] = XX.YY.ZZ.WW

- affinity[3] (XX): 最高层，通常保留或表示超级簇
- affinity[2] (YY): Cluster ID (簇ID)
- affinity[1] (ZZ): CPU ID within cluster
- affinity[0] (WW): 通常为0或用于更细粒度控制
```

### 2.6.2 SR6P3C4的Affinity配置

```c
// SDK中定义的核心/簇ID
typedef enum {
    IRQ_CLUSTER_0,   // Cluster 0
    IRQ_CLUSTER_1,  // Cluster 1
    IRQ_CLUSTER_2,  // Cluster 2
    IRQ_CLUSTER_PER, // 外设Cluster (用于特殊路由)
    IRQ_CLUSTER_NONE
} irq_cluster_t;

typedef enum {
    IRQ_CORE_0,
    IRQ_CORE_1,
    IRQ_CORE_2,
    IRQ_CORE_3,
    IRQ_CORE_DME = IRQ_CORE_0,  // DME核心 (Cluster 3, Core 0)
    IRQ_CORE_DSPH = IRQ_CORE_1  // DSPH核心 (Cluster 3, Core 1)
} irq_core_t;
```

### 2.6.3 中断路由配置

```c
// 配置SPI中断路由到指定核心
static void gic_configure_spi(irq_id_t int_id, irq_group_t group,
                               irq_signal_t signal, uint32_t priority,
                               uint32_t core)
{
    // 使用IROUTER寄存器配置目标核心
    // Affinity值格式: [47:32]保留, [31:16]目标, [15:0]保留
    // 在SR6P3C4中: (uint64_t)core 直接作为目标
    GICD.GICD_IROUTER[int_id].R = (uint64_t)core;
}
```

### 2.6.4 Affinity Routing路由示意图

```
┌─────────────────────────────────────────────────────────────────┐
│                   Affinity Routing 多核路由                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  中断目标配置                                                    │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │ GICD_IROUTER[n] 寄存器                                   │  │
│  │                                                           │  │
│  │  bit[47:32] = 0x0000 (保留)                              │  │
│  │  bit[31:16] = Affinity[3:0]                              │  │
│  │         │                                                │  │
│  │         │  ┌──────┬──────┬──────┬──────┐                 │  │
│  │         └──│  0.0 │  0.1 │  1.0 │  1.1 │                 │  │
│  │            │Core0 │Core1 │Core0 │Core1 │                 │  │
│  │            └──────┴──────┴──────┴──────┘                 │  │
│  │  bit[15:0] = 0x0000 (保留)                              │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  路由示例:                                                       │
│  ┌────────────┬────────────────────────────────────────────┐   │
│  │ IROUTER值   │ 路由目标                                    │   │
│  ├────────────┼────────────────────────────────────────────┤   │
│  │ 0x00000000  │ Cluster 0, Core 0                         │   │
│  │ 0x00000001  │ Cluster 0, Core 1                         │   │
│  │ 0x00010000  │ Cluster 1, Core 0                         │   │
│  │ 0x00010001  │ Cluster 1, Core 1                         │   │
│  │ 0xFFFFFFFF  │ 路由到所有可用核心                          │   │
│  └────────────┴────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.6.5 跨Cluster中断路由

SR6P3C4使用IBCM模块支持跨Cluster中断：

```c
// 跨Cluster中断生成
void irq_notify(irq_cluster_t cluster, irq_core_t core, irq_id_t int_id)
{
    if (gic_is_sgi(int_id) != 0u) {
        if ((uint32_t)cluster == get_cluster_id()) {
            // 同Cluster: 使用GIC SGI
            gic_generate_sgi(cluster, core, int_id, IRQ_GROUP_NORMAL_IRQ, 0);
        } else {
            // 跨Cluster: 使用IBCM
            uint32_t value = ((uint32_t)cluster << 4UL) |
                            ((uint32_t)core << 3UL) |
                            (uint32_t)int_id;
            ibcm_notify(value);
        }
    }
}
```

---

## 中断分组与FIQ/IRQ映射图

```
+=========================================================================+
|                    中断分组与FIQ/IRQ映射机制                             |
+=========================================================================+

                         外设中断
                            |
                            v
                    +--------------+
                    |  GIC Dist   |
                    |  (分组判断)  |
                    +------+-------+
                           |
               +-----------+-----------+
               |                       |
          Group 0                Group 1
         (安全中断)              (非安全)
               |                       |
               v                       v
            +-------+              +-------+
            |  FIQ  |              |  IRQ  |
            +---+---+              +---+---+
                |                      |
                v                      v
          +---------+            +---------+
          | 安全固件 |            |  普通OS  |
          | 处理函数 |            | 处理函数  |
          +---------+            +---------+

+=========================================================================+
|                        优先级字段格式                                      |
+=========================================================================+

  : +优先级字段---+---+---+---+---+---+---+---+
              | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
              +---+---+---+---+---+---+---+---+
                |   |   |
                |   |   +--- 保留 (置0)
                |   +------- 有效优先级位 (实际使用高5位)
                +----------- 保留

  SR6P3C4使用掩码 0xF8:
              +---+---+---+---+---+---+---+---+
              | 1 | 1 | 1 | 1 | 1 | 0 | 0 | 0 | = 0xF8
              +---+---+---+---+---+---+---+---+
                              +---+---+
                              |   |   |
                              |   |   +--- bit[2:0] 忽略
                              |   +------- bit[4:3] 优先级组
                              +----------- bit[7:5] 主优先级

+=========================================================================+
|                        优先级比较示例                                    |
+=========================================================================+

  中断A: 优先级 = 16 (00010000b)
  中断B: 优先级 = 24 (00011000b)
  中断C: 优先级 = 8  (00001000b)

  优先级比较:
     +----------+
     | 中断 C  | <-- 最高优先级 (数值最小)
     +----------+
     | 中断 A  |
     +----------+
     | 中断 B  | <-- 最低优先级 (数值最大)
     +----------+

  注意: 数值越小，优先级越高!

+=========================================================================+
```

---

## 本章小结

本章下半部分详细介绍了GIC的核心机制：

1. **中断优先级**
   - 8位优先级字段，高5位有效
   - ICC_PMR掩码决定允许的中断范围
   - 数值越低优先级越高

2. **中断分组 (Group 0/1)**
   - Group 0 → FIQ (安全中断)
   - Group 1 → IRQ (非安全中断)
   - 通过GICD_IGROUPR和ICC_IGRPEN配置

3. **Affinity Routing**
   - 基于Cluster/Core的层次化路由
   - GICD_IROUTER寄存器配置目标
   - 跨Cluster使用IBCM模块

---

## 下章预告

第三章将深入讲解SR6P3C4的GIC硬件架构，包括：
- Distributor寄存器详解
- Redistributor寄存器详解
- CPU Interface寄存器详解
- IBCM跨集群模块

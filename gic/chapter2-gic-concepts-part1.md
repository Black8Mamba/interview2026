# 第二章：GICv3/v4核心概念 (上)

## 2.1 中断类型详解

GICv3/v4架构定义了三种类型的中断，每种中断都有其特定的用途和行为特性。理解这些中断类型是掌握GIC的基础。

### 2.1.1 SGI (Software Generated Interrupt - 软件生成中断)

**定义**
SGI是通过软件指令触发的中断，主要用于CPU核心之间的通信。在多核系统中，一个核心可以通过写入特定的系统寄存器来向其他核心发送中断信号。

**特性**
- **中断ID**: 0-15（共16个）
- **触发方式**: 通过写入ICC_SGI0R或ICC_SGI1R寄存器
- **目标**: 可以路由到单个或多个目标核心
- **用途**:
  - 多核间同步
  - 任务间通信
  - 缓存一致性维护请求

**硬件原理**

```
Core 0 写入ICC_SGI0R
         │
         ▼
    ┌─────────┐
    │  GIC    │
    │ Distri- │
    │ butor   │
    │         │
    └────┬────┘
         │
    ┌────┴────┐
    │          │          │
 ▼──▼──▼  ▼──▼──▼  ▼──▼──▼──
│Core0 │  │Core1 │  │Core2 │
│ SGI  │  │ SGI  │  │ SGI  │
└──────┘  └──────┘  └──────┘
```

**SR6P3C4中的SGI**

```c
// irq_id.h 中SGI定义
typedef enum {
    IRQ_SGI_MIN_INT_NUMBER = 0,
    IRQ_SGI_0_INT_NUMBER = IRQ_SGI_MIN_INT_NUMBER,
    IRQ_SGI_1_INT_NUMBER,
    IRQ_SGI_2_INT_NUMBER,
    IRQ_SGI_3_INT_NUMBER,
    IRQ_SGI_4_INT_NUMBER,
    IRQ_SGI_5_INT_NUMBER,
    IRQ_SGI_6_INT_NUMBER,
    IRQ_SGI_7_INT_NUMBER,
    IRQ_SGI_8_INT_NUMBER,
    IRQ_SGI_9_INT_NUMBER,
    IRQ_SGI_10_INT_NUMBER,
    IRQ_SGI_11_INT_NUMBER,
    IRQ_SGI_12_INT_NUMBER,
    IRQ_SGI_13_INT_NUMBER,
    IRQ_SGI_14_INT_NUMBER,
    IRQ_SGI_15_INT_NUMBER,
    IRQ_SGI_MAX_INT_NUMBER = IRQ_SGI_15_INT_NUMBER,
} irq_id_t;
```

SDK中使用SGI的示例代码：

```c
// 生成SGI中断
void gic_generate_sgi(irq_cluster_t cluster, irq_core_t core,
                       irq_id_t int_id, irq_group_t group, uint32_t irm)
{
    uint64_t icc_sgi;

    // IRM = 1: 中断路由到所有核心（除自己外）
    if (irm == 1UL) {
        icc_sgi = (((uint64_t)int_id & 0xFUL) << 24UL) |
                  (((uint64_t)1UL) << 40U);
    } else {
        // IRM = 0: 路由到指定核心
        icc_sgi = (((uint64_t)int_id & 0xFUL) << 24UL) |
                  ((uint64_t)cluster << 16UL) |
                  ((uint64_t)1UL << (uint64_t)core);
    }

    // 根据分组写入不同的SGI寄存器
    if (group == IRQ_GROUP_FAST_IRQ) {
        arm_write_sysreg_64(CP15_ICC_SGI0R, icc_sgi);  // FIQ
    } else if (group == IRQ_GROUP_NORMAL_IRQ) {
        arm_write_sysreg_64(CP15_ICC_SGI1R, icc_sgi);  // IRQ
    }
}
```

### 2.1.2 PPI (Private Peripheral Interrupt - 私有外设中断)

**定义**
PPI是每个CPU核心私有共享的外设中断。虽然多个外设可以产生PPI，但每个核心都会收到一份独立的副本。PPI对于那些需要为每个核心提供独立中断信号的外设非常重要。

**特性**
- **中断ID**: 16-31（共16个）
- **私有性**: 每个核心独立接收
- **典型用途**:
  - 定时器中断
  - 性能监控中断
  - 虚拟化相关中断

**PPI中断分布图**

```
┌─────────────────────────────────────────────────────────────────┐
│                    PPI中断ID分配 (16-31)                       │
├─────────┬─────────┬─────────┬─────────┬───────────────────────┤
│  ID 16  │  ID 17  │  ID 18  │  ID 19  │        ...           │
│  SWT_0  │  SWT_1  │   ME    │  PPI_19 │                      │
├─────────┴─────────┴─────────┴─────────┴───────────────────────┤
│                    Reserved (20-31)                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Core 0: 独立接收所有PPI                                        │
│  Core 1: 独立接收所有PPI                                        │
│  Core 2: 独立接收所有PPI (如果有)                               │
│  Core 3: 独立接收所有PPI (如果有)                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**SR6P3C4中的PPI**

```c
// irq_id.h 中PPI定义
typedef enum {
    IRQ_PPI_MIN_INT_NUMBER = 16,
    IRQ_SWT_0_INT_NUMBER = IRQ_PPI_MIN_INT_NUMBER,  // ID 16: 系统看门狗0
    IRQ_SWT_1_INT_NUMBER,                            // ID 17: 系统看门狗1
    IRQ_ME_INT_NUMBER,                              // ID 18: 模式转换请求
    IRQ_PPI_19_INT_NUMBER,                          // ID 19
    IRQ_PPI_20_INT_NUMBER,                          // ID 20
    IRQ_PPI_21_INT_NUMBER,                          // ID 21
    IRQ_DEBUG_COMMS_CHANNEL_INT_NUMBER,              // ID 22: 调试通信
    IRQ_PERF_MON_CNT_OVL_INT_NUMBER,                // ID 23: 性能监控
    IRQ_CROSS_TRIGGER_IF_INT_NUMBER,                // ID 24: 交叉触发接口
    IRQ_VIRTUAL_CPU_IF_MAINT_INT_NUMBER,            // ID 25: 虚拟CPU维护
    IRQ_HYPER_TIMER_INT_NUMBER,                     // ID 26: 超级管理器定时器
    IRQ_VIRT_TIMER_INT_NUMBER,                      // ID 27: 虚拟定时器
    IRQ_PPI_28_INT_NUMBER,                          // ID 28
    IRQ_PPI_29_INT_NUMBER,                          // ID 29
    IRQ_PHYS_TIMER_INT_NUMBER,                      // ID 30: 物理定时器
    IRQ_PPI_31_INT_NUMBER,                          // ID 31
    IRQ_PPI_MAX_INT_NUMBER = IRQ_PPI_31_INT_NUMBER,
} irq_id_t;
```

**PPI配置示例**

```c
// 配置PPI中断
static void gic_configure_ppi(irq_id_t int_id, irq_group_t group,
                              irq_signal_t signal, uint32_t priority,
                              uint32_t core)
{
    uint8_t *prio_regs;
    GICR_t  *gicr;

    // 获取指定核心的GICR
    gicr = GICR_SGI_PPI(core);

    // 设置中断分组 (Group 0: FIQ, Group 1: IRQ)
    gicr->GICR_IGROUPR0.R =
        (gicr->GICR_IGROUPR0.R & ~(1UL << (uint32_t)int_id)) |
        ((uint32_t)group << (uint32_t)int_id);

    // 设置中断优先级 (只使用高5位)
    prio_regs = (uint8_t *)(&(gicr->GICR_IPRIORITYR[(uint32_t)int_id >> 2U]));
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));

    // 设置触发方式 (边沿触发或电平触发)
    gicr->GICR_ICFGR1.R =
        (gicr->GICR_ICFGR1.R & ~(3UL << (2U * ((uint32_t)int_id - 16U)))) |
        ((uint32_t)signal << (1U + 2U * ((uint32_t)int_id - 16U)));
}
```

### 2.1.3 SPI (Shared Peripheral Interrupt - 共享外设中断)

**定义**
SPI是系统中所有CPU核心共享的外设中断。这是最常用的中断类型，连接了大部分外设如UART、SPI、CAN等。SPI可以配置为路由到任何一个或一组CPU核心。

**特性**
- **中断ID**: 32-1019（可配置）
- **共享性**: 所有核心可见，但可独立路由
- **典型用途**:
  - 通信外设 (UART, SPI, I2C, CAN)
  - 存储控制器
  - 通用IO中断

**SPI中断分配图**

```
┌─────────────────────────────────────────────────────────────────┐
│                    SPI中断ID分配 (32+)                          │
├──────────┬──────────┬──────────┬──────────┬────────────────────┤
│  32-34   │   35-54  │   62-65  │   87-91  │       ...         │
│  SWT_SYS │   eDMA   │  SIUL2   │ Ethernet │    更多外设       │
├──────────┴──────────┴──────────┴──────────┴────────────────────┤
│                                                                 │
│  可配置路由到:                                                   │
│    - 单一Core (Core 0/1/2/3)                                   │
│    - 多个Core (Core 0+1, Core 1+2, 等)                        │
│    - 所有Core                                                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**SR6P3C4中的部分SPI中断**

```c
// irq_id.h 中部分SPI定义
typedef enum {
    // 系统看门狗 (系统级)
    IRQ_SWT_SYS_0_INT_NUMBER,    // ID 32
    IRQ_SWT_SYS_1_INT_NUMBER,    // ID 33
    IRQ_SWT_SYS_2_INT_NUMBER,    // ID 34

    // eDMA中断 (多个通道组)
    IRQ_EDMA_0_ERROR_FLAGS_0_63_INT_NUMBER,   // ID 35
    IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER,          // ID 36
    IRQ_EDMA_0_FLAGS_8_15_INT_NUMBER,         // ID 37
    // ... 更多eDMA通道

    // SIUL2 (GPIO外部中断)
    IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER,         // ID 62
    IRQ_SIUL2_0_EXT_INT_1_INT_NUMBER,         // ID 63
    IRQ_SIUL2_0_EXT_INT_2_INT_NUMBER,         // ID 64
    IRQ_SIUL2_0_EXT_INT_3_INT_NUMBER,         // ID 65

    // 以太网
    IRQ_ETHERNET_0_CORE_INT_NUMBER,            // ID 87
    IRQ_ETHERNET_0_PMT_LPI_INT_NUMBER,        // ID 88
    IRQ_ETHERNET_0_3_INT_NUMBER,              // ID 89

    // CAN
    IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER, // ID 172
    // ... 更多CAN中断

    // 继续到 ID ~660
} irq_id_t;
```

**SPI配置示例**

```c
// 配置SPI中断
static void gic_configure_spi(irq_id_t int_id, irq_group_t group,
                               irq_signal_t signal, uint32_t priority,
                               uint32_t core)
{
    uint8_t *prio_regs;

    // 1. 设置中断分组
    GICD.GICD_IGROUPR[(uint32_t)int_id >> 5U].B.GROUP_STATUS_BIT =
        (GICD.GICD_IGROUPR[(uint32_t)int_id >> 5U].B.GROUP_STATUS_BIT &
         ~(1UL << ((uint32_t)int_id & 0x1fU))) |
        ((uint32_t)group << ((uint32_t)int_id & 0x1fU));

    // 2. 设置中断优先级
    prio_regs = (uint8_t *)(&(GICD.GICD_IPRIORITYR[(uint32_t)int_id >> 2U]));
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));

    // 3. 配置目标核心 (Affinity Routing)
    // AFF0 = core, AFF1 = 0
    GICD.GICD_IROUTER[int_id].R = (uint64_t)core;

    // 4. 设置触发方式
    GICD.GICD_ICFGR[(uint32_t)int_id >> 4U].R =
        (GICD.GICD_ICFGR[(uint32_t)int_id >> 4U].R &
         ~(3UL << (2U * ((uint32_t)int_id & 0x1FU)))) |
        ((uint32_t)signal << (1U + 2U * ((uint32_t)int_id & 0xFU)));
}
```

---

## 2.2 中断类型判断函数

SDK中提供了便捷的中断类型判断函数：

```c
/**
 * 检查是否为SGI中断
 */
static inline uint32_t gic_is_sgi(irq_id_t int_id)
{
    return (int_id <= IRQ_SGI_MAX_INT_NUMBER) ? 1UL : 0UL;
}

/**
 * 检查是否为PPI中断
 */
static inline uint32_t gic_is_ppi(irq_id_t int_id)
{
    return ((int_id >= IRQ_PPI_MIN_INT_NUMBER) &&
            (int_id <= IRQ_PPI_MAX_INT_NUMBER)) ? 1UL : 0UL;
}

/**
 * 检查是否为SPI中断
 */
static inline uint32_t gic_is_spi(irq_id_t int_id)
{
    return (int_id > IRQ_PPI_MAX_INT_NUMBER) ? 1UL : 0UL;
}
```

---

## 2.3 中断ID汇总表

| 类型 | ID范围 | 数量 | 作用范围 | 典型用途 |
|------|--------|------|----------|----------|
| **SGI** | 0-15 | 16 | 核心间通信 | 多核同步、缓存刷新 |
| **PPI** | 16-31 | 16 | 每核心私有 | 定时器、性能监控 |
| **SPI** | 32+ | 可配置 | 共享/可路由 | 外设中断(UART/CAN等) |

---

## 中断ID分布图

```
+=========================================================================+
|                    SR6P3C4 中断ID分配分布图                              |
+=========================================================================+
|                                                                         |
|  ID范围      |  类型   | 数量  |  说明                              |
|-------------|---------|-------|-----------------------------------|
|  0  ~  15   |  SGI   |  16   |  软件生成中断(核心间通信)          |
|             |         |       |                                   |
|  16  ~  31  |  PPI   |  16   |  私有外设中断(每核心独立)          |
|             |         |       |                                   |
|  32  ~  ~   |  SPI   | 可配置|  共享外设中断(可路由)             |
|             |         |       |                                   |
+=========================================================================+

+=========================================================================+
|                         详细分布                                        |
+=========================================================================+

  SGI (0-15):  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
               | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 |12 |13 |14 |15|
               +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

  PPI (16-31): +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
               |16 |17 |18 |19 |20 |21 |22 |23 |24 |25 |26 |27 |28 |29 |30 |31|
               +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
                 |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
                 |   |   |   |   |   |   |   |   |   |   |   |   |   |   +- 虚
                 |   |   |   |   |   |   |   |   |   |   |   |   |   +--- 虚
                 |   |   |   |   |   |   |   |   |   |   |   |   +------- 物
                 |   |   |   |   |   |   |   |   |   |   |   +----------- 定
                 |   |   |   |   |   |   |   |   |   |   +------------- 时
                 |   |   |   |   |   |   |   |   |   +----------------- 虚
                 |   |   |   |   |   |   |   |   +--------------------- 超
                 |   |   |   |   |   |   |   +----------------------- 虚
                 |   |   |   |   |   |   +--------------------------- 性
                 |   |   |   |   |   +------------------------------- 能
                 |   |   |   |   +----------------------------------- 监
                 |   |   |   +--------------------------------------- 控
                 |   |   +----------------------------------------------- 通
                 |   +----------------------------------------------- 道

  SPI (32+):   +---+---+---+---+---+---+---+---+---+---+---+---+-- ~ --+
               |32 |33 |34 |...| 62| 63| 64| 65|...| 87|...|172|...|660|
               +---+---+---+---+---+---+---+---+---+---+---+---+-- ~ --+

               常用SPI中断速查:
               +------+------+------+------+--------+------+------+
               | 32-34| 36-54 | 62-65 | 87-91 | 102-117| 136+ | 172+ |
               +------+------+------+------+--------+------+------+
               |SWT_SYS|eDMA  | SIUL2 |Ethernet| Linflex| DSPI | CAN |
               +------+------+------+------+--------+------+------+

+=========================================================================+
```

---

## 本章小结

本章上半部分详细介绍了GIC的三种中断类型：

1. **SGI (0-15)**
   - 软件触发，用于核心间通信
   - 通过ICC_SGI0R/ICC_SGI1R寄存器生成

2. **PPI (16-31)**
   - 每核心私有的外设中断
   - 典型如定时器、虚拟化中断

3. **SPI (32+)**
   - 共享外设中断，可灵活路由
   - 覆盖绝大多数外设中断

每种中断类型都有对应的配置API，在后续章节中将详细讲解配置方法。

---

## 下章预告

第二章下半部分将介绍：
- 中断优先级机制与配置方法
- 中断分组 (Group0/Group1) 与 FIQ/IRQ映射
- Affinity Routing多核路由机制

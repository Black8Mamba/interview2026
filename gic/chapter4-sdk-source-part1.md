# 第四章：SDK中断框架源码解析 (上)

## 4.1 SDK中断框架概述

Stellar SDK为SR6P3C4提供了一套完整的中断管理框架，封装了GICv3的复杂操作，提供了简洁易用的API。本章将逐行解析这些源码，帮助读者深入理解中断机制的实现细节。

### 框架结构

```
┌─────────────────────────────────────────────────────────────────┐
│                    SDK 中断框架调用层次                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │               用户代码 (Application)                        ││
│  │    - irq_config() 配置中断                                 ││
│  │    - irq_enable() / irq_disable() 使能/禁用              ││
│  │    - ISR中断处理函数                                        ││
│  └─────────────────────────────────────────────────────────────┘│
│                              │                                    │
│                              ▼                                    │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │               irq.h (API层)                                ││
│  │    irq_config()                                            ││
│  │    irq_enable()                                            ││
│  │    irq_disable()                                           ││
│  │    irq_clear()                                             ││
│  │    irq_notify()                                            ││
│  └─────────────────────────────────────────────────────────────┘│
│                              │                                    │
│                              ▼                                    │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │               intc.c (驱动层)                              ││
│  │    gic_init()          - GIC初始化                         ││
│  │    gic_config_sgi/ppi/spi() - 具体配置                    ││
│  │    gic_enable_sgi/ppi/spi()  - 具体使能                    ││
│  │    gic_generate_sgi()    - SGI生成                          ││
│  └─────────────────────────────────────────────────────────────┘│
│                              │                                    │
│                              ▼                                    │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │               ibcm.c (跨集群)                               ││
│  │    ibcm_notify()        - IBCM跨集群中断                   ││
│  │    ibcm_init()          - IBCM初始化                       ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4.2 irq.h API接口详解

### 文件位置与结构

```c
// 文件: arch/sr6x/interrupt/include/irq.h
// 主要内容:
// 1. 中断相关类型定义 (irq_cluster_t, irq_core_t, irq_group_t等)
// 2. 中断配置API声明
// 3. ISR处理函数宏定义
```

### 核心类型定义

#### irq_cluster_t - 簇类型

```c
/** @brief Cluster type. */
typedef enum {
    IRQ_CLUSTER_0,   /**< Cluster 0 id */
    IRQ_CLUSTER_1,   /**< Cluster 1 id */
    IRQ_CLUSTER_2,   /**< Cluster 2 id */
    IRQ_CLUSTER_PER, /**< Cluster peripheral id */
    IRQ_CLUSTER_NONE /**< No cluster */
} irq_cluster_t;
```

**说明:**
- SR6P3C4有3个计算Cluster(0,1,2)和1个外设Cluster(3)
- IRQ_CLUSTER_PER用于DME/DSPH等特殊核心

#### irq_core_t - 核心类型

```c
/** @brief Core type. */
typedef enum {
    IRQ_CORE_0,                /**< Core 0 */
    IRQ_CORE_1,                /**< Core 1 */
    IRQ_CORE_2,                /**< Core 2 */
    IRQ_CORE_3,                /**< Core 3 */
    IRQ_CORE_DME = IRQ_CORE_0, /**< Alias to identify DME */
    IRQ_CORE_DSPH = IRQ_CORE_1 /**< Alias to identify DSPH */
} irq_core_t;
```

**说明:**
- 每个Cluster最多4个核心
- DME(Direct Memory Engine)和DSPH是特殊核心

#### irq_group_t - 中断分组

```c
/** @brief Interrupt type. */
typedef enum {
    IRQ_GROUP_FAST_IRQ,   /**< Fast interrupt (映射为FIQ) */
    IRQ_GROUP_NORMAL_IRQ, /**< Normal interrupt (映射为IRQ) */
    IRQ_GROUP_DONT_CARE   /**< Interrupt type don't care */
} irq_group_t;
```

#### irq_signal_t - 触发类型

```c
/** @brief Interrupt level type. */
typedef enum {
    IRQ_LEVEL_SENSITIVE, /**< Level sensitive interrupt level type */
    IRQ_EDGE_TRIGGERED,  /**< Edge triggered interrupt level type */
    IRQ_SIGNAL_DONT_CARE /**< Interrupt level type don't care */
} irq_signal_t;
```

#### irq_trigout_t - 触发输出

```c
/** @brief Trigger output type. */
typedef enum {
    IRQ_TRIGOUT_0,   /**< Trigger output interrupt 0 */
    IRQ_TRIGOUT_1,   /**< Trigger output interrupt 1 */
    IRQ_TRIGOUT_2,   /**< Trigger output interrupt 2 */
    IRQ_TRIGOUT_3,   /**< Trigger output interrupt 3 */
    IRQ_TRIGOUT_4,   /**< Trigger output interrupt 4 */
    IRQ_TRIGOUT_5,   /**< Trigger output interrupt 5 */
    IRQ_TRIGOUT_6,   /**< Trigger output interrupt 6 */
    IRQ_TRIGOUT_7,   /**< Trigger output interrupt 7 */
    IRQ_TRIGOUT_8,   /**< Trigger output interrupt 8 */
    IRQ_TRIGOUT_9,   /**< Trigger output interrupt 9 */
    IRQ_TRIGOUT_10,  /**< Trigger output interrupt 10 */
    IRQ_TRIGOUT_11,  /**< Trigger output interrupt 11 */
    IRQ_TRIGOUT_12,  /**< Trigger output interrupt 12 */
    IRQ_TRIGOUT_13,  /**< Trigger output interrupt 13 */
    IRQ_TRIGOUT_14,  /**< Trigger output interrupt 14 */
    IRQ_TRIGOUT_15,  /**< Trigger output interrupt 15 */
    IRQ_TRIGOUT_16,  /**< Trigger output interrupt 16 */
    IRQ_TRIGOUT_17,  /**< Trigger output interrupt 17 */
    IRQ_TRIGOUT_18,  /**< Trigger output interrupt 18 */
    IRQ_TRIGOUT_19,  /**< Trigger output interrupt 19 */
    IRQ_TRIGOUT_20,  /**< Trigger output interrupt 20 */
    IRQ_TRIGOUT_21,  /**< Trigger output interrupt 21 */
    IRQ_TRIGOUT_22,  /**< Trigger output interrupt 22 */
    IRQ_TRIGOUT_23,  /**< Trigger output interrupt 23 */
    IRQ_TRIGOUT_24,  /**< Trigger output interrupt 24 */
    IRQ_TRIGOUT_25,  /**< Trigger output interrupt 25 */
    IRQ_TRIGOUT_26,  /**< Trigger output interrupt 26 */
    IRQ_TRIGOUT_27,  /**< Trigger output interrupt 27 */
    IRQ_TRIGOUT_28,  /**< Trigger output interrupt 28 */
    IRQ_TRIGOUT_29,  /**< Trigger output interrupt 29 */
    IRQ_TRIGOUT_30,  /**< Trigger output interrupt 30 */
    IRQ_TRIGOUT_31   /**< Trigger output interrupt 31 */
} irq_trigout_t;
```

### 核心API函数

#### irq_init() - 中断系统初始化

```c
/**
 * @brief   Initialize IRQ subsystem
 *
 * @note    This function initializes the IRQ subsystem. It must be called
 *          before any other IRQ function.
 *
 * @param   none
 *
 * @return  none
 */
void irq_init(void);
```

#### irq_config() - 配置中断

```c
/**
 * @brief   Configure an interrupt
 *
 * @param   int_id     interrupt id to be configured
 * @param   group      interrupt group (0 = FIQ, 1 = IRQ)
 * @param   signal     interrupt signal type (edge or level)
 * @param   priority   interrupt priority (0-31)
 *
 * @return  none
 */
void irq_config(irq_id_t int_id, irq_group_t group,
                irq_signal_t signal, uint32_t priority);
```

#### irq_enable() / irq_disable() - 使能/禁用中断

```c
/**
 * @brief   Enable an interrupt
 *
 * @param   int_id     interrupt id to be enabled
 *
 * @return  none
 */
void irq_enable(irq_id_t int_id);

/**
 * @brief   Disable an interrupt
 *
 * @param   int_id     interrupt id to be disabled
 *
 * @return  none
 */
void irq_disable(irq_id_t int_id);
```

#### irq_notify() - 产生中断

```c
/**
 * @brief   Generate a software interrupt to another core/cluster
 *
 * @param   cluster    target cluster
 * @param   core       target core
 * @param   int_id     interrupt id to generate
 *
 * @return  none
 */
void irq_notify(irq_cluster_t cluster, irq_core_t core, irq_id_t int_id);
```

#### irq_clear() - 清除中断

```c
/**
 * @brief   Clear an interrupt
 *
 * @param   cluster    source cluster
 * @param   core       source core
 * @param   int_id     interrupt id to clear
 *
 * @return  none
 */
void irq_clear(irq_cluster_t cluster, irq_core_t core, irq_id_t int_id);
```

### ISR处理函数宏

```c
/**
 * @brief   IRQ handler function declaration.
 * @par Description
 * @details
 * This macro hides the details of an ISR function declaration.
 *
 * @param[in] id        a vector name as defined in @p int_id.h
 */
#define IRQ_HANDLER(id) void __attribute__((section(".handlers"))) id(void)
```

**使用示例:**

```c
// 声明一个UART中断处理函数
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    // 中断处理代码
    // ...

    // 中断处理完成
}
```

---

## 4.3 irq_id.h 中断号定义体系

### 文件位置

```c
// 文件: arch/sr6x/interrupt/include/R52/sr6p3/irq_id.h
// 为SR6P3C4芯片定义所有可用的中断ID
```

### 中断ID枚举定义

SDK使用枚举类型定义了所有可用的中断号，这是使用中断时的主要参考：

```c
typedef enum {
    /* SGI: 0-15 */
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

    /* PPI: 16-31 */
    IRQ_PPI_MIN_INT_NUMBER = 16,
    IRQ_SWT_0_INT_NUMBER = IRQ_PPI_MIN_INT_NUMBER,  // 16
    IRQ_SWT_1_INT_NUMBER,                           // 17
    IRQ_ME_INT_NUMBER,                              // 18
    IRQ_PPI_19_INT_NUMBER,                          // 19
    IRQ_PPI_20_INT_NUMBER,                          // 20
    IRQ_PPI_21_INT_NUMBER,                          // 21
    IRQ_DEBUG_COMMS_CHANNEL_INT_NUMBER,             // 22
    IRQ_PERF_MON_CNT_OVL_INT_NUMBER,                // 23
    IRQ_CROSS_TRIGGER_IF_INT_NUMBER,                // 24
    IRQ_VIRTUAL_CPU_IF_MAINT_INT_NUMBER,            // 25
    IRQ_HYPER_TIMER_INT_NUMBER,                     // 26
    IRQ_VIRT_TIMER_INT_NUMBER,                      // 27
    IRQ_PPI_28_INT_NUMBER,                          // 28
    IRQ_PPI_29_INT_NUMBER,                          // 29
    IRQ_PHYS_TIMER_INT_NUMBER,                      // 30
    IRQ_PPI_31_INT_NUMBER,                          // 31
    IRQ_PPI_MAX_INT_NUMBER = IRQ_PPI_31_INT_NUMBER,

    /* SPI: 32+ */
    IRQ_SWT_SYS_0_INT_NUMBER,   // 32
    IRQ_SWT_SYS_1_INT_NUMBER,   // 33
    IRQ_SWT_SYS_2_INT_NUMBER,   // 34
    IRQ_EDMA_0_ERROR_FLAGS_0_63_INT_NUMBER, // 35
    // ... 继续到约660
} irq_id_t;
```

### 中断处理函数映射

除了ID枚举，每个中断还有一个对应的处理函数名映射：

```c
/* SGI 中断处理函数映射 */
#define IRQ_SGI_0_INT_HANDLER    vector0
#define IRQ_SGI_1_INT_HANDLER    vector1
#define IRQ_SGI_2_INT_HANDLER    vector2
// ... 到 vector15

/* PPI 中断处理函数映射 */
#define IRQ_SWT_0_INT_HANDLER    vector16
#define IRQ_SWT_1_INT_HANDLER    vector17
#define IRQ_ME_INT_HANDLER       vector18
// ... 到 vector31

/* SPI 中断处理函数映射 */
#define IRQ_SIUL2_0_EXT_INT_0_INT_HANDLER   vector62
#define IRQ_SIUL2_0_EXT_INT_1_INT_HANDLER   vector63
// ...
```

### 常用外设中断速查表

```c
// =============================================================
//                     常用外设中断速查表
// =============================================================

// GPIO (SIUL2) - 外部中断
IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER,  // ID 62
IRQ_SIUL2_0_EXT_INT_1_INT_NUMBER,  // ID 63
IRQ_SIUL2_0_EXT_INT_2_INT_NUMBER,  // ID 64
IRQ_SIUL2_0_EXT_INT_3_INT_NUMBER,  // ID 65

// eDMA
IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER,   // ID 36
IRQ_EDMA_0_FLAGS_8_15_INT_NUMBER,  // ID 37

// 以太网
IRQ_ETHERNET_0_CORE_INT_NUMBER,    // ID 87

// CAN
IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER,  // ID 172

// UART (Linflex)
IRQ_LINFLEX_0_INT_NUMBER,  // ID 102
IRQ_LINFLEX_1_INT_NUMBER,  // ID 103

// SPI
IRQ_DSPI_6_INT_NUMBER,    // ID 136
IRQ_DSPI_7_INT_NUMBER,    // ID 137

// I2C
IRQ_I2C_0_INT_NUMBER,     // ID 140
IRQ_I2C_1_INT_NUMBER,     // ID 141
```

---

## SDK中断框架调用关系图

```
+=========================================================================+
|                    SDK 中断框架调用层次                                   |
+=========================================================================+

                              用户代码
                                 |
          +----------------------+----------------------+
          |                      |                      |
          v                      v                      v
    +----------+          +----------+          +----------+
    | 配置中断  |          | 使能中断  |          | 产生中断  |
    |irq_config|          |irq_enable|          |irq_notify|
    +----+-----+          +----+-----+          +----+-----+
         |                      |                      |
         +----------------------+----------------------+
                                |
                                v
+=========================================================================+
|                         API 层 (irq.h)                                 |
+=========================================================================+
                                |
                                v
    +------------------------------------------------------------------------+
    |                                                                      |
    |   irq_config()    irq_enable()    irq_disable()   irq_clear()     |
    |   irq_notify()    irq_get_ppi()   irq_route_add()                  |
    |                                                                      |
    +------------------------------------------------------------------------+
                                |
                                v
+=========================================================================+
|                       驱动 层 (intc.c)                                  |
+=========================================================================+
                                |
    +-----------+-----------+-----------+-----------+-----------+
    |           |           |           |           |           |
    v           v           v           v           v           v
+-------+  +-------+  +-------+  +-------+  +-------+  +-------+
|gic_   |  |gic_   |  |gic_   |  |gic_   |  |gic_   |  |gic_  |
|init() |  |conf_  |  |conf_  |  |conf_  |  |enable_|  |gener |
|       |  |sgi()  |  |ppi()  |  |spi()  |  |spi()  |  |ate() |
+-------+  +-------+  +-------+  +-------+  +-------+  +-------+
    |           |           |           |           |           |
    +-----------+-----------+-----------+-----------+-----------+
                                |
                                v
+=========================================================================+
|                       硬件抽象层 (ibcm.c)                               |
+=========================================================================+
                                |
                                v
                         +-------------+
                         | ibcm_init() |
                         | ibcm_notify()|
                         +-------------+

+=========================================================================+
|                       核心数据结构                                      |
+=========================================================================+

    irq_id_t          - 中断号枚举 (0-660+)
    irq_cluster_t     - 簇ID (CLUSTER_0/1/2/PER)
    irq_core_t        - 核心ID (CORE_0/1/2/3)
    irq_group_t       - 中断分组 (FAST_IRQ/NORMAL_IRQ)
    irq_signal_t      - 触发类型 (LEVEL_SENSITIVE/EDGE_TRIGGERED)
    irq_trigout_t     - 触发输出 (TRIGOUT_0-31)

+=========================================================================+
```

---

## 本章小结

本章上半部分介绍了SDK中断框架的上层API：

1. **irq.h API接口**
   - 类型定义: irq_cluster_t, irq_core_t, irq_group_t, irq_signal_t
   - 核心函数: irq_init(), irq_config(), irq_enable(), irq_disable()
   - 特殊函数: irq_notify() (跨集群), irq_clear()

2. **irq_id.h 中断号体系**
   - 完整的枚举定义 (SGI/PPI/SPI)
   - 中断处理函数映射宏
   - 常用外设中断速查

---

## 下章预告

第四章中部分将继续解析：
- intc.c中的gic_init()初始化函数逐行详解
- GIC硬件初始化流程
- 系统寄存器配置

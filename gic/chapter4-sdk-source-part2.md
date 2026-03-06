# 第四章：SDK中断框架源码解析 (中)

## 4.4 intc.c 核心驱动代码逐行解析

### 文件概述

```c
// 文件: arch/sr6x/interrupt/src/R52/intc.c
// 主要功能:
// 1. GIC硬件初始化
// 2. SGI/PPI/SPI中断配置
// 3. 中断使能/禁用/清除
// 4. 跨集群中断支持
```

### 头文件与宏定义

```c
#include <core.h>
#include <platform.h>
#include <irq.h>
#include <ibcm.h>

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

// GIC Distributor基地址
#define GICD  CORTEXR52_GICD_INTERFACE

// 各核心的GIC Redistributor控制寄存器
#define GICR_CTRL_CPU0    CORTEXR52_GICR_CTRL_CPU0
#define GICR_SGI_PPI_CPU0 CORTEXR52_GICR_SGI_PPI_CPU0

#define GICR_CTRL_CPU1    CORTEXR52_GICR_CTRL_CPU1
#define GICR_SGI_PPI_CPU1 CORTEXR52_GICR_SGI_PPI_CPU1

#ifdef CORTEXR52_GICR_CTRL_CPU2
#define GICR_CTRL_CPU2    CORTEXR52_GICR_CTRL_CPU2
#define GICR_SGI_PPI_CPU2 CORTEXR52_GICR_SGI_PPI_CPU2
#endif

#ifdef CORTEXR52_GICR_CTRL_CPU3
#define GICR_CTRL_CPU3    CORTEXR52_GICR_CTRL_CPU3
#define GICR_SGI_PPI_CPU3 CORTEXR52_GICR_SGI_PPI_CPU3
#endif
```

### 局部变量定义

```c
/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

// 定义GICR结构体类型别名
typedef volatile struct GIC_REDISTRIBUTER_SGI_PPI_MM_tag GICR_t;

// 存储各核心GICR指针的数组
#ifdef CORTEXR52_CORE_PER_CLUSTER
static GICR_t *GICR_tags[CORTEXR52_CORE_PER_CLUSTER];
#else
// 默认支持2个核心
static GICR_t *GICR_tags[2];
#endif
```

---

## 4.5 gic_init() 初始化函数详解

### 函数整体结构

```c
/**
 * @brief   Initialize GIC module
 *
 * @noapi
 */
static void gic_init(void)
{
    uint32_t icc_pmr;  // 局部变量：优先级掩码

    /*===========================================================
     * 第一部分: GICD配置 (Distributor)
     *=========================================================*/

    /* GICD configuration - At cluster level */
    /* each cluster can only see their own GIC */

    /* 1. 启用Affinity Routing */
    GICD.GICD_CTLR.B.ARE = 1;
    while (GICD.GICD_CTLR.B.RWP != 0U) {  // 等待写入完成
        ;
    }

    /* 2. 启用Group 0中断 (FIQ) */
    GICD.GICD_CTLR.B.ENABLEGRP0 = 1;
    while (GICD.GICD_CTLR.B.RWP != 0U) {
        ;
    }

    /* 3. 启用Group 1中断 (IRQ) */
    GICD.GICD_CTLR.B.ENABLEGRP1 = 1;
    while (GICD.GICD_CTLR.B.RWP != 0U) {
        ;
    }

    /*===========================================================
     * 第二部分: GICR配置 (Redistributor)
     *=========================================================*/

    /* GICR configuration depends on the core */
    switch (get_core_id()) {
      case 0U:
        // 唤醒Core 0的Redistributor
        GICR_CTRL_CPU0.GICR_WAKER.B.PROCESSORSLEEP = 0;
        while (GICR_CTRL_CPU0.GICR_WAKER.B.CHILDRENASLEEP != 0U) {
            ;
        }
      break;

      case 1U:
        // 唤醒Core 1的Redistributor
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

    /*===========================================================
     * 第三部分: CPU Interface配置
     *=========================================================*/

    /* Kite core specific initialization */
    /* we configure the core to accept all priorities */

    // 配置优先级掩码 - 允许所有优先级
    icc_pmr = 0xF8;  // 0xF8 = 11111000b, 只使用高5位
    arm_write_sysreg(CP15_ICC_PMR, icc_pmr);

    // 启用Group 0中断 (映射为FIQ)
    arm_write_sysreg(CP15_ICC_IGRPEN0, 1);

    // 启用Group 1中断 (映射为IRQ)
    arm_write_sysreg(CP15_ICC_IGRPEN1, 1);

    /* Binary Point Register配置 */
    /* check as 2 minimum allowed as mentioned in Cortex-R52 TRM */
    arm_write_sysreg(CP15_ICC_BPR0, 0);  // Group 0: 无分割，最精细优先级
    arm_write_sysreg(CP15_ICC_BPR1, 1);  // Group 1: 1位分割

    (void)icc_pmr;  // 消除未使用变量警告
}
```

### 初始化流程图

```
┌─────────────────────────────────────────────────────────────────┐
│                       GIC 初始化流程                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. 初始化入口                                                  │
│     irq_init() ──────────────────────────────────┐            │
│                                                    │            │
│     ├─► ibcm_init()                               │            │
│     │   (跨集群模块初始化)                         │            │
│     │                                            ▼            │
│     ├─► GICR_tags[0] = &GICR_SGI_PPI_CPU0        │            │
│     ├─► GICR_tags[1] = &GICR_SGI_PPI_CPU1        │            │
│     │   (保存各核心GICR指针)                       │            │
│     │                                            │            │
│     └─► gic_init() ◄─────────────────────────────┘            │
│                                                                 │
│  2. GICD配置                                                    │
│     ┌────────────────────────────────────────────┐             │
│     │ GICD_CTLR.ARE = 1                          │             │
│     │   启用Affinity Routing                     │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│                ▼                                                   │
│     ┌────────────────────────────────────────────┐             │
│     │ GICD_CTLR.ENABLEGRP0 = 1                   │             │
│     │   启用Group 0 (FIQ)                         │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│                ▼                                                   │
│     ┌────────────────────────────────────────────┐             │
│     │ GICD_CTLR.ENABLEGRP1 = 1                   │             │
│     │   启用Group 1 (IRQ)                         │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│  3. GICR配置    ▼                                                   │
│     ┌────────────────────────────────────────────┐             │
│     │ 根据core_id选择对应Redistributor           │             │
│     │                                           │             │
│     │ Core 0: GICR_WAKER.PROCESSORSLEEP = 0    │             │
│     │ Core 1: GICR_WAKER.PROCESSORSLEEP = 0    │             │
│     │ Core 2: GICR_WAKER.PROCESSORSLEEP = 0    │             │
│     │ Core 3: GICR_WAKER.PROCESSORSLEEP = 0    │             │
│     │                                           │             │
│     │ 等待 CHILDRENASLEEP = 0                   │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│  4. CPU Interface配置                                           │
│                ▼                                                   │
│     ┌────────────────────────────────────────────┐             │
│     │ ICC_PMR = 0xF8                              │             │
│     │   允许所有优先级中断                         │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│                ▼                                                   │
│     ┌────────────────────────────────────────────┐             │
│     │ ICC_IGRPEN0 = 1                             │             │
│     │ ICC_IGRPEN1 = 1                             │             │
│     │   启用Group 0和Group 1                      │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│                ▼                                                   │
│     ┌────────────────────────────────────────────┐             │
│     │ ICC_BPR0 = 0, ICC_BPR1 = 1                │             │
│     │   配置优先级分组                            │             │
│     └──────────┬─────────────────────────────────┘             │
│                │                                                   │
│                ▼                                                   │
│          初始化完成                                              │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 各配置阶段详解

#### 阶段1: Affinity Routing启用

```c
/* Affinity Routing Enable */
GICD.GICD_CTLR.B.ARE = 1;
while (GICD.GICD_CTLR.B.RWP != 0U) {
    ;
}
```

**为什么要等待RWP位?**

`RWP (Register Write Pending)` 位指示GIC是否正在处理之前的配置写入。在向GICD_CTLR写入关键配置后，必须等待RWP清零才能继续下一个配置，否则可能导致配置丢失或不确定行为。

**ARE位的作用:**

```
ARE = 0 (Legacy模式):
┌─────────────────────────────────────────────────┐
│  SGI/PPI: 固定路由到当前核心                    │
│  SPI:    固定路由到所有核心或单个核心           │
│  不支持Affinity格式                             │
└─────────────────────────────────────────────────┘

ARE = 1 (Affinity Routing模式):
┌─────────────────────────────────────────────────┐
│  SGI/PPI: 通过Affinity路由                      │
│  SPI:    通过GICD_IROUTER灵活路由               │
│  支持多Cluster多核心                            │
└─────────────────────────────────────────────────┘
```

#### 阶段2: Redistributor唤醒

```c
case 0U:
    GICR_CTRL_CPU0.GICR_WAKER.B.PROCESSORSLEEP = 0;
    while (GICR_CTRL_CPU0.GICR_WAKER.B.CHILDRENASLEEP != 0U) {
        ;
    }
break;
```

**为什么需要唤醒?**

默认情况下，Redistributor处于休眠状态以节省功耗。唤醒后，GIC才能向对应核心分发中断。

```
Redistributor休眠状态:
┌─────────────────────────────────────────────────┐
│  PROCESSORSLEEP = 1: 核心希望休眠                │
│  CHILDRENASLEEP = 1: Redistributor处于休眠       │
│                                              │
│  效果: 中断不会分发到此核心                      │
└─────────────────────────────────────────────────┘

Redistributor唤醒后:
┌─────────────────────────────────────────────────┐
│  PROCESSORSLEEP = 0: 核心已唤醒                  │
│  CHILDRENASLEEP = 0: Redistributor已就绪         │
│                                              │
│  效果: 中断可以正常分发                          │
└─────────────────────────────────────────────────┘
```

#### 阶段3: CPU Interface配置

```c
icc_pmr = 0xF8;
arm_write_sysreg(CP15_ICC_PMR, icc_pmr);
arm_write_sysreg(CP15_ICC_IGRPEN0, 1);
arm_write_sysreg(CP15_ICC_IGRPEN1, 1);
arm_write_sysreg(CP15_ICC_BPR0, 0);
arm_write_sysreg(CP15_ICC_BPR1, 1);
```

**优先级掩码详解:**

```
ICC_PMR = 0xF8 = 11111000b

bit[7:5] = 111 (优先级7，高5位有效)
bit[4:0] = 00000 (保留)

允许的中断优先级: 0, 8, 16, 24, 32, 40, 48, 56 (8个级别)

如果需要更精细的控制:
ICC_PMR = 0xE0 允许 16个级别 (0, 16, 32, ...)
ICC_PMR = 0xC0 允许 32个级别 (0, 8, 16, ...)
ICC_PMR = 0x00 允许 256个级别 (0-255)
```

---

## 4.6 irq_init() 主入口函数

```c
/**
 * @brief   Initialize IRQ subsystem
 */
void irq_init(void)
{
    // 1. 初始化IBCM模块 (跨集群中断)
    ibcm_init();

    // 2. 初始化各核心的GICR指针
    GICR_tags[0] = &GICR_SGI_PPI_CPU0;
    GICR_tags[1] = &GICR_SGI_PPI_CPU1;

#ifdef GICR_CTRL_CPU2
    GICR_tags[2] = &GICR_SGI_PPI_CPU2;
#endif

#ifdef GICR_CTRL_CPU3
    GICR_tags[3] = &GICR_SGI_PPI_CPU3;
#endif

    // 3. 执行GIC初始化
    gic_init();
}
```

---

## GIC初始化流程图

```
+=========================================================================+
|                       GIC 初始化完整流程                                  |
+=========================================================================+

                                    开始
                                      |
                                      v
                         +--------------------+
                         |   irq_init()      |
                         +--------+---------+
                                  |
                                  v
                    +-----------------------------+
                    |     ibcm_init()            |  跨集群模块初始化
                    +--------+-------------------+
                                  |
                                  v
                    +-----------------------------+
                    |  初始化 GICR_tags[]       |
                    |  GICR_tags[0] = CPU0     |
                    |  GICR_tags[1] = CPU1     |
                    |  GICR_tags[2] = CPU2     |  (如果存在)
                    |  GICR_tags[3] = CPU3     |  (如果存在)
                    +--------+-------------------+
                                  |
                                  v
                    +-----------------------------+
                    |      gic_init()           |  GIC核心初始化
                    +--------+-------------------+
                                  |
            +---------------------+---------------------+
            |                     |                     |
            v                     v                     v
    +---------------+    +---------------+    +---------------+
    |  1. GICD配置  |    |  2. GICR配置  |    |  3. CPU IF   |
    +-------+-------+    +-------+-------+    +-------+-------+
            |                     |                     |
            v                     v                     v
    +---------------+    +---------------+    +---------------+
    | ARE=1         |    | WAKER=0      |    | ICC_PMR=0xF8 |
    | EnableGrp0=1  |    | (唤醒Redist)  |    | IGRPEN0=1   |
    | EnableGrp1=1  |    +---------------+    | IGRPEN1=1   |
    +---------------+            |             | BPR0=0, BPR1=1|
                                 |             +---------------+
                                 v                      |
    +---------------+            |                      |
    | 等待RWP=0     |            v                      |
    +---------------+    +---------------+            |
                           | 等待CHILDREN           |
                           | _ASLEEP=0              |
                           +---------------+            |
                                 |                      |
                                 +----------+-----------+
                                            |
                                            v
                                      初始化完成
                                            |
                                            v
                                      irq_init()返回

+=========================================================================+
|                       关键配置步骤详解                                   |
+=========================================================================+

  [步骤1] GICD配置:
  +------------------------------------------------------------------+
  |  GICD_CTLR.ARE = 1          启用Affinity Routing               |
  |  GICD_CTLR.EnableGrp0 = 1   启用Group 0中断 (FIQ)             |
  |  GICD_CTLR.EnableGrp1 = 1   启用Group 1中断 (IRQ)             |
  +------------------------------------------------------------------+

  [步骤2] GICR配置:
  +------------------------------------------------------------------+
  |  GICR_WAKER.ProcessorSleep = 0   唤醒Redistributor            |
  |  等待 ChildrenAsleep = 0          确保唤醒完成                  |
  +------------------------------------------------------------------+

  [步骤3] CPU Interface配置:
  +------------------------------------------------------------------+
  |  ICC_PMR = 0xF8        允许所有优先级的中断                   |
  |  ICC_IGRPEN0 = 1       使能Group 0 (FIQ)                     |
  |  ICC_IGRPEN1 = 1       使能Group 1 (IRQ)                     |
  |  ICC_BPR0 = 0          Group 0优先级分组(最精细)              |
  |  ICC_BPR1 = 1          Group 1优先级分组                       |
  +------------------------------------------------------------------+

+=========================================================================+
```

---

## 本章小结

本章中部分详细解析了GIC初始化过程：

1. **GICD配置**
   - 启用Affinity Routing
   - 启用Group 0/1中断
   - RWP等待确保配置生效

2. **GICR配置**
   - 根据core_id唤醒对应Redistributor
   - 等待唤醒完成

3. **CPU Interface配置**
   - ICC_PMR优先级掩码
   - ICC_IGRPEN0/1分组使能
   - ICC_BPR0/1优先级分组

---

## 下章预告

第四章下部分将继续解析：
- SGI/PPI/SPI配置函数详解
- 中断使能/禁用函数
- 跨集群中断生成函数
- 中断类型判断函数

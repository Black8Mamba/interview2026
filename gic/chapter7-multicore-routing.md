# 第七章：多核中断路由

## 7.1 SR6P3C4多核架构

### 核心与Cluster分布

SR6P3C4采用多Cluster多核心架构，了解其结构是配置多核中断的基础：

```
┌─────────────────────────────────────────────────────────────────┐
│                   SR6P3C4 多核架构                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Cluster 0          Cluster 1          Cluster 2     Cluster 3 │
│  ┌─────┐           ┌─────┐           ┌─────┐      ┌─────┐   │
│  │Core0│           │Core0│           │Core0│      │ DME │   │
│  │ R52 │           │ R52 │           │ R52 │      │     │   │
│  └─────┘           └─────┘           └─────┘      └─────┘   │
│  ┌─────┐                                                    │
│  │Core1│           ┌─────┐           ┌─────┐      ┌─────┐   │
│  │ R52 │           │Core1│           │Core1│      │DSPH │   │
│  └─────┘           │ R52 │           │ R52 │      │     │   │
│                    └─────┘           └─────┘      └─────┘   │
│                                                                 │
│  说明:                                                            │
│  - Cluster 0-2: 计算核心 (Cortex-R52)                          │
│  - Cluster 3: 外设集群，包含DME和DSPH                         │
│  - DME: Direct Memory Engine                                   │
│  - DSPH: DSP Handler                                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### SDK中的Cluster/Core定义

```c
// irq.h 中的定义
typedef enum {
    IRQ_CLUSTER_0,
    IRQ_CLUSTER_1,
    IRQ_CLUSTER_2,
    IRQ_CLUSTER_PER,   // 外设Cluster
    IRQ_CLUSTER_NONE
} irq_cluster_t;

typedef enum {
    IRQ_CORE_0,
    IRQ_CORE_1,
    IRQ_CORE_2,
    IRQ_CORE_3,
    IRQ_CORE_DME = IRQ_CORE_0,  // DME = Core 0 in Cluster 3
    IRQ_CORE_DSPH = IRQ_CORE_1  // DSPH = Core 1 in Cluster 3
} irq_core_t;
```

---

## 7.2 SPI中断多核路由配置

### 路由到单个核心

```c
#include <irq.h>
#include <irq_id.h>

// 将CAN中断路由到Core 0
void can_interrupt_to_core0(void)
{
    // 配置中断并指定目标核心
    // 参数: int_id, group, signal, priority
    // core参数直接指定目标核心
    irq_config(
        IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        4     // 高优先级
    );
    // 注意: 实际目标核心在GICD_IROUTER中设置
}
```

### 路由到多个核心 (广播)

```c
// GICv3支持将中断同时路由到多个核心
// 需要遍历每个核心单独配置

void can_interrupt_to_all_cores(void)
{
    uint32_t core;

    // 对每个核心单独配置
    for (core = 0; core < 4; core++) {
        // 注意: 这是简化的逻辑
        // 实际需要直接操作GICD_IROUTER
        // 或者使用特定的多目标配置方法
    }
}
```

---

## 7.3 跨Cluster中断 (IBCM)

### 为什么需要IBCM?

GICv3的SGI只能路由到同一Cluster内的核心。跨Cluster通信需要使用IBCM：

```
┌─────────────────────────────────────────────────────────────────┐
│                    跨Cluster中断通信                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Cluster 0                        Cluster 1                    │
│  ┌─────────┐                     ┌─────────┐                   │
│  │ Core 0  │ ────GIC SGI──────►│ Core 0  │                  │
│  └─────────┘    (同Cluster)     └─────────┘                  │
│                                                                 │
│  Cluster 0                        Cluster 1                    │
│  ┌─────────┐ ◄────── IBCM ──────│ Core 1  │                  │
│  │ Core 1  │      (跨Cluster)   └─────────┘                  │
│  └─────────┘                                                │
│                                                                 │
│  Cluster 3 (Peripheral)                                        │
│  ┌─────────┐ ◄────── IBCM ──────│ Core 0  │ (Cluster 0)      │
│  │  DME    │      (跨Cluster)   └─────────┘                  │
│  └─────────┘                                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### SDK中的跨Cluster中断

```c
// irq_notify() 函数用于跨Cluster中断
void irq_notify(irq_cluster_t cluster, irq_core_t core, irq_id_t int_id)
{
    if (gic_is_sgi(int_id) != 0u) {
        if ((uint32_t)cluster == get_cluster_id()) {
            // 同Cluster: 使用GIC直接路由
            gic_generate_sgi(cluster, core, int_id, IRQ_GROUP_NORMAL_IRQ, 0);
        } else {
            // 跨Cluster: 使用IBCM
            uint32_t value = ((uint32_t)cluster << 4UL) |
                            ((uint32_t)core << 3UL) |
                            (uint32_t)int_id;

            // 根据目标Cluster类型选择处理方式
            if (cluster == IRQ_CLUSTER_PER) {
                // DME/DSPH: 最多4个SGI
                if (int_id < IRQ_SGI_4_INT_NUMBER) {
                    ibcm_notify(value);
                }
            } else if (cluster < IRQ_CLUSTER_PER) {
                // 普通Cluster: 最多8个SGI
                if (int_id < IRQ_SGI_8_INT_NUMBER) {
                    ibcm_notify(value);
                }
            }
        }
    }
}
```

### 跨Cluster中断示例

```c
#include <irq.h>
#include <irq_id.h>
#include <core.h>

// 全局变量用于核间通信
volatile uint32_t ipi_message = 0;

// Core 0上的ISR
IRQ_HANDLER(IRQ_SGI_0_INT_HANDLER)
{
    // 读取消息
    uint32_t msg = ipi_message;

    // 处理消息
    handle_ipi(msg);

    // 清除pending (如果需要)
}

// 发送跨Cluster中断
void send_ipi_to_cluster1_core0(void)
{
    // 目标: Cluster 1, Core 0, SGI 0
    irq_notify(IRQ_CLUSTER_1, IRQ_CORE_0, IRQ_SGI_0_INT_NUMBER);
}

// 初始化跨Cluster中断
void ipi_init(void)
{
    uint32_t core_id = get_core_id();

    // 在当前核心上配置SGI0接收
    irq_config(
        IRQ_SGI_0_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        2    // 高优先级
    );

    irq_enable(IRQ_SGI_0_INT_NUMBER);
}
```

---

## 7.4 多核中断配置示例

### 多核定时器配置

```c
#include <irq.h>
#include <irq_id.h>
#include <core.h>

// 每个核心独立的定时器中断配置
void timer_init_per_core(void)
{
    uint32_t core_id = get_core_id();

    // 物理定时器中断是PPI，每个核心独立
    // 在哪个核心调用，就配置到哪个核心
    irq_config(
        IRQ_PHYS_TIMER_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        4    // 高优先级
    );

    irq_enable(IRQ_PHYS_TIMER_INT_NUMBER);

    // 配置硬件定时器 (平台相关)
    // configure_physical_timer(core_id);
}
```

### 外设中断多核分配

```c
#include <irq.h>
#include <irq_id.h>

// 将不同外设中断分配到不同核心
void distribute_interrupts(void)
{
    // CAN中断 -> Core 0 (高优先级实时处理)
    // 配置并路由到Core 0
    // GICD_IROUTER[ID].R = 0;  // 0 = Core 0

    // UART中断 -> Core 1 (通信处理)
    // 配置并路由到Core 1
    // GICD_IROUTER[ID].R = 1;  // 1 = Core 1

    // DMA中断 -> Core 2 (数据处理)
    // 配置并路由到Core 2
    // GICD_IROUTER[ID].R = 2;  // 2 = Core 2

    // 优先级配置
    irq_config(IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ, IRQ_EDGE_TRIGGERED, 4);

    irq_config(IRQ_LINFLEX_0_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ, IRQ_EDGE_TRIGGERED, 16);

    irq_config(IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ, IRQ_EDGE_TRIGGERED, 12);
}
```

---

## 7.5 多核启动与中断初始化

### 启动流程

```c
// 多核系统启动示例
void multicore_startup(void)
{
    uint32_t core_id = get_core_id();
    uint32_t cluster_id = get_cluster_id();

    // 公共初始化 (Master Core执行)
    if (core_id == 0 && cluster_id == 0) {
        // Master Core: 初始化系统
        system_init();
        gic_init();  // 初始化GIC
    }

    // 同步等待所有核心就绪
    sync_cores();

    // 各核心独立初始化
    if (cluster_id == 0) {
        // Cluster 0: 配置特定中断
        configure_cluster0_interrupts();
    } else if (cluster_id == 1) {
        // Cluster 1: 配置特定中断
        configure_cluster1_interrupts();
    }
}
```

### 中断负载均衡示例

```c
#include <irq.h>
#include <irq_id.h>
#include <core.h>

// 负载均衡: 将中断分散到不同核心
void balance_interrupt_load(void)
{
    uint32_t core_id = get_core_id();

    // 方式1: 根据核心ID分配
    switch (core_id) {
        case 0:
            // Core 0: 处理CAN
            irq_config(IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER,
                       IRQ_GROUP_NORMAL_IRQ, IRQ_EDGE_TRIGGERED, 4);
            irq_enable(IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER);
            break;

        case 1:
            // Core 1: 处理UART
            irq_config(IRQ_LINFLEX_0_INT_NUMBER,
                       IRQ_GROUP_NORMAL_IRQ, IRQ_EDGE_TRIGGERED, 16);
            irq_enable(IRQ_LINFLEX_0_INT_NUMBER);
            break;

        case 2:
            // Core 2: 处理DMA
            irq_config(IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER,
                       IRQ_GROUP_NORMAL_IRQ, IRQ_EDGE_TRIGGERED, 12);
            irq_enable(IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER);
            break;
    }
}
```

---

## 7.6 多核中断调试

### 常见问题与排查

```c
/*
 * 多核中断问题排查:
 *
 * 1. 中断没有触发
 *    - 检查目标核心是否在对应Cluster
 *    - 验证GICD_IROUTER配置
 *    - 确认GICD_ISENABLER已使能
 *
 * 2. 跨Cluster中断失败
 *    - 确认IBCM已初始化
 *    - 检查SGI号是否在允许范围
 *    - 验证目标Cluster/Core参数
 *
 * 3. 中断在错误核心触发
 *    - 检查IRQ配置时的core参数
 *    - 确认没有多线程竞争配置
 *
 * 4. 中断响应延迟
 *    - 检查优先级配置
 *    - 确认高优先级中断未阻塞
 *    - 验证栈空间足够
 */
```

### 调试技巧

```c
// 调试: 检查中断配置状态
void debug_interrupt_config(irq_id_t int_id)
{
    uint32_t group;
    uint32_t priority;
    uint32_t enabled;

    // 读取分组
    if (int_id < 32) {
        // SGI/PPI
        group = GICR_SGI_PPI_CPU0.GICR_IGROUPR0.R & (1 << int_id);
    } else {
        // SPI
        group = GICD.GICD_IGROUPR[int_id >> 5].R & (1 << (int_id & 0x1F));
    }

    // 读取优先级
    if (int_id < 32) {
        priority = GICR_SGI_PPI_CPU0.GICR_IPRIORITYR[int_id >> 2].R;
    } else {
        priority = GICD.GICD_IPRIORITYR[int_id >> 2].R;
    }
    priority = (priority >> ((int_id & 0x3) * 8)) & 0xFF;

    // 读取使能状态
    if (int_id < 32) {
        enabled = GICR_SGI_PPI_CPU0.GICR_ISENABLER0.R & (1 << int_id);
    } else {
        enabled = GICD.GICD_ISENABLER[int_id >> 5].R & (1 << (int_id & 0x1F));
    }

    // 打印调试信息
    printf("INT: %d, Group: %d, Priority: %d, Enabled: %d\n",
           int_id, group, priority, enabled);
}
```

---

## 本章小结

本章讲解了多核中断路由：

1. **多核架构**
   - Cluster 0-2 + 外设Cluster
   - 最多4个核心

2. **SPI路由**
   - 路由到单个核心
   - 广播到多核心

3. **跨Cluster通信**
   - IBCM模块
   - SGI限制
   - irq_notify()使用

4. **多核配置示例**
   - 外设分配
   - 负载均衡

5. **调试技巧**
   - 配置状态检查
   - 常见问题排查

---

## 下章预告

第八章将介绍高级特性：
- 安全扩展
- 虚拟化支持
- 调试与优化

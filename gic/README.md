# Stellar SR6P3C4 Cortex-R52 GIC中断控制器完全指南

> 本文档详细介绍了STMicroelectronics Stellar SR6P3C4芯片的GICv3中断控制器，涵盖从基础概念到实战应用的完整内容。

---

## 文档目录

### 第一部分：概念基础

| 章节 | 标题 | 内容简介 |
|------|------|----------|
| 第一章 | GIC概述与版本演进 | GIC功能、版本对比(GICv1-v4)、Cortex-R52支持特性 |
| 第二章 | GICv3/v4核心概念 (上) | SGI/PPI/SPI中断类型详解、SR6P3C4中断ID分配 |
| 第二章 | GICv3/v4核心概念 (下) | 优先级机制、分组(Group0/1)、Affinity Routing |

### 第二部分：硬件与源码

| 章节 | 标题 | 内容简介 |
|------|------|----------|
| 第三章 | SR6P3C4 GIC硬件架构 | GICD/GICR/CPU Interface寄存器详解、IBCM跨集群模块 |
| 第四章 | SDK中断框架源码解析 (上) | irq.h API接口、irq_id.h中断号定义 |
| 第四章 | SDK中断框架源码解析 (中) | intc.c gic_init()初始化函数逐行解析 |
| 第四章 | SDK中断框架源码解析 (下) | SGI/PPI/SPI配置函数详解 |

### 第三部分：实战应用

| 章节 | 标题 | 内容简介 |
|------|------|----------|
| 第五章 | 中断配置实战 | SPI/PPI/SGI配置步骤、完整代码示例 |
| 第六章 | 中断处理函数编写 | ISR规范、Prologue/Epilogue、驱动集成示例 |
| 第七章 | 多核中断路由 | 多Cluster配置、跨Cluster中断(IBCM)、负载均衡 |
| 第八章 | 高级特性与调试 | 安全扩展、虚拟化、调试技巧、问题排查 |

---

## 源码参考

本教程基于以下SDK源码:

```
StellarSDK_5.0.0/
├── arch/sr6x/interrupt/
│   ├── include/
│   │   ├── irq.h                 # 中断API定义
│   │   └── R52/sr6p3/irq_id.h   # 中断ID定义
│   └── src/
│       └── R52/intc.c           # GIC驱动实现
├── dfp/                          # 器件支持包
└── hal/                          # 硬件抽象层
```

---

## 特性图表

### 需要的图表

| 图表名称 | 类型 | 说明 | 章节 |
|----------|------|------|------|
| GIC整体架构框图 | 架构图 | Distributor/Redistributor/CPU Interface关系 | 第三章 |
| 中断类型分布图 | 分布图 | SGI/PPI/SPI ID分配区域 | 第二章 |
| 中断处理流程图 | 流程图 | 外设触发到ISR执行完整流程 | 第六章 |
| 多核中断路由示意图 | 路由图 | Cluster/Core Affinity路由 | 第七章 |
| SDK中断框架调用关系图 | 调用图 | API层次结构 | 第四章 |
| GIC初始化流程图 | 流程图 | gic_init()执行步骤 | 第四章 |
| 中断配置流程图 | 流程图 | 配置步骤逻辑 | 第五章 |
| 优先级分组图 | 示意图 | Group0/1与FIQ/IRQ映射 | 第二章 |

---

## 快速索引

### 常用中断号

```c
// UART (Linflex)
IRQ_LINFLEX_0_INT_NUMBER   // 102

// GPIO (SIUL2)
IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER  // 62
IRQ_SIUL2_0_EXT_INT_1_INT_NUMBER  // 63

// CAN
IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER  // 172

// SPI
IRQ_DSPI_6_INT_NUMBER  // 136

// 定时器
IRQ_PHYS_TIMER_INT_NUMBER  // 30
```

### 核心API

```c
// 初始化
irq_init();

// 配置
irq_config(int_id, group, signal, priority);

// 使能/禁用
irq_enable(int_id);
irq_disable(int_id);

// 跨核中断
irq_notify(cluster, core, int_id);

// 清除
irq_clear(cluster, core, int_id);
```

---

## 文档版本

- **版本**: 1.0
- **创建日期**: 2026-03-06
- **适用芯片**: Stellar SR6P3C4 (Cortex-R52)
- **适用SDK**: StellarSDK 5.0.0

---

## 附录

### 参考资料

1. ARM Generic Interrupt Controller Architecture Specification (GICv3/v4)
2. ARM Cortex-R52 Technical Reference Manual
3. STMicroelectronics Stellar SR6P3C4 Datasheet
4. STMicroelectronics LLA.pdf (Low Level Architecture)

---

*本教程由GIC SDK源码分析整理而成，旨在帮助开发者快速掌握SR6P3C4中断编程。*

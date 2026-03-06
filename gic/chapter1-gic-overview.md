# 第一章：GIC概述与版本演进

## 1.1 GIC功能与作用

### 什么是GIC？

**GIC（Generic Interrupt Controller，通用中断控制器）** 是ARM处理器架构中用于管理和分配中断的硬件模块。在现代多核处理器系统中，GIC承担着至关重要的角色：

```
┌─────────────────────────────────────────────────────────────────┐
│                         系统架构                                │
│                                                                 │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    │
│   │  Core 0 │    │  Core 1 │    │  Core 2 │    │  Core 3 │    │
│   │   CPU   │    │   CPU   │    │   CPU   │    │   CPU   │    │
│   └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘    │
│        │              │              │              │          │
│        └──────────────┴──────────────┴──────────────┘          │
│                             │                                  │
│                    ┌────────▼────────┐                        │
│                    │   GIC (中断控制器) │                       │
│                    └────────┬────────┘                        │
│                             │                                  │
│        ┌──────────────┬─────┴─────┬──────────────┐          │
│        │              │            │              │            │
│   ┌────▼────┐   ┌────▼────┐ ┌────▼────┐  ┌────▼────┐       │
│   │  Timer  │   │   UART   │ │   SPI   │  │   CAN   │       │
│   └─────────┘   └──────────┘ └─────────┘  └─────────┘       │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### GIC的核心功能

1. **中断收集**: 接收来自各类外设的中断请求
2. **中断优先级排序**: 根据优先级和亲和性选择最高优先级中断
3. **中断分发**: 将中断分发到对应的CPU核心
4. **中断屏蔽**: 支持优先级屏蔽和中断使能/禁用
5. **中断状态管理**: 维护中断的pending、active状态

---

## 1.2 GIC版本演进

ARM GIC经历了多次重大架构演进，从最初的GICv1到现在的GICv4，每个版本都带来了显著的性能提升和功能增强。

### GICv1（2008年）

- **发布时间**: ARM ARMv7-A/R架构时期
- **特性**:
  - 最多支持8个CPU核心
  - 最多1024个中断源
  - 简单优先级机制（8位优先级）
  - 软件中断（SGI）支持
  - 两种中断类型：SGI和PPI/SPI

**局限性**: 仅支持单Cluster，不支持Affinity Routing

### GICv2（2009年）

- **发布时间**: ARM Cortex-A9 MPCore时期
- **特性**:
  - 最多支持16个CPU核心
  - 最多1020个中断源
  - 改进的优先级机制
  - 支持中断优先级分组（Group0/Group1）
  - 增加FIQ/IRQ支持
  - 优化的中断分发性能

**局限性**: 仍不支持新特性如消息信号中断(MSI)

### GICv3（2013年）

- **发布时间**: ARM Cortex-A53/A57时期
- **重大特性**:
  - **Affinity Routing**: 基于cluster/core的灵活路由
  - **Redistributor**: 每个CPU核心独立的重分发器
  - **消息信号中断(MSI)**: 支持基于内存的中断
  - **更多中断源**: 最多1024个SPI + 16个SGI + 16个PPI
  - **虚拟化支持**: 基础虚拟化扩展
  - **安全扩展**: 完整支持TrustZone

**关键改进**:
```
GICv3 架构 (多Cluster支持)
┌──────────────────────────────────────────────────────────┐
│                     GIC Distributor                       │
│  (管理所有SPI中断，支持Affinity Routing)                 │
└─────────────────┬────────────────────────────────────────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
┌───▼───┐    ┌────▼────┐   ┌────▼────┐
│ Red   │    │ Red     │   │ Red     │
│ist #0 │    │ist #1   │   │ist #N   │
└───┬───┘    └────┬────┘   └────┬────┘
    │             │             │
┌───▼───┐    ┌────▼────┐   ┌────▼────┐
│ CPU   │    │ CPU     │   │ CPU     │
│ IF #0 │    │ IF #1   │   │ IF #N   │
└───┬───┘    └────┬────┘   └────┴────┘
    │             │
┌───▼───┐    ┌────▼────┐
│ Core0 │    │ Core1   │
└───────┘    └─────────┘
```

### GICv4（2016年）

- **发布时间**: ARM Cortex-A76时期
- **新增特性**:
  - **虚拟中断直接注入**: hypervisor可以直接注入虚拟中断
  - **改进的虚拟化性能**: 减少虚拟中断延迟
  - **支持更多LPI**: 更多的本地私有中断
  - **ITS (Interrupt Translation Service)**: 改进的MSI处理

### 版本特性对比表

| 特性 | GICv1 | GICv2 | GICv3 | GICv4 |
|------|-------|-------|-------|-------|
| 最大CPU核心数 | 8 | 16 | 256 | 256 |
| 最大中断源 | 1024 | 1020 | 1020+ | 1020+ |
| Affinity Routing | 不支持 | 不支持 | 支持 | 支持 |
| Redistributor | 不支持 | 不支持 | 支持 | 支持 |
| MSI消息中断 | 不支持 | 不支持 | 支持 | 支持 |
| 虚拟化支持 | 不支持 | 不支持 | 基础 | 完整 |
| 安全扩展 | 基础 | 基础 | 完整 | 完整 |
| 推荐应用 | 旧款Cortex-A | Cortex-A9 | Cortex-A53/57/76 | 最新ARM服务器 |

---

## 1.3 Cortex-R52支持的GICv3/v4特性

### Cortex-R52与GIC

**Cortex-R52** 是ARM专为实时安全关键应用设计的处理器，支持 **GICv3** 和部分 **GICv4** 特性。这使其非常适合汽车电子、工业控制等对安全性和实时性要求极高的领域。

### Stellar SR6P3C4的GIC特性

基于SDK源码分析，SR6P3C4的GIC实现包含以下关键特性：

#### 1. 多核支持

```c
// intc.c 中的多核配置
#ifdef CORTEXR52_GICR_CTRL_CPU2
#define GICR_CTRL_CPU2    CORTEXR52_GICR_CTRL_CPU2
#define GICR_SGI_PPI_CPU2 CORTEXR52_GICR_SGI_PPI_CPU2
#endif

#ifdef CORTEXR52_GICR_CTRL_CPU3
#define GICR_CTRL_CPU3    CORTEXR52_GICR_CTRL_CPU3
#define GICR_SGI_PPI_CPU3 CORTEXR52_GICR_SGI_PPI_CPU3
#endif
```

- 最多支持 **4个Cortex-R52核心**
- 支持多个Cluster配置
- 每个核心拥有独立的Redistributor

#### 2. Affinity Routing

```c
// 启用Affinity Routing
GICD.GICD_CTLR.B.ARE = 1;
```

- 启用基于affinity的中断路由
- 支持中断路由到指定cluster/core
- 优化了多核系统中的中断分发

#### 3. 中断分组

```c
// 启用Group 0 (FIQ) 和 Group 1 (IRQ)
GICD.GICD_CTLR.B.ENABLEGRP0 = 1;
GICD.GICD_CTLR.B.ENABLEGRP1 = 1;

// CPU Interface配置
arm_write_sysreg(CP15_ICC_IGRPEN0, 1);  // Enable Group 0: FIQ
arm_write_sysreg(CP15_ICC_IGRPEN1, 1);  // Enable Group 1: IRQ
```

- **Group 0**: 安全中断，映射为FIQ
- **Group 1**: 非安全中断，映射为IRQ

#### 4. 优先级支持

```c
// 配置优先级掩码 (允许所有优先级)
icc_pmr = 0xF8;
arm_write_sysreg(CP15_ICC_PMR, icc_pmr);
```

- 8位优先级字段
- 实际使用高5位 (0xF8掩码)
- 256个优先级级别

#### 5. 跨集群中断 (IBCM)

SR6P3C4包含 **IBCM (Inter-Cluster Broadcast Module)**，支持跨集群中断：

```c
// ibcm.c 实现跨集群中断
void irq_notify(irq_cluster_t cluster, irq_core_t core, irq_id_t int_id)
{
    if (gic_is_sgi(int_id) != 0u) {
        if ((uint32_t)cluster == get_cluster_id()) {
            // 同Cluster，使用GIC
            gic_generate_sgi(cluster, core, int_id, IRQ_GROUP_NORMAL_IRQ, 0);
        } else {
            // 跨Cluster，使用IBCM
            ibcm_notify(value);
        }
    }
}
```

### 支持的中断类型

| 类型 | ID范围 | 数量 | 描述 |
|------|--------|------|------|
| SGI | 0-15 | 16 | 软件生成中断，用于core间通信 |
| PPI | 16-31 | 16 | 私有外设中断，每个core独立 |
| SPI | 32+ | 可配置 | 共享外设中断，可路由到任意core |

---

## 本章小结

本章介绍了GIC的基本概念和版本演进：

1. **GIC是ARM处理器中管理中断的核心硬件**
   - 收集、排序、分发中断到各CPU核心
   - 支持优先级、屏蔽、状态管理

2. **GIC版本从v1到v4持续演进**
   - GICv3是现代处理器的标准选择
   - 支持Affinity Routing、MSI、虚拟化等特性

3. **Cortex-R52支持GICv3/v4特性**
   - 最多4个核心的多核支持
   - Affinity Routing灵活路由
   - Group0/Group1安全分组
   - IBCM跨集群中断支持

---

## 下章预告

第二章将详细介绍GICv3/v4的核心概念，包括：
- 中断类型的详细定义
- 中断优先级的处理机制
- 中断分组与FIQ/IRQ映射
- Affinity Routing多核路由原理

配合SR6P3C4的具体中断ID分配，帮助读者建立完整的GIC知识体系。

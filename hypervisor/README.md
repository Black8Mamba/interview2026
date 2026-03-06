# Cortex-R52 Hypervisor 深度解析与实战

> 基于 Stellar SDK 5.0.0 (Stellar SR6P3c4) 源码分析

---

## 文档概述

本文档是一份详细的 Cortex-R52 Hypervisor 学习指南，基于瑞萨电子（Renesas）Stellar SDK 5.0.0 中的 Hypervisor 源码编写。文档涵盖从基础概念到实战应用的完整知识体系，适合不同层次的嵌入式开发者学习参考。

**目标读者：**
- A类：刚接触虚拟化的嵌入式开发者
- B类：有一定基础，想深入了解 Cortex-R52 虚拟化的工程师
- C类：已经有 Hypervisor 开发经验，需要了解 Stellar SDK 特性的开发者

**参考源码：**
- SDK 路径：`parts/virt/st_hypervisor/`
- 调度器：`parts/virt/st_hypervisor/Scheduler/`
- 示例 VM：`parts/virt/st_hypervisor/demos/Hypervisor/`

---

## 目录

### 第一部分：基础概念

- [第 1 章：Cortex-R52 架构概述](./part1/01-cortex-r52-overview.md)
- [第 2 章：ARMv8-R 异常级别](./part1/02-armv8-r-exception-level.md)
- [第 3 章：虚拟化基础概念](./part1/03-virtualization-basics.md)
- [第 4 章：TrustZone 与安全模型](./part1/04-trustzone-security.md)

### 第二部分：Hypervisor 框架

- [第 5 章：Stellar SDK Hypervisor 架构](./part2/05-hypervisor-architecture.md)
- [第 6 章：EL2 异常向量与启动流程](./part2/06-el2-vector-boot.md)
- [第 7 章：HVC 机制](./part2/07-hvc-mechanism.md)

### 第三部分：核心模块实现

- [第 8 章：异常处理模块](./part3/08-exception-handling.md)
- [第 9 章：调度器设计与实现](./part3/09-scheduler.md)
- [第 10 章：外设虚拟化框架](./part3/10-peripheral-virtualization.md)
- [第 11 章：内存保护配置](./part3/11-mpu-configuration.md)

### 第四部分：实战与调试

- [第 12 章：VM0 创建实例分析](./part4/12-vm0-analysis.md)
- [第 13 章：添加新的外设虚拟化支持](./part4/13-add-peripheral-guide.md)
- [第 14 章：调试方法与工具](./part4/14-debugging-tools.md)

---

## 文档约定

### 代码引用格式

源码引用使用以下格式：

```
文件路径:行号
```

例如：`parts/virt/st_hypervisor/arch/src/el2_vectors.S:45`

### 术语翻译

- Hypervisor：虚拟机监视器
- Virtual Machine (VM)：虚拟机
- Exception Level (EL)：异常级别
- TrustZone：TrustZone 安全技术
- MPU：内存保护单元
- GIC：通用中断控制器

---

## 编写信息

- 创建日期：2026-03-06
- 基于 SDK：Stellar SDK 5.0.0
- 目标页数：约 60 页

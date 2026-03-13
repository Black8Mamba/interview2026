# Linux内核中断子系统学习指南 - 目录

## 第1章 中断子系统概述
- [1.1 中断基本概念](./chapter01-overview.md#11-中断基本概念)
- [1.2 Linux中断模型](./chapter01-overview.md#12-linux中断模型)
- [1.3 中断分类](./chapter01-overview.md#13-中断分类)

## 第2章 中断框架核心
- [2.1 irq_desc 结构](./chapter02-framework-core.md#21-irq_desc-结构)
- [2.2 irq_chip 接口](./chapter02-framework-core.md#22-irq_chip-接口)
- [2.3 irqdomain 原理](./chapter02-framework-core.md#23-irqdomain-原理)
- [2.4 中断号映射机制](./chapter02-framework-core.md#24-中断号映射机制)

## 第3章 中断处理流程
- [3.1 硬件中断处理流程](./chapter03-interrupt-flow.md#31-硬件中断处理流程)
- [3.2 中断上下文](./chapter03-interrupt-flow.md#32-中断上下文)
- [3.3 softirq 机制](./chapter03-interrupt-flow.md#33-softirq-机制)
- [3.4 tasklet 与 workqueue](./chapter03-interrupt-flow.md#34-tasklet-与-workqueue)
- [3.5 中断线程化](./chapter03-interrupt-flow.md#35-中断线程化)

## 第4章 ARM64架构与异常处理
- [4.1 ARM64异常级别](./chapter04-arm64-exception.md#41-arm64异常级别)
- [4.2 异常向量表](./chapter04-arm64-exception.md#42-异常向量表)
- [4.3 异常处理入口](./chapter04-arm64-exception.md#43-异常处理入口)
- [4.4 SP_EL0/EL1切换](./chapter04-arm64-exception.md#44-sp_el0el1切换)

## 第5章 GIC中断控制器
- [5.1 GIC架构概述](./chapter05-gic-controller.md#51-gic架构概述)
- [5.2 Distributor分发器](./chapter05-gic-controller.md#52-distributor分发器)
- [5.3 CPU Interface](./chapter05-gic-controller.md#53-cpu-interface)
- [5.4 中断类型](./chapter05-gic-controller.md#54-中断类型)
- [5.5 中断优先级与亲和性](./chapter05-gic-controller.md#55-中断优先级与亲和性)
- [5.6 Linux内核GIC驱动实现](./chapter05-gic-controller.md#56-linux内核gic驱动实现)

## 第6章 设备树与中断映射
- [6.1 DTS中断属性](./chapter06-device-tree.md#61-dts中断属性)
- [6.2 interrupt-parent与级联](./chapter06-device-tree.md#62-interrupt-parent与级联)
- [6.3 irqdomain解析流程](./chapter06-device-tree.md#63-irqdomain解析流程)
- [6.4 典型设备中断配置](./chapter06-device-tree.md#64-典型设备中断配置)

## 第7章 中断性能优化
- [7.1 中断亲和性](./chapter07-performance.md#71-中断亲和性)
- [7.2 中断负载均衡](./chapter07-performance.md#72-中断负载均衡)
- [7.3 软中断调优](./chapter07-performance.md#73-软中断调优)
- [7.4 中断延迟分析](./chapter07-performance.md#74-中断延迟分析)
- [7.5 Real-time相关优化](./chapter07-performance.md#75-real-time相关优化)

## 第8章 调试与案例分析
- [8.1 中断调试工具](./chapter08-debugging.md#81-中断调试工具)
- [8.2 常见问题分析](./chapter08-debugging.md#82-常见问题分析)
- [8.3 中断风暴处理](./chapter08-debugging.md#83-中断风暴处理)
- [8.4 真实案例解析](./chapter08-debugging.md#84-真实案例解析)

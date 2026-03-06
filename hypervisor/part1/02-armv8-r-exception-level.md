# 第 2 章：ARMv8-R 异常级别

## 2.1 EL0/EL1/EL2/EL3 详解

ARMv8-R 架构使用 Exception Level（异常级别）来区分不同的特权级别。这是 ARM 架构的核心概念，与 ARMv7 的运行模式（User、SVC、FIQ 等）有本质区别。

### 2.1.1 异常级别概述

| 异常级别 | 执行状态 | 描述 |
|---------|---------|------|
| EL0 | AArch32/AArch64 | 用户态/应用程序级别，非特权模式 |
| EL1 | AArch32/AArch64 | 特权态（操作系统内核），可访问特权资源 |
| EL2 | AArch32/AArch64 | 虚拟机监视器（Hypervisor），虚拟化支持 |
| EL3 | AArch32/AArch64 | 安全监视器（Secure Monitor），安全状态切换 |

### 2.1.2 各级别职责

**EL0（用户模式）**
- 应用程序运行级别
- 只能访问受限资源
- 不能直接访问硬件外设
- 不能修改系统配置

**EL1（特权模式）**
- 操作系统内核运行级别
- 可以访问所有硬件资源
- 可以管理 EL0 进程
- 处理系统调用（SVC）

**EL2（Hypervisor 模式）**
- 虚拟机监视器运行级别
- 虚拟化核心实现
- 管理多个虚拟机
- 处理虚拟化异常

**EL3（安全监视器）**
- 安全世界入口
- 安全状态切换
- 信任根实现

### 2.1.3 异常级别切换

异常级别切换规则：

1. **异常进入**：可以从较低的 EL 切换到较高的 EL
2. **异常返回**：从较高 EL 返回较低 EL（通过 ERET 指令）
3. **安全状态**：EL3 是 Secure/Non-Secure 转换的唯一入口

## 2.2 异常处理模型

### 2.2.1 异常类型

在 ARMv8-R 中，异常可分为以下几类：

- **同步异常**：由指令执行产生
  - SVC（系统调用）
  - HVC（Hypervisor 调用）
  - SMC（安全监视器调用）
  - 数据中止/预取中止

- **异步异常**：由外设产生
  - IRQ（普通中断）
  - FIQ（快速中断）
  - SError（系统错误）

### 2.2.2 异常向量表

当发生异常时，处理器会根据异常类型跳转到对应的异常向量：

```c
// EL2 异常向量表示例
void el2_vector_synchronous(void);   // 同步异常
void el2_vector_irq(void);            // IRQ 中断
void el2_vector_fiq(void);            // FIQ 中断
void el2_vector_serror(void);        // 系统错误
```

### 2.2.3 异常处理流程

1. **保存状态**：处理器自动保存 PSTATE 到 SPSR_EL2
2. **设置返回地址**：返回地址保存到 ELR_EL2
3. **切换级别**：跳转到异常向量
4. **处理异常**：执行异常处理程序
5. **返回**：通过 ERET 指令返回

## 2.3 Cortex-R52 复位后的运行级别

### 2.3.1 复位后状态

Cortex-R52 复位后的运行级别是 **EL2（Hypervisor 模式）**，这是与其他 Cortex-R 处理器的重要区别。

这种设计有重要意义：

- **直接运行 Hypervisor**：复位后可以直接进入虚拟化环境
- **简化启动流程**：不需要从 EL1 切换到 EL2
- **支持安全启动**：可以在 EL2 实现安全启动

### 2.3.2 启动流程

在 Stellar SDK 中，复位后流程如下：

```
复位 → EL2 (Hypervisor) → 配置 MPU/GIC → 启动调度器 → 切换到 VM (EL1)
```

### 2.3.3 EL2 初始化任务

在 EL2 运行时，需要完成以下初始化：

```c
uint32_t hyper_init(void)
{
    // 1. 配置 EL2 异常向量
    set_vbar_el2(el2_vectors);

    // 2. 配置 MPU
    hyp_mpu_init();

    // 3. 初始化 GIC
    gic_init();

    // 4. 启动调度器
    sched_init();

    // 5. 返回第一个要运行的 VM ID
    return 0;
}
```

## 2.4 安全世界与非安全世界

### 2.4.1 TrustZone 安全架构

Cortex-R52 支持 ARM TrustZone 安全扩展，将系统划分为两个世界：

- **Secure World（安全世界）**：可信代码执行区域
- **Normal World（非安全世界）**：普通代码执行区域

### 2.4.2 安全状态切换

安全状态切换只能通过 EL3 完成：

```
Normal World (EL1/EL2) ←→ EL3 (Secure Monitor) ←→ Secure World
```

### 2.4.3 在 Stellar SDK 中的应用

在 Stellar SDK 的 Hypervisor 设计中：

- Hypervisor 运行在 Non-Secure EL2
- VM 运行在 Non-Secure EL1
- 安全外设和关键资源可以配置在 Secure World

### 2.4.4 VMID 机制

Virtual Machine ID (VMID) 用于区分不同的虚拟机：

```c
// 设置 VMID
void set_vmid(uint32_t vmid)
{
    uint32_t vsctlr = read_vsctlr();
    vsctlr = (vsctlr & ~0xFF) | (vmid & 0xFF);
    write_vsctlr(vsctlr);
}
```

## 2.5 本章小结

本章详细介绍了 ARMv8-R 的异常级别模型：

1. **四级特权**：EL0-EL3 区分不同特权级别
2. **异常处理**：同步/异步异常的向量和处理流程
3. **Cortex-R52 特性**：复位后运行在 EL2
4. **TrustZone**：安全世界和非安全世界的隔离

这些概念是理解 Hypervisor 工作原理的基础。下一章将介绍虚拟化的基础概念。

---

*参考资料：*
- ARM Architecture Reference Manual (ARMv8, for R profile)
- ARM Cortex-R52 处理器技术参考手册

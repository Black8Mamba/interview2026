# 第 3 章：虚拟化基础概念

## 3.1 Hypervisor 类型

### 3.1.1 Type-1 Hypervisor（裸机型）

Type-1 Hypervisor 直接运行在硬件上，不依赖任何操作系统：

- **特点**：
  - 直接控制硬件资源
  - 更少的性能开销
  - 更高的可靠性和实时性
- **典型代表**：
  - Stellar SDK Hypervisor
  - QNX Hypervisor
  - AUTOSAR Adaptive Hypervisor

### 3.1.2 Type-2 Hypervisor（托管型）

Type-2 Hypervisor 运行在宿主操作系统上：

- **特点**：
  - 依赖宿主操作系统
  - 部署灵活
  - 适合开发测试环境
- **典型代表**：
  - VMware Workstation
  - VirtualBox

### 3.1.3 Stellar SDK Hypervisor 类型

Stellar SDK 中的 Hypervisor 是 Type-1 型，运行在 EL2 级别，直接管理硬件资源。

## 3.2 虚拟化扩展要求

### 3.2.1 ARM 虚拟化扩展

ARMv8-R 架构提供了完整的虚拟化支持：

- **EL2 异常级别**：独立的 Hypervisor 运行级别
- **虚拟化异常**：支持虚拟化相关的异常处理
- **第二级地址转换**（Stage 2）：实现虚拟机内存虚拟化
- **虚拟中断**：支持虚拟中断注入

### 3.2.2 虚拟化必备特性

| 特性 | 说明 |
|------|------|
| 内存虚拟化 | Stage 2 页表转换 |
| 中断虚拟化 | 虚拟中断注入 |
| I/O 虚拟化 | 外设访问模拟/透传 |
| 异常处理 | 陷入-模拟模型 |

## 3.3 虚拟机（VM）概念

### 3.3.1 VM 定义

虚拟机（Virtual Machine，VM）是 Hypervisor 管理的虚拟运行单元，每个 VM 有独立的：

- **虚拟 CPU（vCPU）**：VM 看到的 CPU 资源
- **虚拟内存**：独立的地址空间
- **虚拟外设**：分配的外设资源
- **执行上下文**：独立的寄存器状态

### 3.3.2 VM 状态

在 Stellar SDK 中，VM 有以下状态（定义在 `sched.h`）：

```c
typedef enum {
    SCHED_VM_NONE,        /**< VM only initialized */
    SCHED_VM_STARTED,     /**< VM Started */
    SCHED_VM_STOPPED,     /**< VM Stopped */
    SCHED_VM_SUSPENDED,   /**< VM Suspended */
    SCHED_VM_RESUMED,     /**< VM Resumed */
    SCHED_VM_ERROR        /**< VM in Error */
} sched_vm_status_t;
```

### 3.3.3 VM 配置文件

每个 VM 在 `vm_config.h` 中配置：

```c
typedef struct {
    uint32_t vmid;                 // VM ID
    uint32_t priority;             // 调度优先级
    uint32_t time_slice;           // 时间片大小
    uint32_t memory_base;           // 内存基址
    uint32_t memory_size;           // 内存大小
    uint32_t entry_point;           // 入口地址
    uint32_t periph_mask;           // 外设掩码
} vm_config_t;
```

## 3.4 虚拟中断与注入

### 3.4.1 虚拟中断原理

虚拟中断是 Hypervisor 向 VM 注入的中断，用于：

- 模拟外设产生的中断
- 实现 VM 之间的通信
- 调度器时间片中断

### 3.4.2 中断注入流程

```
物理中断发生
    ↓
GIC 报告给 EL2 (Hypervisor)
    ↓
Hypervisor 判断目标 VM
    ↓
配置虚拟中断
    ↓
注入到目标 VM (EL1)
    ↓
VM 中断处理程序执行
```

### 3.4.3 GIC 虚拟化

Stellar SDK 使用 GICv2，支持：

- **物理中断**：外设产生的真实中断
- **虚拟中断**：Hypervisor 注入的中断
- **List Register**：虚拟中断队列

## 3.5 本章小结

本章介绍了虚拟化的基础概念：

1. **Hypervisor 类型**：Type-1（裸机型）适合嵌入式实时系统
2. **虚拟化扩展**：ARMv8-R 提供完整的虚拟化支持
3. **虚拟机概念**：VM 是 Hypervisor 管理的虚拟运行单元
4. **虚拟中断**：实现 VM 与外设、VM 之间的通信

下一章将介绍 TrustZone 安全模型。

---

*参考资料：*
- ARM虚拟化指南
- Stellar SDK Hypervisor 架构文档

# 第 4 章：TrustZone 与安全模型

## 4.1 TrustZone 安全架构

### 4.1.1 概述

ARM TrustZone 是系统级安全技术，通过硬件隔离实现可信计算环境。TrustZone 将系统划分为两个完全隔离的世界：

- **Secure World（安全世界）**：运行可信代码
- **Normal World（普通世界）**：运行普通操作系统和应用

### 4.1.2 安全目标

TrustZone 实现以下安全目标：

1. **隔离**：安全世界与普通世界完全隔离
2. **可信启动**：安全引导链
3. **密钥保护**：安全存储加密密钥
4. **外设保护**：敏感外设只能被安全世界访问

### 4.1.3 架构示意

```
+------------------+
|   Application    |  ← Normal World EL0
+------------------+
|   OS Kernel     |  ← Normal World EL1
+------------------+
|                  |
|   Hypervisor     |  ← Non-Secure EL2
|                  |
+------------------+
| Secure Monitor   |  ← Secure EL3
+------------------+
| Secure OS/Services| ← Secure World
+------------------+
```

## 4.2 Secure World 与 Normal World

### 4.2.1 安全世界（Secure World）

安全世界可以访问所有资源：

- 安全内存区域
- 安全外设
- 加密加速器
- 安全定时器

**典型应用**：
- 安全引导（Secure Boot）
- 数字版权管理（DRM）
- 支付应用
- 密钥存储

### 4.2.2 普通世界（Normal World）

普通世界运行常规软件：

- Linux/Android 系统
- AUTOSAR 操作系统
- Hypervisor 和 VM
- 用户应用程序

### 4.2.3 世界切换

两个世界之间的切换只能通过 EL3（Secure Monitor）完成：

```c
// 安全监视器调用示例
void smc_call(uint32_t func_id, uint32_t arg0, uint32_t arg1)
{
    // 通过 SMC 指令切换到 Secure World
    asm volatile("smc #0" : : "r"(func_id), "r"(arg0), "r"(arg1));
}
```

## 4.3 安全监视器（Secure Monitor）

### 4.3.1 角色

Secure Monitor 是两个世界之间的安全网关：

- 处理 SMC（Secure Monitor Call）调用
- 管理安全状态切换
- 保护安全资源

### 4.3.2 职责

| 职责 | 说明 |
|------|------|
| 世界切换 | 安全 ↔ 非安全状态切换 |
| 上下文保存 | 保存/恢复两个世界的状态 |
| 资源保护 | 确保安全资源不被非法访问 |
| 密钥管理 | 安全密钥的存储和使用 |

## 4.4 Stellar SDK 安全设计

### 4.4.1 安全架构

在 Stellar SDK 中，安全设计包括：

1. **分区隔离**：不同安全等级的应用运行在不同分区
2. **外设保护**：敏感外设只能被授权的 VM 访问
3. **内存隔离**：通过 MPU 实现虚拟机之间的内存隔离

### 4.4.2 Hypervisor 安全职责

Stellar SDK 的 Hypervisor 运行在 Non-Secure EL2，负责：

- **VM 隔离**：确保 VM 之间不能相互访问内存
- **外设访问控制**：验证 VM 对外设的访问权限
- **中断分配**：安全地分配中断到各 VM

### 4.4.3 VM 内存隔离配置

每个 VM 有独立的内存区域：

```c
// VM 内存配置示例
typedef struct {
    uint32_t base_addr;    // 区域基址
    uint32_t size;         // 区域大小
    uint32_t attr;         // 属性 (R/W/X)
    uint32_t access;       // 访问控制
} vm_memory_region_t;
```

### 4.4.4 安全外设示例

Stellar SDK 中可配置的安全外设：

- **RGM（Reset & Clock Manager）**：复位和时钟管理
- **PMU（Power Management Unit）**：电源管理
- **安全定时器**：只能被安全世界访问

## 4.5 虚拟化与安全的结合

### 4.5.1 安全虚拟化

在 Cortex-R52 上，虚拟化与 TrustZone 可以结合使用：

```
+------------------+
|    VM0 (Rich)   |  ← Non-Secure EL1
+------------------+
|    VM1 (RTOS)   |  ← Non-Secure EL1
+------------------+
|   Hypervisor    |  ← Non-Secure EL2
+------------------+
| Secure Monitor  |  ← Secure EL3
+------------------+
```

### 4.5.2 安全设计原则

1. **最小特权**：每个 VM 只拥有必要的权限
2. **深度防御**：多层安全保护
3. **安全分离**：不同安全等级的代码分开运行
4. **审计追踪**：记录关键安全事件

## 4.6 本章小结

本章介绍了 TrustZone 安全架构：

1. **TrustZone 架构**：划分为安全世界和普通世界
2. **安全监视器**：两个世界之间的安全网关
3. **Stellar SDK 安全设计**：VM 隔离、外设保护
4. **虚拟化与安全结合**：在虚拟化环境中实现安全隔离

---

*参考资料：*
- ARM TrustZone 技术概述
- ARM Cortex-R52 安全架构文档
- Stellar SDK 安全设计文档

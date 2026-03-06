# 第 5 章：Stellar SDK Hypervisor 架构

## 5.1 整体架构概览

Stellar SDK Hypervisor 是一个 Type-1 型虚拟机监视器，运行在 ARMv8-R 架构的 EL2 异常级别。它为 Stellar SR6P3c4 MCU 提供完整的虚拟化支持。

### 5.1.1 架构层次

```
+----------------------------------------------------------+
|                     Guest VMs (EL1)                       |
|  +-------------+  +-------------+  +-------------+       |
|  |    VM0     |  |    VM1     |  |    VM2     |  ...  |
|  |  (FreeRTOS)|  |  (FreeRTOS)|  |  (FreeRTOS)|       |
|  +-------------+  +-------------+  +-------------+       |
+----------------------------------------------------------+
|              Hypervisor Layer (EL2)                        |
|  +-------------+  +-------------+  +-------------+       |
|  |  Scheduler |  |   HAL       |  |  Exception  |       |
|  |            |  |  (外设虚拟化)|  |  Handler   |       |
|  +-------------+  +-------------+  +-------------+       |
+----------------------------------------------------------+
|              Hardware (Cortex-R52)                         |
|  +-------------+  +-------------+  +-------------+       |
|  |    MPU     |  |    GIC     |  |   Peripherals|       |
|  +-------------+  +-------------+  +-------------+       |
+----------------------------------------------------------+
```

### 5.1.2 核心组件

Stellar SDK Hypervisor 主要包含以下核心组件：

| 组件 | 描述 | 路径 |
|------|------|------|
| Hypervisor Core | 核心异常处理和服务 | `parts/virt/st_hypervisor/arch/` |
| Scheduler | VM 调度器 | `parts/virt/st_hypervisor/Scheduler/` |
| HAL | 外设虚拟化抽象层 | `parts/virt/st_hypervisor/hal/` |
| Demo | 示例 VM | `parts/virt/st_hypervisor/demos/` |

## 5.2 目录结构分析

### 5.2.1 整体目录

```
parts/virt/st_hypervisor/
├── arch/                      # 架构相关代码
│   ├── include/              # 头文件
│   │   ├── hyper.h           # Hypervisor 主头文件
│   │   └── irq_hyp.h         # 中断相关头文件
│   └── src/                  # 源文件
│       ├── el2_vectors.S     # EL2 异常向量
│       ├── hyp_exceptions.S  # 异常处理汇编
│       ├── hyp_handler.c     # 异常处理 C 代码
│       ├── hyp_services.c    # Hypervisor 服务
│       └── hyper_trampoline.S # HVC 调用跳转
├── Scheduler/                 # 调度器
│   ├── include/              # 调度器头文件
│   │   ├── sched.h           # 调度器主头文件
│   │   └── sched_hyp.h       # Hypervisor 调度相关
│   └── src/                  # 调度器实现
│       ├── sched.c           # 调度器主实现
│       ├── sched_hyp.c       # Hypervisor 调度
│       └── scheduler.S       # 调度汇编
├── hal/                      # 外设虚拟化 HAL
│   ├── hyper/                # Hypervisor 侧 HAL
│   │   ├── agt_hyp.c
│   │   ├── edma_hyp.c
│   │   ├── siul2_hyp.c
│   │   └── ...
│   ├── vm/                  # VM 侧 HAL
│   │   ├── agt_hvc.c
│   │   ├── edma_hvc.c
│   │   └── ...
│   └── include/             # HAL 头文件
├── demos/                   # 示例
│   └── Hypervisor/          # Hypervisor Demo
│       ├── src/
│       │   ├── cluster0/core0/hyper/
│       │   └── vm/vm0~vm11/
│       └── include/vm_config/
└── tests/                   # 测试用例
```

### 5.2.2 关键文件说明

**Hypervisor 核心文件：**

| 文件 | 功能 |
|------|------|
| `arch/include/hyper.h` | Hypervisor 主头文件，定义模块 ID、HVC 协议 |
| `arch/src/el2_vectors.S` | EL2 异常向量表定义 |
| `arch/src/hyp_handler.c` | 异常处理 C 代码，根据 HSR 判断异常类型 |
| `arch/src/hyp_services.c` | 服务分发，根据 module_id 分发到各模块 |

## 5.3 核心组件介绍

### 5.3.1 Hypervisor Core

Hypervisor Core 负责：

- **异常处理**：处理来自 VM 的异常
- **服务分发**：将 HVC 调用分发到对应模块
- **VM 管理**：管理虚拟机的生命周期

### 5.3.2 Scheduler

调度器负责：

- **VM 调度**：按时间片轮转调度 VM
- **状态管理**：管理 VM 的运行状态
- **上下文切换**：保存/恢复 VM 上下文

### 5.3.3 HAL (外设虚拟化层)

HAL 层负责：

- **外设模拟**：虚拟化外设行为
- **HVC 接口**：提供 VM 访问外设的接口
- **资源分配**：管理外设资源的分配

### 5.3.4 支持的外设

Stellar SDK Hypervisor 支持以下外设虚拟化：

| 外设 | 模块 ID | 说明 |
|------|---------|------|
| SIUL2 | HYPER_SIUL2_ID | GPIO 控制 |
| ME | HYPER_ME_ID | 模块使能 |
| PCU | HYPER_PCU_ID | 电源控制 |
| RGM | HYPER_RGM_ID | 复位管理 |
| EDMA | HYPER_EDMA_ID | DMA 控制器 |
| AGT | HYPER_AGT_ID | 定时器 |
| GST | HYPER_GST_ID | 通用定时器 |
| IRQ | HYPER_IRQ_ID | 中断管理 |

## 5.4 HVC 调用协议

### 5.4.1 HVC ID 格式

Hypervisor 使用 `HYPER_MAKE_FUNCT_ID` 宏定义 HVC 调用 ID：

```c
// 定义在 hyper.h
#define HYPER_MAKE_FUNCT_ID(module, instance, func)      \
    ((((module) << 21) & 0xFFE00000UL) |                  \
     (((instance) << 16) & 0x001F0000UL) |               \
     ((func) & 0x0000FFFFUL))
```

**位域分配：**
- bit[31:21] - Module ID (模块 ID)
- bit[20:16] - Instance ID (实例 ID)
- bit[15:0] - Function ID (功能 ID)

### 5.4.2 模块枚举

```c
typedef enum {
    HYPER_VMM_ID,   /**< VMM module */
    HYPER_SCHED_ID, /**< Scheduler module */
    HYPER_OTA_ID,   /**< OTA module */
    HYPER_SIUL2_ID, /**< SIUL2 module */
    HYPER_ME_ID,    /**< ME module */
    HYPER_PCU_ID,   /**< PCU module */
    HYPER_RGM_ID,   /**< RGM module */
    HYPER_EDMA_ID,  /**< EDMA module */
    HYPER_AGT_ID,   /**< AGT module */
    HYPER_GST_ID,   /**< GST module */
    HYPER_IRQ_ID,   /**< IRQ module */
} hyper_module_t;
```

### 5.4.3 服务分发流程

```c
// hyp_services.c 中的服务分发
uint32_t el2_services(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t module_id = HYPER_GET_MODULE_ID(args->regs[0]);

    switch (module_id) {
    case HYPER_VMM_ID:
        vmm_hyper_exec(vm_id, args);
        break;
    case HYPER_SCHED_ID:
        sched_hyper_exec(vm_id, args);
        break;
    case HYPER_AGT_ID:
        agt_hyper_exec(args);
        break;
    // ... 其他模块
    }
    return args->regs[0];
}
```

## 5.5 构建系统

### 5.5.1 Makefile 结构

Hypervisor 使用 Makefile 构建：

```makefile
# 核心源文件
MODULE_C_SRCS += \
    arch/src/hyp_handler.c \
    arch/src/hyp_services.c

# 汇编源文件
MODULE_A_SRCS += \
    arch/src/el2_vectors.S \
    arch/src/hyp_exceptions.S
```

### 5.5.2 编译选项

关键编译选项：

```makefile
# EL2 模式编译
CFLAGS += -mel2
CFLAGS += -marm

# Thumb 模式
CFLAGS += -mthumb
```

## 5.6 本章小结

本章介绍了 Stellar SDK Hypervisor 的整体架构：

1. **架构层次**：从硬件到 VM 的完整层次结构
2. **目录结构**：各组件的组织方式
3. **核心组件**：Hypervisor Core、Scheduler、HAL
4. **HVC 协议**：模块调用的编码格式
5. **构建系统**：Makefile 构建配置

下一章将详细介绍 EL2 异常向量与启动流程。

---

*参考资料：*
- `parts/virt/st_hypervisor/arch/include/hyper.h`
- `parts/virt/st_hypervisor/arch/src/hyp_services.c`
- Stellar SDK 构建文档

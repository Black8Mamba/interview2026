# 第 8 章：异常处理模块

## 8.1 异常处理概述

异常处理是 Hypervisor 的核心功能之一，负责处理来自 VM 的各类异常。在 ARMv8-R 架构中，异常处理通过 EL2 级别的异常向量实现。

### 8.1.1 异常类型

在 Stellar SDK 中，支持以下异常类型：

| 异常类型 | 描述 | 处理方式 |
|----------|------|----------|
| HVC | Hypervisor 调用 | 分发到对应模块处理 |
| SVC | 系统调用 | 转发给 VM 或 Hypervisor 处理 |
| 数据中止 | 内存访问错误 | 错误处理或 VM 终止 |
| 预取中止 | 指令获取错误 | 错误处理 |
| IRQ | 外部中断 | 中断处理和虚拟化 |
| FIQ | 快速中断 | 中断处理 |

## 8.2 EL2 异常向量实现

### 8.2.1 向量表定义

参考源码：`parts/virt/st_hypervisor/arch/src/el2_vectors.S`

向量表定义了 544 个中断向量入口：

```assembly
// parts/virt/st_hypervisor/arch/src/el2_vectors.S

.section    .vectors, "ax"

.globl      _vectors
_vectors:
    .long       vector0,    vector1,    vector2,    vector3
    .long       vector4,    vector5,    vector6,    vector7
    // ... 更多向量 (vector0 ~ vector543)
```

### 8.2.2 向量结构设计

Stellar SDK 使用扁平化的向量表，每个向量对应一个中断号：

```c
// 向量号与中断号的对应关系
// vector[N] 对应中断号 N
// 支持 0 ~ 543 号中断，共 544 个中断
```

### 8.2.3 默认向量实现

每个向量默认是一个简单的返回：

```assembly
.type vector0, _ASM_FUNCTION_
vector0:
    bx          lr      // 直接返回，不做处理
```

### 8.2.4 未处理中断

对于未处理的中断，跳转到死循环：

```assembly
.type irq_unhandled, _ASM_FUNCTION_
_unhandled_irq:
    b       _unhandled_irq     // 死循环，等待调试
```

## 8.3 异常处理流程

### 8.3.1 处理流程概述

```
异常发生
    ↓
处理器自动保存状态 (SPSR_EL2, ELR_EL2)
    ↓
跳转到异常向量
    ↓
el2_hyp_entry_exception (汇编入口)
    ↓
el2_hypervisor_handler (C 语言处理)
    ↓
读取 HSR 判断异常类型
    ↓
调用对应服务处理
    ↓
ERET 返回到 VM
```

### 8.3.2 汇编入口

参考源码：`parts/virt/st_hypervisor/arch/src/hyp_exceptions.S`

```assembly
// parts/virt/st_hypervisor/arch/src/hyp_exceptions.S

.global el2_hyp_entry_exception
.type el2_hyp_entry_exception, _ASM_FUNCTION_
el2_hyp_entry_exception:
    // 保存使用的寄存器
    push {r4-r12, lr}

    // 加载 C 语言处理函数地址
    ldr r4, =el2_hypervisor_handler
    orr r4, r4, 0x1      // 设置 Thumb 模式位

    // 调用 C 语言处理函数
    blx r4

    // 恢复寄存器
    pop {r4-r12, lr}

    // 返回到 VM
    eret
```

**设计分析：**
- 使用 `push`/`pop` 保存和恢复寄存器
- 调用 C 函数 `el2_hypervisor_handler`
- 使用 `eret` 返回

### 8.3.3 C 语言处理函数

参考源码：`parts/virt/st_hypervisor/arch/src/hyp_handler.c`

```c
// parts/virt/st_hypervisor/arch/src/hyp_handler.c

uint32_t el2_hypervisor_handler(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4,
                                uint32_t arg5)
{
    uint32_t vl_hsr_syndrome;
    uint32_t vl_hsr_ec;
    uint32_t vsctlr_EL1;
    uint32_t vsctlr_EL2;

    uint32_t    ret = 0;
    plat_regs_t args;

    // 1. 读取 HSR (Hypervisor Syndrome Register)
    vl_hsr_syndrome = read_hsr();
    vl_hsr_ec       = ((vl_hsr_syndrome >> 26) & 0x3F);  // Exception Class

    // 2. 读取 VSCTLR 获取当前 VM ID
    vsctlr_EL1 = read_vsctlr();

    // 3. 设置 EL2 VMID
    vsctlr_EL2 = HYP_VMID;
    set_vsctlr(vsctlr_EL2);

    // 4. 根据异常类型分发处理
    switch (vl_hsr_ec) {
    case HYPER_SYNDROME_HVC:
        // 处理 HVC 调用
        args.regs[0] = arg0;
        args.regs[1] = arg1;
        args.regs[2] = arg2;
        args.regs[3] = arg3;
        args.regs[4] = arg4;
        args.regs[5] = arg5;
        ret = el2_services(vsctlr_EL1, &args);
        break;

    case HYPER_SYNDROME_SVC:
        // 处理 SVC
        break;

    // ... 其他异常类型

    default:
        break;
    }

    // 5. 恢复原始 VMID
    set_vsctlr(vsctlr_EL1);

    return ret;
}
```

## 8.4 HSR 异常识别

### 8.4.1 HSR 寄存器

HSR（Hypervisor Syndrome Register）包含异常的详细信息：

| 位域 | 说明 |
|------|------|
| EC | Exception Class (异常类别) |
| IL | Instruction Length |
| ISS | Instruction Specific Syndrome |

### 8.4.2 Exception Class 定义

参考源码：`parts/virt/st_hypervisor/arch/src/hyp_handler.c`

```c
#define HYPER_SYNDROME_UNKNOWN                      0x00UL
#define HYPER_SYNDROME_WFE_WFI                      0x01UL
#define HYPER_SYNDROME_MCR_ACCESS_F                 0x03UL
#define HYPER_SYNDROME_MCRR_ACCESS                  0x04UL
#define HYPER_SYNDROME_MCR_ACCESS_E                  0x05UL
#define HYPER_SYNDROME_LDC_STC_ACCESS               0x06UL
#define HYPER_SYNDROME_SIMD_FPU_ACCESS              0x07UL
#define HYPER_SYNDROME_MCR_MRC_ACCESS               0x08UL
#define HYPER_SYNDROME_MCRR_MRRC_ACCESS             0x0CUL
#define HYPER_SYNDROME_ILL_STATE_PC_FAULT_AARCH32  0x0EUL
#define HYPER_SYNDROME_SVC                          0x11UL
#define HYPER_SYNDROME_HVC                          0x12UL
#define HYPER_SYNDROME_PREFETCH_ABORT_ROUTED_AT_HYP 0x20UL
#define HYPER_SYNDROME_PREFETCH_ABORT_TAKEN_AT_HYP  0x21UL
#define HYPER_SYNDROME_ILL_STATE_PC_FAULT          0x22UL
#define HYPER_SYNDROME_DATA_ABORT_ROUTED_AT_HYP     0x24UL
#define HYPER_SYNDROME_DATA_ABORT_TAKEN_AT_HYP      0x25UL
```

### 8.4.3 关键异常处理

**HVC 异常处理（0x12）：**

```c
case HYPER_SYNDROME_HVC:
    // 提取参数
    args.regs[0] = arg0;
    args.regs[1] = arg1;
    args.regs[2] = arg2;
    args.regs[3] = arg3;
    args.regs[4] = arg4;
    args.regs[5] = arg5;

    // 调用服务处理
    ret = el2_services(vsctlr_EL1, &args);
    break;
```

**SVC 异常处理（0x11）：**

```c
case HYPER_SYNDROME_SVC:
    // SVC 通常转发给 VM 处理
    // 或者由 Hypervisor 模拟
    break;
```

## 8.5 服务分发

### 8.5.1 服务分发函数

参考源码：`parts/virt/st_hypervisor/arch/src/hyp_services.c`

```c
// parts/virt/st_hypervisor/arch/src/hyp_services.c

uint32_t el2_services(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t ret = 0;
    uint32_t module_id;

    // 从 HVC ID 中提取模块 ID
    module_id = HYPER_GET_MODULE_ID(args->regs[0]);

    switch (module_id) {
    case HYPER_VMM_ID:
        vmm_hyper_exec(vm_id, args);
        ret = args->regs[0];
        break;

    case HYPER_SCHED_ID:
        sched_hyper_exec(vm_id, args);
        ret = args->regs[0];
        break;

    case HYPER_SIUL2_ID:
        siul_hyper_exec(vm_id, args);
        ret = args->regs[0];
        break;

    case HYPER_AGT_ID:
        agt_hyper_exec(args);
        ret = args->regs[0];
        break;

    // ... 更多模块

    default:
        break;
    }

    return ret;
}
```

### 8.5.2 模块处理函数类型

```c
// 模块处理函数类型定义
typedef void (*hyper_module_exec_t)(uint32_t vm_id, plat_regs_t *args);

// 各模块的弱定义实现
extern void __attribute__((weak)) vmm_hyper_exec(uint32_t vm_id, plat_regs_t *args);
extern void __attribute__((weak)) sched_hyper_exec(uint32_t vm_id, plat_regs_t *args);
extern void __attribute__((weak)) agt_hyper_exec(plat_regs_t *args);
```

## 8.6 GIC 集成

### 8.6.1 中断控制器概述

Stellar SDK 使用 GICv2（Generic Interrupt Controller version 2）：

- 支持最多 544 个中断源
- 支持优先级嵌套
- 支持虚拟中断注入

### 8.6.2 中断处理流程

```
外设产生中断
    ↓
GIC 报告到 EL2 (Hypervisor)
    ↓
Hypervisor 读取中断号
    ↓
判断目标 VM
    ↓
注入虚拟中断到目标 VM
    ↓
VM IRQ 处理程序执行
```

### 8.6.3 中断虚拟化

Hypervisor 可以配置虚拟中断：

```c
// 注入虚拟中断到 VM
void vm_inject_irq(uint32_t vm_id, uint32_t irq)
{
    // 设置虚拟中断
    gic_set_virq_pending(vm_id, irq);
}
```

## 8.7 核心函数分析

### 8.7.1 函数调用关系

```
el2_hyp_entry_exception (汇编)
    ↓
el2_hypervisor_handler (C)
    ↓
el2_services (服务分发)
    ↓
[模块处理函数]
    - sched_hyper_exec
    - siul_hyper_exec
    - agt_hyper_exec
    - ...
```

### 8.7.2 关键设计点

1. **VMID 管理**：切换 VM 时保存/恢复 VMID
2. **状态保存**：异常处理前保存 VM 状态
3. **服务分发**：统一的模块化服务分发机制
4. **弱定义**：模块处理函数使用弱定义，便于扩展

## 8.8 本章小结

本章详细介绍了异常处理模块：

1. **异常类型**：HVC、SVC、IRQ 等各类异常
2. **向量表**：544 个中断向量的实现
3. **处理流程**：从异常发生到返回的完整流程
4. **HSR 识别**：通过 HSR 判断异常类型
5. **服务分发**：统一的模块化服务分发
6. **GIC 集成**：中断控制器的虚拟化支持

---

*参考资料：*
- `parts/virt/st_hypervisor/arch/src/el2_vectors.S`
- `parts/virt/st_hypervisor/arch/src/hyp_exceptions.S`
- `parts/virt/st_hypervisor/arch/src/hyp_handler.c`
- `parts/virt/st_hypervisor/arch/src/hyp_services.c`

# 第 7 章：HVC (Hypervisor Call) 机制

## 7.1 HVC 指令原理

### 7.1.1 概述

HVC（Hypervisor Call）是 ARM 架构中用于从 Guest（VM）调用 Hypervisor 服务的指令。当 VM 需要请求 Hypervisor 服务（如访问外设、调度控制等）时，执行 HVC 指令。

### 7.1.2 HVC 指令格式

```assembly
HVC #imm
```

- **#imm**：立即数，指定要调用的服务号（0-65535）

### 7.1.3 异常产生

当 VM 执行 HVC 指令时：

1. 触发同步异常
2. 处理器切换到 EL2
3. 跳转到 HVC 异常向量
4. 执行 Hypervisor 处理程序

## 7.2 Hypervisor 服务调用

### 7.2.1 调用流程

```
VM (EL1)                  Hypervisor (EL2)
   |                            |
   |  HVC #0                   |
   |--------------------------> |
   |                            |
   |  处理请求                  |
   |                            |
   |  返回结果                  |
   |<--------------------------|
   |                            |
```

### 7.2.2 参数传递

参数通过寄存器传递：

```c
// 参数结构定义
typedef struct {
    uint32_t regs[8];   // r0-r7
} plat_regs_t;
```

- `regs[0]`：HVC ID（包含模块、实例、功能 ID）
- `regs[1-5]`：其他参数
- `regs[6-7]`：返回值

### 7.2.3 调用示例

VM 调用 Hypervisor 服务的示例：

```c
// VM 侧调用示例
void vm_request_service(uint32_t module, uint32_t instance, uint32_t func, uint32_t arg)
{
    plat_regs_t args;

    // 构造 HVC ID
    args.regs[0] = HYPER_MAKE_FUNCT_ID(module, instance, func);
    args.regs[1] = arg;

    // 调用 Hypervisor
    _hyper_trampoline(&args);

    // 获取返回值
    return args.regs[0];
}
```

## 7.3 HVC 处理流程

### 7.3.1 异常识别

在 `hyp_handler.c` 中，通过读取 HSR（Hypervisor Syndrome Register）判断异常类型：

```c
// parts/virt/st_hypervisor/arch/src/hyp_handler.c

uint32_t el2_hypervisor_handler(uint32_t arg0, uint32_t arg1, ...)
{
    uint32_t hsr = read_hsr();
    uint32_t hsr_ec = ((hsr >> 26) & 0x3F);  // Exception Class

    switch (hsr_ec) {
    case HYPER_SYNDROME_HVC:
        // 处理 HVC 调用
        break;
    // ... 其他异常类型
    }
}
```

### 7.3.2 HVC 处理步骤

1. **读取参数**：从寄存器获取 HVC ID 和参数
2. **解析 ID**：分解出 Module/Instance/Function ID
3. **VMID 获取**：从 VSCTLR 获取当前 VM ID
4. **服务分发**：调用对应的模块处理函数
5. **返回结果**：将结果写入寄存器

```c
// 服务分发
uint32_t el2_services(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t module_id = HYPER_GET_MODULE_ID(args->regs[0]);
    uint32_t instance_id = HYPER_GET_INSTANCE_ID(args->regs[0]);
    uint32_t func_id = HYPER_GET_FUNCTION_ID(args->regs[0]);

    switch (module_id) {
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

### 7.3.3 返回处理

处理完成后，通过 ERET 指令返回到 VM：

```assembly
; 异常处理完成后
pop {r4-r12, lr}
eret    ; 返回到 VM
```

## 7.4 VM 与 Hypervisor 通信

### 7.4.1 通信方式

VM 与 Hypervisor 之间的通信主要通过：

1. **HVC 调用**：同步请求-响应模式
2. **虚拟中断**：异步通知
3. **共享内存**：大数据传输

### 7.4.2 HVC 服务类型

| 服务类型 | 模块 ID | 说明 |
|----------|---------|------|
| VMM | HYPER_VMM_ID | 虚拟机管理 |
| Scheduler | HYPER_SCHED_ID | 调度控制 |
| SIUL2 | HYPER_SIUL2_ID | GPIO 控制 |
| AGT | HYPER_AGT_ID | 定时器控制 |
| EDMA | HYPER_EDMA_ID | DMA 控制 |
| IRQ | HYPER_IRQ_ID | 中断管理 |

### 7.4.3 典型服务调用示例

**调度器服务调用：**

```c
// 终止 VM
void vm_terminate(uint32_t vm_id)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_SCHED_ID, 0, SCHED_TERMINATEVM_ID);
    args.regs[1] = vm_id;

    _hyper_trampoline(&args);
}
```

**AGT 定时器服务：**

```c
// 启动定时器
void agt_start(uint32_t instance, uint32_t period)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_AGT_ID, instance, AGT_START);
    args.regs[1] = period;

    _hyper_trampoline(&args);
}
```

## 7.5 HVC 接口设计

### 7.5.1 设计原则

1. **简洁性**：HVC ID 编码清晰
2. **可扩展性**：支持新增模块和功能
3. **安全性**：验证参数合法性
4. **性能**：快速路径处理常见请求

### 7.5.2 模块化设计

每个外设模块有独立的处理函数：

```c
// 模块处理函数类型
typedef void (*hyper_module_exec_t)(uint32_t vm_id, plat_regs_t *args);

// 模块注册
typedef struct {
    uint32_t module_id;
    hyper_module_exec_t exec;
} hyper_module_t;

// 模块表
static hyper_module_t modules[] = {
    {HYPER_SCHED_ID, sched_hyper_exec},
    {HYPER_SIUL2_ID, siul_hyper_exec},
    {HYPER_AGT_ID, agt_hyper_exec},
    // ... 更多模块
};
```

### 7.5.3 错误处理

```c
uint32_t el2_services(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t module_id = HYPER_GET_MODULE_ID(args->regs[0]);

    // 参数验证
    if (module_id >= MODULE_MAX) {
        args->regs[0] = ERROR_INVALID_MODULE;
        return ERROR_INVALID_MODULE;
    }

    // 调用模块处理函数
    // ...

    return ERROR_OK;
}
```

## 7.6 本章小结

本章详细介绍了 HVC（Hypervisor Call）机制：

1. **HVC 指令原理**：从 VM 调用 Hypervisor 服务的指令
2. **服务调用**：参数传递和调用流程
3. **处理流程**：HSR 识别、服务分发、返回处理
4. **VM 通信**：HVC 是 VM 与 Hypervisor 通信的主要方式
5. **接口设计**：模块化、可扩展的服务接口

---

*参考资料：*
- `parts/virt/st_hypervisor/arch/src/hyp_handler.c`
- `parts/virt/st_hypervisor/arch/src/hyp_services.c`
- `parts/virt/st_hypervisor/arch/include/hyper.h`

# 第 13 章：添加新的外设虚拟化支持

## 13.1 设计思路

### 13.1.1 概述

本章介绍如何为 Stellar SDK Hypervisor 添加新的外设虚拟化支持。以添加一个自定义外设为例，说明完整的实现步骤。

### 13.1.2 设计要点

1. **模块化设计**：遵循现有的 HAL 层架构
2. **权限控制**：确保 VM 只访问授权的外设
3. **接口统一**：使用标准的 HVC 调用方式

## 13.2 HAL 层实现步骤

### 13.2.1 定义模块 ID

在 `hyper.h` 中添加新的模块 ID：

```c
// parts/virt/st_hypervisor/arch/include/hyper.h

#define HYPER_CUSTOM_PERIPH_ID  0x0C  // 新的外设模块 ID
```

### 13.2.2 创建 Hypervisor 侧 HAL

创建 `parts/virt/st_hypervisor/hal/hyper/custom_hyp.c`：

```c
#include <typedefs.h>
#include <hyper.h>

// 硬件访问函数
static void custom_periph_write(uint32_t offset, uint32_t value)
{
    volatile uint32_t *reg = (volatile uint32_t *)(CUSTOM_PERIPH_BASE + offset);
    *reg = value;
}

static uint32_t custom_periph_read(uint32_t offset)
{
    volatile uint32_t *reg = (volatile uint32_t *)(CUSTOM_PERIPH_BASE + offset);
    return *reg;
}

// 外设虚拟化处理函数
void custom_hyper_exec(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t func_id = HYPER_GET_FUNCTION_ID(args->regs[0]);
    uint32_t instance = HYPER_GET_INSTANCE_ID(args->regs[0]);

    // 权限检查
    if (!check_custom_periph_permission(vm_id, instance)) {
        args->regs[0] = ERROR_NO_PERMISSION;
        return;
    }

    switch (func_id) {
    case CUSTOM_WRITE_REG:
        custom_periph_write(args->regs[1], args->regs[2]);
        args->regs[0] = 0;
        break;

    case CUSTOM_READ_REG:
        args->regs[0] = custom_periph_read(args->regs[1]);
        break;

    case CUSTOM_START:
        custom_periph_start(instance);
        args->regs[0] = 0;
        break;

    case CUSTOM_STOP:
        custom_periph_stop(instance);
        args->regs[0] = 0;
        break;

    default:
        args->regs[0] = ERROR_INVALID_FUNCTION;
        break;
    }
}
```

### 13.2.3 创建 VM 侧 HAL

创建 `parts/virt/st_hypervisor/hal/vm/custom_hvc.c`：

```c
#include <typedefs.h>
#include <hyper.h>

// 写寄存器
int custom_write_reg(uint32_t instance, uint32_t offset, uint32_t value)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_CUSTOM_PERIPH_ID, instance, CUSTOM_WRITE_REG);
    args.regs[1] = offset;
    args.regs[2] = value;

    _hyper_trampoline(&args);

    return args.regs[0];
}

// 读寄存器
uint32_t custom_read_reg(uint32_t instance, uint32_t offset)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_CUSTOM_PERIPH_ID, instance, CUSTOM_READ_REG);
    args.regs[1] = offset;

    _hyper_trampoline(&args);

    return args.regs[0];
}

// 启动外设
int custom_start(uint32_t instance)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_CUSTOM_PERIPH_ID, instance, CUSTOM_START);

    _hyper_trampoline(&args);

    return args.regs[0];
}

// 停止外设
int custom_stop(uint32_t instance)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_CUSTOM_PERIPH_ID, instance, CUSTOM_STOP);

    _hyper_trampoline(&args);

    return args->regs[0];
}
```

## 13.3 VM 接口定义

### 13.3.1 头文件定义

创建 `parts/virt/st_hypervisor/hal/vm/custom.h`：

```c
#ifndef _CUSTOM_H_
#define _CUSTOM_H_

#include <stdint.h>

// 功能 ID 定义
#define CUSTOM_WRITE_REG    0x01
#define CUSTOM_READ_REG     0x02
#define CUSTOM_START        0x03
#define CUSTOM_STOP         0x04

// API 函数
int custom_write_reg(uint32_t instance, uint32_t offset, uint32_t value);
uint32_t custom_read_reg(uint32_t instance, uint32_t offset);
int custom_start(uint32_t instance);
int custom_stop(uint32_t instance);

#endif // _CUSTOM_H_
```

## 13.4 注册到服务分发

### 13.4.1 添加模块处理函数

在 `hyp_services.c` 中添加：

```c
// parts/virt/st_hypervisor/arch/src/hyp_services.c

extern void custom_hyper_exec(uint32_t vm_id, plat_regs_t *args);

uint32_t el2_services(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t module_id = HYPER_GET_MODULE_ID(args->regs[0]);

    switch (module_id) {
    // ... 其他模块

    case HYPER_CUSTOM_PERIPH_ID:
        custom_hyper_exec(vm_id, args);
        ret = args->regs[0];
        break;

    default:
        break;
    }

    return ret;
}
```

## 13.5 VM 配置

### 13.5.1 添加外设权限

在 VM 配置中添加外设权限：

```c
// vm_config.h

#define PERIPH_CUSTOM    (1 << 11)  // 自定义外设

// VM 配置中添加
static const vm_config_t vm_config[VM_MAX_NUMBER] = {
    {
        .vmid = 0,
        .periph_mask = PERIPH_UART5 | PERIPH_GPIO | PERIPH_CUSTOM,
        // ...
    },
};
```

## 13.6 测试验证

### 13.6.1 VM 中测试代码

```c
// 在 VM 中使用自定义外设

#include <custom.h>

void test_custom_periph(void)
{
    // 启动外设
    custom_start(0);

    // 写寄存器
    custom_write_reg(0, 0x00, 0x12345678);

    // 读寄存器
    uint32_t value = custom_read_reg(0, 0x00);

    // 停止外设
    custom_stop(0);
}
```

### 13.6.2 测试步骤

1. **编译 Hypervisor**：确保新模块编译通过
2. **配置 VM**：在 VM 配置中启用外设权限
3. **运行测试**：在 VM 中调用外设 API
4. **验证结果**：检查返回值和预期行为

## 13.7 本章小结

本章介绍了如何添加新的外设虚拟化支持：

1. **设计思路**：模块化设计和权限控制
2. **HAL 层实现**：Hypervisor 侧和 VM 侧的实现
3. **服务注册**：添加到服务分发机制
4. **VM 配置**：配置外设权限
5. **测试验证**：完整的测试流程

---

*参考资料：*
- `parts/virt/st_hypervisor/hal/hyper/agt_hyp.c`
- `parts/virt/st_hypervisor/hal/vm/agt_hvc.c`

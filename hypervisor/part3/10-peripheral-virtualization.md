# 第 10 章：外设虚拟化框架

## 10.1 HAL 层架构

### 10.1.1 概述

HAL（Hardware Abstraction Layer）层是 Stellar SDK Hypervisor 的重要组成部分，负责外设虚拟化。HAL 层分为两部分：

- **Hypervisor 侧 HAL**：运行在 EL2，负责外设的真正访问和控制
- **VM 侧 HAL**：运行在 EL1（VM 中），提供虚拟外设接口

### 10.1.2 架构层次

```
+-------------------+
|    Guest VMs     |
+-------------------+
|  VM Side HAL    |  ← EL1 (VM)
+-------------------+
        | HVC
        v
+-------------------+
| Hypervisor Side  |  ← EL2 (Hypervisor)
+-------------------+
|  - Module HAL    |
|  - Physical HW   |
+-------------------+
```

### 10.1.3 目录结构

```
parts/virt/st_hypervisor/hal/
├── hyper/                    # Hypervisor 侧 HAL
│   ├── agt_hyp.c            # AGT 定时器虚拟化
│   ├── edma_hyp.c           # EDMA 虚拟化
│   ├── siul2_hyp.c          # GPIO 虚拟化
│   └── ...
├── vm/                      # VM 侧 HAL
│   ├── agt_hvc.c            # AGT HVC 调用
│   ├── edma_hvc.c           # EDMA HVC 调用
│   └── ...
└── include/                 # HAL 头文件
```

## 10.2 外设虚拟化原理

### 10.2.1 虚拟化方式

外设虚拟化主要有三种方式：

| 方式 | 描述 | 适用场景 |
|------|------|----------|
| 透传（Passthrough） | VM 直接访问物理外设 | 高性能需求 |
| 模拟（Emulation） | Hypervisor 模拟外设行为 | 完全隔离 |
| 半虚拟化（Para-virtualization） | VM 调用 HVC 接口 | 平衡性能和隔离 |

### 10.2.2 访问控制

Hypervisor 通过以下机制控制 VM 对外设的访问：

1. **MPU 配置**：限制 VM 访问外设地址空间
2. **HVC 调用**：VM 不能直接访问外设，必须通过 HVC
3. **权限检查**：Hypervisor 验证 VM 的访问权限

## 10.3 主要外设虚拟化实现

### 10.3.1 AGT 定时器虚拟化

AGT（Asynchronous General-Purpose Timer）是 Stellar MCU 的定时器外设。

**Hypervisor 侧：**

参考源码：`parts/virt/st_hypervisor/hal/hyper/agt_hyp.c`

```c
// AGT 定时器虚拟化处理
void agt_hyper_exec(plat_regs_t *args)
{
    uint32_t func_id = HYPER_GET_FUNCTION_ID(args->regs[0]);
    uint32_t instance = HYPER_GET_INSTANCE_ID(args->regs[0]);

    switch (func_id) {
    case AGT_START:
        // 启动定时器
        agt_start(instance, args->regs[1]);
        break;

    case AGT_STOP:
        // 停止定时器
        agt_stop(instance);
        break;

    case AGT_GET_COUNTER:
        // 获取计数器值
        args->regs[0] = agt_get_counter(instance);
        break;

    default:
        break;
    }
}
```

**VM 侧：**

参考源码：`parts/virt/st_hypervisor/hal/vm/agt_hvc.c`

```c
// VM 调用 AGT 服务
void agt_start(uint32_t instance, uint32_t period)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_AGT_ID, instance, AGT_START);
    args.regs[1] = period;

    _hyper_trampoline(&args);
}

uint32_t agt_get_counter(uint32_t instance)
{
    plat_regs_t args;

    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_AGT_ID, instance, AGT_GET_COUNTER);

    _hyper_trampoline(&args);

    return args.regs[0];
}
```

### 10.3.2 SIUL2 GPIO 虚拟化

SIUL2（System Integration Unit Lite 2）控制 GPIO 引脚。

**Hypervisor 侧：**

参考源码：`parts/virt/st_hypervisor/hal/hyper/siul2_hyp.c`

```c
// SIUL2 GPIO 虚拟化处理
void siul_hyper_exec(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t func_id = HYPER_GET_FUNCTION_ID(args->regs[0]);

    // 检查 VM 是否有权限访问 GPIO
    if (!check_gpio_permission(vm_id, args->regs[1])) {
        args->regs[0] = ERROR_NO_PERMISSION;
        return;
    }

    switch (func_id) {
    case SIUL2_SET_PIN:
        // 设置引脚状态
        siul_set_pin(args->regs[1], args->regs[2]);
        break;

    case SIUL2_GET_PIN:
        // 获取引脚状态
        args->regs[0] = siul_get_pin(args->regs[1]);
        break;

    case SIUL2_CONFIG_PIN:
        // 配置引脚
        siul_config_pin(args->regs[1], args->regs[2]);
        break;

    default:
        break;
    }
}
```

### 10.3.3 EDMA 虚拟化

EDMA（Enhanced Direct Memory Access）控制器用于内存到内存的外设的数据传输。

**Hypervisor 侧：**

```c
// EDMA 虚拟化处理
void edma_hyper_exec(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t func_id = HYPER_GET_FUNCTION_ID(args->regs[0]);

    switch (func_id) {
    case EDMA_START_TRANSFER:
        // 检查 VM 权限
        if (!check_edma_permission(vm_id, args->regs[1])) {
            args->regs[0] = ERROR_NO_PERMISSION;
            return;
        }
        // 启动 DMA 传输
        edma_start(args->regs[1], args->regs[2]);
        break;

    case EDMA_STOP_TRANSFER:
        // 停止 DMA 传输
        edma_stop(args->regs[1]);
        break;

    case EDMA_GET_STATUS:
        // 获取 DMA 状态
        args->regs[0] = edma_get_status(args->regs[1]);
        break;

    default:
        break;
    }
}
```

## 10.4 HVC 接口设计

### 10.4.1 接口规范

每个外设模块的 HVC 接口遵循统一的规范：

```c
// 外设模块 HVC 接口
typedef struct {
    uint32_t module_id;           // 模块 ID
    uint32_t instance_id;          // 实例 ID
    uint32_t function_id;          // 功能 ID
    uint32_t num_params;          // 参数数量
    uint32_t (*handler)(void *);  // 处理函数
} peripheral_hvc_handler_t;
```

### 10.4.2 模块注册

```c
// 外设模块注册表
static peripheral_hvc_handler_t peripheral_handlers[] = {
    {HYPER_AGT_ID, 0, AGT_START, 2, agt_start_handler},
    {HYPER_AGT_ID, 0, AGT_STOP, 1, agt_stop_handler},
    {HYPER_SIUL2_ID, 0, SIUL2_SET_PIN, 2, siul2_set_pin_handler},
    // ... 更多外设
};
```

### 10.4.3 权限检查

```c
// VM 外设访问权限检查
int check_peripheral_permission(uint32_t vm_id, uint32_t peripheral_id)
{
    vm_config_t *vm_config = get_vm_config(vm_id);

    // 检查 VM 配置中是否包含该外设
    return (vm_config->periph_mask & (1 << peripheral_id)) != 0;
}
```

## 10.5 支持的外设列表

| 外设 | 模块 ID | 功能 |
|------|---------|------|
| SIUL2 | HYPER_SIUL2_ID | GPIO 控制 |
| ME | HYPER_ME_ID | 模块使能 |
| PCU | HYPER_PCU_ID | 电源控制 |
| RGM | HYPER_RGM_ID | 复位管理 |
| EDMA | HYPER_EDMA_ID | DMA 控制 |
| AGT | HYPER_AGT_ID | 定时器 |
| GST | HYPER_GST_ID | 通用定时器 |
| IRQ | HYPER_IRQ_ID | 中断管理 |

## 10.6 本章小结

本章详细介绍了外设虚拟化框架：

1. **HAL 层架构**：Hypervisor 侧和 VM 侧的分层设计
2. **虚拟化原理**：透传、模拟、半虚拟化三种方式
3. **AGT 定时器虚拟化**：完整实现示例
4. **SIUL2 GPIO 虚拟化**：带权限检查的实现
5. **EDMA 虚拟化**：DMA 传输的虚拟化
6. **HVC 接口设计**：统一的接口规范和权限检查

---

*参考资料：*
- `parts/virt/st_hypervisor/hal/hyper/agt_hyp.c`
- `parts/virt/st_hypervisor/hal/vm/agt_hvc.c`
- `parts/virt/st_hypervisor/hal/hyper/siul2_hyp.c`

# 第 14 章：调试方法与工具

## 14.1 调试环境搭建

### 14.1.1 硬件环境

调试 Stellar SDK Hypervisor 需要的硬件：

- **开发板**：Stellar SR6P3c4 开发板
- **调试器**：ST-Link 或兼容的 JTAG/SWD 调试器
- **调试主机**：运行调试软件的 PC

### 14.1.2 软件环境

需要的软件工具：

- **IDE**：IAR Embedded Workbench / Keil MDK / ARM GCC + GDB
- **调试器驱动**：ST-Link 驱动
- **串口工具**：Tera Term 或类似的串口终端

### 14.1.3 连接配置

```
PC (Debugger)  ←→  Debug Adapter  ←→  Stellar Board
    |                              |
    |                      +---------------+
    |                      | Cortex-R52   |
    |                      +---------------+
    |
    +→ Serial (UART) ←→ Debug Output
```

## 14.2 常见问题排查

### 14.2.1 VM 无法启动

**症状**：VM 启动后立即停止

**排查步骤**：

1. 检查 VM 配置是否正确
2. 检查入口地址是否有效
3. 检查内存分配是否足够
4. 检查外设权限配置

```c
// 添加调试信息
void vm_start_debug(vm_id_t vmid)
{
    printf("VM%d: Starting...\n", vmid);
    printf("Entry: 0x%08x\n", vm_table[vmid].entry_point);
    printf("Stack: 0x%08x\n", vm_table[vmid].stack_ptr);
}
```

### 14.2.2 外设访问失败

**症状**：VM 无法访问外设

**排查步骤**：

1. 检查 VM 的外设权限配置
2. 检查 HVC 调用参数是否正确
3. 检查 Hypervisor 日志

```c
// 添加权限检查调试
if (!check_peripheral_permission(vm_id, periph_id)) {
    printf("VM%d: No permission for periph %d\n", vm_id, periph_id);
    return ERROR_NO_PERMISSION;
}
```

### 14.2.3 调度异常

**症状**：VM 调度不正常

**排查步骤**：

1. 检查调度器初始化
2. 检查 VM 状态
3. 检查时间片配置

```c
// 调度调试
void sched_debug(void)
{
    printf("Current VM: %d\n", current_vm_id);
    for (int i = 0; i < VM_MAX_NUMBER; i++) {
        printf("VM%d: status=%d\n", i, vm_table[i].status);
    }
}
```

## 14.3 性能分析

### 14.3.1 性能指标

关键性能指标：

| 指标 | 描述 | 测量方法 |
|------|------|----------|
| VM 切换时间 | 上下文切换开销 | 计时器测量 |
| 中断延迟 | 中断响应时间 | 示波器/计时器 |
| HVC 调用延迟 | 服务调用开销 | 计时器测量 |

### 14.3.2 性能测量

```c
// VM 切换时间测量
uint32_t vm_switch_start, vm_switch_end;

void before_vm_switch(void)
{
    vm_switch_start = get_cycle_count();
}

void after_vm_switch(void)
{
    vm_switch_end = get_cycle_count();
    printf("VM switch time: %d cycles\n", vm_switch_end - vm_switch_start);
}

// HVC 调用延迟测量
void hvc_latency_test(void)
{
    uint32_t start, end;

    start = get_cycle_count();
    _hyper_trampoline(&args);
    end = get_cycle_count();

    printf("HVC latency: %d cycles\n", end - start);
}
```

### 14.3.3 优化建议

1. **减少 VM 切换**：合并短生命周期的 VM
2. **优化 HVC 处理**：使用缓存减少重复验证
3. **调整时间片**：根据 VM 优先级动态调整

## 14.4 日志系统

### 14.4.1 日志级别

```c
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} log_level_t;
```

### 14.4.2 日志输出

```c
#define LOG_ERROR(fmt, ...)   log_print(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   log_print(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   log_print(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)  log_print(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

void log_print(log_level_t level, const char *fmt, ...)
{
    if (level <= current_log_level) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}
```

### 14.4.3 调试日志示例

```c
// 异常处理日志
void el2_hypervisor_handler_debug(uint32_t hsr)
{
    uint32_t ec = (hsr >> 26) & 0x3F;

    LOG_DEBUG("Exception: EC=0x%02x\n", ec);

    switch (ec) {
    case HYPER_SYNDROME_HVC:
        LOG_INFO("HVC call from VM\n");
        break;
    case HYPER_SYNDROME_DATA_ABORT_TAKEN_AT_HYP:
        LOG_ERROR("Data abort!\n");
        break;
    default:
        LOG_WARN("Unknown exception: 0x%02x\n", ec);
        break;
    }
}
```

## 14.5 调试技巧

### 14.5.1 断点设置

在调试时设置断点：

1. **Hypervisor 入口**：在 `hyper_init()` 设置断点
2. **HVC 处理**：在 `el2_services()` 设置断点
3. **调度函数**：在 `schedule()` 设置断点
4. **外设处理**：在外设处理函数设置断点

### 14.5.2 内存检查

```c
// 检查 VM 内存区域
void dump_vm_memory(vm_id_t vmid)
{
    vm_config_t *config = get_vm_config(vmid);

    printf("VM%d Memory:\n", vmid);
    printf("  Base:   0x%08x\n", config->memory_base);
    printf("  Size:   0x%08x\n", config->memory_size);

    // dump 内存内容
    hexdump(config->memory_base, 256);
}
```

### 14.5.3 寄存器检查

```c
// 打印关键寄存器
void print_registers(void)
{
    uint32_t spsr, elr, vbar;

    asm volatile("mrs %0, spsr_el2" : "=r"(spsr));
    asm volatile("mrs %0, elr_el2" : "=r"(elr));
    asm volatile("mrs %0, vbar_el2" : "=r"(vbar));

    printf("SPSR_EL2: 0x%08x\n", spsr);
    printf("ELR_EL2: 0x%08x\n", elr);
    printf("VBAR_EL2: 0x%08x\n", vbar);
}
```

## 14.6 本章小结

本章介绍了调试方法与工具：

1. **调试环境**：硬件和软件环境搭建
2. **问题排查**：VM 无法启动、外设访问失败、调度异常
3. **性能分析**：性能指标测量和优化建议
4. **日志系统**：日志级别和调试输出
5. **调试技巧**：断点、内存检查、寄存器检查

---

*参考资料：*
- Stellar SDK 调试文档
- ARM Cortex-R52 调试手册

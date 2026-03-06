# 第 12 章：VM0 创建实例分析

## 12.1 VM0 源码结构

### 12.1.1 概述

VM0 是 Stellar SDK Hypervisor 的演示虚拟机，基于 FreeRTOS 运行，提供 LED 闪烁、UART 通信等功能。

### 12.1.2 源码路径

参考源码：`parts/virt/st_hypervisor/demos/Hypervisor/src/vm/vm0/main.c`

### 12.1.3 目录结构

```
demos/Hypervisor/
├── src/
│   ├── cluster0/core0/hyper/
│   │   └── hyper.c           # Hypervisor 入口
│   ├── vm/
│   │   ├── vm0/
│   │   │   └── main.c       # VM0 主程序
│   │   ├── vm1/
│   │   │   └── main.c
│   │   └── ...
│   └── user_mpu_config.c    # MPU 配置
└── include/
    └── vm_config/
        └── sr6p3/
            └── vm_config.h   # VM 配置
```

## 12.2 VM 配置文件分析

### 12.2.1 VM 配置结构

参考源码：`parts/virt/st_hypervisor/demos/Hypervisor/include/vm_config/sr6p3/vm_config.h`

```c
typedef struct {
    uint32_t vmid;                 // VM ID
    uint32_t priority;             // 调度优先级
    uint32_t time_slice;          // 时间片大小
    uint32_t memory_base;          // 内存基址
    uint32_t memory_size;          // 内存大小
    uint32_t entry_point;          // 入口地址
    uint32_t periph_mask;         // 外设访问掩码
} vm_config_t;
```

### 12.2.2 VM0 配置示例

```c
static const vm_config_t vm_config[VM_MAX_NUMBER] = {
    // VM0
    {
        .vmid = 0,
        .priority = 0,
        .time_slice = 1000,
        .memory_base = 0x10000000,
        .memory_size = 0x00100000,
        .entry_point = 0x10000000,
        .periph_mask = PERIPH_UART5 | PERIPH_GPIO | PERIPH_LED
    },
    // VM1
    {
        .vmid = 1,
        .priority = 1,
        .time_slice = 1000,
        .memory_base = 0x10100000,
        .memory_size = 0x00100000,
        .entry_point = 0x10100000,
        .periph_mask = PERIPH_UART6 | PERIPH_GPIO
    },
};
```

## 12.3 任务创建与运行

### 12.3.1 VM0 主函数

```c
// parts/virt/st_hypervisor/demos/Hypervisor/src/vm/vm0/main.c

int main(void)
{
    // 1. 系统初始化
    system_init();

    // 2. 打印欢迎信息
    printWelcomeMessage();

    // 3. 创建 FreeRTOS 任务
    xTaskCreate(vTask_LedBlink,      // 任务函数
                "LED Blink",           // 任务名称
                configMINIMAL_STACK_SIZE,  // 栈大小
                NULL,                 // 参数
                tskIDLE_PRIORITY + 1, // 优先级
                NULL);                // 任务句柄

    // 4. 启动调度器
    vTaskStartScheduler();

    // 不应到达这里
    for(;;);

    return 0;
}
```

### 12.3.2 LED 闪烁任务

```c
static void vTask_LedBlink(void *pvParameters)
{
    (void)pvParameters;

    // 配置 LED 引脚
    configure_led(LED1);

    while (1) {
        // 点亮 LED
        led_on(LED1);

        // 延时
        vTaskDelay(pdMS_TO_TICKS(500));

        // 熄灭 LED
        led_off(LED1);

        // 延时
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 12.3.3 VM 状态获取

```c
// VM 状态获取函数
const char* vm_status[] = {
    "VM not present",
    "Running",
    "Stopped",
    "Suspended"
};

// 获取 VM 状态
uint32_t status = vmm_get_vm_status(vm_id);
printf("VM%d: %s\n", vm_id, vm_status[status]);
```

## 12.4 代码流程详解

### 12.4.1 启动流程

```
复位
    ↓
Hypervisor 初始化 (EL2)
    ↓
调度器初始化
    ↓
启动 VM0 (EL1)
    ↓
VM0 main()
    ↓
创建 FreeRTOS 任务
    ↓
启动调度器
    ↓
LED 闪烁任务运行
```

### 12.4.2 VM 初始化

```c
void vm_init(vm_id_t vmid)
{
    // 1. 获取 VM 配置
    vm_config_t *config = get_vm_config(vmid);

    // 2. 配置 MPU
    user_mpu_config(vmid);

    // 3. 配置外设权限
    configure_periph_permissions(vmid, config->periph_mask);

    // 4. 设置入口地址
    set_entry_point(config->entry_point);
}
```

## 12.5 VM 间通信

### 12.5.1 共享内存

```c
// 定义共享内存
uint32_t shared_mem_vm0[256] __attribute__((section(".shared_mem_section")));

// VM 间共享内存地址
#ifndef sr6p3
#define IPC_REGION_BASE    0x64000400
#else
#define IPC_REGION_BASE    0x68000400
#endif
#define IPC_REGION_SIZE    (1 * 1024)
```

### 12.5.2 IPC 通信

```c
// VM 间通信
void ipc_send(vm_id_t target_vm, void *data, uint32_t size)
{
    // 写入共享内存
    memcpy(shared_mem, data, size);

    // 触发目标 VM 的中断
    vm_inject_irq(target_vm, IPC_IRQ);
}
```

## 12.6 本章小结

本章详细分析了 VM0 的实现：

1. **源码结构**：VM0 基于 FreeRTOS
2. **配置文件**：vm_config.h 中的配置
3. **任务创建**：FreeRTOS 任务创建流程
4. **启动流程**：从复位到任务运行的完整流程
5. **VM 间通信**：共享内存和 IPC 机制

---

*参考资料：*
- `parts/virt/st_hypervisor/demos/Hypervisor/src/vm/vm0/main.c`
- `parts/virt/st_hypervisor/demos/Hypervisor/include/vm_config/sr6p3/vm_config.h`

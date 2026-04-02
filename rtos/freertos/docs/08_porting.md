# 第八章：移植适配

> 本章目标：理解 FreeRTOS 移植层结构，了解常见芯片移植要点

## 章节结构

- [ ] 8.1 移植层概述
- [ ] 8.2 移植层文件结构
- [ ] 8.3 关键移植接口
- [ ] 8.4 STM32 移植示例
- [ ] 8.5 常见移植问题
- [ ] 8.6 面试高频问题
- [ ] 8.7 避坑指南

---

## 8.1 移植层概述

### 为什么需要移植层

FreeRTOS 内核是平台无关的，但需要硬件相关的代码：
- 任务切换（上下文保存/恢复）
- 中断管理
- 时钟节拍

### 移植层与内核的关系

```
┌─────────────────────────────────────┐
│           FreeRTOS 内核              │
│      (平台无关，tasks.c, queue.c)    │
├─────────────────────────────────────┤
│         移植层接口 (portable/)       │
│  - port.c     任务切换实现           │
│  - portmacro.h  宏定义               │
│  - heap_x.c   内存分配策略           │
├─────────────────────────────────────┤
│           硬件平台                   │
│  - ARM Cortex-M / RISC-V / MIPS     │
│  - 具体 MCU：STM32 / ESP32 等        │
└─────────────────────────────────────┘
```

---

## 8.2 移植层文件结构

### 标准 FreeRTOS 目录

```
FreeRTOS/
├── Source/
│   ├── tasks.c
│   ├── queue.c
│   ├── list.c
│   ├── timers.c
│   ├── event_groups.c
│   ├── croutine.c
│   └── portable/
│       ├── MemMang/          ← 堆策略
│       │   ├── heap_1.c
│       │   ├── heap_2.c
│       │   ├── heap_3.c
│       │   ├── heap_4.c
│       │   └── heap_5.c
│       ├── ARM_CM3/          ← Cortex-M3 移植
│       ├── ARM_CM4F/         ← Cortex-M4F 移植
│       ├── ARM_CM7/          ← Cortex-M7 移植
│       ├── RISC-V/           ← RISC-V 移植
│       └── GCC/              ← GCC 编译器适配
├── Demo/
│   └── ARM_CM4F_STM32F407ZG_Keil/
└── include/
```

### 移植层必需文件

| 文件 | 作用 |
|------|------|
| `port.c` | 任务切换实现、临界区实现 |
| `portmacro.h` | 端口相关宏定义（数据类型、开关中断） |
| `portmacro.c` | 可能包含部分宏实现 |

---

## 8.3 关键移植接口

### 1. 任务切换：portSAVE_CONTEXT / portRESTORE_CONTEXT

```c
// ARM Cortex-M 移植层关键代码
// 任务切换时保存和恢复寄存器

.macro portSAVE_CONTEXT
    // 将 R4-R11, LR 入栈到任务栈
    push {r4-r11, lr}
    // 获取当前任务栈指针，保存到 TCB
    mrs r0, psp
    isb
    stmdb r0!, {r4-r11}
    ldr r1, =pxCurrentTCB
    ldr r2, [r1]
    str r0, [r2]
    .endm

.macro portRESTORE_CONTEXT
    // 从 TCB 恢复任务栈指针
    ldr r1, =pxCurrentTCB
    ldr r2, [r1]
    ldr r0, [r2]
    // 弹栈恢复 R4-R11
    ldmia r0!, {r4-r11}
    // 恢复 PSP
    msr psp, r0
    // 返回，跳转到新任务
    bx lr
    .endm
```

### 2. 临界区宏

```c
// portmacro.h
#define portENTER_CRITICAL()         vPortEnterCritical()
#define portEXIT_CRITICAL()          vPortExitCritical()

#define portDISABLE_INTERRUPTS()     vPortSetInterruptMask()
#define portENABLE_INTERRUPTS()      vPortClearInterruptMask()

// 实现
static UBaseType_t uxCriticalNesting = 0;

void vPortEnterCritical(void) {
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;
}

void vPortExitCritical(void) {
    uxCriticalNesting--;
    if (uxCriticalNesting == 0) {
        portENABLE_INTERRUPTS();
    }
}
```

### 3. 中断屏蔽：portSET_INTERRUPT_MASK_FROM_ISR

```c
// 保存当前中断状态并屏蔽
#define portSET_INTERRUPT_MASK_FROM_ISR()      uxPortSetInterruptMask()

// 清除中断屏蔽，恢复之前状态
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)  vPortClearInterruptMask(x)

static UBaseType_t uxPortSetInterruptMask(void) {
    UBaseType_t uxPrevInterruptStatus;
    __asm volatile ("mrs %0, basepri \n orr %0, %1 \n msr basepri, %0" : "=&r" (uxPrevInterruptStatus) : "i" (configMAX_SYSCALL_INTERRUPT_PRIORITY));
    return uxPrevInterruptStatus;
}
```

### 4. 调度器启动：xPortStartScheduler

```c
BaseType_t xPortStartScheduler(void) {
    // 配置 PendSV 优先级
    *(portNVIC_SHPR3) |= portNVIC_PENDSV_PRI;

    // 配置 SysTick
    vPortSetupTimerInterrupt();

    // 启动第一个任务（不返回）
    vPortStartFirstTask();

    return pdTRUE;  // 不会执行到这里
}
```

### 5. SysTick 实现

```c
void vPortSetupTimerInterrupt(void) {
    // 设置重装载值
    portLONG ulSysReload = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1;
    portNVIC_SYSTICK_LOAD = ulSysReload;

    // 配置控制寄存器
    portNVIC_SYSTICK_CTRL = portNVIC_SYSTICK_CLK | portNVIC_SYSTICK_INT | portNVIC_SYSTICK_ENABLE;
}

void SysTick_Handler(void) {
    if (xTaskIncrementTick() != pdFALSE) {
        *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET;  // 触发 PendSV
    }
}
```

---

## 8.4 STM32 移植示例

### 典型 STM32 工程结构

```
Project/
├── Core/
│   ├── Inc/
│   │   ├── FreeRTOSConfig.h    ← 配置文件
│   │   ├── portmacro.h         ← 移植层头文件
│   │   └── stm32f4xx_hal_conf.h
│   ├── Src/
│   │   ├── main.c
│   │   ├── stm32f4xx_it.c       ← 中断处理
│   │   ├── port.c               ← 移植层实现
│   │   └── ...
│   └── Startup/
│       └── startup_stm32f407xx.s
├── Middlewares/
│   └── Third_Party/
│       └── FreeRTOS/
│           └── Source/
├── Drivers/
└── Makefile / Keil / IAR
```

### FreeRTOSConfig.h 关键配置

```c
// STM32F4 系列配置示例
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// 基础配置
#define configUSE_PREEMPTION        1
#define configCPU_CLOCK_HZ          ( SystemCoreClock )       // 168000000
#define configTICK_RATE_HZ          ( 1000 )                  // 1ms tick
#define configMAX_PRIORITIES         ( 32 )
#define configMINIMAL_STACK_SIZE     ( 128 )
#define configTOTAL_HEAP_SIZE        ( 20 * 1024 )

// 中断配置
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 )
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  ( 15 )

// 功能裁剪
#define configUSE_MUTEXES            1
#define configUSE_RECURSIVE_MUTEXES  1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_TIMERS             1
#define configTIMER_TASK_STACK_DEPTH ( configMINIMAL_STACK_SIZE * 2 )
#define configTIMER_TASK_PRIORITY    ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH     10

// 栈溢出检测
#define configCHECK_FOR_STACK_OVERFLOW    2

// 空闲任务
#define configIDLE_SHOULD_YIELD     1

// 其他
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configSUPPORT_STATIC_ALLOCATION     0
#define configUSE_16_BIT_TICKS              0

#endif
```

### STM32 HAL 库中断处理

```c
// stm32f4xx_it.c

// SysTick 中断由 FreeRTOS 处理，用户不需修改
// void SysTick_Handler(void) { if (xTaskIncrementTick()...) }

// 其他中断需要添加 FromISR API 调用

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);

    // FreeRTOS 中断安全处理
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (uartRxAvailable) {
        uint8_t data = uartRxBuffer[uartRxHead++];
        xQueueSendFromISR(xUartQueue, &data, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

---

## 8.5 常见移植问题

### 1. HardFault

**可能原因：**
- 栈指针配置错误
- 任务函数指针为空
- 内存越界

**排查方法：**
```gdb
(gdb) info registers
(gdb) backtrace
```

### 2. 定时器不工作

**检查项：**
- SysTick 中断是否使能
- configCPU_CLOCK_HZ 是否正确
- 中断优先级是否正确

### 3. 任务不切换

**检查项：**
- 高优先级任务是否在阻塞
- configUSE_PREEMPTION 是否开启
- 调度器是否启动 `vTaskStartScheduler()`

---

## 8.6 面试高频问题

### Q1：FreeRTOS 移植需要改哪些文件？

**参考答案：**
- `FreeRTOSConfig.h`：配置文件
- `port.c`/`portmacro.h`：移植层实现
- 中断处理文件：如 `stm32f4xx_it.c`
- 堆策略文件：`heap_4.c` 等

---

### Q2：PendSV 和 SysTick 的作用？

**参考答案：**
- **SysTick**：系统节拍定时器，产生定时中断，用于任务延时和时间片
- **PendSV**：软中断，用于触发上下文切换（任务切换）

---

### Q3：FreeRTOS 如何实现任务切换？

**参考答案要点：**
1. 高优先级任务就绪或时间片耗尽
2. 触发 PendSV 中断
3. PendSV handler 调用 `vTaskSwitchContext()`
4. 保存当前任务上下文到栈
5. 更新 `pxCurrentTCB` 指向新任务
6. 恢复新任务上下文

---

### Q4：移植时常见问题有哪些？

**参考答案：**
- HardFault：栈配置错误、内存越界
- 定时器不工作：时钟频率配置错误
- 任务不切换：优先级配置、调度器未启动
- 中断不响应：中断优先级超出范围

---

## 8.7 避坑指南

1. **configCPU_CLOCK_HZ 必须与实际一致** — 错误导致延时不准
2. **中断优先级不要超出 0-15** — Cortex-M 只支持这个范围
3. **PendSV 优先级必须设为最低** — 确保切换时不被其他中断打断
4. **切换到新芯片先验证 Tick 中断** — 确认节拍时钟工作正常
5. **堆内存要足够大** — 任务栈和队列都需要内存

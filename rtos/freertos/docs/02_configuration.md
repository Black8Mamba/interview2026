# 第二章：FreeRTOS 配置项详解

> 本章目标：掌握 FreeRTOSConfig.h 核心配置项，理解配置对系统行为的影响

## 章节结构

- [ ] 2.1 配置文件概述
- [ ] 2.2 内核配置
- [ ] 2.3 内存配置
- [ ] 2.4 中断配置
- [ ] 2.5 调度器配置
- [ ] 2.6 功能裁剪配置
- [ ] 2.7 面试高频问题
- [ ] 2.8 避坑指南

---

## 2.1 配置文件概述

### FreeRTOSConfig.h 位置

```c
// STM32 工程中一般在
Project/
├── Core/
│   └── Inc/
│       └── FreeRTOSConfig.h    ← 核心配置文件
```

### 配置文件的层次

```
┌─────────────────────────────────┐
│      FreeRTOSConfig.h           │  ← 用户定制（必改）
├─────────────────────────────────┤
│      projdefs.h                │  ← 项目默认配置
├─────────────────────────────────┤
│      FreeRTOS.h                │  ← 主头文件
├─────────────────────────────────┤
│      portmacro.h               │  ← 架构相关（移植层）
└─────────────────────────────────┘
```

---

## 2.2 内核配置

### configCPU_CLOCK_HZ

```c
#define configCPU_CLOCK_HZ    ( SystemCoreClock )  // 通常等于 SystemCoreClock
```

**说明：** CPU 时钟频率，用于计算节拍中断间隔。必须与实际硬件一致。

### configTICK_RATE_HZ

```c
#define configTICK_RATE_HZ    ( 1000 )  // 默认 1000Hz = 1ms Tick
```

**说明：** 系统节拍时钟频率。值越大调度越精细，但开销也越大。
- 1000Hz：时间精度 1ms，适合高精度延时场景
- 100Hz：时间精度 10ms，普通嵌入式足够

### configMAX_PRIORITIES

```c
#define configMAX_PRIORITIES    ( 32 )  // 可配置范围 1-1024
```

**说明：** 最大任务优先级。数值越大优先级越高。

### configMINIMAL_STACK_SIZE

```c
#define configMINIMAL_STACK_SIZE    ( 128 )  // 单位：字（STM32=4字节）
```

**说明：** 空闲任务（IDLE Task）使用的栈大小。默认 128 字 = 512 字节。

### configUSE_PREEMPTION

```c
#define configUSE_PREEMPTION    1    // 1=抢占式, 0=协作式
```

**说明：**
- `1`：高优先级任务就绪时立即抢占低优先级任务
- `0`：任务主动让出 CPU，同优先级任务需配合 `taskYIELD()`

---

## 2.3 内存配置

### configTOTAL_HEAP_SIZE

```c
#define configTOTAL_HEAP_SIZE    ( 20 * 1024 )  // 20KB 堆内存
```

**说明：** FreeRTOS 动态内存池大小。任务、队列、信号量都从这里分配。

### configSUPPORT_DYNAMIC_ALLOCATION

```c
#define configSUPPORT_DYNAMIC_ALLOCATION    1    // 允许动态内存
```

### configSUPPORT_STATIC_ALLOCATION

```c
#define configSUPPORT_STATIC_ALLOCATION    0    // 禁止静态内存（可按需开启）
```

---

## 2.4 中断配置

### configMAX_SYSCALL_INTERRUPT_PRIORITY

```c
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 )
```

**说明：** FreeRTOS 允许在中断中调用的 API（FromISR 系列）的最高优先级。

**关键规则：**
```
优先级数值越小 = 优先级越高（ARM Cortex-M）

| 优先级范围          | 说明                        |
|--------------------|-----------------------------|
| 0-4 (含)           | 无法调用任何 FreeRTOS API   |
| 5-15 (含)          | 可调用 FromISR API          |
```

### configLIBRARY_LOWEST_INTERRUPT_PRIORITY / configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

```c
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY        ( 15 )
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  ( 5 )
```

**说明：** 计算中断优先级的辅助宏，用于 `portSET_INTERRUPT_MASK_FROM_ISR()`。

---

## 2.5 调度器配置

### configUSE_TIME_SLICING

```c
#define configUSE_TIME_SLICING    1    // 默认开启
```

**说明：** 同优先级任务是否采用时间片轮转。关闭后同优先级任务不会自动切换。

### configUSE_16_BIT_TICKS

```c
#define configUSE_16_BIT_TICKS    0    // 默认关闭（使用 32 位 Tick）
```

**说明：** TickCount 类型。开启可节省内存，但最大延时受限（65535 ticks）。

### configIDLE_SHOULD_YIELD

```c
#define configIDLE_SHOULD_YIELD    1
```

**说明：** 空闲任务是否在无事可做时让出 CPU 给同优先级的 Ready 任务。

---

## 2.6 功能裁剪配置

### configUSE_COUNTING_SEMAPHORES

```c
#define configUSE_COUNTING_SEMAPHORES    1
```

### configUSE_MUTEXES

```c
#define configUSE_MUTEXES    1
```

### configUSE_RECURSIVE_MUTEXES

```c
#define configUSE_RECURSIVE_Mutexes    1
```

### configUSE_TASK_NOTIFICATIONS

```c
#define configUSE_TASK_NOTIFICATIONS    1    // 任务通知（轻量级通信）
```

### configUSE_TICKLESS_IDLE

```c
#define configUSE_TICKLESS_IDLE    0    // 低功耗模式，默认关闭
```

### configUSE_APPLICATION_TASK_TAG

```c
#define configUSE_APPLICATION_TASK_TAG    0
```

---

## 2.7 面试高频问题

### Q1：FreeRTOS 的 Tick 时钟频率配置多少合适？

**参考答案：**
- 一般 100-1000Hz
- 高精度场景用 1000Hz
- 普通场景用 100Hz 降低 CPU 开销
- 权衡：频率越高，延时精度越高，但上下文切换开销越大

### Q2：configMAX_SYSCALL_INTERRUPT_PRIORITY 如何理解？

**参考答案要点：**
- ARM Cortex-M 优先级数值越小优先级越高
- 高于此优先级的中断不能调用 FreeRTOS API
- 低于或等于此优先级的中断可以调用 FromISR 系列 API
- 通常设置为 5-10，留出足够的高优先级给时间-critical 中断

### Q3：堆内存配置太小会有什么后果？

**参考答案：**
- `pvPortMalloc` 返回 NULL
- `xTaskCreate` 失败，无法创建任务
- `xQueueCreate` 失败，队列创建失败
- 系统运行一段时间后内存碎片化导致分配失败

### Q4：如何计算一个任务的栈大小？

**参考答案：**
- 任务函数局部变量消耗
- 函数调用嵌套深度（返回地址）
- 中断嵌套时保存的寄存器
- 浮点运算需要额外空间
- 建议：普通任务 256-512 字，复杂任务 1024+ 字

---

## 2.8 避坑指南

1. **Tick 频率不要设置过高** — 1000Hz 以上开销显著增加
2. **堆内存配置要留余量** — 考虑内存碎片，预留 20-30% 余量
3. **中断优先级不要超出范围** — ARM Cortex-M 只支持 0-15（4位）
4. **关闭功能要谨慎** — 关闭队列可能导致某些 API 不可用
5. **修改配置后要重新编译** — 头文件修改后必须重新 build

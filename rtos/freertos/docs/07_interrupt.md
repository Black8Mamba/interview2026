# 第七章：中断管理与定时器

> 本章目标：掌握 FreeRTOS 中断配置、定时器节拍、FreeRTOS 软件定时器

## 章节结构

- [ ] 7.1 中断管理概述
- [ ] 7.2 中断优先级配置
- [ ] 7.3 中断安全 API
- [ ] 7.4 定时器节拍
- [ ] 7.5 软件定时器
- [ ] 7.6 面试高频问题
- [ ] 7.7 避坑指南

---

## 7.1 中断管理概述

### 中断概念

```
正常程序执行：
main() → Task1() → Task2() → ...

中断发生时：
Task1() 执行中...
  ↓ 硬件中断触发
硬件自动压栈（PC, xPSR, R0-R3, R12, LR）
  ↓
ISR (中断服务例程)
  ↓
硬件自动弹栈
  ↓
Task1() 继续执行
```

### ARM Cortex-M 中断特征

- **硬件自动压栈/弹栈**：R0-R3, R12, LR, PC, xPSR
- **向量中断**：中断入口地址由硬件决定
- **优先级可嵌套**：高优先级可打断低优先级
- **所有中断共用栈**：使用主栈（MSP）

---

## 7.2 中断优先级配置

### 中断优先级规则

```
ARM Cortex-M 优先级数值越小 = 优先级越高

| 优先级范围 | 说明 |
|-----------|------|
| 0 | 最高（不可配置） |
| 1-4 | 高优先级（FreeRTOS 不允许调用 API） |
| 5-15 | 中等优先级（可调用 FromISR API） |
| 16+ | 不存在（Cortex-M4 支持 0-15，共 16 级） |
```

### FreeRTOS 配置

```c
// FreeRTOSConfig.h
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 )
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY    ( 15 )
```

### 中断屏蔽关系

```
┌─────────────────────────────────────────────────────┐
│                   可屏蔽中断范围                      │
│                                                     │
│  优先级 0-4  ────────────────────────────────── ✗  │
│       │                                                │
│       │  configMAX_SYSCALL_INTERRUPT_PRIORITY = 5    │
│       ▼                                                │
│  优先级 5-15  ──────────────────────────────── ✓ ──► FromISR API 可用
│                                                     │
└─────────────────────────────────────────────────────┘
```

### 优先级设置示例

```c
// STM32 HAL 库设置 USART 中断优先级
HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
// 参数2：抢占优先级 = 5（可调用 FromISR）
// 参数3：子优先级 = 0

// 配置 Timer 中断（SysTick 优先级固定，FreeRTOS 使用）
// 不需要用户配置
```

---

## 7.3 中断安全 API

### FromISR API 列表

| 普通 API | 中断 API | 说明 |
|---------|----------|------|
| xQueueSend | xQueueSendFromISR | 发送队列 |
| xQueueReceive | xQueueReceiveFromISR | 接收队列 |
| xSemaphoreGive | xSemaphoreGiveFromISR | 释放信号量 |
| xSemaphoreTake | — | 中断中不可用 |
| xEventGroupSetBits | xEventGroupSetBitsFromISR | 设置事件位 |
| xTaskNotify | xTaskNotifyFromISR | 发送通知 |
| xTaskNotifyGive | vTaskNotifyGiveFromISR | 发送通知（简化） |

### FromISR 使用示例

```c
// 串口接收中断
void USART1_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t data = huart1.Instance->DR;

        // 发送数据到队列（中断安全）
        xQueueSendFromISR(xUartQueue, &data, &xHigherPriorityTaskWoken);
    }

    // 强制上下文切换（如有必要）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### portYIELD_FROM_ISR 详解

```c
// portYIELD_FROM_ISR 宏定义
#define portYIELD_FROM_ISR(x) \
    if (x) vTaskNotifyGiveFromISR(NULL, NULL); \
    do { __asm volatile ("dsb" ::: "memory"); \
         __asm volatile ("isb"); \
    } while (0)

// 作用：
// 1. 如果 xHigherPriorityTaskWoken == pdTRUE，通知任务切换
// 2. 内存屏障，确保指令顺序
```

### 中断中不能使用的 API

```c
// ❌ 以下 API 不能在中断中调用
vTaskDelay();              // 阻塞调用
vTaskDelete();            // 删除任务
xSemaphoreTake();         // 可能阻塞
xQueueSend();             // 可能阻塞
xMutexTake();             // 可能阻塞

// ✅ 中断中应该：
// - 使用 FromISR 版本的 API
// - 只做简短处理，尽量把复杂逻辑放到任务中
```

---

## 7.4 定时器节拍

### SysTick 中断

FreeRTOS 使用 ARM Cortex-M 的 SysTick 作为系统节拍定时器：

```c
// FreeRTOS 启动时配置
void vPortSetupTimerInterrupt(void) {
    // 设置 SysTick 时钟源和重装载值
    // configCPU_CLOCK_HZ / configTICK_RATE_HZ - 1
    SysTick->LOAD = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1;
    SysTick->CTRL = SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk;
}
```

### Tick 中断服务例程

```c
// SysTick_Handler
void SysTick_Handler(void) {
    if (xTaskIncrementTick() != pdFALSE) {
        // 有更高优先级任务就绪，触发 PendSV
        *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET;
    }
}
```

### 节拍计数器

```c
// 获取当前 Tick 计数值
TickType_t tick = xTaskGetTickCount();

// Tick 转换为毫秒
TickType_t ms = pdMS_TO_TICKS(100);     // 100ms → 100 ticks

// 毫秒转换为 Tick
TickType_t ticks = pdMS_TO_TICKS(500);  // 500ms → 500 ticks (假设 1ms/tick)
```

---

## 7.5 软件定时器

### 软件定时器概述

- **后台定时器任务** — 由 FreeRTOS 定时器服务任务执行
- **回调函数** — 在定时器任务上下文中执行
- **一次性/周期性** — 支持两种模式

### 配置

```c
// FreeRTOSConfig.h
#define configUSE_TIMERS                1    // 启用软件定时器
#define configTIMER_TASK_STACK_DEPTH    ( configMINIMAL_STACK_SIZE * 2 )
#define configTIMER_TASK_PRIORITY       ( configMAX_PRIORITIES - 1 ) // 建议最高-1
#define configTIMER_QUEUE_LENGTH        10
```

### 创建定时器

```c
#include "timers.h"

// 定时器回调函数
void MyTimerCallback(TimerHandle_t xTimer) {
    // 被定时调用
    static uint32_t count = 0;
    count++;
    // 清除看门狗等操作
}

// 创建一次性定时器
TimerHandle_t xOneShotTimer = xTimerCreate(
    "OneShot",           // 名称（仅调试用）
    pdMS_TO_TICKS(1000), // 延时 1000ms
    pdFALSE,              // pdFALSE=一次性, pdTRUE=周期
    (void *)0,           // 参数
    MyTimerCallback      // 回调函数
);

// 创建周期定时器
TimerHandle_t xPeriodicTimer = xTimerCreate(
    "Periodic",
    pdMS_TO_TICKS(500),  // 周期 500ms
    pdTRUE,              // 周期模式
    (void *)0,
    MyTimerCallback
);
```

### 启动/停止定时器

```c
// 启动定时器（延迟启动）
xTimerStart(xTimer, portMAX_DELAY);     // 任务中
xTimerStartFromISR(xTimer, &xHigher);   // 中断中

// 停止定时器
xTimerStop(xTimer, portMAX_DELAY);

// 重启定时器（重置）
xTimerReset(xTimer, portMAX_DELAY);

// 修改定时器周期
xTimerChangePeriod(xTimer, pdMS_TO_TICKS(200), portMAX_DELAY);
```

### 定时器服务任务

```
┌─────────────────────────────────────────────┐
│         Timer Service Task (daemon)         │
│                                             │
│  等待定时器命令队列：                         │
│  - 启动定时器                                │
│  - 停止定时器                                │
│  - 重设定时器                                │
│  - 删除定时器                                │
│                                             │
│  执行到期的定时器回调函数                     │
└─────────────────────────────────────────────┘
```

### 定时器使用场景

```c
// 看门狗喂狗定时器
TimerHandle_t xWatchdogTimer;

void vWatchdogCallback(TimerHandle_t xTimer) {
    IWDG->SR = 0;  // 喂狗
}

void Watchdog_Init(void) {
    xWatchdogTimer = xTimerCreate(
        "Watchdog",
        pdMS_TO_TICKS(100),  // 每100ms喂一次
        pdTRUE,
        NULL,
        vWatchdogCallback
    );
    xTimerStart(xWatchdogTimer, 0);
}
```

---

## 7.6 面试高频问题

### Q1：FreeRTOS 中断优先级如何配置？

**参考答案要点：**
- ARM Cortex-M 数值越小优先级越高，范围 0-15
- 高于 `configMAX_SYSCALL_INTERRUPT_PRIORITY`(5) 的中断不能调用任何 FreeRTOS API
- 中断优先级 5-15 可以调用 FromISR 系列 API
- SysTick 和 PendSV 通常设置为最低优先级

---

### Q2：哪些 API 可以在中断中使用？

**参考答案：**
- 所有 `*FromISR` 结尾的 API
- 如 `xQueueSendFromISR`、`xSemaphoreGiveFromISR` 等
- 普通 `xSemaphoreTake`、`vTaskDelay` 等不可用

---

### Q3：软件定时器的精度如何？

**参考答案：**
- 精度取决于定时器服务任务的优先级和负载
- 不是硬件定时器，精度较低
- 适用于看门狗喂狗、超时检测等
- 高精度场景用硬件定时器

---

### Q4：portYIELD_FROM_ISR 的作用？

**参考答案：**
- 标记需要立即进行上下文切换
- 触发 PendSV 中断
- 如果 `xHigherPriorityTaskWoken == pdTRUE`，告诉调度器切换到高优先级任务

---

### Q5：SysTick 的重装载值如何计算？

**参考答案：**
```
reload = (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1

例如：168MHz / 1000Hz - 1 = 167999
即每 1ms 触发一次 SysTick 中断
```

---

## 7.7 避坑指南

1. **中断优先级不要设置太低** — 低于 5 的中断无法使用 FromISR API
2. **中断处理要尽量短** — 复杂逻辑放到任务中处理
3. **FromISR 中发送队列后检查 `xHigherPriorityTaskWoken`** — 必要时调用 `portYIELD_FROM_ISR`
4. **软件定时器回调中不要调用阻塞 API** — 定时器任务阻塞会影响其他定时器
5. **定时器服务任务优先级建议设为最高-1** — 确保定时器及时执行
6. **定时器创建后必须 Start** — 创建不等于启动

---

## 7.8 ARM Cortex-M BASEPRI 寄存器深入分析

### 7.8.1 BASEPRI 寄存器详解

```
┌─────────────────────────────────────────────────────────────┐
│                    Base Priority Mask Register             │
├─────────────────────────────────────────────────────────────┤
│  Bit [7:4] - 优先级掩码（可配置位）                           │
│  Bits [3:0] - 保留                                           │
└─────────────────────────────────────────────────────────────┘
```

BASEPRI 是 ARM Cortex-M 架构中用于控制中断屏蔽的关键寄存器。理解它的工作原理对于掌握 FreeRTOS 的中断管理至关重要。

### 7.8.2 BASEPRI 核心规则

```
核心规则：
┌─────────────────────────────────────────────────────────────┐
│  BASEPRI = 0    →  不屏蔽任何中断，所有中断启用               │
│  BASEPRI = N    →  屏蔽所有优先级 ≤ N 的中断（数值越小优先级越高）│                                                             │
└─────────────────────────────────────────────────────────────┘

重要概念：
- 数值越小 = 优先级越高
- BASEPRI 屏蔽的是"优先级数值 ≥ BASEPRI 值"的所有中断
- 即 BASEPRI = 5 会屏蔽优先级 0, 1, 2, 3, 4, 5 的中断
```

### 7.8.3 BASEPRI 优先级掩码示例

以 3 位优先级（0-7）为例：

```
┌─────────────────────────────────────────────────────────────────────┐
│                        BASEPRI 掩码效果示意                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  BASEPRI = 0 (无掩码)                                                │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ 优先级:   0    1    2    3    4    5    6    7              │    │
│  │ 状态:    ✓    ✓    ✓    ✓    ✓    ✓    ✓    ✓  (全部启用)   │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  BASEPRI = 2 (屏蔽 0,1,2)                                           │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ 优先级:   0    1    2    3    4    5    6    7              │    │
│  │ 状态:    ✗    ✗    ✗    ✓    ✓    ✓    ✓    ✓              │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  BASEPRI = 5 (屏蔽 0,1,2,3,4,5)                                     │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ 优先级:   0    1    2    3    4    5    6    7              │    │
│  │ 状态:    ✗    ✗    ✗    ✗    ✗    ✗    ✓    ✓              │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ✓ = 可触发    ✗ = 被屏蔽                                           │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.8.4 FreeRTOS 中的 BASEPRI 配置

```c
// FreeRTOSConfig.h 中的关键配置
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( 5 )
```

这个配置定义了可以调用 FreeRTOS FromISR API 的最高优先级。

```
FreeRTOS BASEPRI 分配策略：

┌─────────────────────────────────────────────────────────────────────┐
│                           优先级 0                                  │
│                      (不可屏蔽，最高优先级)                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│                      优先级 1-4                                      │
│           (被 BASEPRI = 5 屏蔽，不能调用 FromISR)                     │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│                      优先级 5-15                                     │
│          (BASEPRI = 5 以上的部分，可调用 FromISR)                      │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘

当 FreeRTOS 进入临界区时：
- BASEPRI 被设置为 configMAX_SYSCALL_INTERRUPT_PRIORITY (5)
- 优先级 0-5 的中断被屏蔽
- 优先级 6-15 的中断仍然可以触发
```

---

## 7.9 FreeRTOS 临界区与 BASEPRI 实战

### 7.9.1 临界区概念

临界区是 FreeRTOS 中用于保护共享资源的机制。当任务进入临界区时，系统会屏蔽所有可能打断该任务的中断（由 BASEPRI 控制）。

```
临界区工作流程：

    任务 A 执行
         │
         ▼
    portENTER_CRITICAL()
         │
         ▼
    设置 BASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY
         │
         ▼
    ┌─────────────────────────────────────┐
    │     临界区 - 保护共享资源            │
    │     优先级 ≤ 5 的中断被屏蔽          │
    └─────────────────────────────────────┘
         │
         ▼
    portEXIT_CRITICAL()
         │
         ▼
    恢复 BASEPRI = 0 (所有中断启用)
         │
         ▼
    任务 A 继续执行
```

### 7.9.2 portENTER_CRITICAL 实现详解

```c
// 临界区嵌套计数器
volatile UBaseType_t uxCriticalNesting = 0;

/
// portENTER_CRITICAL 实现
//
void vPortEnterCritical(void)
{
    // 第一步：禁用中断
    portDISABLE_INTERRUPTS();

    // 第二步：增加嵌套计数器
    uxCriticalNesting++;

    // 第三步：如果是第一次进入（嵌套层数为1）
    if (uxCriticalNesting == 1) {
        // 记录当前状态
        // 防止在中断中调用 EnterCritical
    }
}

// portDISABLE_INTERRUPTS - ARM Cortex-M 实现
// 这个宏实际调用 vPortRaiseBASEPRI()
#define portDISABLE_INTERRUPTS()    vPortRaiseBASEPRI()
```

### 7.9.3 vPortRaiseBASEPRI 汇编实现

```c
//
// 提升 BASEPRI 以屏蔽中断
//
static void vPortRaiseBASEPRI(void)
{
    uint32_t ulOriginalBASEPRI;

    __asm volatile (
        "mrs %0, basepri \n"           // 读取当前 BASEPRI 值
        "mov %1, %2 \n"                // 加载阈值（configMAX_SYSCALL_INTERRUPT_PRIORITY）
        "msr basepri, %1 \n"           // 写入 BASEPRI - 屏蔽低优先级中断
        : "=r" (ulOriginalBASEPRI), "=r" (ulCurrentBASEPRI)
        : "i" (configMAX_SYSCALL_INTERRUPT_PRIORITY)
        : "memory"
    );

    return ulOriginalBASEPRI;
}
```

### 7.9.4 portEXIT_CRITICAL 实现详解

```c
// 嵌套计数清零阈值（在 portmacro.h 中定义）
#define portCLEAR_NESTED_COUNT    0

//
// 退出临界区
//
void vPortExitCritical(void)
{
    // 如果当前嵌套层数大于清零阈值
    if (uxCriticalNesting > portCLEAR_NESTED_COUNT) {
        // 减少嵌套计数器
        uxCriticalNesting--;

        // 如果嵌套计数器归零，启用中断
        if (uxCriticalNesting == 0) {
            portENABLE_INTERRUPTS();
        }
    }
}

// portENABLE_INTERRUPTS - 恢复 BASEPRI 为 0
#define portENABLE_INTERRUPTS()    vPortSetBASEPRI(0)

// vPortSetBASEPRI - 设置 BASEPRI 值
static void vPortSetBASEPRI(uint32_t ulBASEPRI)
{
    __asm volatile (
        "msr basepri, %0 \n"
        :: "r" (ulBASEPRI)
        : "memory"
    );
}
```

### 7.9.5 临界区嵌套示例

```c
// 临界区嵌套示意

void Task_A(void) {
    portENTER_CRITICAL();              // uxCriticalNesting = 1
    uxCriticalNesting++;

    // 临界区 1
    xQueueSend(xQueue, &data, 0);

    portENTER_CRITICAL();              // uxCriticalNesting = 2
    // 嵌套临界区

        portENTER_CRITICAL();          // uxCriticalNesting = 3
        // 三层嵌套

        portEXIT_CRITICAL();           // uxCriticalNesting = 2
        // BASEPRI 仍然 = 5（未恢复）

    portEXIT_CRITICAL();               // uxCriticalNesting = 1
    // BASEPRI 仍然 = 5（未恢复）

    portEXIT_CRITICAL();               // uxCriticalNesting = 0
    // BASEPRI 恢复为 0（所有中断启用）
}
```

### 7.9.6 临界区嵌套状态机

```
┌─────────────────────────────────────────────────────────────────────┐
│                      临界区嵌套状态机                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   初始状态: uxCriticalNesting = 0                                    │
│                                                                      │
│   ┌─────────────────┐                                                │
│   │  初始状态       │                                                │
│   │  N=0, BASEPRI=0 │                                                │
│   └────────┬────────┘                                                │
│            │ portENTER_CRITICAL()                                   │
│            ▼                                                         │
│   ┌─────────────────┐                                                │
│   │  第1层临界区     │ N=1, BASEPRI=5                                 │
│   │                 │ (屏蔽优先级 0-5)                                │
│   └────────┬────────┘                                                │
│            │ portENTER_CRITICAL()                                   │
│            ▼                                                         │
│   ┌─────────────────┐                                                │
│   │  第2层临界区     │ N=2, BASEPRI=5 (保持)                          │
│   │                 │ (BASEPRI 不变，只增加计数)                      │
│   └────────┬────────┘                                                │
│            │ portEXIT_CRITICAL()                                     │
│            ▼                                                         │
│   ┌─────────────────┐                                                │
│   │  第1层临界区     │ N=1, BASEPRI=5 (保持)                          │
│   │                 │ (BASEPRI 不变)                                 │
│   └────────┬────────┘                                                │
│            │ portEXIT_CRITICAL()                                     │
│            ▼                                                         │
│   ┌─────────────────┐                                                │
│   │  初始状态       │ N=0, BASEPRI=0                                  │
│   │  (中断恢复)      │ (所有中断启用)                                  │
│   └─────────────────┘                                                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 7.10 FromISR 完整语义分析

### 7.10.1 为什么 FromISR 不能阻塞

理解 FromISR 的设计哲学对于正确使用它至关重要。

```c
//
// 普通 API vs FromISR API 的核心区别
//

// 普通 API 示例：xQueueSend
BaseType_t xQueueSend(QueueHandle_t xQueue,
                      const void *pvItemToQueue,
                      TickType_t xTicksToWait)
{
    // 如果队列满...
    if (uxQueueMessagesWaiting(xQueue) >= xQueue->uxLength) {
        // ...阻塞当前任务！
        vTaskDelay(xTicksToWait);  // ← 这在中断中是不可能的！

        // 或者切换到其他任务
        taskYIELD();  // ← 绝对不能在硬件中断中调用！
    }
    // ...
}

// FromISR API 示例：xQueueSendFromISR
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                             const void *pvItemToQueue,
                             BaseType_t *pxHigherPriorityTaskWoken)
{
    // 如果队列满...
    if (uxQueueMessagesWaiting(xQueue) >= xQueue->uxLength) {
        // ...返回 pdFAIL，绝不阻塞！
        return pdFAIL;  // ← 安全返回
    }
    // ...
}
```

### 7.10.2 FromISR 的三个关键约束

```
FromISR 设计的三大约束：

┌─────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  约束 1: 不能阻塞                                                     │
│  ─────────────────────────────────────────────────────────────────  │
│  • 中断不能等待任何条件                                               │
│  • 中断不能切换任务（除非通过 portYIELD_FROM_ISR）                    │
│  • 中断必须立即返回                                                   │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  约束 2: 不能调用会导致阻塞的普通 API                                 │
│  ─────────────────────────────────────────────────────────────────  │
│  • 禁止: xQueueSend(), vTaskDelay(), xSemaphoreTake()               │
│  • 允许: xQueueSendFromISR(), xSemaphoreGiveFromISR()                │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  约束 3: xHigherPriorityTaskWoken 必须检查                           │
│  ─────────────────────────────────────────────────────────────────  │
│  • 如果解除了更高优先级任务的阻塞，必须请求调度                        │
│  • portYIELD_FROM_ISR(xHigherPriorityTaskWoken)                     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.10.3 xHigherPriorityTaskWoken 参数详解

```c
//
// xHigherPriorityTaskWoken 的语义
//

// 场景：低优先级任务 T1 等待队列数据
//       高优先级任务 T2 也在等待同一个队列

void HighPriority ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;  // 初始化为 FALSE

    // 从ISR发送数据到队列
    xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
    //                    ↑
    //                    这个参数会告诉我们是否有更高优先级任务被解除阻塞

    // 重要：必须检查并处理
    if (xHigherPriorityTaskWoken == pdTRUE) {
        // T2（高优先级任务）刚才被队列数据解除阻塞
        // 但我们还在 ISR 上下文中！

        // 必须请求上下文切换，让调度器切换到 T2
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

### 7.10.4 portYIELD_FROM_ISR 完整实现

```c
//
// portYIELD_FROM_ISR - 触发上下文切换（如有必要）
//

// 常见实现方式1：使用 PendSV
#define portYIELD_FROM_ISR(x)                                         \
    do {                                                              \
        if (x != pdFALSE) {                                           \
            traceRETURN_FROM_ISR_TO_SCHEDULER();                      \
            *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET;                \
        }                                                             \
    } while(0)

// 常见实现方式2：使用 SVC（某些 ARM Cortex-M 变体）
#define portYIELD_FROM_ISR(x)                                         \
    do {                                                              \
        if (x != pdFALSE) {                                           \
            traceRETURN_FROM_ISD_TO_SCHEDULER();                      \
            portNVIC_INT_CTRL = portNVIC_PENDSVSET;                  \
        }                                                             \
    } while(0)

// 寄存器定义（ARM Cortex-M）
#define portNVIC_PENDSVSET    ( 0x10000000UL )  // 触发 PendSV
#define portNVIC_INT_CTRL    ( *(volatile uint32_t *)0xE000ED04UL )
```

### 7.10.5 FromISR 使用完整流程图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        FromISR 使用流程图                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  中断触发                                                             │
│     │                                                                │
│     ▼                                                                │
│  ┌──────────────────┐                                               │
│  │ ISR 开始         │                                               │
│  └────────┬─────────┘                                               │
│           │                                                          │
│           ▼                                                          │
│  ┌─────────────────────────────────────────────┐                   │
│  │ BaseType_t xHigherPriorityTaskWoken = pdFALSE│                   │
│  └─────────────────────────────────────────────┘                   │
│           │                                                          │
│           ▼                                                          │
│  ┌─────────────────────────────────────────────┐                   │
│  │ xQueueSendFromISR / xSemaphoreGiveFromISR ... │                   │
│  │       │                                      │                   │
│  │       ▼                                      │                   │
│  │  队列有空间？                                 │                   │
│  │       │                                      │                   │
│  │   是──┼──→ 数据入队，检查是否有任务被解除阻塞│                   │
│  │       │                                      │                   │
│  │   否──┼──→ 返回 pdFAIL                       │                   │
│  │       │                                      │                   │
│  └───────┼──────────────────────────────────────┘                   │
│          │                                                          │
│          ▼                                                          │
│  ┌─────────────────────────────────────────────┐                   │
│  │ xHigherPriorityTaskWoken == pdTRUE?        │                   │
│  └───────┬─────────────────────────────────────┘                   │
│          │                                                          │
│     是    │    否                                                   │
│    ┌──────┴──────────────────────────────────────┐                  │
│    │                                             │                  │
│    ▼                                             ▼                  │
│  ┌─────────────────┐                   ┌─────────────────┐          │
│  │ portYIELD_FROM_ISR│                   │ 直接返回（无需   │          │
│  │ (xHigherPriority │                   │  上下文切换）   │          │
│  │  TaskWoken)      │                   │                 │          │
│  └────────┬────────┘                   └────────┬────────┘          │
│           │                                        │                   │
│           ▼                                        ▼                   │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │ ISR 返回 → 触发 PendSV → 上下文切换到高优先级任务           │     │
│  └─────────────────────────────────────────────────────────────┘     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.10.6 FromISR 常见错误

```c
// ❌ 错误示例 1：不检查 xHigherPriorityTaskWoken
void BAD_USART_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(xUartQueue, &data, &xHigherPriorityTaskWoken);

    // 忘记调用 portYIELD_FROM_ISR！
    // 可能导致高优先级任务不能及时处理数据
}

// ❌ 错误示例 2：在 FromISR 中调用普通 API
void BAD_IRQHandler(void) {
    // 错误！xQueueSend 可能阻塞！
    xQueueSend(xUartQueue, &data, portMAX_DELAY);  // ← 严重错误！

    // 应该使用：
    // xQueueSendFromISR(xUartQueue, &data, NULL);
}

// ❌ 错误示例 3：在中断中使用会阻塞的信号量
void BAD_IRQHandler(void) {
    // 错误！xSemaphoreTake 可能阻塞！
    xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);  // ← 严重错误！

    // 在中断上下文中，应该用 FromISR 版本
    // 或者用 task notify 机制
}

// ❌ 错误示例 4：使用 pdFALSE 而非 pdTRUE 的地址
void BAD_IRQHandler(void) {
    BaseType_t xHigh;

    // 错误！应该是 pdFALSE 的地址，不是 pdFALSE 本身
    xQueueSendFromISR(xQueue, &data, pdFALSE);  // ← 编译可能错误

    // 正确写法：
    BaseType_t xHigh = pdFALSE;
    xQueueSendFromISR(xQueue, &data, &xHigh);
}
```

---

## 7.11 中断嵌套完整分析

### 7.11.1 ARM Cortex-M 中断嵌套规则

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM Cortex-M 中断嵌套规则                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. 可以嵌套：高优先级中断可以打断低优先级中断                        │
│  2. 不能嵌套：低优先级中断不能打断高优先级中断                        │
│  3. 优先级 0 是最高优先级，不可屏蔽                                   │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                         中断嵌套示例                                  │
│                                                                      │
│  时间轴                                                               │
│  ─────                                                               │
│     │                                                                │
│  ───┼───────────────────────────────► 时间                          │
│     │                                                                │
│     │   ┌────────────────────────────────────────┐                  │
│     │   │  Priority 5 ISR (低优先级)             │                  │
│     │   │  BASEPRI = 5 during this ISR          │                  │
│     │   └────────────────────────────────────────┘                  │
│     │         │                                                      │
│     │         │  优先级 2 的中断触发（更高优先级）                    │
│     │         │                                                      │
│     │         ▼                                                      │
│     │   ┌────────────────────────────────────────┐                  │
│     │   │  Priority 2 ISR (高优先级)              │                  │
│     │   │  打断 Priority 5 ISR 执行              │                  │
│     │   └────────────────────────────────────────┘                  │
│     │         │                                                      │
│     │         │  Priority 2 ISR 结束          │                      │
│     │         ▼                                                      │
│     │   ┌────────────────────────────────────────┐                  │
│     │   │  Priority 5 ISR 继续执行（恢复）        │                  │
│     │   └────────────────────────────────────────┘                  │
│     │                                                                │
│     ▼                                                                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.11.2 BASEPRI 在中断嵌套中的作用

```
┌─────────────────────────────────────────────────────────────────────┐
│                    BASEPRI 在中断嵌套中的作用                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  当 ISR 执行时，BASEPRI 会被设置以防止不必要的嵌套                    │
│                                                                      │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  初始状态：BASEPRI = 0                                          │  │
│  │  主代码正在执行                                                 │  │
│  └───────────────────────────────────────────────────────────────┘  │
│         │                                                          │
│         │  Priority 1 中断触发                                      │
│         ▼                                                          │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  进入 ISR，自动设置 BASEPRI = 1（屏蔽 ≤1 的中断）                │  │
│  │  Priority 1 ISR 执行                                            │  │
│  │  BASEPRI = 1 → 屏蔽 0, 1                                         │  │
│  │  注意：优先级 2-15 仍可触发（除非进一步设置 BASEPRI）             │  │
│  └───────────────────────────────────────────────────────────────┘  │
│         │                                                          │
│         │  在 Priority 1 ISR 期间，Priority 3 中断触发               │
│         │  Priority 3 > BASEPRI(1) → 可以嵌套！                     │
│         ▼                                                          │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Priority 3 ISR 执行（嵌套）                                    │  │
│  │  注意：一些实现会在嵌套时保持 BASEPRI=1                          │  │
│  │  一些实现会更新 BASEPRI=3                                        │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.11.3 FreeRTOS 中的中断嵌套级别

```
┌─────────────────────────────────────────────────────────────────────┐
│                   FreeRTOS 中断嵌套级别                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Level 0: 主代码（任务）执行                                          │
│  ─────────────────────────────────────────────────────────────────  │
│  BASEPRI = 0（无屏蔽）                                               │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Level 1: FreeRTOS 临界区                                             │
│  ─────────────────────────────────────────────────────────────────  │
│  BASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY (5)                  │
│  优先级 0-4 被屏蔽，5-15 可响应                                        │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Level 2: 中断服务例程（ISR）                                         │
│  ─────────────────────────────────────────────────────────────────  │
│  BASEPRI 由硬件设置或 ISR 代码设置                                    │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Level 3: 优先级 0 中断（NMI，不可屏蔽）                              │
│  ─────────────────────────────────────────────────────────────────  │
│  BASEPRI 不影响 NMI                                                  │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.11.4 完整中断嵌套流程图

```
优先级 0 (最高 - BASEPRI 无法屏蔽)
   │
   │
   │   ┌───────────────────────────────────────────────────────────┐
   │   │  NMI 中断（不可屏蔽）                                      │
   │   │  任何时候都可以触发                                        │
   │   └───────────────────────────────────────────────────────────┘
   │
   ▼
优先级 1 (高优先级中断 A)
   │
   │   ┌───────────────────────────────────────────────────────────┐
   │   │  中断 A 执行 (Priority 1)                                  │
   │   │                                                           │
   │   │  BASEPRI 设置为 5（configMAX_SYSCALL_INTERRUPT_PRIORITY）│
   │   │                                                           │
   │   │  效果：屏蔽优先级 0-5 的中断                               │
   │   │       允许优先级 6-15 的中断嵌套                           │
   │   │                                                           │
   │   │  ┌───────────────────────────────────────────────────┐    │
   │   │  │  嵌套：中断 B (Priority 6) 触发并执行              │    │
   │   │  │  Priority 6 > BASEPRI(5) → 允许嵌套               │    │
   │   │  │                                                   │    │
   │   │  │  BASEPRI 可能保持为 5（保持最高级别的屏蔽）       │    │
   │   │  └───────────────────────────────────────────────────┘    │
   │   │                                                           │
   │   │  ┌───────────────────────────────────────────────────┐    │
   │   │  │  嵌套：中断 C (Priority 7) 触发并执行              │    │
   │   │  │  Priority 7 > BASEPRI(5) → 允许嵌套               │    │
   │   │  └───────────────────────────────────────────────────┘    │
   │   │                                                           │
   │   │  中断 B、C 执行完成                                       │
   │   │                                                           │
   │   └───────────────────────────────────────────────────────────┘
   │   中断 A 执行完成
   │
   ▼
优先级 5 (configMAX_SYSCALL_INTERRUPT_PRIORITY)
   │
   │   ┌───────────────────────────────────────────────────────────┐
   │   │  中断 D (Priority 5) - 最低优先级的 FromISR 可用中断      │
   │   │                                                           │
   │   │  注意：这个优先级等于 BASEPRI 值                           │
   │   │  通常会被屏蔽，但这是可以调用 FromISR 的最高优先级         │
   │   └───────────────────────────────────────────────────────────┘
   │
   ▼
优先级 15 (最低优先级)
```

---

## 7.12 SysTick 完整实现分析

### 7.12.1 SysTick 概述

SysTick 是 ARM Cortex-M 架构提供的系统节拍定时器，FreeRTOS 使用它作为系统时钟源。

```
┌─────────────────────────────────────────────────────────────────────┐
│                         SysTick 特性                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  • 24 位递减计数器                                                   │
│  • 可配置时钟源（处理器时钟或外部参考时钟）                           │
│  • 自动重装载值                                                     │
│  • 计数器归零 时触发中断                                             │
│  • FreeRTOS 用作系统节拍                                             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.12.2 vPortSetupTimerInterrupt 完整实现

```c
//
// vPortSetupTimerInterrupt - 配置 SysTick
// 由 xPortStartScheduler() 调用
//
void vPortSetupTimerInterrupt(void)
{
    // 计算重装载值
    // configTICK_RATE_HZ = 1000 表示 1ms 一个 tick
    uint32_t ulReloadValue;

    ulReloadValue = configCPU_CLOCK_HZ / configTICK_RATE_HZ;
    ulReloadValue--;  // -1 因为 SysTick 计数器从 reload 值向下计数到 0

    // 验证重装载值不超出 24 位范围
    configASSERT(ulReloadValue < (0xFFFFFFUL));

    // 设置重装载寄存器
    portNVIC_SYSTICK_LOAD = ulReloadValue;

    // 清除当前计数器值（写入任意值都会清零）
    portNVIC_SYSTICK_CURRENT_VALUE = 0;

    // 配置控制寄存器：
    // - ENABLE: 启用计数器
    // - TICKINT: 计数器归零时触发 SysTick 中断
    // - CLKSOURCE: 使用处理器时钟
    portNVIC_SYSTICK_CTRL = portNVIC_SYSTICK_CLK |
                             portNVIC_SYSTICK_INT |
                             portNVIC_SYSTICK_ENABLE;
}
```

### 7.12.3 SysTick 相关寄存器定义

```c
// ARM Cortex-M SysTick 寄存器定义
#define portNVIC_SYSTICK_CTRL    ( *(volatile uint32_t *)0xE000E010UL )
#define portNVIC_SYSTICK_LOAD    ( *(volatile uint32_t *)0xE000E014UL )
#define portNVIC_SYSTICK_CURRENT_VALUE ( *(volatile uint32_t *)0xE000E018UL )

// 控制寄存器位定义
#define portNVIC_SYSTICK_ENABLE       ( 1UL << 0UL )   // Counter enable
#define portNVIC_SYSTICK_TICKINT      ( 1UL << 1UL )   // SysTick exception request
#define portNVIC_SYSTICK_CLKSOURCE    ( 1UL << 2UL )   // Clock source selection
#define portNVIC_SYSTICK_COUNTFLAG    ( 1UL << 16UL )  // Count flag

// 时钟源选择
#define portNVIC_SYSTICK_CLK          portNVIC_SYSTICK_CLKSOURCE
```

### 7.12.4 SysTick_Handler 完整实现

```c
//
// SysTick_Handler - 系统节拍中断处理
//
void SysTick_Handler(void)
{
    uint32_t ulPendedSVC;

    // 增加系统节拍计数器
    if (xTaskIncrementTick() != pdFALSE) {
        // 需要上下文切换
        // 设置 PendSV 来触发上下文切换
        ulPendedSVC = portNVIC_PENDSVSET;
    } else {
        // 不需要上下文切换
        // 清除 PendSV（以防有残留的挂起位）
        ulPendedSVC = portNVIC_PENDSVSET + 1;  // 加1是为了在汇编中区分
        // 实际实现可能使用：
        // ulPendedSVC = portNVIC_PENDSVCLR;
    }

    // 实际代码可能直接：
    // if (xTaskIncrementTick() != pdFALSE) {
    //     *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET;
    // }

    // 清除 SysTick 中断挂起位（某些实现需要）
    // 或者在 portNVIC_SYSTICK_CURRENT_VALUE 中写入任意值
}
```

### 7.12.5 xTaskIncrementTick 详解

```c
//
// xTaskIncrementTick - 增加节拍计数器并检查是否需要调度
//
BaseType_t xTaskIncrementTick(void)
{
    BaseType_t xSwitchRequired = pdFALSE;
    TickType_t xTicksToWake;

    // 如果使用时间切片，增加节拍计数
    if (xPendedTicks == 0) {
        // 当前没有累积的节拍
        uxSchedulerSuspended = pdFALSE;
    }

    // 增加全局节拍计数器
    xTickCount++;

    // 检查是否溢出
    if (xTickCount == 0) {
        // 溢出处理
    }

    // 检查是否有任务应该唤醒
    if (xTickCount >= xNextTaskWakeTime) {
        xSwitchRequired = pdTRUE;
        // 将任务从延迟列表移到就绪列表
    }

    // 检查时间片是否用完
    #if configUSE_TIME_SLICING == 1
        // 时间片轮转检查
        if (++xNumOfOverflows >= configTICK_RATE_HZ) {
            xNumOfOverflows = 0;
            // 处理时间片
        }
    #endif

    return xSwitchRequired;
}
```

### 7.12.6 SysTick 计算示例

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SysTick 重装载值计算示例                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  示例 1：168MHz CPU，1ms tick                                        │
│  ─────────────────────────────────────────────────────────────────  │
│  configCPU_CLOCK_HZ   = 168000000 Hz                                │
│  configTICK_RATE_HZ   = 1000 Hz                                      │
│                                                                      │
│  reload = (168000000 / 1000) - 1                                    │
│         = 168000 - 1                                                 │
│         = 167999                                                     │
│                                                                      │
│  验证：167999 在 24 位范围内 (0 < 167999 < 16777215) ✓                │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  示例 2：72MHz CPU，10ms tick                                        │
│  ─────────────────────────────────────────────────────────────────  │
│  configCPU_CLOCK_HZ   = 72000000 Hz                                 │
│  configTICK_RATE_HZ   = 100 Hz                                       │
│                                                                      │
│  reload = (72000000 / 100) - 1                                      │
│         = 720000 - 1                                                 │
│         = 719999                                                     │
│                                                                      │
│  验证：719999 在 24 位范围内 ✓                                        │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  示例 3：8MHz CPU，1ms tick（STM32F0 等）                            │
│  ─────────────────────────────────────────────────────────────────  │
│  configCPU_CLOCK_HZ   = 8000000 Hz                                  │
│  configTICK_RATE_HZ   = 1000 Hz                                      │
│                                                                      │
│  reload = (8000000 / 1000) - 1                                      │
│         = 8000 - 1                                                   │
│         = 7999                                                       │
│                                                                      │
│  验证：7999 在 24 位范围内 ✓                                          │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.12.7 SysTick 初始化流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                      SysTick 初始化流程                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  main()                                                             │
│     │                                                               │
│     ▼                                                               │
│  ┌─────────────────┐                                                │
│  │ vTaskStartScheduler() │                                         │
│  └────────┬────────┘                                                │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────────────────────────────────┐                   │
│  │ xPortStartScheduler()                       │                   │
│  │                                             │                   │
│  │ 1. 配置 PendSV 为最低优先级                  │                   │
│  │ 2. 调用 vPortSetupTimerInterrupt()          │                   │
│  │ 3. 初始化临界区嵌套计数器                     │                   │
│  │ 4. 启动第一个任务（不返回）                   │                   │
│  └─────────────────────────────────────────────┘                   │
│           │                                                         │
│           ▼                                                         │
│  ┌─────────────────────────────────────────────┐                   │
│  │ vPortSetupTimerInterrupt()                  │                   │
│  │                                             │                   │
│  │ 1. 计算 reload = configCPU_CLOCK_HZ /       │                   │
│  │                   configTICK_RATE_HZ - 1   │                   │
│  │ 2. 设置 portNVIC_SYSTICK_LOAD = reload      │                   │
│  │ 3. 清除 portNVIC_SYSTICK_CURRENT_VALUE      │                   │
│  │ 4. 设置 portNVIC_SYSTICK_CTRL = ENABLE |     │                   │
│  │                     TICKINT | CLKSOURCE     │                   │
│  └─────────────────────────────────────────────┘                   │
│           │                                                         │
│           ▼                                                         │
│  SysTick 中断开始工作                                                │
│  每 1/ configTICK_RATE_HZ 秒触发一次                                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 7.13 软件定时器任务架构详解

### 7.13.1 定时器服务任务概述

FreeRTOS 的软件定时器由一个专门的 daemon 任务（定时器服务任务）管理。

```
┌─────────────────────────────────────────────────────────────────────┐
│                     定时器服务任务架构                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │               Timer Service Task (Daemon)                   │   │
│   │                   优先级: configTIMER_TASK_PRIORITY         │   │
│   │                   栈大小: configTIMER_TASK_STACK_DEPTH       │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                │                                     │
│                                │ 等待命令                              │
│                                ▼                                     │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                  Timer Command Queue                        │   │
│   │              大小: configTIMER_QUEUE_LENGTH                 │   │
│   │                                                               │   │
│   │   支持的命令:                                                 │   │
│   │   • tmrCOMMAND_START           - 启动定时器                   │   │
│   │   • tmrCOMMAND_STOP            - 停止定时器                   │   │
│   │   • tmrCOMMAND_RESET           - 重置定时器                   │   │
│   │   • tmrCOMMAND_CHANGE_PERIOD   - 修改周期                    │   │
│   │   • tmrCOMMAND_DELETE          - 删除定时器                   │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                │                                     │
│                                │ 到期检查                             │
│                                ▼                                     │
│   ┌─────────────────────────────────────────────────────────────┐   │
│   │                   Active Timer List                         │   │
│   │                                                               │   │
│   │   定时器按到期时间排序，daemon 任务检查最近到期                │   │
│   │   的定时器，如有到期则执行回调函数                             │   │
│   └─────────────────────────────────────────────────────────────┘   │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.13.2 定时器命令处理流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                      定时器命令处理流程                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  应用程序代码                                                        │
│       │                                                             │
│       │ xTimerStart(xTimer, portMAX_DELAY)                          │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                      │
│  │ 发送命令到 Timer Command Queue            │                      │
│  │ 命令: tmrCOMMAND_START                    │                      │
│  └─────────────────────────────────────────┘                      │
│       │                                                             │
│       ▼                                                             │
│  Timer Service Task (Daemon)                                       │
│       │                                                             │
│       │ 从队列接收命令                                               │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                      │
│  │ 处理 tmrCOMMAND_START                    │                      │
│  │ • 将定时器插入 Active Timer List         │                      │
│  │ • 按到期时间排序                          │                      │
│  └─────────────────────────────────────────┘                      │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                      │
│  │ Timer Tick (SysTick) 触发               │                      │
│  │ • xTaskIncrementTick() 检查定时器       │                      │
│  │ • 到期定时器移到 expired list           │                      │
│  └─────────────────────────────────────────┘                      │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────────────────────────────────────┐                      │
│  │ 执行定时器回调函数                        │                      │
│  │ vTimerCallback(xTimer)                   │                      │
│  └─────────────────────────────────────────┘                      │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.13.3 定时器相关配置参数

```c
// FreeRTOSConfig.h 中的定时器配置

// 启用软件定时器功能
#define configUSE_TIMERS                1

// 定时器服务任务优先级
// 建议设置为最高用户任务优先级（configMAX_PRIORITIES - 1）
#define configTIMER_TASK_PRIORITY       ( configMAX_PRIORITIES - 1 )

// 定时器服务任务栈大小
#define configTIMER_TASK_STACK_DEPTH    ( configMINIMAL_STACK_SIZE * 2 )

// 定时器命令队列长度
// 应足够处理所有并发定时器操作
#define configTIMER_QUEUE_LENGTH        10
```

### 7.13.4 定时器回调约束详解

```
┌─────────────────────────────────────────────────────────────────────┐
│                      定时器回调约束                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  警告：定时器回调在 Timer Service Task 上下文中执行！                │
│  ─────────────────────────────────────────────────────────────────  │
│                                                                      │
│  这意味着：                                                          │
│  • 回调不是 ISR                                                    │
│  • 不能使用 FromISR 版本 API（虽然可以，但没必要）                   │
│  • 不在中断上下文，不能使用 portYIELD_FROM_ISR()                    │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                        定时器回调中                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ✅ 可以做：                                                         │
│  ───────                                                            │
│  • 调用任何普通 FreeRTOS API                                         │
│  • 使用信号量/互斥量（带超时）                                        │
│  • 发送数据到队列（带超时）                                           │
│  • 简短的处理逻辑                                                    │
│                                                                      │
│  ❌ 不能做：                                                         │
│  ───────                                                            │
│  • 调用会永久阻塞的 API                                              │
│  • 长时间 delays                                                    │
│  • vTaskDelay() - 会导致死锁！                                       │
│  • xSemaphoreTake(portMAX_DELAY) - 危险！                           │
│  • 无限循环                                                          │
│  • printf() 等阻塞 I/O（视情况）                                     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.13.5 定时器回调正确示例

```c
// ✅ 正确示例：最小化工作，通知其他任务处理
void vTimerCallback_BestPractice(TimerHandle_t xTimer) {
    // 简短处理，只做必要的操作
    static uint32_t count = 0;
    count++;

    // 通知工作任务处理复杂逻辑
    // 使用信号量比队列更高效
    xSemaphoreGiveFromISR(xWorkSemaphore, NULL);
    // 注意：这里用 FromISR 是因为在中断上下文
}

// ✅ 正确示例：定时器中发送数据到队列
void vTimerCallback_SendData(TimerHandle_t xTimer) {
    SensorData_t data;

    // 采集数据
    data.value = ReadSensor();
    data.timestamp = xTaskGetTickCount();

    // 发送到处理队列（带超时确保不阻塞）
    // 注意：在定时器回调中应该用 xQueueSend 而不是 FromISR 版本
    if (xQueueSend(xDataQueue, &data, 0) != pdPASS) {
        // 队列满，处理溢出
        // 不要阻塞！
    }
}

// ❌ 错误示例 1：定时器回调中调用 vTaskDelay
void BAD_TimerCallback_Delay(TimerHandle_t xTimer) {
    // 错误！会导致死锁
    // Timer Service Task 在等待这个定时器到期
    // 但定时器要到期需要 Timer Service Task 运行
    // 死锁！
    vTaskDelay(pdMS_TO_TICKS(100));  // ← 绝对不要！
}

// ❌ 错误示例 2：永久等待信号量
void BAD_TimerCallback_Semaphore(TimerHandle_t xTimer) {
    // 错误！可能导致死锁
    if (xSemaphoreTake(xDataSemaphore, portMAX_DELAY) == pdTRUE) {
        // 处理数据
    }
    // 如果信号量永远不来，这里会永久阻塞
    // 导致所有其他定时器都无法工作
}
```

### 7.13.6 定时器状态转换图

```
┌─────────────────────────────────────────────────────────────────────┐
│                       定时器状态转换图                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│                      ┌─────────────────┐                            │
│                      │    Dormant      │                            │
│                      │    (休眠)        │                            │
│                      └────────┬────────┘                            │
│                               │                                      │
│                    xTimerCreate()                                   │
│                               │                                      │
│                               ▼                                      │
│                      ┌─────────────────┐                            │
│          ┌───────────│    Ready        │───────────┐                │
│          │           │    (就绪)        │           │                │
│          │           └─────────────────┘           │                │
│          │                   │                     │                │
│          │         xTimerStart()                   │                │
│          │                   │                     │                │
│          │                   ▼                     │                │
│          │           ┌─────────────────┐           │                │
│          │           │    Running      │           │                │
│          │           │    (运行)        │           │                │
│          │           └────────┬────────┘           │                │
│          │                    │                    │                │
│          │        Timer Expires                │                │
│          │                    │                    │                │
│          │                    ▼                    │                │
│          │           ┌─────────────────┐           │                │
│          │           │   Active       │◄──────────┘                │
│          │           │   (执行回调)    │  xTimerReset()             │
│          │           └────────┬────────┘                             │
│          │                    │                                     │
│          │        Callback Returns                                   │
│          │                    │                                     │
│          │          ┌─────────┴─────────┐                           │
│          │          │                   │                           │
│          │     pdFALSE (一次性)    pdTRUE (周期)                   │
│          │          │                   │                           │
│          │          ▼                   ▼                           │
│          │   ┌─────────────┐     ┌─────────────┐                    │
│          │   │   Dormant   │     │   Running   │                    │
│          │   │   (休眠)     │     │   (运行)    │                    │
│          │   └─────────────┘     └─────────────┘                    │
│          │                                                           │
│          │ xTimerStop()                                              │
│          └────────────────────────────────────────────────────────► │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 7.14 面试高频问题（扩展答案）

### Q1：FreeRTOS 中断优先级如何配置？（详细版）

**参考答案：**

ARM Cortex-M 架构中，中断优先级配置需要考虑以下因素：

```
1. 优先级范围
   - ARM Cortex-M4: 0-15（共16级）
   - 数值越小 = 优先级越高
   - 优先级 0 是最高优先级，不可屏蔽

2. FreeRTOS 的中断分类

   ┌─────────────────────────────────────────────────────────────────┐
   │ 优先级 0        - 最高优先级，NMI 等，不可屏蔽                  │
   │ 优先级 1-4      - 高优先级，BASEPRI=5 会屏蔽这些               │
   │                   但这些中断不能调用任何 FreeRTOS API           │
   │ 优先级 5        - configMAX_SYSCALL_INTERRUPT_PRIORITY        │
   │                   可以调用 FromISR API                         │
   │ 优先级 6-15     - 低优先级，可以正常调用 FromISR API            │
   └─────────────────────────────────────────────────────────────────┘

3. 配置建议
   - SysTick: 不需要用户配置，FreeRTOS 自动设为最低
   - PendSV: 设为最低优先级（15），确保任务切换不会打断中断
   - 外设中断（UART, SPI等）: 设为 5-15 范围内
```

---

### Q2：哪些 API 可以在中断中使用？（完整版）

**参考答案：**

只有 `*FromISR` 后缀的 API 可以在中断中使用：

```
可用的 FromISR API：
─────────────────────────────────────────────────────────────────────
xQueueSendFromISR()          - 发送数据到队列（不阻塞）
xQueueReceiveFromISR()       - 从队列接收数据（不阻塞）
xSemaphoreGiveFromISR()      - 释放信号量（不阻塞）
xEventGroupSetBitsFromISR()  - 设置事件组位
xTaskNotifyFromISR()         - 发送任务通知
vTaskNotifyGiveFromISR()     - 发送任务通知（简化版）
xTimerStartFromISR()         - 启动定时器
xTimerStopFromISR()          - 停止定时器
xTimerResetFromISR()         - 重置定时器
xTimerChangePeriodFromISR()  - 修改定时器周期

绝对禁止在中断中使用的 API：
─────────────────────────────────────────────────────────────────────
vTaskDelay()                 - 会阻塞当前任务（中断不能阻塞！）
vTaskDelayUntil()            - 同上
xSemaphoreTake()             - 可能阻塞
xMutexTake()                 - 可能阻塞（且互斥量不能在 ISR 中使用）
xQueueSend()                 - 可能阻塞
xQueueReceive()              - 可能阻塞
vTaskDelete()                - 可能导致系统不稳定
vTaskSuspend()               - 可能导致死锁
```

---

### Q3：FromISR 的 xHigherPriorityTaskWoken 参数有什么用？（深入版）

**参考答案：**

这是 FromISR 中最重要的参数之一：

```
1. 问题背景
   ────────
   中断可以打断任何任务。当中断执行时：
   - 如果当前任务是低优先级任务
   - 中断可能解除了一个高优先级任务的阻塞

2. 问题：中断返回时应该切换到哪个任务？
   ────────
   - 刚才被中断打断的低优先级任务？
   - 还是刚被解除阻塞的高优先级任务？

3. xHigherPriorityTaskWoken 的作用
   ────────
   如果 FromISR 调用的结果解除了一个更高优先级任务的阻塞，
   则此参数被设置为 pdTRUE。

4. 必须调用 portYIELD_FROM_ISR()
   ────────
   portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

   这会触发 PendSV 中断，确保调度器在中断返回后
   切换到更高优先级的任务。

5. 如果不调用会怎样？
   ────────
   高优先级任务不会被立即执行
   直到下一个 tick 中断或其他事件触发调度
   这会导致实时响应变差
```

---

### Q4：为什么 FreeRTOS 不允许在中断中阻塞？（设计哲学）

**参考答案：**

```
设计哲学：中断是硬件机制，不应该阻塞

1. 中断的职责
   ────────
   - 响应硬件事件
   - 尽可能快速地处理
   - 标记需要处理的工作
   - 退出，让调度器决定下一步

2. 阻塞在中断中的问题
   ────────
   假设中断中可以阻塞：
   - 中断调用 xQueueSend(queue, data, portMAX_DELAY)
   - 队列满，中断开始等待...
   - 但中断不能切换任务！
   - 系统死锁！

3. FromISR 的设计原则
   ────────
   - 不阻塞，立即返回
   - 返回 pdPASS 表示成功
   - 返回 pdFAIL 表示操作失败（如队列满）
   - 通过 xHigherPriorityTaskWoken 触发调度

4. 正确的做法
   ────────
   void ISR() {
       // 尽可能少的工作
       xQueueSendFromISR(queue, data, &woken);

       // 退出，让调度器处理
       portYIELD_FROM_ISR(woken);
   }
```

---

### Q5：SysTick 的精度由什么决定？（深入分析）

**参考答案：**

```
1. SysTick 精度的影响因素
   ────────
   • configTICK_RATE_HZ - tick 频率
   • configCPU_CLOCK_HZ - CPU 时钟频率
   • 中断延迟

2. Tick 周期计算
   ────────
   Period = 1 / configTICK_RATE_HZ

   例如：
   - configTICK_RATE_HZ = 1000 → 1ms tick
   - configTICK_RATE_HZ = 100  → 10ms tick

3. SysTick 计数器精度
   ────────
   SysTick 是 24 位计数器，最大值 = 16,777,215

   最大 tick 周期 = 16,777,215 / configCPU_CLOCK_HZ

   例如 168MHz CPU：
   最大周期 = 16,777,215 / 168,000,000 ≈ 0.1 秒

4. 软件定时器的精度
   ────────
   软件定时器精度 ≤ tick 周期

   因为定时器只在 tick 中断时检查是否到期

5. 高精度需求的替代方案
   ────────
   如果需要高精度（< 1ms）：
   - 使用硬件定时器（如 STM32 的 TIM1-20）
   - 使用 DWT->CYCCNT 周期计数
```

---

### Q6：FreeRTOS 临界区与中断屏蔽的区别？（面试难点）

**参考答案：**

```
这是面试中经常混淆的概念：

1. 临界区（Critical Section）
   ────────
   portENTER_CRITICAL() / portEXIT_CRITICAL()

   • 是软件概念
   • 使用 BASEPRI 屏蔽中断
   • 可以嵌套
   • 屏蔽哪些中断由 configMAX_SYSCALL_INTERRUPT_PRIORITY 决定

2. 中断屏蔽（Interrupt Mask）
   ────────
   __disable_irq() / __enable_irq()

   • 是硬件指令
   • 屏蔽所有可屏蔽中断（NMI 除外）
   • 不能嵌套（除非手动记录状态）
   • 使用 CPSR/IAR 寄存器

3. 如何选择？
   ────────
   ┌────────────────────────────────────────────────────────┐
   │ 场景                    │ 推荐方法                     │
   ├────────────────────────────────────────────────────────┤
   │ 保护 FreeRTOS 资源       │ 临界区（portENTER_CRITICAL）│
   │ 需要精确控制屏蔽范围     │ 临界区                       │
   │ 绝对需要屏蔽所有中断     │ 中断屏蔽                     │
   │ 在 C 库函数中使用        │ 中断屏蔽                     │
   │ 实时性要求极高           │ 中断屏蔽（最快）             │
   └────────────────────────────────────────────────────────┘

4. 临界区的优势
   ────────
   • 可以嵌套，不会意外死锁
   • 通过 uxCriticalNesting 跟踪
   • 退出时自动恢复之前的状态
```

---

### Q7：定时器回调为什么不能调用 vTaskDelay？（死锁分析）

**参考答案：**

```
vTaskDelay 在定时器回调中是致命的：

1. vTaskDelay 的工作原理
   ────────
   vTaskDelay(n) 会：
   - 将当前任务（Timer Service Task）放入延迟列表
   - 阻塞当前任务
   - 触发调度器选择下一个任务

2. 死锁原因
   ────────
   Timer Service Task 在等待定时器到期
   定时器到期需要 Timer Service Task 执行回调
   但 Timer Service Task 被 vTaskDelay 阻塞了！

   死锁！

3. 图示
   ────────
   Timer Service Task
         │
         │ vTaskDelay(100)
         │
         ▼
   ┌─────────────────┐
   │ BLOCKED (延迟)   │
   └─────────────────┘
         │
         │ 但定时器到期需要我执行！
         │
         ▼
      [死锁]

4. 正确做法
   ────────
   定时器回调中：
   - 做最小化的工作
   - 用信号量/队列通知其他任务
   - 其他任务处理复杂逻辑
```

---

### Q8：portYIELD_FROM_ISR 和 taskYIELD 的区别？（进阶问题）

**参考答案：**

```
这是区分中级和高级工程师的问题：

1. taskYIELD
   ────────
   • 只能在任务上下文中使用
   • 直接触发 PendSV 中断
   • 立即触发上下文切换
   • 切换到就绪的高优先级任务

2. portYIELD_FROM_ISR
   ────────
   • 在中断上下文中使用
   • 通常设置一个标志，由中断返回时处理
   • 某些实现会直接触发 PendSV
   • 关键：确保中断返回时切换到正确的任务

3. 关键区别
   ────────
   ┌──────────────────────────────────────────────────────┐
   │ 特性          │ taskYIELD      │ portYIELD_FROM_ISR  │
   ├──────────────────────────────────────────────────────┤
   │ 使用上下文     │ 任务           │ 中断                 │
   │ 何时切换       │ 立即           │ 中断返回时           │
   │ 典型用法       │ 主动让出 CPU   │ 中断中请求调度       │
   └──────────────────────────────────────────────────────┘

4. 为什么中断返回时切换？
   ────────
   - 中断可能打断了低优先级任务
   - 但更高优先级任务在中断中被唤醒
   - 中断返回时，应该切换到高优先级任务
   - 而不是返回被打断的低优先级任务
```

---

## 7.15 综合实战案例

### 案例 1：完整的串口中断处理

```c
/**
 * 完整的串口接收中断处理方案
 */

#define UART_QUEUE_SIZE    10

// 全局变量
QueueHandle_t xUartRxQueue;
StaticQueue_t xUartRxQueueBuffer;
uint8_t xUartRxQueueStorage[UART_QUEUE_SIZE * sizeof(uint8_t)];

// 中断处理函数
void USART1_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 检查接收数据寄存器非空标志
    if (USART1->SR & USART_SR_RXNE) {
        // 读取数据
        uint8_t data = USART1->DR;

        // 发送到队列（中断安全）
        // 如果队列满，丢弃数据而不是阻塞
        if (xQueueSendFromISR(xUartRxQueue,
                              &data,
                              &xHigherPriorityTaskWoken) == pdFAIL) {
            // 队列满，溢出处理
            // 可以设置标志让任务处理
        }
    }

    // 清除 USART 中断标志（根据具体芯片）
    // HAL 库会自动清除

    // 强制上下文切换（如有必要）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 初始化函数
void UART_Init(void) {
    // 创建队列
    xUartRxQueue = xQueueCreateStatic(
        UART_QUEUE_SIZE,
        sizeof(uint8_t),
        xUartRxQueueStorage,
        &xUartRxQueueBuffer
    );

    // 配置 USART
    // ...

    // 设置中断优先级为 5（可调用 FromISR 的最低优先级）
    NVIC_SetPriority(USART1_IRQn, 5);
    NVIC_EnableIRQ(USART1_IRQn);
}

// 数据处理任务
void vUartTask(void *pvParameters) {
    uint8_t data;

    while (1) {
        // 等待数据
        if (xQueueReceive(xUartRxQueue,
                          &data,
                          portMAX_DELAY) == pdTRUE) {
            // 处理接收到的数据
            ProcessUartData(data);
        }
    }
}
```

### 案例 2：定时器与信号量配合

```c
/**
 * 定时器回调与信号量配合的典型模式
 */

// 定时器句柄
TimerHandle_t xSensorTimer;
SemaphoreHandle_t xSensorSemaphore;

// 定时器回调
void vSensorTimerCallback(TimerHandle_t xTimer) {
    // 只做一件事：给出信号量
    // 复杂处理交给工作任务
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(xSensorSemaphore,
                          &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 传感器读取任务
void vSensorTask(void *pvParameters) {
    while (1) {
        // 等待定时器信号
        if (xSemaphoreTake(xSensorSemaphore,
                           portMAX_DELAY) == pdTRUE) {
            // 读取传感器
            float value = ReadSensor();

            // 处理数据（可能耗时）
            ProcessSensorData(value);

            // 发送数据到显示队列
            SendToDisplay(value);
        }
    }
}

// 主函数
int main(void) {
    // 创建二值信号量
    xSensorSemaphore = xSemaphoreCreateBinary();

    // 创建定时器（每 100ms 触发一次）
    xSensorTimer = xTimerCreate(
        "SensorTimer",
        pdMS_TO_TICKS(100),    // 周期
        pdTRUE,                 // 周期定时器
        NULL,
        vSensorTimerCallback
    );

    // 启动定时器
    xTimerStart(xSensorTimer, 0);

    // 启动调度器
    vTaskStartScheduler();

    // 不应到达这里
    for (;;);
}
```

### 案例 3：多优先级中断嵌套处理

```c
/**
 * 演示中断优先级和嵌套
 */

// 高优先级中断（不能调用 FromISR）
void HighPriority_IRQHandler(void) {
    // 这个中断优先级 < 5
    // 不能调用任何 FreeRTOS FromISR API

    // 只能做纯粹的硬件操作
    uint32_t status = HWREG( peripheral_BASE + STATUS_OFFSET );
    HWREG( peripheral_BASE + DATA_OFFSET ) = status;  // ACK

    // 如果需要和任务通信，使用任务通知（但不从 ISR 等待）
    // 可以用 xTaskNotifyFromISR 发送通知
}

// 中优先级中断（可以调用 FromISR）
void MediumPriority_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 这个中断优先级 >= 5
    // 可以安全调用 FromISR API

    // 从外设读取数据
    uint32_t data = HWREG( peripheral_BASE + DATA_OFFSET );

    // 发送到队列
    xQueueSendFromISR(xDataQueue, &data, &xHigherPriorityTaskWoken);

    // 请求上下文切换（如有必要）
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 低优先级中断（可以调用 FromISR，不会打断高优先级）
void LowPriority_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // 这个中断优先级 >= 6
    // 如果已经有一个 MediumPriority ISR 执行
    // 这个中断可以嵌套执行

    // 设置事件标志
    xEventGroupSetBitsFromISR(xEventGroup,
                              EVENT_FLAG_DATA_READY,
                              &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

---

## 7.16 调试技巧与常见问题排查

### 调试中断问题的常用方法

```
1. 确认中断是否触发
   ────────
   • 在中断处理函数开头加 LED 闪烁
   • 或使用调试器的 Breakpoint

2. 检查中断优先级
   ────────
   • 确认 NVIC 中断优先级寄存器值
   • STM32: NVIC->IP[irq_num] 的高 4 位
   • 优先级数值越小优先级越高

3. 检查 BASEPRI
   ────────
   • 在 FreeRTOS 中断被屏蔽时检查 BASEPRI
   • __asm volatile ("mrs %0, basepri" : "=r"(value))
   • 如果 BASEPRI = 5，则优先级 0-5 被屏蔽

4. 检查 uxCriticalNesting
   ────────
   • 如果进入临界区后忘记退出
   • uxCriticalNesting 会一直增长
   • 所有中断都被永久屏蔽

5. 使用 configASSERT
   ────────
   #define configASSERT(x) if (!(x)) { taskDISABLE_INTERRUPTS(); for(;;); }

   可以捕获很多配置错误
```

### 常见中断配置错误

```
❌ 错误 1：中断优先级设为 0
─────────────────────────────────────────────────────────────────────
NVIC_SetPriority(UART_IRQn, 0);  // 错误！

问题：
- 优先级 0 是最高优先级
- 如果使用了 configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
- 这个中断会被 FreeRTOS 屏蔽！

正确：
NVIC_SetPriority(UART_IRQn, 5);  // 或更高


❌ 错误 2：在临界区内调用 HAL _DELAY
─────────────────────────────────────────────────────────────────────
portENTER_CRITICAL();
HAL_Delay(100);  // 错误！
portEXIT_CRITICAL();

问题：
- HAL_Delay 会关闭全局中断
- 但我们已经进入了临界区
- 可能导致死锁

正确：
- 不要在临界区内调用任何可能阻塞的函数


❌ 错误 3：忘记调用 portYIELD_FROM_ISR
─────────────────────────────────────────────────────────────────────
void ISR(void) {
    xQueueSendFromISR(q, &data, NULL);  // 错误！第二个参数是错的
}

问题：
- 应该是 &xHigherPriorityTaskWoken 的地址
- 不是 NULL

正确：
BaseType_t xHigh = pdFALSE;
xQueueSendFromISR(q, &data, &xHigh);
portYIELD_FROM_ISR(xHigh);


❌ 错误 4：在中断中长时间处理
─────────────────────────────────────────────────────────────────────
void ISR(void) {
    // 错误！中断中做复杂处理
    for (int i = 0; i < 1000; i++) {
        process_data(data[i]);
    }
}

问题：
- 中断响应延迟增加
- 可能丢失其他中断
- 其他任务无法及时运行

正确：
- 中断中只做标记/简单处理
- 复杂处理放到任务中
```

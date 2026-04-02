# 第三章：任务管理

> 本章目标：掌握任务的创建、删除、状态切换，以及任务控制块结构

## 章节结构

- [ ] 3.1 任务基本概念
- [ ] 3.2 任务创建与删除
- [ ] 3.3 任务优先级
- [ ] 3.4 任务状态切换
- [ ] 3.5 任务控制块（TCB）
- [ ] 3.6 任务延时
- [ ] 3.7 面试高频问题
- [ ] 3.8 避坑指南

---

## 3.1 任务基本概念

### 什么是任务

在 FreeRTOS 中，**任务（Task）** 是调度的基本单位。每个任务都是一个无限循环函数：

```c
void vTaskCode(void *pvParameters) {
    // 任务初始化
    for (;;) {
        // 任务主体
        // 做某件事...

        // 适当延时，让出CPU
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 任务 vs 线程 vs 协程

| 特性 | Task | Thread | Coroutine |
|------|------|--------|-----------|
| 独立栈 | ✅ 有 | ✅ 有 | ❌ 无 |
| 独立调度 | ✅ 是 | ✅ 是 | ❌ 协作式 |
| 并发执行 | ✅ 是 | ✅ 是 | ❌ 同一时刻一个 |
| 内存消耗 | 中 | 大 | 小 |

---

## 3.2 任务创建与删除

### 动态创建（常用）

```c
#include "FreeRTOS.h"
#include "task.h"

void MyTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        // 任务逻辑
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 创建任务
TaskHandle_t myTaskHandle = NULL;
BaseType_t result = xTaskCreate(
    MyTask,                   // 任务函数
    "MyTask",                 // 任务名称（用于调试）
    256,                      // 栈深度（字，STM32=256*4=1024字节）
    NULL,                     // 传递给任务的参数
    2,                        // 优先级（数值越大优先级越高）
    &myTaskHandle             // 任务句柄（输出）
);

if (result == pdPASS) {
    // 任务创建成功
} else {
    // 创建失败（内存不足）
}
```

### 静态创建

```c
// 预定义任务栈和任务控制块
StaticTask_t myTaskBuffer;
StackType_t myTaskStack[256];

TaskHandle_t myTaskHandle = xTaskCreateStatic(
    MyTask,
    "MyTask",
    256,
    NULL,
    2,
    myTaskStack,        // 提供栈内存
    &myTaskBuffer       // 提供TCB内存
);
```

### 删除任务

```c
// 通过句柄删除任务
vTaskDelete(myTaskHandle);

// 删除自己
vTaskDelete(NULL);
```

### 代码解读：xTaskCreate 参数

| 参数 | 说明 | 注意事项 |
|------|------|---------|
| pvTaskCode | 任务函数指针 | 必须是无限循环 |
| pcName | 任务名称 | 仅用于调试，最大 16 字符 |
| usStackDepth | 栈深度（字） | STM32: 256字=1KB, 512字=2KB |
| pvParameters | 传给任务的参数 | 可为 NULL |
| uxPriority | 优先级 | 0 最低，最大 configMAX_PRIORITIES-1 |
| pxCreatedTask | 任务句柄输出 | 可为 NULL（不需要句柄时） |

---

## 3.3 任务优先级

### 优先级规则

```c
#define configMAX_PRIORITIES    (32)

// 数值越大优先级越高
uxPriority = 5;  // 比 uxPriority = 3 的任务优先级高
```

### 获取/设置优先级

```c
// 获取任务优先级
UBaseType_t priority = uxTaskPriorityGet(myTaskHandle);

// 获取当前任务优先级
UBaseType_t currentPriority = uxTaskPriorityGet(NULL);

// 设置任务优先级
vTaskPrioritySet(myTaskHandle, 4);

// 设置当前任务优先级
vTaskPrioritySet(NULL, 3);
```

### 优先级继承（互斥量）

当高优先级任务等待低优先级任务持有的互斥量时，会发生优先级继承：

```c
// 任务1：低优先级，持有互斥量
void LowPriorityTask(void *pvParameters) {
    xSemaphoreTake(myMutex, portMAX_DELAY);  // 持有互斥量
    // 做长时间操作...
    xSemaphoreGive(myMutex);
}

// 任务2：高优先级，等待互斥量
void HighPriorityTask(void *pvParameters) {
    // 此时 LowPriorityTask 会被临时提升到相同优先级
    xSemaphoreTake(myMutex, portMAX_DELAY);
    // ...
}
```

---

## 3.4 任务状态切换

### 四种状态

```
        ┌────────────────────────────────────────┐
        │                                        │
        │   ┌──────┐     调度器选中      ┌────────┐
        │   │Ready │ ─────────────────▶ │ Running │
        │   └──────┘                    └────────┘
        │      ▲                              │
        │      │         ┌─────────────┐       │
        │      └──────── │   Blocked   │ ◀─────┘
        │                └─────────────┘   vTaskDelay / 等待事件
        │
        │   ┌────────────┐
        └──▶│ Suspended  │
            └────────────┘
                vTaskSuspend / vTaskResume
```

### 状态说明

| 状态 | 说明 | 何时进入 |
|------|------|---------|
| Running | 正在CPU执行 | 调度器选中 |
| Ready | 就绪，等待CPU | 创建/阻塞结束/恢复 |
| Blocked | 等待事件（延时/信号量/队列） | 调用阻塞API |
| Suspended | 挂起，不参与调度 | vTaskSuspend() |

### 代码示例：状态切换

```c
void TaskExample(void *pvParameters) {
    for (;;) {
        // Running 状态...

        // ===== 进入 Blocked 状态 =====
        // 延时 100ms，任务进入 Blocked，调度器选其他任务
        vTaskDelay(pdMS_TO_TICKS(100));

        // 等待信号量，信号量来之前一直 Blocked
        if (xSemaphoreTake(myBinarySemaphore, portMAX_DELAY) == pdTRUE) {
            // 收到信号，恢复 Ready -> Running
        }

        // ===== 进入 Suspended 状态 =====
        vTaskSuspend(NULL);  // 挂起自己
        // ... 其他任务可以调用 vTaskResume() 恢复

        // ===== 进入 Deleted =====
        vTaskDelete(NULL);  // 删除自己
    }
}
```

---

## 3.5 任务控制块（TCB）

### 3.5.1 TCB 结构体完整源码分析

任务控制块（Task Control Block，TCB）是 FreeRTOS 核心数据结构之一，每个任务都有自己独立的 TCB。TCB 存储了任务的所有状态信息，是任务调度的关键。

```c
// 完整 TCB 定义 - 来自 FreeRTOS Kernel 源码 tasks.c
typedef struct tskTaskControlBlock {
    // ===== 栈管理关键字段 =====
    volatile StackType_t *pxTopOfStack;    // 栈顶指针（最关键字段）

    StackType_t *pxStack;                  // 栈起始地址（栈底附近）

    char pcTaskName[configMAX_TASK_NAME_LEN]; // 任务名称（调试用）

    // ===== 调度链表字段 =====
    ListItem_t xStateListItem;             // 状态链表节点
                                          // 用于将任务插入就绪/阻塞链表

    ListItem_t xEventListItem;            // 事件链表节点
                                          // 用于任务等待事件时插入事件链表

    // ===== 优先级字段 =====
    UBaseType_t uxPriority;               // 任务优先级（0=最低）

    // ===== 任务通知（configUSE_TASK_NOTIFICATIONS == 1）=====
    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        volatile uint32_t ulNotifiedValue;    // 任务通知值
        volatile uint8_t ucNotifyState;       // 通知状态
                                                    // 取值范围：
                                                    // taskNOT_WAITING_NOTIFICATION = 0
                                                    // taskWAITING_NOTIFICATION      = 1
                                                    // taskNOTIFICATION_RECEIVED      = 2
    #endif

    // ===== 互斥量相关（configUSE_MUTEXES == 1）=====
    #if ( configUSE_MUTEXES == 1 )
        UBaseType_t uxMutexesHeld;         // 任务持有的互斥量数量
                                           // 用于实现优先级继承
    #endif

    // ===== 应用任务标签（configUSE_APPLICATION_TASK_TAG == 1）=====
    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
        TaskHookFunction_t pxTaskTag;     // 任务钩子函数指针
    #endif

    // ===== 任务运行时统计（configGENERATE_RUN_TIME_STATS == 1）=====
    #if ( configGENERATE_RUN_TIME_STATS == 1 )
        uint32_t ulRunTimeCounter;        // 任务运行时间计数器
    #endif

    // ===== 数字跟踪/调试相关 =====
    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxTCBNumber;          // TCB 编号（用于调试）
        UBaseType_t uxTaskNumber;        // 任务编号
    #endif

    // ===== 浮点上下文保护（configUSE_FPU == 1）=====
    #if ( configFPU_USED == 1 )
        uint32_t ulFPU registers[16];    // 浮点寄存器保存区
    #endif

    // ===== 核心兼容性字段 =====
    #if ( configUSE_CORE_AFFINITY == 1 && configNUMBER_OF_CORES > 1 )
        UBaseType_t uxCoreAffinityMask; // CPU 核心亲和性掩码
    #endif

    // ===== 栈溢出检测辅助字段 =====
    StackType_t xMyStackStart;           // 栈起始位置（用于溢出检测）
                                          // 与 pxStack 不同，pxStack 是分配的起始地址
                                          // xMyStackStart 是栈实际开始的位置

} tskTCB;
```

**TCB 内存布局图：**

```
内存高地址
┌────────────────────────────────────────┐
│         pxTopOfStack 指向这里           │ ◀── 栈生长方向（向下）
│  ┌──────────────────────────────────┐   │
│  │     任务栈（自顶向下生长）       │   │
│  │                                  │   │
│  │   (StackType_t pxStack)         │   │
│  └──────────────────────────────────┘   │
│                                        │
│  ┌──────────────────────────────────┐   │
│  │        TCB 结构体本体             │   │
│  │  - pxTopOfStack                  │   │
│  │  - pxStack                       │   │
│  │  - xStateListItem                │   │
│  │  - xEventListItem                │   │
│  │  - uxPriority                    │   │
│  │  - pcTaskName[]                  │   │
│  │  - 条件编译字段...               │   │
│  └──────────────────────────────────┘   │
└────────────────────────────────────────┘
内存低地址
```

**关键字段详解：**

| 字段 | 类型 | 作用 | 重要性 |
|------|------|------|--------|
| pxTopOfStack | StackType_t* | 栈顶指针，指向当前栈位置 | ★★★★★ |
| pxStack | StackType_t* | 分配的栈内存起始地址 | ★★★★ |
| xStateListItem | ListItem_t | 用于插入就绪/阻塞链表 | ★★★★★ |
| xEventListItem | ListItem_t | 用于插入事件等待链表 | ★★★ |
| uxPriority | UBaseType_t | 任务优先级 | ★★★★★ |

### 3.5.2 pxPortInitialiseStack 完整源码分析

任务栈初始化是任务创建的核心步骤之一。`pxPortInitialiseStack` 函数负责为新任务准备一个初始化的栈状态，使得任务第一次被调度时能够像从中断返回一样开始执行。

**ARM Cortex-M 架构栈初始化实现：**

```c
// 完整栈初始化源码 - 来自 port.c (ARM Cortex-M)
// 此函数为任务创建一个"假装的"中断栈帧

StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,     // 当前栈顶指针
    TaskFunction_t pxCode,          // 任务入口函数地址
    void *pvParameters              // 传递给任务的参数
) {
    // 模拟中断发生时会保存的寄存器（按入栈顺序）
    // Cortex-M 中断栈帧布局（入栈顺序：xPSR, R15(RPC), R14(LR), R12, R3-R0, R11-R4）

    // ===== Step 1: 设置初始程序状态寄存器 xPSR =====
    // xPSR 包含条件标志和 Thumb 状态位
    // Thumb 状态必须设置（位 24），否则会导致硬件异常
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0x01000000;    // xPSR = 0x01000000
                                                // Bit 24 = 1 表示 Thumb 状态

    // ===== Step 2: 设置程序计数器 PC =====
    // 任务第一次运行时从这里开始执行
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pxCode;        // PC = 任务入口函数地址

    // ===== Step 3: 设置链接寄存器 LR =====
    // LR 存储函数返回地址，任务退出时跳转到此地址
    // prvTaskExitError 是一个错误处理函数，如果任务意外退出会被调用
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)prvTaskExitError; // LR = 退出错误处理

    // ===== Step 4-8: 设置 R12, R3, R2, R1 =====
    // 这些寄存器在中断处理中可能使用，初始化为 0
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R12（通用寄存器）

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R3

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R2

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R1

    // ===== Step 9: 设置 R0 =====
    // R0 通常用于传递第一个参数
    // 这里将 pvParameters（任务参数）传递给任务
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pvParameters;   // R0 = 任务参数

    // ===== Step 10-17: 设置 R11-R4（手动保存的寄存器）=====
    // 在任务切换时，这些寄存器会被保存到栈上
    // 初始化为 0

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R11

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R10

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R9

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R8

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R7

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R6

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R5

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)0;              // R4

    // ===== 返回新的栈顶指针 =====
    // TCB 中的 pxTopOfStack 指向这里
    // 下次任务切换时从这里恢复寄存器
    return pxTopOfStack;
}

// prvTaskExitError - 任务意外退出的错误处理
// 如果任务函数返回，会跳转到此函数
static void prvTaskExitError(void) {
    // 致命错误：任务不应该退出
    // 应当进入死循环，便于调试
    for(;;) {
        // 可以在此设置断点
        portDISABLE_INTERRUPTS();
        while(1) {
            // 死循环
        }
    }
}
```

**Cortex-M 栈帧完整布局图：**

```
高地址 ──────────────────────────────────────────────────────
                    ┌────────────────────────┐
  pxTopOfStack+48   │      xPSR              │  ← 初始值 0x01000000（Thumb状态）
                    │      PC                │  ← 任务入口函数地址
                    │      LR                │  ← prvTaskExitError
                    │      R12               │
                    │      R3                │
                    │      R2                │
                    │      R1                │
                    │      R0                │  ← pvParameters（任务参数）
                    │      R11               │
                    │      R10               │
                    │      R9                │
                    │      R8                │
                    │      R7                │
                    │      R6                │
                    │      R5                │
                    │      R4                │
                    └────────────────────────┘
                                │
                                │ 向下生长
                                ▼
                    ┌────────────────────────┐
                    │                        │
                    │    任务实际使用空间     │
                    │   (局部变量/调用深度)   │
                    │                        │
                    │                        │
                    └────────────────────────┘
                                │
                                │ 向下生长
                                ▼
低地址 ──────────────────────────────────────────────────────
                    pxStack（栈基地址）
```

**栈初始化的关键点：**

1. **为什么需要假中断帧？**
   - Cortex-M 架构使用 PSP（进程栈指针）作为任务栈
   - 第一次调度任务时，硬件期望从栈中恢复寄存器状态
   - 就像从中断返回一样（R14=EXC_RETURN）

2. **Thumb 状态位的重要性：**
   - ARM 处理器有两种状态：ARM 状态和 Thumb 状态
   - Cortex-M 只支持 Thumb 状态（xPSR 的 bit 24 必须为 1）
   - 如果忘记设置Thumb位，会导致硬件Fault

3. **R0 作为参数传递：**
   - ARM 调用约定：R0 传递第一个参数
   - FreeRTOS 将任务参数通过 R0 传递给任务函数

4. **prvTaskExitError 的作用：**
   - 如果任务函数错误地返回了（不是无限循环）
   - CPU 会跳转到 LR 指向的地址
   - 此时会调用 prvTaskExitError，进入死循环

### 3.5.3 任务创建完整内部流程

任务创建 `xTaskCreate()` 内部经历多个步骤，下面详细分析每个步骤：

```
xTaskCreate() 完整执行流程
═══════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────┐
│  Step 1: 参数验证                                       │
│                                                         │
│  BaseType_t xTaskCreate(                                │
│      TaskFunction_t pxTaskCode,    // ① 任务函数不能为NULL│
│      const char *pcName,            // ② 名字最长 configMAX_TASK_NAME_LEN
│      configSTACK_DEPTH_TYPE usStackDepth, // ③ 栈深度 > 0
│      void *pvParameters,            // ④ 参数（可为NULL）
│      UBaseType_t uxPriority,        // ⑤ 优先级 < configMAX_PRIORITIES
│      TaskHandle_t *pxCreatedTask    // ⑥ 句柄输出（可为NULL）
│  )                                                         │
│                                                         │
│  验证失败 → 返回 errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 2: 申请 TCB 内存                                  │
│                                                         │
│  tskTCB *pxNewTCB;                                      │
│  pxNewTCB = (tskTCB *)pvPortMalloc(sizeof(tskTCB));     │
│                                                         │
│  if (pxNewTCB == NULL) {                                │
│      // 内存不足，分配失败                               │
│      return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;     │
│  }                                                       │
│                                                         │
│  memset(pxNewTCB, 0, sizeof(tskTCB)); // 清零 TCB       │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 3: 申请任务栈内存                                  │
│                                                         │
│  StackType_t *pxStack;                                  │
│  pxStack = (StackType_t *)pvPortMalloc(                 │
│      usStackDepth * sizeof(StackType_t)                 │
│  );                                                      │
│                                                         │
│  if (pxStack == NULL) {                                 │
│      vPortFree(pxNewTCB);   // 释放已分配的 TCB         │
│      return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;     │
│  }                                                       │
│                                                         │
│  // 填充栈magic数（用于溢出检测）                       │
│  #if ( configSTACK_FILL_ON_CREATION == 1 )              │
│      memset(pxStack, tskSTACK_FILL_BYTE,                │
│             usStackDepth * sizeof(StackType_t));       │
│  #endif                                                 │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 4: 初始化任务栈                                    │
│                                                         │
│  pxNewTCB->pxStack = pxStack;                           │
│  pxNewTCB->xMyStackStart = (StackType_t)pxStack;         │
│                                                         │
│  pxTopOfStack = pxPortInitialiseStack(                 │
│      pxStack + usStackDepth,  // 栈顶（高地址）         │
│      pxTaskCode,              // 任务入口               │
│      pvParameters             // 任务参数               │
│  );                                                     │
│                                                         │
│  pxNewTCB->pxTopOfStack = pxTopOfStack;  // 保存栈顶   │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │  pxPortInitialiseStack 做了什么：                 │  │
│  │                                                   │  │
│  │  1. 在栈顶压入假的 xPSR (0x01000000)              │  │
│  │  2. 压入 PC = pxTaskCode（任务入口）               │  │
│  │  3. 压入 LR = prvTaskExitError（错误处理）         │  │
│  │  4. 压入 R12, R3, R2, R1 = 0                       │  │
│  │  5. 压入 R0 = pvParameters（任务参数）             │  │
│  │  6. 压入 R11-R4 = 0（手动保存的寄存器）            │  │
│  │  7. 返回新的栈顶指针                              │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 5: 初始化 TCB 其他字段                             │
│                                                         │
│  // 任务名称                                             │
│  strncpy(pxNewTCB->pcTaskName, pcName,                  │
│           configMAX_TASK_NAME_LEN - 1);                 │
│  pxNewTCB->pcTaskName[configMAX_TASK_NAME_LEN - 1] = 0;│
│                                                         │
│  // 优先级                                               │
│  pxNewTCB->uxPriority = uxPriority;                    │
│                                                         │
│  // 链表节点初始化（不插入任何链表）                     │
│  vListInitialiseItem(&(pxNewTCB->xStateListItem));      │
│  vListInitialiseItem(&(pxNewTCB->xEventListItem));     │
│                                                         │
│  // 设置链表所有者                                        │
│  listSET_LIST_ITEM_OWNER(&(pxNewTCB->xStateListItem),  │
│                           pxNewTCB);                    │
│  listSET_LIST_ITEM_OWNER(&(pxNewTCB->xEventListItem),  │
│                           pxNewTCB);                    │
│                                                         │
│  // 初始化通知相关字段（如果启用）                        │
│  #if ( configUSE_TASK_NOTIFICATIONS == 1 )              │
│      pxNewTCB->ulNotifiedValue = 0;                    │
│      pxNewTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION; │
│  #endif                                                 │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 6: 添加到就绪链表                                  │
│                                                         │
│  taskENTER_CRITICAL();  // 进入临界区                   │
│  {                                                     │
│      // 全局任务计数                                     │
│      uxCurrentNumberOfTasks++;                         │
│                                                         │
│      // 如果是第一个任务，创建空闲任务                    │
│      if (uxCurrentNumberOfTasks == 1) {                 │
│          prvInitialiseTaskLists();                     │
│          prvCreateIdleTask();                          │
│      }                                                 │
│                                                         │
│      // 添加到就绪队列                                    │
│      prvAddTaskToReadyQueue(pxNewTCB);                 │
│  }                                                     │
│  taskEXIT_CRITICAL();   // 退出临界区                   │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 7: prvAddTaskToReadyQueue 详细实现                │
│                                                         │
│  static void prvAddTaskToReadyQueue(tskTCB *pxTCB) {   │
│                                                         │
│      // 更新全局最高优先级就绪标记                       │
│      if (pxTCB->uxPriority > uxTopReadyPriority) {     │
│          uxTopReadyPriority = pxTCB->uxPriority;        │
│      }                                                  │
│                                                         │
│      // 将任务的 xStateListItem 插入对应优先级的链表    │
│      // 链表按时间片顺序排列                             │
│      listINSERT_END(&pxReadyTasksLists[pxTCB->uxPriority],│
│                     &(pxTCB->xStateListItem));         │
│  }                                                      │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 8: 触发上下文切换（如果需要）                      │
│                                                         │
│  // 如果调度器正在运行，且新任务优先级高于当前任务       │
│  if (xSchedulerRunning != 0) {                         │
│      if (pxCurrentTCB->uxPriority < pxNewTCB->uxPriority)│
│      {                                                  │
│          // 请求一个 pendSV 中断进行上下文切换          │
│          portYIELD_WITHIN_API();                       │
│          // 实际设置: volatile uint32_t portNVIC_INT_CTRL_REG;  │
│          // portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET;        │
│      }                                                  │
│  }                                                      │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 9: 返回成功                                        │
│                                                         │
│  if (pxCreatedTask != NULL) {                          │
│      *pxCreatedTask = (TaskHandle_t)pxNewTCB;          │
│  }                                                      │
│                                                         │
│  return pdPASS;                                         │
└─────────────────────────────────────────────────────────┘
```

**就绪队列数据结构：**

```
pxReadyTasksLists[] - 按优先级索引的链表数组
═══════════════════════════════════════════════════════════

优先级 7:  ┌─────────────────┐
          │ xStateListItem  │──▶ Task5
          └────────┬────────┘
                   │ next
          ┌────────▼────────┐
          │ xStateListItem  │──▶ NULL（尾）
          └─────────────────┘

优先级 6:  ┌─────────────────┐
          │ xStateListItem  │──▶ Task3
          └─────────────────┘

优先级 5:  (空链表 - 无就绪任务)

优先级 4:  ┌─────────────────┐
          │ xStateListItem  │──▶ Task1
          └────────┬────────┘
                   │ next
          ┌────────▼────────┐
          │ xStateListItem  │──▶ Task2
          └─────────────────┘

...
优先级 0:  (最低优先级)
```

### 3.5.4 任务删除完整内部流程

`vTaskDelete()` 实现任务删除，但内存不会立即释放（采用延迟删除策略）：

```
vTaskDelete() 完整执行流程
═══════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────┐
│  前置条件检查                                            │
│                                                         │
│  void vTaskDelete(TaskHandle_t xTaskToDelete) {        │
│                                                         │
│      TCB_t *pxTCB;                                      │
│                                                         │
│      // xTaskToDelete = NULL 表示删除自己               │
│      if (xTaskToDelete == NULL) {                      │
│          pxTCB = pxCurrentTCB;   // 指向当前任务         │
│      } else {                                          │
│          pxTCB = (TCB_t *)xTaskToDelete;               │
│      }                                                  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 1: 从调度链表移除任务                              │
│                                                         │
│  taskENTER_CRITICAL();                                 │
│  {                                                     │
│      // 移除任务的状态链表节点                           │
│      // 任务可能处于：就绪/阻塞/挂起状态                 │
│      listREMOVE_ITEM(pxTCB->xStateListItem);           │
│                                                         │
│      // 如果任务在等待事件，也移除事件链表               │
│      listREMOVE_ITEM(pxTCB->xEventListItem);           │
│                                                         │
│      // 任务计数减一                                     │
│      uxCurrentNumberOfTasks--;                         │
│  }                                                     │
│  taskEXIT_CRITICAL();                                 │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 2: 添加到待删除链表（延迟删除）                    │
│                                                         │
│  // 为什么不直接释放内存？                               │
│  // 原因1: 可能存在中断正在访问该任务的 TCB              │
│  // 原因2: 避免在临界区中执行内存释放（可能导致阻塞）    │
│  // 原因3: 简化同步问题                                  │
│                                                         │
│  taskENTER_CRITICAL();                                 │
│  {                                                     │
│      // 将任务添加到 xTasksWaitingDeletion 链表         │
│      listINSERT_END(&xTasksWaitingDeletion,           │
│                      &pxTCB->xStateListItem);          │
│  }                                                     │
│  taskEXIT_CRITICAL();                                 │
│                                                         │
│  // 设置删除请求标记                                     │
│  pxTCB->ucStaticallyAllocated = pdFALSE;               │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 3: 如果是静态创建任务                              │
│                                                         │
│  #if ( configSUPPORT_STATIC_ALLOCATION == 1 )          │
│      if (pxTCB->ucStaticallyAllocated == pdTRUE) {     │
│          // 静态任务不释放内存，标记需要在其他地方清理   │
│          // 用户需要负责释放 pvTaskGetStackStart()     │
│          // 返回的内存                                   │
│      }                                                  │
│  #endif                                                 │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│  Step 4: 请求上下文切换                                 │
│                                                         │
│  // 如果删除的是当前任务或更高优先级的任务              │
│  if (xSchedulerRunning != 0) {                         │
│      if (pxTCB == pxCurrentTCB) {                      │
│          // 删除的是自己，调度器会选择下一个任务        │
│          portYIELD_WITHIN_API();                      │
│      } else if (pxTCB->uxPriority >= pxCurrentTCB->uxPriority) │
│      {                                                 │
│          // 删除更高优先级任务，也需要切换               │
│          portYIELD_WITHIN_API();                      │
│      }                                                  │
│  }                                                      │
└─────────────────────────────────────────────────────────┘
```

**空闲任务负责实际内存释放：**

```
空闲任务中的内存释放流程
═══════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────┐
│  空闲任务主循环                                          │
│                                                         │
│  void prvIdleTask(void *pvParameters) {                 │
│      for (;;) {                                        │
│          // 检查是否有待删除的任务                       │
│          while (listCURRENT_LIST_LENGTH(               │
│                     &xTasksWaitingDeletion) > 0) {     │
│                                                         │
│              // 获取第一个待删除的任务                   │
│              tskTCB *pxTCB;                            │
│              pxTCB = listGET_OWNER_OF_HEAD_ENTRY(      │
│                         &xTasksWaitingDeletion);      │
│                                                         │
│              // 从链表中移除                             │
│              listREMOVE_ITEM(&pxTCB->xStateListItem); │
│                                                         │
│              // 释放内存                                 │
│              vPortFree(pxTCB->pxStack);  // 释放栈     │
│              vPortFree(pxTCB);            // 释放 TCB │
│                                                         │
│              // 递减任务计数                             │
│              --uxTasksDeleted;                        │
│          }                                             │
│                                                         │
│          // 其他空闲任务处理...                          │
│      }                                                  │
│  }                                                      │
└─────────────────────────────────────────────────────────┘
```

**延迟删除的优势：**

| 优势 | 说明 |
|------|------|
| 避免死锁 | 如果在持有互斥量时删除，可能导致互斥量永久阻塞 |
| 中断安全 | 避免在中断上下文中进行复杂的内存释放操作 |
| 缓存友好 | 内存释放在空闲任务中批量处理 |
| 简化调试 | 任务删除是确定性的，不会意外丢失 |

### 3.5.5 栈溢出检测实现详解

FreeRTOS 提供两种栈溢出检测方法，通过 `configCHECK_FOR_STACK_OVERFLOW` 配置：

```c
// configCHECK_FOR_STACK_OVERFLOW 配置选项：
// 0:   不启用栈溢出检测
// 1:   方法1：检查栈指针是否超出边界
// 2:   方法2：栈填充 + 检查栈基附近是否被破坏（更安全）
// >= 3: 方法3：同时检查栈指针和栈内容
```

**方法1：栈指针越界检查**

```c
// 在上下文切换时检查
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // 栈指针越界触发
    // xTask: 出问题的任务句柄
    // pcTaskName: 任务名称字符串
    for(;;);  // 进入死循环，便于调试器捕获
}

// 在 pendSV_Handler 中调用
// 检查 pxCurrentTCB->pxTopOfStack 是否在有效范围内
```

**方法2：栈填充与检查（推荐方式）**

```c
// 在任务创建时填充栈
#if ( configSTACK_FILL_ON_CREATION == 1 )
    // 填充模式：0xA5A5A5A5
    memset(pxStack, tskSTACK_FILL_BYTE,
           usStackDepth * sizeof(StackType_t));
#endif

// 栈布局示意：
//
// 高地址  ┌────────────────────────────┐
//         │    未使用的栈空间           │ ← 仍然是 0xA5A5A5A5
//         ├────────────────────────────┤
//         │                            │
//         │    任务运行时的栈使用区      │ ← 已被修改
//         │                            │
//         ├────────────────────────────┤
//         │    栈基检查区               │ ← 应该是 0xA5A5A5A5
//  pxStack │    (configMINIMAL_STACK_SIZE/4) │   ← 如果被破坏=溢出
// 低地址  └────────────────────────────┘

// 溢出检查钩子函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // 栈破坏检测触发
    // 此函数在溢出检测到时被调用

    // 典型调试策略：
    // 1. 设置断点于此处
    // 2. 检查 xTask 指向的 TCB
    // 3. 检查 pxTaskName
    // 4. 查看 pxCurrentTCB->pxTopOfStack vs xMyStackStart

    // 错误处理
    printf("Stack overflow detected in task: %s\n", pcTaskName);

    for(;;) {
        // 死循环等待 debugger
    }
}
```

**栈溢出检测流程图：**

```
任务创建时                                    上下文切换时
     │                                              │
     ▼                                              ▼
┌─────────────────┐                      ┌─────────────────────┐
│ 分配任务栈内存   │                      │ 保存当前任务上下文   │
└────────┬────────┘                      └──────────┬──────────┘
         │                                        │
         ▼                                        ▼
┌─────────────────┐                      ┌─────────────────────┐
│ 填充 0xA5A5A5A5 │                      │ 检查栈基附近内容     │
│ 到整个栈空间    │                      │ 是否仍为 0xA5A5A5A5 │
└────────┬────────┘                      └──────────┬──────────┘
         │                                        │
         ▼                                        ▼
┌─────────────────┐                      ┌─────────────────────┐
│ 记录栈基地址    │                      │ 栈基被破坏？         │
│ xMyStackStart   │─────────────────────▶│         │           │
└─────────────────┘                      │    是   │   否      │
                                        ▼         ▼           │
                              ┌─────────────────┐             │
                              │ 调用 Overflow   │             │
                              │ Hook 函数       │             │
                              └─────────────────┘             │
                                        │                     │
                                        ▼                     ▼
                              ┌─────────────────┐   ┌─────────────────────┐
                              │ 停止/调试       │   │ 恢复下一个任务上下文 │
                              └─────────────────┘   └─────────────────────┘
```

**常见栈溢出原因：**

| 原因 | 说明 | 解决方案 |
|------|------|----------|
| 局部变量过大 | `void func() { char buf[1024]; }` | 减小局部变量或使用动态分配 |
| 深度递归调用 | 递归层数过多 | 限制递归深度或改为循环 |
| 中断嵌套过深 | 中断处理函数栈帧过大 | 简化中断处理 |
| printf参数过多 | `printf("%d %d %d...")` 消耗大量栈 | 减少 printf 参数 |
| 浮点运算 | FPU 寄存器保存需要大量栈空间 | 启用 FPU 时增大栈 |

**栈大小计算公式：**

```
实际需要栈大小 = 基础栈帧 + 局部变量栈 + 调用深度栈 + 中断栈 + 安全余量

其中：
- 基础栈帧 ≈ 10-15 words（xPSR, PC, LR, R0-R12）
- 局部变量栈 = 所有局部变量总和
- 调用深度栈 = 嵌套深度 × 平均函数栈帧大小
- 中断栈 = 最大中断嵌套深度 × 中断栈帧大小
- 安全余量 = 建议 20-30% 的总大小
```

### 3.5.6 任务调度核心数据结构

**FreeRTOS 调度器使用的主要数据结构：**

```c
// 来自 tasks.c 的全局变量

// ===== 就绪队列 =====
PRIVILEGED_DATA static List_t pxReadyTasksLists[configMAX_PRIORITIES];
// 每个优先级一个链表，存储该优先级所有就绪任务

// ===== 延时链表 =====
PRIVILEGED_DATA static List_t xDelayedTaskList1;    // 延时任务链表1
PRIVILEGED_DATA static List_t xDelayedTaskList2;   // 延时任务链表2
PRIVILEGED_DATA static List_t *pxDelayedTaskList;   // 指向当前使用的延时链表
PRIVILEGED_DATA static List_t *pxOverflowDelayedTaskList;

// ===== 挂起链表 =====
PRIVILEGED_DATA static List_t xSuspendedTaskList;

// ===== 待删除链表 =====
PRIVILEGED_DATA static List_t xTasksWaitingDeletion;

// ===== 当前运行任务 =====
PRIVILEGED_DATA static tskTCB *pxCurrentTCB;

// ===== 任务计数 =====
PRIVILEGED_DATA static UBaseType_t uxCurrentNumberOfTasks;

// ===== 优先级追踪 =====
PRIVILEGED_DATA static UBaseType_t uxTopReadyPriority;
```

**链表结构（List_t）：**

```c
// 链表节点定义
typedef struct xLIST {
    volatile UBaseType_t uxNumberOfItems;   // 链表中节点数量
    ListItem_t *configLIST_VOLATILE pxIndex; // 当前索引（用于遍历）
    MiniListItem_t xListEnd;                 // 链表尾部节点
} List_t;

// 链表项定义
struct xLIST_ITEM {
    configLIST_VOLATILE BaseType_t xItemValue;      // 排序值（延时时间等）
    struct xLIST_ITEM *configLIST_VOLATILE pxNext;   // 下一个节点
    struct xLIST_ITEM *configLIST_VOLATILE pxPrevious; // 前一个节点
    void *configLIST_VOLATILE pvOwner;               // 拥有此节点的任务（TCB）
    void *configLIST_VOLATILE pvContainer;           // 所属链表
};

typedef struct xLIST_ITEM ListItem_t;
```

**任务调度流程图：**

```
┌──────────────────────────────────────────────────────────────────┐
│                         定时器中断 (Tick)                         │
│                                                                  │
│  每次 Tick 中断:                                                 │
│  1. 增加 tick 计数                                               │
│  2. 检查延时期满任务 → 移至就绪队列                              │
│  3. 调用 vTaskIncrementTick()                                   │
│  4. 如果需要触发调度 → 设置 pendSV                              │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                         PendSV 中断处理                          │
│                                                                  │
│  触发时检查:                                                     │
│  1. 保存当前任务上下文（寄存器到栈）                             │
│  2. 更新当前任务的 TCB                                            │
│  3. 选择最高优先级就绪任务                                       │
│  4. 恢复新任务的上下文                                            │
└──────────────────────────────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                      最高优先级任务运行                           │
└──────────────────────────────────────────────────────────────────┘
```

### 3.5.7 任务优先级配置宏详解

```c
// FreeRTOSConfig.h 中的优先级配置

// ===== configMAX_PRIORITIES =====
// 最大优先级数量（不包括空闲任务优先级）
// 默认值: 5
// 建议: 根据实际任务数量设置，不宜过大（每个优先级需要独立链表）
#define configMAX_PRIORITIES    (5)

// ===== configMAX_PRIORITIES - 1 的特殊用途 =====
// 空闲任务的优先级固定为 0
// tskIDLE_PRIORITY = 0

// ===== configUSE_TIME_SLICING =====
// 时间片轮转调度
// 1: 相同优先级的任务轮流执行（每个 Tick 一个时间片）
// 0: 相同优先级的任务不轮转，一直执行直到阻塞
#define configUSE_TIME_SLICING    1

// ===== configUSE_PORT_OPTIMISED_TASK_SELECTION =====
// 使用硬件优化的优先级选择算法（替代链表遍历）
// 某些硬件有计算前导零指令（CLZ），可以快速找到最高优先级
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    1

// ===== configRUN_MULTIPLE_PRIORITIES =====
// 允许多个任务同时处于就绪状态
// 1: 所有最高优先级任务同时运行
// 0: 同一时刻只运行一个任务
#define configRUN_MULTIPLE_PRIORITIES    0

// ===== configIDLE_SHOULD_YIELD =====
// 空闲任务是否应该让出 CPU 给其他任务
// 1: 空闲任务在完成处理后立即让出 CPU
// 0: 空闲任务运行完整时间片
#define configIDLE_SHOULD_YIELD    1
```

**优先级与调度行为的关系：**

| 配置组合 | 行为描述 |
|----------|----------|
| `RUN_MULTIPLE_PRIORITIES=0` | 同一时间只有最高优先级任务运行 |
| `RUN_MULTIPLE_PRIORITIES=1` | 多个不同优先级的任务可同时运行 |
| `TIME_SLICING=1` | 同优先级任务轮转执行 |
| `TIME_SLICING=0` | 同优先级任务不轮转 |

---

## 3.6 任务延时

### 相对延时 vTaskDelay

```c
// 延时 100ms，期间任务进入 Blocked，其他任务可运行
vTaskDelay(pdMS_TO_TICKS(100));
```

**注意：** vTaskDelay 是相对延时，调用后从当前时刻算起延时指定时间，精度受 Tick 中断影响。

### 绝对延时 vTaskDelayUntil

```c
TickType_t xLastWakeTime;
xLastWakeTime = xTaskGetTickCount();  // 初始化

for (;;) {
    // 每隔 100ms 执行一次（周期精确）
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));

    // 执行周期性任务
    PeriodicTask();
}
```

**区别：**
- `vTaskDelay`：下次执行时间 = 当前时间 + 延时
- `vTaskDelayUntil`：下次执行时间 = 上次执行时间 + 周期（更精确）

### portMAX_DELAY

```c
// 永久等待，直到事件发生
xQueueReceive(myQueue, &data, portMAX_DELAY);
```

`portMAX_DELAY` = `0xFFFFFFFF`（32位Tick），即最大延时约 49 天。

---

## 3.7 面试高频问题

### Q1：FreeRTOS 任务有哪些状态？

**参考答案：**
- Running（运行态）：正在CPU执行
- Ready（就绪态）：等待被调度器选中
- Blocked（阻塞态）：等待延时或事件
- Suspended（挂起态）：被显式挂起，不参与调度

---

### Q2：动态创建和静态创建任务的区别？

| 对比 | 动态创建 | 静态创建 |
|------|---------|---------|
| 内存来源 | 从 FreeRTOS 堆分配 | 用户预分配 |
| 内存位置 | 堆（可碎片化） | 栈/全局（确定性强） |
| 代码复杂度 | 简单，一行搞定 | 需要预定义 TCB 和栈 |
| 适用场景 | 运行时创建/删除任务 | 确定性要求高的场景 |

---

### Q3：任务栈大小如何计算？

**参考答案要点：**
1. 函数局部变量消耗
2. 函数调用嵌套（保存返回地址）
3. 中断嵌套（保存所有寄存器）
4. 浮点运算（如果启用 FPU）
5. 估算公式：`局部变量 + 嵌套深度 * 4 + 中断上下文(约80字)`
6. 建议普通任务 256-512 字（1-2KB）

---

### Q4：空闲任务的作用是什么？

**参考答案：**
- 当没有其他就绪任务时执行
- 释放已删除任务的内存（heap_1 不释放）
- 在 configIDLE_SHOULD_YIELD=1 时让出 CPU
- 可钩子函数执行周期性任务（低优先级）

```c
// 空闲任务钩子示例
void vApplicationIdleHook(void) {
    // 进入低功耗或处理后台任务
}
```

---

### Q5：任务删除后内存何时释放？

**参考答案：**
- 动态创建的任务，栈和 TCB 在任务删除后立即释放（heap_2/3/4）
- heap_1 不会释放任务栈，只能重启或进入空载状态
- 静态创建的任务需要用户手动释放预分配的内存

---

### Q6：任务控制块TCB包含哪些字段？（深度追问）

**参考答案（详细版）：**

```c
typedef struct tskTaskControlBlock {
    // ===== 栈管理（最关键）=====
    volatile StackType_t *pxTopOfStack;    // 栈顶指针，指向当前栈顶位置
    StackType_t *pxStack;                  // 分配的栈内存起始地址
    StackType_t xMyStackStart;             // 栈实际起始位置（用于溢出检测）

    // ===== 任务标识 =====
    char pcTaskName[configMAX_TASK_NAME_LEN]; // 任务名称（调试用）

    // ===== 调度链表节点 =====
    ListItem_t xStateListItem;             // 状态链表节点（就绪/阻塞链表）
    ListItem_t xEventListItem;             // 事件链表节点（等待事件）

    // ===== 优先级 =====
    UBaseType_t uxPriority;               // 当前任务优先级

    // ===== 任务通知（配置启用）=====
    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        volatile uint32_t ulNotifiedValue;    // 通知值
        volatile uint8_t ucNotifyState;       // 通知状态
    #endif

    // ===== 互斥量相关（配置启用）=====
    #if ( configUSE_MUTEXES == 1 )
        UBase_type uxMutexesHeld;         // 持有的互斥量计数
    #endif

    // ===== 其他可选功能 =====
    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
        TaskHookFunction_t pxTaskTag;    // 任务标签钩子
    #endif

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
        uint32_t ulRunTimeCounter;        // 运行时间统计
    #endif
} tskTCB;
```

**面试追问：为什么 pxTopOfStack 是 volatile？**
- 因为中断和任务切换都会修改它
- 编译器不能对它进行优化，必须每次从内存读取

**面试追问：xStateListItem 和 xEventListItem 的区别？**
- `xStateListItem`：用于将任务插入调度链表（就绪/阻塞/挂起）
- `xEventListItem`：用于将任务插入特定事件的等待链表（信号量、队列等）

---

### Q7：任务栈是如何初始化的？（深度追问）

**参考答案（详细版）：**

ARM Cortex-M 架构的任务栈初始化创建了一个"假中断帧"：

```c
StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,
    TaskFunction_t pxCode,
    void *pvParameters
) {
    // 模拟中断栈帧（从高到低地址）

    pxTopOfStack--;
    *pxTopOfStack = 0x01000000;    // xPSR: Thumb状态位必须设置

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pxCode;  // PC: 任务入口

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)prvTaskExitError;  // LR

    // R12-R3 初始化为0
    pxTopOfStack--;
    *pxTopOfStack = 0;    // R12

    pxTopOfStack--;
    *pxTopOfStack = 0;    // R3

    pxTopOfStack--;
    *pxTopOfStack = 0;    // R2

    pxTopOfStack--;
    *pxTopOfStack = 0;    // R1

    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)pvParameters;  // R0: 参数

    // R11-R4 初始化为0
    pxTopOfStack--;
    *pxTopOfStack = 0;    // R11
    // ... R10-R4

    return pxTopOfStack;
}
```

**栈帧布局：**
```
高地址 ──────────────────────────────────────
        │  xPSR (0x01000000)  │ ← Thumb位必须
        │  PC (任务入口)       │ ← 第一次运行跳转
        │  LR (退出函数)       │
        │  R12                │
        │  R3                 │
        │  R2                 │
        │  R1                 │
        │  R0 (参数)          │
        │  R11                │
        │  ...                │
        │  R4                 │
低地址  └────────────────────┘ ← pxTopOfStack返回
```

**面试追问：为什么需要假中断帧？**
- 第一次调度任务时，硬件从栈恢复寄存器状态
- 就像从中断返回一样（PendSV_Handler 返回时）
- 没有这个假帧，任务第一次运行时会读取无效数据

---

### Q8：FreeRTOS如何检测栈溢出？（深度追问）

**参考答案（详细版）：**

FreeRTOS 有两种栈溢出检测方法：

**方法1：栈指针检查（configCHECK_FOR_STACK_OVERFLOW = 1）**
- 在上下文切换时检查 SP 是否超出边界
- 只能检测已经发生的越界

**方法2：栈填充检查（configCHECK_FOR_STACK_OVERFLOW >= 2）**
- 任务创建时用 0xA5A5A5A5 填充整个栈
- 任务切换时检查栈基附近是否仍为 0xA5A5A5A5
- 如果被修改，说明发生过溢出

```c
// 栈溢出钩子函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // 在此处设置断点调试
    // 可检查 xTask 的 pxTopOfStack 和 xMyStackStart
    for(;;);
}
```

**面试追问：为什么不直接检查 pxTopOfStack？**
- pxTopOfStack 指向当前栈顶，不一定超出边界
- 栈溢出是往低地址生长时超出 xMyStackStart
- 需要检查栈基附近的"安全区"是否被破坏

---

### Q9：任务删除后内存何时释放？（深度追问）

**参考答案（详细版）：**

**延迟删除机制：**

任务删除不会立即释放内存，而是采用延迟删除策略：

```
vTaskDelete() 执行：
1. 将任务从调度链表移除
2. 将任务添加到 xTasksWaitingDeletion 链表
3. 触发上下文切换

空闲任务执行：
1. 检查 xTasksWaitingDeletion 是否有任务
2. 取出待删除任务
3. 调用 vPortFree() 释放 TCB 和栈内存
```

**为什么需要延迟删除？**
- 避免在临界区中执行复杂操作
- 防止中断与任务间竞态条件
- 简化调度器实现

**不同堆实现的差异：**

| 堆类型 | 删除后内存释放 |
|--------|---------------|
| heap_1 | 不释放（只能重启） |
| heap_2 | 延迟释放 |
| heap_3 | 延迟释放 |
| heap_4 | 延迟释放（合并相邻空闲块） |
| heap_5 | 延迟释放 |

**面试追问：heap_1 为什么不能释放？**
- heap_1 使用简单数组实现，没有 free 功能
- 只分配不释放，内存效率低但确定性高
- 适用于资源固定、不需要删除任务的系统

---

### Q10：FreeRTOS任务调度算法是怎样的？

**参考答案（详细版）：**

**调度器数据结构：**

```c
// 就绪队列：数组+链表实现
static List_t pxReadyTasksLists[configMAX_PRIORITIES];

// 当前运行任务
static tskTCB *pxCurrentTCB;
```

**调度算法步骤：**

```
1. 找出最高优先级就绪任务
   - 遍历 pxReadyTasksLists 从高优先级到低优先级
   - 找到第一个非空链表

2. 如果多个同优先级任务
   - 时间片轮转（configUSE_TIME_SLICING = 1）
   - 每个 tick 切换到下一个任务

3. 上下文切换到选中任务
   - 保存当前任务上下文到栈
   - 更新 pxCurrentTCB
   - 恢复新任务上下文
```

**优化选择算法（configUSE_PORT_OPTIMISED_TASK_SELECTION）：**

某些 ARM 指令可以快速找到最高优先级：
- CLZ（Count Leading Zeros）指令
- 将优先级掩码转换为单一整数的最高位位置

**面试追问：为什么优先级数组要遍历？**
- 如果使用优化算法，可用 CLZ 指令 O(1) 找到最高优先级
- 未优化时需要 O(n) 遍历
- 实际系统中优先级数量有限（通常 5-10 个），遍历开销可接受

---

### Q11：vTaskDelay 和 vTaskDelayUntil 的区别？

**参考答案（详细版）：**

```c
// vTaskDelay - 相对延时
vTaskDelay(100);  // 从现在起延时 100 ticks

// vTaskDelayUntil - 绝对延时
TickType_t xLastWakeTime = xTaskGetTickCount();
vTaskDelayUntil(&xLastWakeTime, 100);  // 相对于上次执行时间
```

**执行时序对比：**

```
vTaskDelay(100):
时间:  0 ----100----200----300----
      │     │      │      │
      ├─────┤      │      │
      执行  延时   执行    延时
      (100ms)     (100ms)
      注意: 每次都是从上次唤醒开始算，不是上次执行结束


vTaskDelayUntil(&last, 100):
时间:  0 ----100----200----300----
      │     │      │      │
      ├─────┤      │      │
      执行  执行   执行    执行
      (固定100ms周期，抖动小)


实际执行时间：
假设每次执行需要 20ms：

vTaskDelay:
  任务在 0-20ms 执行，21ms 开始延时到 100ms
  下一个周期从 100ms 开始，但 100-120ms 执行
  120ms 开始延时到 200ms...
  实际周期 = 100ms + 执行时间 = 120ms（有抖动）

vTaskDelayUntil:
  任务在 0-20ms 执行，期望下次 100ms 开始
  100-120ms 执行，期望下次 200ms 开始
  实际周期 = 精确 100ms（无抖动）
```

**适用场景：**

| 函数 | 适用场景 |
|------|----------|
| vTaskDelay | 非周期性任务，容忍执行时间变化 |
| vTaskDelayUntil | 周期性任务，要求精确间隔 |

---

### Q12：FreeRTOS 如何实现任务同步？

**参考答案（详细版）：**

FreeRTOS 提供多种任务同步机制：

**1. 二值信号量（Binary Semaphore）**

```c
SemaphoreHandle_t xSemaphore;

// 创建
xSemaphore = xSemaphoreCreateBinary();

// ISR 给出信号
xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);

// 任务获取信号
xSemaphoreTake(xSemaphore, portMAX_DELAY);
```

**2. 计数信号量（Counting Semaphore）**

```c
// 创建，计数最大为 5
xSemaphore = xSemaphoreCreateCounting(5, 0);
```

**3. 互斥量（Mutex）**

```c
// 创建互斥量
xMutex = xSemaphoreCreateMutex();

// 获取（优先级继承）
xSemaphoreTake(xMutex, portMAX_DELAY);

// 释放
xSemaphoreGive(xMutex);
```

**4. 队列（Queue）**

```c
// 创建队列
QueueHandle_t xQueue = xQueueCreate(10, sizeof(int));

// 发送
xQueueSend(xQueue, &data, 0);

// 接收
xQueueReceive(xQueue, &data, portMAX_DELAY);
```

**面试追问：互斥量和二值信号量的区别？**

| 特性 | 互斥量 | 二值信号量 |
|------|--------|-----------|
| 持有者 | 必须由持有者释放 | 可被任何任务释放 |
| 优先级继承 | 支持 | 不支持 |
| 递归获取 | 支持 | 不支持 |
| 适用场景 | 资源保护 | 同步通知 |

**优先级继承解决的问题：**

```
假设：
- Task H（高优先级）等待互斥量 M
- Task L（低优先级）持有互斥量 M

无优先级继承：
Task L 运行 → Task H 就绪但无法运行（互斥量阻塞）

有优先级继承：
Task L 继承 Task H 的优先级 → Task L 快速执行完 → 释放互斥量 → Task H 运行
```

---

## 3.8 避坑指南

1. **任务函数必须是无限循环** — 不能有出口，否则会导致硬 Fault
2. **不要在任务中做无限循环空转** — 必须有 `vTaskDelay` 或阻塞调用让出 CPU
3. **栈大小宁大勿小** — 栈溢出导致奇怪死机，调试困难
4. **删除自己用 `vTaskDelete(NULL)`** — 不要用 `return`
5. **vTaskDelay 和 vTaskDelayUntil 不要混用** — 周期任务用后者更精确
6. **不要在中断服务函数中调用 `vTaskDelete` 等阻塞 API**

---

## 3.9 进阶主题

### 3.9.1 任务通知（Task Notifications）

FreeRTOS v8.2.0 引入了任务通知，是一种轻量级同步机制。

```c
// 任务通知相关 API
BaseType_t xTaskNotifyGive(TaskHandle_t xTaskToNotify);
vTaskNotifyGiveFromISR(TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken);

uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t xTicksToWait);

BaseType_t xTaskNotify(uint32_t ulValue, eNotifyAction eAction);
BaseType_t xTaskNotifyFromISR(uint32_t ulValue, eNotifyAction eAction,
                              BaseType_t *pxHigherPriorityTaskWoken);
```

**优势：**
- 比信号量/队列更快（无需创建内核对象）
- 内存占用更小
- 适合单向通知场景

**限制：**
- 只能有一个任务等待通知
- 不能跨 ISR 同步多个任务

### 3.9.2 任务钩子（Application Task Tag）

任务钩子允许为任务附加自定义数据：

```c
// FreeRTOSConfig.h 启用
#define configUSE_APPLICATION_TASK_TAG    1

// 定义钩子函数
void vMyTaskHook(void *pvParameter) {
    // 自定义处理
}

// 设置钩子
vTaskSetApplicationTaskTag(myTaskHandle, vMyTaskHook);

// 在任务中调用钩子
if (pxCurrentTCB->pxTaskTag != NULL) {
    pxCurrentTCB->pxTaskTag(NULL);
}
```

### 3.9.3 任务运行时统计

启用配置可以统计任务运行时间：

```c
// FreeRTOSConfig.h
#define configGENERATE_RUN_TIME_STATS    1
#define configUSE_TRACE_FACILITY        1

// 需要提供时间统计源
extern uint32_t getRunTimeCounterValue(void);
vTaskGetRunTimeStats(pcWriteBuffer);  // 获取统计信息
```

### 3.9.4 多个核心（SMP）支持

FreeRTOS 支持多核对称多处理（SMP）：

```c
// FreeRTOSConfig.h
#define configNUMBER_OF_CORES    2

// 任务亲和性设置
BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t pvTaskCode,
    const char *pcName,
    uint32_t usStackDepth,
    void *pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *pvCreatedTask,
    BaseType_t xCoreID  // 指定核心
);
```

**SMP 调度特点：**
- 同一优先级的多个任务可同时在不同核心运行
- 需要考虑共享资源访问的线程安全性

---

## 3.6 任务延时

### 相对延时 vTaskDelay

```c
// 延时 100ms，期间任务进入 Blocked，其他任务可运行
vTaskDelay(pdMS_TO_TICKS(100));
```

**注意：** vTaskDelay 是相对延时，调用后从当前时刻算起延时指定时间，精度受 Tick 中断影响。

### 绝对延时 vTaskDelayUntil

```c
TickType_t xLastWakeTime;
xLastWakeTime = xTaskGetTickCount();  // 初始化

for (;;) {
    // 每隔 100ms 执行一次（周期精确）
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));

    // 执行周期性任务
    PeriodicTask();
}
```

**区别：**
- `vTaskDelay`：下次执行时间 = 当前时间 + 延时
- `vTaskDelayUntil`：下次执行时间 = 上次执行时间 + 周期（更精确）

### portMAX_DELAY

```c
// 永久等待，直到事件发生
xQueueReceive(myQueue, &data, portMAX_DELAY);
```

`portMAX_DELAY` = `0xFFFFFFFF`（32位Tick），即最大延时约 49 天。

---

## 3.7 面试高频问题

### Q1：FreeRTOS 任务有哪些状态？

**参考答案：**
- Running（运行态）：正在CPU执行
- Ready（就绪态）：等待被调度器选中
- Blocked（阻塞态）：等待延时或事件
- Suspended（挂起态）：被显式挂起，不参与调度

---

### Q2：动态创建和静态创建任务的区别？

| 对比 | 动态创建 | 静态创建 |
|------|---------|---------|
| 内存来源 | 从 FreeRTOS 堆分配 | 用户预分配 |
| 内存位置 | 堆（可碎片化） | 栈/全局（确定性强） |
| 代码复杂度 | 简单，一行搞定 | 需要预定义 TCB 和栈 |
| 适用场景 | 运行时创建/删除任务 | 确定性要求高的场景 |

---

### Q3：任务栈大小如何计算？

**参考答案要点：**
1. 函数局部变量消耗
2. 函数调用嵌套（保存返回地址）
3. 中断嵌套（保存所有寄存器）
4. 浮点运算（如果启用 FPU）
5. 估算公式：`局部变量 + 嵌套深度 * 4 + 中断上下文(约80字)`
6. 建议普通任务 256-512 字（1-2KB）

---

### Q4：空闲任务的作用是什么？

**参考答案：**
- 当没有其他就绪任务时执行
- 释放已删除任务的内存（heap_1 不释放）
- 在 configIDLE_SHOULD_YIELD=1 时让出 CPU
- 可钩子函数执行周期性任务（低优先级）

```c
// 空闲任务钩子示例
void vApplicationIdleHook(void) {
    // 进入低功耗或处理后台任务
}
```

---

### Q5：任务删除后内存何时释放？

**参考答案：**
- 动态创建的任务，栈和 TCB 在任务删除后立即释放（heap_2/3/4）
- heap_1 不会释放任务栈，只能重启或进入空载状态
- 静态创建的任务需要用户手动释放预分配的内存

---

## 3.8 避坑指南

1. **任务函数必须是无限循环** — 不能有出口，否则会导致硬 Fault
2. **不要在任务中做无限循环空转** — 必须有 `vTaskDelay` 或阻塞调用让出 CPU
3. **栈大小宁大勿小** — 栈溢出导致奇怪死机，调试困难
4. **删除自己用 `vTaskDelete(NULL)`** — 不要用 `return`
5. **vTaskDelay 和 vTaskDelayUntil 不要混用** — 周期任务用后者更精确
6. **不要在中断服务函数中调用 `vTaskDelete` 等阻塞 API**

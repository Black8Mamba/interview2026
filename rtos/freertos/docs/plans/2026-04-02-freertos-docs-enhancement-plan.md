# FreeRTOS 文档详细化扩展计划

> **目标：** 将现有骨架文档扩展为详细、深入的学习资料
> **重点：** 任务调度 O(1) 实现、源码级分析、代码示例

---

## 扩展策略

### 章节扩展优先级

| 优先级 | 章节 | 扩展重点 |
|--------|------|---------|
| P0 | 任务调度与切换 | **O(1) 调度器、源码分析、链表操作** |
| P0 | 任务管理 | TCB 源码、状态机源码、栈初始化 |
| P1 | 内存管理 | 堆算法源码、分配释放流程图 |
| P1 | 同步机制 | 队列/信号量底层实现 |
| P2 | 配置项 | 每个配置项的影响分析 |
| P2 | 中断管理 | FromISR 完整分析 |
| P2 | 调试方案 | 工具使用详细步骤 |
| P3 | 移植适配 | 精简即可 |
| P3 | 概述 | 补充架构图和组件关系 |

---

## 详细扩展任务

### Task 1: 扩展任务调度章节 — 深入 O(1) 调度器

**目标文件：** `docs/04_scheduling.md`

**扩展内容：**

#### 1.1 调度器数据结构（新增 ~300 行）

```c
// 就绪任务队列 — 优先级数组实现 O(1) 入队出队
typedef struct {
    List_t xReadyTasks[configMAX_PRIORITIES];  // 每个优先级一个链表
    UBaseType_t uxTopReadyPriority;            // 记录最高就绪优先级
    UBaseType_t uxSchedulerState;               // 调度器状态
} ReadyQueue_t;

// 全局就绪队列
static ReadyQueue_t xReadyTasks;

// pxCurrentTCB — 当前运行任务
volatile TCB_t * pxCurrentTCB = NULL;

// 获取最高优先级 O(1) — 通过 uxTopReadyPriority
#define taskSELECT_HIGHEST_PRIORITY_TASK() \
    do { \
        portGET_HIGHEST_PRIORITY(uxTopReadyPriority, uxReadyPriorities); \
        listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, &(xReadyTasks.xReadyTasks[uxTopReadyPriority])); \
    } while(0)
```

#### 1.2 就绪队列操作（新增 ~200 行）

```c
// 入队 O(1) — 新任务就绪时
void prvAddTaskToReadyQueue(TCB_t *pxTCB) {
    if (pxTCB->uxPriority >= uxTopReadyPriority) {
        uxTopReadyPriority = pxTCB->uxPriority;
    }
    listINSERT_END(&xReadyTasks.xReadyTasks[pxTCB->uxPriority], &pxTCB->xStateListItem);
}

// 出队 O(1) — 最高优先级任务
listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, &readyQueue[highestPriority]);
```

#### 1.3 时间片调度（新增 ~150 行）

```c
// Tick 中断中判断是否需要切换
void xTaskIncrementTick(void) {
    // ...
    if (pxCurrentTCB->xTicks > 0) {
        pxCurrentTCB->xTicks--;
    } else {
        // 时间片耗尽，移到链表末尾，选取下一个任务
        listINSERT_END(&xReadyTasks.xReadyTasks[pxCurrentTCB->uxPriority],
                       &pxCurrentTCB->xStateListItem);
        taskSELECT_HIGHEST_PRIORITY_TASK();
    }
}
```

#### 1.4 上下文切换完整流程（新增 ~300 行）

```
1. taskYIELD() 或 PendSV 触发
       ↓
2. 硬件自动压栈 PSP → R0-R3, R12, LR, PC, xPSR
       ↓
3. 进入 PendSV_Handler
       ↓
4. portSAVE_CONTEXT() — 软件压栈 R4-R11
       ↓
5. vTaskSwitchContext() — 选择下一个任务
       ↓
6. 更新 pxCurrentTCB 指向新任务
       ↓
7. portRESTORE_CONTEXT() — 弹栈 R4-R11
       ↓
8. 退出 PendSV，自动弹栈恢复 R0-R3, R12, LR, PC, xPSR
       ↓
9. 继续执行新任务
```

#### 1.5 优先级位图算法（新增 ~200 行）

ARM Cortex-M 使用位图快速找到最高优先级：

```c
// 32 位优先级查找 — 一次 CLZ 指令 O(1)
__asm uint32_t prvGetHighestPriority(void) {
    uxPriority = uxTopReadyPriority;
    CLZ uxPriority, xReadyPriorities
    ADD uxPriority, #1
}
```

### Task 2: 扩展任务管理章节 — TCB 和栈

**目标文件：** `docs/03_task_management.md`

**扩展内容：**

#### 2.1 TCB 完整源码分析（新增 ~400 行）

```c
// tasks.c 中的 TCB 定义
typedef struct tskTaskControlBlock {
    volatile StackType_t *pxTopOfStack;    // 栈顶 — 核心字段

    ListItem_t xStateListItem;              // 状态链表节点

    ListItem_t xEventListItem;              // 事件等待链表节点

    UBaseType_t uxPriority;                 // 当前优先级

    StackType_t *pxStack;                   // 栈起始地址

    char pcTaskName[configMAX_TASK_NAME_LEN]; // 任务名

    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
        volatile uint32_t ulNotifiedValue;   // 通知值
        volatile uint8_t ucNotifyState;       // 通知状态
    #endif

    #if ( configUSE_TICKLESS_IDLE != 0 )
        uint32_t ulNotCalledIdle : 1;        // 未调用空闲标志
    #endif

    StackType_t xMyStackStart;              // 栈起始位置（用于溢出检测）
} tskTCB;
```

#### 2.2 任务创建完整流程（新增 ~300 行）

```
xTaskCreate() 调用流程：

1. 分配 TCB（pvPortMalloc）
       ↓
2. 分配栈（pvPortMalloc）
       ↓
3. 初始化栈（pxPortInitialiseStack）
       ↓   - 设置初始寄存器值
       - 伪造中断返回帧
       ↓
4. 初始化 TCB
       ↓   - 优先级
       - 栈顶指针
       - 链表节点
       ↓
5. 添加到就绪队列（prvAddTaskToReadyQueue）
       ↓
6. 如果调度器运行，检查是否需要抢占
```

#### 2.3 栈初始化源码分析（新增 ~200 行）

```c
// ARM Cortex-M 栈布局（任务创建时）
StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,
    TaskFunction_t pxCode,
    void *pvParameters
) {
    pxTopOfStack--;

    // 中断返回帧（伪造）
    *pxTopOfStack-- = (StackType_t)0x01000000;   // xPSR
    *pxTopOfStack-- = (StackType_t)pxCode;        // PC = 任务入口
    *pxTopOfStack-- = (StackType_t)prvTaskExitError; // LR = 退出错误
    *pxTopOfStack-- = 0;                          // R12
    *pxTopOfStack-- = 0;                          // R3
    *pxTopOfStack-- = 0;                          // R2
    *pxTopOfStack-- = 0;                          // R1
    *pxTopOfStack-- = (StackType_t)pvParameters;  // R0 = 参数

    // 手动保存的寄存器
    *pxTopOfStack-- = 0;   // R11
    *pxTopOfStack-- = 0;   // R10
    *pxTopOfStack-- = 0;   // R9
    *pxTopOfStack-- = 0;   // R8
    *pxTopOfStack-- = 0;   // R7
    *pxTopOfStack-- = 0;   // R6
    *pxTopOfStack-- = 0;   // R5
    *pxTopOfStack-- = 0;   // R4

    return pxTopOfStack;   // 返回新栈顶
}
```

### Task 3: 扩展内存管理章节 — 堆算法源码

**目标文件：** `docs/05_memory_management.md`

**扩展内容：**

#### 3.1 heap_4.c 完整算法（新增 ~500 行）

```c
// 空闲块头部
typedef struct A_BLOCK_LINK {
    size_t xBlockSize;                      // 块大小（含头部）
    struct A_BLOCK_LINK *pxNextFreeBlock;    // 下一个空闲块
} BlockLink_t;

// 块大小掩码（去除低两位状态位）
#define heapBLOCK_SIZE_MASK ( ~(size_t)heapBYTE_ALIGNMENT_MASK )

// xStart 和 xEnd 管理空闲链表
static BlockLink_t xStart;
static BlockLink_t *pxEnd = NULL;

// 分配算法 — 最佳适应 O(n)
void *pvPortMalloc(size_t xWantedSize) {
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
    void *pvReturn = NULL;

    // 对齐并加上头部
    xWantedSize += xHeapStructSize;
    xWantedSize &= heapBYTE_ALIGNMENT_MASK;

    if (xWantedSize > 0 && xWantedSize <= xFreeBytesRemaining) {
        // 最佳适应：从最小满足的块开始
        pxPreviousBlock = &xStart;
        pxBlock = xStart.pxNextFreeBlock;

        while (pxBlock->xBlockSize < xWantedSize) {
            pxPreviousBlock = pxBlock;
            pxBlock = pxBlock->pxNextFreeBlock;
        }

        // 找到满足的块，分配
        if (pxBlock != pxEnd) {
            pvReturn = (void *)((uint8_t *)pxBlock + xHeapStructSize);
            // 从空闲链表移除
            pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;
        }
    }
    return pvReturn;
}

// 释放算法 — 合并相邻空闲块 O(1) 或 O(n)
void vPortFree(void *pv) {
    BlockLink_t *pxLink = (BlockLink_t *)((uint8_t *)pv - xHeapStructSize);
    BlockLink_t *pxIterator;

    // 简单插入合并
    pxIterator = &xStart;
    while (pxIterator->pxNextFreeBlock > pxLink) {
        pxIterator = pxIterator->pxNextFreeBlock;
    }

    // 合并逻辑：检查是否相邻
    if ((uint8_t *)pxLink + pxLink->xBlockSize == (uint8_t *)pxIterator->pxNextFreeBlock) {
        pxLink->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
        pxLink->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
    }
}
```

### Task 4: 扩展同步机制章节 — 队列底层实现

**目标文件：** `docs/06_synchronization.md`

**扩展内容：**

#### 4.1 队列结构体分析（新增 ~300 行）

```c
// queue.c 中的队列结构
typedef struct QueueDefinition {
    int8_t *pcHead;              // 队列头
    int8_t *pcTail;              // 队列尾
    int8_t *pcWriteTo;           // 写入位置
    int8_t *pcReadFrom;          // 读取位置

    UBaseType_t uxLength;        // 队列长度
    UBaseType_t uxItemSize;      // 每个元素大小

    volatile int8_t cMessagesWaiting;  // 等待消息数

    UBaseType_t uxMutexHeldCount;     // 持有计数（互斥量用）

    List_t xTasksWaitingToSend;       // 等待发送的任务列表
    List_t xTasksWaitingToReceive;    // 等待接收的任务列表

    #if ( configUSE_QUEUE_SETS == 1 )
        struct QueueDefinition *pxQueueSetContainer;
    #endif

    // ...
} xQUEUE;

// 队列类型
typedef enum {
    queueQUEUE_TYPE_BASE,       // 普通队列
    queueQUEUE_TYPE_MUTEX,       // 互斥量
    queueQUEUE_TYPE_COUNTING_SEMAPHORE,  // 计数信号量
    queueQUEUE_TYPE_BINARY_SEMAPHORE    // 二值信号量
} queueQUEUE_TYPE;
```

#### 4.2 发送/接收阻塞机制（新增 ~200 行）

```c
// 阻塞接收实现
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait) {
    BaseType_t xEntryTimeSet = pdFALSE;
    TimeOut_t xTimeOut;

    while (cMessagesWaiting == 0) {  // 无消息
        if (xTicksToWait > 0) {
            // 当前任务加入等待列表，阻塞
            vTaskPlaceOnUnorderedEventList(
                &xQueue->xTasksWaitingToReceive,
                xTicksToWait + xTaskGetTickCount()
            );
            taskYIELD();  // 让出 CPU
        } else {
            return pdFALSE;  // 超时
        }
    }

    // 收到消息，复制到缓冲区
    memcpy(pvBuffer, pcReadFrom, uxItemSize);
    return pdTRUE;
}
```

### Task 5: 扩展中断管理章节

**目标文件：** `docs/07_interrupt.md`

**扩展内容：**

#### 5.1 BASEPRI 寄存器详解（新增 ~200 行）

```
ARM Cortex-M 优先级掩码：

BASEPRI 寄存器：
- 设为 N 时，屏蔽所有优先级 <= N 的中断
- 设为 0 时，不屏蔽任何中断

FreeRTOS 配置：
configMAX_SYSCALL_INTERRUPT_PRIORITY = 5

进入临界区时：
portDISABLE_INTERRUPTS() → BASEPRI = 5
→ 屏蔽优先级 0-5 的中断
→ 6-15 优先级的中断仍可响应

退出临界区时：
portENABLE_INTERRUPTS() → BASEPRI = 0
→ 所有中断恢复
```

#### 5.2 FromISR 完整说明（新增 ~150 行）

```c
// FromISR 与普通 API 区别：

普通 API:
- 可能导致任务阻塞
- 只能在任务上下文调用
- 如 xQueueSend(), xSemaphoreTake()

FromISR API:
- 不会阻塞，永远立即返回
- 额外参数 xHigherPriorityTaskWoken
- 如 xQueueSendFromISR()

// 使用模式：
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);

// 必须在退出中断前判断是否需要切换
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### Task 6: 扩展调试章节

**目标文件：** `docs/09_debugging.md`

**扩展内容：**

#### 6.1 GDB 调试 FreeRTOS（新增 ~300 行）

```gdb
# 查看所有任务
(gdb) info threads

# 切换到任务上下文
(gdb) thread N

# 查看任务栈
(gdb) bt

# 查看 TCB 结构
(gdb) p *(tskTCB*)0x20000ABC

# 查看就绪队列
(gdb) p xReadyTasks

# 查看当前任务
(gdb) p pxCurrentTCB

# 内存查看
(gdb) x/32x 0x20000000

# 栈溢出检测
(gdb) watch *(uint32_t*)((char*)pxCurrentTCB->pxStack + uxStackDepth*4 - 4)
```

#### 6.2 SystemView 使用（新增 ~200 行）

```
SEGGER SystemView 配置步骤：

1. 下载安装 J-Link 软件
2. 添加源码到工程：
   - SEGGER_SYSVIEW.c
   - SEGGER_SYSVIEW_FreeRTOS.c

3. 初始化：
   SEGGER_SYSVIEW_Conf();

4. 使用 API：
   SEGGER_SYSVIEW_Printf("Task %s started", name);
   SEGGER_SYSVIEW_OnTaskCreate(taskHandle);

5. 连接调试器，启动 SystemView 录制

6. 分析时间线：
   - 任务调度点
   - 中断响应时间
   - 阻塞等待事件
```

### Task 7: 更新 index.md 添加学习路径建议

**目标文件：** `docs/index.md`

**扩展内容：**
- 推荐阅读顺序
- 每章学习目标
- 面试冲刺阅读顺序

---

## 任务执行顺序

```
Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6 → Task 7
  ↑         ↑         ↑         ↑         ↑         ↑
 P0        P0        P1        P1        P2        P2
```

---

## 执行选项

**A. 子代理并行执行** — 我启动多个子代理同时扩展不同章节，速度快

**B. 我顺序执行** — 我依次扩展各章节，可随时调整方向

**C. 指定章节** — 告诉我你最想先完善哪一章，我优先处理

你选择哪种方式？

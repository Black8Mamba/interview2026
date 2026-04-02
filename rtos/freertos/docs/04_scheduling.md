# 第四章：任务调度与切换

> 本章目标：深入理解 FreeRTOS 调度算法、上下文切换机制、临界区保护

## 章节结构

- [ ] 4.1 调度器概述
- [ ] 4.2 调度算法
- [ ] 4.3 上下文切换
- [ ] 4.4 临界区与中断屏蔽
- [ ] 4.5 任务优先级详解
- [ ] 4.6 面试高频问题
- [ ] 4.7 避坑指南

---

## 4.1 调度器概述

### 调度器启动

```c
// 创建任务后，启动调度器
vTaskStartScheduler();

// 从此再不会返回
// 最高优先级就绪任务开始运行
```

**启动流程：**

```
main()
  → xTaskCreate()      创建任务
  → vTaskStartScheduler()  启动调度器
    → 创建空闲任务
    → 启动节拍定时器（PendSV + SysTick）
    → 选择最高优先级任务执行 ← 不会再返回
```

### 调度器相关配置

```c
#define configUSE_PREEMPTION    1    // 抢占式调度
#define configUSE_TIME_SLICING  1    // 时间片轮转
#define configCPU_CLOCK_HZ      (SystemCoreClock)
#define configTICK_RATE_HZ      (1000)
```

---

## 4.2 调度算法

### 抢占式优先级调度

**核心规则：**
1. 始终运行最高优先级就绪任务
2. 高优先级任务就绪时，立即抢占低优先级任务
3. 同优先级任务按时间片轮转

### O(1)调度器概述

FreeRTOS的核心调度器设计目标是**常数时间复杂度O(1)**的任务调度。这意味着无论系统中有多少任务，调度器选择一个最高优先级任务并切换的时间都是固定的。这对于实时系统至关重要——最坏情况响应时间必须是确定的。

传统RTOS调度器使用红黑树或链表查找最高优先级，时间复杂度为O(log n)或O(n)。FreeRTOS通过**优先级就绪数组**和**位图算法**实现了真正的O(1)调度。

### O(1)调度器数据结构详解

FreeRTOS的就绪队列采用**优先级数组**设计，每个优先级都有独立的链表：

```c
// FreeRTOS就绪队列核心数据结构
typedef struct ReadyQueue {
    List_t xReadyTasks[configMAX_PRIORITIES];  // 每个优先级独立的链表
    volatile UBaseType_t uxTopReadyPriority;    // 追踪当前最高就绪优先级
    volatile UBaseType_t uxSchedulerState;     // 调度器状态
} ReadyQueue_t;

// 全局就绪队列实例
static ReadyQueue_t xReadyTasks;
```

**数据结构设计优势分析：**

1. **数组访问O(1)**：通过优先级直接索引找到对应链表，时间复杂度为O(1)
2. **uxTopReadyPriority优化**：记录当前最高就绪优先级，避免每次遍历所有优先级
3. **每个优先级独立链表**：同优先级任务组织在同一个链表中，支持时间片轮转

**内存布局示意：**

```
xReadyTasks 就绪队列
┌─────────────────────────────────────────────────────────────┐
│ uxTopReadyPriority = 5 (当前最高就绪优先级)                  │
├─────────────────────────────────────────────────────────────┤
│ 优先级      链表                                              │
│ [0]         ───→ [idle_task] ───→ END                       │
│ [1]         ───→ END                                         │
│ [2]         ───→ [task_C] ───→ [task_D] ───→ END            │
│ [3]         ───→ END                                         │
│ [4]         ───→ END                                         │
│ [5]         ───→ [task_A] ───→ [task_B] ───→ END  ← 最高优先级│
│ [6]         ───→ END                                         │
│ ...         ...                                              │
│ [31]        ───→ END                                         │
└─────────────────────────────────────────────────────────────┘
```

### 优先级位图算法（Priority Bitmap Algorithm）

FreeRTOS使用**位图算法**配合ARM Cortex-M的**CLZ（Count Leading Zeros）指令**实现O(1)最高优先级查找。

#### 传统方法 vs 位图方法

**传统方法（O(n)）：**
```c
// 遍历所有优先级查找最高的就绪任务 - 时间复杂度O(n)
UBaseType_t prvGetHighestPriority(void) {
    UBaseType_t uxHighestPriority = 0;
    for (UBaseType_t i = configMAX_PRIORITIES - 1; i > 0; i--) {
        if (listLIST_IS_EMPTY(&xReadyTasks.xReadyTasks[i]) == pdFALSE) {
            uxHighestPriority = i;
            break;
        }
    }
    return uxHighestPriority;
}
```

**位图方法（O(1)）：**

FreeRTOS维护一个32位的位图变量，每个bit代表一个优先级是否就绪：

```c
// 32位系统示例：configMAX_PRIORITIES = 32
// uxReadyPriorities 是一个32位无符号整数
// bit[N] = 1 表示优先级N有任务就绪

// 位图示例：0b00000000000000000000000000011010
//          └── bit[1]=1, bit[3]=1, bit[4]=1 就绪
```

#### CLZ指令详解

**CLZ（Count Leading Zeros）指令**是ARM Cortex-M提供的硬件指令，用于计算前导零的数量：

```
语法：CLZ Rd, Rm
功能：计算Rm中前导零的数量，结果存入Rd

例子：
CLZ R0, R1

如果 R1 = 0b00000000000000000000000000011010 (十进制 26)
    R0 = 27  (从左边数有27个零，然后是1)
```

**O(1)优先级计算公式：**
```
最高优先级 = 31 - CLZ(uxReadyPriorities)
```

为什么？因为32位数字的前导零数量 + 最高bit位置 = 31。

```c
// FreeRTOS 优先级查找宏定义
#define portGET_HIGHEST_PRIORITY(uxTop, uxReadyPriorities) \
    uxTop = uxReadyPriorities; \
    if( uxTop > uxTopReadyPriority ) { \
        uxTopReadyPriority = uxTop; \
    }

// 实际使用示例：
// 假设 uxReadyPriorities = 0b00000000000000000000000000011010
// CLZ = 27 (前导零数量)
// 最高优先级 = 31 - 27 = 4

// 在ARM Cortex-M3/M4上，CLZ指令执行周期为2-3个时钟周期
// 这使得最高优先级查找真正达到O(1)常数时间
```

#### 位图更新机制

**设置优先级位：**
```c
// 当任务进入就绪状态时，设置对应优先级位
static inline void prvSetTaskPriorityBitmap(UBaseType_t uxPriority) {
    uxReadyPriorities |= (1UL << uxPriority);
}
```

**清除优先级位：**
```c
// 当任务离开就绪状态时，清除对应优先级位
static inline void prvClearTaskPriorityBitmap(UBaseType_t uxPriority) {
    // 只有该优先级没有其他就绪任务时才清除
    if (listLIST_IS_EMPTY(&xReadyTasks.xReadyTasks[uxPriority])) {
        uxReadyPriorities &= ~(1UL << uxPriority);
    }
}
```

### prvAddTaskToReadyQueue 源码分析

**函数作用：将任务添加到就绪队列**

```c
// FreeRTOS任务入队核心函数
void prvAddTaskToReadyQueue(TCB_t *pxTCB) {
    // 步骤1：O(1)链表尾插 - 直接找到对应优先级链表，在尾部插入
    // listINSERT_END 是链表操作的O(1)实现
    listINSERT_END(&xReadyTasks.xReadyTasks[pxTCB->uxPriority],
                   &pxTCB->xStateListItem);

    // 步骤2：O(1)更新位图 - 设置该优先级对应的bit
    prvSetTaskPriorityBitmap(pxTCB->uxPriority);

    // 步骤3：更新最高优先级追踪器
    // uxTopReadyPriority 记录见过的最高优先级，用于快速路径优化
    if (pxTCB->uxPriority >= uxTopReadyPriority) {
        uxTopReadyPriority = pxTCB->uxPriority;
    }
}
```

**入队操作分解图解：**

```
假设任务A（优先级5）即将加入就绪队列

入队前状态：
xReadyTasks.xReadyTasks[5]: [TaskB] → [TaskC] → END
                                      ↑ pxTail

pxTCB->xStateListItem:
┌──────────────┐
│ pvOwner      │  ← 指向TaskA的TCB
│ pxNext       │  ← 初始指向自己（未链接）
│ pxPrevious   │
└──────────────┘

执行 listINSERT_END(&xReadyTasks.xReadyTasks[5], &pxTCB->xStateListItem):

链表结构 (listINSERT_END在尾部插入)：
[TaskB] → [TaskC] → [TaskA(新)] → [TaskB] (循环回到头部)
                   ↑新的尾部

时间复杂度分析：
- 链表尾部插入：O(1)  (有尾指针或循环链表特性)
- 位图更新：O(1)      (位运算)
- 优先级更新：O(1)   (简单比较赋值)
```

**链表listINSERT_END的O(1)实现：**

```c
// 链表尾部插入的奥义在于：链表是双向循环链表
// 链表头节点的pxNext指向第一个元素，pxPrevious指向最后一个元素

#define listINSERT_END(pxList, pxNewListItem) \
    do { \
        volatile List_t * const pxList_tmp = (pxList); \
        (pxNewListItem)->pxNext = pxList_tmp->pxIndex; \
        (pxNewListItem)->pxPrevious = pxList_tmp->pxIndex->pxPrevious; \
        pxList_tmp->pxIndex->pxPrevious->pxNext = (pxNewListItem); \
        pxList_tmp->pxIndex->pxPrevious = (pxNewListItem); \
    } while(0)

// 图解：
// 原链表：[A] ↔ [B] ↔ [C] (循环)
//         ↑                            ↑
//       pxIndex                      pxIndex->pxPrevious (尾部)

// 插入D后：[A] ↔ [B] ↔ [C] ↔ [D] (循环)
//         ↑                  ↑
//       pxIndex          pxIndex->pxPrevious (新尾部)
```

### prvRemoveTaskFromReadyQueue 源码分析

**函数作用：从就绪队列移除任务**

```c
// FreeRTOS任务出队核心函数
void prvRemoveTaskFromReadyQueue(TCB_t *pxTCB) {
    // 步骤1：从链表中移除 - O(1)双向链表移除
    listREMOVE_ITEM(&pxTCB->xStateListItem);

    // 步骤2：检查该优先级是否还有任务，如果没有则清除位图
    if (listLIST_IS_EMPTY(&xReadyTasks.xReadyTasks[pxTCB->uxPriority])) {
        prvClearTaskPriorityBitmap(pxTCB->uxPriority);
    }
}
```

**时间复杂度总结：**

| 操作 | 时间复杂度 | 原因 |
|------|----------|------|
| 任务入队 | O(1) | 链表尾插 + 位图置位 |
| 任务出队 | O(1) | 链表移除 + 位图检查 |
| 查找最高优先级 | O(1) | CLZ指令 |
| 同优先级轮转 | O(1) | 环形链表遍历器 |

### 时间片轮转（Time Slicing）

当多个同优先级任务就绪时，调度器轮流执行每个任务一个时间片：

```
时间线 →
| Task1(10ms) | Task2(10ms) | Task1(10ms) | Task2(10ms) | ...
   时间片1      时间片2
```

```c
#define configUSE_TIME_SLICING  1    // 开启（默认）

// 关闭时间片后，同优先级任务不会自动轮转
// 必须低优先级任务主动调用 taskYIELD() 让出
```

### 时间片轮转源码深度分析

时间片轮转的核心在**xTaskIncrementTick()**函数中实现，每次Tick中断都会检查是否需要切换任务。

#### xTaskIncrementTick 完整源码分析

```c
// FreeRTOS Tick中断处理核心函数
BaseType_t xTaskIncrementTick(TaskTimer_t * const pxTimer ) {
    BaseType_t xSwitchRequired = pdFALSE;

    // 防止中断嵌套时的重复处理
    if (uxSchedulerSuspended == pdFALSE) {
        const TickType_t xConstTickCount = uxTickCount + 1;
        uxTickCount = xConstTickCount;

        // 优先级天花板协议检查
        // 如果当前任务持有了互斥量且优先级被提升过
        if (xMountReadyTasks != pdFALSE) {
            // 处理优先级天花板协议（优先级继承）
            prvCheckPriorityExpiration();
        }

        // 时间片轮转核心逻辑
        #if configUSE_TIME_SLICING == 1
        {
            // 检查当前任务的时间片是否耗尽
            if (pxCurrentTCB->xTicks > 0) {
                // 时间片未耗尽，减1继续执行
                pxCurrentTCB->xTicks--;
            } else {
                // 时间片耗尽！需要切换到同优先级的下一个任务
                // O(1)操作：将当前任务移到同优先级链表尾部
                listINSERT_END(&xReadyTasks.xReadyTasks[pxCurrentTCB->uxPriority],
                              &pxCurrentTCB->xStateListItem);

                // O(1)选择下一个最高优先级任务
                taskSELECT_HIGHEST_PRIORITY_TASK();

                xSwitchRequired = pdTRUE;
            }
        }
        #endif

        // 任务过期处理（vTaskDelay等）
        if (xConstTickCount >= xNextTaskUnblockTime) {
            // 唤醒已到期的阻塞任务
            prvCheckExpiredTasks(&xSwitchRequired);
        }

        // 空闲任务呼唤（当没有其他任务需要运行时）
        #if configUSE_IDLE_HOOK == 1
        {
            if (xSwitchRequired == pdFALSE) {
                // 只有在没有更高优先级任务就绪时才执行空闲任务
                vApplicationIdleHook();
            }
        }
        #endif
    }

    return xSwitchRequired;
}
```

#### 时间片耗尽时的O(1)切换过程

```c
// 时间片耗尽时的关键切换代码分析

// 当前任务时间片减到0时执行：
else {
    // 步骤1：O(1)链表操作 - 将当前任务移到同优先级链表尾部
    // 这实现了Round-Robin轮转
    listINSERT_END(
        &xReadyTasks.xReadyTasks[pxCurrentTCB->uxPriority],  // 找到当前优先级链表
        &pxCurrentTCB->xStateListItem                        // 当前任务节点
    );

    // 步骤2：O(1)选择下一个任务
    taskSELECT_HIGHEST_PRIORITY_TASK();

    // 步骤3：返回需要切换标志
    xSwitchRequired = pdTRUE;
}

// 时间片轮转示意图：
//
// 同优先级链表：[TaskA] ↔ [TaskB] ↔ [TaskC] (循环)
//                                     ↑
//                                  pxIndex (当前)

// TaskA时间片耗尽后：
// 1. listINSERT_END 将TaskA移到尾部
// 2. 链表变为：[TaskB] ↔ [TaskC] ↔ [TaskA]
//                                  ↑
//                               pxIndex

// TaskB成为下一个执行的任务
```

#### 时间片轮转的环形遍历机制

FreeRTOS使用一个**环形链表遍历器（Round-Robin Iterator）**来实现同优先级任务的轮转：

```c
// 链表结构中的遍历器
typedef struct xLIST {
    volatile ListItem_t *pxIndex;      // 当前遍历位置指针
    UBaseType_t uxNumberOfItems;       // 链表中的项目数量
    MiniListItem_t xListEnd;           // 链表哨兵节点（end marker）
} List_t;

// 关键宏：获取下一个链表节点
#define listGET_OWNER_OF_NEXT_ENTRY(pxVar, pxList) \
    do { \
        List_t * const pxList_tmp = (pxList); \
        if (++(pxList_tmp->pxIndex) == (ListItem_t *)(&(pxList_tmp->xListEnd))) { \
            (pxList_tmp->pxIndex) = (ListItem_t *)((pxList_tmp->xListEnd)).pxNext; \
        } \
        (pxVar) = (pxList_tmp->pxIndex)->pvOwner; \
    } while(0)

// 这个宏是O(n)遍历！其中n是同优先级任务数量
```

**环形遍历过程图解：**

```
链表结构（带哨兵节点的循环链表）：

[ListItem: TaskA] ↔ [ListItem: TaskB] ↔ [ListItem: TaskC] ↔ [ListItem: END]
     ↑                                                            │
     └────────────────────────────────────────────────────────────┘

pxIndex 指针移动过程：
1. 初始：pxIndex 指向 TaskA
2. 第一次调用 listGET_OWNER_OF_NEXT_ENTRY：
   - pxIndex++ 后指向 TaskB
   - 返回 TaskB

3. 第二次调用：
   - pxIndex++ 后指向 TaskC
   - 返回 TaskC

4. 第三次调用：
   - pxIndex++ 后指向 END 哨兵
   - 检测到是END节点，pxIndex 绕回到 TaskA
   - 返回 TaskA

这实现了循环遍历，每次调用都顺序获取下一个任务。
```

**关键发现：listGET_OWNER_OF_NEXT_ENTRY 是O(n)遍历**

虽然选取下一个最高优先级任务是O(1)，但在**同优先级列表内轮转时**，时间复杂度是**O(n)**，其中n是同优先级任务数量。

这是FreeRTOS设计中的一个微妙之处：
- 选择最高优先级：O(1) ✓（通过位图+CLZ）
- 跨优先级任务选择：O(1) ✓（直接索引）
- 同优先级内轮转：O(n) ✗（需要遍历链表）

**为什么这样设计是合理的？**

1. **实际场景中同优先级任务通常很少**（通常1-3个）
2. **O(n)中的n是同优先级数量，不是总任务数**
3. **关键路径（高优先级任务抢占）始终是O(1)**

### 优先级上限天花板协议

当任务获取互斥量时，其优先级会临时提升，防止优先级反转：

```
场景：
- 任务H（高优先级）：需要互斥量
- 任务M（中优先级）：运行中
- 任务L（低优先级）：持有互斥量

问题：任务H被阻塞，任务M抢占了CPU，造成优先级反转

解决：任务L的优先级提升到任务H，任务M无法抢占
```

### taskSELECT_HIGHEST_PRIORITY_TASK 宏详解

这个宏是FreeRTOS调度器的核心，选择下一个要运行的任务：

```c
// 选择最高优先级任务的宏
#define taskSELECT_HIGHEST_PRIORITY_TASK() \
    do { \
        UBaseType_t uxTopPriority; \
        \
        /* 步骤1：O(1)找出最高就绪优先级 */ \
        /* 使用前导零计数(CLZ)指令，在ARM Cortex-M上这是单条指令 */ \
        portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities); \
        \
        /* 步骤2：从该优先级链表中获取下一个任务 */ \
        /* O(n)遍历，n是同优先级任务数 */ \
        listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, \
            &(xReadyTasks.xReadyTasks[uxTopPriority])); \
    } while(0)
```

**执行流程图：**

```
调用 taskSELECT_HIGHEST_PRIORITY_TASK()

         │
         ▼
┌─────────────────────────────────┐
│ 1. portGET_HIGHEST_PRIORITY()   │
│    - 读取 uxReadyPriorities 位图 │
│    - CLZ 指令计算前导零数量       │
│    - 得出 uxTopPriority         │
│    时间复杂度: O(1)              │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│ 2. listGET_OWNER_OF_NEXT_ENTRY  │
│    - xReadyTasks[uxTopPriority] │
│    - pxIndex 指向下一个任务      │
│    - 返回该任务的TCB             │
│    时间复杂度: O(n) 同优先级数   │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│ 3. pxCurrentTCB 更新            │
│    - pxCurrentTCB 指向新任务TCB  │
│    - 下次上下文切换将恢复此任务   │
└─────────────────────────────────┘
```

### vTaskSwitchContext 深度分析

```c
// FreeRTOS上下文切换的核心函数
void vTaskSwitchContext(void) {
    // 选择下一个要运行的任务
    // 这会更新 pxCurrentTCB 指向新的任务
    taskSELECT_HIGHEST_PRIORITY_TASK();

    // 跟踪记录 - 用于调试和可视化
    traceTASK_SWITCHED_IN();
}
```

**为什么vTaskSwitchContext这么简单？**

FreeRTOS的设计哲学是**关注点分离**：
- 调度器逻辑（选择哪个任务）：在C代码中，平台无关
- 上下文切换（保存/恢复寄存器）：在汇编中，平台相关

这种设计使得：
1. 调度逻辑易于测试和验证
2. 移植到新架构时只需重写汇编部分
3. 代码更清晰，维护更容易

### O(1)调度器的完整时序图

```
场景：系统中存在3个任务
- TaskH (优先级7) - 高优先级
- TaskM (优先级4) - 中优先级  
- TaskL (优先级2) - 低优先级

当前TaskM正在运行，TaskH突然就绪

                    Time
                     │
TaskM ───────────────┼────────────────────────────→ [被抢占]
                     │
                     │ 1. TaskH 调用阻塞API（如等待信号量）
                     │    → prvAddTaskToReadyQueue(TaskH)
                     │
                     │ 2. 调度器检查到更高优先级任务就绪
                     │    → 设置 xSwitchRequired = pdTRUE
                     │
                     │ 3. Tick中断或portYIELD触发PendSV
                     │
                     │ 4. PendSV_Handler 执行
                     │
                     │ 5. portSAVE_CONTEXT() - 保存TaskM上下文
                     │
                     │ 6. vTaskSwitchContext()
                     │    → taskSELECT_HIGHEST_PRIORITY_TASK()
                     │    → uxReadyPriorities = 0b10000000 (bit7=1)
                     │    → CLZ = 24
                     │    → uxTopPriority = 31-24 = 7  ← O(1)
                     │    → pxCurrentTCB = TaskH.TCB
                     │
                     │ 7. portRESTORE_CONTEXT() - 恢复TaskH上下文
                     │
                     ▼
TaskH ─────────────────────────────────────────────────────→ [运行]

调度延迟：从TaskH就绪到执行，时间固定可预测
最大延迟 = PendSV中断响应时间 + 上下文保存/恢复时间
这对于实时系统至关重要！
```

---

## 4.3 上下文切换

### 何时触发切换

| 触发时机 | 说明 |
|---------|------|
| Tick 中断 | 时间片耗尽，切换同优先级任务 |
| 高优先级任务就绪 | 抢占当前运行任务 |
| 任务调用 `taskYIELD()` | 主动让出 CPU |
| 阻塞/挂起当前任务 | 调度其他任务 |

### PendSV 中断

FreeRTOS 使用 **PendSV** 异常来触发上下文切换：

```c
// 触发 PendSV（软中断）
portYIELD();

// 实际实现：
// #define portYIELD() do { *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET; } while(0)
```

### 上下文切换完整过程详解

FreeRTOS的上下文切换是嵌入式系统中最精妙的设计之一。它充分利用了ARM Cortex-M的硬件特性，将寄存器保存/恢复的开销降到最低。

#### 完整的上下文切换流程图

```
1. taskYIELD() 或 PendSV 触发
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 2. 硬件自动压栈：R0-R3, R12, LR, PC, xPSR → PSP (进程栈指针) │
│    - 这8个寄存器由硬件自动保存到当前任务的栈帧               │
│    - 发生在异常进入时，由处理器自动完成                       │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 3. PendSV_Handler 中断处理函数被调用                         │
│    - 处理器跳转到PendSV异常处理程序                          │
│    - 使用MSP（主栈指针）继续执行                             │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 4. portSAVE_CONTEXT() — 手动压栈 R4-R11                     │
│    - 编译器生成的函数 prologue/epilogue                       │
│    - 将R4-R11保存到当前任务的PSP栈帧中                       │
│    - 此时完整的任务上下文已保存                              │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 5. vTaskSwitchContext() — O(1)选择下一个任务                 │
│    - taskSELECT_HIGHEST_PRIORITY_TASK()                     │
│    - 更新 pxCurrentTCB 指向新的任务                           │
│    - 纯C代码，平台无关                                       │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 6. pxCurrentTCB 已更新，指向新任务的TCB                       │
│    - xGenericListItem.pxNext 指向新任务                      │
│    - 新任务的栈顶指针 pxTopOfStack 已准备好                   │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 7. portRESTORE_CONTEXT() — 手动弹栈 R4-R11                  │
│    - 从新任务的PSP栈帧中恢复R4-R11                           │
│    - 设置 PSP 指向新任务的栈帧                                │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 8. Exception Return — 硬件自动弹栈 R0-R3, R12, LR, PC, xPSR  │
│    - 处理器从栈中恢复剩余寄存器                               │
│    - PC指向新任务的下一条指令地址                             │
│    - 自动切换回进程栈指针(PSP)                                │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│ 9. Resume new task — 新任务开始执行                          │
│    - 从上次暂停的位置继续运行                                 │
│    - 完全看不到自己被切换的过程                               │
└──────────────────────────────────────────────────────────────┘
```

#### ARM Cortex-M 寄存器压栈机制详解

ARM Cortex-M处理器在异常进入时自动保存8个寄存器到栈：

**硬件自动保存的寄存器（R0-R3, R12, LR, PC, xPSR）：**

```
异常发生时，处理器自动执行：
PUSH {R0-R3, R12, LR, PC, xPSR}

栈帧布局（从高地址向低地址生长）：

高地址
┌─────────────────┐
│    xPSR         │ ← 程序状态寄存器
├─────────────────┤
│      PC         │ ← 程序计数器（返回地址）
├─────────────────┤
│      LR         │ ← 连接寄存器（返回地址）
├─────────────────┤
│     R12         │ ← 调用者保存寄存器
├─────────────────┤
│     R3          │
│     R2          │ ← 通用寄存器
│     R1          │
│     R0          │
├─────────────────┤
│                 │ ← 手动压栈的 R4-R11 会放在这里
低地址
```

**为什么选择R4-R11手动保存？**

1. **调用者保存 vs 被调用者保存**
   - R0-R3, R12, LR, PC, xPSR：由调用者（编译器/硬件）保存
   - R4-R11：由被调用函数自己保存（被调用者保存）

2. **性能优化**
   - 硬件压栈在异常进入时自动完成，无额外开销
   - 软件（编译器）生成的函数会保存R4-R11
   - 所以FreeRTOS复用这个机制

3. **中断响应延迟**
   - 减少中断响应时需要保存的寄存器数量
   - 加快上下文切换速度

#### portSAVE_CONTEXT 汇编实现分析

```c
// FreeRTOS上下文保存宏（ARM Cortex-M）
// 在 portasm.s 或 portmacro.h 中定义

#define portSAVE_CONTEXT()                                          \
    extern volatile UBaseType_t uxCriticalNesting;                  \
    mrs r0, psp                    /* R0 = PSP (进程栈指针) */      \
    ldr r1, =pxCurrentTCB          /* R1 = &pxCurrentTCB */         \
    ldr r1, [r1]                   /* R1 = pxCurrentTCB */          \
    stmdb r0!, {r4-r11}           /* 保存R4-R11到PSP栈 */           \
    str r0, [r1]                   /* 保存新的栈顶到TCB */          \
    mrs r0, primask                /* R0 = 当前中断状态 */            \
    stmdb r0!, {r0}                /* 保存primask到栈 */             \
    ldr r1, =uxCriticalNesting     /* R1 = &uxCriticalNesting */    \
    str r0, [r1]                   /* 保存到全局变量 */              \
    ldr r0, =portendiNVIC          /* R0 = 结束 */                   \
    bx r0                          /* 跳转到结束 */

; 简化版流程分析：
;
; 1. MRS R0, PSP
;    将进程栈指针(PSP)读取到R0
;
; 2. LDR R1, =pxCurrentTCB
;    将TCB指针变量地址加载到R1
;
; 3. LDR R1, [R1]
;    将pxCurrentTCB的值（即当前TCB地址）加载到R1
;
; 4. STMDB R0!, {R4-R11}
;    多寄存器存储，将R4-R11压入R0指向的地址
;    R0! 表示写完后更新R0（递减）
;
; 5. STR R0, [R1]
;    将新的栈指针保存到TCB中
;    下次恢复时从这里读取栈顶
```

#### portRESTORE_CONTEXT 汇编实现分析

```c
// FreeRTOS上下文恢复宏
#define portRESTORE_CONTEXT()                                       \
    extern volatile UBaseType_t uxCriticalNesting;                  \
    ldr r0, =uxCriticalNesting     /* R0 = &uxCriticalNesting */     \
    ldr r0, [r0]                   /* R0 = uxCriticalNesting */       \
    ldr r1, =portendiNVIC         /* R1 = 中断嵌套计数结束标记 */    \
    cmp r0, r1                    /* 比较 */                         \
    bne exit_critical             /* 不相等则跳转 */                 \
    ldr r0, =pxCurrentTCB         /* R0 = &pxCurrentTCB */          \
    ldr r0, [r0]                   /* R0 = pxCurrentTCB */           \
    ldr r0, [r0]                   /* R0 = pxTopOfStack */           \
    ldmia r0!, {r4-r11}          /* 从栈恢复R4-R11 */                \
    msr psp, r0                   /* 更新PSP */                      \
    bx lr                         /* 返回 */                        \
    exit_critical:                 /* 临界区退出 */                  \
    ...

; 简化版流程分析：
;
; 1. LDR R0, =pxCurrentTCB
;    加载TCB指针地址
;
; 2. LDR R0, [R0]
;    加载pxCurrentTCB指向的TCB地址
;
; 3. LDR R0, [R0]
;    加载TCB中保存的栈顶指针(pxTopOfStack)
;
; 4. LDMIA R0!, {R4-R11}
;    多寄存器加载，从栈恢复R4-R11
;    R0! 表示加载后更新R0
;
; 5. MSR PSP, R0
;    将新的栈指针写入PSP
;
; 6. BX LR
;    返回，此时会自动恢复R0-R3等寄存器
```

#### 任务栈帧完整布局

**任务首次创建时的初始栈帧：**

```
FreeRTOS任务创建时，会初始化一个"假的"栈帧，
模拟任务已经执行过一次中断保存的样子。

高地址
┌─────────────────┐
│    0xFFFFFFFD   │ ← xPSR (Thumb状态位)
├─────────────────┤
│  task_function  │ ← PC (任务入口函数地址)
├─────────────────┤
│       0         │ ← LR (退出时跳转到什么地址)
├─────────────────┤
│       0         │ ← R12
├─────────────────┤
│       0         │ ← R3
├─────────────────┤
│       0         │ ← R2
├─────────────────┤
│       0         │ ← R1
├─────────────────┤
│ pvParameters    │ ← R0 (传递给任务的参数)
├─────────────────┤
│                 │ ← 这里是手动压栈区域 (R4-R11)
│                 │    首次运行时为空，因为从未执行过保存
└─────────────────┘
  pxTopOfStack 指向

低地址

首次任务切换时：
- 硬件自动压栈：R0-R3, R12, LR, PC, xPSR
- 软件手动压栈：R4-R11 (此时还未保存，是干净的)
```

**任务正常运行中的栈帧：**

```
高地址
┌─────────────────┐
│    xPSR         │ ← 硬件自动压栈
├─────────────────┤
│      PC         │ ← 被中断的任务下一条指令地址
├─────────────────┤
│      LR         │ ← 异常返回模式
├─────────────────┤
│     R12         │
├─────────────────┤
│     R3          │
│     R2          │
│     R1          │
│     R0          │
├─────────────────┤
│     R11         │ ← 软件手动压栈
│     R10         │
│     R9          │
│     R8          │
│     R7          │
│     R6          │
│     R5          │
│     R4          │
├─────────────────┤
│                 │ ← 任务实际使用的栈空间
│                 │
│                 │
└─────────────────┘
  pxTopOfStack 指向

低地址
```

### 任务控制块（TCB）结构分析

```c
// FreeRTOS任务控制块结构
typedef struct TaskDefinition {
    StackType_t *pxTopOfStack;        // 栈顶指针 (的关键！)

    ListItem_t xStateListItem;       // 状态链表节点
    ListItem_t xEventListItem;       // 事件链表节点

    UBaseType_t uxPriority;          // 任务优先级

    StackType_t *pxStack;            // 栈起始地址

    char pcTaskName[configMAX_TASK_NAME_LEN];  // 任务名称

    #if configUSE_MUTEXES == 1
        UBaseType_t uxBasePriority;  // 基础优先级（用于优先级继承）
    #endif

    #if configUSE_TASK_NOTIFICATIONS == 1
        volatile uint32_t ulNotifiedValue;     // 通知值
        volatile uint8_t ucNotifyState;         // 通知状态
    #endif

    #if configUSE_TIME_SLICING == 1
        TickType_t xTicks;           // 时间片剩余 ticks
    #endif

    // ... 其他成员
} TaskHandle_t;
```

**pxTopOfStack 的关键作用：**

```
任务切换时的栈操作：

保存阶段：
1. 获取 pxCurrentTCB->pxTopOfStack（当前栈顶）
2. STMDB R0!, {R4-R11}（保存寄存器到栈）
3. 更新 pxCurrentTCB->pxTopOfStack = R0（保存新的栈顶位置）

恢复阶段：
1. 获取 pxCurrentTCB->pxTopOfStack（新栈顶）
2. LDMIA R0!, {R4-R11}（恢复寄存器）
3. MSR PSP, R0（更新PSP）
```

### 中断优先级与调度器的交互

ARM Cortex-M的NVIC（嵌套向量中断控制器）支持16级优先级（0-15）：

```
优先级数值越小 = 优先级越高（这是ARM的规定！）

| 优先级值 | 优先级级别 | FreeRTOS使用 |
|---------|-----------|-------------|
| 0       | 最高      | 不可屏蔽中断 |
| 1-4     | 高优先级  | 不能调用FreeRTOS API |
| 5-15    | 中低优先级| 可以调用FromISR API |
| 16+     | 不存在    | Cortex-M只支持0-15 |

configMAX_SYSCALL_INTERRUPT_PRIORITY = 5
configKERNEL_INTERRUPT_PRIORITY = 15 (config lowest)
```

**关键配置：**

```c
// FreeRTOSConfig.h 中的关键配置

#define configMAX_SYSCALL_INTERRUPT_PRIORITY    5
// 中断优先级 <= 5 的中断可以调用 FreeRTOS API
// 中断优先级 > 5 的中断不能调用任何 FreeRTOS API

#define configKERNEL_INTERRUPT_PRIORITY         15
// SysTick 和 PendSV 的优先级设置为最低
// 确保用户中断可以正常嵌套
```

**为什么PendSV设置为最低优先级？**

```
原因：
1. 确保所有高优先级中断可以随时打断PendSV
2. 让用户中断先处理完，再进行任务切换
3. 避免频繁的中断嵌套导致栈溢出

典型配置：
- 用户中断（UART, SPI等）：优先级 5-10
- SysTick（节拍定时器）：优先级 15（最低）
- PendSV（软件触发）：优先级 15（最低）
```

### 栈帧布局

**任务首次运行时：**

```
High Address
┌─────────────────┐
│  xPSR           │ ← 初始状态
├─────────────────┤
│  PC (R15)       │ ← 任务入口地址
├─────────────────┤
│  LR (R14)       │ ← 退出时跳转到任务函数
├─────────────────┤
│  R12            │
├─────────────────┤
│  R3             │
│  R2             │
│  R1             │ ← pvParameters 参数
│  R0             │
├─────────────────┤
│  R11            │
│  ...            │
│  R4             │ ← 手动压栈（寄存器）
└─────────────────┘
  Low Address
  (pxTopOfStack 指向)
```

---

## 4.4 临界区与中断屏蔽

### 临界区（Critical Section）

```c
// 进入临界区 — 关闭所有可屏蔽中断
taskENTER_CRITICAL();
// 受保护代码，不可被中断打断
taskEXIT_CRITICAL();
```

**实现原理：**
```c
// portmacro.h
#define taskENTER_CRITICAL()    portENTER_CRITICAL()

#define portENTER_CRITICAL()    vPortEnterCritical()

void vPortEnterCritical(void) {
    portDISABLE_INTERRUPTS();        // 设置 BASEPRI，屏蔽中断
    uxCriticalNesting++;             // 嵌套计数
    if (uxCriticalNesting == 1) {
        // 记录当前任务在临界区
    }
}
```

### 中断屏蔽级别

ARM Cortex-M 中断优先级：

```
优先级数值越小 = 优先级越高

| 优先级 | 说明 |
|--------|------|
| 0 | 最高（不可屏蔽） |
| 1-4 | 高优先级中断（FreeRTOS 不允许调用 API） |
| 5-15 | 可调用 FromISR API |
| 16+ | 不存在（Cortex-M 只支持 0-15） |
```

```c
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5)

// 中断优先级 <= 5 可调用 FreeRTOS API
// 中断优先级 > 5 不可调用任何 FreeRTOS API
```

### taskENTER_CRITICAL vs 关闭中断

| 特性 | taskENTER_CRITICAL | portDISABLE_INTERRUPTS |
|------|-------------------|------------------------|
| 影响范围 | 所有可屏蔽中断 | 可配置中断范围 |
| 嵌套安全 | ✅ 有计数 | ✅ 有 |
| 使用场景 | 短临界区代码 | 需要响应中断时 |

### 中断安全 API（FromISR）

```c
// 可以在中断中调用的 API
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

// 中断中发送信号量
xSemaphoreGiveFromISR(myBinarySemaphore, &xHigherPriorityTaskWoken);

// 中断中发送队列
xQueueSendFromISR(myQueue, &data, &xHigherPriorityTaskWoken);

// 强制上下文切换（如有必要）
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

---

## 4.5 任务优先级详解

### 优先级获取与设置

```c
// 获取任务优先级
UBaseType_t uxPriority = uxTaskPriorityGet(myTaskHandle);

// 获取当前任务句柄
TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();

// 设置任务优先级
vTaskPrioritySet(myTaskHandle, 5);

// 设置当前任务优先级
vTaskPrioritySet(NULL, 3);
```

### 空闲任务优先级

```c
// 空闲任务优先级固定为 0（最低）
// 除非 configIDLE_SHOULD_YIELD = 1
```

### 优先级限制

```c
// 用户任务最高优先级
#define configMAX_PRIORITIES    (32)
// 用户可用优先级：0 到 configMAX_PRIORITIES-1
// 空闲任务优先级 = 0（最低）
```

---

## 4.6 面试高频问题

### Q1：FreeRTOS 的调度算法是什么？

**参考答案要点：**
- 抢占式优先级调度 + 时间片轮转
- 最高优先级任务始终运行
- 同优先级任务按时间片轮转执行
- 可配置为协作式调度（关闭抢占）

**扩展回答：**

FreeRTOS采用**两级调度算法**：

**第一级：优先级抢占**
- 系统维护一个就绪队列数组，每个优先级对应一个链表
- 调度器始终选择最高优先级的非空链表中的第一个任务
- 时间复杂度O(1)，通过位图+CLZ指令实现

**第二级：同优先级轮转**
- 当有多个同优先级任务就绪时，采用Round-Robin策略
- 每个任务执行一个时间片（configTICK_RATE_HZ倒数）
- 通过链表遍历器pxIndex实现O(n)轮转

```c
// 调度算法核心
#define taskSELECT_HIGHEST_PRIORITY_TASK() \
    do { \
        UBaseType_t uxTopPriority; \
        portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities); \
        listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, \
            &(xReadyTasks.xReadyTasks[uxTopPriority])); \
    } while(0)
```

---

### Q2：什么是优先级反转？如何解决？

**参考答案：**
```
场景：
- 任务H（高优先级）：需要使用共享资源
- 任务M（中优先级）：运行中
- 任务L（低优先级）：持有共享资源的互斥量

问题：任务H等待任务L，任务M抢占CPU，任务H无法运行

FreeRTOS 解决方案：优先级继承
- 任务L的优先级临时提升到任务H
- 任务M无法抢占，任务L快速释放互斥量
- 任务H得以运行
```

**深度分析：**

优先级反转是实时系统中的经典问题。假设场景：

```
时间轴：

T1: TaskL 获取互斥量M，开始运行
T2: TaskH 就绪（需要互斥量M，被阻塞）
T3: TaskM 就绪，抢占TaskL开始运行  ← 问题！TaskH在等TaskL，但TaskM插队
T4: TaskL 释放互斥量M
T5: TaskH 获得互斥量，开始运行

实际执行顺序：TaskL → TaskM → TaskH
高优先级任务H反而最后执行！
```

**解决方案：优先级继承（Priority Inheritance）**

```c
// FreeRTOS互斥量数据结构
typedef struct QueueDefinition {
    List_t xTasksWaitingToReceive;    // 等待获取的任务列表
    List_t xTasksWaitingToSend;       // 等待释放的任务列表

    TaskHandle_t xMutexHolder;        // 当前持有互斥量的任务
    UBaseType_t uxPriority;           // 互斥量优先级（天花板）
    // ...
} Queue_t;

// 当高优先级任务等待互斥量时：
// 1. 互斥量持有者的优先级提升到与等待者相同
// 2. TaskL的优先级从2提升到7（与TaskH相同）
// 3. TaskM无法抢占TaskL
// 4. TaskL快速释放互斥量
// 5. TaskL优先级恢复
// 6. TaskH获得互斥量，运行
```

**FreeRTOS天花板协议（Ceiling Protocol）：**
- 任务获取互斥量时，优先级提升到该互斥量的天花板优先级
- 确保任务在持有互斥量期间不会被任何任务抢占

---

### Q3：上下文切换时保存哪些内容？

**参考答案：**
- 自动保存（R0-R3, R12, LR, xPSR, PC）：硬件压栈
- 手动保存（R4-R11）：软件压栈
- PSP（进程栈指针）切换
- 任务控制块（TCB）中记录栈顶位置

**深度分析：**

ARM Cortex-M的上下文保存分为两部分：

**1. 硬件自动压栈（异常进入时自动执行）：**

```
异常发生时，ARM Cortex-M处理器自动将以下寄存器
压入当前栈（使用PSP，即进程栈指针）：

PUSH {R0-R3, R12, LR, PC, xPSR}

栈帧布局：
SP (Post-Decrement) → [xPSR] → [PC] → [LR] → [R12] → [R3] → [R2] → [R1] → [R0]

注意：此时SP已经是PSP，不是MSP
```

**2. 软件手动压栈（PendSV处理函数中执行）：**

```c
// portSAVE_CONTEXT 伪代码
mrs r0, psp              // R0 = PSP
stmdb r0!, {r4-r11}      // 保存R4-R11
str r0, [r1]             // 更新TCB中的栈顶指针
```

**为什么这样设计？**

| 寄存器 | 保存方式 | 原因 |
|--------|---------|------|
| R0-R3, R12, LR, PC, xPSR | 硬件自动 | ARM Cortex-M规定，异常入口自动保存 |
| R4-R11 | 软件手动 | 这些是"被调用者保存"寄存器，编译器会保存 |

**实际保存的寄存器列表：**

```c
// 完整上下文 = 8个硬件寄存器 + 8个软件寄存器 = 16个寄存器
// 加上primask等状态，共17个值需要保存

// 栈使用量计算：
// 8个32位寄存器 × 4字节 = 32字节（硬件）
// 8个32位寄存器 × 4字节 = 32字节（软件）
// 1个primask × 4字节 = 4字节
// 总计：68字节 的栈空间用于上下文保存
```

---

### Q4：FreeRTOS 如何实现任务切换？

**参考答案要点：**
1. 触发 PendSV 异常（通过 `portYIELD()`）
2. PendSV 中断中调用 `vTaskSwitchContext()`
3. 从就绪队列中选择最高优先级任务
4. 更新 `pxCurrentTCB` 指向新任务
5. 恢复新任务的栈指针（PSP）
6. 退出中断，执行新任务

**完整源码分析：**

**步骤1：触发PendSV**
```c
// portYIELD 宏定义
#define portYIELD() \
    do { \
        *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET; \
        __dsb(CS_SY); \
        __isb(CS_SY); \
    } while(0)

// 这会设置PendSV挂起位，当CPU可以响应时跳转到PendSV_Handler
```

**步骤2：PendSV_Handler执行**
```c
// 伪代码，实际是汇编
PendSV_Handler:
    // 1. 保存上下文
    portSAVE_CONTEXT()

    // 2. 调用调度器选择下一个任务
    bl vTaskSwitchContext

    // 3. 恢复新任务上下文
    portRESTORE_CONTEXT()
```

**步骤3：vTaskSwitchContext**
```c
void vTaskSwitchContext(void) {
    // 选择下一个任务，更新pxCurrentTCB
    taskSELECT_HIGHEST_PRIORITY_TASK();

    // 调试跟踪
    traceTASK_SWITCHED_IN();
}
```

**步骤4-6：恢复新任务**
```c
// portRESTORE_CONTEXT 伪代码
portRESTORE_CONTEXT:
    ldr r0, =pxCurrentTCB     // 获取当前TCB
    ldr r0, [r0]
    ldr r0, [r0]              // 获取栈顶 pxTopOfStack
    ldmia r0!, {r4-r11}      // 恢复R4-R11
    msr psp, r0              // 更新PSP
    bx lr                     // 返回，自动恢复R0-R3等
```

**完整时序图：**
```
TaskA 运行中...

TaskA 调用 portYIELD()
    │
    ├─→ 设置 PendSV 挂起位
    │
    ├─→ 当前代码继续执行（或触发中断）
    │
    └─→ PendSV 异常被CPU响应
           │
           ├─→ 硬件自动压栈：R0-R3, R12, LR, PC, xPSR → PSP
           │
           ├─→ 跳转到 PendSV_Handler (使用MSP)
           │
           ├─→ portSAVE_CONTEXT:
           │     ├─ R4-R11 保存到 TaskA 栈
           │     ├─ pxCurrentTCB->pxTopOfStack = 新栈顶
           │     └─ uxCriticalNesting 保存
           │
           ├─→ vTaskSwitchContext():
           │     ├─ taskSELECT_HIGHEST_PRIORITY_TASK()
           │     ├─ uxReadyPriorities CLZ → 最高优先级
           │     └─ pxCurrentTCB = TaskB.TCB
           │
           ├─→ portRESTORE_CONTEXT:
           │     ├─ pxCurrentTCB->pxTopOfStack → R0
           │     ├─ R4-R11 恢复到 TaskB 栈
           │     └─ PSP = TaskB 的栈指针
           │
           └─→ BX LR (异常返回)
                  │
                  ├─→ 自动弹栈：R0-R3, R12, LR, PC, xPSR
                  └─→ 跳转到 TaskB 的下一条指令
```

---

### Q5：临界区的作用是什么？注意事项？

**参考答案：**
- 临界区代码执行期间不允许任何中断打断
- 临界区应尽量短，否则影响系统实时性
- 支持嵌套，退出次数必须与进入次数匹配
- 不可在临界区中调用会阻塞的 API

**深度分析：**

**临界区实现原理：**

```c
// 进入临界区
#define taskENTER_CRITICAL()    portENTER_CRITICAL()

// portENTER_CRITICAL 实现
void vPortEnterCritical(void) {
    portDISABLE_INTERRUPTS();    // 设置BASEPRI屏蔽中断
    uxCriticalNesting++;         // 嵌套计数+1

    if (uxCriticalNesting == 1) {
        // 第一次进入，记录当前任务
        vRecordPrimaryCoreState();
    }
}

// 退出临界区
#define taskEXIT_CRITICAL()     portEXIT_CRITICAL()

// portEXIT_CRITICAL 实现
void vPortExitCritical(void) {
    uxCriticalNesting--;         // 嵌套计数-1

    if (uxCriticalNesting == 0) {
        // 完全退出临界区，恢复中断
        portENABLE_INTERRUPTS();
        vClearPrimaryCoreState();
    }
}
```

**ARM Cortex-M BASEPRI 寄存器：**

```
BASEPRI 寄存器用于屏蔽特定优先级以下的中断：

BASEPRI = 0  → 不屏蔽任何中断
BASEPRI = 5  → 屏蔽优先级5-15的中断（优先级0-4正常响应）
BASEPRI = 15 → 屏蔽所有可屏蔽中断

FreeRTOS 临界区使用 BASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY
这意味着：
- 优先级0-4的中断（硬件中断）仍然可以响应
- 优先级5-15的中断被暂时屏蔽
```

**嵌套安全性：**

```c
// 嵌套示例
taskENTER_CRITICAL();      // uxCriticalNesting = 1, BASEPRI = 5
    // 临界区代码 A
    taskENTER_CRITICAL();  // uxCriticalNesting = 2, BASEPRI不变（已经是5）
        // 临界区代码 B
    taskEXIT_CRITICAL();   // uxCriticalNesting = 1, BASEPRI不变
taskEXIT_CRITICAL();       // uxCriticalNesting = 0, BASEPRI = 0(恢复)
```

**注意事项详解：**

| 注意事项 | 原因 | 正确做法 |
|---------|------|---------|
| 临界区尽量短 | 屏蔽中断影响系统响应 | 只保护共享数据访问，不做耗时操作 |
| 禁止阻塞调用 | 会导致死锁 | 只使用非阻塞的ISR-safe API |
| 嵌套要匹配 | 不匹配会导致死锁 | 成对使用ENTER/EXIT |
| 不可递归进入 | 可能导致死锁 | 设计时避免递归场景 |

**错误示例：**
```c
// 错误：临界区中调用阻塞API
taskENTER_CRITICAL();
    xSemaphoreTake(myMutex, portMAX_DELAY);  // 错误！会永久阻塞
taskEXIT_CRITICAL();

// 正确：临界区只做简单赋值
taskENTER_CRITICAL();
    shared_variable = new_value;
    xReturnFromISR = pdTRUE;  // 只做标志设置
taskEXIT_CRITICAL();
```

---

### Q6：时间片长度是多少？如何配置？

**参考答案：**
- 时间片长度 = 1 / configTICK_RATE_HZ
- configTICK_RATE_HZ = 1000 时，时间片 = 1ms
- 每个 Tick 中断进行一次时间片轮转判断

**深度分析：**

**时间片配置：**

```c
// FreeRTOSConfig.h
#define configTICK_RATE_HZ      (1000)    // Tick频率1000Hz
#define configUSE_TIME_SLICING  (1)       // 开启时间片轮转

// 时间片长度计算：
// 1 tick = 1 / configTICK_RATE_HZ = 1 / 1000 = 1ms
```

**时间片轮转流程：**

```c
// xTaskIncrementTick 中的时间片逻辑
BaseType_t xTaskIncrementTick(TaskTimer_t * const pxTimer) {
    if (uxSchedulerSuspended == pdFALSE) {
        // 增加Tick计数
        const TickType_t xConstTickCount = uxTickCount + 1;
        uxTickCount = xConstTickCount;

        #if configUSE_TIME_SLICING == 1
        {
            // 时间片检查
            if (pxCurrentTCB->xTicks > 0) {
                pxCurrentTCB->xTicks--;  // 时间片减1
            } else {
                // 时间片耗尽，切换到同优先级下一个任务
                listINSERT_END(
                    &xReadyTasks.xReadyTasks[pxCurrentTCB->uxPriority],
                    &pxCurrentTCB->xStateListItem
                );
                taskSELECT_HIGHEST_PRIORITY_TASK();
            }
        }
        #endif
    }
}
```

**时间片与Tick的关系：**

```
configTICK_RATE_HZ = 1000 (1ms tick)

时间片配置：
┌─────────────────────────────────────────────────────────┐
│ configUSE_TIME_SLICING = 1 (开启)                       │
│                                                         │
│ 时间片长度 = configTICK_RATE_HZ / configPRIORITY_SWITCH │
│                                                         │
│ 如果 configPRIORITY_SWITCH 未定义，默认 = configTICK_RATE_HZ│
│ 即每个Tick进行一次轮转检查                              │
└─────────────────────────────────────────────────────────┘

注意：FreeRTOS的时间片轮转发生在Tick中断中
     不是每个任务执行完就立即轮转
     最多延迟一个Tick周期
```

**时间片轮转示意：**

```
假设2个同优先级任务 TaskA, TaskB，优先级4

Tick1: TaskA 开始运行，xTicks = 1
       TaskA 运行中...

Tick2: xTaskIncrementTick 执行
       检查 TaskA.xTicks > 0 → 减1后等于0
       时间片耗尽！
       TaskA移到链表尾部 → [TaskB] → [TaskA]
       TaskB成为下一个执行的任务

Tick2: TaskB 开始运行，xTicks = 1
       TaskB 运行中...

Tick3: TaskB.xTicks 耗尽
       TaskB移到尾部 → [TaskA] → [TaskB]
       TaskA再次运行

这样实现了同优先级任务的Round-Robin轮转
```

---

### Q7：为什么FreeRTOS的调度器是O(1)复杂度？

**参考答案：**

FreeRTOS的调度器设计确保了**最坏情况执行时间是常数**，不受任务数量影响。

**O(1)调度的三个关键操作：**

| 操作 | 传统方法 | FreeRTOS方法 | 复杂度 |
|------|---------|-------------|--------|
| 任务入队 | 链表搜索插入位置 | 优先级数组直接索引 + 链表尾插 | O(1) |
| 任务出队 | 链表搜索删除位置 | 链表移除 + 位图检查 | O(1) |
| 查找最高优先级 | 遍历所有优先级 | 位图 + CLZ指令 | O(1) |

**详细分析：**

```c
// 1. 任务入队 - O(1)
void prvAddTaskToReadyQueue(TCB_t *pxTCB) {
    // 直接索引到对应优先级链表 - O(1)
    listINSERT_END(&xReadyTasks.xReadyTasks[pxTCB->uxPriority],
                   &pxTCB->xStateListItem);
    // 位图置位 - O(1)
    uxReadyPriorities |= (1UL << pxTCB->uxPriority);
}

// 2. 任务出队 - O(1)
void prvRemoveTaskFromReadyQueue(TCB_t *pxTCB) {
    listREMOVE_ITEM(&pxTCB->xStateListItem);  // O(1)双向链表移除
    // 检查位图是否需要清除 - O(1)
    if (listLIST_IS_EMPTY(&xReadyTasks.xReadyTasks[pxTCB->uxPriority])) {
        uxReadyPriorities &= ~(1UL << pxTCB->uxPriority);
    }
}

// 3. 查找最高优先级 - O(1)
// ARM Cortex-M CLZ指令是单条汇编指令
#define portGET_HIGHEST_PRIORITY(uxTop, uxReadyPriorities) \
    uxTop = uxReadyPriorities; \
    if( uxTop > uxTopReadyPriority ) { \
        uxTopReadyPriority = uxTop; \
    }

// 假设 uxReadyPriorities = 0b00000000000000000000000010000000
//                                    ↑ bit7 = 1, 优先级7有任务就绪
// CLZ = 24 (前导零数量)
// 最高优先级 = 31 - 24 = 7 ✓
```

**为什么这很重要？**

```
实时系统要求：最坏情况响应时间可预测

假设系统有1000个任务：

传统O(n)调度：
- 最坏情况：遍历1000个任务 → 时间不可预测
- 响应时间：不确定，取决于任务数量

FreeRTOS O(1)调度：
- 最坏情况：固定的位图操作 + CLZ指令
- 响应时间：确定，通常是几十个CPU周期

对于硬实时系统（如汽车安全气囊控制系统）：
必须保证中断响应和任务切换时间是确定性的
FreeRTOS的O(1)调度是这种场景的关键设计
```

---

### Q8：portYIELD 和 portYIELD_FROM_ISR 的区别？

**参考答案：**

| 特性 | portYIELD | portYIELD_FROM_ISR |
|------|-----------|-------------------|
| 调用位置 | 任务代码 | 中断处理函数 |
| 是否带参数 | 无 | 带 BaseType_t 参数 |
| 作用 | 触发PendSV | 条件触发PendSV |

**源码分析：**

```c
// 任务中调用 - 无条件触发PendSV
#define portYIELD() \
    do { \
        *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET; \
        __dsb(CS_SY); \
        __isb(CS_SY); \
    } while(0)

// 中断中调用 - 条件触发PendSV
#define portYIELD_FROM_ISR(x) \
    do { \
        if (x != pdFALSE) { \
            *(portNVIC_INT_CTRL) = portNVIC_PENDSVSET; \
        } \
    } while(0)

// 使用示例
// 任务中
void myTaskFunction(void) {
    // ... 代码 ...
    portYIELD();  // 立即触发任务切换
}

// 中断中
void myISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(myQueue, &data, &xHigherPriorityTaskWoken);

    // 只有当有更高优先级任务就绪时才触发切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

**为什么中断中要条件触发？**

```c
// 原因：避免不必要的上下文切换

// 场景1：中断只是普通数据处理，不需要任务切换
void UART_ISR(void) {
    char c = UART->DR;  // 读取数据
    buffer[bufp++] = c;
    // 没有更高优先级任务就绪，不需要切换
    // 如果每次都触发PendSV，会浪费CPU时间
}

// 场景2：中断释放了信号量，唤醒了高优先级任务
void SemaphoreISR(void) {
    xSemaphoreGiveFromISR(mySemaphore, &xHigherPriorityTaskWoken);
    // 必须切换，让高优先级任务立即运行
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // 触发PendSV
}
```

---

### Q9：空闲任务的作用是什么？可以删除吗？

**参考答案：**

**空闲任务的作用：**

1. **保证总是有任务可运行**：当所有用户任务都在等待时，运行空闲任务
2. **回收已删除任务的内存**：空闲任务调用vTaskDelete清理
3. **执行空闲钩子函数**：用户可以在空闲时做低优先级事情
4. **实现优先级天花板协议**：当启用configUSE_MUTEXES时

```c
// 空闲任务主函数
void prvIdleTask(void *pvParameters) {
    for (;;) {
        // 检查是否有任务需要清理
        // ...
        #if configUSE_IDLE_HOOK == 1
            vApplicationIdleHook();  // 用户自定义的空闲处理
        #endif

        // 如果配置了空闲任务让步，同优先级用户任务可以运行
        #if configIDLE_SHOULD_YIELD == 1
            taskYIELD();
        #endif
    }
}
```

**为什么不能删除空闲任务？**

```
1. 系统必须始终有一个任务在运行（CPU不能空闲）
2. 空闲任务负责清理已删除任务的资源
3. 如果删除了空闲任务，删除任务时就会死锁
```

**配置选项：**

```c
// FreeRTOSConfig.h
#define configUSE_IDLE_HOOK      0    // 是否启用空闲钩子
#define configIDLE_SHOULD_YIELD  1    // 空闲任务是否让步于同优先级用户任务
```

---

### Q10：FreeRTOS如何实现任务优先级翻转（Priority Inversion）？

**参考答案：**

Priority Inversion（优先级反转）和Priority Inheritance（优先级继承）的区别：

**Priority Inversion（问题现象）：**
```
高优先级任务等待低优先级任务持有的资源
中优先级任务抢占低优先级任务
导致高优先级任务反而无法运行

这是问题本身
```

**Priority Inheritance（解决方案）：**
```
当高优先级任务等待低优先级任务持有的资源时
临时提升低优先级任务的优先级
使其尽快完成并释放资源

这是FreeRTOS提供的解决方案
```

**FreeRTOS的互斥量实现：**

```c
// 互斥量获取时的优先级继承
BaseType_t xQueueTakeMutexRecursive(Queue_t *pxMutex, TickType_t xTicksToWait) {
    // ...

    if (pvTaskGetMutexHolder() == xTaskGetCurrentTaskHandle()) {
        // 如果是递归获取（同一任务），增加计数
        pxMutex->uxMessages++;
    } else {
        // 非递归获取
        // 检查互斥量是否可用
        // 如果不可用且需要等待...

        // 关键：实现优先级继承
        TaskHandle_t xMutexHolder = pxMutex->xMutexHolder;
        if (xMutexHolder != NULL) {
            TaskType_t xHeldPriority = pxTCB->uxPriority;
            TaskType_t xDesiredPriority = pxTCB->uxPriority;

            if (xHeldPriority < xDesiredPriority) {
                // 提升互斥量持有者的优先级
                vTaskPrioritySet(xMutexHolder, xDesiredPriority);
            }
        }
    }
}

// 互斥量释放时的优先级恢复
void vQueueReleaseMutex(TaskHandle_t xMutex) {
    // ...

    // 恢复原始优先级
    vTaskPrioritySet(pxMutex->xMutexHolder, pxMutex->uxPriority);
}
```

**优先级继承的时序图：**

```
不使用优先级继承：
TaskH (P7) ──────────────[等待]───────────────────[运行]
TaskM (P4) ─────[抢占]──────────────>
TaskL (P2) ─[运行]──[被抢占]──[继续运行]──[完成]──>
          T1      T2           T3        T4

TaskH等待了 T1→T4 这么长时间！


使用优先级继承：
TaskL (P2→P7) ─[运行(优先级提升)]──[完成]──[优先级恢复]─>
TaskH (P7)    ─────────────────────[运行]──>
TaskM (P4)    ────[无法抢占TaskL]──────────>

TaskH只等待了 T1'→T2'，大幅缩短！
```

---

## 4.7 避坑指南

1. **临界区时间不能过长** — 会影响系统响应和中断延迟
2. **不要在临界区中调用阻塞 API** — 会导致系统死锁
3. **PendSV 优先级要设置正确** — 嵌入式移植时需配置 NVIC
4. **高优先级任务不能一直占用 CPU** — 低优先级任务会饿死
5. **时间片轮转只在同优先级生效** — 不同优先级是高优先级直接抢占
6. **协作式调度下任务必须主动让出 CPU** — 否则同优先级任务无法切换

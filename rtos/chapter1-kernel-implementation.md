# 第1章：任务调度器深度解析

本章深入分析FreeRTOS内核的核心机制——任务调度器的实现原理。通过源码分析，揭示抢占式调度器如何选择下一个运行任务、任务切换的具体过程，以及Tick中断和空闲任务的作用。

## 1.1 任务调度器核心问题

### 1.1.1 FreeRTOS抢占式调度器如何选择下一个运行任务？

#### 原理阐述

FreeRTOS采用**基于优先级的抢占式调度器**（Priority-based Preemptive Scheduler）。调度器的核心目标是：在任何时刻，总是选择**最高优先级**的就绪任务来运行。

调度器的选择过程遵循以下原则：

1. **最高优先级优先**：总是选择优先级最高的就绪任务
2. **同优先级时间片轮转**：如果多个任务具有相同优先级，它们会轮流执行（当启用时间片时）
3. **抢占**：高优先级任务就绪时，会立即抢占低优先级任务的执行

#### 源码分析

在FreeRTOS中，任务选择通过`prvSelectHighestPriorityTask`函数实现。该函数定义在`tasks.c:983`行。

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:983-1100
static void prvSelectHighestPriorityTask( BaseType_t xCoreID )
{
    UBaseType_t uxCurrentPriority = uxTopReadyPriority;
    BaseType_t xTaskScheduled = pdFALSE;
    TCB_t * pxTCB = NULL;

    // 如果当前任务仍在就绪列表中，先将其移到链表末尾
    // 这是为了解决同优先级任务的时间片轮转问题
    if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxCurrentTCBs[ xCoreID ]->uxPriority ] ),
                                 &pxCurrentTCBs[ xCoreID ]->xStateListItem ) == pdTRUE )
    {
        ( void ) uxListRemove( &pxCurrentTCBs[ xCoreID ]->xStateListItem );
        vListInsertEnd( &( pxReadyTasksLists[ pxCurrentTCBs[ xCoreID ]->uxPriority ] ),
                        &pxCurrentTCBs[ xCoreID ]->xStateListItem );
    }

    // 从最高优先级开始向下搜索，找到第一个非空的就绪链表
    while( xTaskScheduled == pdFALSE )
    {
        if( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxCurrentPriority ] ) ) == pdFALSE )
        {
            // 找到非空链表，获取头部的任务
            const List_t * const pxReadyList = &( pxReadyTasksLists[ uxCurrentPriority ] );
            const ListItem_t * pxEndMarker = listGET_END_MARKER( pxReadyList );
            ListItem_t * pxIterator;

            for( pxIterator = listGET_HEAD_ENTRY( pxReadyList );
                 pxIterator != pxEndMarker;
                 pxIterator = listGET_NEXT( pxIterator ) )
            {
                pxTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxIterator );
                // ... 省略SMP相关的亲和性检查 ...
                pxCurrentTCBs[ xCoreID ] = pxTCB;
                xTaskScheduled = pdTRUE;
                break;
            }
        }
        uxCurrentPriority--;
    }
}
```

关键点解析：

1. **`uxTopReadyPriority`优化**：使用一个全局变量记录当前最高就绪优先级，避免每次都从最高优先级向下遍历。在任务进入就绪状态时更新（见`taskRECORD_READY_PRIORITY`宏）。

2. **就绪链表遍历**：使用`listGET_OWNER_OF_NEXT_ENTRY`宏遍历链表，实现FIFO顺序选择任务。

3. **时间片轮转实现**：通过将当前运行任务移到链表末尾，确保同优先级的其他任务有机会运行。

任务选择入口通过宏`taskSELECT_HIGHEST_PRIORITY_TASK()`调用：

```c
// tasks.c:196
#define taskSELECT_HIGHEST_PRIORITY_TASK( xCoreID )    prvSelectHighestPriorityTask( xCoreID )
```

该宏在`vTaskSwitchContext`函数中被调用（`tasks.c:5114`行），这是任务切换的核心入口。

#### 面试参考答案

> **问题：FreeRTOS如何选择下一个运行的任务？**
>
> **回答：**
> FreeRTOS采用基于优先级的抢占式调度，选择下一个任务的核心流程如下：
>
> 1. **就绪链表结构**：FreeRTOS为每个优先级维护一个双向链表（`pxReadyTasksLists[priority]`），优先级0~configMAX_PRIORITIES-1。使用`uxTopReadyPriority`变量记录当前最高就绪优先级，避免全表扫描。
>
> 2. **选择算法**：在`prvSelectHighestPriorityTask()`中，从`uxTopReadyPriority`开始向下搜索，找到第一个非空链表，然后取链表头部的任务（最早进入就绪状态的任务）。
>
> 3. **时间片轮转**：当任务被选中运行时，如果它原本就在就绪链表中，会先将其移动到链表末尾。这样同优先级的任务会轮询执行。
>
> 4. **抢占触发**：高优先级任务就绪时（如调用`vTaskDelayUntil()`使当前任务阻塞），会立即触发`portYIELD()`或设置PendSV标志，实现抢占。
>
> **关键设计**：使用O(1)复杂度的优先级选择（通过`uxTopReadyPriority`），避免遍历所有优先级链表，保证调度效率。

---

### 1.1.2 任务就绪链表是如何组织的？

#### 原理阐述

FreeRTOS使用**按优先级分组的双向链表**来组织就绪任务。每个优先级对应一个链表，同一优先级的任务按FIFO顺序排列。

#### 源码分析

就绪链表的定义和初始化：

```c
// tasks.c:459
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ];
/**< Prioritised ready tasks. */
```

就绪链表是一个双向链表结构，每个任务控制块（TCB）包含两个链表项：

```c
// 任务控制块(TCB)中的链表成员
typedef struct tskTaskControlBlock
{
    // ... 其他成员 ...
    ListItem_t xStateListItem;      /*< The list item is used to place the TCB in a linked list. */
    ListItem_t xEventListItem;      /*< Used to place the TCB in an event list. */
    // ... 其他成员 ...
} tskTaskControlBlock;
```

任务加入就绪链表通过宏`prvAddTaskToReadyList`实现（`tasks.c:268-274`）：

```c
// tasks.c:268-274
#define prvAddTaskToReadyList( pxTCB )                                                                     \
    do {                                                                                                   \
        traceMOVED_TASK_TO_READY_STATE( pxTCB );                                                           \
        taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority );                                                \
        listINSERT_END( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \
        tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB );                                                      \
    } while( 0 )
```

关键点：

1. **按优先级插入**：根据任务的优先级`uxPriority`插入对应的链表
2. **链表末尾插入**：使用`listINSERT_END`将任务插入链表末尾，实现FIFO
3. **更新最高优先级**：调用`taskRECORD_READY_PRIORITY`更新`uxTopReadyPriority`

```c
// tasks.c:183-186
#define taskRECORD_READY_PRIORITY( uxPriority )            \
    if( ( uxPriority ) > uxTopReadyPriority )             \
    {                                                      \
        uxTopReadyPriority = ( uxPriority );               \
    }
```

#### 面试参考答案

> **问题：FreeRTOS的就绪链表是如何组织的？**
>
> **回答：**
> FreeRTOS使用**优先级数组+双向链表**的组织方式：
>
> 1. **数组结构**：`pxReadyTasksLists[configMAX_PRIORITIES]`是一个链表数组，每个索引对应一个优先级。
>
> 2. **双向链表**：每个优先级对应一个双向链表，任务按FIFO顺序排列。使用TCB中的`xStateListItem`作为链表节点。
>
> 3. **插入操作**：新任务或被唤醒的任务使用`listINSERT_END`插入链表末尾，保证同优先级任务的时间片轮转。
>
> 4. **优先级更新**：使用`uxTopReadyPriority`变量记录当前最高就绪优先级，在任务加入就绪链表时更新，使调度器可以用O(1)复杂度找到最高优先级任务。
>
> 5. **状态管理**：任务的`xStateListItem`同时用于追踪任务状态（在就绪链表、阻塞链表、挂起链表之间移动）。

---

### 1.1.3 configUSE_TIME_SLICING 如何影响调度？

#### 原理阐述

`configUSE_TIME_SLICING`是一个配置选项，控制同优先级任务的时间片轮转行为。

- **启用（默认=1）**：同优先级的多个就绪任务会轮流执行，每个任务运行一个Tick周期
- **禁用（=0）**：同优先级的任务不会自动切换，除非有更高优先级任务抢占

#### 源码分析

时间片检查在`xTaskIncrementTick`函数中实现（`tasks.c:4810-4841`）：

```c
// tasks.c:4807-4841
/* Tasks of equal priority to the currently running task will share
 * processing time (time slice) if preemption is on, and the application
 * writer has not explicitly turned time slicing off. */
#if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
{
    #if ( configNUMBER_OF_CORES == 1 )
    {
        // 检查当前优先级就绪链表是否有多个任务
        if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > 1U )
        {
            xSwitchRequired = pdTRUE;  // 需要触发任务切换
        }
    }
    #else /* SMP多核情况 */
    {
        // 多核下检查每个核心的当前优先级就绪链表
        for( xCoreID = 0; xCoreID < ( ( BaseType_t ) configNUMBER_OF_CORES ); xCoreID++ )
        {
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCBs[ xCoreID ]->uxPriority ] ) ) > 1U )
            {
                xYieldRequiredForCore[ xCoreID ] = pdTRUE;
            }
        }
    }
    #endif
}
#endif
```

时间片逻辑解析：

1. **条件检查**：只有当`configUSE_PREEMPTION=1`且`configUSE_TIME_SLICING=1`时才启用时间片
2. **检查同优先级就绪任务数**：通过`listCURRENT_LIST_LENGTH`检查当前优先级链表是否有多于1个任务
3. **设置切换标志**：如果有多于1个任务，设置`xSwitchRequired = pdTRUE`，在Tick中断处理完后触发PendSV进行任务切换

#### 面试参考答案

> **问题：configUSE_TIME_SLICING配置项对调度有什么影响？**
>
> **回答：**
> `configUSE_TIME_SLICING`控制同优先级任务的轮转时间片：
>
> - **启用（默认=1）**：在Tick中断中，如果发现当前优先级有多个就绪任务，会触发PendSV进行任务切换，实现时间片轮转。每个任务运行一个Tick周期后让出CPU。
>
> - **禁用（=0）**：即使同优先级有多个就绪任务，也不会在Tick时自动切换。只有当有更高优先级任务就绪时才会发生调度。
>
> **典型场景**：
> - 如果需要"合作式"同优先级调度，可设置`configUSE_TIME_SLICING=0`
> - 如果希望同优先级任务公平分享CPU时间，保留默认值1
>
> **相关配置**：时间片轮转还受`configUSE_PREEMPTION`和`configIDLE_SHOULD_YIELD`影响。

---

## 1.2 任务切换机制问题

### 1.2.1 任务切换时具体保存了哪些上下文？

#### 原理阐述

任务切换（Context Switch）需要保存当前任务的执行上下文，并恢复新任务的上下文。在ARM Cortex-M架构中，上下文包括：

- **通用寄存器**：R0-R12
- **程序状态寄存器**：xPSR（但通常不需要显式保存）
- **栈指针**：PSP（任务栈）
- **链接寄存器**：LR（返回地址）
- **可选：FPU寄存器**：如果任务使用了浮点运算

#### 源码分析

任务切换通过PendSV中断处理程序实现，定义在`portable/GCC/ARM_CM7/r0p1/port.c:493-549`。

**保存当前任务上下文**（进入中断时硬件自动保存部分）：

```c
// portable/GCC/ARM_CM7/r0p1/port.c:493-520
void xPortPendSVHandler( void )
{
    __asm volatile
    (
        "   mrs r0, psp                         \n"  // 获取当前任务的PSP
        "   isb                                 \n"  // 指令同步屏障
        "                                       \n"
        "   ldr r3, pxCurrentTCBConst           \n"  // 获取当前TCB指针
        "   ldr r2, [r3]                        \n"
        "                                       \n"
        "   tst r14, #0x10                      \n"  // 检查是否使用FPU
        "   it eq                               \n"
        "   vstmdbeq r0!, {s16-s31}             \n"  // 保存FPU高寄存器
        "                                       \n"
        "   stmdb r0!, {r4-r11, r14}            \n"  // 保存R4-R11和LR
        "   str r0, [r2]                        \n"  // 保存新的栈顶到TCB
        // ...
    );
}
```

关键点：

1. **PSP获取**：使用`mrs r0, psp`获取进程栈指针（任务使用PSP栈）
2. **FPU保存**：检查xPSR中的FPU位，如果使用FPU则保存s16-s31寄存器
3. **通用寄存器保存**：R4-R11需要手动保存（硬件自动保存R0-R3, R12, LR, xPSR, PC）

**恢复新任务上下文**：

```c
// portable/GCC/ARM_CM7/r0p1/port.c:522-548
    // ...
    "   ldr r1, [r3]                        \n"  // 获取新TCB
    "   ldr r0, [r1]                        \n"  // 获取新任务的栈顶
    "                                       \n"
    "   ldmia r0!, {r4-r11, r14}            \n"  // 恢复R4-R11和LR
    "                                       \n"
    "   tst r14, #0x10                      \n"  // 检查新任务是否使用FPU
    "   it eq                               \n"
    "   vldmiaeq r0!, {s16-s31}             \n"  // 恢复FPU高寄存器
    "                                       \n"
    "   msr psp, r0                         \n"  // 更新PSP
    "   isb                                 \n"
    "                                       \n"
    "   bx r14                              \n"  // 返回，通过LR恢复执行
    // ...
```

#### 面试参考答案

> **问题：FreeRTOS任务切换时保存了哪些上下文？**
>
> **回答：**
> 在ARM Cortex-M架构中，任务切换保存的上下文分为两部分：
>
> **硬件自动保存（进入中断时）**：
> - R0-R3, R12, PC, xPSR：硬件自动压栈
> - LR：返回地址
>
> **软件手动保存**：
> - R4-R11：通用寄存器需要在PendSV中手动保存
> - PSP（任务栈指针）：保存到TCB的第一个成员
> - FPU寄存器（s16-s31）：如果任务使用了浮点运算
>
> **栈帧布局**：任务栈中保存的完整栈帧包括：xPSR, PC, LR, R12, R0-R3（硬件），以及R4-R11（软件）。共8个字（32字节）+ 可选的FPU寄存器。
>
> **为什么这样设计**：ARM Cortex-M的中断自动入栈已经保存了大部分寄存器，选择保存R4-R11是因为这是编译器常用的寄存器，保存它们可以保证函数调用的一致性。R4-R11共8个寄存器，是保存/恢复成本和效率的平衡点。

---

### 1.2.2 为什么使用 PendSV 触发任务切换？

#### 原理阐述

PendSV（可挂起的系统调用）是一种异步异常，用于实现任务切换。选择PendSV的原因包括：

1. **延迟执行**：允许将任务切换延迟到更合适的时机
2. **最低优先级**：可以配置为最低优先级，确保其他中断处理完成
3. **同步触发**：可以手动设置触发任务切换

#### 源码分析

**PendSV配置**（port.c:420）：

```c
// portable/GCC/ARM_CM7/r0p1/port.c:420
/* Make PendSV and SysTick the lowest priority interrupts, and make SVCall
 * the highest priority. */
configASSERT( portMAX_SYSCALL_INTERRUPT_PRIORITY <= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
```

**Tick中断中触发PendSV**（port.c:552-576）：

```c
// portable/GCC/ARM_CM7/r0p1/port.c:552-576
void xPortSysTickHandler( void )
{
    portDISABLE_INTERRUPTS();
    traceISR_ENTER();
    {
        /* Increment the RTOS tick. */
        if( xTaskIncrementTick() != pdFALSE )
        {
            traceISR_EXIT_TO_SCHEDULER();

            /* A context switch is required.  Context switching is performed in
             * the PendSV interrupt.  Pend the PendSV interrupt. */
            portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
        }
    }
    portENABLE_INTERRUPTS();
}
```

关键设计：

1. **Tick中断中不直接切换**：Tick中断（SysTick）只设置PendSV标志位，不直接执行任务切换
2. **延迟切换**：PendSV会等待所有其他ISR处理完成后再执行
3. **手动触发**：使用`portNVIC_PENDSVSET_BIT`设置PendSV挂起位

**PendSV触发方式定义**：

```c
// portable/GCC/ARM_CM7/r0p1/portmacro.h
#define portNVIC_PENDSVSET_BIT     ( 1UL << 28UL )
```

#### 面试参考答案

> **问题：为什么FreeRTOS使用PendSV来触发任务切换，而不是在Tick中断中直接切换？**
>
> **回答：**
> FreeRTOS选择PendSV触发任务切换有几个关键原因：
>
> 1. **中断尾链处理**：PendSV设计为最低优先级中断，在Tick中断执行完毕后，所有其他高优先级中断的ISR都已经完成。此时PendSV才会执行，确保了干净的中断退出路径。
>
> 2. **延迟执行**：允许在多个地方（Tick中断、任务调用`taskYIELD`、ISR中唤醒高优先级任务）设置PendSV挂起位，统一在合适时机执行切换。
>
> 3. **中断嵌套考虑**：如果在Tick中断中直接进行任务切换，会中断其他可能正在执行的低优先级ISR。使用PendSV可以避免这种嵌套复杂性。
>
> 4. **可挂起特性**：PendSV的"可挂起"意味着即使被触发，也可以被更高优先级中断打断，保持了系统的实时性。
>
> **典型流程**：
> 1. SysTick中断发生 -> 调用`xTaskIncrementTick()`
> 2. 发现需要切换 -> 设置PendSV挂起位
> 3. SysTick中断返回
> 4. 所有高优先级ISR执行完毕
> 5. PendSV handler执行 -> 完成上下文切换

---

## 1.3 Tick和空闲任务问题

### 1.3.1 Tick中断中发生了什么？

#### 原理阐述

Tick中断（通常使用SysTick定时器）是RTOS的心跳，负责：

1. **时间基准**：维护系统时间（xTickCount）
2. **延时处理**：检查阻塞任务是否到期唤醒
3. **调度触发**：判断是否需要进行任务切换

#### 源码分析

Tick中断处理函数`xPortSysTickHandler`已在上一节展示。核心逻辑在于`xTaskIncrementTick`函数（`tasks.c:4670`）：

```c
// tasks.c:4670-4754
BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;
    TickType_t xItemValue;
    BaseType_t xSwitchRequired = pdFALSE;

    if( uxSchedulerSuspended == ( UBaseType_t ) 0U )
    {
        // 1. 增加系统Tick计数
        const TickType_t xConstTickCount = xTickCount + ( TickType_t ) 1;
        xTickCount = xConstTickCount;

        // 处理Tick溢出，切换延迟链表
        if( xConstTickCount == ( TickType_t ) 0U )
        {
            taskSWITCH_DELAYED_LISTS();
        }

        // 2. 检查是否有阻塞任务到期需要唤醒
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            for( ; ; )
            {
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
                {
                    xNextTaskUnblockTime = portMAX_DELAY;
                    break;
                }
                else
                {
                    pxTCB = listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    if( xConstTickCount < xItemValue )
                    {
                        // 未到期的任务，记录下一个解锁时间并退出
                        xNextTaskUnblockTime = xItemValue;
                        break;
                    }
                    else
                    {
                        // 任务到期，从延迟链表移除，加入就绪链表
                        ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                        prvAddTaskToReadyList( pxTCB );

                        #if ( configUSE_MUTEXES == 1 )
                        {
                            // 处理优先级继承
                            if( pxTCB->uxBasePriority > pxTCB->uxMutexesHeld )
                            {
                                // ... 省略优先级继承处理 ...
                            }
                        }
                        #endif
                    }
                }
            }
        }

        // 3. 检查是否需要时间片轮转（同优先级有多个任务）
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
        {
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > 1U )
            {
                xSwitchRequired = pdTRUE;
            }
        }
        #endif
    }
    // ... 省略调度器挂起时的处理 ...

    return xSwitchRequired;
}
```

Tick中断的三个核心功能：

1. **Tick计数更新**：每次中断使`xTickCount`增加1，处理溢出情况
2. **阻塞任务唤醒**：检查延迟链表头部的任务是否到期，到期则加入就绪链表
3. **时间片检查**：如果同优先级有多个任务，设置切换标志

#### 面试参考答案

> **问题：FreeRTOS的Tick中断中发生了什么？**
>
> **回答：**
> Tick中断（SysTick）是FreeRTOS的时间基准，每次中断执行以下操作：
>
> 1. **Tick计数递增**：`xTickCount++`，记录系统运行时间
> 2. **延迟任务检查**：遍历延迟链表（按唤醒时间排序），将到期（xTickCount >= 唤醒时间）的任务从延迟链表移除，加入就绪链表
> 3. **更新下一个解锁时间**：记录下一个即将解锁的任务时间，避免无必要遍历
> 4. **时间片轮转检查**：如果启用时间片且同优先级有多个任务，设置切换标志
> 5. **返回切换标志**：返回`pdTRUE`表示需要任务切换，Tick handler会设置PendSV
>
> **注意**：如果调度器被挂起（`uxSchedulerSuspended > 0`），Tick递增会被延迟到调度器恢复时（通过xTaskResumeAll）。

---

### 1.3.2 空闲任务的作用是什么？

#### 原理阐述

空闲任务（Idle Task）是FreeRTOS自动创建的低优先级任务，当所有其他任务都处于阻塞状态时，空闲任务会运行。空闲任务的作用包括：

1. **CPU回收**：确保CPU始终有任务可执行
2. **资源回收**：清理已删除任务的TCB和栈内存
3. **低功耗支持**：在无任务可运行时进入低功耗模式

#### 源码分析

空闲任务在`prvIdleTask`函数中实现（`tasks.c:5748-5840`）：

```c
// tasks.c:5748-5814
static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    ( void ) pvParameters;

    /** THIS IS THE RTOS IDLE TASK - WHICH IS CREATED AUTOMATICALLY WHEN THE
     * SCHEDULER IS STARTED. **/

    portALLOCATE_SECURE_CONTEXT( configMINIMAL_SECURE_STACK_SIZE );

    for( ; configCONTROL_INFINITE_LOOP(); )
    {
        // 1. 检查并清理已删除任务
        prvCheckTasksWaitingTermination();

        #if ( configUSE_PREEMPTION == 0 )
        {
            // 非抢占模式下主动让出CPU
            taskYIELD();
        }
        #endif

        // 2. 如果有其他空闲优先级任务就绪，主动让出CPU
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
        {
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) >
                ( UBaseType_t ) configNUMBER_OF_CORES )
            {
                taskYIELD();
            }
        }
        #endif

        // 3. 调用用户空闲钩子函数
        #if ( configUSE_IDLE_HOOK == 1 )
        {
            vApplicationIdleHook();
        }
        #endif

        // 4. 低功耗支持（tickless idle模式）
        #if ( configUSE_TICKLESS_IDLE != 0 )
        {
            // ... 省略tickless idle处理 ...
        }
        #endif
    }
}
```

空闲任务的四个核心功能：

1. **`prvCheckTasksWaitingTermination`**：检查`xTasksWaitingTermination`链表，释放被删除任务的内存
2. **主动让出CPU**：在非抢占模式或配置了`configIDLE_SHOULD_YIELD`时让出CPU
3. **用户钩子**：调用`vApplicationIdleHook()`，用户可在其中进入低功耗
4. **低功耗模式**：支持tickless idle，关闭Tick定时器省电

#### 面试参考答案

> **问题：FreeRTOS的空闲任务有什么作用？**
>
> **回答：**
> 空闲任务（Idle Task）是FreeRTOS调度器启动时自动创建的最低优先级任务（优先级0），主要作用：
>
> 1. **内存回收**：当任务调用`vTaskDelete()`删除自己时，任务的TCB和栈内存不能立即释放（因为任务仍在运行）。这些资源被放入`xTasksWaitingTermination`链表，由空闲任务在下次运行时释放（`prvCheckTasksWaitingTermination`）。
>
> 2. **CPU回收**：确保CPU永远不会空闲到无所事事。即使所有用户任务都阻塞，空闲任务也能运行。
>
> 3. **主动让出**：
>    - `configUSE_PREEMPTION=0`时：每次循环调用`taskYIELD()`尝试调度其他任务
>    - `configIDLE_SHOULD_YIELD=1`时：如果有其他空闲优先级任务就绪，主动让出CPU
>
> 4. **用户钩子**：`configUSE_IDLE_HOOK=1`时，用户可实现`vApplicationIdleHook()`，常用于：
>    - 进入低功耗模式
>    - 系统监控
>    - 后台任务处理
>
> 5. **Tickless Idle支持**：`configUSE_TICKLESS_IDLE=1`时，空闲任务可以调用`portSUPPRESS_TICKS_AND_SLEEP()`关闭SysTick进入深度休眠，省电。
>
> **面试追问**：如果空闲任务长时间占用CPU而其他任务无法运行，检查是否：
> - 高优先级任务处于死循环或阻塞
> - 禁止了中断
> - 配置了`configIDLE_SHOULD_YIELD=0`

---

## 1.4 本章小结

本章深入分析了FreeRTOS任务调度器的核心实现：

| 主题 | 关键点 |
|------|--------|
| **任务选择** | 基于优先级的O(1)算法，使用`uxTopReadyPriority`优化，使用双向链表组织就绪任务 |
| **时间片** | `configUSE_TIME_SLICING`控制同优先级轮转，在Tick中断中检查并触发切换 |
| **上下文切换** | PendSV handler保存R4-R11、PSP、FPU寄存器，硬件自动保存R0-R3,R12,PC,xPSR |
| **PendSV优势** | 最低优先级、延迟执行、统一切换入口、确保ISR完成 |
| **Tick中断** | 递增xTickCount、唤醒阻塞任务、检查时间片、触发PendSV |
| **空闲任务** | 内存回收、CPU兜底、用户钩子、低功耗支持 |

理解这些核心机制对于掌握FreeRTOS的调度原理、排查任务调度相关问题至关重要，也是高级工程师面试中经常考察的深度知识点。

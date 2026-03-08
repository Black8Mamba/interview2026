# 第4章：中断与实时性

本章深入分析FreeRTOS中断管理机制、临界区对系统响应的影响、FromISR API的设计意图，以及调度延迟的测量与优化方法。这些内容是构建高可靠性实时系统的关键技术基础。

## 4.1 中断管理问题

### 4.1.1 临界区对系统响应的影响

#### 原理阐述

临界区（Critical Section）是操作系统中用于保护共享资源的特殊代码区域。在FreeRTOS中，临界区通过禁用中断来实现对内核数据结构的原子操作，防止任务切换和数据竞争。然而，这种保护机制会带来**系统响应延迟**的副作用。

临界区对系统响应的影响主要体现在以下几个方面：

1. **中断响应延迟**：进入临界区时，调度器会禁用或限制中断（取决于具体实现），导致中断无法立即响应
2. **任务调度延迟**：临界区期间无法进行任务切换，高优先级任务可能无法及时获得CPU使用权
3. **中断嵌套受限**：在某些实现中，临界区会限制中断嵌套深度

FreeRTOS提供了两级临界区保护机制：

- **基础临界区**（`taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()`）：完全禁用中断
- **中断安全临界区**（`taskENTER_CRITICAL_FROM_ISR()`/`taskEXIT_CRITICAL_FROM_ISR()`）：在中断中使用，限制系统调用优先级以上的中断

#### 源码分析

FreeRTOS的临界区实现在ARM Cortex-M架构中主要依赖BASEPRI寄存器。关键实现位于`portmacro.h`：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/portmacro.h:210-224
portFORCE_INLINE static void vPortRaiseBASEPRI( void )
{
    uint32_t ulNewBASEPRI;

    __asm volatile
    (
        "   mov %0, %1                                              \n" \
        "   cpsid i                                                 \n" \
        "   msr basepri, %0                                         \n" \
        "   isb                                                     \n" \
        "   dsb                                                     \n" \
        "   cpsie i                                                 \n" \
        : "=r" ( ulNewBASEPRI ) : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) : "memory"
    );
}
```

关键设计说明：

1. **BASEPRI机制**：通过设置BASEPRI寄存器，屏蔽所有优先级低于`configMAX_SYSCALL_INTERRUPT_PRIORITY`的中断。这是比完全禁用中断更优雅的方案，允许高优先级中断（如实时性关键的定时器中断）继续响应。

2. **中断状态保存**：`ulPortRaiseBASEPRI()`函数会返回原始的BASEPRI值，使得退出临界区时能够恢复之前的中断状态：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/portmacro.h:228-247
portFORCE_INLINE static uint32_t ulPortRaiseBASEPRI( void )
{
    uint32_t ulOriginalBASEPRI, ulNewBASEPRI;

    __asm volatile
    (
        "   mrs %0, basepri                                         \n" \
        "   mov %1, %2                                              \n" \
        "   cpsid i                                                 \n" \
        "   msr basepri, %1                                         \n" \
        "   isb                                                     \n" \
        "   dsb                                                     \n" \
        "   cpsie i                                                 \n" \
        : "=r" ( ulOriginalBASEPRI ), "=r" ( ulNewBASEPRI ) : "i" ( configMAX_SYSCALL_INTERRUPT_PRIORITY ) : "memory"
    );

    return ulOriginalBASEPRI;
}
```

在`tasks.c`中，临界区被广泛应用于保护内核数据结构。例如在查询任务状态的函数中：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2507-2514
taskENTER_CRITICAL();
{
    pxStateList = listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
    pxEventList = listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) );
    pxDelayedList = pxDelayedTaskList;
    pxOverflowedDelayedList = pxOverflowDelayedTaskList;
}
taskEXIT_CRITICAL();
```

这段代码保护了对任务链表的读取操作，防止在遍历链表时被中断修改导致数据不一致。

#### 面试参考答案

> **问题：临界区对系统响应有什么影响？**
>
> **回答：**
> 临界区是FreeRTOS中保护共享资源的核心机制，但它会对系统实时响应产生以下影响：
>
> **1. 中断响应延迟**
> - 进入临界区时，通过BASEPRI寄存器屏蔽低于`configMAX_SYSCALL_INTERRUPT_PRIORITY`的中断
> - 配置建议：将`configMAX_SYSCALL_INTERRUPT_PRIORITY`设置为较低值（如5），允许更高优先级的中断随时响应
> - 临界区应尽可能短，只包含必要的原子操作
>
> **2. 调度延迟**
> - 临界区期间无法触发PendSV进行任务切换
> - 这意味着即使高优先级任务就绪，也需要等待临界区退出
> - 典型影响延迟在几微秒到数十微秒之间（取决于CPU频率和临界区代码量）
>
> **3. 工程实践建议**
> - 临界区代码必须**快速执行**，禁止在临界区内调用任何可能导致阻塞的API
> - 使用`configASSERT()`验证中断优先级配置是否正确
> - 优先考虑使用**中断延迟处理模式**（Deferred Interrupt Handling），在ISR中只做最小化处理，将复杂逻辑放到任务中

> **延伸问题：如何减少临界区对响应时间的影响？**
> - 将临界区代码最小化，只保护必要的共享数据访问
> - 使用消息队列或事件标志组等机制，将中断处理延迟到任务上下文中
> - 合理配置`configMAX_SYSCALL_INTERRUPT_PRIORITY`，允许高优先级中断嵌套

---

### 4.1.2 FromISR API 命名规范的意义

#### 原理阐述

FreeRTOS API分为两类：常规API和中断安全API（FromISR）。这种命名规范并非简单的代码约定，而是**安全设计原则的体现**。使用`FromISR`后缀有以下几个重要意义：

1. **强制区分调用上下文**：从语义上提醒开发者，该函数只能在中断服务程序（ISR）或任务中调用
2. **防止误用导致系统崩溃**：在中断中调用非FromISR函数可能导致不可预知的行为
3. **中断优先级约束**：FromISR函数只能在`configMAX_SYSCALL_INTERRUPT_PRIORITY`以下的中断中调用

FreeRTOS的核心设计原则之一是**中断入口必须尽可能快**。因此，从ISR版本API做了以下优化：

- 不使用基础临界区（会完全禁用中断），而是使用中断安全的临界区管理
- 返回值包含是否需要触发调度的信息（`BaseType_t *pxHigherPriorityTaskWoken`参数）
- 内部使用`portENTER_CRITICAL_FROM_ISR()`和`portEXIT_CRITICAL_FROM_ISR()`宏

#### 源码分析

以`uxTaskPriorityGetFromISR`为例，分析FromISR API的实现特点：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2643-2684
UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask )
{
    TCB_t const * pxTCB;
    UBaseType_t uxReturn;
    UBaseType_t uxSavedInterruptStatus;

    /* RTOS ports that support interrupt nesting have the concept of a
     * maximum  system call (or maximum API call) interrupt priority.
     * Interrupts that are above the maximum system call priority are keep
     * permanently enabled, even when the RTOS kernel is in a critical section,
     * but cannot make any calls to FreeRTOS API functions. */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    uxSavedInterruptStatus = ( UBaseType_t ) taskENTER_CRITICAL_FROM_ISR();
    {
        pxTCB = prvGetTCBFromHandle( xTask );
        uxReturn = pxTCB->uxPriority;
    }
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );

    return uxReturn;
}
```

关键设计要点：

1. **中断优先级断言检查**：`portASSERT_IF_INTERRUPT_PRIORITY_INVALID()`确保当前中断优先级在允许范围内，防止从不可调用FreeRTOS API的中断中调用

2. **中断安全的临界区**：`taskENTER_CRITICAL_FROM_ISR()`和`taskEXIT_CRITICAL_FROM_ISR()`配对使用，允许在中断嵌套时正确管理临界区深度

3. **返回值设计**：虽然`uxTaskPriorityGetFromISR`没有`pxHigherPriorityTaskWoken`参数，但其他FromISR函数（如`xQueueSendFromISR`）都有此参数，用于通知调度器在中断退出时进行任务切换

在`portmacro.h`中，FromISR版本的临界区宏定义如下：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/portmacro.h:117-118
#define portSET_INTERRUPT_MASK_FROM_ISR()         ulPortRaiseBASEPRI()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )    vPortSetBASEPRI( x )
```

#### 面试参考答案

> **问题：FreeRTOS中FromISR API命名规范的意义是什么？**
>
> **回答：**
> `FromISR`后缀是FreeRTOS安全设计的核心规范，它有以下重要意义：
>
> **1. 语义安全约束**
> - 命名本身就是最强的不成文规定，提醒开发者只能在中断上下文中使用
> - 从API名称上强制区分了两种调用场景，防止在任务中误用中断安全版本
>
> **2. 内部实现差异**
> - FromISR版本使用中断安全的临界区（`taskENTER_CRITICAL_FROM_ISR`）
> - 常规版本使用基础临界区（`taskENTER_CRITICAL`），两者在中断嵌套时的行为不同
> - FromISR版本增加了`portASSERT_IF_INTERRUPT_PRIORITY_INVALID()`检查，确保中断优先级符合要求
>
> **3. 中断优先级约束**
> - 只能在`configMAX_SYSCALL_INTERRUPT_PRIORITY`以下的中断中调用
> - 高于此优先级的中断属于"不可屏蔽"中断，不能调用任何FreeRTOS API
> - 这点在Cortex-M架构中尤为重要，错误的优先级配置会导致系统崩溃
>
> **4. 调度触发机制**
> - FromISR函数通过`pxHigherPriorityTaskWoken`参数通知是否需要触发调度
> - 典型用法：
>   ```c
>   BaseType_t xHigherPriorityTaskWoken = pdFALSE;
>   xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
>   if(xHigherPriorityTaskWoken == pdTRUE) {
>       portYIELD_FROM_ISR();
>   }
>   ```
>
> **常见误区**：在中断中调用非FromISR API看似能工作，但存在以下风险：
> - 可能导致数据竞争和状态不一致
> - 可能在特定条件下（如中断嵌套）导致死锁
> - 可能无法正确触发调度，导致系统实时性下降

---

## 4.2 调度延迟问题

### 4.2.1 如何测量调度延迟

#### 原理阐述

调度延迟（Scheduling Latency）是指从事件发生（如中断、任务就绪）到目标任务开始执行之间的时间间隔。在实时系统中，调度延迟是衡量系统实时性能的关键指标。主要包括：

1. **中断延迟（Interrupt Latency）**：从硬件中断信号产生到CPU开始执行ISR的时间
2. **调度延迟（Scheduling Latency）**：从ISR返回或任务就绪到目标任务开始执行的时间
3. **上下文切换时间（Context Switch Time）**：保存当前任务状态并恢复新任务状态所需的时间

测量调度延迟的方法有多种：
- **硬件计数器**：使用DWT_CYCCNT等性能计数器
- **GPIO翻转**：通过GPIO输出测量时间
- **OS Trace工具**：使用Tracealyzer等可视化工具

#### 源码分析

在ARM Cortex-M中，可以使用DWT（Data Watchpoint and Trace）单元的CYCCNT寄存器进行高精度计时。FreeRTOS配置中提供了相关宏定义：

```c
// FreeRTOSConfig.h
#define configUSE_TRACE_FACILITY                1
#define configGENERATE_RUN_TIME_STATS            1
```

测量调度延迟的核心思路：

```c
// 使用DWT_CYCCNT测量调度延迟
volatile uint32_t ulInterruptTime;
volatile uint32_t ulTaskStartTime;
volatile uint32_t ulSchedulingLatency;

// 在中断中记录时间戳
void ISR_Handler(void) {
    ulInterruptTime = DWT->CYCCNT;

    // 处理中断...

    // 触发延迟处理任务
    xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);

    if(xHigherPriorityTaskWoken == pdTRUE) {
        // 记录调度触发时刻
        ulSchedulingLatency = DWT->CYCCNT - ulInterruptTime;
        portYIELD_FROM_ISR();
    }
}

// 在任务中记录执行开始时间
void vTaskFunction(void *pvParameters) {
    ulTaskStartTime = DWT->CYCCNT;

    // 任务开始执行
    // 总延迟 = ulTaskStartTime - ulInterruptTime
}
```

FreeRTOS的上下文切换通过PendSV中断实现，关键代码在`port.c`中：

```c
// 调度延迟 = PendSV处理时间 + 任务栈恢复时间
// 典型值：Cortex-M3/M4上约20-50us，M7上约10-30us
```

使用Tracealyzer可以直观地查看调度延迟：

```c
// 启用FreeRTOS trace hooks
#define configUSE_TRACE_HOOKS                   1
#define configUSE_STATS_FORMATTING_FUNCTIONS   1
```

#### 面试参考答案

> **问题：如何测量FreeRTOS的调度延迟？**
>
> **回答：**
> 调度延迟测量是实时系统性能评估的核心工作。以下是几种常用的测量方法：
>
> **1. 硬件计数器法（推荐）**
> 使用ARM Cortex-M的DWT_CYCCNT寄存器：
> ```c
> // 初始化
> CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
> DWT->CYCCNT = 0;
> DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
>
> // 测量中断到任务的总延迟
> volatile uint32_t ulInterruptEnter, ulTaskStart, ulTotalLatency;
>
> void GPIO_ISR(void) {
>     ulInterruptEnter = DWT->CYCCNT;
>     // 最小化ISR处理...
>     xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
>     if(xHigherPriorityTaskWoken) {
>         portYIELD_FROM_ISR();  // 这里触发PendSV
>     }
> }
>
> void vTaskHandler(void *p) {
>     ulTaskStart = DWT->CYCCNT;
>     ulTotalLatency = ulTaskStart - ulInterruptEnter;
> }
> ```
>
> **2. GPIO翻转法（适合示波器测量）**
> ```c
> void GPIO_ISR(void) {
>     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
>     // ISR处理...
>     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
> }
>
> void vTaskHandler(void *p) {
>     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
>     // 任务处理...
>     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
> }
> ```
> 通过测量两个GPIO信号边沿的时间差即可得到调度延迟。
>
> **3. Tracealyzer法**
> - 配置`configUSE_TRACE_HOOKS`和`configUSE_STATS_FORMATTING_FUNCTIONS`
> - 使用Tracealyzer记录任务执行轨迹
> - 从可视化的时间轴上直接读取延迟值
>
> **典型延迟指标（基于Cortex-M4 @ 168MHz）：**
> | 指标 | 典型值 | 最差值 |
> |------|--------|--------|
> | 中断延迟 | 6-12 cycle | 20+ cycle |
> | 调度延迟 | 15-30 us | 50+ us |
> | 上下文切换 | 5-15 us | 30 us |
>
> **影响调度延迟的主要因素：**
> - 临界区长度（应尽可能短）
> - 中断处理时间（ISR应快速完成）
> - `configMAX_SYSCALL_INTERRUPT_PRIORITY`配置
> - 任务栈深度和缓存命中率

---

### 4.2.2 高优先级任务阻塞低优先级的原因

#### 原理阐述

在FreeRTOS中，高优先级任务阻塞低优先级任务的现象通常被称为**优先级反转**（Priority Inversion）。这是一个经典的实时系统问题，其根本原因在于共享资源的访问控制机制。

优先级反转的典型场景：

1. **假设**：任务A（优先级3）获取互斥锁 -> 任务B（优先级2）就绪（抢占A）-> 任务C（优先级1）获取同一互斥锁（阻塞A）
2. **结果**：低优先级任务C阻塞了高优先级任务A，形成了"优先级反转"

FreeRTOS通过**优先级继承协议**（Priority Inheritance）来缓解这个问题。当高优先级任务因等待互斥锁而阻塞时，持有锁的低优先级任务的优先级会被临时提升到与等待者相同的水平。

#### 源码分析

FreeRTOS的优先级继承实现在信号量获取操作中。以`xQueueSemaphoreTake`函数为例：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c
BaseType_t xQueueSemaphoreTake( QueueHandle_t xQueue, TickType_t xTicksToWait )
{
    // ... 省略部分代码 ...

    if( pxQueue->uxMessagesWaiting == ( UBaseType_t ) 0 )
    {
        // 队列为空，检查是否需要阻塞
        if( xTicksToWait > ( TickType_t ) 0 )
        {
            if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
            {
                // 互斥量：记录原始持有者优先级，用于优先级继承
                xPriorityOfWaitingTask = pxCurrentTCB->uxPriority;
                ( void ) xTaskPriorityDisinherit( pxQueue->pxMutexHolder );
                pxQueue->pxMutexHolder = NULL;
            }

            // 阻塞等待任务
            vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );

            // ... 触发优先级继承的代码 ...
        }
    }
    // ... 省略部分代码 ...
}
```

优先级继承的关键函数`xTaskPriorityDisinherit`：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2908-2950
BaseType_t xTaskPriorityDisinherit( TaskHandle_t xTaskToDisinherit )
{
    TCB_t * pxTCB;
    BaseType_t xReturn = pdFALSE;

    if( xTaskToDisinherit != NULL )
    {
        pxTCB = ( TCB_t * ) xTaskToDisinherit;

        if( pxTCB->uxPriority != pxTCB->uxBasePriority )
        {
            /* 当前优先级高于基础优先级，说明发生了优先级继承 */
            pxTCB->uxPriority = pxTCB->uxBasePriority;
            /* 可能需要触发任务切换 */
            if( pxTCB != pxCurrentTCB )
            {
                xReturn = pdTRUE;
            }
        }
    }

    return xReturn;
}
```

优先级恢复在互斥量释放时执行：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c:1095-1120
BaseType_t xQueueGenericSend( QueueHandle_t xQueue,
                              const void * pvItemToQueue,
                              TickType_t xTicksToWait,
                              const BaseType_t xJustPeeking )
{
    // ... 省略部分代码 ...

    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
    {
        /* 互斥量释放：恢复持有者的原始优先级 */
        ( void ) xTaskPriorityInherit( pxQueue->pxMutexHolder );
    }

    // ... 省略部分代码 ...
}
```

`xTaskPriorityInherit`函数的实现：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2870-2906
BaseType_t xTaskPriorityInherit( TaskHandle_t xTaskToInherit )
{
    TCB_t * pxTCB;
    BaseType_t xReturn = pdFALSE;

    if( xTaskToInherit != NULL )
    {
        pxTCB = ( TCB_t * ) xTaskToInherit;

        /* 只有当等待任务的优先级高于当前持有者时才提升 */
        if( pxCurrentTCB->uxPriority > pxTCB->uxPriority )
        {
            /* 提升持有者的优先级 */
            pxTCB->uxPriority = pxCurrentTCB->uxPriority;

            /* 更新就绪链表位置 */
            ( void ) uxTaskRemoveFromEventList( &( pxTCB->xEventListItem ) );
            ( void ) prvAddTaskToReadyList( pxTCB );
            xReturn = pdTRUE;
        }
    }

    return xReturn;
}
```

#### 面试参考答案

> **问题：高优先级任务阻塞低优先级任务的原因是什么？**
>
> **回答：**
> 这是实时系统中经典的**优先级反转**（Priority Inversion）问题。其根本原因是**共享资源（互斥锁）的访问控制**机制。
>
> **典型场景分析：**
> ```
> 时刻T1: 任务L(优先级1)获取互斥锁M，进入临界区
> 时刻T2: 高优先级任务H(优先级3)就绪，抢占L
> 时刻T3: 任务H尝试获取互斥锁M，但M被L持有，H阻塞
> 时刻T4: 中优先级任务M(优先级2)就绪，抢占L
> 时刻T5: L继续执行并释放M
> 时刻T6: H获取M并恢复运行
> ```
> 在这个场景中，任务M（优先级2）意外地"劫持"了CPU，导致高优先级任务H无法运行，形成了优先级反转。
>
> **FreeRTOS的解决方案：优先级继承协议**
> FreeRTOS通过优先级继承（Priority Inheritance）来缓解这个问题：
>
> 1. **当高优先级任务H等待被低优先级任务L持有的互斥锁时**
>    - 系统检测到这种情况
>    - 将L的优先级临时提升到H的优先级（`xTaskPriorityInherit`）
>
> 2. **当L释放互斥锁时**
>    - L的优先级恢复为原始优先级（`xTaskPriorityDisinherit`）
>    - 高优先级任务H得以继续执行
>
> **代码实现分析：**
> - `xTaskPriorityInherit()`：在`queue.c`中的信号量获取函数调用，提升持有者优先级
> - `xTaskPriorityDisinherit()`：在`tasks.c`中，恢复持有者的基础优先级
> - TCB中的`uxBasePriority`字段记录任务的原始基础优先级
>
> **优先级继承的局限性：**
> - 只能缓解，不能完全消除优先级反转
> - 死锁场景下无效：两个互斥锁AB被任务AB分别持有，任务A等待B持有的锁，任务B等待A持有的锁
> - 可能引入额外的调度延迟
>
> **工程实践建议：**
> - 尽量减少互斥锁的使用范围和持有时间
> - 优先级继承需要`configUSE_MUTEXES`配置为1
> - 对于安全关键系统，考虑使用**优先级天花板协议**（Priority Ceiling Protocol）

---

## 4.3 本章小结

本章深入分析了FreeRTOS中断管理与实时性的核心技术要点：

1. **临界区的影响**：临界区通过BASEPRI机制实现，会对中断响应和任务调度产生延迟。设计时应保持临界区代码最短，并合理配置`configMAX_SYSCALL_INTERRUPT_PRIORITY`。

2. **FromISR API规范**：命名规范是安全设计的重要组成部分，FromISR版本使用中断安全的临界区管理，并提供调度触发机制。

3. **调度延迟测量**：可通过DWT_CYCCNT、GPIO翻转或Tracealyzer等工具测量调度延迟，这是评估实时系统性能的关键指标。

4. **优先级反转问题**：通过优先级继承协议缓解，理解其根本原因和局限性对设计高可靠性实时系统至关重要。

---

## 参考资料

| 文件 | 描述 |
|------|------|
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/r0p1/portmacro.h` | 临界区宏定义实现 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2500-2800` | 临界区使用示例、优先级继承/继承函数 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c` | 互斥量优先级继承实现 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/FreeRTOS.h` | FromISR相关API声明 |

---

*本章完*

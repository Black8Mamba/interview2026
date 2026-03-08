# 第5章：实际项目问题

本章深入分析FreeRTOS在实际项目中常见的疑难问题，包括死锁、优先级反转、栈溢出和内存泄漏。这些问题在嵌入式产品中可能导致系统不稳定或崩溃，是高级工程师必须掌握的核心调试技能。

---

## 5.1 死锁问题

### 5.1.1 FreeRTOS中可能导致死锁的场景

#### 原理阐述

死锁（Deadlock）是指两个或多个任务相互等待对方持有的资源，导致所有任务都无法继续执行的状态。在FreeRTOS中，死锁通常发生在以下场景：

1. **互斥锁死锁**：两个任务相互持有对方需要的互斥锁
2. **递归锁死锁**：同一任务递归获取同一互斥锁（未使用递归互斥量）
3. **临界区死锁**：在临界区内调用阻塞API导致永久等待
4. **优先级死锁**：高优先级任务等待低优先级任务持有的资源

死锁产生的四个必要条件（Coin条件）：
- **互斥条件**：资源一次只能被一个任务持有
- **占有并等待**：任务持有资源的同时等待其他资源
- **不抢占条件**：资源不能被强制从持有者手中夺取
- **循环等待**：形成资源等待的环路

#### 案例分析

**案例1：互斥锁循环等待**

```c
// 任务A：处理传感器数据，需要先获取mutex_sensor，再获取mutex_storage
void vTaskSensorProcessing(void *pvParameters) {
    while(1) {
        xSemaphoreTake(mutex_sensor, portMAX_DELAY);    // 获取传感器锁
        // 读取传感器数据...
        xSemaphoreTake(mutex_storage, portMAX_DELAY);   // 尝试获取存储锁
        // 存储数据...
        xSemaphoreGive(mutex_storage);
        xSemaphoreGive(mutex_sensor);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 任务B：上传数据到服务器，需要先获取mutex_storage，再获取mutex_sensor
void vTaskDataUpload(void *pvParameters) {
    while(1) {
        xSemaphoreTake(mutex_storage, portMAX_DELAY);   // 获取存储锁
        // 读取存储数据...
        xSemaphoreTake(mutex_sensor, portMAX_DELAY);    // 尝试获取传感器锁
        // 处理...
        xSemaphoreGive(mutex_sensor);
        xSemaphoreGive(mutex_storage);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

上述代码存在典型的死锁场景：任务A持有mutex_sensor等待mutex_storage，任务B持有mutex_storage等待mutex_sensor，形成循环等待。

**案例2：临界区内阻塞**

```c
// 错误的实现：在临界区内调用可能阻塞的API
void vISRHandler(void) {
    taskENTER_CRITICAL();
    {
        // 错误：在临界区内调用阻塞API
        xQueueSendFromISR(xQueue, &data, NULL);  // 可能阻塞！
        // 或者：
        xSemaphoreGiveFromISR(xSemaphore, NULL); // 可能阻塞！
    }
    taskEXIT_CRITICAL();
}
```

虽然FromISR版本通常不会阻塞，但在某些配置下可能导致问题。更危险的是在任务中调用可能阻塞的API：

```c
// 错误的实现：在持有锁的情况下调用可能阻塞的函数
void vTaskProcess(void *pvParameters) {
    while(1) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        {
            // 错误：在持有互斥锁时调用阻塞函数
            vTaskDelay(100);  // 死锁！其他需要此锁的任务将永远等待
            // 或者：
            xQueueSend(xQueue, &data, portMAX_DELAY);  // 死锁！
        }
        xSemaphoreGive(mutex);
    }
}
```

#### 面试参考答案

> **问题：FreeRTOS中可能导致死锁的场景有哪些？如何预防？**
>
> **回答：**
>
> FreeRTOS中常见的死锁场景主要包括以下几类：
>
> **1. 互斥锁循环等待**
> - 两个或多个任务以不同顺序获取多个互斥锁
> - 预防方法：统一加锁顺序，所有任务必须按照相同的顺序获取多个锁
>
> **2. 递归锁误用**
> - 使用普通信号量/互斥量进行递归调用
> - 预防方法：使用`xSemaphoreCreateRecursiveMutex()`创建递归互斥量
>
> **3. 临界区内阻塞**
> - 在临界区（`taskENTER_CRITICAL`）内调用任何可能阻塞的API
> - 预防方法：严格区分临界区代码范围，只保护必要的原子操作
>
> **4. 优先级死锁**
> - 高优先级任务等待低优先级任务持有的资源，而低优先级任务被中优先级任务抢占
> - 预防方法：使用优先级继承机制（FreeRTOS互斥量自动支持）
>
> **工程实践建议：**
> - 建立锁获取规范：统一多锁获取顺序
> - 锁持有时间监控：使用Tracealyzer监控最大锁持有时间
> - 超时机制：使用带超时的`xSemaphoreTake(mutex, timeout)`而非`portMAX_DELAY`
> - 死锁检测：实现锁超时检测机制，记录超时日志

---

## 5.2 优先级反转问题

### 5.2.1 优先级反转的经典案例分析

#### 原理阐述

优先级反转（Priority Inversion）是实时系统中经典的调度问题。当高优先级任务等待低优先级任务持有的资源时，中等优先级的任务可能抢占低优先级任务的执行权，导致高优先级任务被间接阻塞。

优先级反转的三个阶段：
1. **阶段1**：高优先级任务H获取资源后释放，进入等待
2. **阶段2**：低优先级任务L获取资源执行，中优先级任务M抢占L
3. **阶段3**：L继续执行并释放资源，H才能继续

FreeRTOS通过优先级继承协议（Priority Inheritance）来缓解这个问题。

#### 源码分析

FreeRTOS互斥量的优先级继承实现位于`queue.c`和`tasks.c`中。当高优先级任务等待互斥锁时，系统会临时提升持有锁的低优先级任务的优先级：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2870-2889
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

关键点说明：
- 只有当等待任务的优先级高于持有者时才触发继承
- 继承后持有者的优先级被提升到与等待者相同
- 释放锁时优先级恢复（通过`xTaskPriorityDisinherit`函数）

#### 案例分析

**经典Mars Pathfinder案例**

1997年火星探路者（Mars Pathfinder）任务中就发生了著名的优先级反转问题：

```
时间线：
T1: 任务M (Weather, 优先级低) 获取总线访问权
T2: 任务H (High Priority) 就绪，需要访问总线但被阻塞
T3: 任务M (Weather) 被中断处理程序抢占
T4: 中优先级任务I (Info Bus) 开始执行
T5: 任务H 永远等待任务M释放总线
T6: 看门狗超时，系统重启
```

最终NASA通过添加优先级继承解决此问题。

**FreeRTOS中的优先级反转实例**

```c
// 优先级配置：Task_H(优先级3) > Task_M(优先级2) > Task_L(优先级1)
SemaphoreHandle_t xUartMutex;

void vTaskHighPriority(void *pv) {
    while(1) {
        xSemaphoreTake(xUartMutex, portMAX_DELAY);
        // 发送关键数据
        vUartSend("Critical");
        xSemaphoreGive(xUartMutex);
    }
}

void vTaskLowPriority(void *pv) {
    while(1) {
        xSemaphoreTake(xUartMutex, portMAX_DELAY);
        // 发送日志数据（耗时操作）
        for(int i=0; i<1000; i++) {
            vUartSend("Log data...\n");
        }
        xSemaphoreGive(xUartMutex);
    }
}

void vTaskMediumPriority(void *pv) {
    while(1) {
        // 无锁计算密集任务
        vDoSomeCalculation();
    }
}
```

执行流程：
1. Task_L获取UART互斥锁，开始发送日志
2. Task_H就绪尝试获取锁，被阻塞
3. Task_M就绪，抢占Task_L
4. Task_L无法继续执行释放锁
5. Task_H需要等待Task_M和Task_L完成

启用优先级继承后，Task_L的优先级会被临时提升到3，使其能够尽快完成并释放锁。

#### 面试参考答案

> **问题：请分析优先级反转问题，并说明FreeRTOS如何解决？**
>
> **回答：**
>
> **优先级反转问题分析：**
>
> 优先级反转是指高优先级任务因等待低优先级任务持有的资源而被阻塞，间接被中优先级任务"劫持"的现象。这种情况会导致：
> - 高优先级任务响应延迟不确定
> - 看门狗超时导致系统重启
> - 系统实时性无法保证
>
> **经典场景：**
> ```
> 任务L(P1) -> 获取锁 -> 被任务M(P2)抢占
> 任务H(P3) -> 等待锁 -> 被永久阻塞
> 任务M(P2)持续运行，导致任务H无法执行
> ```
>
> **FreeRTOS解决方案：优先级继承协议**
>
> FreeRTOS通过互斥量的优先级继承机制缓解此问题：
>
> 1. **锁获取时**：`xTaskPriorityInherit()`函数检测到高优先级任务等待时，将低优先级持有者的优先级提升到与等待者相同
>
> 2. **锁释放时**：`xTaskPriorityDisinherit()`函数恢复持有者的基础优先级
>
> 3. **实现位置**：
>    - `queue.c`中的`xQueueSemaphoreTake`调用优先级继承
>    - `tasks.c`中的`xTaskPriorityInherit`和`xTaskPriorityDisinherit`实现具体逻辑
>    - TCB中的`uxBasePriority`字段保存原始优先级
>
> **局限性说明：**
> - 只能缓解，不能完全消除（死锁场景无效）
> - 只对互斥量（Mutex）有效，二值信号量不支持
> - 需要`configUSE_MUTEXS`配置为1
>
> **工程实践建议：**
> - 优先使用互斥量而非二值信号量进行资源共享
> - 尽量减少锁的持有时间
> - 避免在持锁时进行阻塞操作
> - 使用Tracealyzer监控优先级反转事件

---

## 5.3 栈溢出问题

### 5.3.1 如何调试栈溢出

#### 原理阐述

栈溢出（Stack Overflow）是指任务使用的栈空间超过分配大小，导致栈内存越界。这会破坏其他任务或内核数据，可能导致以下后果：

1. **数据损坏**：栈溢出可能覆盖其他任务的数据或内核结构
2. **系统崩溃**：栈溢出可能导致HardFault异常
3. **随机故障**：内存损坏可能导致不可预测的行为

FreeRTOS提供三种栈溢出检测模式，通过`configCHECK_FOR_STACK_OVERFLOW`配置：

| 配置值 | 检测方法 | 检测精度 | 性能影响 |
|--------|----------|----------|----------|
| 0 | 不检测 | - | 无 |
| 1 | 检查栈指针是否超出限制 | 中等 | 低 |
| 2 | 检查栈填充图案 | 较高 | 中等 |

#### 源码分析

栈溢出检测在任务上下文切换时执行，关键代码位于`tasks.c`：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:5099-5100
/* Check for stack overflow, if configured. */
taskCHECK_FOR_STACK_OVERFLOW();
```

栈溢出检测宏定义在`stack_macros.h`中：

```c
// configCHECK_FOR_STACK_OVERFLOW == 1: 仅检查栈指针
#if ( ( configCHECK_FOR_STACK_OVERFLOW == 1 ) && ( portSTACK_GROWTH < 0 ) )
#define taskCHECK_FOR_STACK_OVERFLOW()                                    \
    do {                                                                   \
        if( pxCurrentTCB->pxTopOfStack <= pxCurrentTCB->pxStack + portSTACK_LIMIT_PADDING ) \
        {                                                                  \
            vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName ); \
        }                                                                  \
    } while( 0 )
#endif

// configCHECK_FOR_STACK_OVERFLOW > 1: 检查栈填充图案
#if ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) && ( portSTACK_GROWTH < 0 ) )
#define taskCHECK_FOR_STACK_OVERFLOW()                                    \
    do {                                                                   \
        const uint32_t * const pulStack = ( uint32_t * ) pxCurrentTCB->pxStack; \
        const uint32_t ulCheckValue = ( uint32_t ) 0xa5a5a5a5U;          \
                                                                           \
        if( ( pulStack[ 0 ] != ulCheckValue ) ||                          \
            ( pulStack[ 1 ] != ulCheckValue ) ||                          \
            ( pulStack[ 2 ] != ulCheckValue ) ||                          \
            ( pulStack[ 3 ] != ulCheckValue ) )                           \
        {                                                                  \
            vApplicationStackOverflowHook( ( TaskHandle_t ) pxCurrentTCB, pcOverflowTaskName ); \
        }                                                                  \
    } while( 0 )
#endif
```

关键设计说明：
1. **栈填充**：任务创建时，栈空间被填充为`tskSTACK_FILL_BYTE`（默认0xa5）
2. **检测时机**：在`vTaskSwitchContext()`中被调用，即任务切换时
3. **检测方式**：检查栈低地址的若干字节是否仍为0xa5

#### 案例分析

**案例：栈溢出调试流程**

```c
// FreeRTOSConfig.h配置
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_MALLOC_FAILED_HOOK      1
#define configASSERT( x )                  ( ( x ) || vAssertCalled( __FILE__, __LINE__ ) )

// 栈溢出钩子函数实现
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // 记录溢出任务信息
    printf("[STACK OVERFLOW] Task: %s, Handle: %p\n", pcTaskName, xTask);

    // 尝试获取栈使用信息
    TaskStatus_t xTaskStatus;
    vTaskGetInfo(xTask, &xTaskStatus, pdTRUE, eInvalid);

    printf("  Stack High Water Mark: %lu words\n", xTaskStatus.uxStackHighWaterMark);

    // 写入错误日志到Flash
    LogError("StackOverflow", pcTaskName);

    // 进入错误处理状态
    while(1) {
        // 闪烁LED指示错误
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(100);
    }
}
```

**栈大小计算方法：**

```c
// 方法1：使用栈高水位标记
void vTaskCode(void *pv) {
    while(1) {
        // 定期检查栈使用情况
        UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
        if(highWaterMark < 100) {
            printf("Warning: Stack low! Only %lu words remaining\n", highWaterMark);
        }

        // 栈-intensive操作
        char buffer[512];  // 本地数组使用栈空间
        memset(buffer, 0, sizeof(buffer));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// 方法2：静态分析
// 栈需求 = 函数调用深度 × 最大局部变量 + 中断栈帧大小
// 典型计算：
// - 函数调用嵌套：最大深度 × 32字节（返回地址+寄存器）
// - 局部数组：最大数组大小
// - 中断嵌套：最大中断嵌套深度 × 栈帧大小
// 安全系数：计算结果 × 1.5
```

#### 面试参考答案

> **问题：如何调试FreeRTOS中的栈溢出问题？**
>
> **回答：**
>
> **1. 启用栈溢出检测**
>
> 在`FreeRTOSConfig.h`中配置：
> ```c
> #define configCHECK_FOR_STACK_OVERFLOW    2
> ```
>
> 并实现栈溢出钩子：
> ```c
> void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
>     // 记录错误日志
>     // 可选：进入安全状态
> }
> ```
>
> **2. 栈使用监控**
>
> 使用FreeRTOS API监控栈使用：
> ```c
> // 获取栈高水位（剩余空间）
> UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
>
> // 获取所有任务的栈状态
> TaskStatus_t *pxTaskStatusArray = pvPortMalloc(sizeof(TaskStatus_t) * uxTaskGetNumberOfTasks());
> uint32_t ulTotalStack = 0;
> UBaseType_t uxTaskNumber;
> uxTaskGetSystemState(pxTaskStatusArray, uxTaskGetNumberOfTasks(), &uxTaskNumber);
> ```
>
> **3. 常见栈溢出原因：**
> - 递归调用过深
> - 大型局部数组
> - 中断嵌套过深
> - 函数调用链太深
> - printf/fprintf等可变参数函数
>
> **4. 栈大小确定方法：**
> - 静态分析：分析最大函数调用深度和局部变量
> - 动态测试：运行最大负载时监控栈水线
> - 经验值：一般任务至少512字节，复杂任务2-4KB
>
> **5. 调试技巧：**
> - 使用Tracealyzer的栈分析功能
> - 在关键任务中周期性打印栈水线
> - 使用MPU设置栈边界保护

---

## 5.4 内存泄漏问题

### 5.4.1 内存泄漏场景有哪些

#### 原理阐述

内存泄漏（Memory Leak）是指程序分配内存后未正确释放，导致内存占用持续增长。在FreeRTOS中，内存泄漏主要发生在以下场景：

1. **动态内存分配未释放**：使用`pvPortMalloc()`分配但未调用`vPortFree()`
2. **任务删除但未清理资源**：删除任务时未释放任务持有的资源
3. **队列/信号量泄漏**：创建IPC对象后未删除
4. **中断中分配内存**：在ISR中分配内存但无法安全释放

FreeRTOS的堆内存管理（heap_1~heap_5）各有特点，其中heap_1不支持释放，heap_2~heap_5支持内存释放但可能产生碎片。

#### 源码分析

FreeRTOS内存分配器实现位于`portable/MemMang/`目录。以heap_4.c为例，分析内存分配和释放机制：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/MemMang/heap_4.c
void *pvPortMalloc( size_t xWantedSize )
{
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
    void *pvReturn = NULL;

    vTaskSuspendAll();
    {
        // 首次适配算法
        pxPreviousBlock = &xStart;
        pxBlock = xStart.pxNextFreeBlock;

        while( pxBlock->xBlockSize < xWantedSize + xHeapStructSize )
        {
            pxPreviousBlock = pxBlock;
            pxBlock = pxBlock->pxNextFreeBlock;

            if( pxBlock == &xEnd )
            {
                break;
            }
        }
    }
    ( void ) xTaskResumeAll();

    return pvReturn;
}
```

关键点：
- heap_4使用最佳适配（Best Fit）算法
- 释放时合并相邻空闲块
- 内存对齐由`portBYTE_ALIGNMENT`控制

#### 案例分析

**案例1：任务资源未释放**

```c
// 错误的实现：任务删除时未释放持有的资源
void vTaskDataProcess(void *pvParameters) {
    uint8_t *pBuffer = pvPortMalloc(1024);  // 分配缓存
    QueueHandle_t xQueue = xQueueCreate(10, sizeof(Data_t));  // 创建队列

    while(1) {
        // 处理数据...
        xQueueReceive(xQueue, &data, portMAX_DELAY);
    }
    // 注意：任务永远不会退出，但如果被删除，内存将泄漏
}

// 正确的实现：使用静态分配
StaticQueue_t xQueueBuffer;
uint8_t ucQueueStorage[ 10 * sizeof( Data_t ) ];
QueueHandle_t xQueue = xQueueCreateStatic(10, sizeof(Data_t),
                                           ucQueueStorage, &xQueueBuffer);
```

**案例2：条件分支中的内存泄漏**

```c
// 错误的实现：某些分支分配内存但未全部释放
void vTaskProcessCommand(Command_t *cmd) {
    uint8_t *pData = NULL;

    switch(cmd->type) {
        case TYPE_A:
            pData = pvPortMalloc(cmd->length);  // 分配
            if(pData) {
                memcpy(pData, cmd->data, cmd->length);
                processTypeA(pData);
                // 错误：忘记释放
            }
            break;

        case TYPE_B:
            pData = pvPortMalloc(cmd->length);  // 分配
            if(pData) {
                memcpy(pData, cmd->data, cmd->length);
                processTypeB(pData);
                vPortFree(pData);  // 释放
            }
            break;
    }
    // TYPE_A分支忘记释放，导致泄漏
}

// 正确的实现：使用统一释放模式
void vTaskProcessCommand(Command_t *cmd) {
    uint8_t *pData = pvPortMalloc(cmd->length);
    if(pData == NULL) return;

    memcpy(pData, cmd->data, cmd->length);

    switch(cmd->type) {
        case TYPE_A:
            processTypeA(pData);
            break;
        case TYPE_B:
            processTypeB(pData);
            break;
    }

    vPortFree(pData);  // 统一在函数末尾释放
}
```

**案例3：中断中的内存分配**

```c
// 错误的实现：在ISR中分配内存
void vUART_ISR(void) {
    if(UART->SR & RXNE) {
        // 错误：在中断中分配内存
        uint8_t *pBuffer = pvPortMalloc(256);
        if(pBuffer) {
            *pBuffer = UART->DR;
            xQueueSendFromISR(xQueue, &pBuffer, NULL);  // 传递指针
        }
    }
}

// 正确的实现：使用静态缓冲区或任务通知
void vUART_ISR(void) {
    if(UART->SR & RXNE) {
        uint8_t data = UART->DR;
        // 使用静态缓冲区或直接发送数据
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
        if(xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}
```

**内存泄漏检测方法：**

```c
// 方法1：实现内存分配钩子
void vApplicationMallocFailedHook(void) {
    printf("[MALLOC FAILED] Heap exhausted!\n");
    printf("  Free heap: %lu bytes\n", xPortGetFreeHeapSize());
}

// 方法2：周期性打印堆状态
void vTaskMonitor(void *pv) {
    while(1) {
        printf("Free Heap: %lu bytes\n", xPortGetFreeHeapSize());
        printf("Min Free Heap: %lu bytes\n", xPortGetMinimumEverFreeHeapSize());

        // 监控泄漏
        static uint32_t lastFreeHeap = 0;
        if(lastFreeHeap != 0 && (lastFreeHeap - xPortGetFreeHeapSize()) > 1024) {
            printf("WARNING: Possible memory leak detected! Lost %lu bytes\n",
                   lastFreeHeap - xPortGetFreeHeapSize());
        }
        lastFreeHeap = xPortGetFreeHeapSize();

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

#### 面试参考答案

> **问题：FreeRTOS中内存泄漏的场景有哪些？如何检测和预防？**
>
> **回答：**
>
> **常见内存泄漏场景：**
>
> **1. 动态内存未释放**
> - `pvPortMalloc()`分配后未调用`vPortFree()`
> - 特别容易发生在错误处理分支中
>
> **2. 任务资源泄漏**
> - 任务删除时未释放持有的队列、信号量、内存
> - 正确做法：实现清理回调或在任务删除前手动清理
>
> **3. 条件分支泄漏**
> - 某些代码路径分配内存但其他路径未释放
> - 解决：统一释放模式，分配和释放配对
>
> **4. ISR中分配内存**
> - FreeRTOS不建议在ISR中调用`pvPortMalloc()`
> - 解决：使用静态缓冲区或延迟到任务中处理
>
> **5. 递归调用泄漏**
> - 递归函数中分配内存未正确释放
> - 解决：避免在递归中使用动态分配
>
> **内存泄漏检测方法：**
>
> ```c
> // 1. 启用内存分配失败钩子
> #define configUSE_MALLOC_FAILED_HOOK    1
> void vApplicationMallocFailedHook(void) { }
>
> // 2. 周期性监控堆状态
> printf("Free: %lu, MinFree: %lu\n",
>        xPortGetFreeHeapSize(),
>        xPortGetMinimumEverFreeHeapSize());
>
> // 3. 使用工具：Tracealyzer、Percepio Lem
> ```
>
> **预防措施：**
> - 优先使用静态分配替代动态分配
> - 建立内存分配规范：谁分配谁释放
> - 代码审查重点检查malloc/free配对
> - 安全认证系统中禁用动态内存分配

---

## 5.5 本章小结

本章深入分析了FreeRTOS实际项目中的四大核心问题：

1. **死锁问题**：掌握循环等待、递归锁、临界区阻塞等死锁场景，建立统一的锁获取规范是预防关键。

2. **优先级反转问题**：理解优先级继承协议的原理和局限性，合理使用互斥量可以有效缓解优先级反转。

3. **栈溢出问题**：通过配置`configCHECK_FOR_STACK_OVERFLOW`和使用`uxTaskGetStackHighWaterMark()`监控栈使用，结合静态分析和动态测试确定合适的栈大小。

4. **内存泄漏问题**：规范内存分配释放原则，优先使用静态分配，周期性监控堆状态是预防内存泄漏的有效手段。

---

## 参考资料

| 文件 | 描述 |
|------|------|
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:5099-5100` | 栈溢出检测调用位置 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/stack_macros.h` | 栈溢出检测宏定义 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2870-2900` | 优先级继承/反继承实现 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/MemMang/heap_4.c` | 内存分配器实现 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/task.h:1930+` | 栈溢出钩子函数声明 |

---

*本章完*

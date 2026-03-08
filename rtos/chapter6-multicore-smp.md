# 第6章：多核SMP与系统集成

随着嵌入式系统对性能要求的不断提升，多核处理器在嵌入式领域越来越普及。FreeRTOS作为主流的嵌入式RTOS，从较新版本开始提供了SMP（对称多处理）支持。本章深入分析FreeRTOS多核SMP的实现机制、多核同步问题以及系统集成策略。

---

## 6.1 FreeRTOS SMP支持现状

### 6.1.1 FreeRTOS SMP的实现状态

#### 原理阐述

FreeRTOS从10.0.0版本开始正式引入SMP支持，FreeRTOS LTS 11.1.0版本已具备完整的多核调度能力。SMP模式下，多个CPU核心共享同一内存空间和外围设备，内核可以在任意核心上调度任务执行。

与单核调度相比，SMP面临的核心挑战包括：
- **全局资源竞争**：多个核心可能同时访问内核数据结构
- **缓存一致性**：不同核心的本地缓存需要保持同步
- **负载均衡**：合理分配任务到各核心以提高利用率

#### 源码分析

FreeRTOS通过`configNUMBER_OF_CORES`配置核心数量，定义在`tasks.c`中：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:451
PRIVILEGED_DATA TCB_t * volatile pxCurrentTCBs[ configNUMBER_OF_CORES ];
```

关键设计：每个核心维护独立的`pxCurrentTCB`指针，实现任务的核间隔离：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:491
PRIVILEGED_DATA static volatile BaseType_t xYieldPendings[ configNUMBER_OF_CORES ];
```

每个核心独立的`xYieldPendings`标志确保调度决策不会相互干扰。

#### 面试参考答案

> **问题：FreeRTOS对SMP的支持情况如何？**
>
> **回答：**
>
> FreeRTOS从10.0.0版本开始正式支持SMP，当前LTS 11.1.0版本具备完整的多核调度能力。
>
> **配置方式：**
> - `configNUMBER_OF_CORES`：设置为2、4等值启用多核
> - `configUSE_CORE_AFFINITY`：启用任务核心亲和性
> - `configRUN_MULTIPLE_PRIORITIES`：控制是否允许不同优先级的任务同时运行
>
> **当前限制：**
> - 调度器采用对称多处理模型，所有核心共享相同的优先级配置
> - 不支持非对称多处理（AMP）模式
> - 某些内核对象（如某些信号量类型）在多核环境下有限制

---

### 6.1.2 多核系统任务分配策略

#### 原理阐述

SMP调度器需要在多个核心间合理分配任务，主要策略包括：

1. **负载均衡策略**：将任务分配到负载最低的核心
2. **亲和性策略**：将任务固定在特定核心上运行
3. **优先级策略**：确保高优先级任务优先获得执行机会

FreeRTOS采用混合策略：默认情况下任务可在任意核心运行，但支持通过亲和性绑定特定核心。

#### 源码分析

核心亲和性通过TCB中的`uxCoreAffinityMask`字段管理：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:366-367
#if ( configUSE_CORE_AFFINITY == 1 )
    UBaseType_t uxCoreAffinityMask; /**< Used to link the task to certain cores. */
#endif
```

任务创建时可指定亲和性：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/task.h:1403
// 示例：将任务绑定到核心1
vTaskCoreAffinitySet( xHandle, ( 1 << 1 ) );
```

调度时的亲和性检查在`prvYieldForTask()`函数中实现：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:917-918
#if ( configUSE_CORE_AFFINITY == 1 )
    if( ( pxTCB->uxCoreAffinityMask & ( ( UBaseType_t ) 1U << ( UBaseType_t ) xCoreID ) ) != 0U )
#endif
```

#### 面试参考答案

> **问题：多核系统中的任务分配策略有哪些？FreeRTOS如何实现？**
>
> **回答：**
>
> **任务分配策略：**
>
> 1. **默认负载均衡**：任务可在任意核心运行，调度器自动选择
> 2. **核心亲和性绑定**：通过`vTaskCoreAffinitySet()`将任务绑定到特定核心
> 3. **优先级感知的抢占**：高优先级任务可抢占低优先级任务所在的核心
>
> **FreeRTOS实现方式：**
>
> ```c
> // 任务亲和性设置
> vTaskCoreAffinitySet( xTaskHandle, (1 << 0) );  // 绑定到核心0
> vTaskCoreAffinitySet( xTaskHandle, (1 << 0) | (1 << 1) );  // 绑定到核心0和1
>
> // 获取任务亲和性
> UBaseType_t mask = uxTaskCoreAffinityGet( xTaskHandle );
> ```
>
> **工程实践建议：**
> - 实时性要求高的任务绑定到特定核心，避免核间迁移带来的缓存失效
> - 需要访问特定外设的任务可考虑核心亲和性
> - 共享数据的任务尽量绑定到同一核心，减少缓存一致性开销

---

## 6.2 多核同步问题

### 6.2.1 核间通信方式

#### 原理阐述

多核系统中的核间通信（IPC）是实现协同工作的基础。主要通信方式包括：

1. **共享内存**：最直接的通信方式，但需要同步机制保护
2. **消息队列**：FreeRTOS的xQueue可在多核间共享
3. **任务通知**：轻量级的核间同步机制
4. **核间中断（IPI）**：通过硬件中断触发对方核心执行

#### 源码分析

FreeRTOS的队列支持多核共享，关键在于访问保护：

```c
// 队列创建时可指定在多核间共享
QueueHandle_t xQueueCreate( UBaseType_t uxQueueLength, UBaseType_t uxItemSize );
```

任务通知是高效的核间通信方式：

```c
// FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/task.h:2501
// 发送通知到指定任务（可跨核心）
BaseType_t xTaskNotifyIndexed( TaskHandle_t xTaskToNotify,
                                UBaseType_t uxIndexToNotify,
                                uint32_t ulValue,
                                eNotifyAction eAction );
```

#### 面试参考答案

> **问题：核间通信方式有哪些？FreeRTOS如何实现？**
>
> **回答：**
>
> **核间通信方式：**
>
> 1. **共享内存 + 互斥锁**
>    - 最基础的方式，通过全局变量共享数据
>    - 需要配合信号量/互斥锁保护访问
>
> 2. **消息队列（xQueue）**
>    - FreeRTOS队列本身支持多核共享
>    - 优点：自带同步机制，API简单
>    - 缺点：数据拷贝有一定开销
>
> 3. **任务通知（Task Notification）**
>    - 最轻量的同步方式，无需创建额外对象
>    - 支持带值通知和事件标志
>    - 适合单bit或计数场景
>
> 4. **核间中断（IPI）**
>    - 通过硬件中断机制通知另一核心
>    - 适合需要立即处理的紧急事件
>
> **使用示例：**
> ```c
> // 方式1：使用队列进行核间通信
> QueueHandle_t xIPCQueue = xQueueCreate(10, sizeof(IPCMessage_t));
> // 核心0发送
> xQueueSend(xIPCQueue, &msg, 0);
> // 核心1接收
> xQueueReceive(xIPCQueue, &msg, portMAX_DELAY);
>
> // 方式2：使用任务通知
> // 核心0通知核心1的任务
> xTaskNotifyIndexed( xTaskOnCore1, 0, 0x01, eSetBits );
> // 核心1等待通知
> ulTaskNotifyTakeIndexed( 0, pdTRUE, portMAX_DELAY );
> ```

---

### 6.2.2 缓存一致性处理

#### 原理阐述

多核系统中，每个核心拥有独立的L1缓存，共享L2/L3缓存或主内存。缓存一致性问题发生在：核心A修改了共享数据，核心B的缓存中仍是旧值。

**MESI协议**是常见的缓存一致性协议：
- **M（Modified）**：数据被修改且仅在本核心缓存中
- **E（Exclusive）**：数据仅在本核心缓存中，且未修改
- **S（Shared）**：数据在多个核心缓存中有效
- **I（Invalid）**：缓存行无效

#### 面试参考答案

> **问题：多核系统中缓存一致性如何处理？**
>
> **回答：**
>
> **缓存一致性挑战：**
>
> 多核系统中，每个核心的本地缓存可能保存同一内存地址的数据。当一个核心修改数据时，其他核心的缓存中仍是旧值，导致数据不一致。
>
> **硬件级解决方案：**
>
> 1. **MESI协议**：大多数ARM Cortex-A/R处理器采用，通过总线嗅探保持缓存一致
> 2. **硬件缓存一致性引擎**：如ARM的CCI（Cache Coherent Interconnect）
>
> **软件级解决方案：**
>
> 1. **内存屏障（Memory Barrier）**
>    - `DMB`：数据内存屏障，确保所有内存访问完成
>    - `DSB`：数据同步屏障，比DMB更强
>    - `ISB`：指令同步屏障，冲刷流水线
>
> 2. **缓存失效操作**
>    - 写入共享数据后主动失效其他核心的缓存行
>    - 使用`DCache_CleanInvalidate()`等函数
>
> **FreeRTOS中的处理：**
>
> FreeRTOS假设运行在硬件支持缓存一致性的平台上（如Cortex-A系列）。对于不支持硬件一致性的Cortex-M系列多核，需要：
>
> - 共享数据标记为Non-cachable（`__attribute__((section(".ram_d1")))`）
> - 或使用D-Cache维护操作
>
> **工程实践建议：**
>
> - 对共享数据的写操作后使用`__DMB()`内存屏障
> - 避免频繁跨核心共享大数据块
> - 使用`volatile`关键字确保编译器不优化掉访问

---

## 6.3 本章小结

本章分析了FreeRTOS多核SMP支持的核心机制：

1. **SMP支持现状**：FreeRTOS LTS 11.1.0已具备完整SMP能力，通过`configNUMBER_OF_CORES`配置核心数，支持核心亲和性绑定。

2. **任务分配策略**：默认负载均衡策略配合亲和性绑定，可通过`vTaskCoreAffinitySet()`控制任务运行的核心。

3. **核间通信**：支持队列、任务通知、核间中断等多种方式，选择依据是数据量、实时性要求和实现复杂度。

4. **缓存一致性**：依赖硬件MESI协议，软件层面需正确使用内存屏障，确保跨核访问的数据一致性。

---

## 参考资料

| 文件 | 描述 |
|------|------|
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:451` | pxCurrentTCBs多核数组定义 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:366-367` | uxCoreAffinityMask亲和性字段 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:917-918` | 亲和性检查代码 |
| `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/task.h:1357+` | 核心亲和性API声明 |

---

*本章完*

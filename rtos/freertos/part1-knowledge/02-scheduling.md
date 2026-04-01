# 调度机制 (Scheduling)

## 调度器类型 ⭐⭐⭐

FreeRTOS 支持两种调度方式，通过配置选项灵活选择。

### 抢占式调度

抢占式调度是 FreeRTOS 的默认调度方式，也是最常用的模式。

**核心机制：**
- 高优先级任务可随时抢占正在运行的低优先级任务
- 当高优先级任务进入就绪状态时，立即触发任务切换
- 保证了高优先级任务的实时响应

**时间片轮转 (Time Slicing)：**
- 同一优先级的多个任务轮流执行
- 每个任务运行一个时间片（一个 Tick 中断周期）
- 通过 configUSE_TIME_SLICING 配置（默认开启）

```c
// FreeRTOSConfig.h
#define configUSE_TIME_SLICING 1  // 开启时间片轮转
// #define configUSE_TIME_SLICING 0  // 关闭时间片轮转
```

**特点：**
- 优点：响应速度快，适合实时系统
- 缺点：可能导致优先级反转

### 协作式调度

协作式调度需要任务主动让出 CPU 控制权。

**核心机制：**
- 任务必须显式调用 vTaskYield() 主动让出
- 只有当运行中的任务主动放弃 CPU，其他任务才有机会执行
- 不会发生抢占

```c
// 协作式调度示例
void vTaskFunction(void *pvParameters)
{
    while (1) {
        // 执行一部分工作
        processData();

        // 主动让出 CPU，让其他任务有机会运行
        vTaskDelay(1);  // 或 vTaskYield()
    }
}
```

**适用场景：**
- 任务之间有明确的执行顺序要求
- 需要避免优先级反转问题
- 对任务切换开销敏感的场景
- 简单的状态机实现

**配置方式：**
```c
// FreeRTOSConfig.h
#define configUSE_PREEMPTION 0  // 禁用抢占式调度，启用协作式调度
```

## Tickless 模式 ⭐⭐⭐

Tickless 模式是 FreeRTOS 的低功耗特性，通过动态调整 Tick 中断来降低功耗。

### 原理

**传统模式的问题：**
- 即使没有任务需要执行，Tick 中断仍然周期性触发
-频繁的中断和任务切换导致不必要的功耗

**Tickless 模式工作原理：**
1. 进入空闲任务时，停止 Tick 中断
2. 根据下一个任务的唤醒时间计算休眠时长
3. 使用硬件定时器在精确时间唤醒
4. 唤醒后恢复 Tick 中断，补偿休眠期间的时间

```c
// FreeRTOSConfig.h
#define configUSE_TICKLESS_IDLE 1  // 启用 Tickless 模式
```

**关键配置：**
```c
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP 2  // 最小休眠 Tick 数
```

### 消费电子应用

**典型应用场景：**
- 智能手表、健身追踪器
- 物联网传感器节点
- 无线耳机
- 遥控器

**低功耗设计考量：**
- 延长电池使用寿命
- 降低发热
- 在深度休眠期间保持低功耗

**实现示例：**
```c
// 消费电子中的 Tickless 使用
void vApplicationIdleHook(void)
{
    // FreeRTOS 会自动进入低功耗模式
    // 醒来后继续执行空闲任务
}
```

### 汽车电子应用

**典型应用场景：**
- 汽车仪表盘
- 车载信息娱乐系统
- ADAS（高级驾驶辅助系统）控制器
- 车身电子控制单元（BCM）

**汽车电子特殊要求：**
- 启动时间优化：快速响应外部事件
- 实时性保证：在低功耗和实时响应之间取得平衡
- 可靠性：确保在各种工作条件下稳定运行

**特殊配置：**
```c
#define configTICK_RATE_HZ 100  // 汽车电子常用 100Hz 或 200Hz
```

## 调度算法

FreeRTOS 采用基于优先级的抢占式调度算法。

### 优先级队列

**优先级范围：**
```c
#define configMAX_PRIORITIES (32)  // 可配置，最大 32 级
```

**优先级分配策略：**
- 数值越大优先级越高（可配置）
- 最高优先级任务（configMAX_PRIORITIES - 1）优先执行
- 空闲任务优先级为 0

### 就绪队列链表

FreeRTOS 使用多级就绪队列管理不同优先级的任务。

**数据结构：**
```c
// 简化示意
typedef struct xLIST {
    volatile UBaseType_t uxNumberOfItems;
    ListItem_t * pxIndex;
    MiniListItem_t xListEnd;
} List_t;

// 每个优先级对应一个 List_t
List_t pxReadyTasksLists[configMAX_PRIORITIES];
```

**就绪队列管理：**
- 新任务进入就绪状态时，根据优先级插入对应链表
- 最高优先级任务位于链表头部
- 同优先级任务按 FIFO 顺序排列

### 任务切换过程

**触发条件：**
1. 高优先级任务进入就绪态（抢占）
2. 当前任务调用 vTaskDelay() 或阻塞 API
3. 时间片用完（时间片轮转）
4. 任务主动调用 vTaskYield()

**任务切换步骤：**

1. **保存当前任务上下文：**
   - 保存寄存器值到任务栈
   - 保存程序计数器 (PC)
   - 保存堆栈指针 (SP)

2. **选择下一个任务：**
   - 查找最高优先级就绪任务
   - 从就绪队列中取出该任务

3. **恢复新任务上下文：**
   - 从新任务栈恢复寄存器值
   - 更新 PSP（进程栈指针）
   - 恢复程序计数器

4. **返回新任务继续执行**

```c
// 任务切换核心函数（PendSV 中断）
void PendSV_Handler(void)
{
    // 1. 保存当前任务寄存器
    saveCurrentTaskContext();

    // 2. 更新当前任务状态
    currentTask = pxCurrentTCB;
    currentTask->pxTopOfStack = currentSP;

    // 3. 选择下一个任务
    portGET_HIGHEST_PRIORITY(uxTopPriority, uxReadyPriorities);
    listGET_OWNER_OF_NEXT_ENTRY(pxCurrentTCB, &pxReadyTasksLists[uxTopPriority]);

    // 4. 恢复新任务上下文
    restoreNewTaskContext();
}
```

## 调度相关配置总结

```c
// FreeRTOSConfig.h 调度相关配置

// 抢占式/协作式调度
#define configUSE_PREEMPTION        1

// 时间片轮转
#define configUSE_TIME_SLICING      1

// Tickless 模式
#define configUSE_TICKLESS_IDLE    1

// 最大优先级数量
#define configMAX_PRIORITIES        32

// Tick 时钟频率
#define configTICK_RATE_HZ         (1000)

// 空闲任务优先级
#define configIDLE_SHOULD_YIELD    1
```
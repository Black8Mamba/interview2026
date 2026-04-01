# 任务管理 (Task Management)

## 核心概念 ⭐⭐⭐

### 任务状态

FreeRTOS 中的任务共有四种状态：

- **Running（运行）**：任务正在执行，CPU 当前正在执行该任务的代码。在任意时刻，只能有一个任务处于运行状态。

- **Ready（就绪）**：任务具备运行条件，等待被调度器选中执行。就绪列表中的任务按优先级排列，高优先级任务优先获得 CPU 使用权。

- **Blocked（阻塞）**：任务正在等待某个事件（如信号量、队列、事件组、超时等）而无法执行。阻塞状态的任务不参与调度。

- **Suspended（挂起）**：任务被显式挂起，无法被调度器选中执行。挂起状态通常用于临时禁用某个任务。

> 状态转换图：
> ```
> Running → Ready（被更高优先级任务抢占）
> Ready → Running（被调度器选中）
> Running → Blocked（等待事件）
> Blocked → Ready（事件发生或超时）
> Running/Ready/Blocked → Suspended（调用 vTaskSuspend()）
> Suspended → Ready（调用 vTaskResume()）
> ```

### 任务创建

#### 动态创建 - xTaskCreate()

```c
BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,      // 任务函数指针
    const char * const pcName,     // 任务名称（用于调试）
    const uint32_t ulStackDepth,   // 任务栈深度（字为单位）
    void * const pvParameters,    // 传递给任务的参数
    UBaseType_t uxPriority,        // 任务优先级
    TaskHandle_t * const pxCreatedTask  // 任务句柄（输出）
);

// 返回值：pdPASS 表示成功，errQUEUE_NOT_FULL 表示失败
```

**示例：**

```c
void vTaskFunction(void *pvParameters)
{
    for(;;) {
        // 任务逻辑
    }
}

// 创建任务
TaskHandle_t xTaskHandle = NULL;
xTaskCreate(vTaskFunction, "Task1", 1024, NULL, 1, &xTaskHandle);
```

#### 静态创建 - xTaskCreateStatic()

```c
TaskHandle_t xTaskCreateStatic(
    TaskFunction_t pvTaskCode,
    const char * const pcName,
    uint32_t ulStackDepth,
    void * const pvParameters,
    UBaseType_t uxPriority,
    StackType_t * const pxStackBuffer,  // 外部提供的栈缓冲区
    StaticTask_t * const pxTaskBuffer  // 外部提供的 TCB 缓冲区
);
```

**使用场景**：静态创建适用于以下情况：
- 对内存分配有严格要求的嵌入式系统
- 需要任务栈在特定内存区域（如 TCM、SRAM）
- 避免动态内存分配带来的碎片化问题

#### 任务栈大小计算方法

任务栈大小以字（word）为单位，而不是字节。计算方法：

1. **静态分析**：分析任务函数中所有局部变量的最大深度
2. **函数调用链**：考虑所有嵌套调用的栈深度
3. **中断嵌套**：预留中断嵌套所需的栈空间
4. **安全余量**：通常增加 20%-50% 的安全边界

**计算示例：**

```c
void vTaskFunction(void *pvParameters)
{
    char buffer[256];      // 256 字
    int local_var;         // 1 字
    // 函数调用、printf 等也会消耗栈空间
}
```

建议栈深度：`256 + 函数调用开销 + 中断预留 ≈ 512 字`

> 配置项 `configCHECK_FOR_STACK_OVERFLOW` 可在运行时检测栈溢出。

### 任务删除

#### vTaskDelete()

```c
void vTaskDelete(TaskHandle_t xTaskToDelete);

// 删除自身
vTaskDelete(NULL);
```

**使用注意点：**

1. **内存回收**：被删除任务的任务控制块（TCB）和栈内存不会自动释放，除非该任务是由空闲任务创建的（这种情况下空闲任务会回收内存）。

2. **删除时机**：在任务内部调用 `vTaskDelete(NULL)` 删除自身是安全的。

3. **资源清理**：删除任务前应确保释放该任务持有的所有资源（信号量、队列、内存等），否则会导致资源泄漏。

4. **阻塞任务**：如果任务处于阻塞状态，删除操作仍会生效。

**示例：**

```c
void vTaskFunction(void *pvParameters)
{
    // ... 任务逻辑 ...

    // 清理资源
    vSemaphoreDelete(xSemaphore);

    // 删除自身
    vTaskDelete(NULL);
}
```

#### 空闲任务回收机制

FreeRTOS 始终会自动创建一个空闲任务（Idle Task），其重要作用之一就是回收被删除任务的动态内存：

- 如果任务是通过 `xTaskCreate()` 动态创建的，删除后内存由空闲任务回收
- 如果任务是通过 `xTaskCreateStatic()` 静态创建的，内存不会自动释放，需手动处理

> 配置项 `configUSE_TIME_SLICING` 控制时间片轮转调度。

### 任务优先级

#### configMAX_PRIORITIES 配置

```c
// FreeRTOSConfig.h
#define configMAX_PRIORITIES 5  // 建议 5-32 之间
```

- 优先级数值范围：0 到 (configMAX_PRIORITIES - 1)
- 数值越大，优先级越高
- 优先级 0 预留给空闲任务

#### 优先级数值与调度顺序

- 调度器始终选择最高优先级的就绪任务执行
- 同优先级任务采用时间片轮转调度（需启用 `configUSE_TIME_SLICING`）
- 低优先级任务可以被高优先级任务抢占

**调度规则：**
```
优先级 4 → 优先级 3 → 优先级 2 → 优先级 1 → 优先级 0（空闲）
  高         中         低       更低      最低
```

#### 优先级翻转问题

优先级翻转（Priority Inversion）是指高优先级任务因等待低优先级任务释放资源而被阻塞，导致中等优先级任务反而先执行的现象。

**发生场景：**
```
时间线：
T1: 优先级低的任务获取共享资源（锁）
T2: 优先级高的任务尝试获取该资源，进入阻塞
T3: 中等优先级的任务抢占 CPU 开始执行
T4: 中等优先级任务长时间运行，阻塞高优先级任务
T5: 低优先级任务释放资源
T6: 高优先级任务终于获得资源继续执行
```

**解决方案：**

1. **优先级继承（Priority Inheritance）**
   ```c
   // 创建互斥量时启用
   xSemaphoreHandle xMutex = xSemaphoreCreateMutex();
   // 当高优先级任务等待互斥量时，低优先级任务的优先级会临时提升
   ```

2. **优先级天花板（Priority Ceiling）**
   - 将资源的访问优先级提升到系统最高，避免被其他任务抢占

> 配置项 `configUSE_MUTEXES` 启用互斥量优先级继承。

---

## 消费电子面试重点

### 任务优先级如何配置

**原则：**

1. **实时性要求**：关键实时任务（如音频处理、显示刷新）分配最高优先级
2. **周期任务**：周期性任务根据周期时间分配优先级，周期越短优先级越高
3. **依赖关系**：被依赖的任务优先级应高于依赖它的任务
4. **避免频繁切换**：控制高优先级任务数量，减少上下文切换开销

**配置示例：**

```c
// FreeRTOSConfig.h
#define configMAX_PRIORITIES 5

// 任务优先级定义
#define PRIORITY_HIGHEST     4  // 音频处理等实时任务
#define PRIORITY_HIGH        3  // 显示刷新
#define PRIORITY_MEDIUM      2  // 通信处理
#define PRIORITY_LOW         1  // 日志记录
#define PRIORITY_IDLE        0  // 空闲任务（默认）

// 创建任务时指定优先级
xTaskCreate(vAudioTask, "Audio", 512, NULL, PRIORITY_HIGHEST, NULL);
xTaskCreate(vDisplayTask, "Display", 512, NULL, PRIORITY_HIGH, NULL);
```

### 任务栈大小如何估算

**方法：**

1. **静态分析**：分析代码中最大局部变量 + 函数调用深度
2. **运行时检测**：启用栈溢出检测，运行时观察栈使用情况
3. **经验估算**：根据任务复杂度给出初始值，后续调整

**实践建议：**

```c
// 简单任务：512-1024 字
// 中等任务：1024-2048 字
// 复杂任务：2048-4096 字

// 栈溢出检测配置
#define configCHECK_FOR_STACK_OVERFLOW 2  // 详细检查
#define configRECORD_STACK_HIGH_ADDRESS 1  // 记录高水位
```

---

## 汽车电子面试重点

### 任务监控机制

汽车电子系统对可靠性要求极高，需要完善的监控机制：

#### 1. 任务运行时间监控

```c
// 启用任务运行时间统计
#define configGENERATE_RUN_TIME_STATS 1

void vTaskMonitorTask(void *pvParameters)
{
    TaskStatus_t xTaskDetails;
    uint32_t ulTotalTime, ulRunTime;

    for(;;) {
        vTaskGetRunTimeStats(pcWriteBuffer);
        // 分析任务 CPU 占用时间
        printf("%s\n", pcWriteBuffer);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

#### 2. 任务状态监控

```c
void vMonitorTask(void *pvParameters)
{
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize;
    uint32_t ulTotalTasks;

    for(;;) {
        uxArraySize = uxTaskGetNumberOfTasks();
        pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

        if(pxTaskStatusArray != NULL) {
            uxArraySize = uxTaskGetSystemState(
                pxTaskStatusArray,
                uxArraySize,
                &ulTotalTime
            );

            // 检查每个任务的状态
            for(UBaseType_t i = 0; i < uxArraySize; i++) {
                if(pxTaskStatusArray[i].eCurrentState == eBlocked) {
                    // 记录阻塞任务
                }
            }
            vPortFree(pxTaskStatusArray);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

#### 3. 看门狗监控

```c
// 任务定期喂狗
void vCriticalTask(void *pvParameters)
{
    for(;;) {
        // 执行关键操作
        // ...

        // 喂狗
        wdt_feed();

        vTaskDelay(pdMS_TO_TICKS(10));  // 周期应小于看门狗超时时间
    }
}
```

### 任务故障恢复策略

#### 1. 任务超时检测

```c
#define TASK_TIMEOUT_MS 100

void vMonitorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for(;;) {
        // 检查关键任务是否超时
        if(ulTaskGetRuntime(xTaskHandle) > TASK_TIMEOUT_MS) {
            // 触发故障处理
            vTaskReset(xTaskHandle);  // 重置任务
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
    }
}
```

#### 2. 任务重启机制

```c
void vTaskRestart(TaskHandle_t xTaskHandle)
{
    TaskHandle_t xNewTaskHandle = NULL;

    // 获取原任务的创建参数（需要自行保存）
    TaskParameters_t xParams = xTaskGetParameters(xTaskHandle);

    // 删除旧任务
    vTaskDelete(xTaskHandle);

    // 重新创建任务
    xTaskCreate(
        xParams.pvTaskCode,
        xParams.pcName,
        xParams.ulStackDepth,
        xParams.pvParameters,
        xParams.uxPriority,
        &xNewTaskHandle
    );
}
```

#### 3. 分层故障处理

```c
typedef enum {
    FAULT_LEVEL_WARNING,      // 警告：记录日志
    FAULT_LEVEL_ERROR,       // 错误：尝试恢复
    FAULT_LEVEL_CRITICAL     // 严重：系统复位
} FaultLevel_e;

void vFaultHandler(FaultLevel_e level)
{
    switch(level) {
        case FAULT_LEVEL_WARNING:
            log_warning("Task fault detected");
            break;

        case FAULT_LEVEL_ERROR:
            // 尝试重启任务
            vTaskRestart(xFaultTaskHandle);
            break;

        case FAULT_LEVEL_Critical:
            // 记录故障后系统复位
            log_error("Critical fault - system reset");
            system_reset();
            break;
    }
}
```

---

## 常见面试问题

1. **任务和协程的区别？**
   - 任务有独立的栈，协程共享栈
   - 任务可以抢占，协程协作式调度

2. **空闲任务的作用？**
   - 回收内存
   - 运行系统空闲钩子

3. **如何选择动态创建和静态创建？**
   - 动态创建：简单快速，内存灵活
   - 静态创建：确定性更强，适合实时系统

4. **任务优先级如何设计？**
   - 按实时性要求分配
   - 关键任务最高优先级
   - 同优先级任务时间片轮转
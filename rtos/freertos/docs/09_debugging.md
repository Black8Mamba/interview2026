# 第九章：调试方案

> 本章目标：掌握 FreeRTOS 常见问题调试方法、内核状态诊断、内存问题排查

## 章节结构

- [ ] 9.1 调试配置
- [ ] 9.2 常见崩溃分析
- [ ] 9.3 内核状态诊断
- [ ] 9.4 内存问题排查
- [ ] 9.5 优先级反转检测
- [ ] 9.6 运行时诊断工具
- [ ] 9.7 调试技巧与经验
- [ ] 9.8 面试高频问题
- [ ] 9.9 避坑指南

---

## 9.1 调试配置

### 开启调试功能

```c
// FreeRTOSConfig.h
#define configDEBUG_44_HCZ           1

// 栈溢出检测（级别 1 或 2）
#define configCHECK_FOR_STACK_OVERFLOW    2

// 堆调试（内存分配检查）
#define configUSE_MALLOC_FAILED_HOOK    1
#define configUSE_IDLE_HOOK             1
```

### 栈溢出钩子

```c
// 栈溢出时会调用此函数
void vApplicationStackOverflowHook(
    TaskHandle_t xTask,
    char *pcTaskName
) {
    // 记录日志：哪个任务栈溢出了
    printf("Stack overflow in task: %s\r\n", pcTaskName);

    // 打印寄存器状态（调试用）
    // 进入死循环或复位
    for(;;);
}
```

### 内存分配失败钩子

```c
void vApplicationMallocFailedHook(void) {
    // 堆内存耗尽时调用
    printf("Malloc failed! Increase configTOTAL_HEAP_SIZE\r\n");

    // 可记录哪个任务/队列创建失败
    for(;;);
}
```

### 空闲任务钩子

```c
void vApplicationIdleHook(void) {
    // 可进入低功耗模式
    // 或处理后台任务
    __WFI();  // Wait For Interrupt
}
```

---

## 9.2 常见崩溃分析

### 1. HardFault 硬故障

**典型症状：**
- 系统立即死机
- 调试器停在 HardFault_Handler

**常见原因：**
- 栈溢出（写入超出栈范围）
- 非法内存访问（NULL 指针、越界）
- 除零错误
- 非法指令

**分析方法：**

```gdb
// GDB 调试
(gdb) info registers

// 查看栈回溯
(gdb) bt

// 查看当前地址
(gdb) x/i $pc

// 查看内存
(gdb) x/32x 0x20000000
```

**CFSR 寄存器分析 HardFault 类型：**

```c
// 在 HardFault_Handler 中读取 CFSR
uint32_t CFSR = SCB->CFSR;

if (CFSR & 0x00000002) {
    // UFSR: undefined instruction
} else if (CFSR & 0x00000100) {
    // BFSR: bus fault
} else if (CFSR & 0x00008000) {
    // MMFSR: memory management fault (栈溢出)
}
```

### 2. 看门狗复位

**判断方法：**
- 系统周期性地重启
- 可通过 RCC_CSR 判断复位原因

```c
if (RCC->CSR & RCC_CSR_IWDGRSTF) {
    // 看门狗复位
    printf("Watchdog reset\r\n");
    RCC->CSR |= RCC_CSR_RMVF;  // 清除标志
}
```

### 3. 任务死循环

**症状：**
- 系统不崩溃但无响应
- 高优先级任务占用 CPU 不释放

**排查方法：**

```c
// 在高优先级任务中添加延时
for (;;) {
    DoWork();
    vTaskDelay(pdMS_TO_TICKS(10));  // 让出 CPU
}
```

### 4. 优先级反转

**症状：**
- 高优先级任务响应慢
- 系统整体响应不稳定

**排查：**
- 观察任务执行顺序
- 使用 Tracer 或 SystemView 工具

---

## 9.3 内核状态诊断

### 任务列表查询

```c
// 获取任务数量
UBaseType_t taskCount = uxTaskGetNumberOfTasks();

// 获取任务信息
TaskStatus_t *pxTaskStatusArray;
UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

// 获取所有任务状态
uxArraySize = uxTaskGetSystemState(
    pxTaskStatusArray,    // 输出数组
    uxArraySize,         // 数组大小
    NULL                 // 不关心总运行时间
);

// 打印任务信息
for (UBaseType_t i = 0; i < uxArraySize; i++) {
    printf("Task: %s, Priority: %u, State: %d\r\n",
        pxTaskStatusArray[i].pcTaskName,
        pxTaskStatusArray[i].uxCurrentPriority,
        pxTaskStatusArray[i].eCurrentState);
}

vPortFree(pxTaskStatusArray);
```

### 任务状态枚举

```c
typedef enum {
    eRunning = 0,    // 运行中
    eReady,          // 就绪
    eBlocked,       // 阻塞
    eSuspended,      // 挂起
    eDeleted         // 已删除
} eTaskState;
```

### 获取任务句柄

```c
// 通过名称获取任务句柄
TaskHandle_t handle = xTaskGetHandle("TaskName");
if (handle == NULL) {
    // 任务不存在
}

// 获取当前任务句柄
TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();

// 获取任务名称
char *name = pcTaskGetTaskName(currentTask);
```

### 获取任务栈水位

```c
// 获取任务栈最小剩余空间
UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);

// 周期性检查
void MonitorTask(void *pv) {
    for (;;) {
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
        printf("Stack HWM: %u words\r\n", hwm);

        // 如果低于阈值，报警
        if (hwm < 50) {
            // 栈快用尽了
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 获取 CPU 使用率

```c
// 需要启用 configUSE_STATS_FORMATTING_FUNCTIONS
void PrintCPUUsage(void) {
    char *buf = pvPortMalloc(500);
    if (buf) {
        // 获取 CPU 使用率统计
        vTaskGetRunTimeStats(buf);
        printf("%s", buf);  // 打印各任务 CPU 时间
        vPortFree(buf);
    }
}

// configUSE_STATS_FORMATTING_FUNCTIONS 需要计时器支持
```

---

## 9.4 内存问题排查

### 堆内存状态

```c
// 获取当前剩余堆大小
size_t freeHeap = xPortGetFreeHeapSize();
printf("Free heap: %u bytes\r\n", freeHeap);

// 获取历史最低剩余
size_t minFree = xPortGetMinimumEverFreeHeapSize();
printf("Min ever free heap: %u bytes\r\n", minFree);
```

### 内存泄漏检测

**方法一：定期检查剩余堆**

```c
void MemoryMonitorTask(void *pv) {
    size_t lastFree = xPortGetFreeHeapSize();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        size_t currentFree = xPortGetFreeHeapSize();
        size_t delta = lastFree - currentFree;

        if (delta > 100) {  // 5秒内减少超过100字节
            printf("Possible memory leak: %u bytes\r\n", delta);
        }

        lastFree = currentFree;
    }
}
```

**方法二：标记计数**

```c
// 在分配处添加日志
void *MyMalloc(size_t size, const char *file, int line) {
    void *ptr = pvPortMalloc(size);
    printf("Malloc: %s:%d, ptr=%p, size=%u\r\n", file, line, ptr, size);
    return ptr;
}

#define pvPortMalloc(s) MyMalloc(s, __FILE__, __LINE__)
```

### 内存碎片化

```c
// 检查碎片化：分配大块内存看是否成功
void DefragmentationTest(void) {
    const size_t testSize = 1024;
    void *ptr1 = pvPortMalloc(testSize);
    void *ptr2 = pvPortMalloc(testSize);
    void *ptr3 = pvPortMalloc(testSize);

    vPortFree(ptr2);  // 释放中间块

    // 尝试分配更大的块
    void *ptr4 = pvPortMalloc(testSize * 2);  // 如果失败，说明碎片化

    vPortFree(ptr1);
    vPortFree(ptr3);
    vPortFree(ptr4);
}
```

### 栈溢出诊断

```c
// 方法1：使用钩子函数（前面已讲）

// 方法2：检查栈内容（canary value）
void CheckStack(TaskHandle_t task) {
    StackType_t *stackStart = *((StackType_t **)task);  // 需根据TCB结构

    // 检查栈末尾的 canary 值是否被破坏
    // canary 通常设置为 0xA5A5A5A5
    uint32_t *stackEnd = (uint32_t *)((uint32_t)stackStart - 栈大小);

    if (stackEnd[栈字数-1] != 0xA5A5A5A5) {
        printf("Stack overflow detected!\r\n");
    }
}
```

---

## 9.5 优先级反转检测

### 运行时观察

```c
// 周期性打印任务状态
void TaskMonitorTask(void *pv) {
    TaskStatus_t *status;
    uint32_t totalTime;

    for (;;) {
        status = pvPortMalloc(uxTaskGetNumberOfTasks() * sizeof(TaskStatus_t));

        uxTaskGetSystemState(status, uxTaskGetNumberOfTasks(), &totalTime);

        for (UBaseType_t i = 0; i < uxTaskGetNumberOfTasks(); i++) {
            printf("Task: %-10s Prio: %2u State: %d Time: %lu%%\r\n",
                status[i].pcTaskName,
                status[i].uxCurrentPriority,
                status[i].eCurrentState,
                (status[i].ulRunTimeCounter * 100) / totalTime);
        }

        vPortFree(status);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

### 优先级继承观察

```c
// 当使用互斥量时可以观察优先级变化
void MutexUserTask(void *pv) {
    printf("Task %s initial priority: %u\r\n",
        pcTaskGetTaskName(NULL),
        uxTaskPriorityGet(NULL));

    xSemaphoreTake(xMutex, portMAX_DELAY);

    // 获取互斥量后，优先级可能已被提升
    printf("Task %s priority after mutex take: %u\r\n",
        pcTaskGetTaskName(NULL),
        uxTaskPriorityGet(NULL));

    xSemaphoreGive(xMutex);
}
```

---

## 9.6 运行时诊断工具

### FreeRTOS+Trace（官方工具）

- **功能：** 记录任务调度、信号量、队列等事件
- **输出：** 可视化时间线图
- **适用：** 复杂多任务调试

```c
// 使用示例
#include "trcRecorder.h"

vTraceStart();  // 开始录制

// ... 运行一段时间 ...

vTraceStop();   // 停止录制
// 在 PC 端查看分析
```

### SEGGER SystemView

- **功能：** 实时记录 FreeRTOS 事件
- **优点：** 实时性强，无需停止系统

```c
// 初始化
SEGGER_SYSVIEW_Conf();  // 在 main() 中调用

// 添加用户事件
SEGGER_SYSVIEW_Printf("Task %s started", pcTaskName);
```

### J-Link RTT

```c
#include "SEGGER_RTT.h"

// 打印调试信息
SEGGER_RTT_WriteString(0, "Hello FreeRTOS\r\n");
```

### 自定义调试队列

```c
typedef struct {
    uint8_t type;
    uint32_t timestamp;
    void *data;
} DebugEvent_t;

QueueHandle_t xDebugQueue;

void Debug_Init(void) {
    xDebugQueue = xQueueCreate(100, sizeof(DebugEvent_t));
}

void Debug_Log(uint8_t type, void *data) {
    DebugEvent_t evt = {
        .type = type,
        .timestamp = xTaskGetTickCount(),
        .data = data
    };
    xQueueSend(xDebugQueue, &evt, 0);  // 非阻塞
}

void Debug_Task(void *pv) {
    DebugEvent_t evt;
    for (;;) {
        if (xQueueReceive(xDebugQueue, &evt, portMAX_DELAY) == pdPASS) {
            // 处理并打印日志
            printf("[%lu] Event %u\r\n", evt.timestamp, evt.type);
        }
    }
}
```

---

## 9.7 调试技巧与经验

### 调试时禁用优化

```c
// Keil / ARM GCC
// 编译优化级别设为 O0，否则调试时变量值不可靠

// __attribute__((optimize("O0")))
void DebugFunction(void) __attribute__((optimize("O0")));
```

### 使用 ITM 打印

```c
// ARM Cortex-M 的 ITM 调试输出，比 UART 更快
#define ITM_Port32(*((volatile unsigned *)0xE0000000))

void ITM_SendString(char *str) {
    while (*str) {
        ITM_SendChar(*str++);
    }
}

// 使用：
ITM_SendString("Debug message\r\n");
```

### 任务局部变量观察

```c
// GDB 调试时查看任务栈
(gdb) info threads    // 列出所有任务
(gdb) thread N        // 切换到任务 N
(gdb) bt              // 查看栈回溯
(gdb) p localVar      // 查看局部变量
```

### 常见调试检查点

```c
// 1. 检查调度器是否启动
// printf 放在 vTaskStartScheduler() 之后

// 2. 检查任务是否创建成功
BaseType_t result = xTaskCreate(...);
configASSERT(result == pdPASS);  // 断言

// 3. 检查队列/信号量是否创建成功
QueueHandle_t q = xQueueCreate(...);
configASSERT(q != NULL);

// 4. 中断中检查返回值
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueSendFromISR(..., &xHigherPriorityTaskWoken);
configASSERT(xHigherPriorityTaskWoken == pdFALSE || xHigherPriorityTaskWoken == pdTRUE);
```

### 核心转储（Live Coredump）

```c
// 当系统崩溃时保存状态
void SaveCoreDump(void) {
    // 保存寄存器
    uint32_t registers[16];
    __asm volatile (
        "mov %0, r0\n" "mov %1, r1\n" /* ... */
        : "=r"(registers[0]), "=r"(registers[1]) /* ... */
    );

    // 保存当前 TCB
    TaskHandle_t current = xTaskGetCurrentTaskHandle();

    // 写入 Flash 或发送出去
    Flash_Write((uint8_t *)registers, sizeof(registers));
}
```

---

## 9.8 面试高频问题

### Q1：FreeRTOS 如何检测栈溢出？

**参考答案：**
- 开启 `configCHECK_FOR_STACK_OVERFLOW`（1 或 2）
- 级别 1：检查栈指针是否超出范围
- 级别 2：额外检查栈末尾的 canary 值
- 实现 `vApplicationStackOverflowHook()` 钩子

---

### Q2：FreeRTOS 内存泄漏如何排查？

**参考答案：**
- 定期检查 `xPortGetFreeHeapSize()`
- 对比不同时间点的剩余堆大小
- 标记计数法：每次分配记录
- 使用工具：FreeRTOS+Trace、Valgrind（模拟环境）

---

### Q3：如何排查系统死机问题？

**参考答案步骤：**
1. 判断是 HardFault 还是看门狗复位
2. 检查 CFSR 寄存器确定异常类型
3. 查看栈回溯定位崩溃位置
4. 检查中断是否正常执行
5. 检查任务优先级配置
6. 使用 Tracer 工具观察任务调度

---

### Q4：如何分析 CPU 使用率？

**参考答案：**
- 使用 `vTaskGetRunTimeStats()` 获取各任务运行时间
- 计算各任务占用百分比
- 高 CPU 占用的任务可能是死循环
- 空闲任务 CPU 占用低说明系统负载高

---

### Q5：FreeRTOS 有什么调试工具？

**参考答案：**
- **FreeRTOS+Trace**：任务调度可视化
- **SEGGER SystemView**：实时事件录制
- **J-Link RTT**：高速调试输出
- **ITM/SWO**：ARM Cortex-M 调试接口
- **GDB**：通用调试器，配合 OpenOCD

---

## 9.9 避坑指南

1. **调试时关闭编译器优化** — O0 级别变量值才可靠
2. **ITM 输出比 UART 更快** — 但需要调试器支持
3. **断言比返回值检查更早发现问题** — 善用 `configASSERT()`
4. **定期检查栈水位** — 宁可多分配栈也不可溢出
5. **内存问题优先查泄漏** — 泄漏是渐进式的，碎片化是突发性的
6. **SystemView 是实时调试利器** — 不停止系统即可观察

---

## 9.10 GDB 调试 FreeRTOS 完整命令参考

### 9.10.1 任务 inspection 命令

```gdb
# ===== 任务列表相关命令 =====

# 列出所有线程（任务）
info threads

# 显示当前线程
info threads

# 所有线程的栈帧信息（包含优先级、状态）
thread apply all info frame

# 切换到特定任务上下文
thread N

# 查看当前任务的详细信息
info frame

# ===== 栈回溯命令 =====

# 打印完整栈回溯（backtrace）
bt

# 打印所有线程的栈回溯
thread apply all bt

# 跳转到栈帧 N
frame N

# 查看当前栈帧
info frame

# 查看局部变量
info locals

# ===== 寄存器命令 =====

# 显示所有 CPU 寄存器
info registers

# 显示特定寄存器（如 R0）
print $r0

# 显示进程栈指针
print $psp

# 显示主栈指针
print $msp

# 显示程序计数器
print $pc

# 显示程序状态寄存器
print $xPSR

# ===== 内存查看命令 =====

# 以字节形式显示内存（32 字节）
x/32x 0x20000000

# 以字（4 字节）形式显示内存
x/16w 0x20000000

# 以指令形式显示内存
x/16i 0x08000000

# 显示特定地址的内存为特定类型
x/4hx 0x20000000    # 显示 4 个半字（2 字节）

# ===== FreeRTOS TCB 检查 =====

# 查看当前 TCB 指针
print pxCurrentTCB

# 查看当前任务名称
print (char*)pxCurrentTCB->pcTaskName

# 查看当前任务优先级
print pxCurrentTCB->uxPriority

# 查看任务栈基地址
print pxCurrentTCB->pxStack

# 查看任务栈顶地址
print pxCurrentTCB->pxTopOfStack

# 查看任务栈深度（字数）
print pxCurrentTCB->uxStackDepth

# 计算栈使用量（栈向下增长）
# 如果 pxTopOfStack 远低于 pxStack，说明使用了大量栈
print (uint32_t)pxCurrentTCB->pxStack - (uint32_t)pxCurrentTCB->pxTopOfStack

# ===== 就绪队列检查 =====

# 查看就绪队列数组
print xReadyTasks

# 查看最高就绪优先级
print xReadyTasks.uxTopReadyPriority

# 遍历就绪队列（需要知道内部结构）
print xReadyTasks.xGenericListItem

# 查看挂起就绪列表
print uxCurrentPendingList

# ===== 阻塞列表检查 =====

# 查看延时列表
print xDelayedTaskList1
print xDelayedTaskList2

# 查看暂停任务列表
print xSuspendedTaskList

# ===== 事件组检查 =====

# 查看事件组位掩码
print xEventGroups

# ===== 队列和信号量检查 =====

# 查看队列结构（需要类型信息）
print *(Queue_t)QueueHandle

# 查看信号量计数
print *(SemaphoreHandle_t)SemaphoreHandle
```

### 9.10.2 断点命令

```gdb
# ===== 基础断点 =====

# 在函数处设置断点
break xTaskCreate
break vTaskDelete
break vTaskDelay
break vTaskSwitchContext

# 在指定地址设置断点
break *0x08001234

# 在指定文件和行设置断点
break main.c:123

# ===== 条件断点 =====

# 在任务创建时中断（停在 N 行时）
break xTaskCreate
condition $bpnum taskHandle != NULL

# 当特定任务调用 vTaskDelay 时中断
break vTaskDelay
condition $bpnum pxCurrentTCB->pcTaskName == "MyTask"

# 当堆内存低于阈值时中断
break vApplicationMallocFailedHook
condition $bpnum 1  # 总是中断

# ===== 看门狗断点 =====

# 在栈溢出钩子中断
break vApplicationStackOverflowHook

# 在内存分配失败钩子中断
break vApplicationMallocFailedHook

# ===== 硬件断点（Flash 调试）=====

# 硬件断点（最多 6-8 个）
hbreak vTaskSwitchContext

# 读取断点
rbreak .*TaskCreate.*  # 正则匹配

# ===== 临时断点 =====

# 只生效一次的断点
tbreak xTaskCreate

# ===== 禁用/启用断点 =====

# 禁用断点 N
disable N

# 启用断点 N
enable N

# 删除断点 N
delete N

# 列出所有断点
info breakpoints
```

### 9.10.3 内存观察点命令

```gdb
# ===== 内存观察点 =====

# 监视内存地址变化（任何写入）
watch *(uint32_t*)0x20000000

# 监视特定变量的变化
watch pxCurrentTCB

# 监视栈区域（检测栈溢出）
watch *(uint32_t*)((char*)pxCurrentTCB->pxStack + uxStackDepth*4 - 4)

# 读取监视（内存被读取时中断）
rwatch *(uint32_t*)0x20000000

# 访问监视（读取或写入时中断）
awatch *(uint32_t*)0x20000000

# ===== 变更监视 =====

# 打印变量旧值和新值
watch pxCurrentTCB
set pagination off
set print history on

# 在 GDB 脚本中记录变化
define watch-change
    watch $arg0
    commands
        silent printf "Value changed to: %p\n", $arg0
        continue
    end
end
```

### 9.10.4 单步调试命令

```gdb
# ===== 单步执行 =====

# 单步执行一条 C 语句（不进入函数）
next
n

# 单步执行一条指令
nexti
ni

# 单步执行（进入函数）
step
s

# 单步执行一条指令（进入函数）
stepi
si

# 继续执行（直到下一个断点）
continue
c

# 继续执行直到返回当前函数
finish

# 继续执行直到到达指定行
until

# 继续执行直到到达指定位置
until 100

# ===== 跳过信号/异常 =====

# 遇到信号时不停止
handle SIGSEGV noprint nostop

# 忽略所有异常
handle all nostop noprint
```

### 9.10.5 GDB 自动化脚本

```gdb
# ===== .gdbinit FreeRTOS 专用脚本 =====

# 放在项目根目录或家目录的 .gdbinit 文件

# 启用漂亮打印
set print pretty on

# 每行打印一个结构体成员
set print union on

# 显示数组内容
set print array on
set print array-indexes on

# 自动加载 FreeRTOS 类型定义
set demangle-style gnu-v3

# ===== 自定义 FreeRTOS 信息命令 =====

define freertos-info
    set print pretty on
    printf "=== FreeRTOS 系统状态 ===\n"
    printf "当前任务: %s\n", pxCurrentTCB->pcTaskName
    printf "当前优先级: %d\n", pxCurrentTCB->uxPriority
    printf "栈基地址: %p\n", pxCurrentTCB->pxStack
    printf "栈顶地址: %p\n", pxCurrentTCB->pxTopOfStack
    printf "栈深度: %u 字\n", pxCurrentTCB->uxStackDepth
    printf "最高就绪优先级: %d\n", xReadyTasks.uxTopReadyPriority
    printf "剩余堆内存: %u 字节\n", xPortGetFreeHeapSize()
    printf "历史最低剩余堆: %u 字节\n", xPortGetMinimumEverFreeHeapSize()
end
document freertos-info
    打印 FreeRTOS 系统状态信息
end

# 快捷命令
define fi
    freertos-info
end
document fi
    freertos-info 的简写
end

# ===== 任务列表命令 =====

define task-list
    set print pretty on
    printf "=== 任务列表 ===\n"
    printf "%-16s %-8s %-10s %-10s\n", "任务名", "优先级", "状态", "栈高水位"
    # 遍历 xReadyTasks 等列表
    # 具体实现依赖于 FreeRTOS 版本
end
document task-list
    列出所有 FreeRTOS 任务
end

# ===== 栈检查命令 =====

define check-stack
    printf "=== 栈检查 ===\n"
    printf "pxCurrentTCB: %p\n", pxCurrentTCB
    printf "当前任务栈: %p - %p\n", pxCurrentTCB->pxStack, pxCurrentTCB->pxTopOfStack
    # 检查 canary 值
    set $stack_end = (uint32_t*)((char*)pxCurrentTCB->pxStack + pxCurrentTCB->uxStackDepth*4)
    set $canary = *($stack_end - 1)
    if $canary == 0xA5A5A5A5
        printf "栈 canary 值正常: 0x%08X\n", $canary
    else
        printf "警告: 栈 canary 值被破坏: 0x%08X\n", $canary
    end
end
document check-stack
    检查当前任务栈状态
end

# ===== HardFault 分析命令 =====

define analyze-hardfault
    printf "=== HardFault 分析 ===\n"
    # 读取 CFSR
    set $cfsr = *(uint32_t*)0xE000ED28
    printf "CFSR = 0x%08X\n", $cfsr

    # HFSR
    set $hfsr = *(uint32_t*)0xE000ED2C
    printf "HFSR = 0x%08X\n", $hfsr

    # DFSR
    set $dfsr = *(uint32_t*)0xE000ED30
    printf "DFSR = 0x%08X\n", $dfsr

    # MMFAR
    set $mmfar = *(uint32_t*)0xE000ED34
    printf "MMFAR = 0x%08X\n", $mmfar

    # BFAR
    set $bfar = *(uint32_t*)0xE000ED38
    printf "BFAR = 0x%08X\n", $bfar

    printf "\n--- 错误类型 ---\n"
    if $cfsr & 0x00008000
        printf "MMFSR: 内存管理故障 (MMAR 有效)\n"
    end
    if $cfsr & 0x00000100
        printf "BFSR: 总线故障\n"
    end
    if $cfsr & 0x00000002
        printf "UFSR: 未定义指令\n"
    end
end
document analyze-hardfault
    分析 HardFault 状态寄存器
end

# ===== 内存块填充命令 =====

define fill-stack
    set $addr = $arg0
    set $size = $arg1
    set $pattern = $arg2
    set $i = 0
    while $i < $size
        set *((uint32_t*)($addr + $i * 4)) = $pattern
        set $i = $i + 1
    end
    printf "已填充 %u 个字 (0x%08X) 从 %p 开始\n", $size, $pattern, $addr
end
document fill-stack
    用指定模式填充内存区域
end
```

### 9.10.6 GDB 会话示例

```gdb
# ===== 完整调试会话示例 =====

# 启动 GDB 并加载符号
arm-none-eabi-gdb firmware.elf

# 连接 JTAG/SWD 调试器
target remote localhost:3333

# 加载程序到 Flash
load

# 重置目标
monitor reset halt

# 设置断点
break HardFault_Handler
break vTaskSwitchContext

# 继续执行
continue

# 等待断点触发后：

# 1. 查看寄存器
info registers

# 2. 查看栈回溯
bt

# 3. 查看当前 FreeRTOS 状态
freertos-info

# 4. 检查栈溢出
check-stack

# 5. 单步调试
next
next
step

# 6. 继续到下一个断点
continue

# ===== 典型崩溃分析 =====

# 当停在 HardFault 时：
(kgdb) info registers
(kgdb) bt                    # 栈回溯
(kgdb) analyze-hardfault     # 分析 fault 寄存器
(kgdb) x/16i $pc             # 查看 PC 处指令
(kgdb) print *(TaskHandle_t)pxCurrentTCB  # 查看 TCB

# ===== 任务切换调试 =====

# 在任务切换处设置断点
break vTaskSwitchContext

# 继续运行
continue

# 每次断点停下时查看当前任务
(kgdb) print (char*)pxCurrentTCB->pcTaskName
(kgdb) print pxCurrentTCB->uxPriority

# 继续
continue
```

---

## 9.11 J-Link RTT 调试详解

### 9.11.1 RTT 简介

SEGGER J-Link RTT（Real Time Transfer）是一种高速实时传输技术：
- 速度比 SWO 快 3-9 倍
- 非阻塞发送，不影响实时性能
- 支持多个通道（Channel 0-7）
- 双向通信，可发送命令到目标

### 9.11.2 RTT 初始化配置

```c
// 方式一：在 main() 初始化时自动配置
#include "SEGGER_RTT.h"

int main(void) {
    // 配置上行通道（发送到主机）
    // 通道 0: 默认终端
    // 缓冲大小 0 表示自动分配
    SEGGER_RTT_ConfigUpBuffer(0, "RTTUP", NULL, 0,
                               SEGGER_RTT_MODE_NO_BLOCK_TRIM);

    // 配置下行通道（从主机接收）
    SEGGER_RTT_ConfigDownBuffer(0, "RTTDOWN", NULL, 0,
                                 SEGGER_RTT_MODE_NO_BLOCK_TRIM);

    printf("System Started\n");
    // 或使用：
    // SEGGER_RTT_WriteString(0, "System Started\r\n");

    // 启动调度器
    vTaskStartScheduler();

    for(;;);
}

// 方式二：手动初始化
void RTT_Debug_Init(void) {
    // 确保 SEGGER_RTT.h 已包含
    // SEGGER_RTT_Conf.h 确保配置正确

    // 发送启动消息
    SEGGER_RTT_WriteString(0, "=== RTT Debug Initialized ===\r\n");
    SEGGER_RTT_WriteString(0, "Build: " __DATE__ " " __TIME__ "\r\n");
}
```

### 9.11.3 RTT 打印函数详解

```c
// ===== 非阻塞打印（推荐） =====

// 最基本的字符串打印 - 非阻塞
// 如果缓冲区满，新数据会被丢弃
SEGGER_RTT_WriteString(0, "Debug message\r\n");

// 阻塞直到发送完成（不推荐在中断中使用）
SEGGER_RTT_WriteString(SEGGER_RTT_MODE_BLOCK_IF_HOST_FULL,
                        "Blocking message\r\n");

// 自动换行模式
SEGGER_RTT_WriteString(SEGGER_RTT_MODE_WRITE_NO_BLOCK,
                        "Non-blocking message\r\n");

// ===== 格式化打印 =====

// SEGGER_RTT_Printf - 支持格式化输出
// 注意：不是标准 printf，支持的格式有限

// 整数打印
SEGGER_RTT_Printf(0, "Value: %d\r\n", 42);           // 十进制整数
SEGGER_RTT_Printf(0, "Hex: 0x%X\r\n", 255);          // 十六进制
SEGGER_RTT_Printf(0, "Unsigned: %u\r\n", 100);       // 无符号
SEGGER_RTT_Printf(0, "Long: %ld\r\n", 123456L);      // 长整型

// 字符串打印
SEGGER_RTT_Printf(0, "String: %s\r\n", "hello");     // 字符串
SEGGER_RTT_Printf(0, "Char: %c\r\n", 'A');           // 字符

// 指针打印
SEGGER_RTT_Printf(0, "Pointer: %p\r\n", (void*)0x20000000);

// 多参数
SEGGER_RTT_Printf(0, "Int=%d, Hex=0x%X, Str=%s\r\n",
                  42, 0x2A, "test");

// 宽度指定
SEGGER_RTT_Printf(0, "%05d\r\n", 42);    // 输出: 00042
SEGGER_RTT_Printf(0, "%8s\r\n", "Hi");   // 右对齐

// ===== 二进制数据打印 =====

// 以十六进制 dump 内存
void RTT_PrintHexDump(void *addr, int len) {
    uint8_t *p = (uint8_t*)addr;
    char buffer[128];
    int idx = 0;

    for (int i = 0; i < len; i++) {
        idx += sprintf(&buffer[idx], "%02X ", p[i]);
        if ((i + 1) % 16 == 0) {
            buffer[idx] = '\0';
            SEGGER_RTT_WriteString(0, buffer);
            SEGGER_RTT_WriteString(0, "\r\n");
            idx = 0;
        }
    }
    if (idx > 0) {
        buffer[idx] = '\0';
        SEGGER_RTT_WriteString(0, buffer);
        SEGGER_RTT_WriteString(0, "\r\n");
    }
}

// ===== 彩色打印（终端支持时） =====

// J-Link RTT Viewer 支持 ANSI 颜色
SEGGER_RTT_WriteString(0, "\033[1;31m");  // 红色粗体
SEGGER_RTT_WriteString(0, "Error: xxx\r\n");
SEGGER_RTT_WriteString(0, "\033[0m");     // 重置

// 颜色代码
// 0 = 重置
// 1 = 粗体
// 30-37 = 前景色（黑红绿黄蓝紫青白）
// 40-47 = 背景色
```

### 9.11.4 RTT 通道使用

```c
// ===== 多个通道的使用 =====

// 通道 0: 终端输出（默认）
// 通道 1: 实时数据（高频）
// 通道 2: 日志输出
// 通道 3: 命令输入

// 配置不同通道
SEGGER_RTT_ConfigUpBuffer(1, "Data", NULL, 1024,
                           SEGGER_RTT_MODE_NO_BLOCK_TRIM);
SEGGER_RTT_ConfigUpBuffer(2, "Log", NULL, 512,
                           SEGGER_RTT_MODE_BLOCK_IF_HOST_FULL);

// 发送到不同通道
SEGGER_RTT_WriteString(0, "Terminal output\r\n");
SEGGER_RTT_WriteString(1, "Data channel\r\n");  // 高频数据
SEGGER_RTT_WriteString(2, "Log message\r\n");   // 日志

// ===== RTT 读取（从主机接收） =====

// 检查是否有数据可读
int bytes = SEGGER_RTT_HasData(0);
if (bytes > 0) {
    char buffer[64];
    int read = SEGGER_RTT_Read(0, buffer, sizeof(buffer));
    // 处理接收到的数据
}

// 阻塞读取
char cmd[32];
SEGGER_RTT_Read(0, cmd, sizeof(cmd));

// ===== RTT 控制台交互 =====

// 实现简单的命令行接口
void RTT_ProcessCommand(char *cmd) {
    if (strcmp(cmd, "status") == 0) {
        SEGGER_RTT_Printf(0, "Free heap: %u\r\n",
                          xPortGetFreeHeapSize());
    } else if (strcmp(cmd, "tasks") == 0) {
        // 打印任务列表
    } else if (strcmp(cmd, "help") == 0) {
        SEGGER_RTT_WriteString(0, "Commands: status, tasks, help\r\n");
    }
}

void RTT_CommandTask(void *arg) {
    char buffer[64];
    int idx = 0;

    SEGGER_RTT_WriteString(0, "RTT Console Ready\r\n> ");

    for (;;) {
        int c = SEGGER_RTT_GetKey();  // 非阻塞获取字符
        if (c > 0) {
            if (c == '\r' || c == '\n') {
                buffer[idx] = '\0';
                RTT_ProcessCommand(buffer);
                idx = 0;
                SEGGER_RTT_WriteString(0, "> ");
            } else if (idx < sizeof(buffer) - 1) {
                buffer[idx++] = c;
                SEGGER_RTT_WriteString(0, (char*)&c);  // 回显
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 9.11.5 J-Link Commander RTT 命令

```bash
# ===== J-Link Commander 中的 RTT 命令 =====

# 连接时自动启用 RTT
JLink.exe -device STM32F407VG -if SWD -speed 4000

# 在 J-Link Commander 中：
# 启动 RTT
rtt start

# 停止 RTT
rtt stop

# 选择通道
rtt channel 0

# 查找 RTT 块
rtt scrollback 1000

# 配置通道
rtt cfgbuf 0 4096

# 清除终端
cls

# 打印字符串
print "Hello"

# ===== J-Link RTT Viewer 工具 =====

# 使用 RTT Viewer 的优势：
# - 支持多个通道同时显示
# - 支持保存日志
# - 支持搜索和过滤
# - 支持时间戳
```

---

## 9.12 SEGGER SystemView 完整配置指南

### 9.12.1 SystemView 概述

SystemView 是一种实时事件录制和可视化工具：
- 记录任务切换、中断、API 调用等事件
- 时间戳精度达到微秒级
- 不停止系统即可观察
- 识别调度问题、优先级反转、死锁等

### 9.12.2 SystemView 文件准备

```
Step 1: 下载 J-Link 软件包
- 访问 SEGGER 官网下载 J-Link Software
- 安装后包含 SystemView 软件

Step 2: 项目中需要添加的文件
- SEGGER_SYSVIEW.c      # SystemView 核心
- SEGGER_SYSVIEW.h      # 头文件
- SEGGER_SYSVIEW_FreeRTOS.c  # FreeRTOS 适配层
- SEGGER_SYSVIEW_FreeRTOS.h  # 适配层头文件

Step 3: 配置头文件
- 确保 FreeRTOSConfig.h 中 configUSE_IDLE_HOOK = 1
- 确保 configTICK_RATE_HZ 正确定义
```

### 9.12.3 SystemView 配置代码

```c
// ===== sysview_config.h 或 main.c =====

#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_FreeRTOS.h"

extern uint32_t SystemCoreClock;  // 在 system_stm32f4xx.c 中定义

void SEGGER_SYSVIEW_Conf(void) {
    // 1. 初始化 SystemView
    SEGGER_SYSVIEW_Init();

    // 2. 配置定时器信息
    // 系统时钟频率（CPU 主频）
    SEGGER_SYSVIEW_SetSysTimFreq(SystemCoreClock);

    // 3. 配置 tick 频率
    SEGGER_SYSVIEW_SetTickFreq(configTICK_RATE_HZ);

    // 4. 设置设备名称（可选）
    SEGGER_SYSVIEW_SetDeviceName("STM32F407");

    // 5. 开始录制
    SEGGER_SYSVIEW_Start();

    // 此时 SystemView 可以连接到目标开始录制
}

// ===== 在 main() 中调用 =====
int main(void) {
    // 硬件初始化
    SystemCoreClock = 168000000;  // STM32F4 168MHz
    // ... 其他初始化 ...

    // 在调度器启动前配置 SystemView
    SEGGER_SYSVIEW_Conf();

    // 创建任务
    xTaskCreate(vTaskCode, "Task1", configMINIMAL_STACK_SIZE,
                NULL, 1, NULL);

    // 启动调度器
    vTaskStartScheduler();

    for(;;);
}
```

### 9.12.4 SystemView 用户事件

```c
// ===== 用户自定义事件 =====

// 简单字符串事件（开销最小）
SEGGER_SYSVIEW_Printf("Task %s started", pcTaskName);

// 带参数的事件
SEGGER_SYSVIEW_Printf("Value updated: %d", value);

// 标记任务创建
void MyTask_Create(void) {
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    SEGGER_SYSVIEW_OnTaskCreate(handle);
    // ... 任务代码 ...
    SEGGER_SYSVIEW_OnTaskTerminate(handle);  // 如果任务会结束
}

// 标记任务切换
// SystemView_FreeRTOS.c 会自动处理任务切换事件
// 但可以手动记录额外信息

// 标记中断进入/退出
// 通常由 SystemView 自动处理

// ===== 区间事件（用于测量执行时间）=====

// 开始区间
SEGGER_SYSVIEW_RecordEnterTimer();

#define TRACE_FUNC_ENTER() \
    SEGGER_SYSVIEW_RecordEnterTimer()

#define TRACE_FUNC_LEAVE() \
    SEGGER_SYSVIEW_RecordExitTimer()

void MyFunction(void) {
    TRACE_FUNC_ENTER();
    // ... 函数代码 ...
    TRACE_FUNC_LEAVE();
}

// ===== 标记事件（瞬间发生）=====

// 在关键点放置标记
SEGGER_SYSVIEW_Print("Checkpoint reached");

// 带时间的标记
SEGGER_SYSVIEW_Printf("Variable x = %d at checkpoint", x);

// ===== 性能测量 =====

// 使用 SystemView 测量代码段执行时间
void Measure_Code_Section(void) {
    uint32_t start, end;

    start = SEGGER_SYSVIEW_GetTimestamp();

    // 要测量的代码
    for (volatile int i = 0; i < 1000; i++);

    end = SEGGER_SYSVIEW_GetTimestamp();

    SEGGER_SYSVIEW_Printf("Section took %u us",
                          (end - start) / (SystemCoreClock / 1000000));
}
```

### 9.12.5 SystemView 与调度器集成

```c
// ===== FreeRTOSConfig.h 配置 =====

#define configUSE_IDLE_HOOK                1
#define configUSE_TICK_HOOK                1
#define configTICK_RATE_HZ                 1000
#define configCPU_CLOCK_HZ                 SystemCoreClock

// ===== 调度器钩子实现 =====

// Idle 钩子 - SystemView 需要
void vApplicationIdleHook(void) {
    // SystemView_FreeRTOS.c 中的空闲处理会调用
    // 此处可以进入低功耗模式
}

// Tick 钩子 - SystemView 需要
void vApplicationTickHook(void) {
    // SystemView 的 tick 中断处理
}

// ===== 中断配置 =====
```

### 9.12.6 SystemView 操作步骤

```
Step 1: 编译包含 SystemView 的固件

Step 2: 启动 J-Link 连接目标
- 使用 J-Link Commander 或直接启动 SystemView

Step 3: 打开 SystemView 软件

Step 4: 连接目标
- Target -> Connect
- 或点击工具栏连接按钮
- 选择正确的设备和接口

Step 5: 开始录制
- 点击 "Start Recording"
- 或按 F5
- SystemView 会实时显示任务调度

Step 6: 停止录制分析
- 点击 "Stop Recording"
- 或按 Shift+F5

Step 7: 分析时间线
- 观察任务切换点
- 测量中断响应时间
- 查找过长阻塞
- 识别优先级反转
```

### 9.12.7 SystemView 分析功能

```
===== 时间线分析 =====

Task Timeline（任务时间线）：
- Y 轴：任务/中断
- X 轴：时间
- 颜色：不同状态（运行、就绪、阻塞）

Interrupt Timeline（中断时间线）：
- 显示所有中断的响应时间
- 标记中断嵌套

===== 统计分析 =====

Task Statistics（任务统计）：
- 各任务运行时间
- 任务切换次数
- CPU 使用率

Interrupt Statistics（中断统计）：
- 中断频率
- 中断最大/最小/平均响应时间

===== 问题识别 =====

Priority Inversion（优先级反转）：
- 显示高优先级任务等待低优先级任务的情况
- 红色标记显示反转持续时间

Task Wait（任务等待）：
- 显示任务等待事件的时间
- 过长等待用黄色标记

Interrupt Latency（中断延迟）：
- 从中断请求到开始执行的延迟
- 超过阈值用红色标记

Context Switch（任务切换）：
- 记录每次切换的原因
- 显示切换耗时
```

---

## 9.13 FreeRTOS+Trace Recorder 配置指南

### 9.13.1 Trace Recorder 简介

FreeRTOS+Trace 是一种商业工具，提供完整的调试和可视化功能：
- 详细的调度事件记录
- 内存分配追踪
- 中断事件记录
- CPU 使用率分析
- 支持流式传输和缓冲模式

### 9.13.2 TraceRecorder 配置

```c
// ===== trcConfig.h 配置 =====

// 基本配置
#define TRC_CFG_SCHEDULING_ONLY        0
#define TRC_CFG_INCLUDE_EVENTS          1
#define TRC_CFG_INCLUDE_XTOR_INFO      1
#define TRC_CFG_INCLUDE_TASK_PRIORITY_INFO  1
#define TRC_CFG_INCLUDE_ISR_TRIGGER    1

// 跟踪模式
#define TRC_CFG_RECORDER_MODE           TRC_RECORDER_MODE_STREAMING

// 缓冲区大小（根据可用 RAM 设置）
#define TRC_CFG_PAGED_EVENT_BUFFER_PAGE_COUNT 2
#define TRC_CFG_PAGED_EVENT_BUFFER_PAGE_SIZE 512

// 时间戳（根据 MCU 选择）
#define TRC_CFG_HARDWARE_PORT     TRC_HARDWARE_PORT_ARM_Cortex_M

// 事件过滤器（可选）
#define TRC_CFG_EVENT_FILTER_TRIGGER    1
#define TRC_CFG_EVENT_SYSTEM_STATE      1

// ===== 初始化代码 =====

#include "trcRecorder.h"

void Trace_Init(void) {
    // 启动录制
    // 方式一：立即开始
    vTraceEnable(TRC_START);

    // 方式二：等待主机连接（流模式）
    // vTraceEnable(TRC_START_AWAIT_HOST);

    // 方式三：仅录制任务调度（最小开销）
    // vTraceEnable(TRC_START);
}

void Trace_Stop(void) {
    vTraceDisable();
}

// ===== 在 main() 中使用 =====
int main(void) {
    // 硬件初始化
    // ...

    // 初始化 trace
    Trace_Init();

    // 创建任务
    xTaskCreate(...);

    // 启动调度器
    vTaskStartScheduler();
}

// ===== 停止录制 =====
void StopTrace(void) {
    vTraceDisable();

    // 导出数据
    // 根据配置，可能已经自动发送到主机
}
```

### 9.13.3 Trace 事件手动标记

```c
// ===== 标记任务事件 =====

// 任务创建
traceTASK_CREATE(taskHandle);

// 任务删除
traceTASK_DELETE(taskHandle);

// 任务切换
traceTASK_SWITCHED_OUT();
traceTASK_SWITCHED_IN();

// ===== 标记队列事件 =====

// 队列发送
traceQUEUE_SEND(xQueue);
traceQUEUE_SEND_FROM_ISR(xQueue);

// 队列接收
traceQUEUE_RECEIVE(xQueue);

// ===== 标记信号量事件 =====

traceBINARY_SEMAPHORE_GIVE(xSemaphore);
traceBINARY_SEMAPHORE_TAKE(xSemaphore);

traceMUTEX_GIVE(xMutex);
traceMUTEX_TAKE(xMutex);

// ===== 内存分配追踪 =====

traceMALLOC(ptr, size);
traceFREE(ptr);
```

### 9.13.4 获取 Trace 缓冲区

```c
// ===== 读取缓冲区数据 =====

// 获取缓冲区信息
void GetTraceBuffer(void) {
    uint32_t *buffer;
    uint32_t size;

    // 获取缓冲区指针和大小
    vTraceGetBuffer(&buffer, &size);

    // 方式一：通过 UART 发送
    // UART_Transmit((uint8_t*)buffer, size * 4);

    // 方式二：通过 J-Link RTT 发送
    // SEGGER_RTT_Write(0, buffer, size * 4);

    // 方式三：保存到 SD 卡
    // SD_Write(buffer, size * 4);
}

// ===== 实时流模式 =====

// 在单独任务中持续发送 trace 数据
void TraceStreamTask(void *arg) {
    uint32_t buffer[1024];

    while (1) {
        // 检查新数据
        uint32_t available = uiTrace.GetStreamedData(buffer,
                                                      sizeof(buffer));

        if (available > 0) {
            // 通过网络/UART 发送
            SendToHost(buffer, available * 4);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

---

## 9.14 HardFault 调试详解

### 9.14.1 HardFault 原因分类

```
===== HardFault 类型 =====

1. 硬故障（HardFault）
   - 由可配置故障escalation 触发
   - 原因：未处理的 MemManage、BusFault、UsageFault

2. 总线故障（BusFault）
   - 指令预取失败
   - 数据访问失败
   - 精确/非精确总线错误

3. 内存管理故障（MemManageFault）
   - MPU 违规
   - 栈溢出（写入越界）
   - 非法内存访问

4. 用法故障（UsageFault）
   - 未定义指令
   - 无效状态（Thumb 位）
   - 非法除零
   - 对齐错误
```

### 9.14.2 HardFault 处理函数实现

```c
// ===== 保存上下文的 HardFault 处理 =====

// 方法一：汇编包装器
void HardFault_Handler(void) {
    __asm volatile(
        "tst lr, #4 \n"
        "ite eq \n"
        "mrseq r0, msp \n"
        "mrsne r0, psp \n"
        "mov r1, lr \n"
        "b HardFault_Handler_C"
    );
}

// 方法二：使用 __get_PSP()
void HardFault_Handler(void) {
    uint32_t *msp = (uint32_t*)__get_MSP();
    uint32_t *psp = (uint32_t*)__get_PSP();

    uint32_t lr = __get_LR();
    uint32_t pc = __get_PC();

    if (lr & 0x4) {
        // 使用 PSP（进程栈）
        HardFault_Handler_C(psp, lr);
    } else {
        // 使用 MSP（主栈）
        HardFault_Handler_C(msp, lr);
    }
}

// ===== C 语言处理函数 =====

void HardFault_Handler_C(uint32_t *stack, uint32_t lr_value) {
    // stack 指向栈上的寄存器副本
    // 栈帧布局：[R0, R1, R2, R3, R12, LR, PC, xPSR]

    // 提取关键寄存器
    uint32_t r0  = stack[0];
    uint32_t r1  = stack[1];
    uint32_t r2  = stack[2];
    uint32_t r3  = stack[3];
    uint32_t r12 = stack[4];
    uint32_t lr  = stack[5];
    uint32_t pc  = stack[6];
    uint32_t xPSR= stack[7];

    // 打印状态
    printf("========== HardFault ==========\r\n");
    printf("R0  = 0x%08X\r\n", r0);
    printf("R1  = 0x%08X\r\n", r1);
    printf("R2  = 0x%08X\r\n", r2);
    printf("R3  = 0x%08X\r\n", r3);
    printf("R12 = 0x%08X\r\n", r12);
    printf("LR  = 0x%08X\r\n", lr);
    printf("PC  = 0x%08X\r\n", pc);
    printf("xPSR= 0x%08X\r\n", xPSR);
    printf("LR_exc = 0x%08X\r\n", lr_value);

    // 读取 Fault 寄存器
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t mmfar = SCB->MMFAR;
    uint32_t bfar = SCB->BFAR;

    printf("\r\n--- Fault Status ---\r\n");
    printf("CFSR  = 0x%08X\r\n", cfsr);
    printf("HFSR  = 0x%08X\r\n", hfsr);
    printf("MMFAR = 0x%08X\r\n", mmfar);
    printf("BFAR  = 0x%08X\r\n", bfar);

    // 分析 CFSR
    Analyze_CFSR(cfsr);

    printf("\r\n--- Stack Frame ---\r\n");
    printf("MainStackBase: 0x%08X\r\n", SCB->MSP);
    printf("ProcStackBase: 0x%08X\r\n", SCB->PSP);

    // 保存到 Flash（可选）
    // SaveCrashData(stack, pc, lr, cfsr);

    // 死循环，方便调试器连接
    while(1) {
        __NOP();
    }
}

// ===== CFSR 分析函数 =====

void Analyze_CFSR(uint32_t cfsr) {
    // UFSR (UsageFault)
    if (cfsr & 0x00000001) printf("  [UFSR] UNDEFINSTR - 未定义指令\r\n");
    if (cfsr & 0x00000002) printf("  [UFSR] INVSTATE - 无效状态\r\n");
    if (cfsr & 0x00000004) printf("  [UFSR] INVPC - 无效 PC\r\n");
    if (cfsr & 0x00000008) printf("  [UFSR] NOCP - 无协处理器\r\n");
    if (cfsr & 0x00010000) printf("  [UFSR] UNALIGNED - 非对齐访问\r\n");
    if (cfsr & 0x00008000) printf("  [UFSR] DIVBYZERO - 除零错误\r\n");

    // BFSR (BusFault)
    if (cfsr & 0x00000100) printf("  [BFSR] IBUSERR - 指令总线错误\r\n");
    if (cfsr & 0x00000200) printf("  [BFSR] PRECISERR - 精确数据总线错误\r\n");
    if (cfsr & 0x00000400) printf("  [BFSR] IMPRECISERR - 非精确数据总线错误\r\n");
    if (cfsr & 0x00000800) printf("  [BFSR] UNSTKERR - 弹栈错误\r\n");
    if (cfsr & 0x00001000) printf("  [BFSR] STKERR - 压栈错误\r\n");
    if (cfsr & 0x00008000) printf("  [BFSR] BFARVALID - BFAR 有效\r\n");

    // MMFSR (MemManage)
    if (cfsr & 0x00000001) printf("  [MMFSR] IACCVIOL - 指令访问违规\r\n");
    if (cfsr & 0x00000002) printf("  [MMFSR] DACCVIOL - 数据访问违规\r\n");
    if (cfsr & 0x00000008) printf("  [MMFSR] MUNSTKERR - MPU 弹栈错误\r\n");
    if (cfsr & 0x00000010) printf("  [MMFSR] MSTKERR - MPU 压栈错误\r\n");
    if (cfsr & 0x00000080) printf("  [MMFSR] MMARVALID - MMFAR 有效\r\n");
}

// ===== HFSR 分析函数 =====

void Analyze_HFSR(uint32_t hfsr) {
    if (hfsr & 0x40000000) printf("  [HFSR] FORCED - 从可配置故障升级\r\n");
    if (hfsr & 0x00000001) printf("  [HFSR] VECTBL - 向量表读取错误\r\n");
    if (hfsr & 0x00000002) printf("  [HFSR] CORE2 - 保留\r\n");
}
```

### 9.14.3 GDB 分析 HardFault

```gdb
# ===== HardFault 后 GDB 分析 =====

# 1. 查看触发时的寄存器
(gdb) info registers

# 2. 查看栈回溯
(gdb) bt

# 3. 查看当前指令
(gdb) x/8i $pc

# 4. 查看栈内容
(gdb) x/16wx $sp

# 5. 读取 fault 寄存器
(gdb) set $cfsr = *(unsigned int*)0xE000ED28
(gdb) set $hfsr = *(unsigned int*)0xE000ED2C
(gdb) set $mmfar = *(unsigned int*)0xE000ED34
(gdb) set $bfar = *(unsigned int*)0xE000ED38

(gdb) printf "CFSR=0x%08x HFSR=0x%08x\n", $cfsr, $hfsr

# 6. 确定栈帧位置
# MSP 通常指向栈顶
(gdb) set $msp_val = *(unsigned int*)0xE000ED04
(gdb) printf "MSP = 0x%08x\n", $msp_val

# 7. 查看栈上的返回地址
(gdb) x/8wx $msp_val - 8
# 这显示之前压栈的寄存器

# 8. 使用自动分析命令（如果有）
(gdb) analyze-hardfault
```

---

## 9.15 系统挂起调试检查清单

### 9.15.1 系统挂起初步判断

```
===== 判断方法 =====

1. 调试器连接测试
   - 能否连接调试器？
   - 能连接 → CPU 在运行，可能死锁
   - 不能连接 → 可能是看门狗/硬故障

2. 观察症状
   - 完全无响应？
   - 特定任务无响应？
   - 定时器/中断还在运行？
```

### 9.15.2 调试器检查步骤

```gdb
# ===== 连接调试器检查 =====

# 1. 暂停系统
monitor halt

# 2. 查看当前状态
(gdb) info threads
# 如果只有一个线程，说明只有一个任务在运行

# 3. 查看当前任务
(gdb) thread apply all bt

# 4. 检查调度器状态
(gdb) print uxCurrentPendingList
(gdb) print xDelayedTaskList1
(gdb) print xReadyTasks

# 5. 检查中断状态
(gdb) monitor debug reg
# 查看是否有关键中断被禁止

# 6. 检查临界区嵌套
(gdb) print uxCriticalNesting
# 如果 > 0 且不再变化，说明卡在临界区
```

### 9.15.3 挂起原因分类检查

```c
// ===== 原因 1: 所有任务都阻塞 =====

// 检查任务状态
void DumpAllTaskStates(void) {
    TaskStatus_t *states;
    uint32_t total;
    int allBlocked = 1;

    states = pvPortMalloc(uxTaskGetNumberOfTasks() * sizeof(TaskStatus_t));
    uxTaskGetSystemState(states, uxTaskGetNumberOfTasks(), &total);

    printf("=== Task States ===\r\n");
    for (int i = 0; i < uxTaskGetNumberOfTasks(); i++) {
        printf("%-12s State=%d\r\n",
               states[i].pcTaskName,
               states[i].eCurrentState);
        if (states[i].eCurrentState != eBlocked) {
            allBlocked = 0;
        }
    }

    if (allBlocked) {
        printf("WARNING: All tasks blocked - possible deadlock!\r\n");
    }

    vPortFree(states);
}

// ===== 原因 2: 高优先级任务占用 CPU =====

// 检查任务优先级
void CheckPriorityInversion(void) {
    TaskStatus_t *states;
    states = pvPortMalloc(uxTaskGetNumberOfTasks() * sizeof(TaskStatus_t));
    uxTaskGetNumberOfTasks();

    // 检查是否有高优先级任务一直运行
    for (int i = 0; i < uxTaskGetNumberOfTasks(); i++) {
        if (states[i].eCurrentState == eRunning) {
            printf("Running task: %s (Priority %u)\r\n",
                   states[i].pcTaskName,
                   states[i].uxCurrentPriority);
        }
    }

    vPortFree(states);
}

// ===== 原因 3: 任务卡在临界区 =====

// 在调试器中或通过 ITM 检查
extern UBaseType_t uxCriticalNesting;

void CheckCriticalNesting(void) {
    printf("Critical nesting: %u\r\n", uxCriticalNesting);
    if (uxCriticalNesting > 10) {
        printf("ERROR: Stuck in critical section!\r\n");
    }
}

// ===== 原因 4: 任务等待永不解除的条件 =====

// 检查信号量/队列状态
void CheckBlockingObjects(void) {
    // 检查所有队列
    extern QueueDefinition *xQueueList[];
    extern int xQueueCount;

    for (int i = 0; i < xQueueCount; i++) {
        printf("Queue[%d]: waiting=%u, available=%u\r\n",
               i,
               xQueueList[i]->uxMessagesWaiting,
               xQueueList[i]->uxLength - xQueueList[i]->uxMessagesWaiting);
    }
}

// ===== 原因 5: 中断被永久禁止 =====

// 检查 PRIMASK, FAULTMASK, BASEPRI
void CheckInterruptMask(void) {
    uint32_t primask, faultmask, basepri;

    __asm volatile(
        "mrs %0, primask\n"
        "mrs %1, faultmask\n"
        "mrs %2, basepri\n"
        : "=r"(primask), "=r"(faultmask), "=r"(basepri)
    );

    printf("PRIMASK=%u, FAULTMASK=%u, BASEPRI=0x%X\r\n",
           primask, faultmask, basepri);

    if (basepri != 0) {
        printf("WARNING: Interrupts masked above priority %u\r\n",
               basepri);
    }
}
```

### 9.15.4 中断调试检查

```c
// ===== 检查是否有中断挂起 =====

// 检查中断控制状态
void CheckInterruptStatus(void) {
    // NVIC 中断使能状态
    uint32_t iser0 = NVIC->ISER[0];
    uint32_t iser1 = NVIC->ISER[1];

    printf("ISER0=0x%08X\r\n", iser0);
    printf("ISER1=0x%08X\r\n", iser1);

    // 检查挂起的中断
    uint32_t ispr0 = NVIC->ISPR[0];
    uint32_t ispr1 = NVIC->ISPR[1];

    printf("ISPR0=0x%08X\r\n", ispr0);
    printf("ISPR1=0x%08X\r\n", ispr1);

    // 找到挂起的中断
    for (int i = 0; i < 32; i++) {
        if (ispr0 & (1 << i)) {
            printf("Pending IRQ[%d]\r\n", i);
        }
    }
}

// ===== 检查 SysTick =====

void CheckSysTick(void) {
    uint32_t ctrl = SysTick->CTRL;
    uint32_t load = SysTick->LOAD;
    uint32_t val = SysTick->VAL;

    printf("SysTick CTRL=0x%08X\r\n", ctrl);
    printf("SysTick LOAD=0x%08X\r\n", load);
    printf("SysTick VAL=0x%08X\r\n", val);

    if (ctrl & (1 << 16)) {
        printf("COUNTFLAG set - tick occurred\r\n");
    }
}
```

### 9.15.5 系统挂起调试清单

```
===== 系统挂起检查清单 =====

[ ] 1. 能否连接调试器？
    - 能 → CPU 还在运行，跳到步骤 3
    - 不能 → 可能是 HardFault/看门狗，检查 RCC_CSR

[ ] 2. 检查复位原因
    - 看门狗复位 → 任务没在超时内喂狗
    - 硬故障复位 → 检查 HFRSR/CFSR
    - 电源复位 → 检查电源问题

[ ] 3. 暂停并检查当前执行的代码
    - 查看 PC 指向哪里
    - 查看调用栈

[ ] 4. 检查所有任务状态
    - uxTaskGetSystemState() 查看所有任务
    - 是否全部处于 blocked 状态？

[ ] 5. 检查阻塞对象
    - 所有信号量/队列/事件组状态
    - 是否有任务在等一个永远不会发生的事件？

[ ] 6. 检查中断状态
    - BASEPRI 是否被设置为禁止中断？
    - uxCriticalNesting 是否异常高？

[ ] 7. 检查优先级反转
    - SystemView/Trace 查看是否有优先级反转
    - 低优先级任务是否持有高优先级任务需要的锁？

[ ] 8. 检查看门狗
    - 看门狗是否被喂狗？
    - 喂狗任务是否运行？

[ ] 9. 检查栈溢出
    - 任务栈 canary 值是否被破坏？
    - 高优先级任务的栈是否足够？

[ ] 10. 检查内存碎片化
    - xPortGetFreeHeapSize() 是否在减少？
    - heap_4 是否有大量不连续空闲块？
```

---

## 9.16 常见崩溃模式与解决方案

### 9.16.1 崩溃模式分类

```
===== 模式 1: 启动立即 HardFault =====

症状：
- 系统上电后立即进入 HardFault
- 死在启动代码或第一个任务

常见原因：
- 栈指针初始化错误
- pxCurrentTCB 为 NULL
- 堆内存分配失败
- MPU 配置错误

排查方法：
1. 检查 __initial_sp 是否正确
2. 检查 SystemInit() 是否在 main() 前调用
3. 检查 vTaskStartScheduler() 前的内存分配
4. 在 xTaskCreate() 处设置断点

解决方案：
- 确保启动文件中栈指针正确
- 在 configCHECK_FOR_STACK_OVERFLOW = 2
- 检查 heap 是否足够分配 TCB


===== 模式 2: 运行一段时间后 HardFault =====

症状：
- 系统正常运行数分钟/数小时后崩溃
- 通常在特定操作时触发

常见原因：
- 栈溢出（随任务切换累积）
- 内存泄漏导致后续分配失败
- 队列/信号量操作越界
- 中断优先级配置错误

排查方法：
1. 定期打印栈水位
2. 监控堆使用量
3. 检查 canary 值
4. 使用 SystemView 定位崩溃时机

解决方案：
- 增加任务栈大小
- 修复内存泄漏
- 正确配置中断优先级


===== 模式 3: 任务停止但系统存活 =====

症状：
- 系统不崩溃但无响应
- 特定任务不运行
- 其他任务可能正常

常见原因：
- 任务陷入死循环（无 vTaskDelay）
- 任务等待一个永不满足的条件
- 任务被删除但其他任务在等它
- 优先级配置错误

排查方法：
1. SystemView 查看任务调度
2. 打印任务状态
3. 检查任务是否调用 vTaskDelay
4. 检查任务间依赖关系

解决方案：
- 确保所有任务有 vTaskDelay(pdMS_TO_TICKS(1))
- 使用 portMAX_DELAY 时有超时保护
- 正确设置任务优先级


===== 模式 4: 间歇性崩溃 =====

症状：
- 崩溃随机发生
- 难以复现
- 每次崩溃位置不同

常见原因：
- 竞态条件
- 内存踩踏（野指针）
- 中断与任务间同步问题
- 缓冲区溢出

排查方法：
1. 检查所有指针解引用
2. 检查中断与任务的共享变量访问
3. 使用 MPU 保护关键内存
4. 启用堆/栈调试

解决方案：
- 使用内存保护单元 (MPU)
- 中断访问共享变量加锁
- 减少全局变量使用
- 增加栈大小


===== 模式 5: malloc 失败但堆有空间 =====

症状：
- 分配失败但 xPortGetFreeHeapSize() 显示有足够空间
- heap_1/heap_2 碎片化

常见原因：
- heap_2 的内存碎片化（不同大小块）
- 最大连续块不足以满足请求
- 泄漏导致堆耗尽

排查方法：
1. 使用 heap_4 替代 heap_2
2. 记录每次分配的大小和位置
3. 检查是否有频繁分配/释放不同大小块

解决方案：
- 使用 heap_4 减少碎片
- 预分配常用大小的内存池
- 避免频繁分配/释放大块
```

### 9.16.2 崩溃排查流程图

```
===== HardFault 排查流程 =====

1. 停在 HardFault_Handler
   |
   v
2. 读取 SCB->CFSR
   |
   +---> bit[7:0] != 0 ----> MMFSR (内存管理)
   |                        检查 MMFAR
   |
   +---> bit[15:8] != 0 ---> BFSR (总线)
   |                        检查 BFAR
   |
   +---> bit[31:16] != 0 --> UFSR (用法)
   |
3. 读取栈帧
   |
   v
4. 确定 PC (程序计数器)
   |
   v
5. 反汇编找到问题指令
   |
   v
6. 分析代码找出原因
```

### 9.16.3 常见编码错误对照表

```
===== 错误类型与症状对照 =====

野指针访问：
- 症状：随机地址 HardFault
- 常见位置：指针解引用后未检查 NULL
- 解决：解引用前检查 NULL

数组越界：
- 症状：栈/全局数组写入越界
- 常见位置：for 循环边界错误
- 解决：使用安全的数组包装

结构体成员访问错误：
- 症状：看门狗复位，内存被破坏
- 常见位置：指针类型转换错误
- 解决：仔细检查结构体布局

中断优先级错误：
- 症状：特定中断后系统死机
- 常见位置：configMAX_SYSCALL_INTERRUPT_PRIORITY
- 解决：确保用户中断优先级 <= 此值

临界区死锁：
- 症状：系统停止响应
- 常见位置：临界区内调用阻塞 API
- 解决：临界区尽量短，不调用阻塞 API

消息队列操作错误：
- 症状：任务卡在 queue receive
- 常见位置：队列句柄传错
- 解决：使用静态队列，确保句柄有效
```

---

## 9.17 内存泄漏检测进阶

### 9.17.1 标记-清除法实现

```c
// ===== 内存分配跟踪结构 =====

typedef struct {
    void *ptr;                // 分配的指针
    size_t size;              // 大小
    const char *file;         // 文件名
    int line;                 // 行号
    uint32_t magic;           // 魔数
} MemBlock_t;

#define MEM_BLOCK_MAGIC 0xDEADBEEF

// 存储所有分配的块
#define MAX_ALLOCATIONS 256
MemBlock_t g_allocations[MAX_ALLOCATIONS];
int g_alloc_count = 0;

// ===== 跟踪分配 =====

void *TrackedMalloc(size_t size, const char *file, int line) {
    void *ptr = pvPortMalloc(size);
    if (ptr == NULL) {
        printf("TRACK: Malloc failed %s:%d size=%u\r\n", file, line, size);
        return NULL;
    }

    // 记录分配
    if (g_alloc_count < MAX_ALLOCATIONS) {
        g_allocations[g_alloc_count].ptr = ptr;
        g_allocations[g_alloc_count].size = size;
        g_allocations[g_alloc_count].file = file;
        g_allocations[g_alloc_count].line = line;
        g_allocations[g_alloc_count].magic = MEM_BLOCK_MAGIC;
        g_alloc_count++;
    }

    // 填充内存（方便发现问题）
    memset(ptr, 0xA5, size);

    return ptr;
}

void TrackedFree(void *ptr, const char *file, int line) {
    if (ptr == NULL) {
        return;
    }

    // 查找并移除记录
    for (int i = 0; i < g_alloc_count; i++) {
        if (g_allocations[i].ptr == ptr) {
            // 找到，移除（简单起见用最后一项替换）
            printf("TRACK: Free %s:%d ptr=%p size=%u [%s:%d]\r\n",
                   file, line, ptr,
                   g_allocations[i].size,
                   g_allocations[i].file,
                   g_allocations[i].line);
            g_allocations[i] = g_allocations[g_alloc_count - 1];
            g_alloc_count--;
            break;
        }
    }

    vPortFree(ptr);
}

// ===== 宏定义简化使用 =====

#define TRACKED_MALLOC(size) \
    TrackedMalloc(size, __FILE__, __LINE__)

#define TRACKED_FREE(ptr) \
    TrackedFree(ptr, __FILE__, __LINE__)

// ===== 打印泄漏报告 =====

void PrintMemoryLeaks(void) {
    printf("\r\n========== Memory Leak Report ==========\r\n");
    printf("Total allocations: %d\r\n", g_alloc_count);
    printf("Current heap free: %u bytes\r\n", xPortGetFreeHeapSize());

    if (g_alloc_count == 0) {
        printf("No memory leaks detected.\r\n");
        return;
    }

    printf("\r\n--- Active Allocations ---\r\n");
    size_t total = 0;
    for (int i = 0; i < g_alloc_count; i++) {
        printf("[%d] ptr=%p size=%u from %s:%d\r\n",
               i,
               g_allocations[i].ptr,
               g_allocations[i].size,
               g_allocations[i].file,
               g_allocations[i].line);
        total += g_allocations[i].size;
    }

    printf("\r\nTotal memory in use: %u bytes\r\n", total);
    printf("========================================\r\n");
}
```

### 9.17.2 栈填充法检测栈溢出

```c
// ===== 栈 canary 填充 =====

// 在任务创建后填充栈
void FillTaskStack(TaskHandle_t task) {
    // 获取 TCB 中的栈信息
    // 注意：实际实现依赖 FreeRTOS 版本

    extern void vApplicationStackOverflowHook(TaskHandle_t, char*);

    // 读取任务栈范围（需要根据具体 MCU 和 FreeRTOS 版本）
    uint32_t *stackStart = (uint32_t*)task->pxStack;
    uint32_t stackDepth = task->uxStackDepth;

    // 填充 canary 值
    // 栈从高地址向低地址增长，所以从栈底开始填充
    uint32_t *p = stackStart;
    for (uint32_t i = 0; i < stackDepth; i++) {
        *p = 0xA5A5A5A5;
        p++;
    }

    printf("Task %s stack filled with canary\r\n", task->pcTaskName);
}

// ===== 检查栈 canary =====

StackType_t *CheckStackCanary(TaskHandle_t task) {
    uint32_t *stackStart = (uint32_t*)task->pxStack;
    uint32_t stackDepth = task->uxStackDepth;

    // 查找第一个非 canary 的位置
    uint32_t used = 0;
    for (uint32_t i = 0; i < stackDepth; i++) {
        if (stackStart[i] != 0xA5A5A5A5) {
            used = i;
            break;
        }
    }

    // 计算栈使用百分比
    uint32_t free = stackDepth - used;
    uint32_t percent = (used * 100) / stackDepth;

    printf("Task: %s\r\n", task->pcTaskName);
    printf("  Stack used: %u/%u words (%u%%)\r\n", used, stackDepth, percent);
    printf("  Free: %u words\r\n", free);

    return (StackType_t*)&stackStart[used];
}

// ===== 周期性栈检查任务 =====

void StackMonitorTask(void *arg) {
    printf("Stack Monitor Started\r\n");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        TaskStatus_t *tasks;
        uint32_t total;
        int count = uxTaskGetNumberOfTasks();

        tasks = pvPortMalloc(count * sizeof(TaskStatus_t));
        if (tasks == NULL) continue;

        uxTaskGetSystemState(tasks, count, &total);

        printf("\r\n========== Stack Status ==========\r\n");

        for (int i = 0; i < count; i++) {
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(tasks[i].xHandle);
            printf("%-12s: HWM=%4lu words\r\n",
                   tasks[i].pcTaskName, hwm);

            // 警告低水位
            if (hwm < 50) {
                printf("  WARNING: Low stack high water mark!\r\n");
            }
        }

        printf("Free heap: %u bytes\r\n", xPortGetFreeHeapSize());
        printf("Min ever free: %u bytes\r\n",
               xPortGetMinimumEverFreeHeapSize());

        vPortFree(tasks);
    }
}
```

### 9.17.3 堆碎片化检测

```c
// ===== 堆碎片化分析 =====

// 分析 heap_4 的空闲块
void AnalyzeHeapFragmentation(void) {
    printf("\r\n========== Heap Analysis ==========\r\n");

    size_t freeHeap = xPortGetFreeHeapSize();
    printf("Total free heap: %u bytes\r\n", freeHeap);
    printf("Min ever free: %u bytes\r\n",
           xPortGetMinimumEverFreeHeapSize());

    // 注意：以下分析需要访问 heap_4 内部结构
    // 实际实现取决于使用的 heap 方案

    extern BlockLink_t xStart;
    extern size_t xBlockSize;
    extern size_t xNumberOfSuccessfulAllocations;
    extern size_t xNumberOfSuccessfulFrees;

    printf("Successful allocs: %u\r\n",
           xNumberOfSuccessfulAllocations);
    printf("Successful frees: %u\r\n",
           xNumberOfSuccessfulFrees);

    // 简单的碎片化测试
    void *blocks[10];

    printf("\r\n--- Fragmentation Test ---\r\n");

    // 分配多个块
    for (int i = 0; i < 5; i++) {
        blocks[i] = pvPortMalloc(100);
        printf("Alloc[%d]=%p\r\n", i, blocks[i]);
    }

    // 释放奇数块
    for (int i = 0; i < 5; i += 2) {
        vPortFree(blocks[i]);
        blocks[i] = NULL;
        printf("Free[%d]\r\n", i);
    }

    // 尝试分配大块
    void *big = pvPortMalloc(300);
    printf("Alloc big (300): %p\r\n", big);

    if (big == NULL) {
        printf("WARNING: Large allocation failed - possible fragmentation!\r\n");
    }

    // 清理
    if (big) vPortFree(big);
    for (int i = 1; i < 5; i += 2) {
        if (blocks[i]) vPortFree(blocks[i]);
    }
}
```

---

## 9.18 面试高频问题详解

### 9.18.1 Q1: FreeRTOS 如何检测栈溢出？

**问题分析：**
考察对 FreeRTOS 栈保护机制的理解深度。

**详细答案：**

FreeRTOS 提供两种级别的栈溢出检测：

**级别 1 - 栈指针检查：**
```c
#define configCHECK_FOR_STACK_OVERFLOW 1
```
在任务切换时检查 PSP（进程栈指针）是否超出任务栈范围。优点是开销小，缺点是只能检测到溢出已经发生后。

**级别 2 - 栈 canary 值检查：**
```c
#define configCHECK_FOR_STACK_OVERFLOW 2
```
除了检查栈指针，还会在任务栈末尾放置一个标记值（canary，通常为 0xA5A5A5A5），每次任务切换时检查此值是否被破坏。优点是能更早发现栈溢出，缺点是增加切换开销。

**钩子函数实现：**
```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // xTask: 溢出任务的句柄
    // pcTaskName: 溢出任务的名称
    printf("STACK OVERFLOW in task: %s\r\n", pcTaskName);

    // 可选：保存崩溃上下文
    // 在此处记录有用信息

    // 死循环便于调试
    while(1);
}
```

**进阶补充：**
- 栈大小应设置为最坏情况下需求量的 1.5-2 倍
- 可以周期性检查 uxTaskGetStackHighWaterMark()
- 在 MPU 环境下可设置栈区域为只读，防止溢出

---

### 9.18.2 Q2: 如何排查 FreeRTOS 系统死机？

**问题分析：**
考察综合调试能力和系统分析思路。

**详细答案：**

**步骤一：判断死机类型**
1. 能否连接调试器？
   - 能连接 → CPU 在运行，排查死锁/任务调度
   - 不能连接 → HardFault/看门狗/电源问题

2. 检查复位原因寄存器（RCC_CSR）

**步骤二：如果是 HardFault**
1. 读取 CFSR 寄存器判断故障类型
2. 读取 HFSR、MMFAR、BFAR
3. 分析栈帧，确定 PC 值
4. 反汇编定位问题代码

**步骤三：如果是任务调度问题**
1. 使用 uxTaskGetSystemState() 查看所有任务状态
2. 检查是否存在：全部 blocked（死锁）、高优先级任务忙等

**步骤四：检查中断**
1. 检查 BASEPRI 是否禁止了中断
2. 检查 uxCriticalNesting
3. 检查 SysTick 是否正常

**步骤五：使用工具**
- SEGGER SystemView：观察实时任务调度
- FreeRTOS+Trace：记录完整事件
- J-Link RTT：高速调试输出

---

### 9.18.3 Q3: FreeRTOS 内存管理有哪几种方式？

**问题分析：**
考察对内存管理机制的理解。

**详细答案：**

**heap_1 - 简单固定大小分配器**
- 只支持分配，不支持释放
- 适合静态系统，不需要删除任务/队列
- 开销最小，无碎片问题

**heap_2 - 最佳匹配分配器**
- 支持释放，但不使用合并
- 使用最合适大小的空闲块
- 会产生碎片，不适合长时间运行

**heap_3 - 包装标准 malloc/free**
- 使用标准 C 库的 malloc/free
- 线程安全（需要配置）
- 依赖标准库实现

**heap_4 - 首次匹配+相邻块合并**
- 支持释放和合并相邻空闲块
- 减少碎片问题
- 适合需要频繁分配/释放的场景
- 可以配置对齐方式

**heap_5 - heap_4 + 多区域支持**
- 支持多个非连续内存区域
- 可用于外部 SRAM
- 与 heap_4 相同的分配算法

---

### 9.18.4 Q4: 如何测量 FreeRTOS 任务 CPU 使用率？

**问题分析：**
考察运行时统计功能的使用。

**详细答案：**

**方法一：vTaskGetRunTimeStats()**
```c
// 1. 配置
#define configGENERATE_RUN_TIME_STATS 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

// 2. 配置时基（需要定时器）
void ConfigureTimeForRunTimeStats(void) {
    // 使用 DWT CYCCNT 或定时器
}

// 3. 获取统计
char buf[512];
vTaskGetRunTimeStats(buf);
printf("%s\r\n", buf);
```

**方法二：uxTaskGetSystemState()**
```c
TaskStatus_t *pxTaskStatusArray;
UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
uint32_t ulTotalRunTime;

pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

for (int i = 0; i < uxArraySize; i++) {
    uint32_t cpuPercent = (pxTaskStatusArray[i].ulRunTimeCounter * 100) / ulTotalRunTime;
    printf("%-12s: %3u%%\r\n",
           pxTaskStatusArray[i].pcTaskName,
           cpuPercent);
}
```

---

### 9.18.5 Q5: FreeRTOS 任务间通信方式有哪些？

**问题分析：**
考察对 IPC 机制的理解。

**详细答案：**

**队列 (Queue)**
- 适用于任务间传递数据
- 支持一读一写、多读者
- 阻塞等待可配置超时
- FIFO 顺序

**二进制信号量 (Binary Semaphore)**
- 适用于同步
- 只能为 0 或 1
- 常用于中断与任务同步

**计数信号量 (Counting Semaphore)**
- 适用于资源计数
- 可配置最大值和初始值
- 适用于事件计数

**互斥量 (Mutex)**
- 支持优先级继承
- 防止优先级反转
- 适用于保护共享资源
- 不能在中断中使用

**事件组 (Event Group)**
- 位掩码表示事件
- 可等待多个事件组合
- 适用于复杂同步条件

**任务通知 (Task Notification)**
- 轻量级通信
- 每个任务有一个通知值
- 支持多种接收方式

---

### 9.18.6 Q6: 什么是优先级反转？如何解决？

**问题分析：**
考察对实时系统概念的理解。

**详细答案：**

**优先级反转现象：**
低优先级任务 L 持有资源，高优先级任务 H 等待该资源。此时中优先级任务 M 开始运行，阻塞 H。M > H > L，导致高优先级任务反而被低优先级任务阻塞。

**经典案例：火星探测器 Mars Pathfinder**

**解决方案：**

**1. 优先级继承**
- 当高优先级任务等待低优先级任务持有的资源时，临时提升低优先级任务的优先级
- FreeRTOS 的互斥量自动实现优先级继承

**2. 优先级天花板 (Priority Ceiling)**
- 给资源分配一个最高优先级
- 任何使用该资源的任务临时获得该优先级

**3. 禁止中断**
- 临界区内禁止调度
- 但会响应延迟，不适合长临界区

**FreeRTOS 中的处理：**
```c
// 使用互斥量代替普通信号量
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

xSemaphoreTake(xMutex, portMAX_DELAY);
// 临界区操作
xSemaphoreGive(xMutex);
```

---

### 9.18.7 Q7: FreeRTOS 调度算法是怎样的？

**问题分析：**
考察对调度机制的理解深度。

**详细答案：**

**固定优先级抢占式调度：**
- FreeRTOS 使用固定优先级
- 最高优先级就绪任务运行
- 同优先级任务使用时间片轮转

**configMAX_PRIORITIES**
- 最大优先级数量
- 通常设置为 5-10
- 优先级 0 最低（configMAX_PRIORITIES-1 最高）

**调度点：**
1. 任务调用 vTaskDelay()
2. 任务调用阻塞队列/信号量操作
3. 任务从阻塞恢复
4. 中断中解除更高优先级任务阻塞
5. configUSE_TIME_SLICING 时，同优先级时间片切换

**时间片轮转：**
```c
#define configUSE_TIME_SLICING 1
#define configTICK_RATE_HZ 1000  // 每秒 1000 次 tick
```
每个 tick 检查是否有同优先级其他任务就绪，有则切换。

**空闲任务：**
- 优先级 0，始终就绪
- 负责内存回收、优先级继承等
- 可连接空闲钩子进入低功耗

---

## 9.19 GDB 调试实战案例

### 9.19.1 案例一：栈溢出调试

```
===== 场景：系统运行一段时间后死机 =====

Step 1: 启用栈溢出检测
#define configCHECK_FOR_STACK_OVERFLOW 2

Step 2: 在 GDB 中设置断点
(gdb) break vApplicationStackOverflowHook

Step 3: 运行系统，等待断点触发
(gdb) continue

Step 4: 触发后分析
(gdb) print pcTaskName    # 查看任务名
(gdb) print xTask        # 查看任务句柄
(gdb) bt                  # 查看栈回溯
(gdb) info frame          # 查看栈帧详情

Step 5: 检查栈使用
(gdb) print pxCurrentTCB->pxTopOfStack
(gdb) print pxCurrentTCB->pxStack
(gdb) print pxCurrentTCB->uxStackDepth

Step 6: 计算栈使用量
(gdb) print (uint32_t)pxCurrentTCB->pxStack - (uint32_t)pxCurrentTCB->pxTopOfStack

Step 7: 增加栈大小或优化栈使用
```

### 9.19.2 案例二：内存泄漏定位

```
===== 场景：系统运行数小时后内存不足 =====

Step 1: 添加内存跟踪代码（参考 9.17）

Step 2: 在关键点打印泄漏报告
void PrintLeaks(void) {
    PrintMemoryLeaks();
}

Step 3: 定期调用
void MonitorTask(void *arg) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));  // 每分钟
        PrintLeaks();
    }
}

Step 4: 观察泄漏增长
- 第一次：分配 10 个块
- 第二次：分配 11 个块
- ...

Step 5: 定位泄漏位置
- 对比两次报告
- 找出新增的分配
- 检查对应的代码路径
```

### 9.19.3 案例三：死锁调试

```
===== 场景：系统停止响应 =====

Step 1: 连接调试器并暂停
(gdb) monitor halt
(gdb) info threads

Step 2: 查看所有线程状态
(gdb) thread apply all bt

Step 3: 检查临界区
(gdb) print uxCriticalNesting
# 如果 > 0 且所有线程卡住，说明在临界区

Step 4: 检查互斥量
(gdb) print xMutexHolder
# 查看哪个任务持有了互斥量

Step 5: 检查等待列表
(gdb) print xTasksWaitingForMutex

Step 6: 分析调用栈
# 找出死锁的两个任务
# Task A 持有 Mutex1 等待 Mutex2
# Task B 持有 Mutex2 等待 Mutex1
```

---

## 9.20 调试检查清单速查

### 9.20.1 新项目调试检查清单

```
===== 代码审查检查项 =====

[ ] configCHECK_FOR_STACK_OVERFLOW 设置为 2
[ ] configUSE_MALLOC_FAILED_HOOK 启用
[ ] 所有任务有合适的栈大小
[ ] 所有任务有 vTaskDelay() 或阻塞调用
[ ] 中断优先级配置正确 (<= configMAX_SYSCALL_INTERRUPT_PRIORITY)
[ ] configASSERT() 启用
[ ] 调试时编译优化级别为 O0
```

### 9.20.2 崩溃调试速查

```
===== HardFault 处理顺序 =====

1. 保存栈帧（使用提供的汇编代码）
2. 读取 CFSR, HFSR, MMFAR, BFAR
3. 打印寄存器状态
4. 分析 PC 指向的指令
5. 定位到源代码
```

### 9.20.3 性能优化检查

```
===== CPU 使用率异常 =====

高空闲 CPU + 高一个任务 CPU：
- 正常：该任务忙

高空闲 CPU + 所有任务 CPU 低：
- 正常：系统负载低

低空闲 CPU：
- 问题：某个任务未正确阻塞

周期性 CPU 尖峰：
- 原因：周期性任务执行时间过长
```

### 9.20.4 调试宏建议

```c
// ===== 建议的调试宏 =====

#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) \
        printf("[DEBUG] " fmt "\r\n", ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

// 断言宏
#define ASSERT(x) \
    do { if (!(x)) { printf("ASSERT FAILED: %s\r\n", #x); while(1); } } while(0)

// 跟踪宏
#define TRACE_TASK_CREATE(xHandle, name) \
    SEGGER_SYSVIEW_Printf("Task %s created", name)

#define TRACE_ISR_ENTER() \
    SEGGER_SYSVIEW_RecordEnterISR()

#define TRACE_ISR_EXIT() \
    SEGGER_SYSVIEW_RecordExitISR()
```

---

## 9.21 总结

本章详细介绍了 FreeRTOS 调试的各个方面：

**基础调试能力：**
- GDB 命令行调试
- HardFault 故障分析
- 栈溢出检测
- 内存问题排查

**实时监控工具：**
- J-Link RTT 高速调试输出
- SEGGER SystemView 实时事件录制
- FreeRTOS+Trace 完整跟踪

**调试技巧：**
- 内存泄漏标记追踪
- 系统挂起死锁检查
- 常见崩溃模式分析
- CFSR 故障寄存器解码

**最佳实践：**
- 调试时关闭编译器优化
- 合理使用断言
- 定期检查栈水位
- 使用合适的内存管理方案

**建议：**
1. 开发阶段启用所有调试选项
2. 生产环境保留关键断言
3. 为关键模块添加跟踪点
4. 建立标准的崩溃分析流程
5. 熟练掌握至少一种实时监控工具

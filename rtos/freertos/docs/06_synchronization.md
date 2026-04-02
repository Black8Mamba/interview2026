# 第六章：同步机制

> 本章目标：掌握队列、信号量、互斥量、事件组、任务通知等通信与同步手段

## 章节结构

- [ ] 6.1 同步机制概述
- [ ] 6.2 队列（Queue）
- [ ] 6.3 二值信号量
- [ ] 6.4 计数信号量
- [ ] 6.5 互斥量
- [ ] 6.6 递归互斥量
- [ ] 6.7 事件组
- [ ] 6.8 任务通知
- [ ] 6.9 面试高频问题
- [ ] 6.10 避坑指南

---

## 6.1 同步机制概述

### FreeRTOS 同步组件一览

| 组件 | 用途 | 特性 |
|------|------|------|
| 队列 | 任务间通信 | FIFO，可传输数据 |
| 二值信号量 | 同步 | 0/1 状态 |
| 计数信号量 | 资源计数 | 0~N 状态 |
| 互斥量 | 互斥访问 | 带优先级继承 |
| 递归互斥量 | 嵌套互斥 | 可嵌套获取 |
| 事件组 | 多事件同步 | 位标志组合 |
| 任务通知 | 轻量通信 | 最快但功能少 |

### 选择指南

```
需要传递数据？       → 队列 Queue
需要信号同步？       → 二值信号量 Binary Semaphore
需要计数资源？       → 计数信号量 Counting Semaphore
需要互斥访问？       → 互斥量 Mutex（优先级继承）
需要等待多个事件？   → 事件组 Event Group
需要最快速度？       → 任务通知 Task Notification
```

---

## 6.2 队列（Queue）

### 队列特性

- **FIFO** — 先进先出
- **可被多个任务写入** — 线程安全
- **可被多个任务读取** — 线程安全
- **阻塞机制** — 无数据/满时自动阻塞

### 创建队列

```c
#include "queue.h"

// 创建队列（发送 int 数据，深度 10）
QueueHandle_t xQueue = xQueueCreate(10, sizeof(int));
if (xQueue == NULL) {
    // 创建失败（堆内存不足）
}

// 创建队列（发送结构体）
typedef struct {
    uint8_t cmd;
    uint32_t param;
} Msg_t;

QueueHandle_t msgQueue = xQueueCreate(5, sizeof(Msg_t));
```

### 发送数据

```c
// 任务中发送（阻塞式）
int data = 100;
BaseType_t result = xQueueSend(xQueue, &data, portMAX_DELAY);
// result == pdPASS 表示成功

// 发送结构体
Msg_t msg = { .cmd = 1, .param = 1234 };
xQueueSend(msgQueue, &msg, 0);

// 中断中发送（非阻塞）
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### 接收数据

```c
// 任务中接收（阻塞式）
int received;
BaseType_t result = xQueueReceive(xQueue, &received, portMAX_DELAY);
// pdPASS 表示成功，received 中是接收到的数据

// 非阻塞接收（等待 100ms）
if (xQueueReceive(xQueue, &received, pdMS_TO_TICKS(100)) == pdPASS) {
    // 收到数据
} else {
    // 超时
}

// 中断中接收
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueReceiveFromISR(xQueue, &received, &xHigherPriorityTaskWoken);
```

### 队列读写原理

```
写入：
┌────────────────────────────────────────┐
│  data → [ item ] → [ item ] → [ item ] │ → 队列尾
└────────────────────────────────────────┘
         队列头（最先出）→

读取：
┌────────────────────────────────────────┐
│  data ← [ item ] ← [ item ] ← [ item ] │
└────────────────────────────────────────┘
         ← 取出最早进入的数据
```

---

## 6.3 二值信号量

### 用途

- 任务同步
- 中断与任务同步
- 替代标志位（避免轮询）

### 创建与使用

```c
#include "semphr.h"

// 创建二值信号量（初始为空）
SemaphoreHandle_t xBinarySemaphore = xSemaphoreCreateBinary();
if (xBinarySemaphore == NULL) {
    // 创建失败
}

// 中断中释放信号量（Give）
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xSemaphoreGiveFromISR(xBinarySemaphore, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

// 任务中获取信号量（Take）
if (xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdPASS) {
    // 获取成功，执行任务
}
```

### 典型场景：中断与任务同步

```c
// 中断处理程序
void USART1_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        // 收到数据，释放信号量通知任务
        xSemaphoreGiveFromISR(xBinarySemaphore, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 任务：等待串口数据
void UartTask(void *pv) {
    for (;;) {
        // 阻塞等待，直到信号量来
        if (xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdPASS) {
            // 读取数据并处理
            uint8_t data = USART_ReceiveData(USART1);
            // 处理数据...
        }
    }
}
```

---

## 6.4 计数信号量

### 用途

- 资源池管理（如 N 个缓冲区）
- 事件计数
- 多生产者多消费者

### 创建与使用

```c
// 创建计数信号量（最大计数 5，初始计数 3）
SemaphoreHandle_t xCountSemaphore = xSemaphoreCreateCounting(5, 3);
if (xCountSemaphore == NULL) {
    // 创建失败
}

// 释放（计数+1，达到上限后无效）
xSemaphoreGive(xCountSemaphore);

// 获取（计数-1，为0时阻塞）
if (xSemaphoreTake(xCountSemaphore, portMAX_DELAY) == pdPASS) {
    // 使用资源
}
```

### 典型场景：资源池

```c
#define BUFFER_COUNT  3
SemaphoreHandle_t xBufferSemaphore;

// 初始化：3 个缓冲区可用
xBufferSemaphore = xSemaphoreCreateCounting(BUFFER_COUNT, BUFFER_COUNT);

// 任务获取缓冲区
void *Buffer_Get(void) {
    if (xSemaphoreTake(xBufferSemaphore, pdMS_TO_TICKS(100)) == pdPASS) {
        return malloc(BUFFER_SIZE);  // 返回实际缓冲区
    }
    return NULL;  // 无可用缓冲区
}

// 任务归还缓冲区
void Buffer_Put(void *buf) {
    free(buf);
    xSemaphoreGive(xBufferSemaphore);
}
```

---

## 6.5 互斥量

### 用途

- 保护共享资源（如全局变量、外设）
- 防止数据竞争

### 创建与使用

```c
// 创建互斥量
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();
if (xMutex == NULL) {
    // 创建失败
}

// 获取互斥量
if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdPASS) {
    // 临界区代码
    // 访问共享资源

    xSemaphoreGive(xMutex);  // 释放
}
```

### 互斥量 vs 二值信号量

| 特性 | 互斥量 Mutex | 二值信号量 Binary Semaphore |
|------|-------------|---------------------------|
| 用途 | 互斥访问 | 同步 |
| 持有者 | 有（记录谁获取） | 无 |
| 优先级继承 | ✅ 有 | ❌ 无 |
| 递归获取 | ❌ 不支持 | ❌ 不支持 |
| 同一任务多次获取 | 死锁 | 可以 |

### 优先级继承示例

```c
// 任务 L（低优先级）持有互斥量
void LowPriorityTask(void *pv) {
    xSemaphoreTake(xMutex, portMAX_DELAY);  // 持有
    // 做耗时操作...
    // 如果期间高优先级任务想要获取互斥量
    // L 的优先级会被提升，避免被中优先级抢占
    xSemaphoreGive(xMutex);
}

// 任务 H（高优先级）等待互斥量
void HighPriorityTask(void *pv) {
    xSemaphoreTake(xMutex, portMAX_DELAY);  // 阻塞等待
    // 获取后执行
    xSemaphoreGive(xMutex);
}
```

---

## 6.6 递归互斥量

### 用途

- 同一任务需要多次获取同一互斥量
- 如递归函数中访问保护资源

### 创建与使用

```c
// 创建递归互斥量
SemaphoreHandle_t xRecursiveMutex = xSemaphoreCreateRecursiveMutex();

// 获取（同一任务可多次获取）
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);  // 嵌套获取

// 释放次数必须与获取次数相同
xSemaphoreGiveRecursive(xRecursiveMutex);
xSemaphoreGiveRecursive(xRecursiveMutex);

// 错误示例：获取后不释放
void BadRecursion(void) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    // 递归调用自己
    BadRecursion();  // 会死锁！
}
```

---

## 6.7 事件组

### 用途

- 等待多个事件任意一个
- 等待多个事件全部到达
- 任务间简单广播

### 创建与使用

```c
#include "event_groups.h"

// 创建事件组
EventGroupHandle_t xEventGroup = xEventGroupCreate();

// 设置事件位
#define BIT_0    (1 << 0)
#define BIT_1    (1 << 1)
#define BIT_2    (1 << 2)

// 任务中设置事件
xEventGroupSetBits(xEventGroup, BIT_0 | BIT_1);

// 中断中设置事件
BaseType_t xHigherPriorityTaskWoken;
xEventGroupSetBitsFromISR(xEventGroup, BIT_0, &xHigherPriorityTaskWoken);

// 等待事件
// 等待 BIT_0 或 BIT_1 任一发生（逻辑 OR）
EventBits_t bits = xEventGroupWaitBits(
    xEventGroup,
    BIT_0 | BIT_1,           // 等待的位
    pdTRUE,                  // 退出时清除已等待的位
    pdFALSE,                 // pdTRUE=AND(全部), pdFALSE=OR(任意)
    portMAX_DELAY            // 超时
);

if (bits & BIT_0) {
    // BIT_0 发生了
}
```

### 事件组同步示例

```c
// 等待多个任务完成
void MainTask(void *pv) {
    // 等待任务A和任务B都完成
    EventBits_t bits = xEventGroupWaitBits(
        xEventGroup,
        TASK_A_DONE | TASK_B_DONE,
        pdTRUE,           // 清除已等待的位
        pdTRUE,           // 等待全部（AND）
        portMAX_DELAY
    );

    if (bits == (TASK_A_DONE | TASK_B_DONE)) {
        // 两个任务都完成了，继续执行
    }
}

void TaskA(void *pv) {
    // 执行...
    xEventGroupSetBits(xEventGroup, TASK_A_DONE);
    vTaskDelete(NULL);
}

void TaskB(void *pv) {
    // 执行...
    xEventGroupSetBits(xEventGroup, TASK_B_DONE);
    vTaskDelete(NULL);
}
```

---

## 6.8 任务通知

### 用途

- 最轻量级通信
- 无需单独创建对象
- 直接发送给指定任务

### API

```c
// 发送任务通知
BaseType_t xTaskNotify(
    TaskHandle_t xTaskToNotify,   // 目标任务句柄
    uint32_t ulValue,             // 通知值
    eNotifyAction eAction         // 通知方式
);

// 通知方式
// eSetBits          → 通知值 OR ulValue
// eIncrement        → 通知值 +1
// eSetValueWithOverwrite → 覆盖写入（丢失旧值）
// eSetValueWithoutOverwrite → 不覆盖（旧值未读才写入）

// 等待任务通知（接收方）
uint32_t ulNotifyValue;
BaseType_t result = ulTaskNotifyTake(
    pdTRUE,              // pdTRUE=清除计数, pdFALSE=减法
    portMAX_DELAY        // 超时
);
```

### 任务通知示例

```c
// 发送任务通知
void SenderTask(void *pv) {
    for (;;) {
        // 发送通知，通知值 +1
        xTaskNotify(xTargetTask, 1, eIncrement);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 接收任务通知
void ReceiverTask(void *pv) {
    for (;;) {
        // 等待通知
        uint32_t notifyValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 处理收到的事件（notifyValue 次）
        for (uint32_t i = 0; i < notifyValue; i++) {
            // 处理
        }
    }
}
```

### 任务通知 vs 队列/信号量

| 特性 | 任务通知 | 队列/信号量 |
|------|---------|------------|
| 内存开销 | 最小（无对象） | 需要创建对象 |
| 速度 | 最快 | 较慢 |
| 功能 | 有限 | 丰富 |
| 只能一对一 | 是 | 否 |
| 可靠性 | 替代方案 | 更可靠 |

---

## 6.9 面试高频问题

### Q1：信号量和互斥量的区别？

**参考答案：**
| 特性 | 信号量 | 互斥量 |
|------|--------|--------|
| 用途 | 同步/资源计数 | 互斥访问 |
| 持有者记录 | 无 | 有 |
| 优先级继承 | 无 | 有 |
| 递归获取 | 支持（计数信号量） | 支持（递归互斥量） |
| 释放者 | 任意任务 | 持有者 |

---

### Q2：什么是优先级反转？如何解决？

**参考答案：**
- 高优先级任务等待低优先级任务持有的资源
- 中优先级任务抢占低优先级任务
- 高优先级任务实际等待时间更长

**解决：**
- 优先级继承：低优先级持有者临时提升到高优先级
- 互斥量自动实现优先级继承
- 二值信号量无优先级继承

---

### Q3：队列和信号量的区别？

| 特性 | 队列 | 信号量 |
|------|------|--------|
| 数据传输 | ✅ 有（数据副本） | ❌ 无 |
| 阻塞机制 | ✅ 发送和接收都支持 | 仅获取时阻塞 |
| 多消费者 | ✅ 支持 | 部分支持 |
| 多生产者 | ✅ 支持 | 部分支持 |

---

### Q4：什么时候用事件组？

**参考答案：**
- 需要等待多个事件任意一个发生（OR）
- 需要等待多个事件全部发生（AND）
- 不需要传输数据，只需状态同步
- 简单广播场景

---

### Q5：任务通知有什么限制？

**参考答案：**
- 一对一通信，不能多消费者
- 无阻塞机制（等待方会立即返回或永久等待）
- 无队列缓冲，不能缓存数据
- 通知值只有 32 位

---

## 6.10 避坑指南

1. **互斥量必须在持有者的同一任务中释放** — 不能在其他任务中释放
2. **递归互斥量获取/释放必须配对** — 获取 N 次必须释放 N 次
3. **中断中不能用 `xSemaphoreTake`** — 只能用 `FromISR` 版本
4. **不要在持有互斥量时阻塞** — 会导致其他任务永久等待
5. **信号量/队列创建失败要检查** — 内存不足时返回 NULL
6. **任务通知前目标任务必须存在** — 否则无效

---

## 6.11 队列内部实现深度解析

### 6.11.1 xQUEUE 结构体完整定义

队列是 FreeRTOS 中最核心的通信机制，理解其内部结构对于掌握整个操作系统至关重要。以下是 `queue.c` 中队列结构体的完整定义：

```c
// queue.c 中的队列结构体定义
typedef struct QueueDefinition {
    // 指向队列存储区起始位置的指针
    int8_t *pcHead;

    // 指向队列存储区结束位置的指针
    int8_t *pcTail;

    // 指向下次写入位置的指针（环形缓冲区写指针）
    int8_t *pcWriteTo;

    // 指向下次读取位置的指针（环形缓冲区读指针）
    int8_t *pcReadFrom;

    // 等待发送的任务列表（阻塞在该列表上的任务等待往队列发送数据）
    List_t xTasksWaitingToSend;

    // 等待接收的任务列表（阻塞在该列表上的任务等待从队列接收数据）
    List_t xTasksWaitingToReceive;

    // 队列的最大长度（可容纳的最大消息数）
    volatile UBaseType_t uxLength;

    // 每个消息的字节大小
    volatile UBaseType_t uxItemSize;

    // 当前队列中等待的消息数量
    volatile int8_t cMessagesWaiting;

    #if ( configUSE_QUEUE_SETS == 1 )
        // 队列集容器指针（用于队列集功能）
        void *pxQueueSetContainer;
    #endif

    #if ( configUSE_MUTEXES == 1 )
        // 互斥量持有计数（记录当前持有该互斥量的次数）
        UBaseType_t uxMutexHeldCount;
    #endif

    #if ( configUSE_MUTEXES == 1 )
        // 互斥量持有者指针（记录谁持有该互斥量，用于优先级继承）
        TaskHandle_t pxMutexHolder;
    #endif
} xQUEUE;

// 队列句柄类型（指向 xQUEUE 结构体的指针）
typedef xQUEUE * QueueHandle_t;
```

**结构体成员详解：**

| 成员 | 类型 | 说明 |
|------|------|------|
| `pcHead` | `int8_t*` | 队列存储区的起始地址，用于释放内存时定位 |
| `pcTail` | `int8_t*` | 队列存储区的结束地址 |
| `pcWriteTo` | `int8_t*` | 环形缓冲区写指针，指向下次写入位置 |
| `pcReadFrom` | `int8_t*` | 环形缓冲区读指针，指向下次读取位置 |
| `xTasksWaitingToSend` | `List_t` | 等待发送的任务列表（队列满时阻塞） |
| `xTasksWaitingToReceive` | `List_t` | 等待接收的任务列表（队列空时阻塞） |
| `uxLength` | `UBaseType_t` | 队列深度，即最大消息数 |
| `uxItemSize` | `UBaseType_t` | 每个消息的字节大小 |
| `cMessagesWaiting` | `int8_t` | 当前队列中的消息数量 |
| `pxMutexHolder` | `TaskHandle_t` | 互斥量持有者（仅用于互斥量） |
| `uxMutexHeldCount` | `UBaseType_t` | 递归持有计数（仅用于递归互斥量） |

### 6.11.2 队列环形缓冲区原理图解

队列内部使用环形缓冲区（Ring Buffer）实现，巧妙地利用指针操作实现 FIFO 语义。以下是详细的图解说明：

```
=============================================================
队列环形缓冲区工作原理（长度=5，每个元素=4字节）
=============================================================

【初始状态（空队列）】

pcHead ──────► ┌────┬────┬────┬────┬────┐
               │    │    │    │    │    │
               └────┴────┴────┴────┴────┘
               ↑
               pcWriteTo（写入指针指向索引0）
               pcReadFrom（读取指针指向索引0）

               cMessagesWaiting = 0
               uxLength = 5
               uxItemSize = 4

【队列满状态】

               ┌────┬────┬────┬────┬────┐
               │ A  │ B  │ C  │ D  │ E  │
               └────┴────┴────┴────┴────┘
                                    ↑
                                    pcWriteTo（下次写入位置，已绕回索引0）
               pcReadFrom───────────┘（指向索引0，因为A已被读取）

               cMessagesWaiting = 5
               此时调用 xQueueSend() 会阻塞（如果指定了阻塞时间）

【环形缓冲区的"回绕"机制】

写入操作时，如果 pcWriteTo 到达末尾：
┌────┬────┬────┬────┬────┐
│ 5  │ 6  │ 7  │ 8  │ 4  │  写入 9: pcWriteTo 从索引4回绕到索引0
└────┴────┴────┴────┴────┘
                      ↑
                      pcWriteTo 指向索引4（末尾）

执行写入 9 后：
┌────┬────┬────┬────┬────┐
│ 9  │ 6  │ 7  │ 8  │ 4  │  pcWriteTo 回绕到索引0
└────┴────┴────┴────┴────┘
↑
pcWriteTo 现在指向索引0

读取操作同样会回绕：
pcReadFrom 从索引0 → 索引1 → ... → 索引4 → 索引0 → ...
```

### 6.11.3 队列操作详细流程图

```
【发送数据 xQueueSend() 流程】

                    开始
                      │
                      ▼
            ┌─────────────────────┐
            │ 获取队列锁（进入临界区）│
            └─────────────────────┘
                      │
                      ▼
            ┌─────────────────────┐
            │ 检查队列是否有空间     │
            │ uxMessagesWaiting   │
            │     < uxLength ?     │
            └─────────────────────┘
                │             │
               是            否（队列满）
                │             │
                ▼             ▼
    ┌───────────────────┐  ┌─────────────────────┐
    │ 复制数据到队列     │  │ xTicksToWait == 0 ?  │
    │ pcWriteTo → 存储区 │  └─────────────────────┘
    └───────────────────┘      │           │
                │             是           否
                ▼             ▼             ▼
    ┌───────────────────┐  ┌────────┐  ┌──────────────────┐
    │ cMessagesWaiting++│  │立即返回│  │ 任务加入发送等待列 │
    └───────────────────┘  │errQUEUE_FULL│ │ xTasksWaitingToSend │
                │         └────────┘  │ 执行 taskYIELD()    │
                ▼                      │ 触发调度            │
    ┌───────────────────┐              └──────────────────┘
    │ 有任务在等待接收？  │                    │
    │ (xTasksWaitingTo  │◄───────────────────┘
    │  Receive 不为空)   │
    └───────────────────┘
           │         │
          是         否
           │         │
           ▼         ▼
   ┌──────────────┐  ┌─────────────────────┐
   │ 唤醒等待队列中 │  │ 释放队列锁（退出临界区）│
   │ 首个任务       │  └─────────────────────┘
   │ (通过事件列表)  │            │
   └──────────────┘            │
           │                   ▼
           └──────► 返回 pdPASS

【接收数据 xQueueReceive() 流程】

                    开始
                      │
                      ▼
            ┌─────────────────────┐
            │ 获取队列锁（进入临界区）│
            └─────────────────────┘
                      │
                      ▼
            ┌─────────────────────┐
            │ 检查队列是否有消息   │
            │ cMessagesWaiting > 0?│
            └─────────────────────┘
                │             │
               是            否（队列空）
                │             │
                ▼             ▼
    ┌───────────────────┐  ┌─────────────────────┐
    │ 从 pcReadFrom 复制  │  │ xTicksToWait == 0 ?  │
    │ 数据到 pvBuffer    │  └─────────────────────┘
    └───────────────────┘      │           │
                │             是           否
                ▼             ▼             ▼
    ┌───────────────────┐  ┌────────┐  ┌──────────────────┐
    │ pcReadFrom 移动到 │  │立即返回│  │ 任务加入接收等待列 │
    │ 下一位置（环形）   │  │ pdFALSE│  │ xTasksWaitingTo   │
    └───────────────────┘  └────────┘  │ Receive           │
                │                      │ 执行 taskYIELD()  │
                ▼                      └──────────────────┘
    ┌───────────────────┐                     │
    │ cMessagesWaiting--│◄────────────────────┘
    └───────────────────┘
                │
                ▼
    ┌───────────────────┐
    │ 有任务在等待发送？  │
    │ (xTasksWaitingTo  │
    │  Send 不为空)      │
    └───────────────────┘
           │         │
          是         否
           │         │
           ▼         ▼
   ┌──────────────┐  ┌─────────────────────┐
   │ 唤醒等待队列中 │  │ 释放队列锁（退出临界区）│
   │ 首个任务       │  └─────────────────────┘
   │ (通过事件列表)  │            │
   └──────────────┘            │
           │                   ▼
           └──────► 返回 pdPASS
```

### 6.11.4 xQueueGenericSend() 源码详解

`xQueueGenericSend()` 是所有队列发送操作的内部核心函数，支持队列头插入（`queueSEND_TO_BACK`）和队列尾插入（`queueSEND_TO_FRONT`）两种模式。

```c
// queue.c 中的核心发送函数
BaseType_t xQueueGenericSend(QueueHandle_t xQueue,
                               const void *pvItemToQueue,
                               TickType_t xTicksToWait,
                               BaseType_t xCopyPosition) {
    BaseType_t xEntryTimeSet = pdFALSE;    // 是否已设置超时时间
    BaseType_t xNeedToYield = pdFALSE;      // 是否需要触发任务调度
    TimeOut_t xTimeOut;                     // 超时结构体

    // 无限循环实现阻塞机制
    for (;;) {
        taskENTER_CRITICAL();               // 进入临界区（关闭中断）

        // 检查队列是否有空间
        if (uxMessagesWaiting < uxLength) {
            // 队列有空间，执行复制操作
            prvCopyDataToQueue(xQueue, pvItemToQueue, xCopyPosition);

            // 复制完成后，增加消息计数
            uxMessagesWaiting++;

            taskEXIT_CRITICAL();            // 退出临界区

            // 检查是否有任务在等待接收数据
            if (listIS_EMPTY(&xQueue->xTasksWaitingToReceive) == pdFALSE) {
                // 有等待接收的任务，唤醒它
                if (xTaskRemoveFromEventList(
                        &xQueue->xTasksWaitingToReceive) != pdFALSE) {
                    // 被唤醒的任务优先级高于当前任务，需要调度
                    xNeedToYield = pdTRUE;
                }
            }

            // 发送成功，返回
            return pdPASS;
        }

        // ========== 以下为队列满时的处理 ==========

        // 如果指定不阻塞（xTicksToWait == 0），立即返回
        if (xTicksToWait == 0) {
            taskEXIT_CRITICAL();
            return errQUEUE_FULL;
        }

        // 如果还没设置超时时间，现在设置
        if (xEntryTimeSet == pdFALSE) {
            vTaskSetTimeOutState(&xTimeOut);    // 记录开始时间
            xEntryTimeSet = pdTRUE;              // 标记已设置
        }

        // 检查是否已超时
        if (xTaskCheckTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE) {
            // 还没超时，任务应该阻塞等待

            taskEXIT_CRITICAL();                // 先退出临界区

            // 任务添加到等待发送列表（使用无序事件列表）
            vTaskPlaceOnUnorderedEventList(
                &xQueue->xTasksWaitingToSend,
                xTicksToWait);

            // 触发PendSV中断进行上下文切换
            portYIELD();

            // 当任务从阻塞中恢复时，会从这里继续执行
            // 重新进入循环检查队列是否有空间
        }
        // 如果已超时，循环继续，但这次 xTicksToWait 已变为0
        // 所以下一次会执行 "if (xTicksToWait == 0)" 分支并返回 errQUEUE_FULL
    }
}
```

**关键点分析：**

1. **临界区保护**：`taskENTER_CRITICAL()` 和 `taskEXIT_CRITICAL()` 配对使用，确保队列操作的原子性。

2. **阻塞机制**：使用无限循环 + 超时检查实现阻塞等待。当队列满时，任务将自己添加到 `xTasksWaitingToSend` 列表并触发调度。

3. **任务唤醒**：当有数据被接收（其他任务调用 `xQueueReceive`）时，会从 `xTasksWaitingToReceive` 中唤醒等待的任务。

4. **优先级继承**：通过 `xTaskRemoveFromEventList()` 唤醒任务时，会处理优先级继承（如果使用的是互斥量）。

### 6.11.5 xQueueReceive() 源码详解

`xQueueReceive()` 是队列接收的核心函数，与发送函数对称。

```c
// queue.c 中的接收函数
BaseType_t xQueueReceive(QueueHandle_t xQueue,
                          void *pvBuffer,
                          TickType_t xTicksToWait) {
    BaseType_t xEntryTimeSet = pdFALSE;
    TimeOut_t xTimeOut;

    for (;;) {
        taskENTER_CRITICAL();                   // 进入临界区

        // 检查队列是否有消息
        if (uxMessagesWaiting > (UBaseType_t) 0) {
            // 有消息，执行复制操作
            prvCopyDataFromQueue(xQueue, pvBuffer);

            // 复制完成后，减少消息计数
            uxMessagesWaiting--;

            taskEXIT_CRITICAL();                // 退出临界区

            // 检查是否有任务在等待发送数据
            if (listIS_EMPTY(&xQueue->xTasksWaitingToSend) == pdFALSE) {
                // 有等待发送的任务，唤醒它
                if (xTaskRemoveFromEventList(
                        &xQueue->xTasksWaitingToSend) != pdFALSE) {
                    // 被唤醒的任务优先级高于当前任务
                    xNeedToYield = pdTRUE;
                }
            }

            return pdPASS;                       // 接收成功
        }

        // ========== 以下为队列空时的处理 ==========

        // 如果指定不阻塞，立即返回
        if (xTicksToWait == 0) {
            taskEXIT_CRITICAL();
            return pdFALSE;                      // 返回 pdFALSE 表示没收到
        }

        // 如果还没设置超时时间，现在设置
        if (xEntryTimeSet == pdFALSE) {
            vTaskSetTimeOutState(&xTimeOut);
            xEntryTimeSet = pdTRUE;
        }

        // 检查是否已超时
        if (xTaskCheckTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE) {
            // 还没超时，任务应该阻塞等待
            taskEXIT_CRITICAL();

            // 任务添加到等待接收列表
            vTaskPlaceOnUnorderedEventList(
                &xQueue->xTasksWaitingToReceive,
                xTicksToWait);

            portYIELD();                         // 触发上下文切换
        }
    }
}
```

### 6.11.6 数据复制函数详解

FreeRTOS 使用专门的函数处理数据的复制，支持不同大小数据的拷贝优化：

```c
// 将数据复制到队列（发送时调用）
static void prvCopyDataToQueue(QueueDefinition_t *pxQueue,
                                const void *pvItemToQueue,
                                BaseType_t xCopyPosition) {
    if (pxQueue->uxItemSize == (UBaseType_t) 0) {
        // 互斥量/信号量的情况：不需要复制数据
        return;
    }

    if (xCopyPosition == queueSEND_TO_BACK) {
        // 发送到队列尾部（FIFO，默认行为）
        (void) memcpy(pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize);
        pxQueue->pcWriteTo += pxQueue->uxItemSize;

        // 检查是否到达队列末尾，需要回绕
        if (pxQueue->pcWriteTo >= pxQueue->pcTail) {
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
    } else if (xCopyPosition == queueSEND_TO_FRONT) {
        // 发送到队列头部（LIFO，用于紧急消息）
        pxQueue->pcReadFrom -= pxQueue->uxItemSize;

        // 检查是否回绕到队列末尾之后
        if (pxQueue->pcReadFrom < pxQueue->pcHead) {
            pxQueue->pcReadFrom = pxQueue->pcTail - pxQueue->uxItemSize;
        }

        (void) memcpy(pxQueue->pcReadFrom, pvItemToQueue, pxQueue->uxItemSize);
    }
}

// 从队列复制数据到缓冲区（接收时调用）
static void prvCopyDataFromQueue(QueueDefinition_t *pxQueue,
                                   void *const pvBuffer) {
    if (pxQueue->uxItemSize == (UBaseType_t) 0) {
        // 信号量的情况：不需要复制数据
        return;
    }

    // 从 pcReadFrom 位置复制数据
    (void) memcpy(pvBuffer, pxQueue->pcReadFrom, pxQueue->uxItemSize);

    // 移动读指针到下一个位置
    pxQueue->pcReadFrom += pxQueue->uxItemSize;

    // 检查是否到达队列末尾，需要回绕
    if (pxQueue->pcReadFrom >= pxQueue->pcTail) {
        pxQueue->pcReadFrom = pxQueue->pcHead;
    }
}
```

---

## 6.12 信号量内部实现深度解析

### 6.12.1 信号量是特殊的队列

FreeRTOS 的信号量实际上是使用队列实现的，这是设计上的巧妙之处：

```
┌─────────────────────────────────────────────────────────────────────┐
│                        信号量实现原理                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   二值信号量  =  长度为1，元素大小为0的队列                            │
│                                                                     │
│   计数信号量  =  长度为N，元素大小为0的队列                            │
│                                                                     │
│   互斥量     =  长度为1，元素大小为0的队列 + 持有者记录 + 优先级继承   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.12.2 二值信号量创建与实现

```c
// semphr.h 中的二值信号量创建
SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    QueueHandle_t xQueue;

    // 长度为1，元素大小为0（二值信号量不存储数据）
    xQueue = xQueueCreate((UBaseType_t) 1, (UBaseType_t) 0);

    if (xQueue != NULL) {
        // 二值信号量初始为空（计数为0）
        // 但队列刚创建时 cMessagesWaiting = 0，已经符合要求
        return xQueue;
    }

    return NULL;
}

// 二值信号量的 Give 操作（发送）
// 等价于 xQueueSend(xQueue, NULL, 0)
// 因为元素大小为0，所以不需要实际复制数据
#define xSemaphoreGive(xSemaphore) \
    xQueueSend((xSemaphore), NULL, (TickType_t) 0)

// 二值信号量的 Take 操作（接收）
// 等价于 xQueueReceive(xSemaphore, NULL, portMAX_DELAY)
#define xSemaphoreTake(xSemaphore, xBlockTime) \
    xQueueReceive((xSemaphore), NULL, (xBlockTime))
```

**关键点**：二值信号量的"有"或"无"状态实际上是通过队列中的消息数量（`cMessagesWaiting`）来实现的：
- `cMessagesWaiting = 0` → 信号量不可用（"无"）
- `cMessagesWaiting = 1` → 信号量可用（"有"）

### 6.12.3 计数信号量创建与实现

```c
// 计数信号量创建
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount,
                                           UBaseType_t uxInitialCount) {
    QueueHandle_t xQueue;

    configASSERT(uxMaxCount != 0);
    configASSERT(uxInitialCount <= uxMaxCount);

    // 长度为 uxMaxCount，元素大小为0
    xQueue = xQueueCreate(uxMaxCount, (UBaseType_t) 0);

    if (xQueue != NULL) {
        // 初始化消息数量为 uxInitialCount
        // 注意：xQueueCreate 内部 cMessagesWaiting = 0
        // 所以需要手动设置为初始计数
        xQueue->cMessagesWaiting = (int8_t) uxInitialCount;
        return xQueue;
    }

    return NULL;
}

// 计数信号量的 Give 操作
BaseType_t xSemaphoreGive(xSemaphore) {
    return xQueueSend(xSemaphore, NULL, 0);
}

// 计数信号量的 Take 操作
BaseType_t xSemaphoreTake(xSemaphore, timeout) {
    return xQueueReceive(xSemaphore, NULL, timeout);
}
```

### 6.12.4 互斥量创建与实现

```c
// 互斥量创建
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    QueueHandle_t xQueue;

    // 互斥量本质上是长度为1，元素大小为0的队列
    // 但加上了一些特殊处理：持有者记录和优先级继承
    xQueue = xQueueCreate((UBaseType_t) 1, (UBaseType_t) 0);

    if (xQueue != NULL) {
        // 标记这是一个互斥量（通过设置特殊的 uxItemSize 值）
        // 注意：互斥量的 uxItemSize 实际被设置为 mutexQUEUE_TYPE
        xQueue->uxQueueType = queueQUEUE_TYPE_MUTEX;

        // 互斥量初始可用（持有计数为0）
        xQueue->uxMessagesWaiting = (UBaseType_t) 0;

        // 互斥量持有者初始化为 NULL
        xQueue->pxMutexHolder = NULL;
        xQueue->uxMutexHeldCount = (UBaseType_t) 0;

        return xQueue;
    }

    return NULL;
}

// 互斥量的 Take 操作（关键区别：实现了优先级继承）
BaseType_t xSemaphoreTake(QueueHandle_t xMutex, TickType_t xBlockTime) {
    BaseType_t xResult;

    // 调用通用队列接收函数
    xResult = xQueueGenericReceive(xMutex, NULL, xBlockTime, pdFALSE);

    // 如果成功获取，检查是否需要优先级继承
    if (xResult == pdPASS) {
        // 记录互斥量持有者（当前任务）
        xMutex->pxMutexHolder = xTaskGetCurrentTaskHandle();
        xMutex->uxMutexHeldCount++;
    }

    return xResult;
}

// 互斥量的 Give 操作（关键区别：只能由持有者释放）
BaseType_t xSemaphoreGive(QueueHandle_t xMutex) {
    BaseType_t xResult;
    TaskHandle_t pxMutexHolder;

    // 验证当前任务是否是互斥量的持有者
    // 这是互斥量和二值信号量的关键区别！
    if (xMutex->pxMutexHolder == xTaskGetCurrentTaskHandle()) {
        // 当前任务是持有者，可以释放
        xResult = xQueueGenericSend(xMutex, NULL, queueSEND_TO_BACK);

        if (xResult == pdPASS) {
            // 成功释放
            xMutex->uxMutexHeldCount--;

            if (xMutex->uxMutexHeldCount == 0) {
                // 完全释放了互斥量
                xMutex->pxMutexHolder = NULL;
            }
        }
    } else {
        // 错误：非持有者试图释放互斥量！
        xResult = errQUEUE_FULL;
    }

    return xResult;
}
```

---

## 6.13 优先级继承深度解析

### 6.13.1 什么是优先级继承

优先级继承是一种解决优先级反转问题的机制。优先级反转发生在以下场景：

```
【优先级反转问题示意】

时间 ─────────────────────────────────────────────────►

低优先级任务 L 持有互斥量 M
    ┌────────────────────┐
    │ L: 持有 M, 优先级=1 │◄────────────────────┐
    └────────────────────┘                      │
                         中优先级任务 M 抢占     │
                              ┌────────────────────┐
                              │ M: 运行中, 优先级=2 │
                              │ (与 M 无关，只是抢了L)│
                              └────────────────────┘
                                                      │
                            高优先级任务 H 等待 M     │
                                  ┌────────────────────┐
                                  │ H: 等待 M, 优先级=3 │
                                  └────────────────────┘

结果：L 被 M 抢占 → H 等待 L 释放 M → H 实际等待时间比 M 还长！

这就是"优先级反转"——高优先级任务反而等待更长时间
```

**优先级继承解决方案**：

```
【启用优先级继承后】

时间 ─────────────────────────────────────────────────►

L 持有 M 后，其优先级被提升到 H 的级别
    ┌────────────────────┐
    │ L: 持有 M, 优先级=3 │◄─── 继承自 H 的优先级
    └────────────────────┘
                   M 任务无法抢占（优先级低于 L）
                              ┌────────────────────┐
                              │ M: 就绪但无法运行    │
                              │ 优先级=2 < 3       │
                              └────────────────────┘

H 等待 M
    ┌────────────────────┐
    │ H: 等待 M, 优先级=3 │
    └────────────────────┘

L 释放 M 后，优先级恢复，H 立即获得 M 的执行权

结果：H 的等待时间大大缩短！
```

### 6.13.2 优先级继承源码实现

```c
// task.c 中的优先级继承实现

// 获取互斥量时调用（提升持有者优先级）
void vTaskPriorityInherit(TaskHandle_t pxMutexHolder,
                           UBaseType_t uxBasePriority) {
    // 仅在持有者存在且当前优先级低于继承优先级时提升
    if (pxMutexHolder != NULL) {
        if (pxMutexHolder->uxPriority < uxBasePriority) {
            // 记录原始优先级（用于后续恢复）
            pxMutexHolder->uxBasePriority = uxBasePriority;

            // 提升任务优先级
            pxMutexHolder->uxPriority = uxBasePriority;

            // 重新排序就绪列表（任务可能需要移动到更高优先级队列）
            if (pxMutexHolder->eCurrentState == eReady) {
                prvReAddTaskToReadyList(pxMutexHolder);
            }
        }
    }
}

// 释放互斥量时调用（恢复持有者优先级）
void vTaskPriorityDisinherit(TaskHandle_t pxMutexHolder) {
    if (pxMutexHolder != NULL) {
        // 检查是否还有其他互斥量持有
        if (pxMutexHolder->uxMutexHeldCount == 0) {
            // 完全释放，恢复原始优先级
            pxMutexHolder->uxPriority = pxMutexHolder->uxOriginalPriority;
        }
    }
}
```

### 6.13.3 优先级继承的边界情况

```
【嵌套互斥量的优先级继承】

场景：任务 A 按顺序持有互斥量 M1 和 M2

时间 ─────────────────────────────────────────────────►

A: take(M1) take(M2)
   ┌──────────────────────────────┐
   │ 优先级 = max(继承优先级)      │
   │ 如果 H 等待 M1，M2，优先级会   │
   │ 继承到 H 的最高优先级          │
   └──────────────────────────────┘

A: give(M2)  // 仍持有 M1，优先级保持
   ┌──────────────────────────────┐
   │ uxMutexHeldCount(M1) > 0      │
   │ 优先级不恢复                   │
   └──────────────────────────────┘

A: give(M1)  // 完全释放，优先级恢复
   ┌──────────────────────────────┐
   │ uxMutexHeldCount(M1) == 0     │
   │ 优先级恢复到原始值              │
   └──────────────────────────────┘
```

---

## 6.14 死锁（Deadlock）深度分析

### 6.14.1 死锁的定义与必要条件

死锁是指两个或多个任务相互等待对方持有的资源，导致都无法继续执行。

```
【死锁的四个必要条件（Coffman 条件）】

1. 互斥条件
   - 资源只能被一个任务持有
   - 互斥量天然满足此条件

2. 持有并等待
   - 任务持有资源的同时等待其他资源
   - 例如：持有 M1 的同时等待 M2

3. 不可抢占条件
   - 资源不能被强制夺走
   - 互斥量必须由持有者释放

4. 循环等待条件
   - 形成循环等待链
   - 例如：T1 等 T2，T2 等 T1
```

### 6.14.2 死锁示例（使用互斥量）

```
【经典死锁场景：两个互斥量】

Task A:                      Task B:
────────                      ────────
take(M1)                      take(M2)
    │                            │
    ▼                            ▼
take(M2)                      take(M1)
    │ (阻塞等待 B 释放 M2)        │ (阻塞等待 A 释放 M1)
    ▼                            ▼
    ...                         ...
    │                            │
    ▼                            ▼
(永远执行不到这里)              (永远执行不到这里)

【时序图】

     时间 ▼
     A: ──take(M1)──take(M2)───────────────────
     B: ──────────take(M2)──take(M1)───────────
                    ↑           ↑
                    │           │
                    └── 互相等待 ──┘
                      死锁！
```

### 6.14.3 死锁避免策略

```c
// 策略1：固定顺序获取互斥量（最常用）
// 始终按照 M1 → M2 的顺序获取

void TaskA(void *pv) {
    for (;;) {
        take(M1);              // 始终先拿 M1
        take(M2);              // 再拿 M2
        // 操作受保护的资源
        give(M2);
        give(M1);
    }
}

void TaskB(void *pv) {
    for (;;) {
        take(M1);              // 同样先拿 M1
        take(M2);              // 再拿 M2
        // 操作受保护的资源
        give(M2);
        give(M1);
    }
}

// 策略2：使用超时避免永久死锁
BaseType_t try_take_with_timeout(SemaphoreHandle_t sem, TickType_t timeout) {
    BaseType_t result = xSemaphoreTake(sem, timeout);
    if (result != pdPASS) {
        // 获取失败，可能是超时（可能死锁）
        // 执行错误处理或重试
    }
    return result;
}

// 策略3：检测并恢复（高级）
void detect_deadlock_and_recover(void) {
    uint32_t deadlock_count = 0;

    while (1) {
        if (try_take_with_timeout(M1, pdMS_TO_TICKS(100)) == pdPASS) {
            if (try_take_with_timeout(M2, pdMS_TO_TICKS(100)) == pdPASS) {
                // 成功获取，执行操作
                do_work();
                give(M2);
                give(M1);
                deadlock_count = 0;  // 重置计数
            } else {
                // 获取 M2 超时，释放 M1
                give(M1);
                deadlock_count++;
            }
        }

        if (deadlock_count > MAX_DEADLOCK_THRESHOLD) {
            // 检测到可能的死锁问题，执行恢复
            vTaskDelay(pdMS_TO_TICKS(1000));  // 等待一段时间
            deadlock_count = 0;
        }
    }
}
```

### 6.14.4 哲学家就餐问题

```
【经典死锁问题：哲学家就餐】

     0
   ╭───╮
   │   │
 5 ╰─┬─╯ 1
   │ │
   │ │
 4 ╰─┬─╯ 2
   ╰───╯
     3

每个哲学家需要同时持有左右两把叉子才能吃饭。

错误解法（死锁）：
   每个哲学家都先拿左边的叉子，再拿右边的叉子。

正确解法：
   方案A：奇数号哲学家先拿左边，偶数号先拿右边
   方案B：最多允许 N-1 个哲学家同时尝试进餐
   方案C：使用超时和重试机制
```

```c
// 方案A实现：打破循环等待
void PhilosopherTask(void *pv) {
    int id = philosopher_id;  // 哲学家的编号

    for (;;) {
        think();

        if (id % 2 == 0) {
            // 偶数号：先左后右
            take_fork(left(id));
            take_fork(right(id));
        } else {
            // 奇数号：先右后左
            take_fork(right(id));
            take_fork(left(id));
        }

        eat();

        give_fork(left(id));
        give_fork(right(id));
    }
}
```

---

## 6.15 面试高频问题深度解答

### Q1：信号量和互斥量的区别？【详细解答】

**参考答案：**

| 维度 | 信号量 | 互斥量 |
|------|--------|--------|
| **设计目的** | 同步/资源计数 | 互斥访问 |
| **持有者记录** | 无记录 | 记录持有者任务 |
| **优先级继承** | 无 | 有 |
| **释放者限制** | 任意任务可释放 | 必须由持有者释放 |
| **递归获取** | N/A（计数信号量可多次Take） | 支持（递归互斥量） |
| **实现原理** | 队列（长度为N，元素大小为0） | 队列 + 持有者 + 优先级继承 |
| **典型用途** | 任务同步、中断通知、资源池 | 保护共享资源 |

**为什么互斥量要有持有者记录？**
- 确保资源被正确保护
- 防止其他任务误释放
- 实现优先级继承

**为什么二值信号量不需要持有者记录？**
- 用于同步，不涉及资源保护
- 任何任务都可以"给出"信号量

### Q2：什么是优先级反转？如何解决？【详细解答】

**参考答案：**

```
【优先级反转的三种情况】

情况1：直接优先级反转
┌────────────────────────────────────────────────────┐
│ L（低）持有 M ──► H（高）等待 M ──► L 被 H 抢占      │
│                                                     │
│ 预期：L 先执行，然后 H                               │
│ 实际：H 阻塞等待 L，L 被 H 抢占（实际反转）           │
└────────────────────────────────────────────────────┘

情况2：继承优先级反转（更严重）
┌────────────────────────────────────────────────────┐
│ L（优先级1）持有 M ──► M（中优先级2）抢占 L ──► H（优先级3）等待 M │
│                                                     │
│ 结果：H 等待 M 的时间比 M 运行时间还长！              │
│ 这就是"优先级反转"，可能导致系统响应最坏的情况       │
└────────────────────────────────────────────────────┘
```

**解决方案：优先级继承**

```c
// 当任务 H 试图获取由 L 持有的互斥量时：
void mutex_take(mutex) {
    if (mutex->holder != NULL && mutex->holder->priority < current_priority) {
        // L 的优先级提升到 H 的级别
        mutex->holder->priority = current_priority;

        // L 被移动到更高优先级的就绪队列
        move_to_ready_list(mutex->holder);
    }

    // 继续等待互斥量...
}
```

### Q3：队列和信号量的区别？【详细解答】

**参考答案：**

```
【队列（Queue）】
┌────────────────────────────────────────────────────┐
│                                                    │
│  ┌────┬────┬────┬────┬────┐                        │
│  │msg1│msg2│msg3│    │    │  长度=5                 │
│  └────┴────┴────┴────┴────┘                        │
│   ↑                        ↑                        │
│  pcReadFrom              pcWriteTo                 │
│                                                    │
│ 特性：                                            │
│ - 传输实际数据（按元素大小复制）                    │
│ - FIFO 或 LIFO 顺序                               │
│ - 发送和接收都支持阻塞                             │
│ - 多生产者多消费者天然支持                          │
└────────────────────────────────────────────────────┘

【信号量（Semaphore）】
┌────────────────────────────────────────────────────┐
│                                                    │
│  计数器 = 3                                        │
│                                                    │
│  Give: 计数器++                                   │
│  Take: 计数器--（为0时阻塞）                        │
│                                                    │
│  特性：                                            │
│ - 不传输数据（元素大小=0）                          │
│ - 只表示"有"或"无"                                 │
│ - 只有 Take 操作阻塞                               │
│ - 二值信号量用于同步，计数信号量用于资源池           │
└────────────────────────────────────────────────────┘
```

### Q4：FreeRTOS 如何实现任务间通信？【详细解答】

**参考答案：**

FreeRTOS 提供五种任务间通信机制，按复杂度递增排列：

```
1. 任务通知（Task Notifications）
   - 最轻量，嵌入在 TCB 中
   - 速度最快
   - 限制：一对一通信

2. 队列（Queues）
   - 环形缓冲区实现
   - 支持数据传递
   - 可多消费者/多生产者

3. 二值信号量（Binary Semaphores）
   - 长度为1的队列
   - 用于同步

4. 计数信号量（Counting Semaphores）
   - 长度为N的队列
   - 用于资源计数

5. 互斥量（Mutexes）
   - 带优先级继承的二进制信号量
   - 用于资源保护
```

### Q5：什么是死锁？如何避免？【详细解答】

**参考答案：**

死锁的四个必要条件：
1. 互斥：资源只能被一个任务持有
2. 持有并等待：持有资源时等待其他资源
3. 不可抢占：资源不能被强制夺走
4. 循环等待：形成循环等待链

**避免策略：**

```c
// 策略1：固定获取顺序
// 始终按照相同顺序获取所有互斥量
void worker_task(void) {
    // 错误：不同顺序
    // take(M1); take(M2); ...
    // 正确：固定顺序
    if (priority(M1) < priority(M2)) {
        take(M1); take(M2);
    } else {
        take(M2); take(M1);
    }
}

// 策略2：超时机制
BaseType_t try_lock_all(void) {
    if (xSemaphoreTake(M1, pdMS_TO_TICKS(100)) != pdPASS) {
        return pdFAIL;
    }
    if (xSemaphoreTake(M2, pdMS_TO_TICKS(100)) != pdPASS) {
        xSemaphoreGive(M1);  // 回滚
        return pdFAIL;
    }
    return pdPASS;
}

// 策略3：限制持有时间
void bounded_critical_section(void) {
    uint32_t start = xTaskGetTickCount();

    take(M1);
    // ... 操作 ...
    configASSERT((xTaskGetTickCount() - start) < MAX_HOLD_TIME);
    give(M1);
}
```

### Q6：队列的阻塞机制是如何工作的？【详细解答】

**参考答案：**

```c
// 阻塞机制的核心实现（伪代码）
BaseType_t queue_send_with_blocking(QueueHandle_t xQueue,
                                     void *pvItem,
                                     TickType_t xTicksToWait) {
    TimeOut_t timeout;
    BaseType_t timeout_set = pdFALSE;

    while (1) {
        // 进入临界区
        taskENTER_CRITICAL();

        if (queue_has_space(xQueue)) {
            // 有空间，复制数据
            copy_to_queue(xQueue, pvItem);
            taskEXIT_CRITICAL();
            return pdPASS;
        }

        // 队列满
        if (xTicksToWait == 0) {
            // 不等待，立即返回
            taskEXIT_CRITICAL();
            return errQUEUE_FULL;
        }

        // 初始化超时（只在第一次）
        if (!timeout_set) {
            vTaskSetTimeOutState(&timeout);
            timeout_set = pdTRUE;
        }

        if (is_timeout(&timeout, xTicksToWait)) {
            // 已超时，返回错误
            taskEXIT_CRITICAL();
            return errQUEUE_FULL;
        }

        // 未超时，阻塞等待
        taskEXIT_CRITICAL();

        // 将任务添加到等待发送列表
        vTaskPlaceOnUnorderedEventList(
            &xQueue->xTasksWaitingToSend,
            xTicksToWait);

        // 触发上下文切换
        portYIELD();

        // 唤醒后继续循环检查
    }
}
```

### Q7：互斥量和递归互斥量的区别？【详细解答】

**参考答案：**

| 特性 | 互斥量 | 递归互斥量 |
|------|--------|-----------|
| 嵌套获取 | 不支持（会死锁） | 支持 |
| 持有计数 | 无 | 有（uxMutexHeldCount） |
| 释放次数 | 1次 | 必须与获取次数相同 |
| 内存占用 | 较小 | 较大（需存储递归深度） |
| 用途 | 简单的互斥访问 | 递归函数中的互斥 |

```c
// 递归互斥量的实现
BaseType_t xSemaphoreTakeRecursive(xMutex, xBlockTime) {
    // 检查是否已经是持有者
    if (xMutex->pxMutexHolder == xTaskGetCurrentTaskHandle()) {
        // 已经是持有者，增加递归计数
        xMutex->uxMutexHeldCount++;
        return pdPASS;
    } else {
        // 不是持有者，首次获取
        BaseType_t result = xQueueGenericReceive(xMutex, NULL, xBlockTime);
        if (result == pdPASS) {
            xMutex->pxMutexHolder = xTaskGetCurrentTaskHandle();
            xMutex->uxMutexHeldCount = 1;
        }
        return result;
    }
}

BaseType_t xSemaphoreGiveRecursive(xMutex) {
    // 只能由持有者释放
    if (xMutex->pxMutexHolder != xTaskGetCurrentTaskHandle()) {
        return errQUEUE_FULL;
    }

    // 减少递归计数
    xMutex->uxMutexHeldCount--;

    if (xMutex->uxMutexHeldCount == 0) {
        // 完全释放，唤醒下一个等待者
        xMutex->pxMutexHolder = NULL;
        return xQueueGenericSend(xMutex, NULL, queueSEND_TO_BACK);
    }

    return pdPASS;
}
```

### Q8：中断安全函数的后缀 FromISR 有什么意义？【详细解答】

**参考答案：**

```c
// 普通版本（在任务中调用）
BaseType_t xQueueSend(QueueHandle_t xQueue,
                       const void *pvItemToQueue,
                       TickType_t xTicksToWait);

// ISR版本（中断处理函数中调用）
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                               const void *pvItemToQueue,
                               BaseType_t *pxHigherPriorityTaskWoken);

// 关键区别：
// 1. ISR 版本不能阻塞
//    - 普通版本有 xTicksToWait 参数，可以阻塞
//    - ISR 版本没有此参数，立即返回

// 2. ISR 版本需要指示是否需要调度
//    - 通过 pxHigherPriorityTaskWoken 传出参数
//    - 如果为 pdTRUE，退出中断后需要执行更高优先级任务

// 3. 实现差异：
//    - ISR 版本使用 taskENTER_CRITICAL_FROM_ISR()
//    - 不调用 vTaskPlaceOnUnorderedEventList()
//    - 直接操作就绪列表而不触发阻塞
```

---

## 6.16 高级应用：队列集（Queue Sets）

### 6.16.1 队列集简介

队列集允许任务同时等待多个队列或信号量的事件。

```
【队列集工作原理】

┌─────────────────────────────────────────────────────┐
│                    队列集                           │
│  xQueueSet = xQueueCreateSet(10)                    │
│                                                     │
│   ┌──────┐   ┌──────┐   ┌──────┐                    │
│   │Queue1│   │Queue2│   │Semiph│                    │
│   └──┬───┘   └──┬───┘   └──┬───┘                    │
│      │         │         │                         │
│      └────────┬┴─────────┘                         │
│               │                                     │
│               ▼                                     │
│        xQueueSetMember                              │
│                                                     │
│  Task: 等待 xQueueSet，有任意一个队列有数据时唤醒    │
└─────────────────────────────────────────────────────┘
```

### 6.16.2 队列集使用示例

```c
// 创建队列集
QueueSetHandle_t xQueueSet = xQueueCreateSet(10);

// 创建队列和信号量
QueueHandle_t xQueue1 = xQueueCreate(5, sizeof(char));
SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();

// 将队列和信号量添加到队列集
xQueueAddToSet(xQueue1, xQueueSet);
xQueueAddToSet(xSemaphore, xQueueSet);

// 等待任务
void vWaitTask(void *pv) {
    QueueSetMemberHandle_t xActivatedMember;

    for (;;) {
        // 等待任意一个集合成员变为可用
        xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);

        if (xActivatedMember != NULL) {
            if (xActivatedMember == xQueue1) {
                // Queue1 有数据
                char c;
                xQueueReceive(xQueue1, &c, 0);
                // 处理数据
            } else if (xActivatedMember == (QueueSetMemberHandle_t)xSemaphore) {
                // Semaphore 可用
                xSemaphoreTake(xSemaphore, 0);
                // 处理信号量事件
            }
        }
    }
}
```

---

## 6.17 性能优化建议

### 6.17.1 队列大小选择

```c
// 原则：队列应足够大以容纳峰值数据量

// 错误：小队列导致频繁阻塞
#define BAD_QUEUE_SIZE  2
QueueHandle_t xQueueBad = xQueueCreate(BAD_QUEUE_SIZE, sizeof(uint32_t));
// 风险：生产者速度 > 消费者速度 时频繁丢消息

// 正确：合理的队列大小
#define GOOD_QUEUE_SIZE  20
QueueHandle_t xQueueGood = xQueueCreate(GOOD_QUEUE_SIZE, sizeof(uint32_t));
// 考虑因素：
// - 生产者峰值速度
// - 消费者峰值速度
// - 可接受的阻塞频率
```

### 6.17.2 避免优先级反转

```c
// 问题代码：可能发生优先级反转
void bad_task(void *pv) {
    take(MutexA);
    take(MutexB);  // 如果高优先级任务需要 MutexB，这里会阻塞它
    // 操作资源
    give(MutexB);
    give(MutexA);
}

// 优化：将高优先级操作放在持有互斥量之前
void good_task(void *pv) {
    // 先执行不需要互斥量的高优先级操作
    perform_time_critical_operation();

    take(MutexA);
    take(MutexB);
    // 操作资源
    give(MutexB);
    give(MutexA);
}
```

### 6.17.3 中断设计原则

```c
// 好的设计：中断处理尽可能简短
void UART_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (UART_RX_DATA_AVAILABLE) {
        // 只在中断中保存数据，不做复杂处理
        uint8_t data = UART_READ_DATA();
        xQueueSendFromISR(xDataQueue, &data, &xHigherPriorityTaskWoken);
    }

    // 复杂的解析和处理放在任务中
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// 坏的设计：在中断中做太多事情
void bad_UART_IRQHandler(void) {
    // 复杂的数据处理不应该在中断中
    parse_protocol();      // 太耗时
    update_state_machine(); // 太耗时
    // 这些应该在任务中进行
}
```

---

## 6.18 调试技巧

### 6.18.1 使用 FreeRTOS+TRACE 或可视化工具

```c
// 启用跟踪需要配置 configUSE_TRACE_FACILITY
// 设置为 1 启用基本跟踪
// 设置为 2 启用可视化跟踪

// 可视化工具可以显示：
// - 任务状态转换（运行、就绪、阻塞、挂起）
// - 队列操作
// - 上下文切换时间点
// - CPU 使用率分析
```

### 6.18.2 常见问题诊断

```
【问题1：任务永久阻塞】
症状：某个任务不再执行

可能原因：
1. 互斥量死锁
   检查：是否所有互斥量获取都有对应的释放？

2. 队列满/空阻塞
   检查：是否有对应的发送/接收操作？

3. 事件组位未设置
   检查：设置事件位的任务是否执行？

诊断方法：
- 使用 vTaskList() 打印所有任务状态
- 检查 xTasksWaitingToSend/Receive 列表

【问题2：优先级反转】
症状：高优先级任务响应时间不稳定

诊断：
- 检查是否有任务持有互斥量时被中优先级任务抢占
- 使用互斥量替代二值信号量
```

### 6.18.3 常用调试 API

```c
// 获取任务列表信息（需要 configUSE_TRACE_FACILITY = 1）
void vTaskList(char *pcWriteBuffer);

// 获取运行时间统计
void vTaskGetRunTimeStats(char *pcWriteBuffer);

// 获取任务状态（实验性 API）
TaskStatus_t *pxTaskStatusArray;
UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);

// 诊断互斥量持有者
TaskHandle_t xMutexHolder = xSemaphoreGetMutexHolder(xMutex);
if (xMutexHolder != NULL) {
    // 有任务持有该互斥量
}
```

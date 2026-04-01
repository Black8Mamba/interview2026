# 同步与通信 (Synchronization & Communication)

## 信号量 ⭐⭐⭐

信号量是一种用于任务同步和资源管理的机制。FreeRTOS提供三种类型的信号量：二值信号量、计数信号量和互斥锁。

### 二值信号量

二值信号量只有两个状态：可用和不可用。它主要用于：

- **任务同步**：一个任务等待另一个任务完成某项操作
- **中断与任务同步**：中断服务程序(ISR)释放信号量，任务等待并处理数据

```c
// 创建二值信号量
SemaphoreHandle_t xBinarySemaphore;
xBinarySemaphore = xSemaphoreCreateBinary();

// 在ISR中释放信号量
xSemaphoreGiveFromISR(xBinarySemaphore, NULL);

// 在任务中获取信号量
if (xSemaphoreTake(xBinarySemaphore, portMAX_DELAY) == pdTRUE) {
    // 处理数据
}
```

**典型使用场景**：UART接收中断与数据处理任务的同步。

### 计数信号量

计数信号量可以记录多个事件或资源数量，适用于：

- **资源计数**：管理多个相同类型的资源（如连接池）
- **事件计数**：累计多个事件的发生次数

```c
// 创建计数信号量，最大计数值10
SemaphoreHandle_t xCountingSemaphore;
xCountingSemaphore = xSemaphoreCreateCounting(10, 0);

// 释放信号量（增加计数）
xSemaphoreGive(xCountingSemaphore);

// 获取信号量（减少计数）
xSemaphoreTake(xCountingSemaphore, portMAX_DELAY);
```

**典型使用场景**：生产者-消费者模型中跟踪可用缓冲区数量。

### 互斥锁

互斥锁(Mutex)是一种特殊的二进制信号量，用于保护共享资源的访问。

- **优先级继承**：当高优先级任务等待低优先级任务持有的互斥锁时，低优先级任务的优先级会临时提升，避免优先级反转问题
- **递归问题**：普通互斥锁不能递归获取，同一任务多次获取会导致死锁

```c
// 创建互斥锁
SemaphoreHandle_t xMutex;
xMutex = xSemaphoreCreateMutex();

// 获取互斥锁
if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    // 访问共享资源
    xSemaphoreGive(xMutex);  // 必须释放
}
```

**注意**：不要在ISR中使用互斥锁。

### 递归互斥锁

递归互斥锁允许同一任务多次获取锁而不发生死锁。

```c
// 创建递归互斥锁
SemaphoreHandle_t xRecursiveMutex;
xRecursiveMutex = xSemaphoreCreateRecursiveMutex();

// 同一任务可以多次获取
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);

// 必须释放相同次数
xSemaphoreGiveRecursive(xRecursiveMutex);
xSemaphoreGiveRecursive(xRecursiveMutex);
```

## 队列 Queue ⭐⭐⭐

队列是FreeRTOS中最常用的任务间通信机制，用于在任务之间传递数据。

### 特性

- **任务间通信**：支持从一个任务向另一个任务发送数据
- **阻塞读取/写入**：发送和接收操作可以设置超时时间
- **消息传递机制**：数据通过复制方式传递，发送方和接收方不共享内存

```c
// 创建队列：10个消息，每个消息大小为uint32_t
QueueHandle_t xQueue;
xQueue = xQueueCreate(10, sizeof(uint32_t));

// 发送数据
uint32_t sendValue = 100;
xQueueSend(xQueue, &sendValue, portMAX_DELAY);

// 接收数据
uint32_t recvValue;
xQueueReceive(xQueue, &recvValue, portMAX_DELAY);
```

### 配置

- **队列长度**：队列可以存储的最大消息数量
- **消息大小**：每个消息的字节大小
- **数据拷贝方式**：发送时数据被复制到队列中，接收时从队列复制出来

**阻塞行为**：
- `xQueueSend()` - 当队列满时，任务可以阻塞等待
- `xQueueReceive()` - 当队列空时，任务可以阻塞等待

**注意**：队列是FIFO（先进先出）结构。

## 事件组 Event Groups ⭐⭐

事件组允许任务等待多个事件的任意组合。

### 特性

- **多位标志**：每个事件组最多包含8个位标志
- **等待任意位/所有位**：可以设置等待任意一位或所有位
- **广播通知**：当事件发生时，所有等待该事件的任务都会被唤醒

```c
// 创建事件组
EventGroupHandle_t xEventGroup;
xEventGroup = xEventGroupCreate();

// 设置位标志
xEventGroupSetBits(xEventGroup, BIT_0 | BIT_1);

// 等待任意位
EventBits_t xBits;
xBits = xEventGroupWaitBits(xEventGroup,
                             BIT_0,
                             pdTRUE,  // 清清除已设置的位
                             pdFALSE, // pdTRUE=所有位, pdFALSE=任意位
                             portMAX_DELAY);

// 等待所有位
xBits = xEventGroupWaitBits(xEventGroup,
                             BIT_0 | BIT_1,
                             pdTRUE,
                             pdTRUE,
                             portMAX_DELAY);
```

**典型使用场景**：等待多个外设完成初始化后启动主任务。

## 任务通知 Task Notifications ⭐⭐⭐

任务通知是FreeRTOS v8.2.0引入的高效通信机制。

### 优势

- **更高效**：不需要创建信号量或队列对象
- **无需创建对象**：每个任务自带一个通知值
- **直接发送到任务**：通过任务句柄直接通知目标任务

### 用法

任务通知可以替代二进制信号量和队列的部分功能。

```c
// 替代二进制信号量
// 发送通知
xTaskNotifyGive(xTaskHandle);

// 接收通知
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

// 替代队列（发送数据）
uint32_t notifyValue = 100;
xTaskNotify(xTaskHandle, notifyValue, eSetValueWithOverwrite);

// 替代队列（接收数据）
uint32_t notifyValue;
xTaskNotifyWait(0, ULONG_MAX, &notifyValue, portMAX_DELAY);
```

**注意**：
- 每个任务只有一个通知值
- 使用 `eSetValueWithOverwrite` 会覆盖之前的值
- 使用 `eSetValueWithoutOverwrite` 如果通知未处理则失败

**典型使用场景**：轻量级信号量替代、中断与任务的简单同步。
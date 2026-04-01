# 内存管理 (Memory Management)

## 堆管理方式

FreeRTOS 提供了多种堆管理方案，难度从低到高排列为：`heap_1` < `heap_2` < `heap_3` < `heap_4` < `heap_5`。其中最常用的是 `heap_1`、`heap_2` 和 `heap_4`。

### heap_1 - 简单分配

`heap_1` 是最简单的堆管理方案，只实现内存分配功能，不支持内存释放。

```c
// 配置方式：在 FreeRTOSConfig.h 中添加
#define configUSE_HEAP_1 1
```

**特点：**
- 仅支持 `pvPortMalloc()` 分配内存
- 不支持 `vPortFree()` 释放内存
- 内存碎片风险随运行时间增加
- 代码量最小，执行速度最快

**实现原理：**
- 使用简单的静态数组作为堆空间
- 每次分配从堆顶部开始，顺序分配
- 维护一个 `xNextFreeByte` 指针记录当前可用位置

**适用场景：**
- 创建后不再删除的任务和队列
- 静态对象的分配
- 对内存碎片不敏感的系统
- Bootloader 或初始化阶段

**代码示例：**
```c
// 创建静态任务（使用 heap_1）
StaticTask_t xTaskBuffer;
StackType_t xStack[ configMINIMAL_STACK_SIZE ];

xTaskCreateStatic(
    NULL,                      // 任务函数
    "Task",                    // 任务名称
    configMINIMAL_STACK_SIZE,  // 栈大小
    NULL,                      // 参数
    1,                         // 优先级
    xStack,                    // 栈数组
    &xTaskBuffer               // 任务控制块
);
```

### heap_2 - 最佳匹配

`heap_2` 在 `heap_1` 基础上增加了内存释放功能，使用最佳匹配算法（Best Fit）。

```c
#define configUSE_HEAP_2 1
```

**特点：**
- 支持 `pvPortMalloc()` 和 `vPortFree()`
- 使用最佳匹配算法：找到最小的可用空闲块
- 支持相邻空闲块简单合并
- 适合固定大小内存块分配

**实现原理：**
- 空闲块按大小排序（隐式空闲链表）
- 分配时遍历找到最小匹配块
- 释放时与相邻空闲块合并

**优点：**
- 可以回收已释放的内存
- 分配效率较高

**缺点：**
- 仍可能产生内存碎片
- 不适合频繁分配释放不同大小内存的场景

**适用场景：**
- 固定大小的消息队列或缓冲区
- 定期清理的系统
- 内存使用模式相对固定的场景

### heap_3 - 包装器

`heap_3` 不是一个真正的堆管理器，它是标准 `malloc()/free()` 的包装器。

```c
#define configUSE_HEAP_3 1
```

**特点：**
- 直接调用标准 C 库的 `malloc()` 和 `free()`
- 非确定性行为（执行时间不可预测）
- 需要链接器配置堆大小
- 线程安全（依赖编译器库实现）

**注意：**
- 使用 `heap_3` 时，`configTOTAL_HEAP_SIZE` 不起作用
- 堆大小需要在链接脚本或启动文件中配置

### heap_4 - 首次适应（推荐）

`heap_4` 使用首次适应算法（First Fit），是 FreeRTOS 最推荐的堆管理方案。

```c
#define configUSE_HEAP_4 1
```

**特点：**
- 支持分配和释放
- 首次适应算法：找到第一个足够大的空闲块
- 智能合并：合并相邻的空闲块（边界合并）
- 更好的碎片处理能力

**实现原理：**
- 空闲块通过显式链表管理
- 分配时找到第一个足够大的块
- 如果分配的块比请求的大，可能分割该块
- 释放时检查相邻块，合并成更大的空闲块

**优势：**
- 碎片化程度最低
- 适合长期运行的系统（汽车电子）
- 支持内存池对齐（8字节对齐）

**适用场景：**
- 长期运行不重启的系统
- 内存分配释放频繁的场景
- 汽车电子等对稳定性要求高的应用

**内存对齐配置：**
```c
#define configBYTE_ALIGNMENT 8        // 8字节对齐
#define configBYTE_ALIGNMENT_MASK 7   // 对齐掩码
```

### heap_5 - 跨内存区

`heap_5` 在 `heap_4` 基础上增加了多堆支持，可以管理多个非连续的内存区域。

```c
#define configUSE_HEAP_5 1
```

**特点：**
- 支持多个独立的内存堆
- 可以使用内部 RAM 和外部 RAM
- 需要手动初始化堆区域

**应用场景：**
- 需要同时使用内部 SRAM 和外部 SDRAM
- 多核系统不同核使用不同内存区域
- 分段内存管理

**使用示例：**
```c
// 定义外部 RAM 堆区域
static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];

 HeapRegion_t xHeapRegions[] = {
    { ucHeap, sizeof(ucHeap) },    // 内部 RAM
    { NULL, 0 }                    // 结束标记
};

// 在 FreeRTOS 初始化前调用
vPortDefineHeapRegions( xHeapRegions );
```

## 内存池 (Memory Pool)

FreeRTOS 提供了静态内存池机制，适用于需要确定性内存分配的场景。

### 静态内存池创建

```c
// 内存池控制块
StaticEventGroup_t xEventGroupBuffer;

// 创建静态事件组（基于内存池）
EventGroupHandle_t xEventGroupCreateStatic( &xEventGroupBuffer );
```

### xStreamBuffer 内存池

`StreamBuffer` 是 FreeRTOS 提供的流缓冲区，可以使用静态内存。

```c
#include "stream_buffer.h"

// 配置：使用静态内存的 StreamBuffer
#define sbCREATED_STATICALLY 1

// 创建静态 StreamBuffer
StaticStreamBuffer_t xStreamBufferStruct;
uint8_t ucStreamBufferStorage[ 1024 ];

StreamBufferHandle_t xStreamBufferCreateStatic(
    1024,                      // 缓冲区大小
    1,                         // 触发阈值（大于此值才通知）
    pdTRUE,                    // 允许发送
    ucStreamBufferStorage,     // 存储区
    &xStreamBufferStruct       // 控制块
);
```

### 使用场景

**适合使用静态内存池的情况：**
- 频繁的分配和释放操作
- 对内存碎片敏感的系统
- 需要确定性分配时间的场景
- 实时性要求高的应用

**不适合的情况：**
- 内存使用量无法预估
- 对象数量动态变化
- 内存资源充足且分配不频繁

## 汽车电子重点

在汽车电子应用中，内存管理需要特别关注长期运行稳定性。

### 长期运行稳定性考量

1. **内存碎片处理策略**
   - 优先选择 `heap_4`（首次适应 + 边界合并）
   - 避免频繁分配不同大小的内存块
   - 使用静态分配代替动态分配

2. **内存泄漏检测**
   - 监控堆使用情况：`xPortGetFreeHeapSize()`
   - 记录分配/释放调用次数
   - 定期输出堆状态日志

3. **内存边界检查**
   - 启用栈溢出检测：`configCHECK_FOR_STACK_OVERFLOW`
   - 监控内存使用峰值

### 监控 API

```c
// 获取当前剩余堆空间
size_t xPortGetFreeHeapSize( void );

// 获取最小剩余堆空间（历史记录）
size_t xPortGetMinimumEverFreeHeapSize( void );

// 堆初始化完成检查
void vPortInitialiseBlocks( void );
```

### 配置建议

```c
// FreeRTOSConfig.h 建议配置

// 使用 heap_4（推荐）
#define configUSE_HEAP_4 1

// 堆大小（根据芯片内存和实际需求调整）
#define configTOTAL_HEAP_SIZE ( 32 * 1024 )  // 32KB

// 8字节对齐
#define configBYTE_ALIGNMENT 8

// 栈溢出检测（强烈建议开启）
#define configCHECK_FOR_STACK_OVERFLOW 2

// 内存分配失败钩子函数
#define configUSE_MALLOC_FAILED_HOOK 1
void vApplicationMallocFailedHook( void );
```

### 内存优化技巧

1. **合理选择堆管理方案**
   - 静态分配优先：`xTaskCreateStatic()`
   - 动态分配次之：`pvPortMalloc()`
   - 避免 `heap_3`（标准库包装）

2. **减少动态分配频率**
   - 任务创建后不删除
   - 消息队列预分配
   - 使用静态对象池

3. **内存对齐**
   - 使用 2 的幂次大小
   - 避免 1 字节对齐导致的碎片

4. **定期清理策略**
   - 在系统空闲时合并碎片
   - 重启前记录内存使用趋势
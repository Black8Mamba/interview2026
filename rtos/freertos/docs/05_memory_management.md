# 第五章：内存管理

> 本章目标：掌握 FreeRTOS 五种内存分配策略，理解堆内存管理和常见内存问题

## 章节结构

- [ ] 5.1 内存管理概述
- [ ] 5.2 五种堆分配策略
- [ ] 5.3 内存分配 API
- [ ] 5.4 栈内存管理
- [ ] 5.5 常见内存问题
- [ ] 5.6 面试高频问题
- [ ] 5.7 避坑指南

---

## 5.1 内存管理概述

### FreeRTOS 内存布局

```
┌─────────────────────────────────────────────────────────┐
│                    Flash (代码存储)                      │
├─────────────────────────────────────────────────────────┤
│                    SRAM (运行时内存)                     │
│  ┌─────────────────────────────────────────────────────┐│
│  │  .data    全局/静态变量（已初始化）                 ││
│  ├─────────────────────────────────────────────────────┤│
│  │  .bss     全局/静态变量（未初始化）                 ││
│  ├─────────────────────────────────────────────────────┤│
│  │  Heap    FreeRTOS 堆内存                           ││
│  │  (configTOTAL_HEAP_SIZE)                           ││
│  │  ┌──────┬──────┬──────┬──────┬──────┐             ││
│  │  │ Task │ Task │ Queue│ Timer│ etc  │             ││
│  │  │ TCB  │ Stack│      │      │      │             ││
│  │  └──────┴──────┴──────┴──────┴──────┘             ││
│  ├─────────────────────────────────────────────────────┤│
│  │  System Stack  (中断和调度器使用)                   ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
```

### 内存相关配置

```c
// FreeRTOSConfig.h
#define configTOTAL_HEAP_SIZE    ( 20 * 1024 )          // 堆大小 20KB
#define configSUPPORT_DYNAMIC_ALLOCATION    1           // 动态内存
#define configSUPPORT_STATIC_ALLOCATION     0           // 静态内存
```

---

## 5.2 五种堆分配策略

### 堆文件位置

```
FreeRTOS/Source/portable/MemMang/
├── heap_1.c    ← 最简单，不允许释放
├── heap_2.c    ← 简单，允许释放但不支持合并
├── heap_3.c    ← 调用标准 malloc/free
├── heap_4.c    ← 最佳适应 + 合并，支持释放（最常用）
└── heap_5.c    ← heap_4 + 支持非连续内存区
```

### heap_1.c — 只分配不释放

**特点：**
- 最简单，分配快
- 不支持释放，内存只增不减
- 适用于静态创建任务后不再删除的场景

```c
// 分配算法：简单线性分配
// 每次分配从堆起始地址开始，下一个分配紧跟在上一个之后

void *pvPortMalloc(size_t xWantedSize) {
    BlockLink_t *pxBlock;
    size_t xWantedSize2 = xWantedSize + prvHeadBlockSize;

    // 检查剩余空间
    if (xWantedSize2 <= xFreeBytesRemaining) {
        // 分配 xWantedSize 字节
        // 返回指针
    }
    return NULL;  // 空间不足
}
```

**适用场景：** 任务创建后永不删除

---

### heap_2.c — 允许释放，不合并

**特点：**
- 使用最优适配算法
- 释放后内存可重用，但不与相邻空闲块合并
- 存在内存碎片化问题

```c
// 空闲块链表按大小排序
// 分配时找到最小且满足大小的块

// 释放时简单标记为空闲，不合并相邻块
void vPortFree(void *pv) {
    // 从空闲链表找到对应块
    // 标记为已使用
    // 插入空闲链表（按大小排序插入）
}
```

**适用场景：** 频繁分配和释放，但碎片化可接受的场景

**问题：** 长期运行可能内存碎片化

---

### heap_3.c — 标准 C 库

**特点：**
- 简单包装标准 `malloc()` 和 `free()`
- 线程安全（如果底层实现是线程安全的）
- 需要链接器配置堆区域

```c
void *pvPortMalloc(size_t xWantedSize) {
    return malloc(xWantedSize);
}

void vPortFree(void *pv) {
    free(pv);
}
```

**注意：**
- 不受 `configTOTAL_HEAP_SIZE` 限制
- 内存来自系统堆
- 需配置 linker script 预留堆空间

---

### heap_4.c — 最佳适应 + 合并（最常用）

**特点：**
- 最佳适应算法（找到最小满足大小的空闲块）
- 释放时合并相邻空闲块
- 有效减少碎片化

```c
// 空闲块结构
typedef struct A_BLOCK_LINK {
    struct A_BLOCK_LINK *pxNextFreeBlock;  // 下一个空闲块
    size_t xBlockSize;                     // 块大小（含头部）
} BlockLink_t;
```

**合并相邻空闲块：**

```
释放前：
┌────────┬────────┐
│ Used   │  Free  │  ← 释放左侧 Used 块
└────────┴────────┘

合并后：
┌─────────────────┐
│      Free       │  ← 自动合并为一个大块
└─────────────────┘
```

**适用场景：** 大多数嵌入式应用，**推荐使用**

---

## 5.2.1 heap_4.c 深度源码分析

### BlockLink_t 核心结构详解

heap_4.c 是 FreeRTOS 最常用的堆实现，其核心是 **BlockLink_t** 链表结构。每个allocated或free的内存块都以此结构开头：

```c
/* 文件位置: FreeRTOS/Source/portable/MemMang/heap_4.c */

/* 块头部结构 - 每个内存块（包括已分配和空闲）开头都有这个结构 */
typedef struct A_BLOCK_LINK {
    struct A_BLOCK_LINK *pxNextFreeBlock;  /* 指向下一个空闲块的指针 */
    size_t xBlockSize;                      /* 当前块大小（包含头部自身） */
} BlockLink_t;

/* 重要：xBlockSize 包含 BlockLink_t 头部的大小！
 * 这意味着用户实际可用的内存 = xBlockSize - xHeapStructSize
 */
```

### 堆配置常量

```c
/* 堆总大小配置 - 20KB */
#define configTOTAL_HEAP_SIZE    ( ( size_t ) ( 20 * 1024 ) )

/* 字节对齐要求 - 8字节对齐 */
#define heapBYTE_ALIGNMENT        8
#define heapBYTE_ALIGNMENT_MASK   ( heapBYTE_ALIGNMENT - 1 )  /* 0x07 */

/* 每次分配的大小上限检查 */
#define armccRELEASE_MODE  /* 某些编译器需要 */

/* 内部使用的头部大小常数 */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + ( ( size_t ) ( heapBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) heapBYTE_ALIGNMENT_MASK );
/* 计算结果：sizeof(BlockLink_t) = 8~16字节，对齐后 xHeapStructSize = 16字节（32位系统）或 24字节（64位系统） */
```

### 静态全局变量

```c
/* 空闲链表起始节点 - 始终在链表中（作为哨兵节点） */
static BlockLink_t xStart;

/* 空闲链表结束节点 - 指向堆末尾的哨兵节点 */
static BlockLink_t *pxEnd = NULL;

/* 当前剩余空闲字节数 */
static size_t xFreeBytesRemaining = configTOTAL_HEAP_SIZE;

/* 历史最低空闲字节数 - 用于监测内存压力 */
static size_t xMinimumEverFreeBytesRemaining = configTOTAL_HEAP_SIZE;

/* 互斥锁 - 用于线程安全（heap_3使用） */
#if( configSUPPORT_STATIC_ALLOCATION == 0 )
    static void *pvMallocMutexHandle = NULL;
#endif
```

### 内存块布局图

```
┌─────────────────────────────────────────────────────────────────────┐
│                         堆内存区域                                    │
│                     (configTOTAL_HEAP_SIZE)                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐        │
│  │   BlockLink_t  │  │   BlockLink_t  │  │   BlockLink_t  │        │
│  │  xBlockSize    │  │  xBlockSize    │  │  xBlockSize    │        │
│  │  pxNextFree    │  │  pxNextFree     │  │  pxNextFree    │        │
│  ├────────────────┤  ├────────────────┤  ├────────────────┤        │
│  │                │  │                │  │                │        │
│  │   用户数据     │  │   用户数据     │  │   用户数据     │        │
│  │  (Block 1)     │  │  (Block 2)     │  │  (Block 3)     │        │
│  │  已分配        │  │   空闲         │  │  已分配        │        │
│  │                │  │                │  │                │        │
│  └────────────────┘  └────────────────┘  └────────────────┘        │
│                                                                     │
│  BlockLink_t 大小 = xHeapStructSize = 16字节（32位）/ 24字节（64位） │
│                                                                     │
├─────────────────────────────────────────────────────────────────────┤
│  pxEnd 哨兵节点（虚拟，不占实际内存）                                  │
│  pxEnd->xBlockSize = 0                                             │
│  pxEnd->pxNextFreeBlock = NULL                                     │
└─────────────────────────────────────────────────────────────────────┘

空闲链表结构：
xStart ──→ Block 2 ──→ pxEnd（哨兵）
                  ↑
            pxNextFreeBlock

已分配块不在空闲链表里！
```

---

## 5.2.2 pvPortMalloc 最佳适应算法详解

### 函数签名与入口处理

```c
void *pvPortMalloc(size_t xWantedSize) {
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
    void *pvReturn = NULL;

    /* 步骤1：互斥锁获取（线程安全） */
    vTaskSuspendAll();  /* 挂起所有任务 */
    { /* 临界区开始 */

        /* 步骤2：大小对齐 + 添加头部开销 */
        /* 重要：用户请求的 xWantedSize 不包含头部！
         * 我们需要额外 xHeapStructSize 字节来存储 BlockLink_t
         */
        xWantedSize += xHeapStructSize;

        /* 8字节对齐处理（ARM架构要求） */
        if ((xWantedSize & heapBYTE_ALIGNMENT_MASK) != 0) {
            /* 如果不是8的倍数，向上取整到8的倍数 */
            xWantedSize += (heapBYTE_ALIGNMENT - 1);
            xWantedSize &= ~heapBYTE_ALIGNMENT_MASK;
            /* 示例：xWantedSize=17 → 17+7=24 → 24&(~7)=24 */
            /* 示例：xWantedSize=100 → 100+7=107 → 107&(~7)=104 */
        }

        /* 步骤3：检查内存是否足够 */
        if ((xWantedSize > 0) && (xWantedSize <= xFreeBytesRemaining)) {
            /* 有足够的空闲内存，继续分配 */
        } else {
            /* 内存不足，返回 NULL */
            pvReturn = NULL;
        }
    } /* 临界区结束 */
    xTaskResumeAll();  /* 恢复所有任务 */

    return pvReturn;
}
```

### 最佳适应算法核心

```c
/* 继续 pvPortMalloc 函数体 */

vTaskSuspendAll();
{

    /* 步骤4：最佳适应搜索 - O(n) 时间复杂度
     *
     * 最佳适应策略：遍历整个空闲链表，找到最小且 >= xWantedSize 的块
     * 优点：尽量保留大块内存，减少碎片化
     * 缺点：每次分配需要遍历整个链表
     *
     * 注意：heap_4 的空闲链表按地址排序（不是按大小排序！）
     * 这是为了支持 O(1) 的相邻块合并
     */

    pxPreviousBlock = &xStart;  /* 记录前一个节点 */
    pxBlock = xStart.pxNextFreeBlock;  /* 从第一个空闲块开始 */

    /* 遍历空闲链表，寻找最佳匹配 */
    while ((pxBlock->xBlockSize < xWantedSize) &&    /* 块太小 */
           (pxBlock->pxNextFreeBlock != NULL)) {      /* 还没到链表末尾 */
        pxPreviousBlock = pxBlock;         /* 保存前驱 */
        pxBlock = pxBlock->pxNextFreeBlock; /* 移动到下一个 */
    }

    /* 循环退出条件：
     * 1. pxBlock->xBlockSize >= xWantedSize（找到合适的块）
     * 2. pxBlock->pxNextFreeBlock == NULL（到达链表末尾，没找到）
     */

    /* 步骤5：检查是否找到合适的块 */
    if (pxBlock != pxEnd) {
        /* 找到块了！pxBlock 是最佳匹配的空闲块
         * pxPreviousBlock 是它的前驱节点
         */

        /* 计算用户可用的指针地址
         * 公式：用户指针 = 块起始地址 + 头部大小
         */
        pvReturn = (void *)((uint8_t *)pxBlock + xHeapStructSize);

        /* 步骤6：从空闲链表移除该块 */
        pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

        /* 步骤7：更新统计信息 */
        xFreeBytesRemaining -= pxBlock->xBlockSize;

        if (xFreeBytesRemaining < xMinimumEverFreeBytesRemaining) {
            xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
            /* 记录历史最低空闲内存，用于评估内存压力 */
        }

        /* 步骤8：块分割（Splitting）
         *
         * 如果找到的块比需要的大，就分割成两块：
         * 1. 分配块：大小 = xWantedSize
         * 2. 剩余块：大小 = 原大小 - xWantedSize，加入空闲链表
         *
         * 分割条件：剩余空间 >= xHeapStructSize（能容纳一个完整的 BlockLink_t）
         */
        if ((pxBlock->xBlockSize - xWantedSize) > xHeapStructSize) {
            /* 可以分割 */

            /* 新空闲块的起始地址 */
            pxNewBlockLink = (BlockLink_t *)((uint8_t *)pxBlock + xWantedSize);

            /* 设置新空闲块的大小 */
            pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;

            /* 更新原块的大小 */
            pxBlock->xBlockSize = xWantedSize;

            /* 将新空闲块插入空闲链表 */
            prvInsertBlockIntoFreeList(pxNewBlockLink);
        }
    }
}
xTaskResumeAll();

return pvReturn;
```

### 分配过程图解

```
初始状态：空闲链表有 3 个空闲块
xStart ──→ [50字节] ──→ [80字节] ──→ [120字节] ──→ pxEnd

请求分配 40 字节：

步骤1：计算实际需要 = 40 + 16(头部) = 56 字节，对齐后 = 56 字节

步骤2-4：遍历链表
  [50字节] < 56，跳过
  [80字节] >= 56，找到！

步骤5：分配 [80字节] 块
  pvReturn = [80字节块起始] + 16 = 用户数据指针

步骤6：从链表移除 [80字节]
  xStart ──→ [50字节] ──→ [120字节] ──→ pxEnd

步骤7：更新统计
  xFreeBytesRemaining -= 80

步骤8：分割 [80字节]
  剩余 80 - 56 = 24 字节 < xHeapStructSize(16)？不对齐
  实际：80 - 56 = 24 >= 16，可以分割！

  分割后：
  [56字节分配块] + [24字节新空闲块]

最终状态：
xStart ──→ [50字节] ──→ [24字节] ──→ [120字节] ──→ pxEnd
```

---

## 5.2.3 vPortFree 与块合并算法详解

### 释放函数入口

```c
void vPortFree(void *pv) {
    BlockLink_t *pxLink;

    if (pv != NULL) {
        /* 步骤1：获取块头部
         *
         * pv 是用户数据指针
         * 块头部在其前面 xHeapStructSize 字节处
         *
         * 公式：pxLink = pv - xHeapStructSize
         */
        pxLink = (BlockLink_t *)((uint8_t *)pv - xHeapStructSize);

        /* 确认这个块确实是从堆分配的（调试用） */
        configASSERT(pxLink->xBlockSize > 0);
        configASSERT(pxLink->pxNextFreeBlock == NULL); /* 不应该在空闲链表中 */

        /* 步骤2：插入空闲链表并合并相邻块 */
        prvInsertBlockIntoFreeList(pxLink);

        /* 步骤3：更新统计 */
        xFreeBytesRemaining += pxLink->xBlockSize;
    }
}
```

### 相邻块合并算法（核心亮点）

```c
/* prvInsertBlockIntoFreeList - 关键函数实现 */

static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert) {
    BlockLink_t *pxIterator;
    uint8_t *puc;

    /* ============================================================
     * heap_4 的空闲链表按内存地址排序！
     *
     * 这是实现 O(1) 合并的关键：
     * - 如果链表按地址排序，那么相邻的块在链表中也是相邻的
     * - 合并时只需检查前一个块和后一个块
     * ============================================================
     */

    /* 步骤1：查找插入位置（按地址升序）
     *
     * 遍历链表，找到第一个 pxIterator->pxNextFreeBlock > pxBlockToInsert 的位置
     * 即找到 pxBlockToInsert 应该插入在 pxIterator 和 pxIterator->pxNextFreeBlock 之间
     */
    for (pxIterator = &xStart;
         pxIterator->pxNextFreeBlock < pxBlockToInsert;  /* 地址比较 */
         pxIterator = pxIterator->pxNextFreeBlock) {
        /* 循环体为空，由 for 循环完成遍历 */
        /* 退出时：pxIterator < pxBlockToInsert < pxIterator->pxNextFreeBlock */
    }

    /* 步骤2：检查是否可以与前一个块合并
     *
     * 合并条件：
     * 前一个块的结束地址 == 当前块的起始地址
     * 即：pxIterator + pxIterator->xBlockSize == pxBlockToInsert
     */
    puc = (uint8_t *)pxIterator;

    if ((puc + pxIterator->xBlockSize) == (uint8_t *)pxBlockToInsert) {
        /* 前一个块（pxIterator）正好紧邻 pxBlockToInsert，可以合并！ */

        /* 合并后的块：pxIterator（保留前一个块的节点） */
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;  /* 更新为合并后的块 */
    }
    /* 注意：合并后 pxBlockToInsert 指向合并后的大块 */

    /* 步骤3：检查是否可以与后一个块合并
     *
     * 合并条件：
     * 当前块的结束地址 == 后一个块的起始地址
     * 即：pxBlockToInsert + pxBlockToInsert->xBlockSize == pxIterator->pxNextFreeBlock
     */
    if ((uint8_t *)pxBlockToInsert + pxBlockToInsert->xBlockSize ==
        (uint8_t *)pxIterator->pxNextFreeBlock) {
        /* 后一个块正好紧邻当前块，可以合并！ */

        /* 合并后的大小 */
        pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;

        /* 更新 next 指针，跳过后一个块 */
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
    }

    /* 步骤4：插入块到链表
     *
     * 将 pxBlockToInsert 插入到 pxIterator 和 pxIterator->pxNextFreeBlock 之间
     */
    pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    pxIterator->pxNextFreeBlock = pxBlockToInsert;
}
```

### 合并过程图解

```
场景：释放中间块，触发向前合并

合并前内存布局：
┌──────────────────────────────────────────┐
│  Block A (free)  │  Block B (free)  │ C  │
│  50字节           │  30字节           │ 40 │
└──────────────────────────────────────────┘
地址：0x1000       0x1032          0x1050

空闲链表：
xStart → [A:50] → [B:30] → [C:40] → pxEnd

现在释放 Block C（0x1050，大小40）：

步骤1：找到插入位置
  xStart → [A:50] → [B:30] → [C:40] → pxEnd
                          ↑
                    应该在 B 和 pxEnd 之间插入

步骤2：检查与前一个块（B）是否相邻
  B 的结束地址 = 0x1032 + 30 = 0x1050 = C 的起始地址
  相邻！可以合并

步骤2后：
  合并为 [B+C:70字节]
  xStart → [A:50] → [B+C:70] → pxEnd

步骤3：检查与后一个块（pxEnd）是否相邻
  B+C 的结束地址 = 0x1032 + 70 = 0x1078
  pxEnd 的地址 > 0x1078，不相邻

步骤4：插入链表
  xStart → [A:50] → [B+C:70] → pxEnd

最终内存布局：
┌──────────────────────────────────────────┐
│  Block A (free) │     Block B+C (free)   │
│  50字节          │     70字节              │
└──────────────────────────────────────────┘
```

---

## 5.2.4 heap_4 完整变量与初始化

### vPortInitialiseBlocks 初始化

```c
/* 堆初始化函数 - 在 pvPortMalloc 首次调用时自动调用 */
void vPortInitialiseBlocks(void) {
    /* 确保只在第一次分配前调用一次 */
    if (pxEnd == NULL) {
        /* 初始化结束哨兵节点
         * pxEnd 是一个虚拟节点，地址在堆末尾之外
         * 它的 xBlockSize = 0，作为链表结束标记
         */
        pxEnd = (BlockLink_t *)((size_t)ucHeap + configTOTAL_HEAP_SIZE);
        pxEnd->xBlockSize = 0;
        pxEnd->pxNextFreeBlock = NULL;

        /* 初始化空闲链表起始节点 */
        xStart.pxNextFreeBlock = pxEnd;
        xStart.xBlockSize = 0;

        /* 初始化统计变量 */
        xFreeBytesRemaining = configTOTAL_HEAP_SIZE;
        xMinimumEverFreeBytesRemaining = configTOTAL_HEAP_SIZE;
    }
}
```

### 堆初始化后的内存布局

```
┌─────────────────────────────────────────────────────────────────────┐
│                          堆内存区域                                  │
│                       (configTOTAL_HEAP_SIZE)                        │
├─────────────────────────────────────────────────────────────────┬───┤
│                                                                 │   │
│  ┌─────────────────────────────────────────────────────────┐   │End│
│  │                                                         │   │   │
│  │            全部空闲 - 只有一个巨大的空闲块                 │ ←─┼───┤
│  │                                                         │   │   │
│  └─────────────────────────────────────────────────────────┘   │   │
│                                                                  │   │
├─────────────────────────────────────────────────────────────────┴───┤
│  pxEnd 虚拟节点（不在堆内）                                          │
└─────────────────────────────────────────────────────────────────────┘

初始空闲链表：
xStart.pxNextFreeBlock = pxEnd
xStart.xBlockSize = 0

xStart ──→ pxEnd ──→ NULL
```

---

## 5.2.5 heap_4 vs heap_2 核心差异对比

### 空闲链表组织方式不同

```
heap_2: 按大小排序的空闲链表
┌─────────────────────────────────────┐
│ 空闲链表（按 xBlockSize 升序）         │
│                                     │
│ xStart ──→ [30] ──→ [50] ──→ [80] ──→ pxEnd │
│   ↑                                  ↑
│ 最小块                           最大块 │
│                                     │
│ 分配时：O(n) 遍历，找最小且满足的块     │
│ 释放时：O(n) 遍历，按大小插入           │
└─────────────────────────────────────┘

heap_4: 按地址排序的空闲链表
┌─────────────────────────────────────┐
│ 空闲链表（按内存地址升序）              │
│                                     │
│ xStart ──→ [0x1000] ──→ [0x1080] ──→ [0x1200] ──→ pxEnd │
│   ↑          地址升序                   │
│                                     │
│ 分配时：O(n) 遍历，找最小且满足的块     │
│ 释放时：O(1) 合并 + O(n) 定位插入点     │
└─────────────────────────────────────┘
```

### 合并算法差异

```
heap_2: 不合并
┌─────────┐   ┌─────────┐   ┌─────────┐
│ 已释放  │   │ 已分配  │   │ 已释放  │
│   A     │   │    B    │   │   C     │
└─────────┘   └─────────┘   └─────────┘

A 和 C 都空闲但不能合并，产生碎片：
┌──────┐     ┌──────┐
│  A   │     │  C   │
│ 30B  │     │ 50B  │
└──────┘     └──────┘

heap_4: 主动合并相邻空闲块
┌─────────┐   ┌─────────┐   ┌─────────┐
│ 已释放  │ + │ 已分配  │ + │ 已释放  │  →  ┌─────────┐
│   A     │   │    B    │   │   C     │     │  A+C    │
└─────────┘   └─────────┘   └─────────┘     │  80B    │
                                             └─────────┘
合并后变成一个大块，可再利用
```

---

## 5.2.6 五种堆策略完整对比表

| 策略 | 分配算法 | 释放算法 | 相邻合并 | 时间复杂度(分配) | 时间复杂度(释放) | 碎片程度 | 适用场景 |
|------|---------|---------|---------|------------------|-----------------|---------|---------|
| **heap_1** | 线性分配 | N/A (不支持) | 无 | O(1) | N/A | N/A | 静态系统，任务创建后不删除 |
| **heap_2** | 最佳适应 | 简单插入 | **不支持** | O(n) | O(n) | 严重 | 频繁 alloc/free，碎片可接受 |
| **heap_3** | 调用 malloc | 调用 free | 取决于 stdlib | 取决于 | 取决于 | 取决于 | 需要标准 C 库支持 |
| **heap_4** | 最佳适应 | 插入+合并 | **支持** | O(n) | O(1)* | 最小 | **大多数场景，推荐使用** |
| **heap_5** | 最佳适应 | 插入+合并 | **支持** | O(n) | O(1)* | 最小 | 多段非连续内存区域 |

> *O(1) 释放是在找到要释放的块之后，查找块本身需要 O(n)

---

## 5.2.7 内存碎片化深度剖析

### 什么是内存碎片

内存碎片是指 **已分配块之间无法使用的空闲小区域**。分为：

1. **内部碎片**：分配块大于请求大小，浪费在块内部
   ```
   请求 30 字节，分配 32 字节（对齐） → 浪费 2 字节
   ```

2. **外部碎片**：空闲块太小无法满足任何分配请求
   ```
   总空闲 100 字节，但最大连续空闲块只有 20 字节 → 无法分配 25 字节
   ```

### heap_2 碎片化示例

```
时间线演示：

1. 初始状态：堆全部空闲 200 字节
   ┌─────────────────────────────────────┐
   │            全部空闲 200B             │
   └─────────────────────────────────────┘

2. 分配 A = 50 字节
   ┌───────────┬─────────────────────────┐
   │  A (50B)  │      空闲 150B           │
   └───────────┴─────────────────────────┘

3. 分配 B = 30 字节
   ┌───────────┬─────────┬───────────────┐
   │  A (50B)  │ B (30B) │   空闲 120B    │
   └───────────┴─────────┴───────────────┘

4. 分配 C = 40 字节
   ┌───────────┬─────────┬─────────┬─────┐
   │  A (50B)  │ B (30B) │ C (40B) │ 80B │
   └───────────┴─────────┴─────────┴─────┘

5. 释放 B（30字节）
   ┌───────────┬ ─ ─ ─ ┬─────────┬─────┐
   │  A (50B)  │ B Free │ C (40B) │ 80B │
   └───────────┴ ─ ─ ─ ┴─────────┴─────┘
                  ↑
             产生碎片！

6. 尝试分配 D = 35 字节
   - heap_2 检查空闲块：[80B] >= 35 ✓
   - 成功分配！
   
7. 尝试分配 E = 60 字节
   - heap_2 检查空闲块：[80B] >= 60 ✓
   - 成功分配！

8. 继续分配 F = 50 字节，释放 A...
   
9. 长期运行后（多次分配/释放不同大小）：
   ┌────┬──┬────┬──┬──┬────┬──┬────┬──┬────┐
   │ A  │F │ B  │F │F │ C  │F │ D  │F │ E  │
   └────┴──┴────┴──┴──┴────┴──┴────┴──┴────┘
   F = 各种大小的空闲碎片
   
   问题：即使总空闲有 100 字节，但单个最大块只有 20 字节
   分配 30 字节会失败！
```

### heap_4 合并解决碎片

```
heap_4 的关键改进：释放时自动合并相邻空闲块

1. 初始状态
   ┌─────────────────────────────────────┐
   │            全部空闲 200B             │
   └─────────────────────────────────────┘

2. 分配 A=50, B=30, C=40
   ┌───────────┬─────────┬─────────┬─────┐
   │  A (50B)  │ B (30B) │ C (40B) │ 80B │
   └───────────┴─────────┴─────────┴─────┘

3. 释放 B
   ┌───────────┬ ─ ─ ─ ┬─────────┬─────┐
   │  A (50B)  │ B Free │ C (40B) │ 80B │
   └───────────┴ ─ ─ ─ ┴─────────┴─────┘
                  ↑
   heap_4 检测到 B 与 A 相邻且 A 空闲 → 合并！
   
   合并后：
   ┌───────────────┬─────────┬─────┐
   │  A+B (80B)    │ C (40B)  │ 80B │
   └───────────────┴─────────┴─────┘

4. 释放 A（已合并的大块再次合并）
   ┌ ─ ─ ─ ─ ─ ─ ─ ┬─────────┬─────┐
   │  A+B Free (80B)│ C (40B)  │ 80B │
   └ ─ ─ ─ ─ ─ ─ ─ ┴─────────┴─────┘
   ↑
   检测到 A+B 与左侧无块，与右侧 C 不相邻（中间隔着 B 的原位置？不，B已合并）
   等等，B 的位置被 A+B 覆盖了
   A+B 块起始 = A 起始 = 0x1000，A+B 大小 = 80
   C 起始 = 原来 A+50 = 0x1000+50 = 0x1032
   A+B 结束 = 0x1000+80 = 0x1050
   C 起始 = 0x1032 ≠ 0x1050，不相邻

   所以 A+B 不与 C 合并，但 C 之后是 80B 空闲
   
   释放 C：
   ┌───────────────┬ ─ ─ ─ ┬─────┐
   │  A+B (80B)    │ C Free│ 80B │
   └───────────────┴ ─ ─ ─ ┴─────┘
   ↑
   C 与 A+B 不相邻，但 C 与后面的 80B 相邻 → 合并！
   
   合并后：
   ┌───────────────┬─────────────┐
   │  A+B (80B)    │ C+80B (120B)│
   └───────────────┴─────────────┘

5. 释放最后的 80B 块
   ┌───────────────┬ ─ ─ ─ ─ ─ ─ ─ ┐
   │  A+B (80B)    │ C+80B (120B)   │
   └───────────────┴ ─ ─ ─ ─ ─ ─ ─ ┘
   ↑
   A+B 结束地址 = A+B 起始 + 80
   C+80B 起始 = A+B 起始 + 80 = C+80B 起始
   相邻！合并！

   最终：
   ┌─────────────────────────────────┐
   │       全部合并 200B             │
   └─────────────────────────────────┘

结果：heap_4 有效防止碎片化，内存可持续使用！
```

---

## 5.2.8 heap_4 边界检查与调试支持

### 堆完整性检查

```c
/* heap_4.c 中的调试检查函数 */

#if( configUSE_MALLOC_FAILED_HOOK == 1 )
    /* 分配失败钩子 - 当 pvPortMalloc 返回 NULL 时调用 */
    void vApplicationMallocFailedHook(void) {
        /* 可以记录日志、闪烁 LED、打印错误信息等 */
        printf("ERROR: Heap allocation failed! Free bytes: %u\n",
               (unsigned int)xPortGetFreeHeapSize());
    }
#endif

/* prvHeapCheck - 定期检查堆完整性（如果启用） */
static void prvHeapCheck(void) {
    BlockLink_t *pxBlock = xStart.pxNextFreeBlock;
    size_t xTotalFree = 0;

    /* 遍历所有空闲块，验证链表完整性 */
    while (pxBlock != pxEnd) {
        /* 检查：块大小必须 > 0 */
        configASSERT(pxBlock->xBlockSize > 0);

        /* 检查：块大小必须对齐 */
        configASSERT((pxBlock->xBlockSize & heapBYTE_ALIGNMENT_MASK) == 0);

        /* 检查：块地址必须对齐 */
        configASSERT(((size_t)pxBlock & heapBYTE_ALIGNMENT_MASK) == 0);

        /* 累计空闲大小 */
        xTotalFree += pxBlock->xBlockSize;

        /* 移动到下一个块 */
        pxBlock = pxBlock->pxNextFreeBlock;

        /* 检查：pxNextFreeBlock 不能为 NULL（除非是 pxEnd） */
        configASSERT(pxBlock != NULL || pxBlock == pxEnd);
    }

    /* 验证统计一致性 */
    configASSERT(xTotalFree == xFreeBytesRemaining);
}
```

### 内存分配失败处理

```c
/* 分配失败的最佳实践 */

void safeTaskCreate(void) {
    /* 方法1：检查返回值 */
    TaskHandle_t handle = NULL;
    BaseType_t result = xTaskCreate(
        vTaskCode,           /* 任务函数 */
        "SafeTask",          /* 任务名 */
        256,                 /* 栈大小（字） */
        NULL,                /* 参数 */
        1,                   /* 优先级 */
        &handle              /* 句柄 */
    );

    if (result != pdPASS) {
        /* 分配失败处理 */
        printf("Task creation failed! Heap remaining: %u\n",
               (unsigned int)xPortGetFreeHeapSize());
        /* 可以尝试：
         * 1. 减小其他任务的栈大小
         * 2. 增大 configTOTAL_HEAP_SIZE
         * 3. 延迟创建任务
         */
    }
}

/* 方法2：使用 try-create 模式 */
BaseType_t xTaskCreateSafe(TaskFunction_t pvTaskCode,
                           const char *pcName,
                           uint32_t ulStackDepth,
                           void *pvParameters,
                           UBaseType_t uxPriority,
                           TaskHandle_t *pxCreatedTask) {
    /* 记录分配前的空闲堆大小 */
    size_t xFreeBefore = xPortGetFreeHeapSize();

    /* 尝试分配 */
    *pxCreatedTask = xTaskCreateStatic(
        pvTaskCode, pcName, ulStackDepth,
        pvParameters, uxPriority,
        pxStackBuffer, /* 静态分配的栈 */
        pxTaskBuffer   /* 静态分配的 TCB */
    );

    if (*pxCreatedTask != NULL) {
        return pdPASS;
    } else {
        /* 分配失败，记录信息 */
        size_t xFreeAfter = xPortGetFreeHeapSize();
        printf("Task create failed. Before: %u, After: %u, Requested: %u\n",
               (unsigned int)xFreeBefore,
               (unsigned int)xFreeAfter,
               (unsigned int)(ulStackDepth * sizeof(StackType_t)));
        return pdFAIL;
    }
}
```

---

## 5.2.9 heap_5 多区域内存管理

### heap_5 vs heap_4 的核心区别

heap_5 允许在 **多个非连续的内存区域** 上建立堆，适用于有多个独立 SRAM 区域的 MCU：

```
MCU 有多个独立内存区域：
┌─────────────────────────────────────┐
│  SRAM0  64KB (0x20000000 开始)       │
├─────────────────────────────────────┤
│  SRAM1  64KB (0x30000000 开始)       │
├─────────────────────────────────────┤
│  SRAM2  32KB (0x40000000 开始)       │
└─────────────────────────────────────┘

heap_4 只能在单一区域分配
heap_5 可以在所有区域统一分配
```

### heap_5 初始化

```c
/* 定义内存区域 */
static uint8_t ucHeap1[ 64 * 1024 ] __attribute__((section (".heap_region1")));
static uint8_t ucHeap2[ 64 * 1024 ] __attribute__((section (".heap_region2")));
static uint8_t ucHeap3[ 32 * 1024 ] __attribute__((section (".heap_region3")));

/* 内存区域描述数组 */
const HeapRegion_t xHeapRegions[] = {
    /* 区域 1 */
    { (void *)ucHeap1, sizeof(ucHeap1) },

    /* 区域 2 */
    { (void *)ucHeap2, sizeof(ucHeap2) },

    /* 区域 3 */
    { (void *)ucHeap3, sizeof(ucHeap3) },

    /* 结束标记 - 必须以 NULL, 0 结尾 */
    { NULL, 0 }
};

/* 在调度器启动前调用 */
void vApplicationSetupTimerInterrupt(void) {
    /* 其他初始化... */
}

/* 在 FreeRTOS 初始化时设置堆区域 */
int main(void) {
    /* 配置内存区域 */
    vPortDefineHeapRegions(xHeapRegions);

    /* 启动调度器 */
    vTaskStartScheduler();

    return 0;
}
```

### heap_5 的分配策略

heap_5 使用与 heap_4 相同的最佳适应+合并算法，但空闲链表按 **区域** 组织：

```
heap_5 空闲链表结构：

区域 1 空闲链表：
xStart1 ──→ [块] ──→ [块] ──→ pxEnd1

区域 2 空闲链表：
xStart2 ──→ [块] ──→ pxEnd2

区域 3 空闲链表：
xStart3 ──→ [块] ──→ pxEnd3

分配策略：
1. 优先在能满足请求的第一个区域分配
2. 跨区域合并不支持（物理上不连续）
3. 每个区域独立管理
```

---

### heap_5.c — 非连续内存区

**特点：**
- 在多个非连续内存区域分配
- 适合有多个独立 SRAM 区域的芯片
- 初始化时需要指定所有内存区域

```c
// 定义多个内存区域
static uint8_t ucHeap1[ 64 * 1024 ] __attribute__((section (".ram_d1")));
static uint8_t ucHeap2[ 64 * 1024 ] __attribute__((section (".ram_d2")));

// 初始化
HeapRegion_t xHeapRegions[] = {
    { ucHeap1, sizeof(ucHeap1) },
    { ucHeap2, sizeof(ucHeap2) },
    { NULL, 0 }  // 结束标记
};

vPortDefineHeapRegions(xHeapRegions);
```

---

## 5.3 内存分配 API

### 动态内存分配

```c
#include "stdlib.h"  // 标准库

// FreeRTOS 内存分配
void *pvPortMalloc(size_t xWantedSize);   // 分配
void vPortFree(void *pv);                   // 释放

// 分配后初始化为 0
void *pvPortMalloc(size_t xWantedSize) {
    void *pv = malloc(xWantedSize);
    if (pv) {
        memset(pv, 0, xWantedSize);
    }
    return pv;
}

// 获取内存统计信息
size_t xPortGetFreeHeapSize(void);         // 当前剩余堆大小
size_t xPortGetMinimumEverFreeHeapSize(void); // 历史最低剩余

// heap_3 不支持这些函数
```

### 内存分配示例

```c
// 创建任务（内部调用 pvPortMalloc）
TaskHandle_t handle;
xTaskCreate(TaskFunc, "Task", 256, NULL, 1, &handle);  // 分配 TCB + 栈

// 创建队列（内部调用 pvPortMalloc）
QueueHandle_t queue = xQueueCreate(10, sizeof(int));  // 分配队列结构

// 删除任务时释放内存
vTaskDelete(handle);  // 释放 TCB 和栈

// 手动释放
void *buf = pvPortMalloc(100);
if (buf) {
    // 使用...
    vPortFree(buf);  // 释放
}
```

---

## 5.4 栈内存管理

### 任务栈

每个任务有独立的栈空间：

```c
// 栈大小计算
// 256 字 = 1024 字节 (STM32)
// 512 字 = 2048 字节

xTaskCreate(TaskFunc, "Task", 512, NULL, 1, &handle);
```

### 栈溢出检测

```c
#define configCHECK_FOR_STACK_OVERFLOW    2  // 0=关闭, 1=简单检测, 2=详细检测

// 栈溢出钩子函数
void vApplicationStackOverflowHook(
    TaskHandle_t xTask,
    char *pcTaskName
) {
    // 任务栈溢出处理
    // 可记录日志、复位等
}

// 详细检测会检查栈末尾的 canary 值
```

### 栈使用监控

```c
// 获取任务栈剩余空间
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);

// 返回值：自任务创建以来的最小栈剩余空间
// 低于 20% 时应考虑增大栈大小
```

---

## 5.5 常见内存问题

### 内存泄漏

```c
// ❌ 错误：分配后忘记释放
void BadTask(void *pv) {
    char *buf = pvPortMalloc(100);
    // 使用 buf...
    // 函数结束但没有 vPortFree(buf)
    // 每次调用泄漏 100 字节
}

// ✅ 正确：确保释放
void GoodTask(void *pv) {
    char *buf = pvPortMalloc(100);
    if (buf) {
        // 使用 buf...
        vPortFree(buf);  // 释放
    }
}
```

### 内存碎片化

```c
// heap_2 的问题
// 分配：50字节、30字节、40字节、释放：30字节、分配：35字节
// 碎片化：出现很多小碎片无法利用

// 解决：使用 heap_4（合并相邻空闲块）
#define configAPPLICATION_ALLOCATED_HEAP    1  // 用户自定义堆
```

### 堆内存耗尽

```c
// 创建任务失败检查
BaseType_t result = xTaskCreate(...);
if (result != pdPASS) {
    // 堆内存不足
    // 减小 configTOTAL_HEAP_SIZE 分配的任务栈
    // 或增大 configTOTAL_HEAP_SIZE
}
```

---

## 5.6 面试高频问题

### Q1：FreeRTOS 有哪几种堆内存分配策略？区别？

| 策略 | 分配算法 | 释放 | 合并相邻 | 时间复杂度(Alloc) | 时间复杂度(Free) | 碎片程度 | 适用场景 |
|------|---------|------|--------|------------------|-----------------|---------|---------|
| heap_1 | 线性 | ❌ | ❌ | **O(1)** | N/A | N/A | 静态系统，任务创建后不删除 |
| heap_2 | 最佳适应 | ✅ | ❌ | O(n) | O(n) | 严重 | 频繁 alloc/free，碎片可接受 |
| heap_3 | 标准malloc | ✅ | 取决于库 | 取决于 | 取决于 | 取决于 | 需要标准 C 库支持 |
| heap_4 | 最佳适应 | ✅ | ✅ | O(n) | O(1)* | **最小** | **大多数场景，推荐使用** |
| heap_5 | 最佳适应 | ✅ | ✅ | O(n) | O(1)* | **最小** | 多段非连续内存区域 |

> *O(1) 释放是在找到要释放的块之后，查找块本身需要 O(n)

**面试加分回答：**
> heap_4 是最常用的实现，它采用最佳适应算法找到最小的满足大小的空闲块。释放时通过 `prvInsertBlockIntoFreeList()` 函数检查并合并相邻的空闲块。空闲链表按内存地址排序（而非大小排序），这是实现 O(1) 合并的关键——因为地址排序确保了相邻内存在链表中也是相邻的。

---

### Q2：什么是内存碎片？如何避免？

**参考答案：**

内存碎片分为 **内部碎片** 和 **外部碎片**：

1. **内部碎片**：分配块比请求的大，浪费在块内部
   - 示例：请求 30 字节，系统分配 32 字节（8 字节对齐），浪费 2 字节
   - 无法避免，只能优化对齐方式减少

2. **外部碎片**：空闲块太小，无法满足任何分配请求
   - 示例：总空闲 100 字节，但最大连续块只有 20 字节，无法分配 30 字节
   - 这是主要问题

**碎片化过程演示（heap_2 不合并）：**

```
1. 分配 A=50, B=30, C=40
   ┌────┬────┬────┬────┐
   │ A  │ B  │ C  │ 80 │
   └────┴────┴────┴────┘

2. 释放 B
   ┌────┬空┬────┬────┐
   │ A  │30│ C  │ 80 │
   └────┴空┴────┴────┘
              ↑
         碎片产生了！

3. 多次分配/释放后
   ┌──┬─┬──┬──┬─┬──┬─┬──┬──┬─┐
   │A │F│B │C │F│D │F│E │F │F│  (F=空闲碎片)
   └──┴─┴──┴──┴─┴──┴─┴──┴──┴─┘
   
   总空闲可能很多，但单个最大块很小
```

**避免碎片化的方法：**

1. **使用 heap_4**：自动合并相邻空闲块
   ```
   释放 B 时，如果 A 或 C 也是空闲的，自动合并成大块
   ```

2. **避免频繁分配/释放不同大小的内存**：
   ```c
   // 不好：频繁小块分配
   for (int i = 0; i < 1000; i++) {
       char *buf = pvPortMalloc(10);
       // ...
       vPortFree(buf);
   }

   // 好：批量分配，一次申请够用
   char *buf = pvPortMalloc(10000);
   for (int i = 0; i < 1000; i++) {
       process(buf + i * 10, 10);
   }
   vPortFree(buf);
   ```

3. **固定大小对象池**：
   ```c
   // 使用消息队列代替频繁 malloc
   xQueue = xQueueCreate(10, sizeof(Message_t));
   // 预分配，避免运行时碎片
   ```

---

### Q3：如何检测栈溢出？

**参考答案：**

栈溢出是嵌入式系统的常见致命错误，FreeRTOS 提供多层检测：

**方法 1：栈溢出钩子函数**

```c
// FreeRTOSConfig.h
#define configCHECK_FOR_STACK_OVERFLOW    2  // 0=关闭, 1=简单, 2=详细

// 实现钩子
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("STACK OVERFLOW! Task: %s\n", pcTaskName);
    // 致命错误处理：记录日志、复位等
}
```

**方法 2：监控栈水位线**

```c
// 获取任务创建以来的最小栈剩余空间
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);

if (uxHighWaterMark < 50) {  // 小于 50 字危险
    printf("WARNING: Stack running low! Remaining: %u words\n", uxHighWaterMark);
}
```

**栈大小计算原则：**

```c
// 普通任务：256-512 字（STM32 是 32 位，字=4 字节）
xTaskCreate(TaskFunc, "Task", 256, NULL, 1, &handle);  // 256*4=1KB 栈

// 递归任务：需要更大栈
xTaskCreate(RecursiveTask, "RecTask", 1024, NULL, 1, &handle);

// 检查实际使用：
// uxTaskGetStackHighWaterMark() 返回值 * 4 = 实际剩余字节数
```

**栈溢出检测原理（configCHECK_FOR_STACK_OVERFLOW=2）：**

```
任务栈布局：
┌────────────────────┐ ← 栈顶（高地址）
│    本地变量         │
│    函数调用         │
│    寄存器保存       │
├────────────────────┤
│    栈溢出检测区    │ ← 填充 canary 值（如 0xA5）
│    (Stack Guard)   │
├────────────────────┤ ← 栈底（低地址）
│    TCB             │
└────────────────────┘

调度器检测：定期检查 canary 值是否被破坏
```

---

### Q4：堆内存耗尽会怎样？

**参考答案：**

当堆内存耗尽时，`pvPortMalloc()` 返回 `NULL`，后续操作可能失败：

```c
void *ptr = pvPortMalloc(100);
if (ptr == NULL) {  // 必须检查！
    // 处理分配失败
}
```

**各 API 的失败行为：**

| API | 失败返回值 | 影响 |
|-----|----------|-----|
| `pvPortMalloc()` | NULL | 调用者需处理 |
| `xTaskCreate()` | pdFAIL | 任务未创建 |
| `xQueueCreate()` | NULL | 队列不可用 |
| `xSemaphoreCreateMutex()` | NULL | 互斥锁不可用 |
| `vTaskDelay()` | - | 不受影响（不堆分配） |

**最佳实践：分配前检查**

```c
BaseType_t xSafeTaskCreate(void) {
    size_t xFreeBefore = xPortGetFreeHeapSize();
    TaskHandle_t handle = NULL;
    BaseType_t result = xTaskCreate(TaskFunc, "Task", 512, NULL, 1, &handle);

    if (result != pdPASS) {
        size_t xFreeAfter = xPortGetFreeHeapSize();
        printf("Task create FAILED!\n");
        printf("  Free before: %u bytes\n", (unsigned int)xFreeBefore);
        printf("  Free after:  %u bytes\n", (unsigned int)xFreeAfter);
        printf("  Task stack:  %u bytes\n", 512 * sizeof(StackType_t));
        return pdFAIL;
    }
    return pdPASS;
}
```

**内存压力监测**

```c
// 定期检查内存健康
void vMonitorMemoryHealth(void) {
    size_t xFree = xPortGetFreeHeapSize();
    size_t xMinEver = xPortGetMinimumEverFreeHeapSize();

    printf("Heap Status:\n");
    printf("  Current free:  %u bytes\n", (unsigned int)xFree);
    printf("  Min ever free: %u bytes\n", (unsigned int)xMinEver);
    printf("  Total heap:    %u bytes\n", (unsigned int)configTOTAL_HEAP_SIZE);

    // 如果当前空闲接近历史最低，说明有内存泄漏
    if (xFree < xMinEver + 100) {
        printf("  WARNING: Memory pressure detected!\n");
    }
}
```

---

### Q5：heap_4 如何合并空闲块？详细描述算法

**参考答案（重点问题，必须详细）：**

heap_4 的合并算法在 `prvInsertBlockIntoFreeList()` 函数中实现，核心思想是 **按地址排序的空闲链表 + 相邻检测**。

**关键点 1：空闲链表按地址排序**

```c
// heap_4 的空闲链表结构
static BlockLink_t xStart;       // 哨兵节点
static BlockLink_t *pxEnd = NULL; // 结束哨兵

// 链表按内存地址升序排列！
// xStart -> [addr:0x1000] -> [addr:0x1080] -> [addr:0x1200] -> pxEnd
```

**为什么按地址排序而不是按大小排序？**

> 因为按地址排序使得 **O(1) 合并** 成为可能。相邻的内存块在链表中也是相邻的，合并只需检查前后两个节点。

**关键点 2：合并算法步骤**

```c
static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert) {
    BlockLink_t *pxIterator;

    // 步骤1：找到插入位置（按地址升序）
    // 遍历直到找到 pxIterator->pxNextFreeBlock > pxBlockToInsert
    for (pxIterator = &xStart;
         pxIterator->pxNextFreeBlock < pxBlockToInsert;
         pxIterator = pxIterator->pxNextFreeBlock) {
        // 空循环体
    }

    // 退出时：pxIterator < pxBlockToInsert < pxIterator->pxNextFreeBlock

    // 步骤2：检查能否与前一个块（pxIterator）合并
    uint8_t *puc = (uint8_t *)pxIterator;
    if ((puc + pxIterator->xBlockSize) == (uint8_t *)pxBlockToInsert) {
        // 前一个块的结束地址 == 当前块起始地址
        // 可以合并！
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator;  // 更新指针
    }

    // 步骤3：检查能否与后一个块（pxIterator->pxNextFreeBlock）合并
    if ((uint8_t *)pxBlockToInsert + pxBlockToInsert->xBlockSize ==
        (uint8_t *)pxIterator->pxNextFreeBlock) {
        // 当前块的结束地址 == 后一个块起始地址
        // 可以合并！
        pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
    }

    // 步骤4：插入块到链表
    pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    pxIterator->pxNextFreeBlock = pxBlockToInsert;
}
```

**合并过程图解：**

```
场景：释放 Block B（30字节），它前面的 Block A（50字节）是空闲的

合并前内存：
┌───────────┬─────────┬─────────┐
│  A (free) │ B(alloc)│  C(free)│
│  50B      │ 30B     │  40B    │
└───────────┴─────────┴─────────┘
0x1000     0x1032    0x1050   0x1078

释放 B 后：
1. 找到插入位置：A(0x1000) < B(0x1032) < C(0x1050)

2. 检查前驱 A：
   A.end = 0x1000 + 50 = 0x1032 = B.start
   相邻！合并 A 和 B → A_new(80B)

3. 检查后继 C：
   A_new.end = 0x1000 + 80 = 0x1050 = C.start
   相邻！继续合并 A_new 和 C → A_big(120B)

4. 插入链表

合并后：
┌─────────────────────┐
│     A_big (free)     │
│     120B            │
└─────────────────────┘
0x1000              0x1078
```

---

### Q6：heap_4 的最佳适应算法是如何工作的？

**参考答案：**

```c
void *pvPortMalloc(size_t xWantedSize) {
    BlockLink_t *pxBlock, *pxPreviousBlock;

    // 1. 对齐 + 加头部
    xWantedSize += xHeapStructSize;
    xWantedSize = (xWantedSize + heapBYTE_ALIGNMENT_MASK) & ~heapBYTE_ALIGNMENT_MASK;

    // 2. 遍历空闲链表找最小满足块
    pxPreviousBlock = &xStart;
    pxBlock = xStart.pxNextFreeBlock;

    while ((pxBlock->xBlockSize < xWantedSize) &&
           (pxBlock->pxNextFreeBlock != NULL)) {
        pxPreviousBlock = pxBlock;
        pxBlock = pxBlock->pxNextFreeBlock;
    }

    // 3. 找到块后，分配并可能分割
    if (pxBlock != pxEnd) {
        // 从链表移除
        pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

        // 如果太大就分割
        if ((pxBlock->xBlockSize - xWantedSize) > xHeapStructSize) {
            // 分割成两块
        }
    }
}
```

**最佳适应 vs 首次适应：**

| 算法 | 描述 | 优点 | 缺点 |
|-----|------|-----|-----|
| 首次适应 | 找到第一个满足的块 | 快 | 产生很多小碎片 |
| 最佳适应 | 找到最小满足的块 | 保留大块 | 需要遍历全部 |

heap_4 用最佳适应，尽量保留大块内存给后续大分配使用。

---

### Q7：heap_1 为什么不允许释放？

**参考答案：**

heap_1 是最简单的实现，采用 **线性分配策略**：

```c
// heap_1.c 核心逻辑
static size_t xNextFreeByte = 0;  // 下一次分配的起始位置

void *pvPortMalloc(size_t xWantedSize) {
    vTaskSuspendAll();

    if ((xNextFreeByte + xWantedSize) <= configTOTAL_HEAP_SIZE) {
        void *pvReturn = &ucHeap[xNextFreeByte];
        xNextFreeByte += xWantedSize;
        xTaskResumeAll();
        return pvReturn;
    }

    xTaskResumeAll();
    return NULL;
}
```

**特点：**
- `xNextFreeByte` 只会增加，不会减少
- 释放函数根本不存在
- O(1) 分配，非常快
- 没有碎片管理开销

**适用场景：**
- 任务创建后永不删除
- 静态系统
- 资源极度受限的 MCU

---

### Q8：pvPortMalloc 返回 NULL 后会发生什么？

**参考答案：**

当 `pvPortMalloc` 返回 NULL 时，意味着堆耗尽。FreeRTOS 的核心对象创建函数都有检查：

```c
// xTaskCreate 内部伪代码
if (pvPortMalloc(sizeof(TCB_t)) == NULL) {
    return pdFAIL;  // 分配失败，返回
}

// xQueueCreate 内部伪代码
if (pvPortMalloc(sizeof(Queue_t)) == NULL) {
    return NULL;  // 队列创建失败
}
```

**如果忽视 NULL 检查，后果严重：**

```c
// 危险代码！
TaskHandle_t handle;
xTaskCreate(TaskFunc, "Task", 256, NULL, 1, &handle);  // 如果失败，handle 仍是 NULL
// 后续使用 handle → 崩溃

// 安全代码
TaskHandle_t handle = NULL;
if (xTaskCreate(TaskFunc, "Task", 256, NULL, 1, &handle) != pdPASS) {
    // 处理失败：减小栈、延迟创建、报错等
}
```

**启用内存分配失败钩子：**

```c
// FreeRTOSConfig.h
#define configUSE_MALLOC_FAILED_HOOK    1

// 实现钩子
void vApplicationMallocFailedHook(void) {
    printf("MALLOC FAILED!\n");
    printf("Free heap: %u bytes\n", (unsigned int)xPortGetFreeHeapSize());
    printf("Min ever:  %u bytes\n", (unsigned int)xPortGetMinimumEverFreeBytesRemaining());

    // 强制进入错误处理
    configASSERT(0);
}
```

---

### Q9：heap_3 和标准 malloc 有什么区别？

**参考答案：**

heap_3 只是标准 `malloc/free` 的简单包装：

```c
// heap_3.c
#include <stdlib.h>

void *pvPortMalloc(size_t xWantedSize) {
    return malloc(xWantedSize);  // 直接调用 stdlib
}

void vPortFree(void *pv) {
    free(pv);  // 直接调用 stdlib
}

size_t xPortGetFreeHeapSize(void) {
    return 0;  // 不支持！
}
```

**关键区别：**

| 特性 | heap_3 | heap_4 |
|-----|-------|-------|
| 内存来源 | 系统堆（linker 配置） | configTOTAL_HEAP_SIZE |
| 线程安全 | 取决于 stdlib | FreeRTOS 自己控制 |
| xPortGetFreeHeapSize | ❌ 不支持 | ✅ 支持 |
| 碎片管理 | 取决于 stdlib | 自己实现 |
| 确定性 | 取决于 stdlib | 确定 |

**为什么还要用 heap_3？**

- 需要与第三方库共享堆（第三方库用 malloc）
- 需要标准库的内存调试工具
- 已有成熟的 linker script 堆配置

---

### Q10：如何选择堆策略？

**参考答案：**

```
选择决策树：

1. 系统是否需要删除任务？
   │
   ├─ 否 → heap_1（最快，最简单）
   │
   └─ 是 → 继续

2. 是否有多个独立内存区域？
   │
   ├─ 是 → heap_5
   │
   └─ 否 → 继续

3. 是否需要与标准库共享堆？
   │
   ├─ 是 → heap_3
   │
   └─ 否 → heap_4（推荐）
```

**一般推荐：**

- **首选 heap_4**：功能完整，碎片最少
- **资源极少**：heap_1
- **多区域 MCU**（如 STM32H7）：heap_5
- **需要调试工具**：heap_3

---

## 5.7 避坑指南

1. **使用 heap_4** — 综合最优，大多数场景选择
2. **栈大小宁大勿小** — 栈溢出导致 HardFault
3. **释放后置 NULL** — 避免悬空指针二次释放
4. **监控 `xPortGetFreeHeapSize()`** — 运行时检测内存健康
5. **避免频繁小块分配** — 减少碎片化
6. **`pvPortMalloc` 返回值必须检查** — NULL 表示分配失败

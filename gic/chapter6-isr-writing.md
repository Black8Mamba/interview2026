# 第六章：中断处理函数编写

## 6.1 ISR函数规范

### 函数声明宏

SDK使用宏定义来声明中断处理函数：

```c
/**
 * @brief   IRQ handler function declaration.
 *
 * @param[in] id        a vector name as defined in @p int_id.h
 */
#define IRQ_HANDLER(id) void __attribute__((section(".handlers"))) id(void)
```

**关键特性：**
- `__attribute__((section(".handlers")))`: 将ISR放入专门的代码段
- 确保中断向量表能正确找到处理函数

### 命名规范

```c
// 正确的命名方式 - 使用irq_id.h中定义的处理函数名
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)  // ✓ 正确

// 错误示例
void my_uart_isr(void)  // ✗ 错误 - 向量表无法识别
```

---

## 6.2 中断处理流程

### 中断处理完整流程图

```
┌─────────────────────────────────────────────────────────────────┐
│                      中断处理完整流程                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  外设触发中断                                                     │
│       │                                                          │
│       ▼                                                          │
│  ┌──────────────┐                                               │
│  │ GIC Distri- │                                               │
│  │ butor       │  收集中断, 检查优先级, 路由到目标核心              │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │ GIC Redistri-│  检查优先级屏蔽                                  │
│  │ butor       │                                                │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │ CPU Interface│  发送中断信号到CPU                              │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │ CPU Core     │  保存现场, 跳转到ISR                            │
│  │ (硬件自动)   │                                                │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────────────────────────────┐                       │
│  │       ISR (用户编写)                  │                       │
│  │                                       │                       │
│  │  1. Prologue (可选)                   │                       │
│  │     - 保存寄存器                      │                       │
│  │                                       │                       │
│  │  2. 处理中断                          │                       │
│  │     - 读写外设寄存器                  │                       │
│  │     - 清除中断标志                    │                       │
│  │                                       │                       │
│  │  3. Epilogue (可选)                   │                       │
│  │     - 恢复寄存器                      │                       │
│  │                                       │                       │
│  └──────┬───────────────────────────────┘                       │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────┐                                               │
│  │   EOI        │  写入EOIR通知GIC中断完成                       │
│  │              │  (部分外设需要)                                 │
│  └──────┬───────┘                                               │
│         │                                                        │
│         ▼                                                        │
│  CPU返回到被中断的任务                                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6.3 Prologue与Epilogue

### 什么是Prologue/Epilogue?

Prologue和Epilogue是ISR开头和结尾的保护代码，用于保存和恢复寄存器状态：

```c
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    //================= PROLOGUE =================
    // 保存现场 - 保存可能被破坏的寄存器
    __asm volatile (
        "PUSH {r0-r3, r12, lr}\n"    // 保存寄存器
    );

    //================= HANDLER =================
    // 中断处理代码
    handle_interrupt();

    //================= EPILOGUE =================
    // 恢复现场
    __asm volatile (
        "POP {r0-r3, r12, pc}\n"     // 恢复寄存器, 同时返回
    );
}
```

### SDK中的简化处理

实际上，SDK的头文件中定义了简化版本：

```c
// irq.h 中定义
#define IRQ_PROLOGUE()
#define IRQ_EPILOGUE()

// 编译器通常会自动处理寄存器保存/恢复
// 在ARM架构中, ISR调用会使用特定的栈帧
```

**编译器自动处理：**
- GCC的`__attribute__((interrupt))`会自动生成prologue/epilogue
- 栈帧自动保存必要寄存器
- 返回时自动恢复

### 完整ISR示例

```c
// 使用GCC的中断属性 - 编译器自动处理prologue/epilogue
void __attribute__((interrupt("IRQ"))) IRQ_LINFLEX_0_INT_HANDLER(void)
{
    uint8_t data;

    // 检查是否真正产生了接收中断
    if (LINFLEX0->SR.B.DRFRF == 1) {
        // 读取数据
        data = LINFLEX0->DR.B.DATA8;

        // 处理数据
        process_uart_data(data);
    }

    // 清除中断标志 (关键!)
    // 如果不清除，会立即再次触发中断
    LINFLEX0->SR.R = 0xFFFFFFFF;

    // 某些外设可能需要写入EOI
    // LINFLEX0->EOIR.R = ...;
}
```

---

## 6.4 ISR编写最佳实践

### 原则1: 快速执行

```c
// ✓ 好的做法: 最小化处理, 标志+退出
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    // 只读取数据到缓冲
    rx_buffer[rx_head++] = LINFLEX0->DR.B.DATA8;
    rx_head %= BUFFER_SIZE;

    // 清除标志
    LINFLEX0->SR.R = 0xFFFFFFFF;
}

// ✗ 避免: 在ISR中做耗时操作
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    // 不要这样!
    // - 复杂数据处理
    // - 字符串打印
    // - 延时操作
    // - 阻塞等待
    printf("Data received\n");  // 不好!
    process_data();              // 不好!
}
```

### 原则2: 清除中断标志

```c
// ✓ 关键: 在处理前或处理后清除中断标志
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    // 清除标志 (推荐在读取数据后清除)
    LINFLEX0->SR.R = 0xFFFFFFFF;
}

// 常见的标志清除方式:
// 1. 写入1清除: REG->ISR.R = 0xFFFFFFFF;
// 2. 读取清除: data = REG->DR.R; (某些外设)
// 3. 写入EOI:  REG->EOIR.R = ...;
```

### 原则3: 访问volatile变量

```c
// 使用volatile确保编译器优化时不会跳过
volatile uint32_t interrupt_flag = 0;

// ISR中
IRQ_HANDLER(IRQ_SIUL2_0_EXT_INT_0_INT_HANDLER)
{
    interrupt_flag = 1;
    SIUL2->ISR[0].R = 1;
}

// 主循环中
while (interrupt_flag == 0);  // 等待中断
interrupt_flag = 0;
```

### 原则4: 正确的数据类型

```c
// ✓ 使用固定宽度整数类型
#include <stdint.h>

IRQ_HANDLER(IRQ_TIMER_0_INT_HANDLER)
{
    uint32_t count;
    uint8_t status;

    count = TIMER0->CNT;
    status = TIMER0->SR.R;
}

// 避免使用int等非确定大小类型
```

---

## 6.5 实际外设驱动集成示例

### UART驱动集成

```c
//============================================================
// 文件: uart_isr.c
// 描述: UART中断驱动完整示例
//============================================================

#include <irq.h>
#include <irq_id.h>
#include <linflex.h>

//------------------------------------------------------------
// Ring Buffer结构
//------------------------------------------------------------
typedef struct {
    uint8_t data[256];
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t overflow;
} ring_buffer_t;

static ring_buffer_t uart_rx_buf;

//------------------------------------------------------------
// 初始化函数
//------------------------------------------------------------
void uart_driver_init(void)
{
    // 初始化缓冲
    uart_rx_buf.head = 0;
    uart_rx_buf.tail = 0;
    uart_rx_buf.overflow = 0;

    // 配置中断
    irq_config(
        IRQ_LINFLEX_0_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        16
    );

    // 使能中断
    irq_enable(IRQ_LINFLEX_0_INT_NUMBER);

    // 配置UART硬件
    LINFLEX0->CR.B.MDIS = 1;      // 进入配置模式
    LINFLEX0->CR.B.RXEN = 1;      // 使能接收
    LINFLEX0->CR.B.RFRIE = 1;     // 接收中断使能
    LINFLEX0->CR.B.OVRIE = 1;     // 溢出中断使能
    LINFLEX0->CR.B.MDIS = 0;      // 退出配置模式
}

//------------------------------------------------------------
// 中断处理函数
//------------------------------------------------------------
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    uint8_t data;
    uint32_t next_head;

    // 检查溢出
    if (LINFLEX0->SR.B.OVRF == 1) {
        uart_rx_buf.overflow = 1;
        LINFLEX0->SR.B.OVRF = 1;  // 清除标志
    }

    // 检查接收完成
    while (LINFLEX0->SR.B.DRFRF == 1) {
        // 读取数据
        data = LINFLEX0->DR.B.DATA8;

        // 计算下一个头位置
        next_head = (uart_rx_buf.head + 1) % sizeof(uart_rx_buf.data);

        // 检查溢出
        if (next_head == uart_rx_buf.tail) {
            uart_rx_buf.overflow = 1;
        } else {
            uart_rx_buf.data[uart_rx_buf.head] = data;
            uart_rx_buf.head = next_head;
        }
    }

    // 清除所有中断标志
    LINFLEX0->SR.R = 0xFFFFFFFF;
}

//------------------------------------------------------------
// API函数
//------------------------------------------------------------
uint32_t uart_read(uint8_t *data)
{
    uint32_t count = 0;

    // 从缓冲读取数据
    while (uart_rx_buf.tail != uart_rx_buf.head) {
        *data++ = uart_rx_buf.data[uart_rx_buf.tail];
        uart_rx_buf.tail = (uart_rx_buf.tail + 1) % sizeof(uart_rx_buf.data);
        count++;
    }

    return count;
}

uint32_t uart_get_rx_count(void)
{
    if (uart_rx_buf.head >= uart_rx_buf.tail) {
        return uart_rx_buf.head - uart_rx_buf.tail;
    } else {
        return sizeof(uart_rx_buf.data) - uart_rx_buf.tail + uart_rx_buf.head;
    }
}
```

### DMA完成中断示例

```c
#include <irq.h>
#include <irq_id.h>
#include <edma.h>

// DMA传输状态
typedef enum {
    DMA_IDLE,
    DMA_BUSY,
    DMA_COMPLETE,
    DMA_ERROR
} dma_state_t;

static volatile dma_state_t dma_state = DMA_IDLE;
static uint32_t dma_error_code = 0;

//------------------------------------------------------------
// DMA中断处理
//------------------------------------------------------------
IRQ_HANDLER(IRQ_EDMA_0_FLAGS_0_7_INT_HANDLER)
{
    uint32_t flags;

    // 读取DMA标志
    flags = EDMA0->TCD[0].CSR.B.DONE;

    if (flags) {
        // 传输完成
        dma_state = DMA_COMPLETE;
    } else {
        // 检查错误
        if (EDMA0->ES.B.ERR) {
            dma_state = DMA_ERROR;
            dma_error_code = EDMA0->ES.R;
        }
    }

    // 清除标志
    EDMA0->TCD[0].CSR.B.DONE = 1;
    EDMA0->INTFR.R = 0;
}

//------------------------------------------------------------
// DMA传输函数
//------------------------------------------------------------
int dma_transfer(uint32_t src, uint32_t dest, uint32_t size)
{
    if (dma_state == DMA_BUSY) {
        return -1;  // DMA忙
    }

    // 配置DMA
    // ... (DMA配置代码)

    // 配置中断
    irq_config(
        IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        8
    );
    irq_enable(IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER);

    // 启动传输
    dma_state = DMA_BUSY;
    EDMA0->TCD[0].CSR.B.START = 1;

    return 0;
}

//------------------------------------------------------------
// 等待DMA完成
//------------------------------------------------------------
int dma_wait_complete(uint32_t timeout)
{
    uint32_t start = get_tick();

    while (dma_state == DMA_BUSY) {
        if (get_tick() - start > timeout) {
            return -1;  // 超时
        }
    }

    return (dma_state == DMA_COMPLETE) ? 0 : -1;
}
```

---

## 6.6 中断嵌套注意事项

### 嵌套基本概念

```
中断嵌套示例:
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  主程序执行                                                     │
│       │                                                        │
│       ▼                                                        │
│  IRQ_A 触发 ─────────────────────────────────┐               │
│       │                                         │               │
│       ▼                                         │               │
│  ISR_A 执行                                    │               │
│       │                                         │               │
│       ▼                                         │               │
│  IRQ_B 触发 (更高优先级)                        │               │
│       │                                         │               │
│       ▼                                         │               │
│  ISR_B 执行 (嵌套执行)                          │               │
│       │                                         │               │
│       ▼                                         │               │
│  ISR_B 完成                                    │               │
│       │                                         │               │
│       ▼                                         │               │
│  ISR_A 继续执行                                 │               │
│       │                                         │               │
│       ▼                                         │               │
│  ISR_A 完成                                     │               │
│       │                                         │               │
│       ▼                                         ▼               │
│  返回主程序 ◄───────────────────────────────────               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 在SR6P3C4上配置嵌套

```c
// 中断嵌套配置 (谨慎使用!)
void enable_nested_interrupts(void)
{
    // 1. 在高优先级ISR中启用嵌套
    // 写入PRIMASK清除或设置BASEPRI
}

// 示例: 允许更高优先级中断嵌套
void __attribute__((interrupt("IRQ"))) high_prio_isr(void)
{
    // 清除PRIMASK允许嵌套
    __asm volatile ("cpsie i" : : : "memory");

    // 执行中断处理
    // ...
}
```

### 注意事项

```c
/*
 * 中断嵌套风险:
 *
 * 1. 栈溢出风险
 *    - 每层嵌套消耗栈空间
 *    - 需确保栈大小足够
 *
 * 2. 优先级设计
 *    - 高优先级ISR可能抢占低优先级ISR
 *    - 需仔细设计优先级
 *
 * 3. 资源共享
 *    - 嵌套的ISR间共享资源需保护
 *    - 使用互斥量或禁用中断
 *
 * 4. 推荐做法
 *    - 尽量避免嵌套
 *    - ISR保持简短
 *    - 复杂处理放到主循环
 */
```

---

## 本章小结

本章详细讲解了中断处理函数编写：

1. **ISR函数规范**
   - 使用IRQ_HANDLER宏声明
   - 正确的函数命名

2. **Prologue/Epilogue**
   - 编译器自动处理
   - 可手动保存/恢复

3. **最佳实践**
   - 快速执行
   - 清除中断标志
   - volatile变量使用

4. **实际示例**
   - UART中断驱动
   - DMA中断处理

5. **中断嵌套**
   - 嵌套原理
   - 风险与注意事项

---

## 下章预告

第七章将讲解多核中断路由：
- 多Cluster配置
- 跨集群中断
- 亲和性配置

# 第五章：中断配置实战

## 5.1 中断配置流程概述

在SR6P3C4上配置一个中断需要遵循以下标准流程。本章将通过具体示例详细讲解每一步。

### 配置流程图

```
┌─────────────────────────────────────────────────────────────────┐
│                      中断配置完整流程                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐                                                │
│  │ 1. 初始化   │  irq_init();                                  │
│  │   GIC       │  (在系统启动时调用一次)                        │
│  └──────┬──────┘                                                │
│         │                                                        │
│         ▼                                                        │
│  ┌─────────────┐                                                │
│  │ 2. 确定     │  查看irq_id.h                                  │
│  │   中断号    │  确定要使用的中断ID                            │
│  └──────┬──────┘                                                │
│         │                                                        │
│         ▼                                                        │
│  ┌─────────────┐                                                │
│  │ 3. 配置     │  irq_config(                                  │
│  │   中断参数 │      int_id,    // 中断号                      │
│  │            │      group,     // 分组                         │
│  │            │      signal,    // 触发类型                     │
│  │            │      priority   // 优先级                      │
│  │            │  );                                         │
│  └──────┬──────┘                                                │
│         │                                                        │
│         ▼                                                        │
│  ┌─────────────┐                                                │
│  │ 4. 编写     │  IRQ_HANDLER(handler_name)                    │
│  │   ISR函数   │  { ... }                                     │
│  └──────┬──────┘                                                │
│         │                                                        │
│         ▼                                                        │
│  ┌─────────────┐                                                │
│  │ 5. 使能     │  irq_enable(int_id);                          │
│  │   中断      │                                                │
│  └──────┬──────┘                                                │
│         │                                                        │
│         ▼                                                        │
│         ✓ 配置完成                                               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5.2 SPI中断配置示例 - UART接收中断

### 场景描述

配置UART0 (LINFLEX0)接收中断，当UART接收到数据时产生中断通知CPU。

### 第一步：包含必要头文件

```c
#include <irq.h>           // 中断API
#include <irq_id.h>        // 中断ID定义
#include <linflex.h>       // UART外设驱动
```

### 第二步：确定中断号

根据irq_id.h，UART0的中断号为：

```c
// IRQ_LINFLEX_0_INT_NUMBER = 102
// IRQ_LINFLEX_1_INT_NUMBER = 103
// ...
```

### 第三步：编写中断处理函数

```c
// 声明中断处理函数
// 函数名必须与irq_id.h中的映射一致
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    uint8_t data;

    // 读取接收数据
    data = LINFLEX0->DR.B.DATA8;

    // 处理接收数据
    // ... (用户代码)

    // 中断处理完成，写入EOIR通知GIC
    // 注意：某些外设需要在中断处理函数末尾清除中断标志
}
```

### 第四步：配置并使能中断

```c
void uart0_interrupt_init(void)
{
    /*====================================================
     * 步骤1: 初始化GIC (系统启动时已调用)
     * irq_init();
     *====================================================*/

    /*====================================================
     * 步骤2: 配置UART中断参数
     *====================================================*/

    // 参数说明:
    // - 中断号: IRQ_LINFLEX_0_INT_NUMBER (ID 102)
    // - 分组: IRQ_GROUP_NORMAL_IRQ (IRQ)
    // - 触发类型: IRQ_EDGE_TRIGGERED (边沿触发)
    //   注: UART通常使用边沿触发(字节接收完成产生一次边沿)
    // - 优先级: 16 (数值越低优先级越高, 范围0-31)
    irq_config(
        IRQ_LINFLEX_0_INT_NUMBER,     // 中断号
        IRQ_GROUP_NORMAL_IRQ,         // 分组: IRQ
        IRQ_EDGE_TRIGGERED,           // 触发: 边沿
        16                            // 优先级: 16
    );

    /*====================================================
     * 步骤3: 使能中断
     *====================================================*/
    irq_enable(IRQ_LINFLEX_0_INT_NUMBER);

    /*====================================================
     * 步骤4: 初始化UART硬件并使能接收中断
     *====================================================*/
    // 配置UART硬件 (省略)
    // LINFLEX0->CR.B.RFRIE = 1;  // 接收完成中断使能
}
```

### 完整示例代码

```c
#include <irq.h>
#include <irq_id.h>
#include <linflex.h>

// 全局变量: 接收数据缓冲
static uint8_t uart_rx_buffer[256];
static uint32_t rx_head = 0;
static uint32_t rx_tail = 0;

// UART中断处理函数
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    uint8_t data;

    // 检查接收标志
    if (LINFLEX0->SR.B.DRFRF == 1) {
        // 读取数据
        data = LINFLEX0->DR.B.DATA8;

        // 存入缓冲 (简单环形缓冲实现)
        uart_rx_buffer[rx_head] = data;
        rx_head = (rx_head + 1) % sizeof(uart_rx_buffer);
    }

    // 清除中断标志 (根据具体外设)
    LINFLEX0->SR.R = 0xFFFFFFFF;
}

// UART中断初始化
void uart0_interrupt_init(void)
{
    /* 配置中断 */
    irq_config(
        IRQ_LINFLEX_0_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        16
    );

    /* 使能中断 */
    irq_enable(IRQ_LINFLEX_0_INT_NUMBER);

    /* 配置UART硬件 */
    // 1. 复位UART
    LINFLEX0->CR.B.MDIS = 1;

    // 2. 配置波特率等 (省略)

    // 3. 使能接收中断
    LINFLEX0->CR.B.RFRIE = 1;

    // 4. 退出复位
    LINFLEX0->CR.B.MDIS = 0;
}

// 主函数
int main(void)
{
    // 初始化系统
    system_init();

    // 初始化GIC (如果系统没有自动初始化)
    irq_init();

    // 初始化UART中断
    uart0_interrupt_init();

    // 主循环
    while (1) {
        // 处理接收数据
        if (rx_head != rx_tail) {
            process_byte(uart_rx_buffer[rx_tail]);
            rx_tail = (rx_tail + 1) % sizeof(uart_rx_buffer);
        }
    }

    return 0;
}
```

---

## 5.3 GPIO外部中断配置示例

### 场景描述

配置SIUL2 GPIO中断，监听外部按键按下事件。

### GPIO中断号

```c
// SIUL2外部中断在irq_id.h中定义:
IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER,  // ID 62
IRQ_SIUL2_0_EXT_INT_1_INT_NUMBER,  // ID 63
IRQ_SIUL2_0_EXT_INT_2_INT_NUMBER,  // ID 64
IRQ_SIUL2_0_EXT_INT_3_INT_NUMBER,  // ID 65
```

### 配置示例

```c
#include <irq.h>
#include <irq_id.h>
#include <siul2.h>

// 按键状态
volatile uint8_t key_pressed = 0;

// 外部中断0处理函数 (对应PC0引脚)
IRQ_HANDLER(IRQ_SIUL2_0_EXT_INT_0_INT_HANDLER)
{
    // 清除中断标志
    SIUL2->ISR[0].R = (1 << 0);

    // 设置按键标志
    key_pressed = 1;
}

// GPIO中断初始化
void gpio_interrupt_init(void)
{
    /* 配置GPIO为输入 */
    SIUL2->PCR[0].R = 0x01000000;  // PC0, 上拉使能

    /* 配置外部中断 */
    irq_config(
        IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER,  // 中断号: 62
        IRQ_GROUP_NORMAL_IRQ,               // 分组: IRQ
        IRQ_EDGE_TRIGGERED,                 // 触发: 下降沿
        8                                   // 优先级: 8 (较高)
    );

    /* 使能中断 */
    irq_enable(IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER);
}
```

---

## 5.4 PPI中断配置示例 - 定时器中断

### 场景描述

配置Cortex-R52的物理定时器中断，实现周期性定时功能。

### 定时器中断号

```c
// 定时器相关PPI中断:
IRQ_PHYS_TIMER_INT_NUMBER,    // ID 30 - 物理定时器
IRQ_VIRT_TIMER_INT_NUMBER,    // ID 27 - 虚拟定时器
IRQ_HYPER_TIMER_INT_NUMBER,   // ID 26 - 超级管理器定时器
```

### 配置示例

```c
#include <irq.h>
#include <irq_id.h>
#include <core.h>  // 包含get_core_id()等函数

volatile uint32_t timer_tick = 0;

// 物理定时器中断处理
IRQ_HANDLER(IRQ_PHYS_TIMER_INT_HANDLER)
{
    timer_tick++;

    // 清除定时器中断标志 (需要读取CNTV_CVAL或写入空比较值)
    // 具体操作取决于定时器配置
}

// 定时器中断初始化
void timer_interrupt_init(void)
{
    uint32_t core_id;

    // 获取当前核心ID
    core_id = get_core_id();

    /* 配置PPI定时器中断 */
    // 注意: PPI是每个核心私有的，需要在对应核心上配置
    irq_config(
        IRQ_PHYS_TIMER_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        4    // 高优先级: 4
    );

    /* 使能中断 */
    irq_enable(IRQ_PHYS_TIMER_INT_NUMBER);

    /* 配置硬件定时器 */
    // 使能物理定时器
    // arm_write_sysreg(CP15_CNTCR, 1);
    // 设置比较值
    // arm_write_sysreg(CP15_CNTV_CVAL, ...);
}
```

---

## 5.5 SGI中断配置 - 核间通信

### 场景描述

配置SGI中断用于多核间通信，一个核心可以向其他核心发送中断信号。

### 配置示例

```c
#include <irq.h>
#include <irq_id.h>

// SGI中断号 (0-15可用于核间通信)
// 这里使用SGI0作为示例
#define SGI_TO_CORE0   IRQ_SGI_0_INT_NUMBER
#define SGI_TO_CORE1   IRQ_SGI_1_INT_NUMBER

// SGI中断处理函数
IRQ_HANDLER(IRQ_SGI_0_INT_HANDLER)
{
    // 收到来自其他核心的中断
    // 处理核间通信数据
    handle_ipi_message();
}

// SGI中断初始化 (在每个核心上调用)
void sgi_interrupt_init(void)
{
    // 配置SGI中断
    irq_config(
        SGI_TO_CORE0,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,   // SGI固定为边沿触发
        2                     // 最高优先级
    );

    // 使能中断
    irq_enable(SGI_TO_CORE0);
}

// 发送SGI到另一个核心
void send_sgi_to_core(irq_core_t target_core, irq_id_t sgi_num)
{
    // 使用irq_notify()发送跨核中断
    // 参数: cluster, core, int_id
    irq_notify(IRQ_CLUSTER_0, target_core, sgi_num);
}

// 使用示例
void trigger_core1(void)
{
    // 向Core 1发送SGI1中断
    send_sgi_to_core(IRQ_CORE_1, SGI_TO_CORE1);
}
```

---

## 5.6 中断优先级配置详解

### 优先级配置原则

```c
/*
 * 中断优先级配置建议:
 *
 * 高优先级 (0-7):
 *   - 实时关键外设 (CAN, 安全系统)
 *   - 物理定时器
 *   - 核间通信中断
 *
 * 中优先级 (8-23):
 *   - UART/SPI接收中断
 *   - DMA完成中断
 *   - GPIO外部中断
 *
 * 低优先级 (24-31):
 *   - 非紧急外设中断
 *   - 软件轮询类外设
 *
 * 注意:
 * - 优先级数值越小，优先级越高
 * - 同优先级按ID顺序处理
 * - 确保优先级分组(BPR)设置合理
 */
```

### 多优先级配置示例

```c
void configure_multiple_interrupts(void)
{
    // 高优先级: CAN接收
    irq_config(IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ,
               IRQ_EDGE_TRIGGERED,
               4);

    // 中优先级: UART0接收
    irq_config(IRQ_LINFLEX_0_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ,
               IRQ_EDGE_TRIGGERED,
               16);

    // 中优先级: SPI接收
    irq_config(IRQ_DSPI_6_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ,
               IRQ_EDGE_TRIGGERED,
               20);

    // 低优先级: GPIO
    irq_config(IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER,
               IRQ_GROUP_NORMAL_IRQ,
               IRQ_EDGE_TRIGGERED,
               28);
}
```

---

## 5.7 中断禁用与恢复

### 禁用单个中断

```c
void disable_uart_interrupt(void)
{
    // 禁用UART中断
    irq_disable(IRQ_LINFLEX_0_INT_NUMBER);
}

void enable_uart_interrupt(void)
{
    // 重新使能UART中断
    irq_enable(IRQ_LINFLEX_0_INT_NUMBER);
}
```

### 禁用/恢复全局中断

```c
// 保存/恢复中断状态的示例
void critical_section(void)
{
    uint32_t primask;

    // 禁用全局中断 (通过修改PRIMASK)
    // 注意: 这是一个简化的示例，实际应使用__disable_irq()等标准接口
    // primask = __get_PRIMASK();
    // __disable_irq();

    // 临界区代码
    // ...

    // 恢复全局中断
    // __set_PRIMASK(primask);
}
```

---

## 5.8 完整项目示例

```c
//============================================================
// 文件: main.c
// 描述: SR6P3C4中断配置完整示例
//============================================================

#include <irq.h>
#include <irq_id.h>
#include <system.h>

//------------------------------------------------------------
// 全局变量
//------------------------------------------------------------
volatile uint32_t uart_rx_count = 0;
volatile uint32_t timer_count = 0;
volatile uint8_t button_pressed = 0;

//------------------------------------------------------------
// 中断处理函数声明
//------------------------------------------------------------
IRQ_HANDLER(IRQ_LINFLEX_0_INT_HANDLER)
{
    uint8_t data;

    // 读取UART数据
    if (LINFLEX0->SR.B.DRFRF == 1) {
        data = LINFLEX0->DR.B.DATA8;
        uart_rx_count++;

        // 处理数据...
    }

    // 清除中断标志
    LINFLEX0->SR.R = 0xFFFFFFFF;
}

IRQ_HANDLER(IRQ_SIUL2_0_EXT_INT_0_INT_HANDLER)
{
    button_pressed = 1;
    SIUL2->ISR[0].R = (1 << 0);
}

//------------------------------------------------------------
// 外设初始化
//------------------------------------------------------------
void peripheral_init(void)
{
    // 1. UART0初始化
    LINFLEX0->CR.B.MDIS = 1;    // 复位
    // 配置波特率等...
    LINFLEX0->CR.B.RFRIE = 1;   // 接收中断使能
    LINFLEX0->CR.B.MDIS = 0;    // 退出复位

    // 2. GPIO初始化
    SIUL2->PCR[0].R = 0x01000000;   // PC0, 上拉
}

//------------------------------------------------------------
// 中断系统初始化
//------------------------------------------------------------
void interrupt_system_init(void)
{
    // 1. 初始化GIC (如果系统未自动初始化)
    irq_init();

    // 2. 配置UART0中断
    irq_config(
        IRQ_LINFLEX_0_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        16
    );
    irq_enable(IRQ_LINFLEX_0_INT_NUMBER);

    // 3. 配置GPIO外部中断
    irq_config(
        IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER,
        IRQ_GROUP_NORMAL_IRQ,
        IRQ_EDGE_TRIGGERED,
        24
    );
    irq_enable(IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER);
}

//------------------------------------------------------------
// 主函数
//------------------------------------------------------------
int main(void)
{
    // 系统初始化
    system_init();

    // 初始化外设
    peripheral_init();

    // 初始化中断系统
    interrupt_system_init();

    // 主循环
    while (1) {
        if (button_pressed) {
            button_pressed = 0;
            // 处理按键事件
        }
    }

    return 0;
}
```

---

## 本章小结

本章通过完整示例讲解了中断配置：

1. **UART中断配置**
   - 完整初始化流程
   - 中断处理函数
   - 外设配合

2. **GPIO中断配置**
   - 外部中断配置
   - 边沿触发

3. **PPI定时器中断**
   - 物理定时器配置
   - 多核注意事项

4. **SGI核间通信**
   - 跨核中断发送
   - 优先级配置建议

---

## 下章预告

第六章将深入讲解：
- ISR函数编写规范
- Prologue/Epilogue机制
- 中断上下文处理
- 实际外设驱动集成

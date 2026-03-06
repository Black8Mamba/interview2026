# 第八章：高级特性与调试

## 8.1 安全扩展与TrustZone

### GIC安全扩展概述

Cortex-R52支持GIC安全扩展，允许将中断分为安全和非安全两组，实现可信软件与普通软件的隔离：

```
┌─────────────────────────────────────────────────────────────────┐
│                    GIC安全扩展架构                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   安全世界 (Secure World)          非安全世界 (Normal World)    │
│   ┌─────────────────────┐        ┌─────────────────────────┐  │
│   │   Trusted Firmware   │        │   Normal OS / RTOS       │  │
│   │                     │        │                         │  │
│   │   - 安全驱动        │        │   - 应用代码            │  │
│   │   - 安全服务        │        │   - 外设驱动            │  │
│   └─────────────────────┘        └─────────────────────────┘  │
│              │                                │                 │
│              ▼                                ▼                 │
│   ┌─────────────────────────────────────────────────────┐      │
│   │                   GICv3                            │      │
│   │                                                     │      │
│   │  Group 0 (Secure)    ──────►  FIQ                  │      │
│   │  Group 1 (Non-Secure)──────►  IRQ                  │      │
│   │                                                     │      │
│   └─────────────────────────────────────────────────────┘      │
│              │                                              │
│              ▼                                              │
│         CPU Core (R52)                                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 安全中断配置

```c
// GIC安全扩展相关配置
void configure_secure_interrupts(void)
{
    // 在安全世界配置Group 0中断
    // 这些中断只能被安全世界处理

    // 1. 配置为Group 0 (Secure)
    // GICR_IGROUPR0 或 GICD_IGROUPR 设置对应位为0
    // Group 0 -> FIQ

    // 2. 在安全世界启用Group 0
    arm_write_sysreg(CP15_ICC_IGRPEN0, 1);

    // 3. 在非安全世界启用Group 1
    arm_write_sysreg(CP15_ICC_IGRPEN1, 1);
}

// 中断分组定义
typedef enum {
    IRQ_GROUP_SECURE = 0,      // Group 0 - FIQ (安全)
    IRQ_GROUP_NONSECURE = 1,   // Group 1 - IRQ (非安全)
} irq_security_group_t;
```

### TrustZone中断处理

```c
/*
 * TrustZone中断处理流程:
 *
 * 1. 安全中断触发 (Group 0)
 *    - FIQ向量跳转到安全固件
 *    - 安全固件处理中断
 *    - 可选: 路由到非安全世界
 *
 * 2. 非安全中断触发 (Group 1)
 *    - IRQ向量跳转到普通OS
 *    - 普通OS处理中断
 *
 * 3. 安全到非安全路由
 *    - 安全固件可以使用SGI触发非安全世界中断
 *    - 用于通知或信号量机制
 */
```

---

## 8.2 虚拟化支持 (可选)

### GICv3虚拟化特性

GICv3提供虚拟化支持，允许hypervisor创建虚拟中断：

```
┌─────────────────────────────────────────────────────────────────┐
│                    GIC虚拟化架构                                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐                                            │
│  │   Hypervisor    │                                            │
│  │   (裸机/VM)    │                                            │
│  └────────┬────────┘                                            │
│           │                                                      │
│     ┌─────┴─────┐                                              │
│     │           │                                              │
│  ┌──▼──┐    ┌──▼──┐                                           │
│  │ VM0 │    │ VM1 │                                           │
│  │  OS │    │  OS │                                           │
│  └──┬──┘    └──┬──┘                                           │
│     │           │                                              │
│     │  ┌────────▼────────┐                                     │
│     └──│   Virtual GIC   │◄──┘                                │
│        │  (虚拟中断)     │                                      │
│        └─────────────────┘                                     │
│                  │                                              │
│        ┌────────▼────────┐                                     │
│        │  Physical GIC   │                                      │
│        └─────────────────┘                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 虚拟中断配置 (示例)

```c
/*
 * SR6P3C4的虚拟化配置需要Hypervisor支持
 * 以下是概念性代码
 */

// 配置虚拟中断
void configure_virtual_interrupt(void)
{
    // 1. 启用虚拟化 (在Hypervisor中)
    // GICD_CTLR.DS = 1;

    // 2. 配置物理中断路由到Hypervisor
    // 使用GICD_IROUTER配置

    // 3. Hypervisor创建虚拟中断
    // 配置虚拟机接收的虚拟中断

    // 注: 实际配置依赖于Hypervisor实现
}
```

---

## 8.3 调试技巧与问题排查

### 常见中断问题

```c
/*
 *===========================================================
 *                    中断问题排查清单
 *===========================================================
 *
 * 问题1: 中断从未触发
 * ├─ 检查GIC初始化
 * │   - irq_init()是否已调用?
 * │   - ARE位是否已设置?
 * │   - Group 0/1是否已启用?
 * │
 * ├─ 检查中断配置
 * │   - 优先级是否合理?
 * │   - 触发类型是否正确?
 * │   - 目标核心是否正确?
 * │
 * ├─ 检查外设
 * │   - 外设中断是否已使能?
 * │   - 中断标志是否正确清除?
 * │
 * └─ 检查CPU
 *     - PRIMASK/FAULTMASK是否阻塞?
 *     - 中断是否被禁用?
 *
 * 问题2: 中断响应延迟高
 * ├─ 优先级配置问题
 * │   - 高优先级中断是否过多?
 * │   - 同优先级中断处理时间?
 * │
 * ├─ ISR执行时间
 * │   - ISR是否过于复杂?
 * │   - 是否阻塞?
 * │
 * └─ 系统负载
 *     - 其他高优先级任务?
 *     - 中断嵌套?
 *
 * 问题3: 中断重复触发
 * ├─ 中断标志未清除
 * │   - 清除顺序是否正确?
 * │   - 是否在读取数据前清除?
 * │
 * └─ 电平触发问题
 *     - 外设信号是否保持?
 *
 * 问题4: 多核中断问题
 * ├─ 目标核心错误
 * │   - GICD_IROUTER配置?
 * │   - 核心ID正确?
 * │
 * └─ 跨Cluster失败
 *     - IBCM是否初始化?
 *     - SGI号是否在允许范围?
 *
 */
```

### 中断状态检查函数

```c
#include <irq.h>
#include <irq_id.h>

// 调试: 打印中断状态
void dump_interrupt_status(irq_id_t int_id)
{
    uint32_t status = 0;
    uint32_t pending = 0;
    uint32_t active = 0;

    if (int_id < 32) {
        // SGI/PPI
        // GICR_ISENABLER
        status = GICR_SGI_PPI_CPU0.GICR_ISENABLER0.R;
        // GICR_ISPENDR
        pending = GICR_SGI_PPI_CPU0.GICR_ISPENDR0.R;
        // GICR_ISACTIVER
        active = GICR_SGI_PPI_CPU0.GICR_ISACTIVER0.R;
    } else {
        // SPI
        status = GICD.GICD_ISENABLER[(uint32_t)int_id >> 5UL].R;
        pending = GICD.GICD_ISPENDR[(uint32_t)int_id >> 5UL].R;
        active = GICD.GICD_ISACTIVER[(uint32_t)int_id >> 5UL].R;
    }

    printf("Interrupt %d Status:\n", int_id);
    printf("  Enabled:  %d\n", (status >> (int_id & 0x1F)) & 1);
    printf("  Pending:  %d\n", (pending >> (int_id & 0x1F)) & 1);
    printf("  Active:   %d\n", (active >> (int_id & 0x1F)) & 1);
}

// 调试: 检查GIC整体状态
void dump_gic_status(void)
{
    uint32_t gicd_ctlr;
    uint32_t icc_ctlr;

    // 读取GICD状态
    gicd_ctlr = GICD.GICD_CTLR.R;
    printf("GICD_CTLR: 0x%08X\n", gicd_ctlr);
    printf("  ARE_S:    %d\n", (gicd_ctlr >> 0) & 1);
    printf("  EnableGrp0: %d\n", (gicd_ctlr >> 2) & 1);
    printf("  EnableGrp1: %d\n", (gicd_ctlr >> 3) & 1);

    // 读取CPU Interface状态
    // 通过系统寄存器读取
    printf("\nNote: CPU Interface registers need system register access\n");
}
```

### 中断延迟测量

```c
#include <irq.h>
#include <irq_id.h>
#include <timer.h>

volatile uint32_t int_trig_time = 0;
volatile uint32_t int_start_time = 0;
volatile uint32_t int_latency = 0;

// 在外设ISR中测量延迟
IRQ_HANDLER(IRQ_TIMER_0_INT_HANDLER)
{
    // 记录ISR开始时间
    int_start_time = timer_get_count();

    // 计算延迟
    int_latency = int_start_time - int_trig_time;

    // 清除中断标志
    clear_timer_interrupt();

    // 打印延迟
    printf("Interrupt latency: %d cycles\n", int_latency);
}

// 模拟外设触发
void trigger_interrupt(void)
{
    // 记录触发时间
    int_trig_time = timer_get_count();

    // 触发外设中断
    // set_timer_compare(...);
}
```

---

## 8.4 性能优化

### ISR性能最佳实践

```c
/*
 * ISR性能优化技巧:
 *
 * 1. 最小化ISR执行时间
 *    - 只做必需的处理
 *    - 数据转移到主循环处理
 *
 * 2. 使用缓冲减少中断
 *    - 使用DMA或环形缓冲
 *    - 批量处理数据
 *
 * 3. 合理的中断优先级
 *    - 高优先级给实时关键中断
 *    - 避免优先级反转
 *
 * 4. 减少中断嵌套
 *    - 除非必要，否则禁用嵌套
 *    - 嵌套会显著增加延迟
 *
 * 5. 使用缓存优化
 *    - 代码在SRAM中
 *    - 数据对齐
 *
 * 6. 中断负载均衡
 *    - 将中断分散到多核
 *    - 避免单核瓶颈
 */

// 优化示例: 最小化ISR
IRQ_HANDLER(IRQ_UART_INT_HANDLER)
{
    // ✓ 简单处理: 只读取数据
    while (UART->SR.B.DRFRF) {
        ring_buffer_put(&rx_buf, UART->DR.B.DATA8);
    }

    // 清除标志
    UART->SR.R = 0xFFFFFFFF;

    // ✓ 不做: 数据处理、打印等
    // process_data()  // 放到主循环
}

// 主循环中处理
void main_loop(void)
{
    // 处理接收数据
    while ((count = ring_buffer_get(&rx_buf, &data)) > 0) {
        process_byte(data);
    }
}
```

### 中断负载计算

```c
// 估算中断负载
void calculate_interrupt_load(void)
{
    uint32_t uart_rx_per_sec = 115200 / 10;  // 115200波特率, 10位/字节
    uint32_t uart_isr_cycles = 50;            // ISR执行周期数
    uint32_t timer_isr_per_sec = 1000;         // 1ms定时器
    uint32_t timer_isr_cycles = 30;

    uint32_t total_cycles = (uart_rx_per_sec * uart_isr_cycles) +
                            (timer_isr_per_sec * timer_isr_cycles);

    uint32_t cpu_mhz = 200;  // CPU频率 200MHz
    uint32_t cpu_cycles_per_sec = cpu_mhz * 1000000;
    uint32_t load_percent = (total_cycles * 100) / cpu_cycles_per_sec;

    printf("Interrupt load: %d.%02d%%\n",
           load_percent / 100, load_percent % 100);
}
```

---

## 8.5 常见问题FAQ

### Q1: 如何禁用所有中断?

```c
void disable_all_interrupts(void)
{
    // 方法1: 使用PRIMASK
    __asm volatile ("cpsid i" : : : "memory");

    // 方法2: 设置PRIMASK寄存器
    // __set_PRIMASK(1);
}

void enable_all_interrupts(void)
{
    __asm volatile ("cpsie i" : : : "memory");
    // __set_PRIMASK(0);
}
```

### Q2: 如何实现中断共享?

```c
/*
 * GIC不支持真正的中断共享
 * 但可以通过以下方式实现:
 *
 * 1. 使用同一个高优先级中断
 * 2. 在ISR中轮询多个外设标志
 */

IRQ_HANDLER(IRQ_SHARED_INT_HANDLER)
{
    // 检查外设A
    if (PERIPH_A->ISR.B.INTF) {
        handle_periph_a();
        PERIPH_A->ISR.B.INTF = 1;
    }

    // 检查外设B
    if (PERIPH_B->ISR.B.INTF) {
        handle_periph_b();
        PERIPH_B->ISR.B.INTF = 1;
    }

    // 检查外设C
    if (PERIPH_C->ISR.B.INTF) {
        handle_periph_c();
        PERIPH_C->ISR.B.INTF = 1;
    }
}
```

### Q3: 中断可以递归吗?

```c
/*
 * ARM Cortex-R不支持硬件中断递归
 * 但可以通过软件模拟:
 *
 * 1. 在ISR中检测到需要递归处理时
 * 2. 设置一个标志
 * 3. 退出ISR前手动触发自己
 * 4. 再次进入ISR
 *
 * 警告: 这种方式风险较高，容易导致栈溢出
 */
```

### Q4: 如何选择中断优先级?

```c
/*
 * 优先级选择指南:
 *
 * 0-7:   最高优先级
 *        - 安全关键中断
 *        - 硬件故障检测
 *        - 实时定时器
 *
 * 8-15:  高优先级
 *        - CAN/LIN等通信
 *        - 高速数据采集
 *
 * 16-23: 中优先级
 *        - UART/SPI接收
 *        - GPIO中断
 *
 * 24-31: 低优先级
 *        - 轮询类外设
 *        - 非紧急通知
 *
 * 原则:
 * - 数值越小优先级越高
 * - 同优先级按ID顺序
 * - 避免过多高优先级
 */
```

---

## 本章小结

本章介绍了高级特性和调试：

1. **安全扩展**
   - TrustZone架构
   - Group 0/1配置

2. **虚拟化**
   - GICv3虚拟化概念
   - 虚拟中断配置

3. **调试技巧**
   - 状态检查函数
   - 延迟测量
   - 问题排查清单

4. **性能优化**
   - ISR最佳实践
   - 负载计算
   - 常见FAQ

---

## 附录

### 中断号速查表

```c
//===========================================================
//                  常用中断号速查
//===========================================================

// SGI (0-15)
IRQ_SGI_0_INT_NUMBER ~ IRQ_SGI_15_INT_NUMBER

// PPI (16-31)
IRQ_SWT_0_INT_NUMBER              // 16: 看门狗0
IRQ_SWT_1_INT_NUMBER              // 17: 看门狗1
IRQ_ME_INT_NUMBER                 // 18: 模式管理
IRQ_PHYS_TIMER_INT_NUMBER         // 30: 物理定时器

// GPIO (SIUL2)
IRQ_SIUL2_0_EXT_INT_0_INT_NUMBER  // 62
IRQ_SIUL2_0_EXT_INT_1_INT_NUMBER  // 63
IRQ_SIUL2_0_EXT_INT_2_INT_NUMBER  // 64
IRQ_SIUL2_0_EXT_INT_3_INT_NUMBER  // 65

// 以太网
IRQ_ETHERNET_0_CORE_INT_NUMBER    // 87

// UART (Linflex)
IRQ_LINFLEX_0_INT_NUMBER           // 102
IRQ_LINFLEX_1_INT_NUMBER           // 103

// CAN
IRQ_CAN_SUB_0_M_TTCAN_0_LINE_0_INT_NUMBER  // 172

// SPI
IRQ_DSPI_6_INT_NUMBER             // 136
IRQ_DSPI_7_INT_NUMBER             // 137

// I2C
IRQ_I2C_0_INT_NUMBER              // 140
IRQ_I2C_1_INT_NUMBER              // 141

// DMA
IRQ_EDMA_0_FLAGS_0_7_INT_NUMBER   // 36
```

---

## 文档结束

本教程涵盖了Stellar SR6P3C4 GIC中断控制器的全部内容：

1. **概念基础** - GIC版本、中断类型、优先级
2. **硬件架构** - GICD、GICR、CPU Interface
3. **源码解析** - SDK驱动逐行详解
4. **实战配置** - 完整配置示例
5. **ISR编写** - 规范与最佳实践
6. **多核路由** - 跨Cluster配置
7. **高级特性** - 安全、虚拟化、调试

如有问题，欢迎进一步讨论!

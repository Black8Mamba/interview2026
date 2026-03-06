# 第 6 章：EL2 异常向量与启动流程

## 6.1 EL2 向量表结构

### 6.1.1 向量表概述

EL2 向量表定义了各类异常的入口点。在 ARMv8-R 架构中，每个异常类型都有对应的向量入口。

### 6.1.2 Stellar SDK 向量表

在 Stellar SDK 中，向量表定义在 `el2_vectors.S` 文件中：

```assembly
// parts/virt/st_hypervisor/arch/src/el2_vectors.S

.section    .vectors, "ax"

.globl      _vectors
_vectors:
    .long       vector0,    vector1,    vector2,    vector3
    .long       vector4,    vector5,    vector6,    vector7
    // ... 更多向量
```

### 6.1.3 向量数量

Stellar SDK 的向量表支持最多 544 个中断向量（vector0 ~ vector543），这与 GICv2 支持的中断数量对应。

### 6.1.4 默认向量处理

每个向量默认实现为简单的返回：

```assembly
.type vector0, _ASM_FUNCTION_
vector0:
    bx          lr      // 直接返回
```

对于未处理的中断，定义了专门的死循环：

```assembly
.type irq_unhandled, _ASM_FUNCTION_
_unhandled_irq:
    b       _unhandled_irq     // 死循环
```

## 6.2 启动代码分析

### 6.2.1 复位向量

Cortex-R52 复位后首先执行复位向量，然后跳转到 Hypervisor 初始化代码。

### 6.2.2 Hypervisor 初始化入口

在 `hyper.h` 中定义了初始化函数：

```c
// parts/virt/st_hypervisor/arch/include/hyper.h

/**
 * @brief  Initialize Hypervisor.
 * @el2
 */
uint32_t hyper_init(void);
```

### 6.2.3 初始化流程

Hypervisor 初始化主要完成以下任务：

```c
uint32_t hyper_init(void)
{
    // 1. 设置 EL2 异常向量基址
    set_vbar_el2(el2_vectors);

    // 2. 配置 MPU
    hyp_mpu_init();

    // 3. 初始化 GIC
    gic_init();

    // 4. 初始化调度器
    sched_init();

    // 5. 返回第一个要运行的 VM ID
    return 0;
}
```

## 6.3 初始化流程详解

### 6.3.1 异常向量设置

首先需要设置 EL2 的向量基址寄存器（VBAR_EL2）：

```c
void set_vbar_el2(void *vector_base)
{
    // 写入 VBAR_EL2 寄存器
    asm volatile("msr vbar_el2, %0" : : "r"(vector_base));
    asm volatile("isb");  // 指令同步屏障
}
```

### 6.3.2 MPU 配置

Hypervisor 需要配置 MPU 来实现内存保护：

```c
void hyp_mpu_init(void)
{
    // 1. 禁用 MPU
    mpu_disable();

    // 2. 配置 Hypervisor 区域
    mpu_configure_region(HYP_REGION,
                         HYP_BASE,
                         HYP_SIZE,
                         HYP_ATTR);

    // 3. 配置共享内存区域
    mpu_configure_region(SHARED_REGION,
                         SHARED_BASE,
                         SHARED_SIZE,
                         SHARED_ATTR);

    // 4. 使能 MPU
    mpu_enable();
}
```

### 6.3.3 GIC 初始化

中断控制器初始化：

```c
void gic_init(void)
{
    // 1. 禁用 GIC
    gic_disable();

    // 2. 配置分发器
    gic_dist_init();

    // 3. 配置 CPU 接口
    gic_cpu_init();

    // 4. 设置中断优先级掩码
    gic_set_priority_mask(0xFF);

    // 5. 使能 GIC
    gic_enable();
}
```

### 6.3.4 调度器初始化

调度器初始化：

```c
void sched_init(void)
{
    // 1. 初始化 VM 状态表
    sched_vm_table_init();

    // 2. 配置时间片
    sched_set_time_slice(SCHEDULER_TIMEOUT);

    // 3. 启动调度器
    sched_start();
}
```

## 6.4 多核启动

### 6.4.1 多核架构

Stellar SR6P3c4 支持多核运行，每个核心可以独立运行不同的 VM。

### 6.4.2 启动流程

```
主核心 (Core 0)
    ↓
加载 Hypervisor
    ↓
初始化 EL2 环境
    ↓
启动调度器
    ↓
选择 VM0 运行

从核心 (Core 1, 2, ...)
    ↓
等待主核心初始化完成
    ↓
从调度器获取 VM 配置
    ↓
跳转到对应 VM
```

### 6.4.3 核心间同步

多核启动时需要同步：

```c
// 主核心等待所有核心就绪
void secondary_core_init(void)
{
    // 1. 设置本核心的向量表
    set_vbar_el2(el2_vectors);

    // 2. 配置本核心的 MPU
    hyp_mpu_init();

    // 3. 通知主核心已就绪
    spin_lock(&core_sync);
    core_ready[current_core_id] = 1;
    spin_unlock(&core_sync);
}
```

## 6.5 异常处理入口

### 6.5.1 EL2 异常入口

当异常发生在 EL2 时，会跳转到异常处理入口：

```assembly
// parts/virt/st_hypervisor/arch/src/hyp_exceptions.S

.global el2_hyp_entry_exception
.type el2_hyp_entry_exception, _ASM_FUNCTION_
el2_hyp_entry_exception:
    // 保存寄存器
    push {r4-r12, lr}

    // 调用 C 语言异常处理函数
    ldr r4, =el2_hypervisor_handler
    orr r4, r4, 0x1      // Thumb 模式
    blx r4

    // 恢复寄存器
    pop {r4-r12, lr}
    eret                  // 返回
```

### 6.5.2 异常处理流程

```
异常发生
    ↓
处理器保存状态到 SPSR_EL2
    ↓
保存返回地址到 ELR_EL2
    ↓
跳转到向量入口
    ↓
el2_hyp_entry_exception
    ↓
el2_hypervisor_handler (C 代码)
    ↓
根据 HSR 判断异常类型
    ↓
调用对应服务处理
    ↓
ERET 返回
```

## 6.6 本章小结

本章详细介绍了 EL2 异常向量与启动流程：

1. **向量表结构**：支持 544 个中断向量
2. **启动代码**：hyper_init() 函数的初始化任务
3. **初始化流程**：VBAR 设置 → MPU 配置 → GIC 初始化 → 调度器启动
4. **多核启动**：多核心协同启动机制
5. **异常入口**：异常处理的汇编和 C 代码流程

下一章将详细介绍 HVC（Hypervisor Call）机制。

---

*参考资料：*
- `parts/virt/st_hypervisor/arch/src/el2_vectors.S`
- `parts/virt/st_hypervisor/arch/src/hyp_exceptions.S`
- `parts/virt/st_hypervisor/arch/include/hyper.h`

# 嵌入式MCU面试知识点（详细版）

> 适用于高级嵌入式工程师岗位（智能硬件大厂）
> 本文档针对MCU/MCU+RTOS方向，涵盖Cortex-M/R架构、外设、调试、业务及传感器等核心技术点

---

## 目录

1. [ARM Cortex系列概述](#1-arm-cortex系列概述)
2. [Cortex-M架构知识](#2-cortex-m架构知识)
3. [Cortex-R架构知识](#3-cortex-r架构知识)
4. [Cortex-A架构知识](#4-cortex-a架构知识)
5. [常用外设](#5-常用外设)
6. [DMA常用应用](#6-dma常用应用)
7. [缓存相关知识](#7-缓存相关知识)
8. [调试技巧](#8-调试技巧)
9. [业务知识](#9-业务知识)
10. [传感器接口](#10-传感器接口)

---

## 1. ARM Cortex系列概述

### 1.1 三大系列定位与市场背景

ARM（Advanced RISC Machines）公司在2004年推出Cortex系列处理器，取代了之前的ARM9/10/11系列。Cortex系列分为三个明确的产品线，分别针对不同的市场细分：

#### Cortex-M系列：微控制器市场

Cortex-M系列是ARM历史上最成功的处理器系列之一，专为成本敏感型MCU应用设计。从2004年推出Cortex-M3以来，该系列已成为全球MCU市场的主流架构。

**设计目标**：
- 极低的功耗（μW级静态功耗）
- 最小化的硅面积（<0.5mm² 核心面积）
- 易于编程和调试（单线调试SWD）
- 确定性的中断响应（无需缓存，零等待状态）

**市场地位**：
- STM32（NXP、TI、Microchip等）基于Cortex-M
- 超过50%的32位MCU市场份额
- IoT、可穿戴、智能家居的主力架构

#### Cortex-R系列：实时嵌入式市场

Cortex-R系列面向对可靠性和实时性有严苛要求的嵌入式应用，是工业和汽车安全关键系统的主流选择。

**设计目标**：
- 确定性的中断响应（亚微秒级）
- 高可靠性（内存ECC、错误检测）
- 实时操作系统支持
- 功能安全认证

**应用领域**：
- 汽车ECU（发动机控制、安全气囊、刹车系统）
- 工业控制（PLC、运动控制器）
- 医疗设备（起搏器、监护仪）
- 存储控制（SSD控制器）

#### Cortex-A系列：应用处理器市场

Cortex-A系列面向高性能计算应用，支持运行完整操作系统（Linux、Android）。

**设计目标**：
- 高性能（GHz级主频）
- 丰富的指令集（A64/A32/T32）
- 内存管理单元（MMU）支持虚拟内存
- 硬件虚拟化支持

**典型应用**：
- 智能手机/平板
- 智能车载系统（IVI）
- 边缘计算网关
- 智能摄像头

### 1.2 各系列详细对比

| 特性维度 | Cortex-M | Cortex-R | Cortex-A |
|----------|----------|----------|----------|
| **架构版本** | ARMv6-M/7-M/8-M | ARMv7-R/8-R | ARMv7-A/8-A/9-A |
| **指令集** | Thumb-2 | Thumb-2 + ARM | A64/A32/T32 |
| **流水线深度** | 3级 | 3-6级 | 8-15级 |
| **主频范围** | <300MHz | <600MHz | >2GHz |
| **MMU** | 可选(仅M7/M33) | 必须 | 必须 |
| **缓存** | 可选(I/D-Cache) | 必须(L1+L2) | 必须(L1+L2+L3) |
| **FPU** | 可选 | 可选 | 可选 |
| **DSP指令** | M4/M7/M33支持 | 支持 | 支持 |
| **TrustZone** | M33/M23支持 | 无 | 支持 |
| **中断延迟** | <12周期 | <20周期 | 变化较大 |
| **功耗** | μW-mW级 | mW级 | 数百mW-数W |
| **典型制程** | 40nm+ | 40nm+ | 7nm-28nm |
| **典型芯片** | STM32F4/L4/G4 | Ti C6678 | 高通/联发科/海思 |
| **Linux支持** | 不支持 | 不直接支持 | 完全支持 |
| **RTOS支持** | 完美支持 | 支持 | 支持 |
| **安全认证** | IEC 61508 | ISO 26262/DO-178C | 多样化 |

### 1.3 Cortex-M各型号深度对比

| 型号 | 架构 | 流水线 | 特色功能 | 典型应用 |
|------|------|--------|----------|----------|
| **Cortex-M0** | ARMv6-M | 3级 | 最小面积 | 超低成本MCU |
| **Cortex-M0+** | ARMv6-M | 3级 | 优化功耗 | IoT节点 |
| **Cortex-M3** | ARMv7-M | 3级 | 高性能 | 通用MCU |
| **Cortex-M4** | ARMv7E-M | 3级 | DSP+FPU | 电机控制 |
| **Cortex-M7** | ARMv7E-M | 6级双发 | 最高性能+缓存 | 高级消费电子 |
| **Cortex-M23** | ARMv8-M Baseline | 2级 | 轻量级安全 | 简单安全设备 |
| **Cortex-M33** | ARMv8-M Mainline | 4级 | 完整安全+DSP+FPU | 安全IoT |
| **Cortex-M55** | ARMv8.1-M | 8级 | Helium(DSP SIMD) | AI边缘MCU |

### 1.4 Cortex-R各型号深度对比

| 型号 | 架构 | 核数 | 特色功能 | 典型应用 |
|------|------|------|----------|----------|
| **Cortex-R4** | ARMv7-R | 单核 | 双标量流水线 | 工业/存储 |
| **Cortex-R5** | ARMv7-R | 单核/双核 | 保守设计 | 汽车ADAS |
| **Cortex-R7** | ARMv7-R | 双核/四核 | Out-of-order | 汽车/无线基带 |
| **Cortex-R8** | ARMv7-R | 四核 | 增强L2 | 调制解调器 |
| **Cortex-R52** | ARMv8-R | 多核 | 隔离技术 | 汽车安全 |

### 1.5 异构系统架构

现代嵌入式系统越来越多采用多核异构架构，充分发挥不同核心的优势：

#### 典型架构模式

**MCU + AP架构**：
```
┌─────────────┐     ┌─────────────┐
│  Cortex-M  │     │  Cortex-A  │
│  (实时)    │ ←→  │  (应用)    │
└─────────────┘     └─────────────┘
        │                   │
        └────── 共享内存 ────┘
               (RPMsg)
```

- Cortex-M负责实时控制、物理接口、传感器采集
- Cortex-A负责复杂算法、显示、网络、用户交互
- 通过共享内存+消息队列通信

**典型芯片**：
- NXP i.MX RT系列（Cortex-M7）
- ST STM32MP系列（Cortex-A7 + Cortex-M4）
- 瑞萨R-Car系列

---

## 2. Cortex-M架构知识

### 2.1 Cortex-M3深度解析

Cortex-M3是ARM公司2005年推出的首款Cortex-M处理器，至今仍是嵌入式MCU的主流选择之一。

#### 核心架构

**流水线**：
- 3级流水线：取指(Fetch) → 译码(Decode) → 执行(Execute)
- 分支预测：动态分支预测，减少流水线停顿
- 零等待状态：Flash accelerator配合

**指令集**：
- Thumb-2：16/32位混合指令集
- 性能达到纯ARM指令的95%
- 代码密度比纯32位指令高26%

**重要特性**：
```
┌─────────────────────────────────────────────────────┐
│                   Cortex-M3                         │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐   ┌─────────────┐                  │
│  │ 3级流水线   │   │  乘法器     │ 32位×32位 = 32位 │
│  └─────────────┘   └─────────────┘  1-12周期      │
│  ┌─────────────┐   ┌─────────────┐                  │
│  │硬件除法器   │   │  Bit-banding │ 内存位操作     │
│  └─────────────┘   └─────────────┘                  │
│  ┌─────────────┐   ┌─────────────┐                  │
│  │ 嵌套向量中断 │   │ 睡眼模式    │                 │
│  └─────────────┘   └─────────────┘                  │
└─────────────────────────────────────────────────────┘
```

#### 寄存器组详细说明

**通用寄存器**：
- R0-R7：低寄存器，所有Thumb指令可访问
- R8-R12：高寄存器，某些Thumb指令限制

**特殊寄存器**：

| 寄存器 | 功能 | 特别说明 |
|--------|------|----------|
| **R13 (SP)** | 堆栈指针 | MSP（主堆栈）/PSP（进程堆栈） |
| **R14 (LR)** | 链接寄存器 | 函数返回地址 |
| **R15 (PC)** | 程序计数器 | 指向下一条指令 |
| **xPSR** | 程序状态 | 包含APSR/IPSR/EPSR |

**CONTROL寄存器**（重要）：
```
bit[1] 堆栈选择   0=MSP, 1=PSP
bit[0] 特权级    0=特权级(Supervisor), 1=用户级(User)
```

#### 异常处理模型详解

**向量表结构**（前16个异常）：
```
地址     | 异常号  |  说明
----------|---------|------------------
0x0000   | -       | MSP初始值
0x0004   | 1       | Reset
0x0008   | 2       | NMI
0x000C   | 3       | HardFault
0x0010   | 4       | MemManage
0x0014   | 5       | BusFault
0x0018   | 6       | UsageFault
0x001C   | 7-10    | 保留
0x0034   | 11      | SVCall
0x0038   | 12      | Debug Monitor
0x003C   | 13      | 保留
0x0040   | 14      | PendSV
0x0044   | 15      | SysTick
0x0048+  | 16+     | 外部中断IRQ0-IRQn
```

**异常优先级**：
- 可配置优先级：8位（实际实现可能更少，如4-8位）
- 编号越小优先级越高
- 优先级分组：抢占能力和分组子优先级

**异常响应流程**：
```
1. 异常发生
   ↓
2. CPU将当前状态压栈（xPSR, PC, LR, R12, R3-R0）
   ↓
3. 读取向量表，获取处理函数地址
   ↓
4. 跳转到处理函数
   ↓
5. 执行处理
   ↓
6. EXC_RETURN返回
   ↓
7. 栈中状态恢复
   ↓
8. 继续执行
```

**关键点**：
- 自动保存/恢复上下文
- 零周期中断延迟（中断响应在周期级别）
- 尾链优化（Tail-chaining）：连续中断无需恢复

#### NVIC中断控制器详解

**功能特性**：
- 最多支持256个外部中断（具体芯片实现）
- 1-256个优先级（实际实现如8/16级）
- 向量式中断
- 支持中断嵌套
- 支持中断优先级分组

**编程接口**：

```c
// 使能指定中断
void NVIC_EnableIRQ(IRQn_Type IRQn);

// 禁用指定中断
void NVIC_DisableIRQ(IRQn_Type IRQn);

// 设置中断挂起
void NVIC_SetPendingIRQ(IRQn_Type IRQn);

// 清除中断挂起
void NVIC_ClearPendingIRQ(IRQn_Type IRQn);

// 设置中断优先级 (0=最高)
void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority);

// 设置优先级分组
// 参数: 0-7
// 含义: 分成(preempt-priority-group) + (sub-priority-group)
void NVIC_SetPriorityGrouping(uint32_t PriorityGroup);

// 进入/退出临界区
__disable_irq();  // 关闭全局中断
__enable_irq();   // 打开全局中断
```

**优先级分组示例**：
```
PriorityGroup = 0b010 (2)
[7:1] 抢占优先级 [0] 子优先级

PriorityGroup = 0b101 (5)
[7:3] 抢占优先级 [2:0] 子优先级
```

### 2.2 Cortex-M4深度解析

Cortex-M4在M3基础上增加了DSP和FPU，是电机控制、数字信号处理的主流选择。

#### 与M3的对比

| 特性 | Cortex-M3 | Cortex-M4 |
|------|-----------|-----------|
| 架构 | ARMv7-M | ARMv7E-M |
| DSP指令 | 无 | 有 |
| FPU | 无 | 单精度 |
| 乘法 | 32×32=32 | 32×32+64 |
| 饱和指令 | 无 | 有 |
| SIMD | 无 | 有 |

#### DSP指令集详解

**单周期乘加指令**：
```c
// 32位乘法
__asm uint32_t mul32(uint32_t a, uint32_t b) {
    MUL r0, r0, r1
    BX lr
}

// 32位乘加 (结果64位)
// SMMLA - 有符号乘加，结果32位（饱和）
// SMLAD - 有符号乘加，双16位
```

**SIMD指令**：
```c
// 16位并行加法
// SADD16 R0, R1, R2  ->  R0[31:16]=R1[31:16]+R2[31:16]
//                      R0[15:0]=R1[15:0]+R2[15:0]

// 8位并行操作
// ADD8 R0, R1, R2     // 四个8位同时加
```

**饱和指令**：
```c
// 有符号饱和
__asm int32_t sat_q15(int32_t val) {
    SSAT r0, #16, r0  // 饱和到[-32768, 32767]
    BX lr
}

// 无符号饱和
__asm uint32_t sat_u16(uint32_t val) {
    USAT r0, #16, r0  // 饱和到[0, 65535]
    BX lr
}
```

#### FPU浮点单元详解

**寄存器**：
- S0-S31：32位单精度寄存器
- D0-D31：64位双精度（映射S0-S31）
- FPSCR：浮点状态控制寄存器

**使用示例**：
```c
// 开启FPU
void EnableFPU(void) {
    __set_FPSCR(__get_FPSCR() & ~0x00060000);  // 清零Mode位
    __ASM volatile("isb");
    __ASM volatile("dsb");
}

// 配置SCB
SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
```

**Lazy Stacking**：
- 中断发生时，FPU寄存器延迟保存
- 减少中断延迟
- 只有使用FPU的任务才保存上下文

### 2.3 Cortex-M7深度解析

Cortex-M7是Cortex-M系列的旗舰产品，提供最高性能。

#### 架构特点

**6级双发射流水线**：
```
取指1 → 取指2 → 译码 → 执行1 → 执行2 → 写回
    ↓                                    ↓
    └────────── 分支预测 ────────────────┘
```

**关键特性**：
- 双发射：同时执行两条指令
- 指令/数据缓存：可选16KB-64KB
- TCM接口：紧耦合内存，零等待
- ETM跟踪：完整指令跟踪

#### 缓存配置

**典型配置**：
```
I-Cache: 4-64KB (可配置)
D-Cache: 4-64KB (可配置)
TCM:    0-512KB (代码/数据分离)
```

**缓存策略**：
```c
// 使能缓存
void EnableICache(void) {
    SCB_EnableICache();
}

void EnableDCache(void) {
    SCB_EnableDCache();
}

// 禁用缓存
void DisableDCache(void) {
    SCB_DisableDCache();
}
```

#### TCM接口

**用途**：
- 关键代码/数据加速
- 实时要求高的场景
- DMA缓冲区

**配置**：
```c
// 在链接脚本中分配到TCM区域
MEMORY
{
    ITCM (xrw)  : ORIGIN = 0x00000000, LENGTH = 128K
    DTCM (xrw)  : 0x20000000, LENGTH = 128K
}
```

### 2.4 Cortex-M33深度解析

Cortex-M33是ARMv8-M架构的首批产品，支持TrustZone安全扩展。

#### TrustZone安全架构

**设计目标**：
- 安全世界（Secure）和非安全世界（Non-secure）隔离
- 保护关键资产（密钥、固件）
- 最小化安全攻击面

**状态切换**：
```
Secure World                    Non-Secure World
    │                                  │
    │←───── SG指令/安全调用 ──────────→│
    │                                  │
    │                                  │
```

**编程模型**：
```c
// 安全属性配置
void TZ_SecureInit(void) {
    // 配置SAU
    SAU->CTRL = 0;
    SAU->RNR  = 0;
    SAU->RBAR = 0x20010000;  // 安全内存起始
    SAU->RLAR = 0x2001FFFF;  // 安全内存结束
    SAU->CTRL = 1;           // 使能SAU

    // 配置非安全调用
    SCB->NSACR |= (1 << 10); // 允许非安全访问FPU
}
```

### 2.5 MPU内存保护单元

#### 功能详解

MPU允许配置8-16个内存区域，每个区域可设置：

- 基地址
- 大小（4KB-4GB）
- 访问权限（特权/用户，读/写）
- 执行权限（XN位）
- 缓存属性

#### 配置示例

```c
void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    // 禁用MPU进行配置
    HAL_MPU_Disable();

    // 配置区域0: Flash (可执行, 可缓存)
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x08000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 配置区域1: SRAM (禁止执行)
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x20000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 配置区域2: 外设 (不可缓存, 设备内存)
    MPU_InitStruct.Number = MPU_REGION_NUMBER2;
    MPU_InitStruct.BaseAddress = 0x40000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_1MB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 使能MPU
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 2.6 功耗模式深度解析

Cortex-M支持多种功耗模式，从高功耗高性能到极低功耗深度睡眠。

#### 功耗模式对比

| 模式 | 唤醒源 | SRAM | 时钟 | 典型功耗 |
|------|--------|------|------|----------|
| **Run** | - | 保持 | 全开 | 100μW/MHz+ |
| **Sleep** | 任意中断 | 保持 | 可选 | ~50% Run |
| **Deep Sleep** | 特定中断 | 保持 | LSI | ~10% Run |
| **Stop** | 特定中断 | 保持 | 全关 | ~10μA |
| **Standby** | 特定引脚/RTC | 丢失 | RTC可选 | ~1μA |
| **VBAT** | - | 部分 | RTC可选 | ~0.1μA |

#### 低功耗设计要点

**动态功耗**：
- P = C × V² × f
- 降低电压和频率可显著省电

**静态功耗**：
- 漏电流
- 深度睡眠模式可降至nA级

**实际设计考量**：
```c
// 进入深度睡眠
void EnterDeepSleep(void) {
    // 使能PWR时钟
    __HAL_RCC_PWR_CLK_ENABLE();

    // 配置深度睡眠
    HAL_PWR_EnterDEEPSTOPMode(PWR_LOWPOWERMODE_STOP, PWR_STOPENTRY_WFI);
}

// 进入待机模式
void EnterStandby(void) {
    // 清除唤醒标志
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    // 使能唤醒引脚
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    // 进入待机
    HAL_PWR_EnterSTANDBYMode();
}
```

---

## 3. Cortex-R架构知识

### 3.1 Cortex-R4/R5/R7详细解析

#### Cortex-R4

**架构特点**：
- 双标量流水线（1顺序+1发射）
- 1.5 DMIPS/MHz
- 可选MPU
- 可选浮点单元

**典型应用**：
- 工业自动化
- 汽车电子（非安全关键）
- 存储控制器

#### Cortex-R5

**架构升级**：
- 保守设计（更多检查）
- 增强错误处理
- 可选双核锁步
- 更强MPU

**典型应用**：
- 汽车ADAS系统
- 工业安全
- 医疗设备

#### Cortex-R7

**架构升级**：
- Out-of-order执行（有限）
- 更高的主频
- 强化的缓存
- 典型2 DMIPS/MHz

**典型应用**：
- 汽车信息娱乐
- 基带处理
- 高性能工业控制

### 3.2 实时性保证机制

#### 中断响应

Cortex-R提供确定性的中断响应：

- **中断延迟**：<20周期（典型）
- **最大中断屏蔽时间**：可预测
- **中断优先级**：硬件保证

#### 内存架构

**ECC保护**：
```
┌────────────────────────────────────┐
│           64位数据总线             │
├────────────────────────────────────┤
│  56位数据  │  8位ECC               │
│  (8字节)  │  (单比特纠错)         │
└────────────────────────────────────┘
```

**错误处理**：
- 单比特错误：自动纠正
- 双比特错误：中断报告

### 3.3 双核锁步（DCLS）

#### 工作原理

```
┌─────────┐     ┌─────────┐
│ Core 0   │ ←──→│ Core 1  │
│ (Master) │ 同步 │ (Lock) │
└─────────┘     └─────────┘
    ↓               ↓
┌─────────────────────────────────┐
│    比较器 (Compare)              │
│  周期比较输出/指令               │
└─────────────────────────────────┘
```

#### 应用场景

- 安全气囊控制
- 刹车系统（ABS）
- 发动机控制

### 3.4 Cortex-R vs Cortex-M对比

| 维度 | Cortex-R | Cortex-M |
|------|----------|----------|
| 目标应用 | 安全关键 | 通用MCU |
| 实时性 | 硬实时 | 软实时 |
| 成本 | 高 | 低 |
| ECC | 必须 | 可选 |
| 认证 | ISO 26262 | IEC 61508 |
| 主频 | 最高600MHz | 最高300MHz |
| 典型制程 | 28nm+ | 40nm+ |

---

## 4. Cortex-A架构知识

### 4.1 Cortex-A系列概览

#### 主流型号

| 型号 | 架构 | 核数 | 特点 |
|------|------|------|------|
| Cortex-A53 | ARMv8-A | 1-4 | 64位，能效比 |
| Cortex-A55 | ARMv8-A | 1-8 | DynamIQ，小核 |
| Cortex-A72 | ARMv8-A | 1-4 | 高性能 |
| Cortex-A73 | ARMv8-A | 1-4 | 持续性能 |
| Cortex-A75 | ARMv8-A | 1-8 | 大核，DynamIQ |
| Cortex-A76 | ARMv8.2-A | 1-8 | 笔记本性能 |
| Cortex-A77 | ARMv8.2-A | 1-8 | 5G/AI优化 |
| Cortex-A78 | ARMv8.2-A | 1-8 | 最新大核 |
| Cortex-X1 | ARMv8.2-A | 1-8 | 最高性能 |

### 4.2 ARMv8-A架构基础

#### A64指令集

64位指令集，与32位不兼容：

```asm
// 64位加法
ADD X0, X1, X2      // X0 = X1 + X2

// 32位操作（零扩展）
ADD W0, W1, W2      // W0 = W1 + W2

// 64位立即数
MOV X0, #0x12345678
```

#### 异常级别（ELn）

```
┌─────────────────────────────────────────┐
│  EL0 (用户态)                           │
│  用户程序运行                            │
├─────────────────────────────────────────┤
│  EL1 (内核态)                           │
│  操作系统运行                            │
├─────────────────────────────────────────┤
│  EL2 (虚拟机管理)                       │
│  Hypervisor                             │
├─────────────────────────────────────────┤
│  EL3 (安全监控)                         │
│  Secure Monitor (TrustZone)            │
└─────────────────────────────────────────┘
```

### 4.3 Cortex-A与Cortex-M协同

#### 典型异构系统

```
┌──────────────────────────────────────────────────┐
│              Cortex-A (Linux/Android)            │
│  应用处理  网络  显示  复杂算法                   │
├──────────────────────────────────────────────────┤
│         共享内存 (RPMsg/Mailbox)                 │
├──────────────────────────────────────────────────┤
│              Cortex-M (RTOS/Bare-metal)          │
│  实时控制  传感器  物理接口  电机驱动            │
└──────────────────────────────────────────────────┘
```

---

## 5. 常用外设

### 5.1 GPIO详解

#### 工作模式

**输入模式**：
- 浮空输入（Floating）
- 上拉输入（Pull-up）
- 下拉输入（Pull-down）
- 模拟输入（Analog）

**输出模式**：
- 推挽输出（Push-Pull）
- 开漏输出（Open-Drain）

**复用模式**：
- 复用推挽
- 复用开漏

#### 配置流程

```c
// HAL库配置示例
void GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 使能GPIO时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 配置PA5为推挽输出（LED）
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 配置PA0为浮空输入（按键）
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 配置PA4为ADC输入
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
```

#### 中断配置

```c
// GPIO外部中断配置
void EXTI_Init(void) {
    // 使能SYSCFG时钟
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // 连接EXTI到PA0
    HAL_SYSCFG_EXTILineConfig(EXTI_LINE_0, EXTI_PORT GPIOA);

    // 配置中断
    EXTI_InitTypeDef EXTI_InitStruct = {0};
    EXTI_InitStruct.Line = EXTI_LINE_0;
    EXTI_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    EXTI_InitStruct.Pull = GPIO_PULLUP;
    HAL_EXTI_SetConfigLine(&EXTI_InitHandle, &EXTI_InitStruct);

    // 设置优先级
    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

// 中断处理
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if(GPIO_Pin == GPIO_PIN_0) {
        // 按键处理
    }
}
```

### 5.2 UART详解

#### 通信格式

**数据位**：
- 8N1：8数据位，无校验，1停止位
- 9N1：9数据位，无校验，1停止位
- 8E1：8数据位，偶校验，1停止位

**波特率精度**：
- 误差<2%
- 计算：`(Clock / (16 × Baudrate)) - 1`

#### DMA+中断模式

```c
// UART DMA接收配置
void UART_DMA_Init(void) {
    UART_HandleTypeDef huart;
    DMA_HandleTypeDef hdma_rx;

    // 开启DMA时钟
    __HAL_RCC_DMA1_CLK_ENABLE();

    // 配置DMA
    hdma_rx.Instance = DMA1_Stream5;
    hdma_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_rx.Init.Mode = DMA_CIRCULAR;
    hdma_rx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_rx);

    __HAL_LINKDMA(&huart, hdmarx, hdma_rx);

    // 开启空闲中断
    __HAL_UART_ENABLE_IT(&huart, UART_IT_IDLE);
}

// DMA+空闲中断接收
void UART_IdleRxHandler(UART_HandleTypeDef *huart) {
    uint32_t tmp;

    if(__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(huart);

        // 计算接收长度
        tmp = huart->RxXferSize - __HAL_DMA_GET_COUNTER(huart->hdmarx);

        // 处理数据
        ProcessRxData(huart->pRxBuffPtr, tmp);

        // 重新启动DMA
        HAL_UART_Receive_DMA(huart, rx_buffer, BUFFER_SIZE);
    }
}
```

#### RS485配置

```c
// RS485需要控制DE/RE引脚
void RS485_Init(void) {
    // 配置DE引脚为输出
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_DE;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIO_PORT_DE, &GPIO_InitStruct);

    // 默认接收模式
    HAL_GPIO_WritePin(GPIO_PORT_DE, GPIO_PIN_DE, GPIO_PIN_RESET);
}

// 发送前设置为发送模式
void RS485_Send(uint8_t *data, uint16_t len) {
    // 切换到发送模式
    HAL_GPIO_WritePin(GPIO_PORT_DE, GPIO_PIN_DE, GPIO_PIN_SET);

    // 发送数据
    HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);

    // 等待发送完成
    while(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TXE) == RESET);

    // 切换回接收模式
    HAL_GPIO_WritePin(GPIO_PORT_DE, GPIO_PIN_DE, GPIO_PIN_RESET);
}
```

### 5.3 SPI详解

#### 四种模式

| 模式 | CPOL | CPHA | 空闲SCK | 采样边沿 |
|------|------|------|---------|----------|
| 0 | 0 | 0 | 低电平 | 上升沿 |
| 1 | 0 | 1 | 低电平 | 下降沿 |
| 2 | 1 | 0 | 高电平 | 下降沿 |
| 3 | 1 | 1 | 高电平 | 上升沿 |

#### DMA传输

```c
// SPI DMA发送
void SPI_DMA_Tx(uint8_t *tx_buf, uint16_t size) {
    // 等待SPI空闲
    while(__HAL_SPI_GET_FLAG(&hspi1, SPI_FLAG_TXE) == RESET);

    // 启动DMA传输
    HAL_SPI_Transmit_DMA(&hspi1, tx_buf, size);
}

// SPI DMA接收
void SPI_DMA_Rx(uint8_t *rx_buf, uint16_t size) {
    HAL_SPI_Receive_DMA(&hspi1, rx_buf, size);
}

// 完整通信（同时收发）
void SPI_Transfer(uint8_t *tx_buf, uint8_t *rx_buf, uint16_t size) {
    HAL_SPI_TransmitReceive_DMA(&hspi1, tx_buf, rx_buf, size);
}

// DMA完成回调
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    // 数据处理完成
}
```

#### 多从机配置

```c
// 片选控制
void SPI_Select(uint8_t cs) {
    // 禁用所有从机
    HAL_GPIO_WritePin(CS1_GPIO_Port, CS1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CS2_GPIO_Port, CS2_Pin, GPIO_PIN_SET);

    // 使能指定从机
    switch(cs) {
        case 1: HAL_GPIO_WritePin(CS1_GPIO_Port, CS1_Pin, GPIO_PIN_RESET); break;
        case 2: HAL_GPIO_WritePin(CS2_GPIO_Port, CS2_Pin, GPIO_PIN_RESET); break;
    }
}
```

### 5.4 I2C详解

#### 总线速度

| 模式 | 速度 | 备注 |
|------|------|------|
| 标准 | 100kHz | 兼容SMBus |
| 快速 | 400kHz | |
| 快速+ | 1MHz | |
| 高速 | 3.4MHz | 需要额外硬件 |

#### 状态机

```
┌─────────┐
│  起始   │ ←──┐
└────┬────┘    │
     │         │
┌────▼─────────┐│
│  发送地址   │─┤
└────┬────────┘│
     │         │
     ├─────────┼──────────┐
     │         │          │
┌────▼──┐  ┌──▼────┐  ┌──▼─────┐
│  ACK  │  │ NACK  │  │ 错误   │
└───┬───┘  └───┬───┘  └───┬─────┘
    │          │          │
    │    ┌─────┴─────┐    │
    │    │           │    │
┌───▼────▼───┐ ┌─────▼────▼───┐
│   发送数据  │ │   接收数据   │
└─────┬──────┘ └──────┬──────┘
      │               │
      └───────┬───────┘
              │
        ┌─────▼─────┐
        │   停止    │
        └───────────┘
```

#### DMA传输

```c
// I2C DMA发送
HAL_StatusTypeDef I2C_DMA_Send(uint16_t DevAddress,
                                uint16_t MemAddress,
                                uint8_t *data,
                                uint16_t size) {
    hi2c1.Instance->CR1 |= I2C_CR1_START;

    // 等待起始条件发送完成
    while(!__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_SB));

    // 发送地址
    hi2c1.Instance->DR = DevAddress;

    // 等待地址发送完成
    while(!__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_ADDR));
    __HAL_I2C_CLEAR_ADDRFLAG(&hi2c1);

    // 发送内存地址（如果是内存地址模式）
    if(MemAddress != 0xFFFF) {
        while(!__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_TXE));
        hi2c1.Instance->DR = MemAddress;
    }

    // 使能DMA
    __HAL_I2C_DMABLE_ENABLE(&hi2c1);
    HAL_DMA_Start_IT(hi2c1.hdmatx, (uint32_t)data,
                     (uint32_t)&hi2c1.Instance->DR, size);

    return HAL_OK;
}
```

#### 总线死锁处理

```c
// I2C总线死锁检测与恢复
void I2C_BusRecovery(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 配置SCL为推挽输出
    GPIO_InitStruct.Pin = I2C_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(I2C_PORT, &GPIO_InitStruct);

    // 手动产生9个时钟脉冲
    for(int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
        delay_us(5);
        HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
        delay_us(5);
    }

    // 发送停止条件
    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
    delay_us(5);
    HAL_GPIO_WritePin(I2C_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
    delay_us(5);
    HAL_GPIO_WritePin(I2C_PORT, I2C_SDA_PIN, GPIO_PIN_SET);

    // 恢复I2C配置
    // ... 重新初始化I2C
}
```

### 5.5 TIM定时器详解

#### 定时器类型

| 类型 | 特点 | 用途 |
|------|------|------|
| 基本定时器 | 6位自动重载 | 定时中断 |
| 通用定时器 | 16位PWM/捕获 | 电机/PWM |
| 高级定时器 | 互补输出+死区 | 逆变器 |
| 低功耗定时器 | 睡眠可用 | 唤醒 |

#### PWM输出配置

```c
void TIM_PWM_Init(void) {
    TIM_HandleTypeDef htim;
    TIM_OC_InitTypeDef sConfig = {0};

    // 配置TIM3通道1为PWM
    htim.Instance = TIM3;
    htim.Init.Prescaler = 84-1;        // 84MHz / 84 = 1MHz
    htim.Init.Period = 1000-1;         // 1MHz / 1000 = 1kHz
    htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    HAL_TIM_PWM_Init(&htim);

    // 配置PWM通道
    sConfig.OCMode = TIM_OCMODE_PWM1;
    sConfig.Pulse = 500;               // 50% 占空比
    sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim, &sConfig, TIM_CHANNEL_1);

    // 使能输出
    HAL_TIM_PWM_Start(&htim, TIM_CHANNEL_1);
}

// 改变占空比
void TIM_SetDutyCycle(uint32_t duty) {
    __HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, duty);
}

// 改变频率
void TIM_SetFrequency(uint32_t freq) {
    uint32_t period = 84000000 / freq;
    __HAL_TIM_SET_AUTORELOAD(&htim, period - 1);
}
```

#### 输入捕获

```c
void TIM_IC_Init(void) {
    TIM_IC_InitTypeDef sConfig = {0};

    // 配置通道1为输入捕获
    sConfig.ICPolarity = TIM_ICPOLARITY_RISING;
    sConfig.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfig.ICPrescaler = TIM_ICPSC_DIV1;
    sConfig.ICFilter = 0;
    HAL_TIM_IC_ConfigChannel(&htim, &sConfig, TIM_CHANNEL_1);

    // 使能捕获中断
    HAL_TIM_IC_Start_IT(&htim, TIM_CHANNEL_1);
}

// 捕获回调 - 计算频率和占空比
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        // 捕获上升沿
        uint32_t value1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        // 切换到下降沿捕获
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_ICPOLARITY_FALLING);
    }
    else if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        // 捕获下降沿
        uint32_t value2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

        // 计算
        uint32_t period = value2 - value1;  // 周期
        uint32_t duty = value1;             // 高电平宽度

        // 切回上升沿
        __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_1, TIM_ICPOLARITY_RISING);
    }
}
```

#### 编码器模式

```c
void TIM_Encoder_Init(void) {
    TIM_Encoder_InitTypeDef sConfig = {0};

    // 配置编码器接口
    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Filter = 6;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Filter = 6;

    HAL_TIM_Encoder_Init(&htim, &sConfig);

    // 使能定时器
    HAL_TIM_Encoder_Start(&htim, TIM_CHANNEL_ALL);
}

// 读取编码器值
int32_t Encoder_Read(void) {
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim);
}
```

### 5.6 RTC详解

#### 功能说明

```c
void RTC_Init(void) {
    RTC_HandleTypeDef hrtc;
    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};

    // 配置RTC
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;    // 32.768KHz / 128 - 1 = 256Hz
    hrtc.Init.SynchPrediv = 255;      // 256Hz / 256 = 1Hz
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    HAL_RTC_Init(&hrtc);

    // 设置时间
    sTime.Hours = 12;
    sTime.Minutes = 30;
    sTime.Seconds = 0;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVE_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    HAL_RTC_SetTime(&hrtc, &sTime, FORMAT_BIN);

    // 设置日期
    sDate.WeekDay = RTC_WEEKDAY_WEDNESDAY;
    sDate.Month = RTC_MONTH_MARCH;
    sDate.Date = 5;
    sDate.Year = 26;
    HAL_RTC_SetDate(&hrtc, &sDate, FORMAT_BIN);
}
```

#### 闹钟配置

```c
void RTC_Alarm_Init(void) {
    RTC_AlarmTypeDef sAlarm = {0};

    // 配置闹钟A：每天8:30
    sAlarm.AlarmTime.Hours = 8;
    sAlarm.AlarmTime.Minutes = 30;
    sAlarm.AlarmTime.Seconds = 0;
    sAlarm.AlarmTime.SubSeconds = 0;
    sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
    sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    sAlarm.Alarm = RTC_ALARM_A;

    HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, FORMAT_BIN);
}

// 闹钟中断处理
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
    // 闹钟触发处理
}
```

### 5.7 看门狗详解

#### 独立看门狗（IWDG）

**特点**：
- 独立LSI时钟（~40kHz）
- 适用于对精度要求不高的场景
- 无法停止

```c
void IWDG_Init(void) {
    // 启动IWDG
    // 预分频: 4 * 2^prer = 4 * 2^4 = 256
    // 超时: 40K / 256 = 156Hz (6.4ms)
    // reload: 156 * 1s = 156
    IWDG->KR = IWDG_KEY_ENABLE;
    IWDG->PR = IWDG_PR_PR_4;
    IWDG->RLR = 156;  // 1秒超时
}

// 喂狗
void IWDG_Feed(void) {
    IWDG->KR = IWDG_KEY_RELOAD;
}
```

#### 窗口看门狗（WWDG）

**特点**：
- 使用APB1时钟
- 可配置窗口期
- 早期唤醒中断（EWI）

```c
void WWDG_Init(void) {
    // 窗口值: 0x7F
    // 预分频: 8
    // 计数器初值: 0x7F
    WWDG->CFR = 0x60 | WWDG_CFR_WDGTB_2;  // 窗口0x60, 预分频8
    WWDG->CR = 0x7F | WWDG_CR_WDGA;       // 启动, 初值0x7F
}

// 喂狗（必须在窗口内）
void WWDG_Feed(void) {
    WWDG->CR = 0x7F;  // 重装计数器
}
```

**窗口计算**：
```
窗口期 = (W[6:0] - T[6:0]) * Tclk_WWDG
Tclk_WWDG = PCLK1 / (4096 * 2^(WDGTB[2:0]))
```

### 5.8 CAN详解

#### CAN2.0B

**帧类型**：
- 数据帧：传输数据
- 遥控帧：请求数据
- 错误帧：错误通知
- 过载帧：过载通知
- 间隔帧：分隔帧

**标识符**：
- 标准帧：11位ID
- 扩展帧：29位ID

#### CAN-FD

**特点**：
- 数据段速率：2M-8Mbps
- 数据长度：64字节
- 灵活数据率

#### 位时序配置

```c
void CAN_Init(void) {
    CAN_FilterTypeDef sFilterConfig;

    // 配置过滤器
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    HAL_CAN_ConfigFilter(&hcan, &sFilterConfig);

    // 启动CAN
    HAL_CAN_Start(&hcan);

    // 开启接收中断
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

// 发送消息
HAL_StatusTypeDef CAN_Send(uint32_t ID, uint8_t *data, uint8_t len) {
    CAN_TxHeaderTypeDef TxHeader;
    uint32_t TxMailbox;

    TxHeader.StdId = ID;
    TxHeader.ExtId = 0;
    TxHeader.IDE = CAN_ID_STD;
    TxHeader.RTR = CAN_RTR_DATA;
    TxHeader.DLC = len;
    TxHeader.TransmitGlobalTime = DISABLE;

    return HAL_CAN_AddTxMessage(&hcan, &TxHeader, data, &TxMailbox);
}

// 接收回调
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t rx_data[8];

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, rx_data);

    // 处理数据
    if(RxHeader.StdId == 0x123) {
        // 处理
    }
}
```

### 5.9 USB详解

#### USB设备模式

```c
// USB CDC类初始化（虚拟串口）
void USB_Init(void) {
    USBD_CDC_ItfTypeDef fops = {
        .Control = CDC_Control,
        .Receive = CDC_Receive
    };

    USBD_Init(&hUsbDevice, &CDC_Desc, DEVICE_FS);
    USBD_CDC_RegisterInterface(&hUsbDevice, &fops);
    USBD_Start(&hUsbDevice);
}

// CDC数据接收
uint8_t CDC_Receive(uint8_t *Buf, uint32_t *Len) {
    // 处理接收数据
    ProcessUSBData(Buf, *Len);
    return USBD_OK;
}

// CDC数据发送
uint8_t CDC_Send(uint8_t *Buf, uint32_t Len) {
    return USBD_CDC_TransmitPacket(&hUsbDevice);
}
```

### 5.10 ETH以太网详解

#### LWIP集成

```c
void Ethernet_Init(void) {
    // 初始化DMA描述符
    for(int i = 0; i < ETH_RXBUFNB; i++) {
        RxBuff[i].status = 0;
        RxBuff[i].length = ETH_RX_BUF_SIZE;
        RxBuff[i].buffer = Rx_Buff[i];
        RxBuff[i].next = &RxBuff[i+1];
    }
    RxBuff[ETH_RXBUFNB-1].next = &RxBuff[0];

    // 初始化MAC
    heth.Instance = ETH;
    heth.Init.MACAddr = mac_addr;
    heth.Init.MediaType = ETH_MEDIA_MODE_AUTOMOTIV;
    heth.Init.CheckSumOffload = ETH_CHECKSUM_OFFLOAD_DISABLE;
    heth.Init.DisableDropRetryPackets = ETH_DROP_PACKET_ENABLE;
    heth.Init.ForwardUndersizedGoodFrames = ETH_FORWARD_UNDERSIZED_GOOD_FRAMES;
    heth.Init.ReceiveStoreForward = ETH_RECEIVE_STORE_FORWARD;
    heth.Init.TransmitStoreForward = ETH_TRANSMIT_STORE_FORWARD;
    heth.Init.ForwardErrorFrames = ETH_FORWARD_ERROR_FRAMES;

    HAL_ETH_Init(&heth);

    // 设置MAC地址
    HAL_ETH_SetMACAddr(&heth, mac_addr);

    // 开启接收
    __HAL_ETH_DMA_ENABLE(&heth);
}
```

---

## 6. DMA常用应用

### 6.1 DMA工作原理

#### DMA传输类型

| 类型 | 说明 |
|------|------|
| 内存到内存 | 最高优先级，不需要外设 |
| 外设到内存 | 如ADC/UART接收 |
| 内存到外设 | 如UART/DAC发送 |

#### DMA控制器结构

```
┌─────────────────────────────────────────┐
│              DMA Controller             │
├─────────────────────────────────────────┤
│  Stream/Channel 1                       │
│  - 源地址                                │
│  - 目标地址                              │
│  - 传输数量                              │
│  - 配置(Flow Ctrl, Inc, Size, Priority) │
├─────────────────────────────────────────┤
│  Stream/Channel 2-8                     │
│  ...                                    │
└─────────────────────────────────────────┘
```

### 6.2 内存到内存传输

```c
void DMA_M2M_Init(void) {
    DMA_HandleTypeDef hdma;

    hdma.Instance = DMA2_Stream0;
    hdma.Init.Channel = DMA_CHANNEL_0;
    hdma.Init.Direction = DMA_MEMORY_TO_MEMORY;
    hdma.Init.PeriphInc = DMA_PINC_ENABLE;
    hdma.Init.MemInc = DMA_MINC_ENABLE;
    hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma.Init.Mode = DMA_NORMAL;
    hdma.Init.Priority = DMA_PRIORITY_HIGH;
    hdma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma.Init.PeriphBurst = DMA_PBURST_SINGLE;

    HAL_DMA_Init(&hdma);
}

// 执行内存拷贝
void DMA_MemCopy(uint32_t src, uint32_t dst, uint32_t size) {
    HAL_DMA_Start(&hdma, src, dst, size);
    HAL_DMA_PollForTransfer(&hdma, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);
}
```

### 6.3 外设到内存（UART DMA）

```c
// DMA接收配置
void UART_DMA_Rx_Init(uint8_t *rx_buf, uint32_t size) {
    // 配置DMA
    hdma_usart1_rx.Instance = DMA2_Stream5;
    hdma_usart1_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;  // 循环模式
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;

    HAL_DMA_Init(&hdma_usart1_rx);

    // 关联到UART
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    // 开启DMA
    HAL_UART_Receive_DMA(&huart1, rx_buf, size);
}
```

### 6.4 ADC+DMA

```c
// ADC DMA采集多通道
void ADC_DMA_Init(void) {
    ADC_ChannelConfTypeDef sConfig = {0};

    // 配置ADC
    hadc.Instance = ADC1;
    hadc.Init.ScanConvMode = ADC_SCAN_ENABLE;  // 多通道
    hadc.Init.ContinuousConvMode = ENABLE;
    hadc.Init.DiscontinuousConvMode = DISABLE;
    hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc.Init.NbrOfConversion = 3;  // 3个通道

    HAL_ADC_Init(&hadc);

    // 通道0: PA0 - 温度传感器
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    // 通道1: PA1 - 电位器
    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = ADC_REGULAR_RANK_2;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    // 通道2: PA2 - 光敏
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_3;
    HAL_ADC_ConfigChannel(&hadc, &sConfig);

    // 配置DMA
    hdma_adc.Instance = DMA2_Stream0;
    hdma_adc.Init.Channel = DMA_CHANNEL_0;
    hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc.Init.Mode = DMA_CIRCULAR;
    hdma_adc.Init.Priority = DMA_PRIORITY_HIGH;

    HAL_DMA_Init(&hdma_adc);
    __HAL_LINKDMA(&hadc, dma_handle, hdma_adc);

    // 启动ADC DMA
    HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, 3);
}

// 使用：读取ADC值
uint16_t ADC_GetValue(uint8_t channel) {
    return adc_buf[channel];
}
```

### 6.5 双缓冲模式

```c
// 双缓冲配置
void DMA_DoubleBuffer_Init(void *buf1, void *buf2, uint32_t size) {
    hdma.Instance = DMA1_Stream0;
    hdma.Init.Channel = DMA_CHANNEL_4;
    hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma.Init.MemInc = DMA_MINC_ENABLE;
    hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma.Init.Mode = DMA_CIRCULAR;
    hdma.Init.Priority = DMA_PRIORITY_HIGH;

    // 使能双缓冲
    hdma.Init.DoubleBufferMode = DMA_DOUBLEBUFFERMODE_ENABLE;

    HAL_DMA_Init(&hdma);

    // 设置缓冲区地址
    HAL_DMA_SetBuffers(&hdma, buf1, buf2, size);

    // 启动
    HAL_DMA_Start(&hdma, (uint32_t)&USART1->DR, (uint32_t)buf1, size);
}

// 获取当前缓冲区
void* DMA_GetCurrentBuffer(void) {
    return (void *)((DMA_HandleTypeDef*)&hdma)->Instance->M0AR;
}

// 切换回调
void HAL_DMA_RxHalfCpltCallback(DMA_HandleTypeDef *hdma) {
    // 处理前半部分数据
    ProcessBuffer(hdma->Instance->M0AR, BUFFER_SIZE/2);
}

void HAL_DMA_RxCpltCallback(DMA_HandleTypeDef *hdma) {
    // 处理后半部分数据
    ProcessBuffer(hdma->Instance->M1AR, BUFFER_SIZE/2);
}
```

---

## 7. 缓存相关知识

### 7.1 缓存架构

#### Cortex-M7缓存

```
┌─────────────────────────────────────────────────────────┐
│                     Cortex-M7                           │
│  ┌─────────────┐        ┌─────────────┐                 │
│  │  I-Cache    │        │  D-Cache    │                 │
│  │  4-64KB     │        │  4-64KB     │                 │
│  └──────┬──────┘        └──────┬──────┘                 │
│         │                      │                        │
│         └──────────┬────────────┘                        │
│                    ↓                                    │
│  ┌─────────────────────────────────────────────────┐    │
│  │              Core Pipeline                      │    │
│  └─────────────────────────────────────────────────┘    │
│                    ↓                                    │
│         ┌──────────────────────┐                       │
│         │  TCM Interface        │  零等待              │
│         └──────────────────────┘                       │
└─────────────────────────────────────────────────────────┘
```

#### 缓存行结构

```
┌────────────────────────────────────────┐
│           Cache Line (32B)             │
├──────────┬──────────┬─────────┬─────────┤
│ Tag      │ Index    │ Offset  │  Data   │
│ (21bit)  │ (6bit)   │ (5bit)  │ (256bit)│
└──────────┴──────────┴─────────┴─────────┘
```

### 7.2 缓存策略

| 策略 | 读操作 | 写操作 |
|------|--------|--------|
| **WT (Write Through)** | 缓存命中返回数据 | 同时写缓存和内存 |
| **WB (Write Back)** | 缓存命中返回数据 | 只写缓存，标记脏位 |
| **RA (Read Allocate)** | 未命中分配缓存行 | - |
| **no-RA** | 未命中不分配 | - |

### 7.3 缓存一致性问题

#### DMA与缓存冲突

**问题描述**：
```
场景1: CPU写 -> DMA传输
┌─────────────────────────────────────────┐
│ CPU写入: DMA缓冲区 [0x20001000]        │
│   ↓                                    │
│ 写操作命中D-Cache (数据在Cache中)       │
│   ↓                                    │
│ Cache标记为"脏"                        │
│   ↓                                    │
│ DMA从内存读取 (未考虑Cache)             │
│   ↓                                    │
│ ❌ DMA得到旧数据                       │
└─────────────────────────────────────────┘

场景2: DMA接收 -> CPU读取
┌─────────────────────────────────────────┐
│ DMA写入: DMA缓冲区 [0x20001000]         │
│   ↓                                    │
│ 写入内存 (Cache未更新)                 │
│   ↓                                    │
│ CPU读取: 命中Cache                     │
│   ↓                                    │
│ ❌ CPU得到旧数据                       │
└─────────────────────────────────────────┘
```

#### 解决方案

```c
// 方法1: 禁用缓存区域
void MPU_NonCacheable_Init(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    // 配置DMA缓冲区为不可缓存
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x20010000;  // DMA缓冲区
    MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

// 方法2: 软件维护一致性
// CPU发送数据前Clean缓存
void DMA_SendBuffer(uint8_t *buf, uint32_t len) {
    // 确保缓冲区数据写入内存
    SCB_CleanDCache_by_Addr((uint32_t*)buf, len);

    // 启动DMA
    HAL_DMA_Start(&hdma, (uint32_t)buf, (uint32_t)&USART1->DR, len);
}

// CPU接收数据后Invalidate缓存
void DMA_ReceiveBuffer(uint8_t *buf, uint32_t len) {
    // 启动DMA
    HAL_DMA_Start(&hdma, (uint32_t)&USART1->DR, (uint32_t)buf, len);

    // 等待完成
    HAL_DMA_PollForTransfer(&hdma, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);

    // 使缓存失效，从内存读取新数据
    SCB_InvalidateDCache_by_Addr((uint32_t*)buf, len);
}
```

### 7.4 内存屏障

```c
// 数据屏障 - 确保所有内存访问完成
__DMB();  // Data Memory Barrier

// 指令屏障 - 确保后续指令从内存读取
__ISB();  // Instruction Synchronization Barrier

// 数据同步屏障 - 确保所有内存访问完成并刷新流水线
__DSB();  // Data Synchronization Barrier

// 典型使用场景
void MemoryBarrier_Example(void) {
    // 写操作
    flag = 1;
    __DMB();  // 确保flag写入完成

    // 读操作
    __DMB();  // 确保之前写完成
    if(flag) {
        data = *buffer;
    }
}
```

---

## 8. 调试技巧

### 8.1 调试器

#### J-Link

**特点**：
- 支持所有ARM Cortex
- 高速SWD（50MHz+）
- RTT日志输出
- Trace跟踪（需专业版）

**RTT使用**：
```c
#include "SEGGER_RTT.h"

// 初始化
SEGGER_RTT_Init();

// 输出
SEGGER_RTT_printf(0, "Value: %d\n", value);

// 阻塞输出
SEGGER_RTT_WriteString(0, "Hello\n");
```

#### ST-Link

**特点**：
- STM32专用
- SWD接口
- 成本低
- 有限功能

### 8.2 SWD/JTAG协议

#### SWD时序

```
SWDIO:  →→→→→→→→→→→→→→→→→→→→→→→→→→→
SWCLK:  _‾‾‾_‾‾‾_‾‾‾_‾‾‾_‾‾‾_‾‾‾_‾‾‾_‾‾‾_

        |← 1bit →|
```

#### 调试接口引脚

| 引脚 | 功能 |
|------|------|
| SWDIO | 双向数据 |
| SWCLK | 时钟 |
| SWO | Trace输出（可选） |
| RESET | 复位（可选） |

### 8.3 断点类型

| 类型 | 数量 | 实现方式 |
|------|------|----------|
| 硬件断点 | 4-6个 | 替换指令 |
| 软件断点 | 无限 | BKPT指令 |
| 条件断点 | 有限 | 组合实现 |
| 数据断点 | 2-4个 | 地址监控 |

### 8.4 Trace功能

#### ITM输出

```c
// ITM调试输出
void ITM_Send(uint32_t port, char c) {
    if(ITM->TCR & ITM_TCR_ITMENA_Msk) {  // ITM使能
        if(ITM->PORT[port].u32) {         // FIFO空
            ITM->PORT[port].u8 = c;
        }
    }
}

#define ITM_DEBUG(...)  do { \
    char buf[128]; \
    sprintf(buf, __VA_ARGS__); \
    for(char *p = buf; *p; p++) ITM_Send(0, *p); \
} while(0)
```

### 8.5 常见调试问题

#### 死机分析

**常见原因**：
1. 栈溢出
2. 数组越界
3. 空指针
4. 硬件异常未捕获
5. 看门狗复位

**分析方法**：
```c
// 栈溢出检测
void CheckStackOverflow(void) {
    // 检查堆栈水印
    UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
    if(watermark < 64) {
        printf("Warning: Stack low! %u\n", watermark);
    }
}
```

#### 复位分析

```c
// 分析复位原因
void CheckResetReason(void) {
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) {
        printf("Power-on reset\n");
    }
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
        printf("NRST pin reset\n");
    }
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
        printf("Software reset\n");
    }
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        printf("IWDG reset\n");
    }
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
        printf("WWDG reset\n");
    }
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) {
        printf("Low power reset\n");
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}
```

---

## 9. 业务知识

### 9.1 Bootloader设计

#### 两级Bootloader架构

```
┌─────────────────────────────────────────────────────┐
│                    Flash布局                        │
├─────────────────────────────────────────────────────┤
│ 0x08000000 │  16KB  │  Bootloader A (ROM)         │  厂商
├────────────┼────────┼──────────────────────────────┤
│ 0x08004000 │  64KB  │  Bootloader B (Flash)       │  用户
├────────────┼────────┼──────────────────────────────┤
│ 0x08010000 │ 256KB  │  Application A              │
├────────────┼────────┼──────────────────────────────┤
│ 0x08050000 │ 256KB  │  Application B (Backup)    │
├────────────┼────────┼──────────────────────────────┤
│ 0x08090000 │ 128KB  │  Config/NVM                  │
└────────────┴────────┴──────────────────────────────┘
```

#### 跳转实现

```c
typedef void (*JumpFunction)(void);

void JumpToApp(uint32_t app_address) {
    JumpFunction JumpToApp;
    uint32_t stack_top;

    // 检查栈顶地址是否有效
    if((app_address < APP_FLASH_START) ||
       (app_address >= APP_FLASH_END)) {
        return;
    }

    // 获取栈顶地址
    stack_top = *(uint32_t *)app_address;

    // 检查栈顶是否在SRAM范围内
    if((stack_top < SRAM_START) ||
       (stack_top > SRAM_END)) {
        return;
    }

    // 获取复位向量
    JumpToApp = (JumpFunction)*(uint32_t *)(app_address + 4);

    // 关闭所有外设
    HAL_RCC_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // 禁用中断
    __disable_irq();

    // 清除所有中断挂起
    for(int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    // 设置MSP
    __set_MSP(stack_top);

    // 禁用缓存（如果有）
    SCB_DisableICache();
    SCB_DisableDCache();

    // 数据同步
    __DSB();
    __ISB();

    // 跳转
    JumpToApp();
}
```

#### 固件升级流程

```
┌─────────────────────────────────────────────────────┐
│                   升级流程                          │
├─────────────────────────────────────────────────────┤
│ 1. 接收新固件 (UART/USB/网络)                       │
│ 2. 校验固件 (CRC/签名)                              │
│ 3. 写入备份区 (B区)                                 │
│ 4. 验证备份                                          │
│ 5. 设置升级标志                                      │
│ 6. 复位                                              │
│ 7. Bootloader检查标志                               │
│ 8. 拷贝B区到A区                                     │
│ 9. 清除标志                                          │
│ 10. 启动APP                                          │
└─────────────────────────────────────────────────────┘
```

### 9.2 链接脚本

```ld
/* STM32F4xx 链接脚本 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
    CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}

/* 入口 */
ENTRY(Reset_Handler)

/* 栈和堆 */
_stack_start = ORIGIN(RAM) + LENGTH(RAM);
_estack = _stack_start;
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

SECTIONS
{
    /* 代码段 */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* 只读数据 */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* 数据段 - 加载到Flash，运行复制到RAM */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    _sidata = LOADADDR(.data);

    /* BSS段 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    /* CCMRAM */
    .ccmram :
    {
        . = ALIGN(4);
        *(.ccmram*)
        . = ALIGN(4);
    } >CCMRAM

    /DISCARD/ :
    {
        libc.a (*)
        libm.a (*)
        libgcc.a (*)
    }
}
```

### 9.3 Flash编程

```c
// Flash擦除
HAL_StatusTypeDef Flash_Erase(uint32_t sector, uint32_t num) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    HAL_FLASH_Unlock();

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Banks = FLASH_BANK_1;
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = num;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_StatusTypeDef status = HAL_FLASH_Erase(&EraseInitStruct, &SectorError);

    HAL_FLASH_Lock();

    return status;
}

// Flash写入（双字）
HAL_StatusTypeDef Flash_Write_DoubleWord(uint32_t address, uint64_t data) {
    HAL_FLASH_Unlock();

    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                                   address, data);

    HAL_FLASH_Lock();

    return status;
}

// Flash写入（半字）
HAL_StatusTypeDef Flash_Write_HalfWord(uint32_t address, uint16_t data) {
    HAL_FLASH_Unlock();

    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                                   address, data);

    HAL_FLASH_Lock();

    return status;
}
```

---

## 10. 传感器接口

### 10.1 IMU传感器

#### 加速度计原理

**MEMS加速度计结构**：
```
┌─────────────────────────────────────────┐
│              固定电极                    │
│  ←──────── 质量块 ───────→              │
│         ↓         ↓                     │
│      可动电极   可动电极                  │
│              固定电极                    │
└─────────────────────────────────────────┘

加速度: a = F/m = k*x/m
电容变化: ΔC ∝ x ∝ a
```

**测量轴**：
- X轴：左右加速
- Y轴：前后加速
- Z轴：上下加速（含重力）

**关键参数**：
- 量程：±2g/±4g/±8g/±16g
- 分辨率：16位ADC
- 零偏：±50mg
- 噪声密度：100 μg/√Hz

#### 陀螺仪原理

**MEMS陀螺仪结构**：
```
┌────────────────────────────────────────────┐
│                                            │
│  ←────── 振动质量块 (驱动模式) ───────→    │
│                                            │
│         ║ 科里奥利力                      │
│         ↓                                  │
│    ┌──────────────┐                        │
│    │ 检测质量块   │ ←── 角速度感应          │
│    └──────────────┘                        │
│                                            │
└────────────────────────────────────────────┘
```

**测量原理**：
- 驱动模式：质量块做往复振动
- 角速度输入：产生科里奥利力
- 检测电容变化∝角速度

**关键参数**：
- 量程：±250/±500/±1000/±2000 °/s
- 灵敏度：LSB/°/s
- 零偏稳定性：°/hr

#### 磁力计原理

**各向异性磁阻(AMR)**：
```
┌─────────────────────────────────────┐
│                                     │
│   ════════  铁磁材料薄膜  ════════   │
│                                     │
│   磁场方向改变 → 电阻变化            │
│                                     │
└─────────────────────────────────────┘
```

**椭圆修正**：
- 硬磁补偿（恒定偏移）
- 软磁补偿（灵敏度失真）

#### 传感器融合算法

**互补滤波**：
```c
// 互补滤波姿态解算
void ComplementaryFilter(float *gx, float *gy, float *gz,
                          float *ax, float *ay, float *az,
                          float *pitch, float *roll) {
    // 加速度计计算角度
    float acc_pitch = atan2(-ax, sqrt(ay*ay + az*az));
    float acc_roll = atan2(ay, az);

    // 陀螺仪积分
    static float pitch = 0, roll = 0;
    pitch += *gx * dt;
    roll += *gy * dt;

    // 互补融合 (alpha = 0.98)
    *pitch = alpha * pitch + (1-alpha) * acc_pitch;
    *roll = alpha * roll + (1-alpha) * acc_roll;
}
```

**EKF扩展卡尔曼滤波**：
- 状态向量：[q0,q1,q2,q3, bx,by,bz]
- 预测步：陀螺仪积分
- 更新步：加速度计/磁力计校正

### 10.2 GNSS/GPS

#### NMEA0183协议

**常用语句**：

**$GPGGA** - 定位数据：
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
        │       │         │ │  │ │   │   │    │    │
        │       │         │ │  │ │   │   │    │    └── 椭球高
        │       │         │ │  │ │   │   │    └────── 大地水准面
        │       │         │ │  │ │   │   └────────── 海拔
        │       │         │ │  │ │   └──────────── HDOP
        │       │         │ │  │ └────────────── Satellites
        │       │         │ │  └──────────────── 定位质量(1=GPS)
        │       │         │ └────────────────── 经度E/W
        │       │         └────────────────── 纬度N/S
        │       └──────────────────────────── UTC时间
        └──────────────────────────────── 完整定位
```

**$GPRMC** - 推荐最小定位：
```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
        │    │   │         │         │     │     │     │
        │    │   │         │         │     │     │     └── 磁偏角W/E
        │    │   │         │         │     │     └──────── 磁偏角
        │    │   │         │         │     └─────────── 日期DDMMYY
        │    │   │         │         └────────────── 航向
        │    │   │         └────────────────────── 速度(节)
        │    │   └──────────────────────────────── 经度E/W
        │    └────────────────────────────────── 状态A=有效
        └────────────────────────────────────── UTC时间
```

#### RTK定位技术

**RTK原理**：
```
┌──────────────┐         差分数据(RTCM)         ┌──────────────┐
│   基准站     │ ───────────────────────────→ │   移动站     │
│  已知坐标    │                               │   流动站     │
│  载波相位    │                               │  载波相位    │
│  误差模型    │                               │  双差解算    │
└──────────────┘                               └──────────────┘
```

**双差模型**：
```
Δ∇Φ = Φr - Φb - (λ*Δ∇ρ) + Δ∇N + ε
Δ∇Φ: 双差载波相位
Φ: 载波相位
λ: 波长
ρ: 几何距离
N: 整周模糊度
ε: 测量噪声
```

**RTCM数据格式**：
- RTCM 3.0/3.2/3.3
- 消息类型：1005/1077/1087/1127

**NTRIP协议**：
```
┌──────────┐    HTTP     ┌──────────┐   RTCM    ┌──────────┐
│ NTRIP    │ ──────────→ │ NTRIP    │ ────────→ │   RTK    │
│ Client   │             │ Caster   │           │ 移动站    │
└──────────┘             └──────────┘           └──────────┘
```

---

## 附录

### 常见面试问题汇总

1. **Cortex-M中断响应流程？**
   - 自动保存上下文（xPSR, PC, LR, R12, R3-R0）
   - 读取向量表，跳转到处理函数
   - 执行处理，EXC_RETURN返回
   - 自动恢复上下文

2. **RTOS任务间通信方式？**
   - 信号量（二值/计数/互斥）
   - 消息队列
   - 事件标志组
   - 共享内存+信号量保护

3. **Bootloader跳转注意事项？**
   - 检查栈顶地址有效性
   - 禁用中断
   - 关闭缓存（如有）
   - 设置MSP

4. **DMA与缓存一致性如何处理？**
   - 禁用缓存区域
   - 软件维护（Clean/Invalidate）
   - 使用内存屏障

5. **SPI四种模式的区别？**
   - CPOL: 时钟空闲电平
   - CPHA: 数据采样边沿

6. **I2C总线死锁如何解决？**
   - 手动产生9个SCL脉冲
   - 软件复位I2C外设

7. **CAN总线终端电阻的作用？**
   - 阻抗匹配
   - 消除信号反射
   - 标准值120Ω

8. **MPU配置与内存保护？**
   - 配置8-16个区域
   - 设置访问权限
   - 设置XN位禁止执行

---

*文档版本：v2.0 详细版*
*更新时间：2026-03-05*

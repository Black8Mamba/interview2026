# 嵌入式MCU面试知识点

> 适用于高级嵌入式工程师岗位（智能硬件大厂）

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

### 1.1 三大系列定位

ARM Cortex系列处理器分为三个主要系列，分别针对不同的应用场景：

| 系列 | 定位 | 特点 | 典型应用 |
|------|------|------|----------|
| **Cortex-M** | 微控制器 | 低功耗、低成本、实时性好 | MCU、IoT设备、智能硬件 |
| **Cortex-R** | 实时处理器 | 高可靠性、实时性、容错 | 汽车电子、工业控制、医疗设备 |
| **Cortex-A** | 应用处理器 | 高性能、丰富功能、Linux支持 | 手机、平板、智能终端 |

### 1.2 各系列优缺点对比

#### Cortex-M系列
- **优点**：
  - 极低的功耗
  - 易于使用，开发工具成熟
  - 中断响应快
  - 成本低
  -  Thumb-2指令集（16/32位混合），代码密度高
- **缺点**：
  - 性能相对较弱
  - 不支持MMU（除Cortex-M7可选）
  - 不支持Linux

#### Cortex-R系列
- **优点**：
  - 实时性强，中断延迟确定
  - 高可靠性，支持ECC
  - 支持MMU（部分型号）
  - 适合安全关键应用
- **缺点**：
  - 成本较高
  - 生态相对Cortex-M较少
  - 不支持Linux（需要配合Cortex-A）

#### Cortex-A系列
- **优点**：
  - 性能强，支持Linux/Android
  - 丰富的软件生态
  - 支持MMU和虚拟化
  - 强大的处理能力
- **缺点**：
  - 功耗较高
  - 实时性较差
  - 开发复杂度高
  - 成本高

### 1.3 应用场景分析

**Cortex-M应用场景**：
- 简单传感器节点
- 智能家居设备
-  wearables（手环等）
- 汽车ECU（辅助ECU）
- 工业PLC

**Cortex-R应用场景**：
- 汽车安全系统（安全气囊、刹车）
- 工业实时控制
- 航空电子设备
- 医疗设备（起搏器等）
- 存储控制器

**Cortex-A应用场景**：
- 智能手机/平板
- 智能车载系统
- 边缘计算网关
- 多媒体设备
- 复杂IoT网关

---

## 2. Cortex-M架构知识

### 2.1 Cortex-M3/M4/M7/M33核心架构

#### Cortex-M3
- **架构版本**：ARMv7-M
- **流水线**：3级流水线（取指-译码-执行）
- **指令集**：Thumb-2（16/32位）
- **特性**：
  - 硬件除法器
  - 位操作指令（BB、BFC、BFI、CLZ、RBIT等）
  - 条件执行（IT块）
  - 非对齐访问支持
- **典型芯片**：STM32F1、LPC17xx

#### Cortex-M4
- **架构版本**：ARMv7E-M（在M3基础上增加）
- **流水线**：3级流水线
- **新增特性**：
  - DSP指令（单周期乘加、SIMD）
  - 单精度浮点单元（FPU）
  - 饱和算术指令
- **典型芯片**：STM32F4、NXP MK64

#### Cortex-M7
- **架构版本**：ARMv7E-M
- **流水线**：6级流水线（双发射）
- **新增特性**：
  - 可选L1缓存（I-Cache/D-Cache）
  - 可选MPU
  - 可选ETM跟踪
  - 更高的主频（200MHz+）
  - TCM接口（Tightly Coupled Memory）
- **典型芯片**：STM32F7、IMXRT1050

#### Cortex-M33
- **架构版本**：ARMv8-M（32位）
- **流水线**：4级流水线
- **新增特性**：
  - 安全扩展（TrustZone）
  - 协处理器接口
  - 改进的DSP指令
  - 可选FPU
- **典型芯片**：STM32L5、NXP LPC55S

### 2.2 指令集（Thumb/Thumb-2）

#### Thumb指令集
- 16位指令格式
- 代码密度高
- 性能较低
- 只能访问部分寄存器

#### Thumb-2指令集
- 16位和32位混合指令
- 兼具代码密度和性能
- 首次出现在Cortex-M3
- 支持所有ARM指令功能

**常见Thumb-2指令分类**：
- 数据处理：ADD、SUB、MOV、CMP
- 逻辑运算：AND、ORR、EOR、BIC
- 移位操作：LSL、LSR、ASR、ROR
- 加载存储：LDR、STR、LDM、STM
- 分支：B、BL、BX、BLX
- 特殊：SEV、WFI、WFE、ISB、DSB、DMB

### 2.3 寄存器组

#### 通用寄存器（R0-R12）
- R0-R7：低寄存器，所有指令可访问
- R8-R12：高寄存器，部分指令限制

#### 特殊寄存器
- **R13 (SP)**：堆栈指针
  - MSP：主堆栈指针（复位后默认）
  - PSP：进程堆栈指针（用于线程模式）
- **R14 (LR)**：链接寄存器，保存函数返回地址
- **R15 (PC)**：程序计数器
- **PSR**：程序状态寄存器
  - APSR：应用PSR（flags）
  - IPSR：中断PSR（异常编号）
  - EPSR：执行PSR（Thumb状态）
- **PRIMASK**：中断屏蔽寄存器
- **CONTROL**：控制寄存器
  - bit[0]：特权级（0=特权，1=用户）
  - bit[1]：堆栈选择（0=MSP，1=PSP）

### 2.4 异常处理模型

#### 异常类型
| 异常编号 | 类型 | 优先级 |
|----------|------|--------|
| 0 | - | - |
| 1 | Reset | -3 |
| 2 | NMI | -2 |
| 3 | Hard Fault | -1 |
| 4 | Memory Management Fault | 可配置 |
| 5 | Bus Fault | 可配置 |
| 6 | Usage Fault | 可配置 |
| 11-15 | External Interrupts | 可配置 |

#### 向量表
- 位于Flash起始地址（可重定位）
- 第一个字是主堆栈指针(MSP)初值
- 后续是各异常处理函数地址
- 第0个向量是栈顶地址，第1个是Reset_Handler

#### 优先级
- 编号越小优先级越高
- 可通过NVIC配置
- 优先级分组决定抢占能力

### 2.5 NVIC中断控制器

#### 功能特性
- 支持最多256个中断（具体芯片实现）
- 8位优先级字段（实际实现可能更少）
- 支持向量式中断
- 支持中断嵌套
- 支持优先级分组

#### 编程接口
```c
// 使能中断
NVIC_EnableIRQ(IRQn_Type irq);

// 设置优先级
NVIC_SetPriority(IRQn_Type irq, uint32_t priority);

// 设置优先级分组
NVIC_SetPriorityGrouping(uint32_t priority_group);

// 进入临界区
__disable_irq();  // 关全局中断
__enable_irq();  // 开全局中断
```

### 2.6 内存保护单元（MPU）

#### 功能
- 划分内存区域
- 设置区域属性（权限、缓存策略）
- 防止非法内存访问
- 支持8/16个区域

#### 区域属性
- AP（访问权限）：特权/用户，读/写
- XN：执行禁止
- S：可共享
- TEX：类型扩展
- C/B：缓存/缓冲属性

### 2.7 DSP指令与FPU

#### DSP指令（Cortex-M4/M7）
- **单周期乘加**：`MLA, MLS, SMMLA, SMMLS`
- **SIMD操作**：`ADD8, SUB16, SADD16`
- **饱和指令**：`QADD, QSUB, USAT, SSAT`
- **除法**：SDIV/UDIV（12-20周期）

#### FPU（Cortex-M4/M7/M33）
- 单精度浮点（32位）
- 寄存器S0-S31（可映射到D0-D15）
- 协处理器CP10/CP11
- Lazy stacking支持

### 2.8 功耗模式

| 模式 | 进入指令 | 唤醒源 | SRAM | 时钟 |
|------|----------|--------|------|------|
| Sleep | WFI/WFE | 任意中断 | 保持 | 可选 |
| Deep Sleep | LPDS | 特定中断 | 保持 | 关闭 |
| Stop | 深度停止 | 特定中断 | 保持 | 全关闭 |
| Standby | 深度停止+PU | 特定引脚/RTC | 丢失 | 全关闭 |
| VBAT | - | - | 部分保持 | RTC可选 |

---

## 3. Cortex-R架构知识

### 3.1 Cortex-R4/R5/R7/R8概述

| 型号 | 架构 | 特点 | 应用 |
|------|------|------|------|
| Cortex-R4 | ARMv7-R | 双标量流水线，MPU | 工业、汽车 |
| Cortex-R5 | ARMv7-R | R4+保守+低延迟 | 汽车ADAS |
| Cortex-R7 | ARMv7-R | 双核+MPU+ECC | 存储、汽车 |
| Cortex-R8 | ARMv7-R | R7+更多特性 | 调制解调器 |

### 3.2 实时性特性

#### 高可靠性设计
- 确定性中断响应
- 低中断延迟（可配置）
- 零缺陷设计方法
- 内存 ECC 保护

#### 容错机制
- 双核锁步（DCLS）
- 错误检测与纠正
- 看门狗监控

### 3.3 内存架构

#### MPU
- 5级/7级页表
- 支持内存区域划分
- 内存属性配置

#### ECC
- 1位纠错/2位检测
- 内存错误上报
- 自动纠正

### 3.4 与Cortex-M的区别

| 特性 | Cortex-M | Cortex-R |
|------|----------|----------|
| 架构 | ARMv6-M/7-M/8-M | ARMv7-R/8-R |
| MMU | 可选 | 必须 |
| 缓存 | 可选 | 必须 |
| 实时性 | 良好 | 优秀 |
| ECC | 无 | 必须 |
| 成本 | 低 | 高 |
| Linux支持 | 不支持 | 不直接支持 |

### 3.5 应用场景

- **汽车电子**：安全气囊、刹车系统、发动机控制
- **工业控制**：PLC、运动控制器
- **医疗设备**：起搏器、监护仪
- **存储系统**：SSD控制器

---

## 4. Cortex-A架构知识（与MCU相关）

### 4.1 Cortex-A系列概述

常见型号：
- **Cortex-A5/A7**：低功耗应用处理器
- **Cortex-A8**：单核高性能（已被淘汰）
- **Cortex-A9**：多核（4核 max）
- **Cortex-A15/A17**：高性能
- **Cortex-A53/A57/A72**：64位（ARMv8）
- **Cortex-A55/A75/A76/A77**：DynamIQ架构
- **Cortex-A78/X1/X2**：最新高性能

### 4.2 与M/R系列的主要区别

| 特性 | Cortex-M | Cortex-A |
|------|----------|----------|
| 指令集 | Thumb-2 | A32/T32/A64 |
| MMU | 可选 | 必须 |
| Linux支持 | 不支持 | 支持 |
| 虚拟化 | 无 | 支持 |
| 流水线深度 | 3-6级 | 8-15级 |
| 主频 | <300MHz | >2GHz |

### 4.3 在异构系统中的应用（MPU + A核）

#### 常见架构
- **MCU + A核**：Cortex-M负责实时任务，Cortex-A负责复杂应用
- **AMP异构**：多核不同架构（例：R核+M核+A核）
- **协处理器**：DSP/神经网络加速器

#### 核间通信
- 共享内存 + 消息队列
- Mailbox中断
- RPMsg框架

---

## 5. 常用外设

### 5.1 低速外设

#### GPIO（通用输入输出）

**功能模式**：
- 输入：浮空、上拉、下拉、模拟
- 输出：推挽、开漏
- 复用：外设功能映射
- 中断：上/下/边沿触发

**编程要点**：
```c
// GPIO配置结构体
typedef struct {
    uint32_t Pin;       // 引脚号
    uint32_t Mode;      // 模式
    uint32_t Pull;     // 上拉/下拉
    uint32_t Speed;    // 速度
    uint32_t Alternate;// 复用功能
} GPIO_InitTypeDef;
```

**常见问题**：
- 上拉/下拉电阻选择
- 开漏输出需要上拉电阻
- 复用功能配置冲突

#### UART（通用异步收发）

**工作模式**：
- 全双工异步通信
- 8N1/9N1/7E1等格式
- 硬件流控（RTS/CTS）
- RS485支持（使能引脚控制）
- DMA/中断模式

**常见应用**：
- 调试串口
- GPS模块通信
- 模块AT指令

**常见问题**：
- 波特率误差（需<2%）
- 缓冲区溢出
- DMA与中断优先级

#### SPI（串行外设接口）

**工作模式**：
- 全双工同步
- 主/从模式
- 时钟极性(CPOL)/相位(CPHA)
- 数据位宽（4-16位）
- 多从机模式（片选）
- DMA支持

**常见应用**：
- Flash存储
- 显示驱动
- 传感器通信

**常见问题**：
- MISO上拉导致从机无法工作
- 时钟模式配置错误
- 多从机片选冲突

#### I2C（集成电路总线）

**工作模式**：
- 主/从模式
- 标准模式（100kHz）
- 快速模式（400kHz）
- 快速模式+（1MHz）
- SMBus/PMBus支持
- DMA支持

**常见应用**：
- EEPROM
- 传感器（温湿度、加速度）
- RTC

**常见问题**：
- 从机地址冲突
- 总线死锁（SCL被拉低）
- 上拉电阻选择

#### TIM（定时器）

**工作模式**：
- 定时器基本功能
- PWM输出
- 输入捕获
- 正交编码器
- 脉冲计数
- 触发ADC/DMA

**PWM配置**：
- 频率 = 时钟 / (PSC+1) / (ARR+1)
- 占空比 = CCR / (ARR+1)

**常见应用**：
- LED调光
- 电机控制（PWM + 正交编码）
- 定时任务

#### RTC（实时时钟）

**功能**：
- 日历（年/月/日/时/分/秒）
- 闹钟（可编程）
- 周期性唤醒
- 时间戳功能
- 数字校准

**电源**：
- VBAT引脚（纽扣电池）
- 低功耗保持

#### WDT/IWDT（看门狗）

**独立看门狗（IWDG）**：
- 独立时钟源（LSI，40kHz）
- 12位计数器
- 不可配置超时（近似）
- 只能复位

**窗口看门狗（WWDG）**：
- APB1时钟
- 可配置窗口期
- 早期唤醒中断（EWI）
- 可触发复位或中断

**喂狗时机**：
- IWDG：计数器<窗口值
- WWDG：计数器在窗口范围内

**超时计算**：
```
IWDG: Tout = (4 * 2^prescaler) * (reload) / LSI
```

#### CAN（控制器局域网）

**CAN2.0B**：
- 标准帧（11位ID）
- 扩展帧（29位ID）
- 速率：125k/250/1M
k/500k- 消息过滤（ID掩码）
- 错误处理

**CAN-FD**：
- 灵活数据速率
- 数据段可达64字节
- 速率：2M-8M
- 兼容CAN2.0

**位时序**：
- 同步段(Seg)
- 传播段(Prop Seg)
- 相位段1/2(Phase Seg1/2)
- 采样点位置

**常见问题**：
- 总线终端电阻（120Ω）
- 位时间配置
- 总线负载率
- 错误帧处理

#### COMP（比较器）

- 可编程阈值
- 高速/低速模式
- 输出滤波
- 窗口比较
- 触发ADC/TIM

#### OPAMP（运算放大器）

- 可编程增益
- 跟随/比较/放大模式
- 与ADC/DAC联动

### 5.2 高速外设

#### USB（通用串行总线）

**模式**：
- Device模式：CDC/HID/DFU/MTP
- Host模式：Mass Storage/HID
- OTG模式：双重角色

**速率**：
- USB 1.1：低速1.5Mbps/全速12Mbps
- USB 2.0：高速480Mbps
- USB 3.0：超高速5Gbps

**类驱动**：
- HID：键盘/鼠标/手柄
- CDC：虚拟串口
- MSC：U盘
- DFU：固件升级
- AUDIO：音频

#### ETH（以太网）

**MAC控制器**：
- 10/100M/1G速率
- MII/RMII/GMII接口
- DMA环形缓冲区
- 帧过滤（MAC/VLAN）

**PHY**：
- 物理层芯片
- 自动协商
- MDIO管理

**软件栈**：
- LwIP
- FreeRTOS-TCP
- uIP

#### SDIO（安全数字输入输出）

**支持**：
- SD卡（SDSC/SDHC/SDXC）
- eMMC
- WiFi模块

**速率**：
- SD卡：默认25MHz/高速50MHz
- eMMC：8位/52MHz

#### DCMI/CSI（摄像头接口）

**DCMI（并行）**：
- 8/10/12位并行
- 支持YUV/RGB/JPEG
- DMA传输

**CSI（串行）**：
- MIPI CSI-2
- 1-4 lane
- 更高速率

#### DMA（直接内存访问）

**特性**：
- 独立于CPU传输
- 通道/流/Stream概念
- 优先级配置
- 循环/双缓冲模式

---

## 6. DMA常用应用

### 6.1 内存到内存传输

```c
DMA_InitTypeDef DMA_InitStructure;
DMA_InitStructure.Direction = DMA_MEMORY_TO_MEMORY;
DMA_InitStructure.MemoryInc = DMA_MemoryInc_Enable;
DMA_Init(DMA2_Stream0, &DMA_InitStructure);
```

### 6.2 外设到内存传输

**UART DMA接收**：
```c
DMA_InitStructure.Direction = DMA_PERIPH_TO_MEMORY;
DMA_InitStructure.MemoryInc = DMA_MemoryInc_Enable;
DMA_InitStructure.PeripheralInc = DMA_PeripheralInc_Disable;
DMA_InitStructure.MemoryDataSize = DMA_MemoryDataSize_Byte;
DMA_Init(DMA2_Stream5, &DMA_InitStructure);
// 配置UART的RX DMA
```

**ADC DMA采集**：
```c
DMA_InitStructure.Direction = DMA_PERIPH_TO_MEMORY;
DMA_InitStructure.MemoryDataSize = DMA_MemoryDataSize_HalfWord;
DMA_InitStructure.MemoryInc = DMA_MemoryInc_Enable;
// 连续转换模式 + DMA
```

### 6.3 内存到外设传输

**UART DMA发送**：
```c
DMA_InitStructure.Direction = DMA_MEMORY_TO_PERIPH;
DMA_InitStructure.MemoryInc = DMA_MemoryInc_Enable;
// 配合TXE中断或DMA完成中断
```

### 6.4 双缓冲/循环模式

**双缓冲**：
- 双数据缓冲区
- 切换时产生中断
- 适合高速数据流

**循环模式**：
- 自动重载
- 适合周期性采样
- 无需中断重配置

### 6.5 DMA与中断结合

```c
// DMA半传输完成中断
DMA_ITConfig(DMA2_Stream5, DMA_IT_HT, ENABLE);

// DMA传输完成中断
DMA_ITConfig(DMA2_Stream5, DMA_IT_TC, ENABLE);

// 中断处理
void DMA2_Stream5_IRQHandler(void) {
    if(DMA_GetITStatus(DMA2_Stream5, DMA_IT_TC)) {
        // 数据处理
        DMA_ClearITPendingBit(DMA2_Stream5, DMA_IT_TC);
    }
}
```

### 6.6 DMA链式传输

- 多描述符链表
- 自动切换
- 适合复杂传输场景

### 6.7 DMA异常处理

**常见问题**：
- 传输错误
- 传输超时
- 缓冲区溢出

**处理方法**：
- 使能错误中断
- 检查标志位
- 复位DMA通道

---

## 7. 缓存相关知识

### 7.1 I-Cache / D-Cache

**指令缓存(I-Cache)**：
- 缓存指令
- 读取指令
- 自动缓存命中

**数据缓存(D-Cache)**：
- 缓存数据
- 读写缓存
- 写回/直写策略

### 7.2 缓存行（Cache Line）

- 典型大小：32字节
- 对齐要求
- 最小缓存单位

### 7.3 缓存策略

| 策略 | 描述 |
|------|------|
| WT (Write Through) | 写同时更新内存 |
| WB (Write Back) | 写仅更新缓存 |
| RA (Read Allocate) | 读缺失时分配 |
| no-RA | 读缺失时不分配 |

### 7.4 缓存一致性

#### DMA与缓存一致性问题

**问题描述**：
- DMA从内存读取数据时，内存中可能是脏数据
- DMA写入内存后，CPU缓存中可能是旧数据

**软件维护方法**：

```c
// CPU写DMA缓冲区前：Clean（清空缓存到内存）
void DMA BufferClean(void *addr, uint32_t size) {
    SCB_InvalidateDCache_by_Addr((uint32_t*)addr, size);
}

// CPU读DMA缓冲区后：Invalidate（使缓存失效）
void DMA_BufferInvalidate(void *addr, uint32_t size) {
    SCB_InvalidateDCache_by_Addr((uint32_t*)addr, size);
}
```

**常见场景**：
- UART DMA接收：Invalidate
- UART DMA发送：Clean
- ADC DMA采集：Invalidate
- DAC DMA发送：Clean

#### Cache Coherent DMA
- 硬件维护一致性
- 不需要软件干预

#### MPU配置与缓存
```c
// 配置内存区域为无缓存
MPU_Region_InitTypeDef MPU_InitStruct;
MPU_InitStruct.Enable = MPU_REGION_ENABLE;
MPU_InitStruct.Number = MPU_REGION_NUMBER0;
MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
HAL_MPU_ConfigRegion(&MPU_InitStruct);
```

### 7.5 预取机制

- 指令预取
- 数据预取
- 可配置关闭

### 7.6 MPU对缓存的控制

- 区域缓存属性配置
- Shareable位设置
- 设备内存 vs 普通内存

---

## 8. 调试技巧

### 8.1 J-Link/ST-Link调试器

**J-Link**：
- 支持ARM/Cortex全系列
- 高速SWD/JTAG
- RTT实时日志
- Trace功能（需专业版）

**ST-Link**：
- STM8/32专用
- SWD接口
- 成本低

### 8.2 SWD/JTAG协议原理

**SWD（Serial Wire Debug）**：
- 两根线（SWDIO/SWCLK）
- 复位、编程、调试
- 比JTAG节省引脚

**JTAG**：
- 传统标准
- 5/4线制
- 支持边界扫描

### 8.3 断点类型

| 类型 | 说明 |
|------|------|
| 硬件断点 | 调试器实现，数量有限 |
| 软件断点 | 替换指令，数量无限制 |
| 条件断点 | 满足条件触发 |
| 数据断点 | 访问特定地址触发 |

### 8.4 观察点（Watchpoint）

- 监控数据访问
- 读/写/访问触发
- 用于内存分析

### 8.5 Trace功能

**ETM（Embedded Trace Macrocell）**：
- 指令跟踪
- 需ETM引脚

**ETB（Embedded Trace Buffer）**：
- 片内Trace缓冲区
- 有限容量

**ITM（Instrumentation Trace Macrocell）**：
- 软件日志输出
- 硬件timestamp
- 低开销

### 8.6 常见调试问题分析

**死机问题**：
- 栈溢出
- 非法内存访问
- 硬件异常未捕获

**复位问题**：
- 看门狗未喂狗
- 电源问题
- 复位引脚干扰

**数据异常**：
- 缓存一致性问题
- DMA配置错误
- 并发访问冲突

---

## 9. 业务知识

### 9.1 Bootloader设计

#### 升级流程
1. 接收新固件（UART/USB/网络）
2. 验证固件完整性（CRC/签名）
3. 写入备份区
4. 跳转新固件

#### A/B升级
- 双分区设计
- 失败回滚
- 签名验证

```c
// 典型跳转代码
typedef void (*JumpFunction)(void);
uint32_t app_address = APP_START_ADDRESS;
JumpFunction JumpToApp = (JumpFunction)(*(uint32_t*)(app_address + 4));
SCB_DisableDCache();
SCB_DisableICache();
HAL_RCC_DeInit();
SysTick->CTRL = 0;
JumpToApp();
```

### 9.2 启动流程

**上电**：
1. 复位
2. 读取栈顶地址（0x08000000）
3. 取Reset_Handler地址
4. 执行SystemInit
5. 执行__main
6. 跳转到main

**阶段**：
- ROM Bootloader（厂家）
- Flash Bootloader（用户）
- Application

### 9.3 内存布局

**链接脚本**：
```
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
  .text : { *(.text*) } > FLASH
  .data : { *(.data*) } > RAM AT> FLASH
  .bss  : { *(.bss*) } > RAM
}
```

### 9.4 Flash/EEPROM编程

**Flash操作**：
- 扇区擦除
- 编程（半字/字/双字）
- 擦写寿命（10K-100K次）

**磨损均衡**：
- 均衡算法
- 跳过坏块

### 9.5 功耗管理

**动态电压频率调节(DVFS)**：
- 根据负载调整频率
- 调整电压
- 节省功耗

**低功耗模式**：
- 睡眠模式
- 停止模式
- 待机模式
- 静态功耗 vs 动态功耗

### 9.6 固件升级

**OTA（Over-The-Air）**：
- 网络升级
- 增量升级
- 安全验证

**UART升级**：
- 串口 bootloader
- Xmodem/Ymodem协议

**USB DFU**：
- DFU协议
- USB升级工具

---

## 10. 传感器接口

### 10.1 IMU（惯性测量单元）

#### 加速度计原理（MEMS）
- 微机械质量块
- 电容变化检测
- 测量重力分量
- 单位：g（9.8m/s²）

#### 陀螺仪原理（MEMS、振动结构）
- 振动结构科里奥利力
- 电容变化检测
- 单位：°/s

#### 磁力计/电子罗盘原理
- 地磁场检测
- 霍尔效应/各向异性磁阻
- 单位：μT

#### 传感器融合算法
- **AHRS**：姿态航向参考系统
- **EKF**：扩展卡尔曼滤波
- 互补滤波

#### IMU+GPS融合（组合导航）
- GPS提供绝对位置
- IMU提供相对变化
- 紧耦合/松耦合

#### 常见芯片
- **MPU6050**：6轴（Accel+Gyro）
- **MPU9250**：9轴（+磁力计）
- **BMI088**：高性能6轴
- **LSM6DSV**：6轴+有限状态机

#### 零偏校准、温度补偿
- 静态零偏校准
- 温度曲线补偿
- Allan方差分析噪声

### 10.2 GNSS/GPS

#### GPS/GLONASS/北斗/Galileo系统
- GPS：美国，24颗卫星
- GLONASS：俄罗斯
-北斗：中国，55颗卫星
- Galileo：欧盟

#### NMEA0183协议
- 文本协议
- $GPGGA/GPGLL/GPRMC/GPVTG
- ASCII格式

#### GGA语句
```
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
```

#### RMC语句
```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
```

#### RTK定位技术

**RTK原理**：
- 载波相位差分
- 基准站已知坐标
- 厘米级精度

**基准站与移动站**：
- 基准站：固定坐标
- 移动站：待测点

**双差定位模型**：
- 站间差分
- 星间差分
- 消除钟差

**整周模糊度解算**：
- OTF（On The Fly）
- AR（Ambiguity Resolution）

**RTCM数据链路**：
- 差分数据格式
- RTCM 3.0/3.2/3.3

**NTRIP协议**：
- 网络RTK传输
- Caster/Client/Source

**精度等级**：
- RTK：2cm
- PPP-RTK：cm级
- DGPS：亚米级
- 单点定位：米级

**多路径效应与抗干扰**：
- 多路径信号反射
- 天线设计
- 信号质量评估

**基准站选址**：
- 开阔天空
- 远离干扰
- 稳定供电

#### 常见模块
- **ublox**：M8系列/NEO系列
- **泰斗**：TD1030
- **MTK**：MT3333系列
- **和芯星通**：UM220

#### PPS授时（脉冲对齐）
- 精确到纳秒
- 同步时钟
- 触发采集

#### AGPS辅助定位
- 星历辅助
- 加速定位
- 缩短TTFF

### 10.3 其他传感器

#### 温湿度传感器
- **SHT系列**：SHT20/30/40
- **AM2320**：I2C接口

#### 气压传感器
- **BMP280**：SPI/I2C
- **BMP390**：更高精度

#### 光传感器
- 环境光检测
- 接近检测

#### 距离传感器
- **ToF**：飞行时间法
- **超声波**：声波测距

---

## 附录

### 常见面试问题

1. **Cortex-M中断响应流程？**
2. **RTOS任务间通信方式？**
3. **Bootloader跳转注意事项？**
4. **DMA与缓存一致性如何处理？**
5. **SPI四种模式的区别？**
6. **I2C总线死锁如何解决？**
7. **CAN总线终端电阻的作用？**
8. **MPU配置与内存保护？**

---

*文档版本：v1.0*
*更新时间：2026-03-05*

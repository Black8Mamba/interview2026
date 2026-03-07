# MCU开发面试复习资料

> 适用于中高级/高级岗位面试准备

## 目录

- [第一部分：核心基础知识](#第一部分核心基础知识)
  - [1.1 ARM Cortex-M内核架构](#11-arm-cortex-m内核架构)
  - [1.2 汇编语言与寄存器](#12-汇编语言与寄存器)
  - [1.3 启动流程与内存布局](#13-启动流程与内存布局)
- [第二部分：外设与驱动](#第二部分外设与驱动)
  - [2.1 低速外设](#21-低速外设)
  - [2.2 高速外设](#22-高速外设)
  - [2.3 特殊外设](#23-特殊外设)
- [第三部分：实时操作系统](#第三部分实时操作系统)
  - [3.1 FreeRTOS核心机制](#31-freertos核心机制)
  - [3.2 任务管理与调度](#32-任务管理与调度)
  - [3.3 内存管理](#33-内存管理)
  - [3.4 任务间通信](#34-任务间通信)
  - [3.5 其他RTOS简介](#35-其他rtos简介)
- [第四部分：国产MCU专题](#第四部分国产mcu专题)
  - [4.1 GD32系列](#41-gd32系列)
  - [4.2 CH32系列](#42-ch32系列)
  - [4.3 AT32系列](#43-at32系列)
- [第五部分：面试题精讲](#第五部分面试题精讲)
  - [5.1 内核与架构类](#51-内核与架构类)
  - [5.2 外设与驱动类](#52-外设与驱动类)
  - [5.3 RTOS类](#53-rtos类)
  - [5.4 通信协议类](#54-通信协议类)
  - [5.5 综合场景题](#55-综合场景题)
  - [5.6 手撕代码](#56-手撕代码)
- [第六部分：综合实战](#第六部分综合实战)
  - [6.1 常见项目问题](#61-常见项目问题)
  - [6.2 性能优化](#62-性能优化)
  - [6.3 可靠性设计](#63-可靠性设计)
  - [6.4 开发工具与调试](#64-开发工具与调试)

---

# 第一部分：核心基础知识

## 1.1 ARM Cortex-M内核架构

### 1.1.1 Cortex-M系列架构差异

| 型号 | 架构版本 | 指令集 | MPU | FPU | DSP | 典型产品 |
|------|----------|--------|-----|-----|-----|----------|
| M0   | v6-M     | Thumb  | 可选 | 无  | 无  | STM32F0  |
| M0+  | v6-M     | Thumb  | 有  | 无  | 无  | STM32L0  |
| M3   | v7-M     | Thumb-2| 有  | 无  | 无  | STM32F1  |
| M4   | v7-M     | Thumb-2| 有  | 可选| 有  | STM32F4  |
| M7   | v7-M     | Thumb-2| 有  | 有  | 有  | STM32H7  |
| M23  | v8-M     | Thumb-2| 有  | 可选| 可选| STM32L5  |
| M33  | v8-M     | Thumb-2| 有  | 可选| 有  | STM32U5  |
| M55  | v8.1-M   | Thumb-2| 有  | 有  | 有  | Ethos-U55|

**关键区别：**
- **M0/M0+**: 最低功耗，最小尺寸，适合超低成本产品
- **M3**: 经典内核，性能均衡，生态丰富
- **M4**: 增加DSP指令，适合信号处理
- **M7**: 最高性能，支持Cache，适合高性能应用
- **M33/M55**: 支持TrustZone安全扩展

### 1.1.2 寄存器组

#### 通用寄存器 (R0-R12)
- **R0-R7**: 低寄存器，所有指令均可访问
- **R8-R12**: 高寄存器，部分Thumb指令有限制

#### 特殊寄存器
| 寄存器 | 名称 | 用途 |
|--------|------|------|
| SP | 堆栈指针 | 主堆栈(MSP)/进程堆栈(PSP) |
| LR | 连接寄存器 | 函数返回地址 |
| PC | 程序计数器 | 下一条指令地址 |
| xPSR | 程序状态寄存器 | 标志位/异常编号/Thumb状态 |

#### 控制寄存器 (CONTROL)
- **CONTROL[0]**: 0=特权级，1=用户级
- **CONTROL[1]**: 0=MSP，1=PSP

### 1.1.3 运行模式与特权等级

#### 运行模式
- **Thread Mode**: 正常程序执行模式
- **Handler Mode**: 异常服务程序模式

#### 特权等级
- **特权级 (Privileged)**: 可访问所有资源
- **用户级 (Unprivileged)**: 有限资源访问，受MPU限制

### 1.1.4 NVIC中断控制器

#### 特性
- 最多支持256个中断
- 16个优先级可配置
- 向量表自动取址
- 支持嵌套和分组

#### 优先级分组
```
IPR[0-67] = 32位寄存器，每8位一个中断优先级
例如：STM32F103使用 bits[7:4] 共4位 = 16级优先级
```

### 1.1.5 SysTick定时器

24位递减计数器，系统节拍时钟。

```c
// SysTick配置示例
void SysTick_Init(uint32_t ticks) {
    SysTick->CTRL = 0;                      // 关闭SysTick
    SysTick->LOAD = ticks - 1;              // 重装载值
    SysTick->VAL = 0;                       // 当前值清零
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |  // 时钟源
                    SysTick_CTRL_TICKINT_Msk |    // 开启中断
                    SysTick_CTRL_ENABLE_Msk;      // 开启SysTick
}
```

### 1.1.6 MPU内存保护单元

#### 典型配置
- 8/16个子区域
- 支持背景区域
- 访问权限：特权/用户 × 读/写/执行

```c
// MPU配置示例
void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};
    HAL_MPU_Disable();
    // 配置Flash区域: 只读，不可执行
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x08000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RO_URO;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

#### MPU属性详解：Shareable和Bufferable

##### XN (Execute Never) - 禁止执行
- **含义**: 该区域禁止执行指令
- **作用**: 防止代码从数据区域执行，防止缓冲区溢出攻击
- **典型应用**: 配置Flash和RAM为XN，防止恶意代码注入

##### Shareable (共享属性)

| 值 | 含义 | 应用场景 |
|-----|------|----------|
| 0 - Non-shareable | 非共享 | 单核系统，或多核间不使用共享内存 |
| 1 - Shareable | 可共享 | 多核系统共享内存，外设寄存器 |

**具体功能**:
- **对共享内存的影响**:
  - Shareable内存区域被所有处理器"可见"
  - 处理器间访问共享内存时，硬件会自动维护一致性
  - 在多核系统中，避免数据不一致问题

- **典型使用场景**:
  - 多核系统中的共享内存区域
  - 外设寄存器区域（需要一致性地读写）
  - RTOS中的共享数据结构

- **注意事项**:
  - 访问Non-shareable区域性能更好（无一致性开销）
  - 在单核系统中，配置为Non-shareable即可

##### Bufferable (缓冲属性)

| 值 | 含义 | 影响 |
|-----|------|------|
| 0 - Non-bufferable | 不可缓冲 | 写入直接到达目标地址 |
| 1 - Bufferable | 可缓冲 | 写入可以暂存于缓冲器 |

**具体功能**:
- **Non-bufferable**:
  - 访问直接发送到目标
  - 适合外设寄存器（必须立即写入）
  - 适合需要严格内存顺序的场景

- **Bufferable**:
  - 写入可以先存于写缓冲器，稍后写入目标
  - 提高写入性能
  - **注意**: 可能导致读取到旧数据，需使用DMB/DSB保证顺序

**典型配置示例**:
```c
// 外设区域配置 (不可缓冲)
MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;  // 外设必须立即写入

// SRAM区域配置 (可缓冲，提高性能)
MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

// 共享内存区域配置
MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
```

##### TEX (Type Extension) - 类型扩展

| TEX | 含义 | 典型应用 |
|-----|------|----------|
| 000 | Strongly Ordered / Device | 外设内存 |
| 001 | Normal Memory | 普通内存 |
| 010 | Device | 设备内存 |
| 111 | Normal Memory | 可缓存内存 |

##### Cacheable (缓存属性)

| 值 | 含义 |
|-----|------|
| 0 - Non-cacheable | 不可缓存 |
| 1 - Cacheable | 可缓存 |

**组合使用场景**:
```
Normal Memory (TEX=000, C=1, B=0): Write-through, no allocate
Normal Memory (TEX=000, C=1, B=1): Write-back, no allocate
Normal Memory (TEX=001, C=1, B=1): Write-back, read/write allocate
Device Memory (TEX=100, C=0, B=1): Bufferable device
```

#### MPU配置最佳实践

```c
void MPU_Config_Comprehensive(void) {
    HAL_MPU_Disable();

    // 1. 配置Flash区域: 只读，可缓存，不可执行
    MPU_Region_InitTypeDef MPU_InitStruct = {0};
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x08000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RO_URO;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 2. 配置SRAM区域: 读写，可缓存，不可执行
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x20000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 3. 配置外设区域: 不可缓存，不可缓冲
    MPU_InitStruct.Number = MPU_REGION_NUMBER2;
    MPU_InitStruct.BaseAddress = 0x40000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;
    MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW_USR_RO;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 1.1.7 FPU浮点单元

#### M4/M7 FPU特性
- 单精度浮点运算 (Cortex-M4: FPv4-SP, M7: FPv5-SP)
- 32个S0-S31单精度寄存器
- 可选双精度支持 (M7)

```c
// 开启FPU
void FPU_Init(void) {
    SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  // CP10/CP11全访问
    __DSB();
    __ISB();
}
```

### 1.1.8 TrustZone安全扩展 (M23/M33/M55)

#### 安全状态
- **Secure**: 安全世界，可访问所有资源
- **Non-Secure**: 非安全世界，受限访问

#### 内存划分
```
地址空间划分为Secure/Non-Secure区域
通过SAU/IDAU配置安全属性
```

### 常见面试题

1. **Cortex-M3和M4的区别是什么？**
   - M4增加DSP指令集
   - M4可选FPU
   - M4支持SIMD操作

2. **什么是特权级和用户级？有什么区别？**
   - 特权级可访问所有资源
   - 用户级受MPU限制，无法访问某些寄存器

3. **NVIC中断优先级如何配置？**
   - 通过IPR寄存器配置
   - 数值越小优先级越高
   - 需要设置优先级分组

4. **为什么SysTick常用于RTOS系统节拍？**
   - 简单易用，ARM内核自带
   - 可产生固定频率中断
   - 与处理器架构紧耦合

5. **请详细解释Cortex-M的异常处理流程？**

   **答：**
   - 异常产生时，CPU自动将xPSR、PC、LR、R12、R0-R3压入当前栈（MSP或PSP）
   - 加载向量表中的异常处理函数地址到PC
   - 更新LR寄存器为特殊的EXC_RETURN值（标记返回模式）
   - 切换到Handler模式，使用MSP堆栈

   **异常返回流程：**
   - 执行BX LR或其他返回指令
   - 根据EXC_RETURN恢复PSR、PC、堆栈
   - 从中断处继续执行

6. **CONTROL寄存器的作用是什么？如何切换堆栈？**

   **答：**
   CONTROL寄存器包含：
   - **CONTROL[0]**: 特权级选择
     - 0: 特权级
     - 1: 用户级
   - **CONTROL[1]**: 堆栈选择
     - 0: 使用MSP（主堆栈）
     - 1: 使用PSP（进程堆栈）

   **切换到用户级并使用PSP：**
   ```asm
   ; 初始状态在特权级，使用MSP
   MOVS R0, #0x03    ; CONTROL = 0b11
   MSR CONTROL, R0   ; 切换到用户级，使用PSP
   ```

7. **什么是MPU？它如何保护内存？**

   **答：**
   MPU (Memory Protection Unit) 内存保护单元：
   - 将内存划分为多个区域（通常8或16个）
   - 每个区域可独立设置访问权限
   - 支持子区域禁用（将大区域划分为8个子区域）
   - 支持背景区域（对未配置区域进行保护）

   **访问权限：**
   - 特权级读/写/执行权限
   - 用户级读/写/执行权限
   - XN (Execute Never) 禁止执行

8. **FPU如何开启？浮点运算需要注意什么？**

   **答：**
   开启FPU：
   ```c
   void FPU_Init(void) {
       // CPACR寄存器使能FPU
       SCB->CPACR |= ((3UL << 10*2) | (3UL << 11*2));
       __DSB();  // 数据同步屏障
       __ISB();  // 指令同步屏障
   }
   ```

   **注意事项：**
   - 使用浮点运算时需要足够大的栈空间
   - 中断服务程序中使用FPU需要保存S0-S15, FPSCR寄存器
   - 配置编译器浮点ABI设置

9. **Cortex-M的向量表可以重定位吗？如何实现？**

   **答：**
   可以重定位，通过SCB->VTOR（Vector Table Offset Register）寄存器：

   ```c
   // 重定位向量表到SRAM
   // SRAM起始地址0x20000000
   SCB->VTOR = 0x20000000;

   // 或重定位到Flash其他位置
   // 例如偏移0x10000
   SCB->VTOR = 0x08000000 | 0x10000;
   ```

   **应用场景：**
   - Bootloader：从SRAM启动
   - 固件升级：切换到新程序
   - 多程序加载

10. **详细说明中断嵌套的条件和规则？**

    **答：**
    **中断嵌套条件：**
    - 新中断的优先级必须比当前中断更高（数值更小）
    - 需要使能全局中断（PRIMASK = 0）
    - 需要设置优先级分组

    **嵌套规则：**
    - 高优先级中断可以抢占低优先级中断
    - 同优先级中断不能嵌套
    - 中断嵌套深度受限于堆栈大小
    - 复位、NMI、HardFault不可嵌套

    **中断响应过程：**
    1. 响应中断，保存现场
    2. 切换到对应优先级
    3. 执行中断服务程序
    4. 恢复现场，返回

11. **MPU的Shareable和Bufferable属性有什么区别？**

    **答：**
    **Shareable（共享属性）：**
    - Shareable: 内存区域可被多个处理器共享
    - Non-shareable: 内存区域仅被单个处理器使用
    - 多核系统中，Shareable区域需要硬件维护一致性

    **Bufferable（缓冲属性）：**
    - Bufferable: 写入可以存入缓冲器，延迟写入
    - Non-bufferable: 写入立即到达目标
    - 外设寄存器必须使用Non-bufferable

12. **Cortex-M启动时，.data和.bss段是如何初始化的？**

    **答：**
    **初始化流程：**
    1. **.data段复制：**
       - 位置：Flash → SRAM
       - 内容：已初始化的全局/静态变量
       - 代码：启动汇编中循环复制

    2. **.bss段清零：**
       - 位置：SRAM
       - 内容：未初始化的全局/静态变量
       - 代码：启动汇编中循环清零

    **示例代码：**
    ```asm
    ; 复制.data
    LDR R0, =_sidata    ; Flash源地址
    LDR R1, =_sdata     ; SRAM目标起始
    LDR R2, =_edata     ; SRAM目标结束
    ; 循环复制...

    ; 清零.bss
    LDR R0, =_sbss      ; BSS起始
    LDR R1, =_ebss      ; BSS结束
    ; 循环清零...
    ```

---

## 1.2 汇编语言与寄存器

### 1.2.1 Thumb/Thumb-2指令集

#### Thumb指令特点
- 16位指令长度，节省存储空间
- Thumb-2: 16/32位混合，保持兼容性

### 1.2.2 常用汇编指令

#### 数据传送
```asm
MOVS R0, #0x10     ; 立即数到寄存器
LDR  R0, [R1]      ; 加载内存数据
STR  R0, [R1]      ; 存储数据到内存
LDRB / STRB        ; 字节操作
LDRH / STRH        ; 半字操作
```

#### 算术运算
```asm
ADD  R0, R1, R2    ; R0 = R1 + R2
ADDS R0, R0, #1    ; 带标志位加法
SUB  R0, R1, R2    ; R0 = R1 - R2
MUL  R0, R1, R2    ; R0 = R1 * R2
```

#### 逻辑运算
```asm
AND  R0, R0, #0x0F ; 逻辑与
ORR  R0, R0, #0x01; 逻辑或
EOR  R0, R0, #0x01; 异或
LSL  R0, R0, #1    ; 逻辑左移
LSR  R0, R0, #1    ; 逻辑右移
```

#### 跳转指令
```asm
B    label         ; 无条件跳转
BEQ  label         ; 相等跳转
BNE  label         ; 不相等跳转
BL   func          ; 跳转并保存返回地址
BX   LR            ; 返回
```

### 1.2.3 MRS/MSR特殊寄存器访问

```asm
; 读取特殊寄存器
MRS  R0, PRIMASK   ; 读取PRIMASK到R0
MRS  R0, CONTROL   ; 读取CONTROL到R0

; 写入特殊寄存器
MSR  PRIMASK, R0   ; 写入PRIMASK
MSR  CONTROL, R0   ; 写入CONTROL
```

### 1.2.4 CPSIE/CPSID开关中断

```asm
CPSIE I            ; 开启中断 (PRIMASK=0)
CPSID I            ; 关闭中断 (PRIMASK=1)
CPSIE F            ; 开启浮点异常
CPSID F            ; 关闭浮点异常
```

### 1.2.5 DMB/DSB/ISB内存屏障指令

#### DMB (Data Memory Barrier)
- 确保所有内存访问在屏障前完成

#### DSB (Data Synchronization Barrier)
- 等待所有内存访问完成

#### ISB (Instruction Synchronization Barrier)
- 刷新流水线，重新取指

```asm
; 典型使用场景：外设操作后
LDR  R0, [R1]
DSB             ; 确保内存访问完成
; 后续操作...
```

### 1.2.6 汇编与C混合编程

#### 内联汇编
```c
__asm void func(void) {
    PUSH {R0, R1, LR}
    // C代码中直接写汇编
    POP {R0, R1, PC}
}
```

#### 独立汇编文件
```c
// C声明
extern void func(void);

// 汇编实现
EXPORT func
func PROC
    ; 函数体
    ENDP
```

### 常见面试题

1. **请说明DMB、DSB、ISB的区别**
   - DMB: 数据内存屏障，确保访存顺序
   - DSB: 数据同步屏障，等待访存完成
   - ISB: 指令同步屏障，刷新流水线

2. **如何原子性地开启/关闭全局中断？**
   - 使用CPSID/CPSIE指令
   - 或操作PRIMASK寄存器

---

## 1.3 启动流程与内存布局

### 1.3.1 Vector Table向量表结构

| 偏移 | 类型 | 描述 |
|------|------|------|
| 0x000 | MSP | 主堆栈指针初始值 |
| 0x004 | Reset | 复位向量 |
| 0x008 | NMI | 不可屏蔽中断 |
| 0x00C | HardFault | 硬件 fault |
| ... | ... | 其他异常/中断 |

```c
// 典型的向量表定义
__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))((uint32_t) &_estack),  // MSP
    Reset_Handler,                           // Reset
    NMI_Handler,                             // NMI
    HardFault_Handler,                       // HardFault
    // ...
};
```

### 1.3.2 启动模式

| 模式 | BOOT0 | BOOT1 | 启动地址 |
|------|-------|-------|----------|
| Flash | 0 | 0 | 0x08000000 |
| System Memory | 1 | 0 | 0x1FFF0000 (STM32) |
| SRAM | x | 1 | 0x20000000 |

### 1.3.3 内存段

#### .isr_vector
- 向量表
- 需要放在Flash起始位置

#### .data
- 已初始化全局变量
- 从Flash复制到SRAM

#### .bss
- 未初始化全局变量
- 启动时清零

#### .heap / .stack
- 堆和栈空间

### 1.3.4 SystemInit()与启动文件

```c
// SystemInit() 典型实现
void SystemInit(void) {
    // 配置向量表偏移
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
    // 配置Flash等待周期
    // 配置时钟...
}
```

### 1.3.5 链接脚本 (Linker Script)

```ld
/* STM32典型链接脚本 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        *(.text)
        *(.text*)
    } >FLASH

    .data :
    {
        _sdata = .;
        *(.data)
        _edata = .;
    } >RAM AT>FLASH

    .bss :
    {
        _sbss = .;
        *(.bss)
        *(COMMON)
        _ebss = .;
    } >RAM
}
```

### 1.3.6 Cortex-M启动汇编主要工作

Cortex-M的启动汇编文件（`startup.s`或`startup_stm32.s`）是系统上电后首先执行的代码，主要完成以下工作：

#### 启动汇编的核心任务

```
上电复位
    │
    ▼
┌─────────────────────┐
│ 1. 设置堆栈指针     │ ← 从向量表第0项获取MSP
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ 2. 跳转到Reset_Handler│ ← 从向量表第1项获取Reset向量
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ 3. 执行初始化代码    │
│   - 复制 .data      │
│   - 清零 .bss       │
│   - 配置堆栈        │
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ 4. 调用SystemInit() │ ← C语言初始化函数
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ 5. 跳转__main       │ ← C运行时库
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ 6. 执行main()       │ ← 用户程序入口
└─────────────────────┘
```

#### 启动汇编详细分析

```asm
;===============================================================================
; STM32启动文件典型结构
;===============================================================================

; 文件: startup_stm32f103xe.s

; -----------------------------------------------------------------------------
; 1. 定义向量表
; -----------------------------------------------------------------------------
    SECTION .isr_vector : DATA
    EXPORT __Vectors
    EXPORT __Vectors_End
    EXPORT __Vectors_Size

__Vectors:
    ; 向量表第0项: 主堆栈指针 (MSP)
    DCD     __initial_sp              ; Top of Stack

    ; 向量表第1项: 复位向量
    DCD     Reset_Handler              ; Reset Handler

    ; 向量表第2项: NMI Handler
    DCD     NMI_Handler                ; NMI Handler

    ; 向量表第3项: HardFault Handler
    DCD     HardFault_Handler         ; Hard Fault Handler

    ; ... 其他异常向量 ...
    DCD     WWDG_IRQHandler           ; Window Watchdog
    DCD     PVD_IRQHandler            ; PVD through EXTI Line detection
    DCD     RTC_IRQHandler            ; RTC through EXTI Line
    ; ... 更多中断向量 ...
__Vectors_End:

__Vectors_Size  EQU  __Vectors_End - __Vectors

; -----------------------------------------------------------------------------
; 2. 复位处理函数
; -----------------------------------------------------------------------------
    SECTION .text : CODE : REORDER (2)

    ; 导出复位处理函数
    EXPORT  Reset_Handler
    EXPORT  __Vectors

Reset_Handler:
    ; -----------------------------
    ; 2.1 禁用中断
    ; -----------------------------
    CPSID   I                         ; 关闭全局中断
                                        ; 防止初始化过程被中断打扰

    ; -----------------------------
    ; 2.2 数据初始化
    ; -----------------------------
    ; 复制 .data 段 (从Flash到SRAM)
    ; .data: 已初始化的全局/静态变量

    LDR     R0, = _sidata            ; .data 源地址 (Flash)
    LDR     R1, = _sdata             ; .data 目标起始地址 (SRAM)
    LDR     R2, = _edata             ; .data 目标结束地址 (SRAM)

    MOVS    R3, #0                    ; 初始化 R3 = 0
    B       LoopCopyDataInit

CopyDataInit:
    LDR     R3, [R0, R3]              ; 从Flash读取数据
    STR     R3, [R1, R3]              ; 写入SRAM
    ADDS    R3, R3, #4                ; R3 += 4

LoopCopyDataInit:
    ADDS    R4, R1, R3                ; 当前目标地址
    CMP     R4, R2                    ; 比较是否到达结束地址
    BCC     CopyDataInit              ; 如果未到达，继续复制

    ; -----------------------------
    ; 2.3 BSS清零
    ; -----------------------------
    ; 清零 .bss 段
    ; .bss: 未初始化的全局/静态变量

    LDR     R2, = _sbss              ; BSS 起始地址
    LDR     R4, = _ebss               ; BSS 结束地址
    MOVS    R3, #0                    ; R3 = 0
    B       LoopFillZerobss

FillZerobss:
    STR     R3, [R2], #4              ; 写入0到BSS区域

LoopFillZerobss:
    CMP     R2, R4                    ; 比较是否到达结束地址
    BCC     FillZerobss               ; 如果未到达，继续清零

IF      :DEF:__MICROLIB               ; 如果使用微库

    EXPORT  __initial_sp              ; 导出堆栈指针
    EXPORT  __heap_base
    EXPORT  __heap_limit

ELSE                                    ; 使用标准库

    ; -----------------------------
    ; 2.4 堆栈初始化
    ; -----------------------------
    ; 分配堆和栈空间

    IMPORT  __use_two_region_memory

    EXPORT  __user_initial_stackheap

__user_initial_stackheap:

    LDR     R0, =  Heap_Mem           ; 堆起始地址
    LDR     R1, = (Stack_Mem + Stack_Size)  ; 栈顶地址
    LDR     R2, = (Heap_Mem + Heap_Size)    ; 堆结束地址
    LDR     R3, = Stack_Mem            ; 栈底地址

    BX      LR

ENDIF

    ; -----------------------------
    ; 2.5 开启中断
    ; -----------------------------
    CPSIE   I                          ; 开启全局中断

    ; -----------------------------
    ; 2.6 调用SystemInit()
    ; -----------------------------
    IMPORT  SystemInit
    LDR     R0, = SystemInit
    BLX     R0                        ; 调用SystemInit()

    ; -----------------------------
    ; 2.7 跳转到__main
    ; -----------------------------
    ; __main 是C运行时库的入口，会:
    ; - 初始化C库
    ; - 调用全局构造函数
    ; - 最终跳转到main()

    IMPORT  __main
    LDR     R0, = __main
    BX      R0                        ; 跳转到__main

    ; -----------------------------
    ; 2.8 死循环 (如果__main返回)
    ; -----------------------------
LoopForever:
    B       LoopForever

; -----------------------------------------------------------------------------
; 3. 默认异常处理函数
; -----------------------------------------------------------------------------
NMI_Handler     PROC
                EXPORT  NMI_Handler            [WEAK]
                B       .
                ENDP

HardFault_Handler PROC
                EXPORT  HardFault_Handler       [WEAK]
                B       .
                ENDP

; ... 其他异常处理函数类似 ...

; -----------------------------------------------------------------------------
; 4. 弱定义中断处理函数
; -----------------------------------------------------------------------------
; 用户可以在C文件中重新定义这些函数
WWDG_IRQHandler PROC
                EXPORT  WWDG_IRQHandler          [WEAK]
                B       .
                ENDP

; ... 其他中断处理函数类似 ...

    END
```

#### 启动过程详解

**1. 向量表加载**
```
地址0x00000000: MSP = __initial_sp    ; 堆栈指针
地址0x00000004: Reset_Handler          ; 复位向量
地址0x00000008: NMI_Handler            ; NMI中断
...
```

**2. .data段复制**
- **位置**: Flash (只读) → SRAM (读写)
- **内容**: 已初始化的全局变量和静态变量
- **示例**:
```c
// 这些变量在Flash中有初始值，需要复制到SRAM
int g_value = 100;      // .data段
const char *str = "hello";  // .data段 (指针本身)
```

**3. .bss段清零**
- **位置**: SRAM
- **内容**: 未初始化的全局变量和静态变量
- **示例**:
```c
// 这些变量在SRAM中但需要清零
int g_buffer[1024];     // .bss段
static int s_counter;   // .bss段
```
- **作用**: 确保未初始化变量的默认值是0

**4. 堆栈初始化**
```
    ┌─────────────────┐ 高地址
    │      栈        │ 向下增长
    │    (Stack)     │
    ├─────────────────┤
    │                │
    │      堆        │ 向上增长
    │    (Heap)      │
    ├─────────────────┤
    │     .bss       │
    ├─────────────────┤
    │     .data      │
    ├─────────────────┤
    │   Vector Table │ 低地址
    └─────────────────┘
```

**5. SystemInit()**
- 配置Flash等待周期
- 设置向量表偏移
- 配置时钟系统
- 初始化FPU (如果存在)

**6. __main vs main**
- `__main`: C运行时库入口，负责:
  - 库初始化
  - 全局对象构造函数调用 (`__libc_init_array`)
  - 最终跳转到 `main()`

#### 常见面试题

1. **STM32上电后的启动流程是什么？**
   - 上电复位 → SystemInit() → __main → main()

2. **向量表可以重定位吗？如何实现？**
   - 可以，通过SCB->VTOR寄存器设置偏移量

3. **为什么中断向量表要4字节对齐？**
   - Thumb指令要求最低位为1表示Thumb模式
   - 实际存储时Thumb模式地址已是4字节对齐

---

# 第二部分：外设与驱动

## 2.1 低速外设

### 2.1.1 GPIO/EXTI

#### GPIO工作模式

| 模式 | 方向 | 特点 |
|------|------|------|
| 输入浮空 | I | 高阻抗状态 |
| 输入上拉 | I | 内部接VDD |
| 输入下拉 | I | 内部接GND |
| 推挽输出 | O | 强高低电平 |
| 开漏输出 | O | 需要上拉 |
| 复用推挽 | O | 外设控制 |
| 复用开漏 | O | 外设控制 |

#### GPIO寄存器 (STM32F1为例)

```c
typedef struct {
    __IO uint32_t CRL;   // 控制寄存器低
    __IO uint32_t CRH;   // 控制寄存器高
    __IO uint32_t IDR;   // 输入数据
    __IO uint32_t ODR;   // 输出数据
    __IO uint32_t BSRR;  // 置位/复位
    __IO uint32_t BRR;   // 复位
    __IO uint32_t LCKR;  // 锁定
} GPIO_TypeDef;
```

#### GPIO配置示例

```c
void GPIO_Init(void) {
    // 配置PA5为推挽输出
    GPIOA->CRL &= ~(0xF << (5 * 4));  // 清零
    GPIOA->CRL |= (0x3 << (5 * 4));  // 推挽输出, 50MHz
}
```

#### EXTI触发方式

| 触发方式 | 描述 |
|----------|------|
| 上升沿 | 从低到高触发 |
| 下降沿 | 从高到低触发 |
| 双边沿 | 上升沿和下降沿都触发 |
| 电平 | 保持触发 |

#### EXTI配置

```c
void EXTI_Init(void) {
    // 配置PA0为外部中断0，上升沿触发
    EXTI->IMR |= EXTI_IMR_MR0;      // 开启中断
    EXTI->RTSR |= EXTI_RTSR_TR0;   // 上升沿使能
    NVIC_EnableIRQ(EXTI0_IRQn);    // NVIC使能
}
```

#### EXTI事件与中断的区别

- **中断**: 触发后进入中断服务程序
- **事件**: 触发后产生脉冲信号，可触发其他外设 (如DMA)

### 2.1.2 复位与时钟(RCC)

#### 复位类型

| 复位类型 | 触发条件 |
|----------|----------|
| Power Reset | 上电/掉电 |
| Soft Reset | 软件触发 (AIRCR) |
| IWDG Reset | 独立看门狗 |
| WWDG Reset | 窗口看门狗 |
| LOCKUP Reset | 内核错误 |

#### 时钟源

| 时钟源 | 频率 | 精度 | 用途 |
|--------|------|------|------|
| HSI | 8MHz | ±1% | 备用/系统时钟 |
| HSE | 4-16MHz | 高 | 主时钟源 |
| LSI | 40kHz | 低 | IWDG/RTC |
| LSE | 32.768kHz | 高 | RTC |
| PLL | 可变 | - | 系统时钟倍频 |

#### PLL配置

```c
void RCC_PLL_Config(uint32_t pllm, uint32_t plln, uint32_t pllp) {
    RCC->CR |= RCC_CR_HSEON;           // 开启HSE
    while(!(RCC->CR & RCC_CR_HSERDY));// 等待HSE就绪

    RCC->CFGR |= RCC_CFGR_PLLSRC;     // HSE作为PLL输入
    RCC->CFGR |= (pllp << 17);        // PLLP分频
    RCC->CFGR |= (plln << 6);         // PLLN倍频
    RCC->CFGR |= (pllm << 0);         // PLLM分频

    RCC->CR |= RCC_CR_PLLON;          // 开启PLL
    while(!(RCC->CR & RCC_CR_PLLRDY));// 等待PLL就绪

    RCC->CFGR |= RCC_CFGR_SW_PLL;     // 切换到PLL
}
```

#### AHB/APB总线时钟

```
SYSCLK → AHB Prescaler → APB1/APB2
          (1,2,4,8,16,64,128,256,512)
```

- APB1: 最高36MHz/42MHz (F1/F4)
- APB2: 最高72MHz/84MHz

### 2.1.3 看门狗

#### IWDG独立看门狗

- 独立于系统时钟
- 12位递减计数器
- 简单可靠

```c
void IWDG_Init(uint16_t prescaler, uint16_t reload) {
    IWDG->KR = 0x5555;    // 解除写保护
    IWDG->PR = prescaler;// 预分频
    IWDG->RLR = reload;  // 重装载值
    IWDG->KR = 0xAAAA;   // 刷新计数器
}

void IWDG_Feed(void) {
    IWDG->KR = 0xAAAA;   // 喂狗
}
```

#### WWDG窗口看门狗

- 必须在窗口期内喂狗
- 可检测软件异常

```c
void WWDG_Init(uint8_t tr, uint8_t wr, uint32_t fprer) {
    WWDG->CFR = (fprer << 7) | wr;  // 窗口值和预分频
    WWDG->CR = (tr & 0x7F) | 0x80;  // 启动并设置计数器
}

void WWDG_Feed(void) {
    WWDG->CR = (WWDG->CR & 0x7F) | (0x40);  // 喂狗
}
```

### 2.1.4 RTC

#### RTC时钟源

- LSE: 32.768kHz晶体
- LSI: 40kHz RC振荡器
- HSE: 外部时钟/2^15

#### RTC配置

```c
void RTC_Init(void) {
    PWR->CR |= PWR_CR_DBP;           // 解除备份域访问
    RCC->BDCR |= RCC_BDCR_LSEON;     // 开启LSE
    while(!(RCC->BDCR & RCC_BDCR_LSERDY));

    RCC->BDCR |= RCC_BDCR_RTCSEL_LSE;// 选择LSE
    RCC->BDCR |= RCC_BDCR_RTCEN;     // 开启RTC

    RTC->WPR = 0xCA;                 // 解除写保护
    RTC->WPR = 0x53;

    RTC->ISR |= RTC_ISR_INIT;         // 进入初始化模式
    while(!(RTC->ISR & RTC_ISR_INITF));

    RTC->PRER = 0xFF;                 // 异步分频
    RTC->PRER |= (0x7F << 16);       // 同步分频

    RTC->ISR &= ~RTC_ISR_INIT;       // 退出初始化模式
    RTC->WPR = 0x00;
}
```

#### RTC闹钟/周期性唤醒

```c
void RTC_SetAlarm(uint32_t alarm_time) {
    RTC->CR &= ~RTC_CR_ALRAE;        // 关闭闹钟
    while(!(RTC->ISR & RTC_ISR_ALRAWF));

    RTC->ALRMAR = alarm_time;        // 设置闹钟时间
    RTC->CR |= RTC_CR_ALRAE;         // 开启闹钟
}
```

### 2.1.5 电源管理(PWR)

#### 调压器

- **正常模式**: 全功能
- **低功耗模式**: 降低功耗

```c
void PWR_VoltageRegulatorConfig(uint32_t voltage) {
    PWR->CR &= ~PWR_CR_VOS;
    PWR->CR |= voltage;
}
```

#### PVD可编程电压检测

```c
void PVD_Init(uint32_t threshold) {
    PWR->CR &= ~PWR_CR_PVDE;
    PWR->CR |= threshold | PWR_CR_PVDE;
    EXTI->IMR |= EXTI_IMR_MR16;
    EXTI->RTSR |= EXTI_RTSR_TR16;
    NVIC_EnableIRQ(PVD_IRQn);
}
```

### 2.1.6 低功耗模式

#### Sleep模式

```c
// 方法1: WFI (Wait For Interrupt)
__WFI();

// 方法2: WFE (Wait For Event)
__WFE();
```

#### Stop模式

```c
void Enter_StopMode(void) {
    PWR->CR &= ~PWR_CR_PDDS;        // 进入Stop模式
    PWR->CR &= ~PWR_CR_LPDS;        // 低功耗调节器
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP; // 浅睡眠
    __WFI();                         // 等待中断唤醒
}
```

#### Standby模式

```c
void Enter_StandbyMode(void) {
    PWR->CR |= PWR_CR_PDDS;         // 进入Standby模式
    PWR->CR |= PWR_CR_CWUF;         // 清除唤醒标志
    SCB->SCR |= SCB_SCR_SLEEPDEEP;  // 深睡眠
    __WFI();
}
```

#### 各级模式对比

| 模式 | 进入方式 | 唤醒源 | 功耗 |
|------|----------|--------|------|
| Sleep | WFI/WFE | 任意中断 | ~mA级 |
| Stop | 深度睡眠 | 特定外设 | ~uA级 |
| Standby | 深度睡眠 | 特定唤醒源 | ~nA级 |

---

### 2.2 高速外设

### 2.2.1 定时器

#### 定时器类型

| 类型 | 特性 | 典型型号 |
|------|------|----------|
| 基本定时器 | 6/7只有递增计数和更新中断 | TIM6/7 |
| 通用定时器 | 16位，支持输入捕获/输出比较/PWM | TIM2/3/4/5 |
| 高级定时器 | 1/8支持刹车/死区/互补输出 | TIM1/8 |
| 低功耗定时器 | 低功耗设计 | TIM2/3/5(L0) |

#### 计数模式

- **向上计数**: 0 → ARR
- **向下计数**: ARR → 0
- **中央对齐**: 0→ARR→0

#### 定时器配置示例

```c
void TIM2_Init(void) {
    // 使能定时器时钟
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // 配置预分频器 (PSC+1分频)
    TIM2->PSC = 7200 - 1;    // 10kHz计数频率

    // 配置自动重装载值
    TIM2->ARR = 10000 - 1;   // 1秒中断

    // 开启更新中断
    TIM2->DIER |= TIM_DIER_UIE;

    // 使能计数器
    TIM2->CR1 |= TIM_CR1_CEN;
}

// 中断服务程序
void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        // 处理定时任务
    }
}
```

#### PWM输出

```c
void TIM_PWM_Init(void) {
    // 配置GPIO为复用功能 (PA0为TIM2_CH1)
    GPIOA->CRL &= ~(0xF << 0);
    GPIOA->CRL |= (0xB << 0);  // 复用推挽输出, 50MHz

    // 配置PWM模式
    TIM2->CCMR1 &= ~TIM_CCMR1_CC1S;
    TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; // PWM模式1
    TIM2->CCMR1 |= TIM_CCMR1_OC1PE;  // 预装载使能

    // 设置占空比
    TIM2->CCR1 = 500;  // 50%占空比

    // 使能通道输出
    TIM2->CCER |= TIM_CCER_CC1E;

    // 使能计数器
    TIM2->CR1 |= TIM_CR1_CEN;
}
```

#### 输入捕获

```c
void TIM_IC_Init(void) {
    // 配置GPIO (PA0为TIM2_CH1)
    GPIOA->CRL &= ~(0xF << 0);
    GPIOA->CRL |= (0x1 << 0);  // 浮空输入

    // 配置输入捕获
    TIM2->CCMR1 |= TIM_CCMR1_CC1S_0;  // 映射到TI1
    TIM2->CCMR1 |= TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_1; // 滤波

    // 开启捕获中断
    TIM2->DIER |= TIM_DIER_CC1IE;

    // 使能通道
    TIM2->CCER |= TIM_CCER_CC1E;
}
```

#### 编码器接口

```c
void TIM_Encoder_Init(void) {
    // 配置GPIO (TI1=TPA6, TI2=TPA7)
    GPIOA->CRL &= ~(0xFF << 24);
    GPIOA->CRL |= (0x11 << 24);  // 浮空输入

    // 配置编码器模式
    TIM3->SMCR &= ~TIM_SMCR_SMS;
    TIM3->SMCR |= TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1; // 编码器模式3

    // 配置滤波
    TIM3->CCMR1 |= TIM_CCMR1_IC1F | TIM_CCMR1_IC2F;

    // 设置预分频器
    TIM3->PSC = 0;

    // 设置ARR
    TIM3->ARR = 0xFFFF;

    // 使能计数器
    TIM3->CR1 |= TIM_CR1_CEN;
}
```

### 2.2.2 DMA控制器

#### DMA特性

- 12个通道 (F1) / 16个通道 (F4)
- 独立于CPU传输数据
- 支持外设到内存、内存到外设、内存到内存

#### DMA配置

```c
void DMA_USART_Init(uint32_t srcAddr, uint32_t dstAddr, uint16_t size) {
    // 使能DMA1时钟
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // 失能DMA通道
    DMA1_Channel4->CCR &= ~DMA_CCR_EN;

    // 设置外设地址
    DMA1_Channel4->CPAR = srcAddr;

    // 设置内存地址
    DMA1_Channel4->CMAR = dstAddr;

    // 设置传输数量
    DMA1_Channel4->CNDTR = size;

    // 配置寄存器
    DMA1_Channel4->CCR |= DMA_CCR_MINC;    // 内存增量
    DMA1_Channel4->CCR |= DMA_CCR_PSIZE_0; // 外设8位
    DMA1_Channel4->CCR |= DMA_CCR_MSIZE_0; // 内存8位
    DMA1_Channel4->CCR |= DMA_CCR_DIR;     // 内存到外设

    // 使能传输完成中断
    DMA1_Channel4->CCR |= DMA_CCR_TCIE;

    // 使能DMA通道
    DMA1_Channel4->CCR |= DMA_CCR_EN;
}
```

#### DMA传输模式

| 模式 | 描述 |
|------|------|
| 普通模式 | 传输完成后停止 |
| 循环模式 | 自动重装载，循环传输 |
| 双缓冲 | 两个缓冲区交替使用 |

### 2.2.3 ADC/DAC

#### ADC特性

- SAR (逐次逼近) 型ADC
- 12位分辨率 (STM32F1/F4)
- 最多18个通道 (16外部+2内部)
- 支持扫描模式/连续转换

#### ADC配置

```c
void ADC_Init(void) {
    // 使能ADC时钟
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    // 使能GPIO时钟
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // 配置GPIO为模拟输入 (PA0)
    GPIOA->CRL &= ~(0xF << 0);

    // 校准ADC
    ADC1->CR2 |= ADC_CR2_CAL;
    while(ADC1->CR2 & ADC_CR2_CAL);

    // 配置采样时间 (239.5周期)
    ADC1->SMPR2 |= ADC_SMPR2_SMP0;

    // 配置规则通道
    ADC1->SQR3 = 0;  // 第一个转换通道

    // 使能ADC
    ADC1->CR2 |= ADC_CR2_ADON;
}

// 启动转换并读取
uint16_t ADC_GetValue(void) {
    ADC1->CR2 |= ADC_CR2_SWSTART;  // 启动转换
    while(!(ADC1->SR & ADC_SR_EOC)); // 等待转换完成
    return ADC1->DR;
}
```

#### DAC配置

```c
void DAC_Init(void) {
    // 使能DAC时钟
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;

    // 使能GPIO (PA4为DAC1输出)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~(0xF << 16);
    GPIOA->CRL |= (0x3 << 16);  // 模拟输入

    // 使能DAC通道
    DAC->CR |= DAC_CR_EN1;
}

// 输出电压
void DAC_SetValue(uint16_t value) {
    DAC->DHR12R1 = value;
}
```

### 2.2.4 通信外设

#### USART/UART

```c
void USART_Init(uint32_t baudrate) {
    // 使能时钟
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // 配置GPIO (PA9=TX, PA10=RX)
    GPIOA->CRH &= ~(0xFF0);
    GPIOA->CRH |= (0x4B0);  // TX推挽, RX浮空

    // 配置USART
    USART1->BRR = 72000000 / baudrate;  // 72MHz时钟
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;  // 发送/接收使能
    USART1->CR1 |= USART_CR1_RXNEIE;  // 接收中断使能

    // 使能USART
    USART1->CR1 |= USART_CR1_UE;
}

// 发送字符
void USART_SendChar(char c) {
    while(!(USART1->SR & USART_SR_TXE));
    USART1->DR = c;
}
```

#### SPI

```c
void SPI_Init(void) {
    // 使能时钟
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    // 配置GPIO (PA5=SCK, PA6=MISO, PA7=MOSI)
    GPIOA->CRL &= ~(0xFFF0);
    GPIOA->CRL |= (0xB4B0);

    // 配置SPI
    SPI1->CR1 |= SPI_CR1_MSTR;     // 主模式
    SPI1->CR1 |= SPI_CR1_BR_0;     // 8分频
    SPI1->CR1 |= SPI_CR1_CPOL;    // 时钟极性
    SPI1->CR1 |= SPI_CR1_CPHA;    // 时钟相位

    // 使能SPI
    SPI1->CR1 |= SPI_CR1_SPE;
}

// SPI发送/接收
uint8_t SPI_SendRecv(uint8_t data) {
    while(!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = data;
    while(!(SPI1->SR & SPI_SR_RXNE));
    return SPI1->DR;
}
```

#### I2C

```c
void I2C_Init(void) {
    // 使能时钟
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // 配置GPIO (PB6=SCL, PB7=SDA)
    GPIOB->CRL &= ~(0xFF00);
    GPIOB->CRL |= (0xFF00);  // 复用开漏

    // 配置I2C
    I2C1->CR2 = 36;              // 36MHz APB1
    I2C1->CCR = 180;             // 100kHz
    I2C1->TRISE = 37;
    I2C1->CR1 |= I2C_CR1_PE;    // 使能
}

// I2C写数据
void I2C_Write(uint8_t addr, uint8_t reg, uint8_t data) {
    I2C1->CR1 |= I2C_CR1_START;  // 发送START
    while(!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = addr << 1;        // 发送从机地址(写)
    while(!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;

    I2C1->DR = reg;              // 发送寄存器地址
    while(!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->DR = data;             // 发送数据
    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;  // 发送STOP
}
```

### 2.2.5 CAN总线

```c
void CAN_Init(void) {
    // 使能时钟
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    // 初始化CAN
    CAN1->MCR |= CAN_MCR_INRQ;  // 进入初始化模式
    while(!(CAN1->MSR & CAN_MSR_INAK));

    // 配置波特率 (500kbps)
    CAN1->BTR = (3 << 24) | (12 << 16) | 5; // SJW, TS1, TS2, BRP

    CAN1->MCR &= ~CAN_MCR_INRQ;  // 退出初始化模式
    while(CAN1->MSR & CAN_MSR_INAK);
}

// 发送CAN帧
uint8_t CAN_Send(uint32_t id, uint8_t *data, uint8_t len) {
    uint32_t txmailbox = 0;

    CAN1->sTxMailBox[txmailbox].TIR = id << 21;
    CAN1->sTxMailBox[txmailbox].TDTR = len;

    for (int i = 0; i < len; i++) {
        CAN1->sTxMailBox[txmailbox].TDLR = data[i];
    }

    CAN1->sTxMailBox[txmailbox].TIR |= CAN_TI0R_TXRQ;
    while(!(CAN1->sTxMailBox[txmailbox].TIR & CAN_TIR_TXOK0));
    return 0;
}
```

---

### 2.3 特殊外设

### 2.3.1 CRC校验

#### CRC32/CRC16

```c
void CRC_Init(void) {
    // 使能CRC时钟
    RCC->AHBENR |= RCC_AHBENR_CRCEN;

    // 复位CRC
    CRC->CR = CRC_CR_RESET;
}

// 计算CRC32
uint32_t CRC32_Calculate(uint8_t *data, uint32_t len) {
    CRC->CR = CRC_CR_RESET;

    for (uint32_t i = 0; i < len; i++) {
        CRC->DR = data[i];
    }

    return CRC->DR;
}
```

### 2.3.2 安全加密

#### AES加密

```c
void AES_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_AESEN;

    // 复位AES
    AES->CR &= ~AES_CR_EN;
    AES->CR |= AES_CR_RST;
    AES->CR &= ~AES_CR_RST;
}

// AES ECB加密
void AES_ECB_Encrypt(uint8_t *key, uint8_t *input, uint8_t *output) {
    // 配置密钥
    for (int i = 0; i < 4; i++) {
        AES->KEYR[i] = ((uint32_t *)key)[i];
    }

    // 配置加密模式
    AES->CR = AES_CR_MODE | AES_CR_CHMOD_0;  // ECB, 加密

    // 输入数据
    for (int i = 0; i < 4; i++) {
        AES->DINR = ((uint32_t *)input)[i];
        while(!(AES->SR & AES_SR_DINNE));
    }

    // 等待完成
    while(!(AES->SR & AES_SR_BUSY));

    // 读取输出
    for (int i = 0; i < 4; i++) {
        ((uint32_t *)output)[i] = AES->DOUTR;
    }
}
```

### 2.3.3 随机数发生器

```c
void RNG_Init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;

    // 使能RNG
    RNG->CR |= RNG_CR_RNGEN;

    // 等待随机数就绪
    while(!(RNG->SR & RNG_SR_DRDY));
}

// 获取随机数
uint32_t RNG_GetRandomNumber(void) {
    while(!(RNG->SR & RNG_SR_DRDY));
    return RNG->DR;
}
```

### 2.3.4 外部存储扩展

#### FSMC (Flexible Static Memory Controller)

```c
void FSMC_SRAM_Init(void) {
    // 使能FSMC时钟
    RCC->AHB3ENR |= RCC_AHB3ENR_FSMCEN;

    // 使能GPIOD/E时钟
    RCC->APB2ENR |= RCC_APB2ENR_IOPDEN | RCC_APB2ENR_IOPEEN;

    // 配置GPIO复用功能
    // ... (配置地址/数据/控制线)

    // 配置FSMC Bank1 NOR/SRAM
    FSMC_Bank1->BTCR[0] = FSMC_BCR1_MWID_0 |   // 16位
                           FSMC_BCR1_MTYP_0 |   // SRAM
                           FSMC_BCR1_MBKEN;     // 使能

    FSMC_Bank1->BTCR[1] = 0x0;  // 时序参数
}

// 访问SRAM
void FSMC_SRAM_Write(uint32_t addr, uint16_t data) {
    *(volatile uint16_t *)(0x60000000 + addr * 2) = data;
}

uint16_t FSMC_SRAM_Read(uint32_t addr) {
    return *(volatile uint16_t *)(0x60000000 + addr * 2);
}
```

---

# 第三部分：实时操作系统

## 3.1 FreeRTOS核心机制

### 3.1.1 内核架构

```
┌─────────────────────────────────────────┐
│              Application                │
├─────────────────────────────────────────┤
│           FreeRTOS Kernel              │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ │
│  │Scheduler│ │  Tasks  │ │  ISR    │ │
│  └─────────┘ └─────────┘ └─────────┘ │
├─────────────────────────────────────────┤
│              Hardware                  │
└─────────────────────────────────────────┘
```

### 3.1.2 源码结构

```
FreeRTOS/
├── Source/
│   ├── list.c           # 链表实现
│   ├── queue.c          # 队列/信号量
│   ├── tasks.c          # 任务管理
│   ├── timers.c         # 软件定时器
│   ├── event_groups.c   # 事件组
│   └── port.c/portasm.s # 架构相关
├── Portable/
│   └── [Compiler]/[Arch] # 移植层
└── include/             # 头文件
```

### 3.1.3 任务状态机

```
        ┌──────────────┐
        │   Running    │ ◄────┐
        └──────────────┘      │
              ▲               │
              │               │ 调度器选中
        ┌────┴─────┐   ┌─────┴─────┐
        │ Ready    │   │ Blocked   │
        └──────────┘   └───────────┘
              │               │
              │               │ 延迟/等待信号量
              │               ▼
        ┌────┴─────┐   ┌───────────┐
        │  Ready    │   │  Suspended │
        └──────────┘   └───────────┘
              │               │
              │               │ 挂起/恢复
              └───────┬───────┘
                      │
                 创建/删除
```

### 3.1.4 调度算法

- **抢占式调度**: 高优先级任务抢占低优先级
- **时间片调度**: 同优先级任务轮转执行
- **配置项**:
  - `configUSE_PREEMPTION`: 抢占式调度
  - `configUSE_TIME_SLICING`: 时间片轮转

## 3.2 任务管理与调度

### 3.2.1 任务创建/删除

```c
// 任务函数
void vTaskFunction(void *pvParameters) {
    while (1) {
        // 任务逻辑
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// 创建任务
void Task_Create(void) {
    xTaskCreate(
        vTaskFunction,          // 任务函数
        "TaskName",             // 任务名称
        configMINIMAL_STACK_SIZE, // 栈大小
        NULL,                   // 参数
        tskIDLE_PRIORITY + 1,   // 优先级
        NULL                    // 任务句柄
    );
}

// 删除任务
void Task_Delete(TaskHandle_t handle) {
    vTaskDelete(handle);
}
```

### 3.2.2 任务挂起/恢复

```c
// 挂起任务
vTaskSuspend(TaskHandle_t);

// 恢复任务
vTaskResume(TaskHandle_t);

// 从ISR恢复任务
BaseType_t xTaskResumeFromISR(TaskHandle_t);
```

### 3.2.3 优先级翻转问题

```
高优先级任务等待低优先级任务释放资源
     T Low     ────┐
                   │
     T Med  ───────┼── 抢占
                   │
     T High     ◄──┘ 等待资源
```

**解决方案**: 优先级继承 (Mutex)

```c
// 创建互斥锁
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// 获取互斥锁
xSemaphoreTake(xMutex, portMAX_DELAY);

// 释放互斥锁
xSemaphoreGive(xMutex);
```

## 3.3 内存管理

### 3.3.1 堆管理方案

| 方案 | 特点 | 适用场景 |
|------|------|----------|
| heap_1 | 最简单，不可释放 | 小型系统 |
| heap_2 | 首次适应，可释放 | 一般应用 |
| heap_3 | 包装malloc/free | 兼容库 |
| heap_4 | 最佳适应，碎片管理 | 复杂应用 |
| heap_5 | heap_4 + 多区域 | 大型系统 |

### 3.3.2 静态分配 vs 动态分配

```c
// 静态分配
static StackType_t xStack[1024];
StaticTask_t xTaskBuffer;
TaskHandle_t xHandle = xTaskCreateStatic(..., xStack, ...);

// 动态分配
xTaskCreate(..., 1024, ...);
```

## 3.4 任务间通信

### 3.4.1 队列

```c
// 创建队列
QueueHandle_t xQueue = xQueueCreate(10, sizeof(uint32_t));

// 发送
uint32_t data = 100;
xQueueSend(xQueue, &data, 0);

// 接收
uint32_t recv_data;
xQueueReceive(xQueue, &recv_data, portMAX_DELAY);
```

### 3.4.2 信号量

```c
// 二值信号量
SemaphoreHandle_t xBinarySem = xSemaphoreCreateBinary();

// 计数信号量
SemaphoreHandle_t xCountSem = xSemaphoreCreateCounting(10, 0);

// 获取
xSemaphoreTake(xSem, portMAX_DELAY);

// 释放
xSemaphoreGive(xSem);

// 从ISR释放
xSemaphoreGiveFromISR(xSem, &xHigherPriorityTaskWoken);
```

### 3.4.3 互斥锁

```c
// 递归互斥锁
SemaphoreHandle_t xMutex = xSemaphoreCreateRecursiveMutex();

// 递归获取
xSemaphoreTakeRecursive(xMutex, portMAX_DELAY);

// 递归释放
xSemaphoreGiveRecursive(xMutex);
```

### 3.4.4 事件标志组

```c
// 创建事件组
EventGroupHandle_t xEventGroup = xEventGroupCreate();

// 设置标志位
xEventGroupSetBits(xEventGroup, BIT_0 | BIT_1);

// 等待标志位
EventBits_t uxBits = xEventGroupWaitBits(
    xEventGroup,
    BIT_0,
    pdTRUE,  // 清除标志
    pdTRUE,  // 与逻辑
    portMAX_DELAY
);
```

### 3.4.5 任务通知

```c
// 发送通知
xTaskNotify(xTaskHandle, value, eSetValueWithOverwrite);

// 等待通知
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

## 3.5 其他RTOS简介

### 3.5.1 uC/OS-II/III

- 抢占式多任务
- 源码结构清晰
- 商业许可证

### 3.5.2 RT-Thread

- 国产RTOS
- 组件丰富
- 支持SMP

### 3.5.3 Zephyr

- Linux基金会项目
- 物联网导向
- 统一API

---

# 第四部分：国产MCU专题

## 4.1 GD32系列

### 4.1.1 与STM32兼容性

| 特性 | 兼容性 |
|------|--------|
| Pin-to-Pin | 高度兼容 |
| 寄存器 | 95%+兼容 |
| 固件库 | 直接替换 |
| 生态 | 兼容ST生态 |

### 4.1.2 型号命名规则

```
GD 32 0 F 103 V 8 T 6
│  │  │ │  │   │ │ │ └─ 封装类型
│  │  │ │  │   │ │ └─── 引脚数
│  │  │ │  │   │ └───── Flash容量
│  │  │ │  │   └────── 系列(V=增强型)
│  │  │ │  └───────── 内核(F= Cortex-M3)
│  │  │ └─────────── 子系列
│  │ └────────────── 系列(0=Basic)
│ └───────────────── GigaDevice
```

### 4.1.3 外设增强特性

- 主频提升至120MHz (F103系列)
- 更多UART/SPI/I2C
- 内部RC精度提升

## 4.2 CH32系列

### 4.2.1 RISC-V内核

- 基于沁微电子 RISC-V 内核 (QingKe)
- 32位RISC-V3A
- 支持RISC-V标准指令集

### 4.2.2 生态与工具链

- 支持MounRiver Studio
- 支持Keil/IAR
- 兼容GD32/ST外设

## 4.3 AT32系列

### 4.3.1 雅特力MCU特点

- 高主频 (最高240MHz)
- 丰富外设
- 国产替代优选

### 4.3.2 入门资源

- AT32官方SDK
- AT-START开发板
- 兼容ST外设API

---

# 第五部分：面试题精讲

## 5.1 内核与架构类

### 选择题/判断题

1. **Cortex-M3内核的流水线是几级？**
   - 答案: 3级 (取指-译码-执行)

2. **Thumb指令集是16位的，对吗？**
   - 答案: 不对，Thumb-2支持16/32位混合

3. **NVIC支持多少个中断？**
   - 答案: 最多256个

4. **M4内核支持DSP指令集，对吗？**
   - 答案: 对

5. **MSP和PSP的区别是什么？**
   - 答案: MSP是主堆栈，PSP是进程堆栈。MSP用于异常处理和系统初始化，PSP用于用户任务

6. **CONTROL寄存器的bit1用于什么？**
   - 答案: 选择使用MSP(0)还是PSP(1)

7. **Cortex-M的向量表第一项是什么？**
   - 答案: 主堆栈指针(MSP)初始值

8. **单片机刚上电时使用的是哪个堆栈？**
   - 答案: MSP (从向量表第一项加载)

### 简答题

1. **Cortex-M3和M4的区别是什么？**
   - M4增加DSP指令集和可选FPU
   - M4支持SIMD并行操作
   - M4有更快的乘法运算

2. **什么是中断优先级分组？有什么作用？**
   - 将优先级分为抢占优先级和子优先级
   - 抢占优先级决定中断嵌套
   - 子优先级决定同优先级中断顺序

3. **为什么Flash读取需要等待周期？**
   - Flash读取速度低于CPU主频
   - 需要插入等待周期保证数据正确

4. **详细说明Cortex-M的中断响应过程？**

   **答：**
   1. 异常发生时，CPU自动将xPSR、PC、LR、R12、R0-R3压入当前堆栈
   2. 根据异常编号从向量表加载异常处理函数地址
   3. 切换到Handler模式，使用MSP作为堆栈指针
   4. 更新LR为EXC_RETURN值（用于返回）
   5. 跳转到异常处理函数执行
   6. 处理完成后，执行返回指令，恢复现场继续执行

5. **什么是EXC_RETURN？它的作用是什么？**

   **答：**
   EXC_RETURN是LR寄存器的特殊值，用于异常返回：

   | 值 | 含义 |
   |---|---|
   | 0xFFFFFFF1 | 返回Handler模式，使用MSP |
   | 0xFFFFFFF9 | 返回Thread模式，使用MSP |
   | 0xFFFFFFFD | 返回Thread模式，使用PSP |

6. **请说明MSP、PSP的使用场景？**

   **答：**
   - **MSP (Main Stack Pointer)**:
     - 系统复位后默认使用
     - 异常处理时使用
     - 用于中断服务程序

   - **PSP (Process Stack Pointer)**:
     - 用户任务/线程使用
     - RTOS中每个任务独立的栈
     - 实现用户级和特权级分离

7. **Cortex-M的Fault异常有哪些？如何处理？**

   **答：**
   | 异常类型 | 描述 | 常见原因 |
   |----------|------|----------|
   | HardFault | 硬 fault | 所有其他 fault 无法处理时触发 |
   | MemManage | 内存管理 fault | MPU 访问违规 |
   | BusFault | 总线 fault | 访问无效地址 |
   | UsageFault | 用法 fault | 未对齐、除零等 |

   **处理方法：**
   - 在 HardFault_Handler 中分析 SP 指针
   - 读取保存的寄存器值确定错误位置
   - 检查内存访问是否越界

8. **为什么 Cortex-M 使用统一的 4GB 地址空间？**

   **答：**
   - 简化软件设计，统一的内存映射
   - Flash、SRAM、外设统一编址
   - 方便链接脚本编写
   - 访问任何资源都使用相同的寻址方式

### 编程题

1. **实现GPIO端口的位带操作**

```c
#define BITBAND(addr, bit) ((addr & 0xF0000000) + 0x2000000 + ((addr & 0xFFFFF) << 5) + (bit << 2))
#define MEM_ADDR(addr)  *((volatile uint32_t *)(addr))
#define BIT_ADDR(addr, bit) MEM_ADDR(BITBAND(addr, bit))

// 使用示例
#define PA5_OUT BIT_ADDR(GPIOA_BASE + 0x0C, 5)  // PA5输出
PA5_OUT = 1;  // 输出高
PA5_OUT = 0;  // 输出低
```

2. **实现精确延时函数（使用SysTick）**

```c
volatile uint32_t g_systick_count = 0;

void SysTick_Handler(void) {
    g_systick_count++;
}

void delay_ms(uint32_t ms) {
    uint32_t start = g_systick_count;
    while ((g_systick_count - start) < ms);
}

// 或者使用DWT单元
void delay_us(uint32_t us) {
    volatile uint32_t *dwt_ctrl = (uint32_t *)0xE0001000;
    volatile uint32_t *dwt_cyccnt = (uint32_t *)0xE0001004;
    volatile uint32_t *demcr = (uint32_t *)0xE000EDFC;

    // 使能DWT
    *demcr |= (1 << 24);  // TRCENA
    *dwt_ctrl |= (1 << 0);  // CYCCNTENA

    uint32_t start = *dwt_cyccnt;
    uint32_t cycles = us * (SystemCoreClock / 1000000);
    while ((*dwt_cyccnt - start) < cycles);
}
```

3. **实现临界区保护（禁用/启用中断）**

```c
// 方法1: 使用cpsid/cpsie
void critical_enter(void) {
    __asm volatile ("cpsid i" : : : "memory");
}

void critical_exit(void) {
    __asm volatile ("cpsie i" : : : "memory");
}

// 方法2: 使用 PRIMASK
void critical_enter(void) {
    __asm volatile (
        "mrs r0, primask\n"
        "cpsid i\n"
        "str r0, [sp, #-4]!\n"
        : : : "r0", "memory"
    );
}

void critical_exit(void) {
    __asm volatile (
        "ldr r0, [sp], #4\n"
        "msr primask, r0\n"
        : : : "r0", "memory"
    );
}
```

## 5.2 外设与驱动类

### 简答题

1. **USART和UART的区别？**
   - USART = UART + 同步时钟
   - UART只支持异步通信
   - USART支持同步模式(如SPI模式)

2. **SPI的四种模式是什么？**
   - CPOL: 时钟极性 (0=低电平空闲, 1=高电平空闲)
   - CPHA: 时钟相位 (0=第一个边沿, 1=第二个边沿)
   - 组合: Mode 0/1/2/3

3. **DMA传输的优势是什么？**
   - 减少CPU负担
   - 提高数据传输效率
   - 支持外设与内存批量传输

### 编程题

1. **实现串口DMA接收不定长数据**

```c
#define RX_BUF_SIZE 1024
uint8_t usart_rx_buf[RX_BUF_SIZE];

void USART_DMA_Init(void) {
    // 开启DMA
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    DMA1_Channel5->CPAR = (uint32_t)&USART1->DR;
    DMA1_Channel5->CMAR = (uint32_t)usart_rx_buf;
    DMA1_Channel5->CNDTR = RX_BUF_SIZE;
    DMA1_Channel5->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_RX;

    // 使能DMA接收
    USART1->CR3 |= USART_CR3_DMAR;
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}
```

## 5.3 RTOS类

### 简答题

1. **FreeRTOS的任务状态有哪些？**
   - Running (运行)
   - Ready (就绪)
   - Blocked (阻塞)
   - Suspended (挂起)

2. **什么是优先级翻转？如何解决？**
   - 高优先级任务等待低优先级任务释放资源
   - 使用优先级继承(互斥锁)
   - 优先级天花板协议

3. **临界段是什么？如何实现？**
   - 临界段是代码的原子执行区域
   - 使用 `taskENTER_CRITICAL()` 和 `taskEXIT_CRITICAL()`
   - 实际上是操作BASEPRI寄存器

### 编程题

1. **实现一个生产者-消费者模型**

```c
#define BUFFER_SIZE 10
QueueHandle_t xQueue;

void Producer(void *pv) {
    uint32_t count = 0;
    while (1) {
        xQueueSend(xQueue, &count, 0);
        count++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Consumer(void *pv) {
    uint32_t data;
    while (1) {
        if (xQueueReceive(xQueue, &data, portMAX_DELAY) == pdTRUE) {
            printf("Received: %lu\n", data);
        }
    }
}
```

## 5.4 通信协议类

### 简答题

1. **I2C的起始和停止条件是什么？**
   - 起始: SCL高时，SDA从高变低
   - 停止: SCL高时，SDA从低变高

2. **CAN总线的特点是什么？**
   - 多主站方式
   - 差分信号传输
   - 错误检测与恢复
   - 远程帧/数据帧

3. **USB的四种传输类型？**
   - 控制传输
   - 中断传输
   - 批量传输
   - 等时传输

## 5.5 综合场景题

### 系统设计

1. **设计一个按键检测系统**
   - 使用GPIO输入 + EXTI中断
   - 软件消抖 (延时检测)
   - 支持短按/长按区分
   - 回调函数处理

### 故障诊断

1. **程序死机如何诊断？**
   - 查看HardFault堆栈
   - 检查内存溢出
   - 分析看门狗复位
   - 使用调试器断点

2. **通信异常如何排查？**
   - 检查时钟配置
   - 验证引脚复用
   - 示波器观察波形
   - 检查协议参数

## 5.6 手撕代码

### 常见算法实现

1. **实现CRC8校验**

```c
uint8_t crc8(uint8_t *data, uint32_t len) {
    uint8_t crc = 0x00;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

2. **实现环形缓冲区**

```c
typedef struct {
    uint8_t *buffer;
    uint16_t head;
    uint16_t tail;
    uint16_t size;
} RingBuffer;

void rb_init(RingBuffer *rb, uint8_t *buf, uint16_t size) {
    rb->buffer = buf;
    rb->size = size;
    rb->head = rb->tail = 0;
}

int rb_write(RingBuffer *rb, uint8_t data) {
    uint16_t next = (rb->head + 1) % rb->size;
    if (next == rb->tail) return -1;  // 满
    rb->buffer[rb->head] = data;
    rb->head = next;
    return 0;
}

int rb_read(RingBuffer *rb, uint8_t *data) {
    if (rb->head == rb->tail) return -1;  // 空
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    return 0;
}
```

---

# 第六部分：综合实战

## 6.1 常见项目问题

### 6.1.1 死机/跑飞/HardFault诊断

#### HardFault常见原因

- 内存访问越界
- 除零操作
- 非法指令
- 看门狗超时

#### HardFault分析

```c
// HardFault处理函数
void HardFault_Handler(void) {
    __asm volatile (
        "mrs r0, psp\n"
        "mrs r1, msp\n"
        "b HardFault_Handler_C\n"
    );
}

void HardFault_Handler_C(uint32_t *sp) {
    uint32_t r0 = sp[0];
    uint32_t r1 = sp[1];
    uint32_t r2 = sp[2];
    uint32_t r3 = sp[3];
    uint32_t r12 = sp[4];
    uint32_t lr = sp[5];
    uint32_t pc = sp[6];
    uint32_t xpsr = sp[7];

    printf("HardFault:\n");
    printf("R0: 0x%08X\n", r0);
    printf("R1: 0x%08X\n", r1);
    printf("PC: 0x%08X\n", pc);
    printf("PSR: 0x%08X\n", xpsr);

    while (1);
}
```

### 6.1.2 堆栈溢出检测

```c
// 栈溢出检测 (在任务切换时检查)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("Stack overflow in task: %s\n", pcTaskName);
    while (1);
}

// 启用栈溢出检测
configCHECK_FOR_STACK_OVERFLOW 2
```

### 6.1.3 内存泄漏

- 避免在循环中分配内存
- 使用静态内存分配
- 定期检查可用堆

### 6.1.4 IAP固件升级

```c
void IAP_Init(void) {
    // 检查是否需要升级
    if (BOOT_FLAG == UPGRADE_FLAG) {
        // 跳转到升级程序
        ((void (*)(void))(*(volatile uint32_t *)(FLASH_APP_ADDR + 4)))();
    } else {
        // 跳转到应用程序
        ((void (*)(void))(*(volatile uint32_t *)(FLASH_APP_ADDR + 4)))();
    }
}
```

## 6.2 性能优化

### 6.2.1 CPU利用率优化

- 减少阻塞延迟
- 使用DMA传输
- 优化中断处理

### 6.2.2 DMA优化

- 使用双缓冲
- 合理配置优先级
- 减少中断频率

### 6.2.3 中断优化

```c
// 零中断服务程序设计
volatile uint32_t flag = 0;

void IRQ_Handler(void) {
    flag = 1;  // 只设置标志
}

void Main_Loop(void) {
    if (flag) {
        flag = 0;
        // 处理中断事务
    }
}
```

### 6.2.4 Flash等待周期

```c
// 根据主频配置Flash等待周期
void FLASH_SetLatency(uint32_t freq) {
    if (freq < 24000000) {
        FLASH->ACR = FLASH_ACR_LATENCY_0;
    } else if (freq < 48000000) {
        FLASH->ACR = FLASH_ACR_LATENCY_1;
    } else {
        FLASH->ACR = FLASH_ACR_LATENCY_2;
    }
}
```

## 6.3 可靠性设计

### 6.3.1 EMC/ESD设计

- 电源去耦电容
- IO保护电路
- PCB布局布线

### 6.3.2 看门狗应用

```c
void Watchdog_Init(void) {
    IWDG->KR = 0x5555;
    IWDG->PR = IWDG_PR_PR_4;   // 256分频
    IWDG->RLR = 4095;          // 最大超时
    IWDG->KR = 0xAAAA;         // 启动
}

// 定期喂狗
void Feed_Dog(void) {
    IWDG->KR = 0xAAAA;
}
```

### 6.3.3 异常处理

```c
void NMI_Handler(void) {
    // 不可屏蔽中断处理
}

void MemManage_Handler(void) {
    // 内存管理错误
}

void BusFault_Handler(void) {
    // 总线错误
}

void UsageFault_Handler(void) {
    // 用法错误
}
```

## 6.4 开发工具与调试

### 6.4.1 IDE对比

| IDE | 优点 | 缺点 |
|-----|------|------|
| Keil | 生态好，调试方便 | 商业授权 |
| IAR | 编译效率高 | 价格较高 |
| STM32CubeIDE | 免费，图形化 | 占用资源大 |
| VS Code + PlatformIO | 免费，开源 | 调试体验一般 |

### 6.4.2 调试器对比

| 调试器 | 特点 |
|--------|------|
| ST-Link | 便宜，STM32专用 |
| J-Link | 高速，兼容性好 |
| DAP-Link | 开源，便宜 |

### 6.4.3 调试技巧

- 使用断点调试
- 观察寄存器/内存
- 串口日志输出
- 逻辑分析仪

---


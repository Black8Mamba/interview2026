\# FreeRTOS 在 Cortex-R52 架构适配移植指南



\## 文档概述



本文档详细介绍 FreeRTOS 实时操作系统在 STMicroelectronics Stellar SR6P3c4 芯片 Cortex-R52 架构上的适配移植方案。本文档基于 SDK 中的 `exampleCommsUARTuartFreeRTOS\_CLI` 示例工程编写，旨在帮助开发者在 SR6P3c4平台上快速搭建 FreeRTOS 开发环境。







\## 1. Cortex-R52 架构特性



\### 1.1 处理器架构简介



Cortex-R52 是 ARM 公司面向实时应用的高性能处理器，属于 ARMv8-R 架构体系。这是 ARM 首次在 Cortex-R 系列中采用 64 位架构，支持 AArch64 和 AArch32 两种执行状态。与 Cortex-M 系列相比，Cortex-R52 具有以下显著特点：



多核支持与隔离特性：



&nbsp;  支持锁步（Lockstep）模式运行，提供硬件级冗余

&nbsp;  支持处理器核心间的空间隔离

&nbsp;  具备内存保护单元（MPU），可配置多个保护区

&nbsp;  支持虚拟化技术（Hypervisor）



指令集特性：



&nbsp;  支持 AArch64（64位）和 AArch32（32位）两种执行状态

&nbsp;  支持 ARM 指令集和 Thumb-2 指令集

&nbsp;  支持浮点单元（FPU）- ARMv8 NEON SIMD 扩展

&nbsp;  具备特权级别（Exception Level）区分



中断控制器：



&nbsp;  集成 ARM Generic Interrupt Controller (GICv2)

&nbsp;  支持最多 544 个中断源

&nbsp;  支持中断优先级嵌套和中断屏蔽



\### 1.2 异常处理模型 (ARMv8-R Exception Level)



ARMv8-R 架构使用 Exception Level（异常级别）来区分不同的特权级别，而非 ARMv7 的运行模式：



&nbsp;异常级别  执行状态             描述                    

&nbsp;----  ---------------  --------------------- 

&nbsp;EL0   AArch32AArch64  用户态应用程序级别            

&nbsp;EL1   AArch32AArch64  特权态（操作系统内核）           

&nbsp;EL2   AArch32AArch64  虚拟机监视器（Hypervisor）    

&nbsp;EL3   AArch32AArch64  安全监视器（Secure Monitor） 



Cortex-R52 复位后运行在 EL2（Hypervisor 模式），这是与其他 Cortex-R 处理器的重要区别。FreeRTOS 应用程序通常运行在 EL1（特权模式）。



当 FreeRTOS 运行时：



&nbsp;  任务执行在 EL1 特权模式

&nbsp;  SVC 异常用于系统调用和任务切换触发

&nbsp;  IRQ 中断用于外设中断处理



\### 1.3 特权级别与安全扩展



Cortex-R52 支持 ARM TrustZone 安全扩展：



&nbsp;  Secure World (安全世界)：可信代码执行区域

&nbsp;  Non-Secure World (非安全世界)：普通代码执行区域



在本文档的 FreeRTOS 移植中，主要关注非安全世界（Non-Secure）EL1 级别的配置。







\## 2. FreeRTOS 移植层架构



\### 2.1 SDK 目录结构



Stellar SDK 中的 FreeRTOS 移植文件位于以下目录：



&nbsp;   middlewarefreertos

&nbsp;   ├── FreeRTOS

&nbsp;   │   ├── include                    # FreeRTOS 核心头文件

&nbsp;   │   │   ├── FreeRTOS.h

&nbsp;   │   │   ├── task.h

&nbsp;   │   │   ├── queue.h

&nbsp;   │   │   ├── list.h

&nbsp;   │   │   ├── FreeRTOSConfig.h       # 主配置文件

&nbsp;   │   │   └── ...

&nbsp;   │   ├── src                        # FreeRTOS 核心源文件

&nbsp;   │   │   ├── tasks.c

&nbsp;   │   │   ├── queue.c

&nbsp;   │   │   ├── list.c

&nbsp;   │   │   └── ...

&nbsp;   │   ├── MemMang

&nbsp;   │   │   └── heap\_4.c                # 内存管理实现

&nbsp;   │   ├── portable

&nbsp;   │   │   └── GCC

&nbsp;   │   │       └── ARM\_CR52          # Cortex-R52 移植文件

&nbsp;   │   │           ├── portmacro.h   # 端口宏定义

&nbsp;   │   │           ├── port.c         # 移植层 C 实现

&nbsp;   │   │           ├── portASM.S      # 移植层汇编实现

&nbsp;   │   │           └── FreeRTOS\_tick\_config.c  # 系统时钟配置



\### 2.2 移植文件说明



针对 Cortex-R52 架构，FreeRTOS 提供了完整的移植层，包括：



&nbsp;文件                        功能描述                   

&nbsp;------------------------  ---------------------- 

&nbsp;`portmacro.h`             定义数据类型、端口特定宏、中断管理接口    

&nbsp;`port.c`                  实现任务栈初始化、调度器启动、临界区管理   

&nbsp;`portASM.S`               实现上下文切换、SVC 和 IRQ 异常处理 

&nbsp;`FreeRTOS\_tick\_config.c`  系统时钟（Tick）配置，使用 AGT 外设 



\### 2.3 构建配置



FreeRTOS 模块在 `freertos.mk` 中定义，构建时根据 `TARGET\_CORE` 变量选择对应的移植文件：



```makefile

\# 针对 R52 核心的源文件配置

ifeq ($(TARGET\_CORE), R52)

MODULE\_C\_SRCS += 

&nbsp;   portableGCCARM\_CR52port.c 

&nbsp;   portableGCCARM\_CR52FreeRTOS\_tick\_config.c 

&nbsp;   MemMangheap\_4.c



MODULE\_A\_SRCS += 

&nbsp;   portableGCCARM\_CR52portASM.S

endif

```







\## 3. FreeRTOSConfig.h 配置详解



FreeRTOSConfig.h 是 FreeRTOS 的核心配置文件，位于示例工程的 `include` 目录下。以下是针对 Cortex-R52 的关键配置说明：



\### 3.1 时钟节拍配置



```c

\#define configTICK\_RATE\_HZ          ( ( TickType\_t ) 1000 )

\#define configUSE\_PREEMPTION         1

\#define configUSE\_TICKLESS\_IDLE      0

```



&nbsp;  `configTICK\_RATE\_HZ` 系统时钟节拍频率，设置为 1000Hz 即 1ms 一个 tick

&nbsp;  `configUSE\_PREEMPTION` 启用抢占式调度，任务可被更高优先级任务抢占

&nbsp;  `configUSE\_TICKLESS\_IDLE` 关闭低功耗 tickless 模式



\### 3.2 任务堆栈与优先级配置



```c

\#define configMAX\_PRIORITIES         ( 30 )

\#define configMINIMAL\_STACK\_SIZE     ( ( unsigned short ) 1024 )

\#define configMAX\_TASK\_NAME\_LEN      ( 10 )

```



&nbsp;  `configMAX\_PRIORITIES` 最大任务优先级数量（0-29）

&nbsp;  `configMINIMAL\_STACK\_SIZE` 最小任务栈大小（1024 字 = 4KB）

&nbsp;  `configMAX\_TASK\_NAME\_LEN` 任务名称最大长度



\### 3.3 内存配置



```c

\#define configTOTAL\_HEAP\_SIZE        ( 32  1024 )

\#define configAPPLICATION\_ALLOCATED\_HEAP  0

```



&nbsp;  `configTOTAL\_HEAP\_SIZE` FreeRTOS 堆内存大小，默认 32KB

&nbsp;  `configAPPLICATION\_ALLOCATED\_HEAP` 堆内存是否由应用程序提供



\### 3.4 中断优先级配置



```c

\#define configUNIQUE\_INTERRUPT\_PRIORITIES      32

\#define configMAX\_API\_CALL\_INTERRUPT\_PRIORITY  18UL

```



这是 Cortex-R52 FreeRTOS 移植中最关键的配置：



&nbsp;  `configUNIQUE\_INTERRUPT\_PRIORITIES` GIC 支持的唯一中断优先级数量（Cortex-R52 GIC 支持 32 个优先级）

&nbsp;  `configMAX\_API\_CALL\_INTERRUPT\_PRIORITY` 允许调用 FreeRTOS API 的最高中断优先级



重要约束：



&nbsp;  `configMAX\_API\_CALL\_INTERRUPT\_PRIORITY` 必须大于 `configUNIQUE\_INTERRUPT\_PRIORITIES  2`

&nbsp;  在 ARM GIC 中，数值越小优先级越高，因此 18  322 不符合要求

&nbsp;  实际配置中 `configMAX\_API\_CALL\_INTERRUPT\_PRIORITY = 18` 表示优先级 0-17 的中断可以调用 FromISR API



\### 3.5 内存分配配置



```c

\#define configSUPPORT\_STATIC\_ALLOCATION        1

\#define configSUPPORT\_DYNAMIC\_ALLOCATION       1

```



同时支持静态和动态内存分配方式。



\### 3.6 可选功能配置



```c

\#define configUSE\_MUTEXES                       1

\#define configUSE\_RECURSIVE\_MUTEXES             1

\#define configUSE\_COUNTING\_SEMAPHORES           1

\#define configUSE\_QUEUE\_SETS                    1

\#define configUSE\_TASK\_NOTIFICATIONS            1

```



\### 3.7 API 函数配置



```c

\#define INCLUDE\_vTaskPrioritySet                1

\#define INCLUDE\_uxTaskPriorityGet               1

\#define INCLUDE\_vTaskDelete                     1

\#define INCLUDE\_vTaskSuspend                    1

\#define INCLUDE\_vTaskDelayUntil                 1

\#define INCLUDE\_vTaskDelay                      1

```







\## 4. 移植层核心实现



\### 4.1 portmacro.h 关键定义



\#### 数据类型定义



```c

typedef uint32\_t StackType\_t;

typedef long BaseType\_t;

typedef unsigned long UBaseType\_t;

typedef uint32\_t TickType\_t;



\#define portMAX\_DELAY    ( TickType\_t ) 0xffffffffUL

```



Cortex-R52 是 32 位处理器，使用 32 位数据类型。



\#### 任务栈生长方向



```c

\#define portSTACK\_GROWTH    ( -1 )

```



栈向下生长（从高地址向低地址）。



\#### 栈对齐要求



```c

\#define portBYTE\_ALIGNMENT    8

```



要求 8 字节对齐，以满足浮点单元和 ARM AAPCS 调用规范要求。



\#### 中断管理宏



```c

\#define portENTER\_CRITICAL()       vPortEnterCritical()

\#define portEXIT\_CRITICAL()         vPortExitCritical()

\#define portDISABLE\_INTERRUPTS()    ulPortSetInterruptMask()

\#define portENABLE\_INTERRUPTS()     vPortClearInterruptMask( 0 )

```



FreeRTOS 使用中断屏蔽方式实现临界区，而非全局中断禁用。



\#### 任务切换触发



```c

\#define portYIELD()    \_\_asm volatile ( SWI 0  memory );

```



通过软件中断（SWISVC）触发任务切换。



\#### 优先级相关宏



```c

\#define portLOWEST\_INTERRUPT\_PRIORITY       ( ( ( uint32\_t ) configUNIQUE\_INTERRUPT\_PRIORITIES ) - 1UL )

\#define portLOWEST\_USABLE\_INTERRUPT\_PRIORITY ( portLOWEST\_INTERRUPT\_PRIORITY - 1UL )

```



\### 4.2 port.c 核心实现



\#### 全局变量



```c

volatile uint32\_t ulCriticalNesting = 9999UL;

uint32\_t ulPortTaskHasFPUContext = pdFALSE;

uint32\_t ulPortYieldRequired = pdFALSE;

uint32\_t ulPortInterruptNesting = 0UL;

```



&nbsp;  `ulCriticalNesting` 临界区嵌套计数，初始值确保调度器启动前临界区有效

&nbsp;  `ulPortTaskHasFPUContext` 任务是否使用浮点单元标志

&nbsp;  `ulPortYieldRequired` 是否需要任务切换标志

&nbsp;  `ulPortInterruptNesting` 中断嵌套深度



\#### 任务栈初始化



```c

StackType\_t pxPortInitialiseStack( StackType\_t pxTopOfStack,

&nbsp;                                    TaskFunction\_t pxCode,

&nbsp;                                    void pvParameters )

```



该函数设置任务初始栈帧，栈帧布局如下（从高地址向低地址）：



&nbsp;   \[  NULL (sentinel)           ]

&nbsp;   \[  NULL                      ]

&nbsp;   \[  NULL                      ]

&nbsp;   \[  initial CPSR              ]  - 初始状态寄存器

&nbsp;   \[  PC (task entry)           ]  - 任务入口地址

&nbsp;   \[  LR (portTASK\_RETURN\_ADDR) ]  - 返回地址

&nbsp;   \[  R12                       ]

&nbsp;   \[  R11                       ]

&nbsp;   \[  R10                       ]

&nbsp;   \[  R9                        ]

&nbsp;   \[  R8                        ]

&nbsp;   \[  R7                        ]

&nbsp;   \[  R6                        ]

&nbsp;   \[  R5                        ]

&nbsp;   \[  R4                        ]

&nbsp;   \[  R3                        ]

&nbsp;   \[  R2                        ]

&nbsp;   \[  R1                        ]

&nbsp;   \[  R0 (parameters)           ]  - 任务参数

&nbsp;   \[  ulCriticalNesting = 0     ]  - 临界区嵌套计数

&nbsp;   \[  ulPortTaskHasFPUContext   ]  - FPU 上下文标志



\#### 调度器启动



```c

BaseType\_t xPortStartScheduler( void )

```



调度器启动流程：



1\.  验证 CPU 模式：确保不在用户模式（User Mode）

2\.  验证二进制点配置：检查 GIC 二进制点寄存器设置

3\.  禁用中断：在 CPU 级别禁用中断

4\.  配置系统时钟：调用 `configSETUP\_TICK\_INTERRUPT()` 配置 tick 中断

5\.  启动第一个任务：调用 `vPortRestoreTaskContext()` 切换到第一个任务



\#### 临界区管理



```c

void vPortEnterCritical( void )

void vPortExitCritical( void )

uint32\_t ulPortSetInterruptMask( void )

void vPortClearInterruptMask( uint32\_t ulNewMaskValue )

```



临界区实现原理：



1\.  通过 GIC 的 ICCPMR（中断优先级屏蔽寄存器）屏蔽低于 `configMAX\_API\_CALL\_INTERRUPT\_PRIORITY` 的中断

2\.  增加减少 `ulCriticalNesting` 计数

3\.  当计数归零时，恢复中断使能



\#### 系统时钟中断处理



```c

void FreeRTOS\_Tick\_Handler( void )

```



Tick 中断处理流程：



1\.  禁用 CPU 中断

2\.  设置 GIC 优先级屏蔽

3\.  调用 `xTaskIncrementTick()` 更新系统 tick

4\.  如果需要任务切换，设置 `ulPortYieldRequired` 标志

5\.  清除中断标志并恢复中断使能



\### 4.3 portASM.S 汇编实现



\#### 上下文保存宏 (portSAVE\_CONTEXT)



```asm

portSAVE\_CONTEXT MACRO

&nbsp;   ; 保存 LR 和 SPSR 到系统模式栈

&nbsp;   SRSDB    sp!, #SYS\_MODE

&nbsp;   CPS      #SYS\_MODE

&nbsp;   PUSH     {R0-R12, R14}



&nbsp;   ; 保存临界区嵌套计数

&nbsp;   LDR      R2, ulCriticalNestingConst

&nbsp;   LDR      R1, \[R2]

&nbsp;   PUSH     {R1}



&nbsp;   ; 保存浮点上下文（如果使用 FPU）

&nbsp;   LDR      R2, ulPortTaskHasFPUContextConst

&nbsp;   LDR      R3, \[R2]

&nbsp;   CMP      R3, #0

&nbsp;   FMRXNE   R1, FPSCR

&nbsp;   VPUSHNE  {D0-D15}

&nbsp;   PUSHNE   {R1}

&nbsp;   PUSH     {R3}



&nbsp;   ; 保存任务栈指针到 TCB

&nbsp;   LDR      R0, pxCurrentTCBConst

&nbsp;   LDR      R1, \[R0]

&nbsp;   STR      SP, \[R1]

ENDM

```



\#### 上下文恢复宏 (portRESTORE\_CONTEXT)



```asm

portRESTORE\_CONTEXT MACRO

&nbsp;   ; 从 TCB 恢复任务栈指针

&nbsp;   LDR      R0, pxCurrentTCBConst

&nbsp;   LDR      R1, \[R0]

&nbsp;   LDR      SP, \[R1]



&nbsp;   ; 恢复浮点上下文

&nbsp;   LDR      R0, ulPortTaskHasFPUContextConst

&nbsp;   POP      {R1}

&nbsp;   STR      R1, \[R0]

&nbsp;   CMP      R1, #0

&nbsp;   POPNE    {R0}

&nbsp;   VPOPNE   {D0-D15}

&nbsp;   VMSRNE   FPSCR, R0



&nbsp;   ; 恢复临界区嵌套计数

&nbsp;   LDR      R0, ulCriticalNestingConst

&nbsp;   POP      {R1}

&nbsp;   STR      R1, \[R0]



&nbsp;   ; 设置中断优先级屏蔽

&nbsp;   CMP      R1, #0

&nbsp;   MOVEQ    R4, #255

&nbsp;   LDRNE    R4, ulMaxAPIPriorityMaskConst

&nbsp;   LDRNE    R4, \[R4]

&nbsp;   \_MCR(ICC\_PMR, R4)

&nbsp;   ISB



&nbsp;   ; 恢复寄存器并返回

&nbsp;   POP      {R0-R12, R14}

&nbsp;   RFEIA    sp!

ENDM

```



\#### SVC 异常处理 (svc\_exception)



软中断（SVC）用于触发任务调度。当调用 `portYIELD()` 时触发：



1\.  保存当前任务上下文

2\.  调用 `vTaskSwitchContext()` 选择新任务

3\.  恢复新任务上下文



\#### IRQ 异常处理 (irq\_exception)



外部中断处理流程：



1\.  计算返回地址并保存

2\.  切换到 Supervisor 模式

3\.  增加中断嵌套计数

4\.  从 GIC IAR 获取中断 ID

5\.  调用对应的中断处理函数（通过向量表）

6\.  写回 GIC EOI

7\.  检查是否需要任务切换







\## 5. 任务切换机制详解



\### 5.1 任务切换概述



任务切换是 FreeRTOS 实时操作系统的核心机制，它允许在多个任务之间切换 CPU 执行权。FreeRTOS 中的任务切换分为两种触发方式：



1\.  主动切换：任务主动调用 `portYIELD()` 或 `vTaskDelay()` 等 API 触发切换

2\.  被动切换：Tick 中断中高优先级任务就绪时触发切换



\### 5.2 任务切换触发流程



&nbsp;   ┌─────────────────────────────────────────────────────────────────────────────┐

&nbsp;   │                           任务切换触发流程                                    │

&nbsp;   ├─────────────────────────────────────────────────────────────────────────────┤

&nbsp;   │                                                                             │

&nbsp;   │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                 │

&nbsp;   │  │   任务 A     │───│  触发条件     │───│   SVC 异常    │                 │

&nbsp;   │  │  (运行中)    │    │  portYIELD() │    │  (svc\_exception)│              │

&nbsp;   │  └──────────────┘    │  Tick 中断   │    └──────┬───────┘                 │

&nbsp;   │         │            └──────────────┘             │                         │

&nbsp;   │         │                                         v                         │

&nbsp;   │         │            ┌──────────────────────────────────────┐              │

&nbsp;   │         │            │        SVC 异常处理入口              │              │

&nbsp;   │         │            │  1. 保存当前任务上下文               │              │

&nbsp;   │         │            │  2. 调用 vTaskSwitchContext()       │              │

&nbsp;   │         │            │  3. 恢复新任务上下文                 │              │

&nbsp;   │         │            └──────────────────────────────────────┘              │

&nbsp;   │         │                                         │                         │

&nbsp;   │         v                                         v                         │

&nbsp;   │  ┌──────────────┐                        ┌──────────────┐                 │

&nbsp;   │  │   任务 A     │───────────────────────│   任务 B     │                 │

&nbsp;   │  │  (就绪挂起) │                        │  (运行中)    │                 │

&nbsp;   │  └──────────────┘                        └──────────────┘                 │

&nbsp;   │                                                                             │

&nbsp;   └─────────────────────────────────────────────────────────────────────────────┘



\### 5.3 portYIELD() 触发机制



在 `portmacro.h` 中，`portYIELD()` 定义为：



```c

\#define portYIELD() \_\_asm volatile ( SWI 0  memory );

```



这会触发 SVC（Supervisor Call）异常，软中断号 0 被用于请求调度。



\### 5.4 SVC 异常处理源码解析



位置：`portASM.S`



```asm

svc\_exception

&nbsp;    保存当前任务上下文 

&nbsp;   portSAVE\_CONTEXT



&nbsp;    调用 FreeRTOS 任务选择函数 

&nbsp;   LDR     R0, vTaskSwitchContextConst

&nbsp;   BLX     R0



&nbsp;    恢复新任务上下文 

&nbsp;   portRESTORE\_CONTEXT

```



\#### 5.4.1 portSAVE\_CONTEXT 宏详解



`portSAVE\_CONTEXT` 宏负责保存当前任务的完整上下文，包括：



```asm

portSAVE\_CONTEXT MACRO

&nbsp;   ; 步骤 1 使用 SRSDB 指令保存返回地址和 SPSR 到栈

&nbsp;   ;         并自动切换到系统模式

&nbsp;   SRSDB    sp!, #SYS\_MODE



&nbsp;   ; 步骤 2 切换到系统模式，继续保存通用寄存器

&nbsp;   CPS      #SYS\_MODE

&nbsp;   PUSH     {R0-R12, R14}          ; R14(LR) 在系统模式是返回地址



&nbsp;   ; 步骤 3 保存临界区嵌套计数

&nbsp;   LDR      R2, ulCriticalNestingConst

&nbsp;   LDR      R1, \[R2]

&nbsp;   PUSH     {R1}



&nbsp;   ; 步骤 4 保存浮点单元上下文（如果任务使用 FPU）

&nbsp;   LDR      R2, ulPortTaskHasFPUContextConst

&nbsp;   LDR      R3, \[R2]

&nbsp;   CMP      R3, #0

&nbsp;   FMRXNE   R1, FPSCR              ; 保存浮点状态寄存器

&nbsp;   VPUSHNE  {D0-D15}                ; 保存 D0-D15 浮点寄存器

&nbsp;   PUSHNE   {R1}

&nbsp;   PUSH     {R3}                    ; 保存 FPU 上下文标志



&nbsp;   ; 步骤 5 将当前栈指针保存到 TCB

&nbsp;   LDR      R0, pxCurrentTCBConst   ; 获取 pxCurrentTCB 指针

&nbsp;   LDR      R1, \[R0]                ; 获取当前 TCB 地址

&nbsp;   STR      SP, \[R1]                ; 保存任务栈指针到 TCB

ENDM

```



栈帧布局（保存后）：



&nbsp;   高地址

&nbsp;   +------------------+

&nbsp;        SPSR          - 原 IRQ 模式的 SPSR

&nbsp;   +------------------+

&nbsp;        LR (R14\_irq)   - IRQ 模式返回地址

&nbsp;   +------------------+

&nbsp;        R12          

&nbsp;   +------------------+

&nbsp;        R11          

&nbsp;   +------------------+

&nbsp;        ...          

&nbsp;   +------------------+

&nbsp;        R0           

&nbsp;   +------------------+

&nbsp;      R14 (LR)         - 系统模式返回地址

&nbsp;   +------------------+

&nbsp;      临界区计数     

&nbsp;   +------------------+

&nbsp;     FPU 上下文标志  

&nbsp;   +------------------+

&nbsp;      FPSCR           - 如果使用 FPU

&nbsp;   +------------------+

&nbsp;      D15-D0          - 如果使用 FPU (32 x 4字节 = 128字节)

&nbsp;   +------------------+  - SP 指向此处

&nbsp;   低地址



\#### 5.4.2 vTaskSwitchContext() 函数解析



位置：`tasks.c`



```c

void vTaskSwitchContext( void )

{

&nbsp;   if( uxSchedulerSuspended != ( UBaseType\_t ) pdFALSE )

&nbsp;   {

&nbsp;        如果调度器被挂起，标记需要切换，待调度器恢复后处理 

&nbsp;       xYieldPending = pdTRUE;

&nbsp;   }

&nbsp;   else

&nbsp;   {

&nbsp;       xYieldPending = pdFALSE;

&nbsp;       traceTASK\_SWITCHED\_OUT();



&nbsp;        更新任务运行时间统计 

&nbsp;       #if ( configGENERATE\_RUN\_TIME\_STATS == 1 )

&nbsp;       {

&nbsp;            ... 更新运行时间

&nbsp;       }

&nbsp;       #endif



&nbsp;        选择最高优先级任务 - 核心算法 

&nbsp;       taskSELECT\_HIGHEST\_PRIORITY\_TASK();



&nbsp;       traceTASK\_SWITCHED\_IN();

&nbsp;   }

}

```



任务选择算法：



```c

&nbsp;使用硬件指令加速的优先级选择 

\#define taskSELECT\_HIGHEST\_PRIORITY\_TASK() 

{ 

&nbsp;   UBaseType\_t uxTopPriority; 

&nbsp;   

&nbsp;    使用 CLZ 指令快速找到最高优先级  

&nbsp;   portGET\_HIGHEST\_PRIORITY( uxTopPriority, uxTopReadyPriority ); 

&nbsp;   

&nbsp;    从就绪链表中取出下一个任务  

&nbsp;   listGET\_OWNER\_OF\_NEXT\_ENTRY( pxCurrentTCB, 

&nbsp;                                 \&( pxReadyTasksLists\[ uxTopPriority ] ) ); 

}

```



`portGET\_HIGHEST\_PRIORITY` 宏使用 ARM 的 `CLZ`（Count Leading Zeros）指令：



```c

\#define portGET\_HIGHEST\_PRIORITY( uxTopPriority, uxReadyPriorities ) 

&nbsp;   uxTopPriority = ( 31 - ARM\_CLZ( uxReadyPriorities ) )

```



\#### 5.4.3 portRESTORE\_CONTEXT 宏详解



`portRESTORE\_CONTEXT` 宏负责恢复新任务的上下文：



```asm

portRESTORE\_CONTEXT MACRO

&nbsp;   ; 步骤 1 从 TCB 恢复新任务的栈指针

&nbsp;   LDR      R0, pxCurrentTCBConst

&nbsp;   LDR      R1, \[R0]

&nbsp;   LDR      SP, \[R1]



&nbsp;   ; 步骤 2 恢复浮点上下文

&nbsp;   LDR      R0, ulPortTaskHasFPUContextConst

&nbsp;   POP      {R1}

&nbsp;   STR      R1, \[R0]

&nbsp;   CMP      R1, #0

&nbsp;   POPNE    {R0}

&nbsp;   VPOPNE   {D0-D15}

&nbsp;   VMSRNE   FPSCR, R0



&nbsp;   ; 步骤 3 恢复临界区嵌套计数

&nbsp;   LDR      R0, ulCriticalNestingConst

&nbsp;   POP      {R1}

&nbsp;   STR      R1, \[R0]



&nbsp;   ; 步骤 4 恢复中断优先级屏蔽

&nbsp;   CMP      R1, #0

&nbsp;   MOVEQ    R4, #255              ; 临界区计数为0，解除屏蔽

&nbsp;   LDRNE    R4, ulMaxAPIPriorityMaskConst

&nbsp;   LDRNE    R4, \[R4]

&nbsp;   \_MCR(ICC\_PMR, R4)              ; 写 GIC 优先级屏蔽寄存器

&nbsp;   ISB



&nbsp;   ; 步骤 5 恢复通用寄存器并返回

&nbsp;   POP      {R0-R12, R14}

&nbsp;   RFEIA    sp!                   ; 从栈恢复 CPSR 和 PC，实现模式切换

ENDM

```



\### 5.5 Tick 中断中的任务切换



Tick 中断是实现时间片轮转和任务延时的基础。当 Tick 中断发生时，可能触发任务切换：



&nbsp;   ┌─────────────────────────────────────────────────────────────────────────────┐

&nbsp;   │                          Tick 中断任务切换流程                               │

&nbsp;   ├─────────────────────────────────────────────────────────────────────────────┤

&nbsp;   │                                                                             │

&nbsp;   │   ┌─────────────┐                                                          │

&nbsp;   │   │  Tick 中断  │  ───  AGT 定时器触发中断                                  │

&nbsp;   │   │  触发       │                                                          │

&nbsp;   │   └──────┬──────┘                                                          │

&nbsp;   │          │                                                                  │

&nbsp;   │          v                                                                  │

&nbsp;   │   ┌─────────────────────────────────────┐                                   │

&nbsp;   │   │       irq\_exception 处理入口         │                                   │

&nbsp;   │   │  1. 计算返回地址 (lr = lr - 4)       │                                   │

&nbsp;   │   │  2. 保存 SPSR 和 LR 到栈             │                                   │

&nbsp;   │   │  3. 切换到 Supervisor 模式           │                                   │

&nbsp;   │   │  4. 增加中断嵌套计数                  │                                   │

&nbsp;   │   └──────┬──────────────────────────────┘                                   │

&nbsp;   │          │                                                                  │

&nbsp;   │          v                                                                  │

&nbsp;   │   ┌─────────────────────────────────────┐                                   │

&nbsp;   │   │  从 GIC 获取中断 ID 并调用处理函数   │                                   │

&nbsp;   │   │  - 调用 FreeRTOS\_Tick\_Handler()    │                                   │

&nbsp;   │   └──────┬──────────────────────────────┘                                   │

&nbsp;   │          │                                                                  │

&nbsp;   │          v                                                                  │

&nbsp;   │   ┌─────────────────────────────────────┐                                   │

&nbsp;   │   │     FreeRTOS\_Tick\_Handler()         │                                   │

&nbsp;   │   │  1. 屏蔽中断                         │                                   │

&nbsp;   │   │  2. xTaskIncrementTick()             │                                   │

&nbsp;   │   │     - 更新 tick 计数                 │                                   │

&nbsp;   │   │     - 检查延时队列                    │                                   │

&nbsp;   │   │     - 如果有更高优先级任务就绪        │                                   │

&nbsp;   │   │       返回 pdTRUE                    │                                   │

&nbsp;   │   │  3. 设置 ulPortYieldRequired 标志    │                                   │

&nbsp;   │   │  4. 清除 Tick 中断                   │                                   │

&nbsp;   │   └──────┬──────────────────────────────┘                                   │

&nbsp;   │          │                                                                  │

&nbsp;   │          v                                                                  │

&nbsp;   │   ┌─────────────────────────────────────┐                                   │

&nbsp;   │   │  检查 ulPortYieldRequired           │                                   │

&nbsp;   │   │                                     │                                   │

&nbsp;   │   │  if (ulPortYieldRequired == pdTRUE) │                                   │

&nbsp;   │   │       执行完整任务切换               │                                   │

&nbsp;   │   │  else                                │                                   │

&nbsp;   │   │       直接返回继续执行原任务          │                                   │

&nbsp;   │   └─────────────────────────────────────┘                                   │

&nbsp;   │                                                                             │

&nbsp;   └─────────────────────────────────────────────────────────────────────────────┘



\#### 5.5.1 FreeRTOS\_Tick\_Handler 源码



位置：`port.c`



```c

void FreeRTOS\_Tick\_Handler( void )

{

&nbsp;   uint32\_t priorityMask = 0;



&nbsp;    在 CPU 级别禁用中断 

&nbsp;   portCPU\_IRQ\_DISABLE();



&nbsp;    设置 GIC 优先级屏蔽，确保 Tick 处理不被干扰 

&nbsp;   priorityMask = ( uint32\_t ) ( configMAX\_API\_CALL\_INTERRUPT\_PRIORITY  portPRIORITY\_SHIFT );

&nbsp;   ARM\_MCR(15, 0, priorityMask, 4, 6, 0);  写 ICCPMR 



&nbsp;   portCPU\_IRQ\_ENABLE();



&nbsp;    增加 RTOS tick 计数 

&nbsp;   if( xTaskIncrementTick() != pdFALSE )

&nbsp;   {

&nbsp;        有更高优先级任务就绪，请求任务切换 

&nbsp;       ulPortYieldRequired = pdTRUE;

&nbsp;   }



&nbsp;    重新使能所有中断 

&nbsp;   portCLEAR\_INTERRUPT\_MASK();

&nbsp;   configCLEAR\_TICK\_INTERRUPT();

}

```



\### 5.6 任务切换关键数据结构



\#### 5.6.1 任务控制块 (TCB)



每个任务都有一个任务控制块（TCB），保存任务的全部状态信息：



```c

typedef struct tskTaskControlBlock

{

&nbsp;   volatile StackType\_t    pxTopOfStack;     栈顶指针 - 关键！



&nbsp;   #if ( configUSE\_MPU\_WRAPPERS == 1 )

&nbsp;        MPU 相关配置 

&nbsp;   #endif



&nbsp;   xListItem\_t            xStateListItem;     用于加入就绪延时队列 

&nbsp;   xListItem\_t            xEventListItem;     用于加入事件队列 

&nbsp;   UBaseType\_t            uxPriority;         任务优先级 



&nbsp;   StackType\_t            pxStack;           栈底指针 



&nbsp;   char                    pcTaskName\[ configMAX\_TASK\_NAME\_LEN ];  任务名称 



&nbsp;   #if ( configUSE\_TRACE\_FACILITY == 1 )

&nbsp;        调试相关 

&nbsp;   #endif



&nbsp;    ... 其他字段 

} tskTaskControlBlock;

```



关键字段说明：



&nbsp;  `pxTopOfStack`：任务栈顶指针，上下文切换时保存恢复栈指针

&nbsp;  `xStateListItem`：用于将任务加入各种链表（就绪队列、延时队列等）

&nbsp;  `uxPriority`：任务优先级（0 为最低优先级）



\#### 5.6.2 pxCurrentTCB 全局指针



```c

extern volatile TCB\_t  pxCurrentTCB;

```



`pxCurrentTCB` 是 FreeRTOS 的核心全局变量，始终指向当前正在运行的任务的 TCB。上下文切换时：



&nbsp;  保存上下文：将当前 SP 保存到 `pxCurrentTCB-pxTopOfStack`

&nbsp;  恢复上下文：从 `pxCurrentTCB-pxTopOfStack` 恢复 SP



\### 5.7 任务切换完整流程图



&nbsp;   ┌─────────────────────────────────────────────────────────────────────────────┐

&nbsp;   │                         完整任务切换流程                                      │

&nbsp;   ├─────────────────────────────────────────────────────────────────────────────┤

&nbsp;   │                                                                             │

&nbsp;   │  【阶段1 触发任务切换】                                                     │

&nbsp;   │                                                                             │

&nbsp;   │  +------------------+      +------------------+                             │

&nbsp;   │  │  portYIELD()    │  OR  │  Tick 中断触发   │                             │

&nbsp;   │  │  (主动调用)     │      │  (被动切换)      │                             │

&nbsp;   │  +--------+--------+      +--------+--------+                             │

&nbsp;   │                                                                          │

&nbsp;   │           v                        v                                       │

&nbsp;   │  +--------+--------+      +--------+--------+                             │

&nbsp;   │  │  SWI 0 指令    │      │  IRQ 异常入口    │                             │

&nbsp;   │  │  触发 SVC 异常 │      │  中断处理        │                             │

&nbsp;   │  +--------+--------+      +--------+--------+                             │

&nbsp;   │                                                                          │

&nbsp;   │           +------------+-----------+                                       │

&nbsp;   │                        v                                                   │

&nbsp;   │  【阶段2 保存当前任务上下文】                                               │

&nbsp;   │                                                                             │

&nbsp;   │  +------------------------------------------------------------------+      │

&nbsp;   │  │  portSAVE\_CONTEXT 宏执行                                         │      │

&nbsp;   │  │  1. SRSDB 保存返回地址和 CPSR 到栈                               │      │

&nbsp;   │  │  2. 切换到系统模式                                                │      │

&nbsp;   │  │  3. PUSH 通用寄存器 R0-R12, R14                                  │      │

&nbsp;   │  │  4. PUSH 临界区嵌套计数                                          │      │

&nbsp;   │  │  5. 保存恢复 FPU 上下文（如果使用）                             │      │

&nbsp;   │  │  6. STR SP - pxCurrentTCB-pxTopOfStack  (保存栈指针)          │      │

&nbsp;   │  +------------------------------------------------------------------+      │

&nbsp;   │                        v                                                   │

&nbsp;   │  【阶段3 选择新任务】                                                       │

&nbsp;   │                                                                             │

&nbsp;   │  +------------------------------------------------------------------+      │

&nbsp;   │  │  vTaskSwitchContext() 函数                                       │      │

&nbsp;   │  │  1. 检查调度器是否挂起                                            │      │

&nbsp;   │  │  2. 更新任务运行时间统计                                          │      │

&nbsp;   │  │  3. taskSELECT\_HIGHEST\_PRIORITY\_TASK()                          │      │

&nbsp;   │  │     - 使用 CLZ 指令找最高优先级                                   │      │

&nbsp;   │  │     - 从就绪链表取下一个任务                                      │      │

&nbsp;   │  │     - 更新 pxCurrentTCB 指向新任务                               │      │

&nbsp;   │  +------------------------------------------------------------------+      │

&nbsp;   │                        v                                                   │

&nbsp;   │  【阶段4 恢复新任务上下文】                                                  │

&nbsp;   │                                                                             │

&nbsp;   │  +------------------------------------------------------------------+      │

&nbsp;   │  │  portRESTORE\_CONTEXT 宏执行                                       │      │

&nbsp;   │  │  1. LDR SP - pxCurrentTCB-pxTopOfStack  (恢复栈指针)          │      │

&nbsp;   │  │  2. 恢复 FPU 上下文（如果使用）                                   │      │

&nbsp;   │  │  3. POP 恢复临界区嵌套计数                                        │      │

&nbsp;   │  │  4. 恢复中断优先级屏蔽                                            │      │

&nbsp;   │  │  5. POP 通用寄存器 R0-R12, R14                                  │      │

&nbsp;   │  │  6. RFEIA sp! 恢复 CPSR 和 PC，返回新任务                       │      │

&nbsp;   │  +------------------------------------------------------------------+      │

&nbsp;   │                        v                                                   │

&nbsp;   │  【阶段5 执行新任务】                                                       │

&nbsp;   │                                                                             │

&nbsp;   │  +------------------+                                                     │

&nbsp;   │  │  任务 B 开始执行  │  -- 新任务从上次暂停处继续运行                 │

&nbsp;   │  +------------------+                                                     │

&nbsp;   │                                                                             │

&nbsp;   └─────────────────────────────────────────────────────────────────────────────┘



\### 5.8 任务栈初始布局



新任务首次创建时，`pxPortInitialiseStack()` 函数会设置一个伪造的栈帧，使任务看起来像是刚被中断过：



```c

StackType\_t pxPortInitialiseStack( StackType\_t pxTopOfStack,

&nbsp;                                    TaskFunction\_t pxCode,

&nbsp;                                    void pvParameters )

{

&nbsp;    初始栈布局（从高地址向低地址） 



&nbsp;   pxTopOfStack = ( StackType\_t ) NULL;     哨兵值 

&nbsp;   pxTopOfStack--;

&nbsp;   pxTopOfStack = ( StackType\_t ) NULL;     哨兵值 

&nbsp;   pxTopOfStack--;

&nbsp;   pxTopOfStack = ( StackType\_t ) NULL;     哨兵值 

&nbsp;   pxTopOfStack--;



&nbsp;    初始 CPSR：系统模式，ARM 状态，使能 IRQFIQ 

&nbsp;   pxTopOfStack = portINITIAL\_SPSR;         0x1f 

&nbsp;   pxTopOfStack--;



&nbsp;    任务入口地址 

&nbsp;   pxTopOfStack = ( StackType\_t ) pxCode;

&nbsp;   pxTopOfStack--;



&nbsp;    返回地址（任务退出时调用） 

&nbsp;   pxTopOfStack = ( StackType\_t ) portTASK\_RETURN\_ADDRESS;

&nbsp;   pxTopOfStack--;



&nbsp;    通用寄存器初始值 

&nbsp;   pxTopOfStack = ( StackType\_t ) 0x12121212;   R12 

&nbsp;   pxTopOfStack--;

&nbsp;   pxTopOfStack = ( StackType\_t ) 0x11111111;   R11 

&nbsp;    ... R10-R4 类似 ... 

&nbsp;   pxTopOfStack = ( StackType\_t ) 0x01010101;   R1 

&nbsp;   pxTopOfStack--;



&nbsp;    任务参数 

&nbsp;   pxTopOfStack = ( StackType\_t ) pvParameters;  R0 

&nbsp;   pxTopOfStack--;



&nbsp;    临界区嵌套计数（初始为0） 

&nbsp;   pxTopOfStack = portNO\_CRITICAL\_NESTING;

&nbsp;   pxTopOfStack--;



&nbsp;    FPU 上下文标志（初始为0，表示不使用 FPU） 

&nbsp;   pxTopOfStack = portNO\_FLOATING\_POINT\_CONTEXT;



&nbsp;   return pxTopOfStack;

}

```



首次运行时的恢复过程：



1\.  `portRESTORE\_CONTEXT` 从栈恢复所有寄存器

2\.  `RFEIA sp!` 指令将 CPSR 恢复到 `portINITIAL\_SPSR`（系统模式，使能中断）

3\.  PC 被设置为任务入口函数地址

4\.  任务开始执行 `vTaskFunction(void pvParameters)`







\## 6. 系统时钟（Tick）配置



\### 6.1 AGT 定时器



Stellar SDK 使用 AGT（Asynchronous General Purpose Timer）作为 FreeRTOS 系统时钟源。AGT 是 SR6P3c4 的一个低功耗定时器外设。



\### 6.2 Tick 配置实现



文件位置：`middlewarefreertosFreeRTOSportableGCCARM\_CR52FreeRTOS\_tick\_config.c`



```c

\#define TICK\_IN\_US ( 1000000  configTICK\_RATE\_HZ)



void vConfigureTickInterrupt( void )

{

&nbsp;   agt\_stop(AGT\_VIRTD);

&nbsp;   agt\_set\_priority(AGT\_VIRTD, portLOWEST\_USABLE\_INTERRUPT\_PRIORITY);

&nbsp;   agt\_set\_alarm(AGT\_VIRTD, TICK\_IN\_US, AGT\_ALARM\_RELOAD);

&nbsp;   agt\_set\_action(AGT\_VIRTD, agt\_expiry\_tick\_cb, NULL);

&nbsp;   agt\_start(AGT\_VIRTD);

}



void vClearTickInterrupt( void )

{

&nbsp;    AGT 自动清除中断，无需额外操作

}

```



Tick 回调函数：



```c

\_\_attribute\_\_((section(.handlers))) void agt\_expiry\_tick\_cb(AGTDriver agtp, void data)

{

&nbsp;   (void)(agtp);

&nbsp;   (void)(data);

&nbsp;   FreeRTOS\_Tick\_Handler();

}

```



\### 6.3 配置要点



&nbsp;  AGT 中断优先级设置为 `portLOWEST\_USABLE\_INTERRUPT\_PRIORITY`（最低可用优先级）

&nbsp;  Tick 周期根据 `configTICK\_RATE\_HZ` 计算（默认 1ms）

&nbsp;  使用 AGT 的自动重载模式实现周期性中断







\## 7. 内存管理



\### 7.1 堆内存配置



FreeRTOS 使用 heap\_4.c 作为内存分配器。heap\_4 采用最佳适配（Best Fit）算法，并支持内存块合并。



```c

\#define configTOTAL\_HEAP\_SIZE    ( 32  1024 )

```



默认堆大小为 32KB，可根据应用程序需求调整。



\### 7.2 静态内存分配



如果使用静态内存分配，需要提供任务控制块和堆栈内存：



```c

\#if (configSUPPORT\_STATIC\_ALLOCATION == 1)

static StaticTask\_t IdleTaskTCBBuffer;

static StackType\_t IdleTaskStackBuffer\[4096  sizeof(StackType\_t)];



void vApplicationGetIdleTaskMemory(StaticTask\_t ppxIdleTaskTCBBuffer,

&nbsp;                                   StackType\_t ppxIdleTaskStackBuffer,

&nbsp;                                   uint32\_t pulIdleTaskStackSize)

{

&nbsp;   ppxIdleTaskTCBBuffer = \&IdleTaskTCBBuffer;

&nbsp;   ppxIdleTaskStackBuffer = IdleTaskStackBuffer;

&nbsp;   pulIdleTaskStackSize = configMINIMAL\_STACK\_SIZE;

}

\#endif

```







\## 8. 应用程序初始化流程



参考 `uartFreeRTOS\_CLI` 示例，FreeRTOS 应用程序的标准初始化流程如下：



\### 8.1 系统初始化



```c

int main(void)

{

&nbsp;    1. 时钟初始化

&nbsp;   clockInit();



&nbsp;    2. GPIO 初始化

&nbsp;   siul2\_init();

&nbsp;   siul2\_start();



&nbsp;    3. 板级初始化

&nbsp;   BOARD\_INIT\_FUNC(BOARD\_NAME);



&nbsp;    4. 中断控制器初始化

&nbsp;   irq\_init();



&nbsp;    5. 系统定时器初始化

&nbsp;   gst\_init(\&gst\_instance\_tbu, 0, GST\_TYPE\_TBU);

&nbsp;   gst\_start(\&gst\_instance\_tbu);



&nbsp;    6. 外设初始化

&nbsp;   agt\_init(\&agt\_instance\_virt, AGT\_VIRT);

&nbsp;   me\_init(\&me\_inst\_0, ME\_CORE\_DOMAIN\_0\_ID);

&nbsp;   me\_init(\&me\_inst\_3, ME\_PERIPHERAL\_DOMAIN\_ID);

&nbsp;   rgm\_init(\&rgm\_inst\_0, RGM\_CORE\_DOMAIN\_0\_ID, RGM\_TYPE\_CORE);

&nbsp;   rgm\_init(\&rgm\_inst\_3, RGM\_PERIPHERAL\_DOMAIN\_ID, RGM\_TYPE\_PERIPHERAL);



&nbsp;    7. OSAL 初始化

&nbsp;   osal\_init();

&nbsp;   osal\_start();



&nbsp;    8. UART 初始化

&nbsp;   linflexd\_init();

&nbsp;   io\_init(\&uart\_instance\_5, UART\_5\_DEVICE\_ID);

&nbsp;   io\_start(\&uart\_instance\_5);



&nbsp;    9. 注册 CLI 命令

&nbsp;   FreeRTOS\_CLIRegisterCommand(\&xConsoleTest);



&nbsp;    10. 启动控制台

&nbsp;   startConsole(NULL);



&nbsp;    11. 启动 FreeRTOS 调度器

&nbsp;   vTaskStartScheduler();

}

```



\### 8.2 模块依赖说明



&nbsp;模块                   功能描述      备注          

&nbsp;-------------------  --------  ----------- 

&nbsp;clockInit()          系统时钟配置    必须首先调用      

&nbsp;siul2\_initstart()  GPIO 初始化  IO 配置需要     

&nbsp;irq\_init()          中断控制器初始化  FreeRTOS 依赖 

&nbsp;gst\_initstart()    系统时间基准    提供时间戳       

&nbsp;agt\_init()          定时器初始化    提供 Tick     

&nbsp;osal\_initstart()   OSAL 抽象层  提供 OS 原语    

&nbsp;linflexd\_init()     UART 初始化  串口通信        







\## 9. 中断管理与优先级



\### 9.1 GIC 中断优先级机制



Cortex-R52 集成的 GICv2 支持：



&nbsp;  最多 32 个唯一优先级

&nbsp;  优先级数值越小优先级越高

&nbsp;  支持中断嵌套



\### 9.2 FreeRTOS 中断分类



在 FreeRTOS 中，中断分为两类：



1\.  系统调用中断（优先级 ≤ configMAX\_API\_CALL\_INTERRUPT\_PRIORITY）

&nbsp;      可以调用 FreeRTOS API

&nbsp;      可以调用 `FromISR` 后缀的函数

&nbsp;      支持中断嵌套



2\.  高优先级中断（优先级  configMAX\_API\_CALL\_INTERRUPT\_PRIORITY）

&nbsp;      不能调用 FreeRTOS API

&nbsp;      会被 FreeRTOS 临界区屏蔽

&nbsp;      用于时间关键的中断处理



\### 9.3 中断优先级配置示例



```c

&nbsp;配置 AGT 中断优先级为最低可用优先级

agt\_set\_priority(AGT\_VIRTD, portLOWEST\_USABLE\_INTERRUPT\_PRIORITY);



&nbsp;配置用户中断

&nbsp;使用 SDK 的 IRQ API 配置其他外设中断

irq\_set\_priority(irq\_id, desired\_priority);

```







\## 10. 移植验证与调试



\### 10.1 验证要点



1\.  调度器启动验证

&nbsp;      确认 `vTaskStartScheduler()` 正常返回

&nbsp;      首个任务正确执行



2\.  任务切换验证

&nbsp;      高优先级任务可抢占低优先级任务

&nbsp;      时间片轮转正常工作



3\.  中断响应验证

&nbsp;      中断可正常触发

&nbsp;      FromISR API 在中断中正确调用



4\.  临界区验证

&nbsp;      临界区可正确屏蔽中断

&nbsp;      嵌套临界区正常工作



\### 10.2 常见问题排查



&nbsp;问题       可能原因        解决方案                                         

&nbsp;-------  ----------  -------------------------------------------- 

&nbsp;调度器启动失败  中断优先级配置错误   检查 configUNIQUE\_INTERRUPT\_PRIORITIES       

&nbsp;任务不切换    Tick 中断未配置  确认 AGT 配置正确                                  

&nbsp;内存溢出     堆内存不足       增加 configTOTAL\_HEAP\_SIZE                   

&nbsp;中断嵌套失败   优先级设置错误     验证 configMAX\_API\_CALL\_INTERRUPT\_PRIORITY 







\## 11. 内存布局详解



\### 11.1 芯片内存映射概述



Stellar SR6P3c4 芯片采用多核架构，包含 Cortex-R52（安全核）和 Cortex-M4（应用核）。芯片的内存映射如下：



\#### 11.1.1 SR6P3c4 内存区域



&nbsp;内存区域                      起始地址         大小      用途             

&nbsp;------------------------  -----------  ------  -------------- 

&nbsp;嵌入式 NVM (EMBED\_NVM)  0x00000000   varies  代码存储（Flash）    

&nbsp;EMBED\_RAM            0x60000000   512KB   主 RAM（代码数据运行） 

&nbsp;系统 RAM 0 (sysram0)    0x64000000   256KB   系统保留堆内存       

&nbsp;系统 RAM 1 (sysram1)    0x64400000   256KB   共享内存           

&nbsp;HyperRAM              0x48000000   4MB     外部扩展内存         

&nbsp;TCM A (ATCM)          0x68000000   64KB    紧耦合存储（代码）      

&nbsp;TCM B (BTCM)          0x68100000   16KB    紧耦合存储（数据）      

&nbsp;TCM C (STCM)          0x68200000   16KB    紧耦合存储（堆栈）      

&nbsp;Cluster 本地 RAM        0x60000000+  varies  多核本地存储         



\### 11.2 链接脚本内存布局



链接脚本（`application-arm\_r52.ld.E`）定义了各内存段的实际布局：



```ld

MEMORY

{

&nbsp;    主 RAM 区域 

&nbsp;   EMBED\_RAM  org = 0x60000000, len = 512k



&nbsp;    系统 RAM 

&nbsp;   sysram0  org = 0x64000000, len = 256k

&nbsp;   sysram1  org = 0x64400000, len = 256k



&nbsp;    外部 HyperRAM 

&nbsp;   hyperram  org = 0x48000000, len = 4096k



&nbsp;    TCM 区域 

&nbsp;   cluster0\_core0\_tcm\_a  org = 0x68000000, len = 64k

&nbsp;   cluster0\_core0\_tcm\_b  org = 0x68100000, len = 16k

&nbsp;   cluster0\_core0\_tcm\_c  org = 0x68200000, len = 16k

}

```



\### 11.3 运行时栈布局



Cortex-R52 架构定义了多种运行模式，每种模式都有独立的栈空间。链接脚本中定义的栈区域如下：



\#### 11.3.1 各模式栈大小定义（config.mk）



&nbsp;栈类型           默认大小  配置变量                    

&nbsp;------------  ----  ----------------------- 

&nbsp;Supervisor 栈  1KB   SUPERVISOR\_STACK\_SIZE 

&nbsp;IRQ 栈         1KB   IRQ\_STACK\_SIZE        

&nbsp;FIQ 栈         1KB   FIQ\_STACK\_SIZE        

&nbsp;Abort 栈       1KB   ABORT\_STACK\_SIZE      

&nbsp;Undefined 栈   1KB   UNDEFINED\_STACK\_SIZE  

&nbsp;Hypervisor 栈  4KB   HYP\_STACK\_SIZE        

&nbsp;User 栈        32KB  USER\_STACK\_SIZE       



\#### 11.3.2 栈布局（链接脚本）



```ld

&nbsp;Supervisor 模式栈 

.supervisor\_stack 

{

&nbsp;   . = ALIGN(0x10);

&nbsp;   \_\_supervisor\_stack\_\_ = .;

&nbsp;   . += SUPERVISOR\_STACK\_SIZE;      默认 1KB 

&nbsp;   \_\_supervisor\_stack\_limit\_\_ = .;

}  EMBED\_RAM



&nbsp;IRQ 模式栈 

.irq\_stack 

{

&nbsp;   . = ALIGN(0x10);

&nbsp;   \_\_irq\_stack\_\_ = .;

&nbsp;   . += IRQ\_STACK\_SIZE;             默认 1KB 

&nbsp;   \_\_irq\_stack\_limit\_\_ = .;

}  EMBED\_RAM



&nbsp;FIQ 模式栈 

.fiq\_stack 

{

&nbsp;   . = ALIGN(0x10);

&nbsp;   \_\_fiq\_stack\_\_ = .;

&nbsp;   . += FIQ\_STACK\_SIZE;             默认 1KB 

&nbsp;   \_\_fiq\_stack\_limit\_\_ = .;

}  EMBED\_RAM



&nbsp;Hypervisor 模式栈 

.hyp\_stack 

{

&nbsp;   . = ALIGN(0x10);

&nbsp;   \_\_hyp\_stack\_\_ = .;

&nbsp;   . += HYP\_STACK\_SIZE;             默认 4KB 

&nbsp;   \_\_hyp\_stack\_limit\_\_ = .;

}  EMBED\_RAM



&nbsp;User 模式栈 

.user\_stack 

{

&nbsp;   . = ALIGN(0x10);

&nbsp;   \_\_user\_stack\_\_ = .;

&nbsp;   . += USER\_STACK\_SIZE;           默认 32KB 

&nbsp;   \_\_user\_stack\_limit\_\_ = .;

}  EMBED\_RAM

```



\#### 11.3.3 栈空间分布图



&nbsp;   EMBED\_RAM 区域 (0x60000000 - 0x6007FFFF, 512KB)

&nbsp;   +--------------------------------------------------+

&nbsp;    0x6007FFFF                                       

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;              User Stack (32KB)                   

&nbsp;       \_\_user\_stack\_limit\_\_ - 高地址             

&nbsp;       ...                                         

&nbsp;       \_\_user\_stack\_\_ - 低地址                   

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;           Hypervisor Stack (4KB)                 

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;              FIQ Stack (1KB)                     

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;              IRQ Stack (1KB)                     

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;            Undefined Stack (1KB)                

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;             Abort Stack (1KB)                   

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;           Supervisor Stack (1KB)                

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;              FreeRTOS Heap                       

&nbsp;       (动态分配，32KB 默认)                      

&nbsp;     +--------------------------------------------+  

&nbsp;     +--------------------------------------------+  

&nbsp;              .bss 段                             

&nbsp;       (未初始化全局变量)                         

&nbsp;     +--------------------------------------------+  

&nbsp;     +--------------------------------------------+  

&nbsp;              .data 段                            

&nbsp;       (已初始化全局变量)                         

&nbsp;     +--------------------------------------------+  

&nbsp;     +--------------------------------------------+  

&nbsp;              .text 段                            

&nbsp;       (代码段)                                   

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;    0x60000000                                       

&nbsp;   +--------------------------------------------------+



\### 11.4 FreeRTOS 堆内存布局



\#### 11.4.1 堆内存配置



FreeRTOS 使用 `configTOTAL\_HEAP\_SIZE` 配置堆大小（默认 32KB）：



```c

&nbsp;FreeRTOSConfig.h 

\#define configTOTAL\_HEAP\_SIZE    ( 32  1024 )

```



\#### 11.4.2 堆内存分配方式



链接脚本中通过 `.freertos\_heap` 段将 FreeRTOS 堆放置在 sysram0 区域：



```ld

\#if defined(SYSRAM0)

&nbsp;   .user\_sysram0 (NOLOAD) 

&nbsp;   {

&nbsp;       KEEP((.freertos\_heap))

&nbsp;   }  sysram0

\#endif



. = ALIGN(0x10);

\_\_heap\_base\_\_ = .;

\_\_heap\_end\_\_  = ORIGIN(EMBED\_RAM) + LENGTH(EMBED\_RAM);

```



\#### 11.4.3 堆内存分布图



&nbsp;   sysram0 区域 (0x64000000 - 0x6403FFFF, 256KB)

&nbsp;   +--------------------------------------------------+

&nbsp;    0x6403FFFF                                       

&nbsp;                                                     

&nbsp;     +--------------------------------------------+  

&nbsp;            FreeRTOS 堆内存                        

&nbsp;       (configTOTAL\_HEAP\_SIZE = 32KB)             

&nbsp;       - 任务 TCB 分配                            

&nbsp;       - 任务栈分配                               

&nbsp;       - 队列、信号量、事件组等                   

&nbsp;                                                  

&nbsp;       \_\_heap\_base\_\_ - 起始地址                 

&nbsp;       \_\_heap\_end\_\_   - 结束地址                 

&nbsp;     +--------------------------------------------+  

&nbsp;                                                     

&nbsp;    0x64000000                                       

&nbsp;   +--------------------------------------------------+



\### 11.5 任务栈与堆的关系



\#### 11.5.1 任务栈分配策略



FreeRTOS 中每个任务都有自己的独立栈空间：



&nbsp;  静态分配：在编译时确定大小，栈空间在 `.user\_stack` 区域外分配

&nbsp;  动态分配：任务创建时从 FreeRTOS 堆中分配栈内存



```c

&nbsp;动态分配任务栈 

xTaskCreate(vTaskFunction, TaskName, 1024, NULL, 1, NULL);



&nbsp;1024 = 栈深度（字）= 4KB 

```



\#### 11.5.2 任务栈与系统栈的关系



&nbsp;   ┌─────────────────────────────────────────────────────────────────┐

&nbsp;   │                    Cortex-R52 栈分布模型                        │

&nbsp;   ├─────────────────────────────────────────────────────────────────┤

&nbsp;   │                                                                  │

&nbsp;   │  User 模式 (任务代码运行)                                        │

&nbsp;   │  ┌──────────────────────────────────────────────────────────┐  │

&nbsp;   │  │  任务栈 (从 FreeRTOS 堆分配)                              │  │

&nbsp;   │  │  - 每个任务独立栈                                          │  │

&nbsp;   │  │  - 大小由 xTaskCreate() 第三个参数指定                    │  │

&nbsp;   │  │  - 默认最小 1024 字 (4KB)                                 │  │

&nbsp;   │  └──────────────────────────────────────────────────────────┘  │

&nbsp;   │                              │                                   │

&nbsp;   │                              v                                   │

&nbsp;   │  SVC 异常触发 (portYIELD())                                     │

&nbsp;   │                              │                                   │

&nbsp;   │                              v                                   │

&nbsp;   │  Supervisor 模式 (系统调用)                                      │

&nbsp;   │  ┌──────────────────────────────────────────────────────────┐  │

&nbsp;   │  │  Supervisor 栈 (1KB，链接脚本定义)                        │  │

&nbsp;   │  │  - 用于 SVC 异常处理                                      │  │

&nbsp;   │  │  - 用于任务切换时的上下文保存                              │  │

&nbsp;   │  └──────────────────────────────────────────────────────────┘  │

&nbsp;   │                              │                                   │

&nbsp;   │                              v                                   │

&nbsp;   │  IRQ 异常触发 (外设中断)                                         │

&nbsp;   │                              │                                   │

&nbsp;   │                              v                                   │

&nbsp;   │  IRQ 模式                                                         │

&nbsp;   │  ┌──────────────────────────────────────────────────────────┐  │

&nbsp;   │  │  IRQ 栈 (1KB，链接脚本定义)                               │  │

&nbsp;   │  │  - 用于中断处理                                           │  │

&nbsp;   │  │  - FreeRTOS Tick 中断处理                                │  │

&nbsp;   │  └──────────────────────────────────────────────────────────┘  │

&nbsp;   │                                                                  │

&nbsp;   └─────────────────────────────────────────────────────────────────┘



\#### 11.5.3 任务创建时的内存分配



&nbsp;   任务创建 xTaskCreate() 时的内存分配流程：



&nbsp;   1. 从 FreeRTOS 堆中分配 TCB (约 300+ 字节)

&nbsp;      ┌─────────────────────┐

&nbsp;      │    TCB 结构体       │  - pvPortMalloc(sizeof(TCB))

&nbsp;      │  - pxTopOfStack    │

&nbsp;      │  - uxPriority     │

&nbsp;      │  - xStateListItem │

&nbsp;      │  - pcTaskName     │

&nbsp;      │  - ...            │

&nbsp;      └─────────────────────┘



&nbsp;   2. 从 FreeRTOS 堆中分配任务栈 (usStackDepth × 4 字节)

&nbsp;      ┌─────────────────────┐

&nbsp;      │    任务栈空间        │  - pvPortMalloc(usStackDepth × 4)

&nbsp;      │  (1024 × 4 = 4KB)  │

&nbsp;      │                     │

&nbsp;      │  \[高地址]           │

&nbsp;      │  +--------------+   │

&nbsp;      │     栈帧数据   │   │ - 任务上下文保存区域

&nbsp;      │  +--------------+   │

&nbsp;      │     本地变量   │   │

&nbsp;      │  +--------------+   │

&nbsp;      │    函数调用   │   │

&nbsp;      │  +--------------+   │

&nbsp;      │  \[低地址]           │

&nbsp;      └─────────────────────┘



&nbsp;   3. 初始化任务栈 (pxPortInitialiseStack)

&nbsp;      - 设置初始栈帧，模拟任务被中断

&nbsp;      - 保存入口地址、参数、寄存器初始值



\### 11.6 完整内存布局总结



&nbsp;   +------------------+ 0x00000000

&nbsp;      NVM (Flash)      代码存储区域

&nbsp;      (varies)       

&nbsp;   +------------------+ 0x20000000 (DME\_iram)

&nbsp;       DME RAM         DME 处理器内部 RAM

&nbsp;   +------------------+ 0x48000000

&nbsp;       HyperRAM        外部扩展内存 (4MB)

&nbsp;   +------------------+ 0x60000000

&nbsp;                    

&nbsp;      EMBED\_RAM       主 RAM 区域 (512KB)

&nbsp;                    

&nbsp;     +------------+ 

&nbsp;      .text         代码段

&nbsp;     +------------+ 

&nbsp;      .data         已初始化数据

&nbsp;     +------------+ 

&nbsp;      .bss          未初始化数据

&nbsp;     +------------+ 

&nbsp;      FreeRTOS      动态内存堆

&nbsp;      Heap          (32KB)

&nbsp;     +------------+ 

&nbsp;      Supervisor    1KB

&nbsp;     +------------+ 

&nbsp;      Abort         1KB

&nbsp;     +------------+ 

&nbsp;      Undefined     1KB

&nbsp;     +------------+ 

&nbsp;      IRQ           1KB

&nbsp;     +------------+ 

&nbsp;      FIQ           1KB

&nbsp;     +------------+ 

&nbsp;      Hypervisor     4KB

&nbsp;     +------------+ 

&nbsp;      User          32KB (任务栈)

&nbsp;     +------------+ 

&nbsp;   +------------------+ 0x64000000

&nbsp;       sysram0        FreeRTOS 堆扩展 (256KB)

&nbsp;   +------------------+ 0x64400000

&nbsp;       sysram1        共享内存 (256KB)

&nbsp;   +------------------+ 0x68000000

&nbsp;       TCM A          ATCM (64KB) - 代码

&nbsp;   +------------------+ 0x68100000

&nbsp;       TCM B          BTCM (16KB) - 数据

&nbsp;   +------------------+ 0x68200000

&nbsp;       TCM C          STCM (16KB) - 堆栈

&nbsp;   +------------------+



\### 11.7 内存配置建议



&nbsp;配置项                      默认值   调整建议                        

&nbsp;-----------------------  ----  --------------------------- 

&nbsp;configTOTAL\_HEAP\_SIZE  32KB  根据任务数量调整，每任务约 4-8KB 栈 + TCB 

&nbsp;USER\_STACK\_SIZE        32KB  如有复杂递归调用可增大                 

&nbsp;IRQ\_STACK\_SIZE         1KB   中断处理简单通常够用                  

&nbsp;SUPERVISOR\_STACK\_SIZE  1KB   通常无需修改                      







\## 12. 总结



本文档详细介绍了 FreeRTOS 在 STMicroelectronics Stellar SR6P3c4 芯片 Cortex-R52 架构上的适配移植方案。关键要点总结如下：



1\.  移植层文件：SDK 已提供完整的移植层，位于 `middlewarefreertosFreeRTOSportableGCCARM\_CR52`



2\.  核心配置：FreeRTOSConfig.h 中需重点关注中断优先级配置（configUNIQUE\_INTERRUPT\_PRIORITIES 和 configMAX\_API\_CALL\_INTERRUPT\_PRIORITY）



3\.  系统时钟：使用 AGT 外设作为 Tick 时钟源，周期默认为 1ms



4\.  中断管理：通过 GIC 实现中断优先级管理，支持中断嵌套



5\.  应用开发：参考 uartFreeRTOS\_CLI 示例工程，按正确顺序初始化系统模块后调用 vTaskStartScheduler() 启动调度器







\## 参考资料



&nbsp;  FreeRTOS 官方文档：httpswww.freertos.org

&nbsp;  ARM Cortex-R52 技术参考手册

&nbsp;  Stellar SDK 官方示例代码






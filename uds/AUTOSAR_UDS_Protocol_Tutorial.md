# AUTOSAR UDS 协议详解与固件下载

> 适用对象：中高级汽车电子开发工程师
>
> 参考标准：ISO 14229-1:2020 | ISO 15765-2:2016 | AUTOSAR

---

## 目录

1. [概述](#1-概述)
2. [CAN传输层 (CANTP)](#2-can传输层-cantp)
3. [CAN接口层 (CANIF)](#3-can接口层-canif)
4. [UDS诊断会话层](#4-uds诊断会话层)
5. [固件下载协议 (0x34-0x37)](#5-固件下载协议-0x34-0x37)
6. [其他常用诊断服务](#6-其他常用诊断服务)
7. [总结与最佳实践](#7-总结与最佳实践)

---

## 1. 概述

### 1.1 UDS协议简介

**UDS (Unified Diagnostic Services)** 是ISO 14229标准定义的统一诊断服务协议，广泛应用于汽车ECU诊断和刷写。

**核心特点：**
- 基于Client-Server模式
- 请求-响应机制
- 会话层概念（Session）
- 功能单元（Functional Units）

**OSI分层：**

| 层级 | 标准 | 说明 |
|------|------|------|
| 应用层 | ISO 14229-1 | UDS服务 |
| 表示层 | ISO 14229-1 | 数据格式 |
| 会话层 | ISO 14229-1 | 会话管理 |
| 传输层 | ISO 15765-2 | CANTP |
| 数据链路层 | ISO 11898-1 | CAN |
| 物理层 | ISO 11898-1 | CAN Transceiver |

### 1.2 AUTOSAR诊断架构

AUTOSAR诊断架构中，各层模块协作关系如下：

```
┌─────────────────────────────────────┐
│         PDU Router (PDUR)           │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    CAN Interface (CANIF)           │  ← 负责CAN帧收发、PDU路由
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    CAN Transport Layer (CANTP)     │  ← 负责数据分帧、组装、流量控制
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    CAN Driver (CAN)                │  ← 驱动CAN控制器
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│    Diagnostic Event Manager (DEM)  │  ← DTC管理
│    Diagnostic Communication Manager │  ← 诊断通信管理
│    (DCM)                            │
└─────────────────────────────────────┘
```

**DCM (Diagnostic Communication Manager)：**
- 处理UDS诊断请求
- 管理诊断会话
- 调用PDUR进行数据传输

**DEM (Diagnostic Event Manager)：**
- 管理DTC（故障码）
- 存储诊断事件

### 1.3 文档范围说明

本文档涵盖：
- CANTP传输层协议详解
- CANIF接口层功能
- UDS诊断会话管理
- 固件下载协议（0x34-0x37）
- 常用诊断服务

不包含：安全访问（0x27）、CAN物理层细节

---

### 面试题

**Q1: UDS和OBD的区别是什么？**

A:
- **UDS (Unified Diagnostic Services)**：通用诊断协议，支持读取数据、写入数据、刷写固件、诊断会话管理等，应用于ECU开发、售后诊断
- **OBD (On-Board Diagnostics)**：排放相关诊断协议，主要用于排放监控，标准化程度高，侧重于故障码和实时数据

UDS通过CAN ID区分不同的ECU，OBD使用固定ID（0x7E8/0x7E0）进行诊断。

---

**Q2: AUTOSAR诊断栈中DCM和CANTP的关系是什么？**

A:
- **DCM**位于应用层，负责处理UDS诊断服务请求（解析请求、调用底层接口）
- **CANTP**位于传输层，负责将大于8字节的数据进行分帧传输，以及接收端的帧组装
- DCM通过PDUR调用CANTP的接口，CANTP完成分帧/组装后将数据交给PDUR，PDUR再交给DCM

---

**Q3: UDS诊断中常用的CAN ID有哪些？**

A:
- **物理寻址**：一对一的通信，使用ECU特定的CAN ID
  - 诊断请求：0x7xx（例：0x7E0, 0x7E2）
  - 诊断响应：0x7xx + 0x08（例：0x7E8, 0x7EA）
- **功能寻址**：一对多的广播通信
  - 诊断请求：0x7DF
  - 响应：各ECU使用自己的物理ID响应

---

## 2. CAN传输层 (CANTP)

### 2.1 CANTP功能概述

**CANTP (CAN Transport Layer)** 是ISO 15765-2定义的传输层协议，用于在CAN总线上传输超过8字节的数据。

**在AUTOSAR中的位置：**
- 上层：PDUR（PDU Router）
- 下层：CANIF（CAN Interface）

**核心功能：**
1. 数据分帧（发送端）
2. 数据组装（接收端）
3. 流量控制（Flow Control）
4. 错误处理与超时管理

### 2.2 帧类型详解

CANTP定义了4种帧类型，通过N_PCI（Network Protocol Control Information）标识：

| 帧类型 | N_PCI字节 | 用途 | 数据长度 |
|--------|-----------|------|----------|
| SF (Single Frame) | 0x0x | 单帧传输 | 0-7字节 |
| FF (First Frame) | 0x1x | 首帧传输 | 0-4095字节 |
| CF (Consecutive Frame) | 0x2x | 连续帧 | 1-7字节 |
| FC (Flow Control) | 0x3x | 流控制 | 0-3字节 |

**N_PCI结构：**

```
Single Frame (SF):
┌─────────┬─────────────────────────────────┐
│ N_PCI   │         Data                     │
│  1 byte │         0-7 bytes               │
└─────────┴─────────────────────────────────┘
[7:4] = 0000 (0x0)

First Frame (FF):
┌─────────┬───────────────┬─────────────────┐
│ N_PCI   │    Length     │     Data         │
│  1 byte │    2 bytes    │   0-4095 bytes   │
└─────────┴───────────────┴─────────────────┘
[7:4] = 0001 (0x1)     [11:0] = DL

Consecutive Frame (CF):
┌─────────┬─────────────────────────────────┐
│ N_PCI   │         Data                     │
│  1 byte │         1-7 bytes                │
└─────────┴─────────────────────────────────┘
[7:4] = 0010 (0x2)     [3:0] = SN

Flow Control (FC):
┌─────────┬───────────┬────────────┬─────────┐
│ N_PCI   │    FS     │    BS      │  STmin  │
│  1 byte │  1 byte   │  1 byte    │ 1 byte  │
└─────────┴───────────┴────────────┴─────────┘
[7:4] = 0011 (0x3)
```

**FS (Flow Status)：**
- 0x00：Continue (继续发送)
- 0x01：Wait (等待)
- 0x02：Overflow (溢出/取消)

**STmin (Separator Time minimum)：** 连续帧之间的最小间隔时间（ms）

**BS (Block Size)：** 发送方一次可发送的连续帧数量

**SN (Sequence Number)：** 连续帧序号，FF之后从0开始，模8循环

### 2.3 数据封装与分帧机制

**单帧传输示例（≤7字节数据）：**

```
请求：0x10 03 01 02 03     (5字节有效数据)
CAN帧：0x7E0  [00] 05 01 02 03     ← N_PCI=0x00 (SF)
                     ↑
                  SF标识 + 5字节数据
```

**多帧传输示例（>7字节数据，假设传输100字节）：**

```
Step 1: First Frame (FF)
CAN帧：0x7E0  [10] 00 64 [数据前6字节]  ← N_PCI=0x10, DL=100
                    └─FF标识─┘

Step 2: Flow Control (FC) - 接收方发送
CAN帧：0x7E8  [30] 00 0A 00            ← N_PCI=0x30
                    │  │  └── STmin=10ms
                    │  └───── BS=0 (无限流，可一直发送)
                    └──────── FS=0 (Continue)

Step 3-17: Consecutive Frames (CF)
CF 0: [20] [数据7字节]  SN=0
CF 1: [21] [数据7字节]  SN=1
...
CF 14: [2E] [数据7字节] SN=14
```

### 2.4 流量控制 (Flow Control)

流量控制机制用于协调发送速率，防止接收方来不及处理：

**FC参数作用：**

| 参数 | 含义 | 典型值 |
|------|------|--------|
| FS | 发送许可 | 0=继续, 1=等待, 2=终止 |
| BS | 块大小 | 0=无限流, N=每N帧后等待FC |
| STmin | 帧间隔 | 0x00-0x7F(ms), 0x80-0xF0(100us) |

**典型交互：**
```
发送方 ──FF(100字节)──> 接收方
接收方 ──FC(BS=8, STmin=20ms)──> 发送方
发送方 ──CF×8──> 接收方 (等待FC)
接收方 ──FC──> 发送方
发送方 ──CF×8──> 接收方
... (重复直到传完)
```

### 2.5 异常处理与超时机制

**CANTP超时参数：**

| 参数 | 说明 | 典型值 |
|------|------|--------|
| N_BS | 发送FF后等待FC的超时 | 1000ms |
| N_CR | 接收CF的超时 | 1000ms |
| N_Br | 接收方发送FC的响应时间 | - |
| N_Bs | 发送方等待FC的总时间 | 1500ms |
| N_Cr | 接收方等待CF的总时间 | 1500ms |
| N_WFT | 接收方等待FC时允许的最大重试次数 | 10 |

**异常处理流程：**
1. **N_BS超时**：发送方放弃本次传输，返回N_TIMEOUT错误
2. **N_CR超时**：接收方放弃接收，返回N_TIMEOUT错误
3. **N_WFT重试**：接收方发送Wait Frame后超时时，可重发FC（N_WFT次），仍无响应则终止

### 2.6 CANTP配置参数

AUTOSAR中CANTP的主要配置项：

```c
/* CANTP配置结构 */
typedef struct {
    /* 通道配置 */
    uint8 CANTpChannelId;

    /* 寻址格式 */
    CANTpAddressFormatType CANTpAddressFormat;  /* STANDARD/EXTENDED/MIXED */

    /* 传输参数 */
    uint16 CANTpTxStMin;        /* 发送STmin */
    uint16 CANTpRxStMin;       /* 接收STmin */
    uint8  CANTpBlockSize;      /* BS值 */

    /* 超时参数 */
    uint16 CANTpNcsBs;          /* N_BS超时 */
    uint16 CANTpNcrCr;          /* N_CR超时 */
    uint8  CANTpNWrftEx;        /* N_WFT最大重试 */

    /* 帧类型使能 */
    boolean CANTpFdf;           /* 是否支持CAN FD */
    boolean CANTpBrs;           /* 是否支持BRS */
} CANTP_ConfigType;
```

---

### 面试题

**Q1: CANTP如何处理大于8字节的诊断数据？**

A:
当诊断数据超过8字节（标准CAN）或64字节（CAN FD）时，CANTP执行分帧传输：
1. **首帧(FF)**：发送方发送首帧，包含总数据长度
2. **流控制(FC)**：接收方返回FC帧，携带BS（块大小）和STmin（帧间隔）参数
3. **连续帧(CF)**：发送方按FC指定的速率发送连续帧，每个CF包含7字节数据（标准CAN）
4. **循环发送**：按FC的BS参数重复"发送CF块→等待FC"流程，直到数据发送完毕

---

**Q2: 解释Flow Control中STmin和BS参数的含义**

A:
- **STmin (Separator Time minimum)**：连续帧之间的最小时间间隔，防止接收方处理不过来。发送方必须在上一帧CF发出后等待至少STmin毫秒才能发送下一帧。
- **BS (Block Size)**：块大小，指定发送方在等待下一个FC之前可以发送的连续帧数量。BS=0表示无限制，可以一直发送直到传完。

典型配置：BS=8, STmin=10ms 表示每发送8帧后等待接收方的FC，两帧之间至少间隔10ms。

---

**Q3: 当接收方CF超时时应如何处理？**

A:
接收方在等待连续帧时，会启动N_CR（Receiver Consecutive Frame Timeout）定时器：
1. 收到CF后，重置N_CR定时器
2. 如果N_CR超时（典型值1000ms），接收方认为发送方出现故障
3. 接收方放弃当前传输，向上层返回N_TIMEOUT_Cr错误
4. 上层（如DCM）可选择：
   - 终止本次诊断服务，返回NRC 0x10
   - 重试整个传输流程

---

**Q4: CANTP的N_WFT参数是什么？何时触发？**

A:
- **N_WFT (Number of Wait Frame Tolerance)**：接收方允许发送Wait Frame（FS=1）的最大次数
- **触发场景**：当接收方暂时无法处理更多数据时，发送FC(FS=1, Wait)让发送方等待
- **处理流程**：发送FC(Wait)后，接收方在N_Br时间内应恢复发送FC(Continue)或FC(Overflow)。如果连续发送N_WFT次Wait后仍未收到Continue，接收方终止传输并返回N_TIMEOUT_Bs错误

典型配置：N_WFT=10，表示最多等待10次

---

## 3. CAN接口层 (CANIF)

### 3.1 CANIF功能概述

**CANIF (CAN Interface)** 是AUTOSAR BSW中的CAN接口模块，负责：

1. **PDU路由**：在CAN驱动、CAN Tp、上层应用之间路由PDU
2. **传输模式控制**：管理PDU的发送/接收使能
3. **CAN帧收发**：调用CAN Driver进行实际收发
4. **确认机制**：向上层提供发送/接收结果确认

**在AUTOSAR架构中的位置：**

```
┌────────────────────────────────────────┐
│  Upper Layer (DCM, PDUR, App SWC)     │
└────────────────┬───────────────────────┘
                 │ CanIf_<Service>
┌────────────────▼───────────────────────┐
│           CANIF Layer                  │
│  - CanIf_Init                          │
│  - CanIf_Transmit                      │
│  - CanIf_ReadRxData                    │
└────────────────┬───────────────────────┘
                 │ Can_<Service>
┌────────────────▼───────────────────────┐
│         CAN Driver层                    │
└────────────────────────────────────────┘
```

### 3.2 PDU路由机制

**PDU (Protocol Data Unit)** 结构：

```c
typedef struct {
    PduIdType         swPduHandle;   /* PDU Handle */
    uint8             sduLength;     /* 数据长度 */
    uint8             sdu[8];         /* CAN数据 */
} Can_PduType;
```

**CANIF中的PDU ID类型：**

| 类型 | 说明 |
|------|------|
| CanIfTxPduId | 发送PDU ID |
| CanIfRxPduId | 接收PDU ID |
| CanIfSduId | 内部PDU ID |

**路由配置示例：**

```c
/* CANIF配置 - 接收路径 */
CanIfRxPduCfg CanIfRxPduConfig[] = {
    {
        .CanIfRxPduId = 0,
        .CanIfRxPduCanId = 0x7E8,      /* 诊断响应CAN ID */
        .CanIfRxPduDlc = 8,
        .CanIfRxPduType = CANIF_PDU_TYPE_STANDARD,
        .CanIfRxPduDataLen = 8,
        .CanIfRxPduUserType = CANIF_USER_TYPE_DCM,
        .CanIfRxPduInitConfig = CANIF_INITIAL_DEFAULT
    },
    // ... 更多接收PDU配置
};
```

### 3.3 传输模式控制

CANIF支持三种传输模式：

| 模式 | 说明 |
|------|------|
| **CANIF_TX_OFFLINE** | 离线，所有发送被禁用 |
| **CANIF_TX_ONLINE** | 正常发送 |
| **CANIF_TX_ONCE** | 只发送一次 |
| **CANIF_TX_OFFLINE_ACTIVE** | 离线但可触发回调 |

**动态传输模式切换：**

```c
/* 设置PDU的传输模式 */
Std_ReturnType CanIf_SetPduTxMode(
    PduIdType CanIfTxPduId,
    CanIf_TransmitMode TxMode
);

/* 设置控制器传输模式 */
Std_ReturnType CanIf_SetControllerMode(
    uint8 ControllerId,
    CanIf_ControllerModeType ControllerMode
);
```

### 3.4 与CANTP的交互

CANIF与CANTP的接口关系：

```
CANTP                         CANIF
  │                              │
  │─ CanIf_Transmit() ─────────>│
  │                              │
  │<─ CanIf_TxConfirmation() ───│
  │                              │
  │─ CanIf_ReadRxData() ───────>│
  │                              │
  │<─ CanIf_RxIndication() ─────│
```

**CANTP调用CANIF的接口：**

```c
/* CANTP发送接口 */
Std_ReturnType CanIf_Transmit(
    PduIdType TxPduId,
    const PduInfoType* PduInfoPtr
);

/* CANTP接收数据 */
Std_ReturnType CanIf_ReadRxData(
    PduIdType CanIfRxPduId,
    PduInfoType* PduInfoPtr
);
```

---

### 面试题

**Q1: CANIF和CANTP的区别是什么？各自负责什么？**

A:
- **CANIF (CAN Interface)**：CAN接口层，负责CAN帧的路由、传输模式控制、收发确认，不关心数据内容，只负责将数据发送到指定CAN ID或从指定CAN ID接收
- **CANTP (CAN Transport Layer)**：传输层，负责数据分帧和组装。当数据超过8字节时，CANTP负责将数据分成多个CAN帧发送，并处理流量控制

简单比喻：CANIF是"邮局"，负责信件的收发；CANTP是"快递公司"，负责大件货物的分批运输。

---

**Q2: 解释CANIF的传输模式机制**

A:
CANIF提供多层传输模式控制：

1. **控制器级别**：CanIf_SetControllerMode控制整个CAN控制器的收发
   - CANIF_SET_START：启动CAN控制器收发
   - CANIF_SET_STOP：停止收发
   - CANIF_SET_SLEEP：进入睡眠

2. **PDU级别**：CanIf_SetPduTxMode控制单个PDU的发送
   - CANIF_TX_ONLINE：正常发送
   - CANIF_TX_OFFLINE：禁用发送
   - CANIF_TX_ONCE：只发送一次后自动转为OFFLINE

这种分层控制使得CANIF可以灵活地管理不同PDU的发送状态，例如在诊断刷写过程中可以禁用非关键报文的发送。

---

**Q3: CANIF如何实现PDU的路由？**

A:
CANIF通过配置表实现PDU路由：

1. **接收路由**：根据接收到的CAN ID，在CanIfRxPduConfig中查找对应的PDU配置，将数据传递给指定的上层模块（如DCM、CANTP）

2. **发送路由**：上层模块调用CanIf_Transmit时传入TxPduId，CANIF根据CanIfTxPduConfig配置查找目标CAN ID和控制器

CANIF配置中定义了CanIfRxPduUserType和CanIfTxPduUserType，用于指定数据应该交给哪个上层模块处理。

---

## 4. UDS诊断会话层

### 4.1 会话层状态机

UDS诊断会话由ISO 14229-1定义的会话层状态机管理：

```
                    ┌─────────────────┐
                    │  Default Session │
                    │   (默认会话)      │
                    └────────┬────────┘
                             │ 0x10 01
                             ▼
              ┌──────────────────────────┐
              │ Extended Diagnostic Session│
              │   (扩展诊断会话)            │
              └────────────┬───────────────┘
                           │ 0x10 02
                           ▼
              ┌──────────────────────────┐
              │ Programming Session       │
              │   (编程会话)              │
              └──────────────────────────┘
```

**会话层状态说明：**

| 状态 | 说明 | 可用服务 |
|------|------|----------|
| Default Session | 默认状态 | 基本诊断服务 |
| Extended Diagnostic Session | 扩展诊断 | 所有诊断服务，包括0x34刷写 |
| Programming Session | 编程会话 | 固件下载/上传服务 |

### 4.2 0x10 会话控制服务

**服务ID：** 0x10

**请求格式：**

```
┌──────┬────────────┐
│ SID  │ subFunction│
│ 0x10 │ 0x01/02/03 │
└──────┴────────────┘
```

**子功能：**

| 值 | 会话类型 | 说明 |
|----|----------|------|
| 0x01 | Default Session | 默认会话 |
| 0x02 | Programming Session | 编程会话 |
| 0x03 | Extended Diagnostic Session | 扩展诊断会话 |

**响应格式：**

```
┌──────┬────────────┬───────────────────┐
│ SID  │  response  │   sessionType     │
│ 0x50 │ + 0x40     │  0x01/02/03       │
└──────┴────────────┴───────────────────┘
      ↑
   正响应 = 请求SID + 0x40
```

**示例 - 请求进入编程会话：**

```
请求: 0x10 02              ← SID=0x10, subFunction=0x02
响应: 0x50 02 xx xx        ← SID=0x50, subFunction=0x02, P2Server/P2*Server参数
```

### 4.3 0x3E 测试present服务

**服务ID：** 0x3E

**作用：** 保持当前诊断会话活跃，防止因S3Server超时而退出会话

**请求格式：**

```
┌──────┬────────────┐
│ SID  │ subFunction│
│ 0x3E │ 0x00/0x80 │
└──────┴────────┘
```

- 0x00： testerPresent，不显示在CAN总线
- 0x80： withResponse，不显示在CAN总线，但要求ECU响应

**示例：**

```
请求: 0x3E 00
响应: 0x7E 00        ← 响应：SID+0x40
```

### 4.4 0x11 ECU复位服务

**服务ID：** 0x11

**请求格式：**

```
┌──────┬────────────┐
│ SID  │ subFunction│
│ 0x11 │ 0x01/02/03 │
└──────┴────────────┘
```

**子功能：**

| 值 | 类型 | 说明 |
|----|------|------|
| 0x01 | hardReset | 硬件复位，完全断电重启 |
| 0x02 | keyOffOn | 钥匙开关复位 |
| 0x03 | softReset | 软件复位，应用层软重启 |
| 0x04 | enableRapidPowerShutDown | 快速断电 |

**示例：**

```
请求: 0x11 01
响应: 0x51 01           ← ECU开始执行硬复位
```

### 4.5 会话超时处理

**P2Server/P2*Server时序：**

| 参数 | 说明 | 典型值 |
|------|------|--------|
| P2Server | ECU响应超时时间 | 50ms |
| P2*Server | P2Server超时后的扩展响应时间 | 5000ms |
| S3Server | 诊断会话保持超时 | 10000ms |

**超时行为：**

```
发送0x3E 00
    │
    ▼
┌──────────────────────┐
│ 计时S3Server (10s)   │
└──────────┬───────────┘
           │ 收到0x3E
           ▼ 重置计时器

           │ S3Server超时
           ▼
    ┌──────────────┐
    │ 退出会话      │
    │ 返回默认会话   │
    └──────────────┘
```

---

### 面试题

**Q1: Default Session和Programming Session的区别？**

A:
- **Default Session（默认会话）**：
  - ECU上电后的默认状态
  - 支持有限的诊断服务：0x10, 0x11, 0x19, 0x22等
  - 不允许执行固件下载（0x34/0x35/0x36/0x37）

- **Programming Session（编程会话）**：
  - 必须通过0x10 02显式请求进入
  - 允许执行固件下载/上传服务（0x34-0x37）
  - 通常需要安全访问（0x27）配合
  - 会话超时时间可能更短

两种会话的根本区别在于权限级别，编程会话允许修改ECU内存，因此需要更严格的安全机制。

---

**Q2: 解释P2Server和P2*Server时序**

A:
- **P2Server**：ECU正常响应超时时间，典型值50ms，表示ECU应在P2Server时间内返回诊断响应
- **P2*Server**：当ECU需要更长时间处理请求（如进入复杂诊断流程）时，首先响应NRC 0x78（requestCorrectlyReceived-Pending），然后在P2*Server时间（典型值5000ms）内完成处理并返回最终响应

典型交互：
```
Tester ── 0x22请求 ──> ECU
Tester <── 0x78 ──     ECU (Pending响应，表示正在处理)
Tester <── 0x62 ──     ECU (最终响应，在P2*Server内)
```

0x78 (ResponsePending) 让Tester知道ECU正在处理，避免Tester因P2Server超时而放弃等待。

---

**Q3: 0x3E服务的作用是什么？**

A:
0x3E (TesterPresent) 用于保持诊断会话活跃，防止因S3Server超时而退出会话。

- ECU在诊断会话中会启动S3Server定时器（典型值10秒）
- 如果10秒内没有收到任何诊断请求，ECU自动退回到Default Session
- Tester定期发送0x3E 00，重置S3Server计时器，保持会话
- 如果不发送0x3E，会话会在S3Server超时后丢失，所有之前配置的诊断参数需要重新设置

---

**Q4: ECU复位后会话状态如何变化？**

A:
- **0x11硬复位/软复位后**：ECU执行复位过程，复位完成后进入Default Session（0x01），所有之前激活的扩展会话或编程会话都会丢失
- **从Default Session进入其他会话**：0x10 02/03激活新会话
- **从编程会话返回**：ECU复位会退回到Default Session，或通过0x10 01主动返回Default Session

**重要**：固件下载完成后通常需要执行0x11复位，复位前应确保0x37 TransferExit已正确执行。

---

## 5. 固件下载协议 (0x34-0x37)

### 5.1 请求下载 (0x34)

**服务ID：** 0x34 - RequestDownload

**作用：** 客户端请求向服务器（ECU）下载数据，指定内存地址和大小

**请求格式：**

```
┌──────┬────────────┬───────────────┬──────────────┬─────────────┐
│ SID  │ dataFormat │ addressLength │   memoryAddr │  memorySize │
│ 0x34 │ Identifier │   Format      │   (变长)     │   (变长)    │
└──────┴────────────┴───────────────┴──────────────┴─────────────┘
```

**字段说明：**

| 字段 | 字节数 | 说明 |
|------|--------|------|
| dataFormatIdentifier | 1 | 压缩/加密格式（0x00=无压缩无加密） |
| addressAndLengthFormatIdentifier | 1 | 地址和长度格式，bit7-4=地址字节数，bit3-0=长度字节数 |
| memoryAddress | 1-4 | 目标内存地址 |
| memorySize | 1-4 | 数据大小 |

**addressAndLengthFormatIdentifier编码：**

```
bit7-4: memoryAddress字节数    bit3-0: memorySize字节数
0x00 = 1字节      0x00 = 1字节
0x11 = 2字节      0x11 = 2字节
0x22 = 3字节      0x22 = 3字节
0x33 = 4字节      0x33 = 4字节
```

**示例：写入0x8000地址，100字节数据**

```
请求: 34 00 44 00 80 00 00 64
       │ │ │ └──────────── memorySize = 0x64 (100字节)
       │ │ └───────────── address = 0x008000 (3字节)
       │ └────────────── 0x44 = 地址3字节+长度4字节
       └──────────────── 0x00 = 无压缩无加密

响应: 74 20 04 F5    ← 正响应：SID+0x40, maxNumberOfBlockLength=0x04F5
       │    └──────── 块长度参数，指定0x36服务每次可传输的最大数据量
       └───────────── maxNumberOfBlockLength = 0x04F5
```

### 5.2 请求上传 (0x35)

**服务ID：** 0x35 - RequestUpload

**作用：** 客户端请求从服务器（ECU）上传数据

**请求格式：** 与0x34类似

```
┌──────┬────────────┬───────────────┬──────────────┬─────────────┐
│ SID  │ dataFormat │ addressLength │   memoryAddr │  memorySize │
│ 0x35 │ Identifier │   Format      │   (变长)     │   (变长)    │
└──────┴────────────┴───────────────┴──────────────┴─────────────┘
```

**示例：读取0x8000地址，100字节数据**

```
请求: 35 00 44 00 80 00 00 64
响应: 74 20 04 F5
```

0x35响应格式与0x34相同，后续使用0x36下载数据。

### 5.3 传输数据 (0x36)

**服务ID：** 0x36 - TransferData

**作用：** 实际传输数据块

**请求格式：**

```
┌──────┬───────────────┬────────────┐
│ SID  │ blockSequence │   data     │
│ 0x36 │   Counter     │   (变长)   │
└──────┴───────────────┴────────────┘
```

**字段说明：**

| 字段 | 说明 |
|------|------|
| blockSequenceCounter | 块序号，从1开始，每次请求+1，模256循环 |
| data | 传输的数据，长度受0x34响应中的maxNumberOfBlockLength限制 |

**示例：**

```
请求: 36 01 [64字节数据]     ← 块序号1，64字节
响应: 76 01                 ← 正响应，返回相同块序号

请求: 36 02 [64字节数据]     ← 块序号2
响应: 76 02                 ← 块序号2的响应

请求: 36 03 [剩余数据]       ← 块序号3
响应: 76 03
```

### 5.4 请求传输Exit (0x37)

**服务ID：** 0x37 - RequestTransferExit

**作用：** 结束数据传输

**请求格式：**

```
┌──────┐
│ SID  │
│ 0x37 │
└──────┘
(无其他参数)
```

**响应格式：**

```
┌──────┐
│ SID  │
│ 0x77 │
└──────┘
```

**示例：**

```
请求: 37
响应: 77
```

### 5.5 块传输机制详解

**完整交互流程：**

```
Step 1: 进入编程会话
Tester ── 10 02 ──> ECU
Tester <── 50 02 ──  ECU (响应)

Step 2: 请求下载
Tester ── 34 00 44 00 80 00 00 64 ──> ECU  (下载100字节到0x8000)
Tester <── 74 20 04 ──                ECU  (maxNumberOfBlockLength=4)

Step 3: 传输数据 (0x36)
Tester ── 36 01 [data 4字节] ──> ECU
Tester <── 76 01 ──                  ECU
Tester ── 36 02 [data 4字节] ──> ECU
Tester <── 76 02 ──                  ECU
... (重复直到传完100字节)

Step 4: 结束传输
Tester ── 37 ──> ECU
Tester <── 77 ──  ECU

Step 5: 复位 (可选)
Tester ── 11 01 ──> ECU
Tester <── 51 01 ──  ECU
```

**maxNumberOfBlockLength：**
- 表示0x36服务每次传输的最大数据量
- 由服务器（ECU）在0x34响应中指定
- 例如：0x04F5 = 1269字节，实际使用时需减去1字节的blockSequenceCounter

### 5.6 完整交互流程示例

**典型的固件下载完整流程：**

```
1. 会话切换
   C: 10 02                    → 进入编程会话
   E: 50 02 xx xx              ← 响应

2. 安全访问 (可选，如需要)
   C: 27 01                    → 请求种子
   E: 67 01 xx xx              ← 返回种子
   C: 27 02 [密钥]             → 发送密钥
   E: 67 02                    ← 解锁成功

3. 请求下载
   C: 34 00 44 00 80 00 00 64  → 下载100字节到0x8000
   E: 74 20 04                  ← 每次最多传输4字节(实际是3字节数据+1字节序号)

4. 传输数据
   C: 36 01 [3字节数据]        → 第1块
   E: 76 01
   C: 36 02 [3字节数据]        → 第2块
   E: 76 02
   ...
   C: 36 22 [3字节数据]        → 第34块 (100/3=33.3，共34块)
   E: 76 22

5. 结束传输
   C: 37                       → 请求结束
   E: 77                       ← 确认结束

6. 校验 (可选，使用0x31)
   C: 31 01 02 03 [参数]       → 执行完整性校验
   E: 71 01 02 03              ← 校验通过

7. 复位
   C: 11 01                    → 硬复位
   E: 51 01
```

---

### 面试题

**Q1: 0x34请求中如何编码内存地址和大小？**

A:
0x34请求中通过addressAndLengthFormatIdentifier字段指定地址和长度的字节数：

- **bit7-4**：memoryAddress的字节数（0x1=1字节，0x2=2字节，0x3=3字节，0x4=4字节）
- **bit3-0**：memorySize的字节数

示例：
- 0x14 = 地址1字节 + 长度4字节
- 0x24 = 地址2字节 + 长度4字节
- 0x34 = 地址3字节 + 长度4字节
- 0x44 = 地址4字节 + 长度4字节

例如0x34 00 44 00 80 00 00 64：
- 00 = dataFormat（无压缩无加密）
- 44 = 地址4字节(0x4) + 长度4字节(0x4)
- 00 80 00 = 内存地址0x8000（3字节）
- 00 00 00 64 = 内存大小100字节（4字节）

---

**Q2: 0x36服务中blockSequenceCounter的作用？**

A:
blockSequenceCounter（块序号）用于：
1. **数据顺序标识**：确保数据块按正确顺序重组
2. **丢包检测**：如果接收到的序号不连续，说明有数据丢失
3. **通信同步**：接收方通过序号确认当前接收的是第几个数据块

序号从1开始递增，每次0x36请求后+1，模256循环。接收方通过验证序号是否连续来检测传输错误。

---

**Q3: 描述完整的0x34-0x37固件下载流程**

A:
1. **0x10 02** - 进入编程会话（必须）
2. **0x34** - 请求下载，指定目标地址和数据大小，ECU返回maxNumberOfBlockLength
3. **循环0x36** - 按块传输数据，每个数据块包含blockSequenceCounter和实际数据
4. **0x37** - 传输完成后请求结束传输
5. **0x11** - （可选）复位ECU使新固件生效

关键点：
- 0x34必须在Programming Session下执行
- 0x34响应中的maxNumberOfBlockLength决定了0x36每次传输的数据量上限
- 0x36序号必须连续，如有丢失需要重新开始传输

---

**Q4: 如果0x36过程中出现错误如何恢复？**

A:
0x36传输过程中常见错误及恢复方式：

1. **序号不连续**：接收方检测到blockSequenceCounter跳变，返回NRC 0x23（条件不满足），需要从上一个正确的块重新发送

2. **超时**：如果等待0x36响应超时（超过P2Server），可以重试当前块，或重新从0x34开始

3. **通信中断**：如果CAN总线出现错误，通常需要完整重新开始传输流程（从0x34开始）

**恢复策略：**
- 实现断点续传机制，记录最后成功接收的块序号
- 在0x34中指定偏移量继续下载（需ECU支持）
- 必要时执行0x11复位重新开始

---

**Q5: 为什么固件下载需要切换到Programming Session？**

A:
原因有三：

1. **安全考虑**：Programming Session是专门用于执行关键操作（如固件下载）的会话类型，需要更严格的权限控制

2. **功能限制**：Default Session下不允许执行0x34-0x37服务，只有Programming Session或Extended Diagnostic Session（部分）才允许

3. **资源独占**：进入Programming Session后，ECU会禁用非必要的通信和任务，确保刷写过程不受干扰，提高稳定性

4. **状态隔离**：刷写过程中可能出现异常（如突然断电），Programming Session有专门的状态管理机制来处理这些情况

---

## 6. 其他常用诊断服务

### 6.1 0x19 读取DTC信息

**服务ID：** 0x19 - ReadDTCInformation

**作用：** 读取故障码信息

**子功能：**

| 子功能 | 说明 |
|--------|------|
| 0x01 | reportNumberOfDTCByStatusMask |
| 0x02 | reportDTCByStatusMask |
| 0x04 | reportDTCSnapshotRecordByDTCNumber |
| 0x06 | reportDTCExtDataRecordByDTCNumber |
| 0x0A | reportAllDTC |

**DTC格式：**

```
┌──────────┬──────────┬──────────┐
│ DTCHigh  │ DTCLow   │ Status   │
│  1 byte  │  1 byte  │  1 byte  │
└──────────┴──────────┴──────────┘
```

**Status字节含义：**

| 位 | 说明 |
|----|------|
| bit0 | testFailed |
| bit1 | testFailedThisOperationCycle |
| bit2 | pendingDTC |
| bit3 | confirmedDTC |
| bit4 | testNotCompletedSinceLastClear |
| bit5 | testFailedSinceLastClear |
| bit6 | testNotCompletedThisOperationCycle |
| bit7 | warningIndicatorRequested |

**示例：**

```
请求: 19 02 01          ← 按状态掩码读取DTC，掩码=01
响应: 59 02 01 01 12 34 85  ← 正响应，有1个DTC: 0x1234, Status=0x85
                              (bit7=1, bit3=1, bit2=1, bit0=1)
```

### 6.2 0x14 清除DTC信息

**服务ID：** 0x14 - ClearDiagnosticInformation

**作用：** 清除存储的DTC信息

**请求格式：**

```
┌──────┬──────────┬──────────┬──────────┐
│ SID  │ DTCHigh  │ DTCLow   │ Status   │
│ 0x14 │  1 byte  │  1 byte  │  1 byte  │
└──────┴──────────┴──────────┴──────────┘
```

**示例：清除所有DTC**

```
请求: 14 FF FF FF          ← FF FF FF 表示清除所有DTC
响应: 54                   ← 正响应
```

### 6.3 0x22 读取数据

**服务ID：** 0x22 - ReadDataByIdentifier

**作用：** 按DID（Data Identifier）读取数据

**请求格式：**

```
┌──────┬────────────┬────────────┐
│ SID  │  DID High │  DID Low   │
│ 0x22 │   1 byte  │   1 byte   │
└──────┴────────────┴────────────┘
```

**示例：读取VIN码**

```
请求: 22 F1 90            ← DID = 0xF190 (标准VIN DID)
响应: 62 F1 90 57 4C ...  ← 正响应，返回17字节VIN
```

### 6.4 0x2E 写入数据

**服务ID：** 0x2E - WriteDataByIdentifier

**作用：** 按DID写入数据

**请求格式：**

```
┌──────┬────────────┬────────────┬────────────┐
│ SID  │  DID High │  DID Low   │   Data     │
│ 0x2E │   1 byte  │   1 byte   │   (变长)   │
└──────┴────────────┴────────────┴────────────┘
```

**示例：写入VIN码**

```
请求: 2E F1 90 57 4C ...  ← 写入17字节VIN
响应: 6E F1 90             ← 正响应，仅返回DID
```

---

### 面试题

**Q1: 0x19服务有哪些常用子功能？**

A:
- **0x01** (reportNumberOfDTCByStatusMask)：按状态掩码统计DTC数量
- **0x02** (reportDTCByStatusMask)：按状态掩码列出所有DTC
- **0x04** (reportDTCSnapshotRecordByDTCNumber)：读取指定DTC的快照数据（故障发生时的环境数据）
- **0x06** (reportDTCExtDataRecordByDTCNumber)：读取指定DTC的扩展数据（计数器、计时器等）
- **0x0A** (reportAllDTC)：读取所有DTC

---

**Q2: DTC的Status字节各位含义是什么？**

A:
| 位 | 名称 | 说明 |
|----|------|------|
| 0 | testFailed | 当前操作周期内测试失败 |
| 1 | testFailedThisOperationCycle | 本操作周期内测试失败过 |
| 2 | pendingDTC | 待定DTC（当前或上一周期失败） |
| 3 | confirmedDTC | 确认DTC（跨周期确认） |
| 4 | testNotCompletedSinceLastClear | 自清除后测试未完成 |
| 5 | testFailedSinceLastClear | 自清除后测试失败过 |
| 6 | testNotCompletedThisOperationCycle | 本周期测试未完成 |
| 7 | warningIndicatorRequested | 请求警告指示 |

典型的confirmed DTC状态：bit3=1 (confirmedDTC)

---

**Q3: 0x22和0x2E与0x34的区别是什么？**

A:
- **0x22 (ReadDataByIdentifier)**：按DID读取数据，用于获取ECU运行数据、配置信息等，数据量较小（通常≤100字节），单帧传输即可

- **0x2E (WriteDataByIdentifier)**：按DID写入数据，用于配置参数写入、标定等，数据量较小

- **0x34 (RequestDownload)**：用于大块数据传输（固件、参数组等），支持分块传输，是固件升级的核心服务

| 特性 | 0x22/0x2E | 0x34-0x37 |
|------|-----------|-----------|
| 数据量 | 小（<100字节） | 大（可达数MB） |
| 传输方式 | 单帧 | 多帧分块 |
| 目标区域 | NVM/变量 | Flash/内存映射区域 |
| 典型应用 | 配置读写 | 固件下载 |

---

## 7. 总结与最佳实践

### 7.1 协议实现关键点

1. **CANTP配置**：根据车型确定STmin、BS参数，注意CAN FD与标准CAN的区别

2. **会话管理**：正确处理0x10会话切换，确保0x34前已进入正确会话

3. **超时处理**：配置合理的P2Server、N_BS、N_CR超时参数

4. **数据校验**：0x37传输Exit后可调用0x31进行数据校验

5. **错误恢复**：实现完整的异常处理和重试机制

### 7.2 常见问题与排查方法

| 问题现象 | 可能原因 | 排查方法 |
|----------|----------|----------|
| 0x34无响应 | 未进入编程会话 | 检查0x10响应 |
| 0x36传输中断 | CF超时/序号错误 | 检查N_CR超时和序号 |
| 会话自动退出 | S3Server超时 | 检查0x3E周期性发送 |
| 写入失败 | 内存地址错误/权限不足 | 验证0x34地址参数 |

### 7.3 面试高频问题汇总

**Q1: UDS诊断开发中最容易遇到的问题是什么？**

A:
- 会话状态管理不当导致服务无法执行
- CANTP分帧/组装错误导致数据传输失败
- 超时参数配置不合理导致通信不稳定
- 多任务环境下线程安全问题

---

**Q2: 如何调试UDS通信问题？**

A:
1. 使用CANoe/PCAN等工具抓取CAN报文，确认帧格式正确
2. 验证诊断请求的SID和参数编码
3. 检查0x78 Pending响应是否频繁
4. 确认CANTP的STmin/BS参数与工具端匹配
5. 使用日志追踪DCM/CANTP/PDUR各层状态

---

**Q3: 固件下载失败后如何处理？**

A:
1. 判断失败阶段：0x34阶段/0x36阶段/0x37阶段
2. 根据NRC错误码确定错误类型
3. 可选择：
   - 重新从0x34开始完整下载
   - 断点续传（如ECU支持）
   - 执行0x11复位后重试
4. 检查flash driver是否正常，flash是否被占用

---

### 附录

**常用UDS服务SID速查表：**

| SID | 服务名称 |
|-----|----------|
| 0x10 | DiagnosticSessionControl |
| 0x11 | ECUReset |
| 0x14 | ClearDiagnosticInformation |
| 0x19 | ReadDTCInformation |
| 0x22 | ReadDataByIdentifier |
| 0x2E | WriteDataByIdentifier |
| 0x31 | RoutineControl |
| 0x34 | RequestDownload |
| 0x35 | RequestUpload |
| 0x36 | TransferData |
| 0x37 | RequestTransferExit |
| 0x3E | TesterPresent |

---

*文档版本：v1.0*
*更新时间：2025-03-17*

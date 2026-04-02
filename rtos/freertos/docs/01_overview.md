# 第一章：FreeRTOS 概述与架构

> 本章目标：建立 FreeRTOS 整体认知，理解架构组成

## 章节结构

- [ ] 1.1 FreeRTOS 简介
- [ ] 1.2 内核架构
- [ ] 1.3 代码结构
- [ ] 1.4 核心概念
- [ ] 1.5 面试高频问题
- [ ] 1.6 避坑指南

---

## 1.1 FreeRTOS 简介

### 什么是 FreeRTOS

FreeRTOS 是一个专为嵌入式系统设计的实时操作系统内核，具有以下特点：

- **开源免费** — MIT 许可证，商业友好
- **轻量级** — 内核代码精简（仅 3-4 个 C 文件）
- **可裁剪** — 通过配置头文件去除不需要的功能
- **跨平台** — 支持 ARM、RISC-V、MIPS、x86 等 40+ 架构
- **实时性** — 确定性调度，响应时间可预测

### 与其他 RTOS 对比

| 特性 | FreeRTOS | RT-Thread | uCOS-II/III |
|------|----------|-----------|--------------|
| 授权 | MIT | Apache 2.0 | 闭源/商用 |
| 社区 | 庞大 | 国内活跃 | 较小 |
| 生态 | AWS 官方支持 | 国产生态 | 较封闭 |
| 学习曲线 | 低 | 中 | 中高 |

---

## 1.2 内核架构

### 整体架构图

```
┌──────────────────────────────────────┐
│           Application Layer          │  ← 用户应用
├──────────────────────────────────────┤
│  Tasks │ Queue │ Semaphore │ Timer  │  ← 任务层
├──────────────────────────────────────┤
│         FreeRTOS Kernel Core        │  ← 内核核心
│   Scheduler │ Task Control │ Memory │     调度器/任务控制/内存
├──────────────────────────────────────┤
│           Hardware Abstraction       │  ← 硬件抽象层（移植层）
│      Port │ Interrupt │ Tick Timer  │
├──────────────────────────────────────┤
│              Hardware                │  ← MCU/处理器
└──────────────────────────────────────┘
```

### 核心组件

1. **调度器（Scheduler）** — 决定何时运行哪个任务
2. **任务（Task）** — 基本执行单元
3. **队列（Queue）** — 任务间通信
4. **信号量（Semaphore）** — 资源同步
5. **互斥量（Mutex）** — 互斥访问
6. **软件定时器（Software Timer）** — 回调机制
7. **内存管理（Memory）** — 堆内存分配策略

---

## 1.3 代码结构

### 核心文件

```
FreeRTOS/
├── Source/
│   ├── tasks.c          ← 任务管理核心
│   ├── queue.c          ← 队列和信号量
│   ├── list.c           ← 内核链表实现
│   ├── event_groups.c   ← 事件组
│   ├── timers.c         ← 软件定时器
│   ├── croutine.c       ← 协程（已弃用）
│   └── portable/
│       ├── MemMang/     ← 内存管理策略
│       │   ├── heap_1.c
│       │   ├── heap_2.c
│       │   ├── heap_3.c
│       │   ├── heap_4.c
│       │   └── heap_5.c
│       └── ARM_CM4F/    ← ARM Cortex-M 移植层
│           └── port.c
├── include/
│   ├── FreeRTOS.h
│   ├── task.h
│   ├── queue.h
│   ├── semphr.h
│   ├── mutex.h
│   └── ...（共 30+ 头文件）
└── Demo/
    └── STM32F4xx_RTOS_Demo/
```

### 必读源码文件

| 文件 | 重要性 | 必读原因 |
|------|--------|---------|
| `tasks.c` | ⭐⭐⭐ | 任务调度的核心，理解内核的钥匙 |
| `list.c` | ⭐⭐ | 调度器依赖的链表操作 |
| `queue.c` | ⭐⭐ | 消息队列、信号量、互斥量实现 |
| `port.c` | ⭐⭐⭐ | 架构相关代码，任务切换原理 |

---

## 1.4 核心概念

### 任务状态机

```
        ┌─────────────┐
        │   Running   │ ← 当前正在执行
        └──────┬──────┘
               │
    ┌──────────┼──────────┐
    ▼          ▼          ▼
┌──────┐  ┌────────┐  ┌────────┐
│Ready │  │Blocked │  │Suspended│
└──┬───┘  └────┬───┘  └────────┘
   │           │
   │    ┌──────┴──────┐
   │    ▼             ▼
   │  ┌────────┐  ┌─────────┐
   └──│Delayed │  │  Event  │
      │  Wait  │  │  Wait   │
      └────────┘  └─────────┘
```

**四种状态：**
- **Running** — 任务正在 CPU 上执行
- **Ready** — 任务已就绪，等待调度器选中
- **Blocked** — 任务在等待延时或事件
- **Suspended** — 任务被 `vTaskSuspend()` 挂起，不参与调度

### 任务优先级

- 数值越大优先级越高（默认 0-configMAX_PRIORITIES-1）
- 同优先级任务采用时间片轮转（Time Slicing）
- 优先级可动态修改（`vTaskPrioritySet`）

### 栈空间

每个任务拥有独立的栈，栈大小在创建时指定：

```c
TaskHandle_t xTaskCreate(
    TaskFunction_t pvTaskCode,    // 任务函数
    const char *pcName,           // 任务名称（仅调试用）
    uint16_t usStackDepth,        // 栈深度（字为单位，STM32 一字=4字节）
    void *pvParameters,           // 传递给任务的参数
    UBaseType_t uxPriority,       // 优先级
    TaskHandle_t *pxCreatedTask   // 任务句柄
);
```

---

## 1.5 面试高频问题

### Q1：FreeRTOS 的特点是什么？适合哪些场景？

**参考答案要点：**
- 开源免费、可裁剪、跨平台、实时性好
- 适合资源受限的 MCU 设备，如 STM32
- 不适合复杂 UI 或大内存需求的场景

### Q2：FreeRTOS 和 Linux 的区别？

| 对比项 | FreeRTOS | Linux |
|--------|-----------|-------|
| 定位 | 实时内核 | 完整操作系统 |
| 内存占用 | < 10KB RAM | > 1MB RAM |
| MMU | 无 | 有 |
| 调度算法 | 优先级+时间片 | CFS 等复杂算法 |
| 实时性 | 硬实时 | 软实时（PREEMPT_RT补丁） |

### Q3：FreeRTOS 支持哪些架构？

- ARM Cortex-M/R/A
- RISC-V
- MIPS
- x86
- PowerPC
- 等 40+ 架构

---

## 1.6 避坑指南

1. **不要用 `sizeof(TaskName)` 作为栈大小单位** — 应使用具体数值
2. **不要忽视栈溢出** — 栈太小会导致硬Fault
3. **不要在中断中调用阻塞API** — 部分API不可在中断上下文中使用
4. **协程已弃用** — 新代码不要使用 `xTaskCreateRestricted`

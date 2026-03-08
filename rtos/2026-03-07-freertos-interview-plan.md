# FreeRTOS高级工程师面试复习资料实现计划

> **For Claude:** REQUIRED SUB-SKILL: 使用 superpowers:subagent-driven-development 或 superpowers:executing-plans 逐任务执行本计划。

**目标：** 创建一份15000字+的FreeRTOS高级工程师面试复习资料，覆盖6大技术方向，每个方向10+深度问题。

**架构：** 混合型内容组织 - 每个主题包含：原理阐述 + 源码分析 + 面试问题 + 实战案例

**技术栈：** FreeRTOS LTS (v10.5.1), Markdown文档

---

## 源码参考路径

```
FreeRTOS-Kernel/
├── tasks.c                    # 任务管理核心 (约6000行)
├── queue.c                    # 队列实现
├── event_groups.c             # 事件组
├── timers.c                   # 软件定时器
├── croutine.c                 # 协程
├── portable/MemMang/
│   ├── heap_1.c               # 简单分配器
│   ├── heap_2.c               # 最佳匹配
│   ├── heap_3.c               # 线程安全包装
│   ├── heap_4.c               # 合并空闲块
│   └── heap_5.c               # 跨区域堆
├── portable/GCC/ARM_CM7/      # Cortex-M7移植层
│   ├── port.c                 # 端口实现
│   └── portmacro.h            # 端口宏定义
└── include/
    ├── task.h                 # 任务头文件
    ├── queue.h                # 队列头文件
    └── semphr.h               # 信号量头文件
```

---

## Task 1: 调研现有面试资料并确定内容大纲

**Files:**
- Read: `interview2026/interview_rtos_qa.md` (全文)
- Read: `interview2026/interview_rtos.md` (全文)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c` (前300行，关键数据结构)

**Step 1: 阅读现有RTOS面试资料**

运行:
```
# 阅读 interview_rtos_qa.md 全文
# 了解现有内容的深度和覆盖面
```

目标: 确认现有内容为基础级别，识别需要新增的高级内容

**Step 2: 分析FreeRTOS内核源码结构**

运行:
```
# 读取 tasks.c 前300行
# 识别关键数据结构: TCB, TaskStatus_t, List_t
# 读取 task.h 中的重要API声明
```

目标: 确定源码分析的关键点和代码位置

**Step 3: 制定详细大纲**

输出: 创建 `freertos_advanced_outline.md`，包含6大章节、每个章节的10+问题列表

---

## Task 2: 编写第1章 - 内核实现原理 (约3000字)

**Files:**
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:600-800` (调度器核心)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2000-2300` (任务切换)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/port.c:200-350` (上下文切换)

**Step 1: 编写任务调度器问题**

```
问题1: FreeRTOS抢占式调度器如何选择下一个运行任务？
- 分析 vTaskSwitchContext() 函数
- 讲解 pxCurrentTCB 指针的作用

问题2: 任务就绪链表是如何组织的？
- 讲解 uxTopReadyPriority 优化
- 讲解 ReadyTasksList 数组结构

问题3: configUSE_TIME_SLICING 如何影响调度？
- 分析时间片轮转实现
- 对比不同配置的效果
```

**Step 2: 编写任务切换机制问题**

```
问题4: 任务切换时具体保存了哪些上下文？
- 分析 xPortPendSVHandler 实现
- 讲解 PSP 和 MSP 的使用

问题5: 为什么使用 PendSV 触发任务切换？
- 讲解 PendSV 优先级配置
- 分析与其他异常的配合
```

**Step 3: 编写Tick和空闲任务问题**

```
问题6: Tick中断中发生了什么？
- 分析 vTaskIncrementTick() 实现
- 讲解任务阻塞超时处理

问题7: 空闲任务的作用是什么？
- 分析 prvIdleTask() 实现
- 讲解空闲任务让出机制
```

---

## Task 3: 编写第2章 - 内存管理 (约2500字)

**Files:**
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/MemMang/heap_1.c` (全文)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/MemMang/heap_4.c` (全文)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:3500-3800` (栈溢出检测)

**Step 1: 编写堆内存分配问题**

```
问题1: heap_1 vs heap_4 的区别是什么？
- 分析两种分配器的算法
- 对比优缺点和适用场景

问题2: 内存碎片化是如何产生的？
- 讲解内存碎片化原理
- 分析 heap_4 的合并策略

问题3: pvPortMalloc 如何实现线程安全？
- 分析临界区保护
- 讲解禁用中断的方式
```

**Step 2: 编写任务栈管理问题**

```
问题4: 任务栈大小如何确定？
- 讲解栈使用分析方法
- 提供栈大小计算公式

问题5: 栈溢出检测是如何工作的？
- 分析 configCHECK_FOR_STACK_OVERFLOW
- 讲解两种检测方法

问题6: 栈对齐有什么要求？
- 讲解 ARM Cortex-M 的栈对齐要求
- 分析 8字节对齐的原因
```

---

## Task 4: 编写第3章 - 任务同步与通信 (约3000字)

**Files:**
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c:100-300` (队列创建)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/queue.c:500-700` (队列发送/接收)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include/semphr.h` (信号量定义)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/event_groups.c:200-400` (事件组)

**Step 1: 编写互斥锁问题**

```
问题1: 二值信号量作为互斥锁有何问题？
- 分析优先级反转现象
- 讲解优先级继承机制

问题2: xSemaphoreCreateMutex() 是如何实现的？
- 分析 mutex 结构
- 讲解递归锁的限制
```

**Step 2: 编写队列问题**

```
问题3: 队列传递指针 vs 传递数据，如何选择？
- 分析两种方式的优缺点
- 讲解常见错误和注意事项

问题4: xQueueSend() 阻塞超时是如何实现的？
- 分析阻塞链表管理
- 讲解 tick count 更新
```

**Step 3: 编写事件组问题**

```
问题5: 事件组如何实现多条件等待？
- 分析事件组数据结构
- 讲解位操作原子性

问题6: 任务通知与队列/信号量相比有何优势？
- 分析性能差异
- 讲解适用场景
```

---

## Task 5: 编写第4章 - 性能优化 (约2000字)

**Files:**
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/GCC/ARM_CM7/portmacro.h` (临界区宏)
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2500-2800` (临界区)

**Step 1: 编写中断管理问题**

```
问题1: 临界区对系统响应的影响？
- 分析 taskENTER_CRITICAL() 实现
- 讲解最大临界区嵌套深度

问题2: FromISR API 命名规范的意义？
- 分析中断安全函数的要求
- 讲解常见使用错误
```

**Step 2: 编写调度延迟问题**

```
问题3: 如何测量调度延迟？
- 讲解测量方法
- 分析延迟来源

问题4: 高优先级任务阻塞低优先级的原因？
- 分析优先级天花板协议
- 讲解正确的中断处理方式
```

---

## Task 6: 编写第5章 - 实际项目问题 (约2500字)

**Files:**
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/tasks.c:2000-2100` (任务切换)
- 分析项目中常见的死锁场景

**Step 1: 编写死锁问题**

```
问题1: FreeRTOS中可能导致死锁的场景？
- 分析互斥锁死锁
- 讲解死锁四要素

问题2: 如何预防死锁？
- 讲解资源排序法
- 分析超时机制
```

**Step 2: 编写优先级反转问题**

```
问题3: 优先级反转的经典案例分析？
- 讲解 M 协议场景
- 分析优先级继承效果
```

**Step 3: 编写栈溢出问题**

```
问题4: 如何调试栈溢出？
- 分析调试方法
- 讲解预防措施
```

---

## Task 7: 编写第6章 - 多核/SMP支持 (约1500字)

**Files:**
- Read: `FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/README.md` (SMP支持情况)
- 分析 FreeRTOS SMP 实现

**Step 1: 编写SMP基础问题**

```
问题1: FreeRTOS对SMP的支持情况？
- 分析当前SMP实现状态
- 讲解配置方法

问题2: 多核系统中的任务分配策略？
- 分析负载均衡
- 讲解核间通信
```

---

## Task 8: 整合并创建最终文档

**Files:**
- Create: `interview2026/interview_freertos_advanced.md`
- Modify: `interview2026/CLAUDE.md` (更新目录)

**Step 1: 整合所有章节**

```
# 创建主文档
# 按顺序整合6大章节
# 添加目录和导航
```

**Step 2: 添加附录内容**

```
# 常用API速查表
# 配置项汇总表
# 常见错误代码表
```

---

## 执行方式选择

**Plan complete and saved to `docs/plans/2026-03-07-freertos-interview-plan.md`.**

**Two execution options:**

**1. Subagent-Driven (this session)** - 我为每个任务调度子代理，任务间进行代码审查，快速迭代

**2. Parallel Session (separate)** - 在新会话中使用 executing-plans，批量执行并设置检查点

请问你选择哪种执行方式？

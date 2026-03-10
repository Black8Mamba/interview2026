# Linux ARM 进程学习资料规划

## 目标

输出一系列关于 Linux ARM 进程创建、调度、管理、调试方面的深度学习资料。

## 读者定位

- 进阶级读者，有一定Linux基础，想深入理解进程子系统

## 内容结构

按子系统主题划分：

1. **01-process-creation.md** - 进程创建：fork/vfork/clone
2. **02-process-scheduling.md** - 进程调度：CFS/RT/ARM调度器
3. **03-process-management.md** - 进程管理：生命周期/信号/退出
4. **04-process-debugging.md** - 进程调试：ftrace/perf/gdb

## 内容深度

- 深度详细：包含源代码分析、内核数据结构、关键函数追踪

## 架构重点

- 以 ARM64 (aarch64) 为主

## 特殊要求

- 每章附带常见面试题

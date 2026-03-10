# 第四章：Linux ARM64 进程调试

> 本章深入探讨Linux进程调试技术，从ftrace到perf再到GDB，涵盖ARM64平台的调试方法

## 目录

- [4.1 Linux调试工具概述](#41-linux调试工具概述)
- [4.2 ftrace函数追踪](#42-ftrace函数追踪)
- [4.3 perf性能分析](#43-perf性能分析)
- [4.4 GDB调试实战](#44-gdb调试实战)
- [4.5 ARM64调试特定](#45-arm64调试特定)
- [4.6 常见面试题](#46-常见面试题)

---

## 4.1 Linux调试工具概述

### 常用调试工具分类

| 类别 | 工具 | 用途 |
|------|------|------|
| 系统追踪 | ftrace, strace, ltrace | 函数调用追踪 |
| 性能分析 | perf, flamegraph | CPU/内存性能分析 |
| 调试器 | GDB, LLDB | 源码级调试 |
| 内存分析 | valgrind, address sanitizer | 内存错误检测 |
| 系统监控 | top, htop, /proc | 实时状态监控 |

### 进程调试相关/proc文件

```bash
# 查看进程信息
/proc/[pid]/status      # 进程状态
/proc/[pid]/stack       # 内核栈
/proc/[pid]/maps        # 内存映射
/proc/[pid]/fd          # 文件描述符
/proc/[pid]/wchan       # 等待通道
/proc/[pid]/sched       # 调度信息

# 查看调度信息
/proc/sched_debug       # 全局调度信息
/proc/[pid]/schedstat   # 调度统计
```

---

## 4.2 ftrace函数追踪

### ftrace简介

ftrace是Linux内核内置的函数追踪工具，提供强大的内核函数调用追踪能力。

### 启用ftrace

```bash
# 挂载debugfs
mount -t debugfs none /sys/kernel/debug/

# 查看可用追踪器
cat /sys/kernel/debug/tracing/available_tracers

# 常用追踪器
# function        - 函数调用追踪
# function_graph - 函数调用图
# sched_switch    - 调度器切换
# wakeup          - 进程唤醒延迟
# irq            - 中断追踪
# blk            - 块IO追踪
```

### 追踪进程创建

```bash
#!/bin/bash
# 追踪fork系统调用

cd /sys/kernel/debug/tracing

# 设置追踪器
echo function > current_tracer

# 设置过滤条件 - 只追踪fork相关函数
echo '*fork*' > set_ftrace_filter

# 启用追踪
echo 1 > tracing_on

# 运行要追踪的程序
./my_program

# 停止追踪
echo 0 > tracing_on

# 查看结果
cat trace
```

### 追踪调度器

```bash
#!/bin/bash
# 追踪进程调度切换

cd /sys/kernel/debug/tracing

# 设置调度追踪器
echo sched_switch > current_tracer

# 设置CPU过滤(可选)
echo 0 > tracing_cpumask

# 启用追踪
echo 1 > tracing_on

# 运行程序
sleep 1

# 停止并查看
echo 0 > tracing_on
cat trace | head -50
```

### ftrace C API

```c
#include <linux/ftrace.h>

/*
 * 简单的ftrace追踪点
 * 在内核代码中使用
 */

/* 追踪函数入口 */
static void notrace my_trace_func(void)
{
    trace_printk("my function called\n");
}

/* 使用tracepoint */
#include <trace/events/sched.h>

/* 追踪调度事件 */
trace_sched_wakeup(new_task, curr);
trace_sched_switch(prev, next);
```

---

## 4.3 perf性能分析

### perf基本使用

```bash
# 查看性能计数器
perf list

# 统计CPU周期
perf stat -e cycles ./program

# 统计多种事件
perf stat -e cycles,instructions,cache-references,cache-misses ./program

# CPU分析
perf record -g ./program
perf report

# 实时监控
perf top
```

### perf分析进程调度

```bash
#!/bin/bash
# 分析进程的调度延迟

# 记录调度事件
perf record -e sched:sched_wakeup -e sched:sched_switch -g ./my_program

# 查看报告
perf report

# 查看调度延迟分析
perf sched latency
```

### perf分析输出示例

```bash
# perf stat 输出示例
$ perf stat -e cycles,instructions ./my_program

 Performance counter stats for './my_program':

         1,234,567,890 cycles                    #    3.000 GHz
           987,654,321 instructions              #    0.80  insn per cycle
             12,345,678 cache-references
              1,234,567 cache-misses              #    9.99% miss rate

       0.411238901 seconds time elapsed
```

### perf与火焰图

```bash
# 生成火焰图数据
perf record -F 99 -p <pid> -g -- sleep 30

# 生成火焰图(需要flamegraph工具)
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg

# 或使用内核自带的
perf report --stdio
```

### perf编程接口

```c
#include <perf/perf.h>

/*
 * perf_event_open 系统调用
 * 用于创建性能监控事件
 */
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct perf_event_attr pea = {
    .type = PERF_TYPE_HARDWARE,
    .config = PERF_COUNT_HW_CPU_CYCLES,
    .disabled = 1,
    .inherit = 1,
};

int fd = perf_event_open(&pea, 0, -1, -1, 0);
if (fd == -1) {
    perror("perf_event_open");
    exit(1);
}

ioctl(fd, PERF_EVENT_IOC_RESET, 0);
ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

// 执行要测量的代码

ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
read(fd, &count, sizeof(long long));
close(fd);
```

---

## 4.4 GDB调试实战

### GDB基本使用

```bash
# 启动GDB
gdb ./my_program

# 常用命令
(gdb) break main           # 设置断点
(gdb) run                  # 运行程序
(gdb) next                 # 单步执行(不进入函数)
(gdb) step                 # 单步执行(进入函数)
(gdb) continue             # 继续运行
(gdb) print var            # 打印变量
(gdb) backtrace            # 查看调用栈
(gdb) info threads         # 查看线程
(gdb) thread apply all bt  # 所有线程堆栈
```

### GDB多进程/多线程调试

```bash
# 调试多进程
(gdb) set follow-fork-mode child  # 跟踪子进程
(gdb) set detach-on-fork off     # 不分离调试

# 调试多线程
(gdb) info threads              # 查看所有线程
(gdb) thread 2                  # 切换到线程2
(gdb) thread apply 2 bt         # 查看线程2的堆栈
(gdb) break filename:line thread all  # 所有线程断点
(gdb) set scheduler-locking on   # 锁定调度
```

### GDB高级调试

```bash
# 条件断点
(gdb) break main if argc > 1

# 观察点
(gdb) watch var                  # 写观察点
(gdb) rwatch var                 # 读观察点
(gdb) awatch var                 # 读写观察点

# 命令自动化
(gdb) commands 1
> print var
> continue
> end

# 调试正在运行的进程
gdb -p <pid>

# 调试core文件
gdb ./program core
```

### GDB脚本自动化

```python
# gdbinit.py - GDB Python脚本

import gdb

class ForkBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(ForkBreakpoint, self).__init__("fork")
        self.silent = True

    def stop(self):
        print("\n=== fork() called ===")
        print("Return value: %s" % gdb.parse_and_eval("$x0"))
        return False

# 设置断点
ForkBreakpoint()

# 继续执行
gdb.execute("continue")
```

```bash
# 使用Python脚本
gdb -x gdbinit.py ./program
```

---

## 4.5 ARM64调试特定

### ARM64调试环境

```bash
# ARM64 Linux系统调试
# 常用工具

# 1. 查看CPU寄存器
cat /proc/[pid]/status | grep -i reg

# 2. 查看栈回溯
cat /proc/[pid]/stack

# 3. GDB远程调试
# 目标板运行gdbserver
gdbserver :1234 ./my_program

# 主机运行GDB
aarch64-linux-gnu-gdb ./my_program
(gdb) target remote 192.168.1.100:1234
```

### ARM64特定调试技巧

```bash
# ARM64寄存器查看
# 查看当前进程的寄存器
cat /proc/self/status | grep -A 40 "Name:"

# 使用gdb查看ARM64寄存器
(gdb) info registers
x0             0x0                 0
x1             0x1                 1
x2             0x2                 2
sp             0x7ffffff000          0x7ffffff000
pc             0x5555555548a8       0x5555555548a8 <main+8>

# 查看浮点/NEON寄存器
(gdb) info registers v0 v1 v2
```

### ARM64内核调试

```bash
# ARM64内核调试
# 使用kgdb(串口)

# 1. 内核启动参数
# 添加: kgdboc=ttyS0,115200 kgdbwait

# 2. 在目标板上启动kgdb
echo g > /proc/sysrq-trigger

# 3. 主机连接
aarch64-linux-gnu-gdb vmlinux
(gdb) target remote /dev/ttyS0
```

### ARM64性能分析

```bash
# ARM64性能分析
# 使用perf

# 记录CPU事件
perf record -e cycles -e instructions -g ./my_program

# ARM64特定事件
perf list | grep -i arm
perf stat -e arm_cortex_frontend_fetch_sqline_l1ic_miss ./my_program

# 分析分支预测
perf record -e branch-instructions -e branch-misses ./my_program
perf report
```

### ARM64核心转储分析

```bash
# ARM64 core文件分析

# 生成core文件
ulimit -c unlimited
./program  # 崩溃

# 使用GDB分析
aarch64-linux-gnu-gdb ./program core

(gdb) bt
(gdb) info registers
(gdb) x/10i $pc
```

---

## 4.6 常见面试题

### 面试题1：ftrace和perf的区别

**问题**：ftrace和perf有什么区别？分别在什么场景下使用？

**答案**：

| 特性 | ftrace | perf |
|------|--------|------|
| 作用范围 | 内核函数追踪 | 全系统性能分析 |
| 使用方式 | debugfs文件系统 | perf命令行工具 |
| 开销 | 较小 | 可配置 |
| 场景 | 内核开发调试 | 应用性能优化 |

**ftrace适用场景**：
- 内核函数调用追踪
- 调度延迟分析
- 中断分析
- 驱动开发调试

**perf适用场景**：
- CPU热点分析
- 缓存命中率分析
- 锁竞争分析
- 应用程序性能优化

---

### 面试题2：如何分析进程调度延迟

**问题**：如何分析和定位Linux进程的调度延迟问题？

**答案**：

1. **使用ftrace**：
```bash
# 追踪调度延迟
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo nop > /sys/kernel/debug/tracing/current_tracer
echo 1 > /sys/kernel/debug/tracing/events/sched/sched_wakeup/enable
echo 1 > /sys/kernel/debug/tracing/events/sched/sched_switch/enable
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 查看
cat /sys/kernel/debug/tracing/trace
```

2. **使用perf sched**：
```bash
# 记录调度事件
perf sched record -- ./my_program
# 分析延迟
perf sched latency
```

3. **分析/proc**：
```bash
# 查看调度统计
cat /proc/[pid]/schedstat
# 格式: (run_time, run_delay, last_sleep_time)
```

---

### 面试题3：GDB调试多线程的常见问题

**问题**：GDB调试多线程程序时有哪些常见问题和解决方法？

**答案**：

1. **线程随机停止**：
   - 默认所有线程都会在断点停止
   - 解决：设置scheduler-locking

2. **线程信息不同步**：
   - 某些操作会导致线程状态变化
   - 解决：使用`thread apply all`查看

3. **死锁问题**：
   - GDB暂停可能影响其他线程
   - 解决：使用`non-stop`模式

```bash
# 常用多线程调试命令
(gdb) set scheduler-locking on      # 只运行当前线程
(gdb) set scheduler-locking step    # 单步时只运行当前线程
(gdb) set non-stop off             # 全停止模式
(gdb) thread apply all bt          # 所有线程堆栈
(gdb) thread find .                # 查找线程
```

---

### 面试题4：如何调试ARM64程序

**问题**：在ARM64架构下调试程序有哪些特定方法？

**答案**：

1. **使用交叉编译工具链**：
```bash
# 编译
aarch64-linux-gnu-gcc -g -o program program.c

# 调试
aarch64-linux-gnu-gdb program
```

2. **远程调试**：
```bash
# 目标板
gdbserver :1234 ./program

# 主机
aarch64-linux-gnu-gdb program
(gdb) target remote 192.168.1.100:1234
```

3. **分析内核转储**：
```bash
# ARM64 core文件
aarch64-linux-gnu-gdb program core
```

4. **使用ARM64特定工具**：
```bash
# perf分析ARM事件
perf record -e arm_cortex_frontend_fetch_sqline_l1ic_miss ./program
```

---

### 面试题5：perf的性能事件有哪些

**问题**：perf有哪些常用的性能事件？

**答案**：

**硬件事件**：
- cycles - CPU周期
- instructions - 执行指令数
- cache-references - 缓存访问
- cache-misses - 缓存未命中
- branch-instructions - 分支指令
- branch-misses - 分支预测失败

**软件事件**：
- context-switches - 上下文切换
- cpu-migrations - CPU迁移
- page-faults - 缺页异常

**ARM64特定事件**：
- arm_cortex_frontend_fetch_sqline_l1ic_miss - 前端取指缓存未命中
- arm_cortex_branch_prediction - 分支预测
- arm_cortex_l1_icache - L1指令缓存

---

### 面试题6：如何分析进程CPU使用率高的原因

**问题**：当发现某个进程CPU使用率很高时，如何分析原因？

**答案**：

1. **使用top/htop**：
```bash
top -p <pid>
htop
```

2. **使用perf**：
```bash
# 采样CPU使用
perf record -g -p <pid> sleep 10
perf report
```

3. **分析热点函数**：
```bash
# 使用flamegraph
perf record -F 99 -p <pid> -g -- sleep 5
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

4. **分析系统调用**：
```bash
# 追踪系统调用
strace -p <pid> -c
strace -p <pid> -T
```

---

### 面试题7：内核调试的方法

**问题**：Linux内核调试有哪些常用方法？

**答案**：

1. **printk调试**：
```c
printk(KERN_DEBUG "debug info: %d\n", var);
```

2. **ftrace追踪**：
```bash
echo function > /sys/kernel/debug/tracing/current_tracer
```

3. **kgdb调试**：
```bash
# 内核启动参数添加
kgdboc=ttyS0,115200 kgdbwait
```

4. **内核配置**：
```bash
# 启用调试选项
CONFIG_DEBUG_INFO=y
CONFIG_KGDB=y
CONFIG_DEBUG_KMEMLEAK=y
```

5. **使用crash工具分析内核转储**：
```bash
crash vmlinux vmcore
```

---

## 本章小结

本章深入分析了Linux进程调试技术：

1. **调试工具概述**：ftrace、perf、GDB等工具定位
2. **ftrace**：内核函数追踪、调度分析
3. **perf**：性能分析、热点定位、火焰图
4. **GDB调试**：多线程调试、断点、脚本自动化
5. **ARM64特定**：交叉调试、远程调试、内核调试

调试是Linux开发的核心技能，熟练掌握这些工具能快速定位和解决各类问题。

---

*上一页：[第三章：进程管理](./03-process-management.md)*

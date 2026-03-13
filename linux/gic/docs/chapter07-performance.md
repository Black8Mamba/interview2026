# 第7章 中断性能优化（重点章节）

中断性能优化是Linux驱动开发中的核心话题，优秀的中断处理策略可以显著提升系统的实时性和吞吐量。本章将深入探讨中断亲和性、负载均衡、软中断调优、延迟分析以及实时系统优化等关键主题。

## 7.1 中断亲和性（IRQ Affinity）

### 7.1.1 原理

中断亲和性（IRQ Affinity）是指将特定硬件中断绑定到特定的CPU核心上处理的机制。默认情况下，Linux内核会将中断分配到所有可用CPU上随机处理，但通过设置中断亲和性，可以实现以下目标：

1. **减少CPU缓存失效**：将中断固定在同一CPU核心上，可以充分利用该核心的L1/L2缓存，减少缓存未命中带来的延迟
2. **提高NUMA节点本地性**：对于NUMA系统，将中断绑定到访问本地内存的CPU上，可以降低内存访问延迟
3. **避免中断风暴**：将高频率中断分散到多个CPU，避免单核过载
4. **实时性保障**：将实时任务相关的中断绑定到专用核心，确保低延迟处理

### 7.1.2 配置方法

**1. 通过 /proc/irq 接口查看和配置**

```bash
# 查看当前中断亲和性设置
cat /proc/irq/1/smp_affinity
cat /proc/irq/1/smp_affinity_list

# 设置中断亲和性（十六进制掩码）
# 将IRQ 1绑定到CPU 0和CPU 1
echo 3 > /proc/irq/1/smp_affinity

# 使用逗号分隔的CPU列表（需要配合 irqbalance 服务）
echo "0,1" > /proc/irq/1/smp_affinity_list
```

**2. 通过 set_irq_affinity 脚本（Intel网卡常用）**

```bash
# Intel网卡的 irq_affinity 脚本
# 查看脚本位置
find /usr -name "set_irq_affinity" 2>/dev/null

# 使用示例
./set_irq_affinity.sh eth0
```

**3. 通过 cpumask 编程接口**

在内核驱动中设置中断亲和性：

```c
#include <linux/cpumask.h>
#include <linux/irq.h>

int set_irq_affinity(struct irq_desc *desc, const struct cpumask *mask)
{
    struct irq_data *data = irq_desc_get_irq_data(desc);

    // 设置中断亲和性掩码
    irq_data_update_affinity(data, mask);

    return 0;
}

// 在驱动初始化中应用
static int my_driver_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int irq = pci_irq_vector(dev, 0);
    cpumask_t mask;

    // 绑定到CPU 0和1
    cpumask_clear(&mask);
    cpumask_set_cpu(0, &mask);
    cpumask_set_cpu(1, &mask);

    irq_set_affinity_hint(irq, &mask);

    return 0;
}
```

**4. 查看实际效果**

```bash
# 查看所有中断的CPU分布
cat /proc/interrupts | head -20

# 实时监控中断分布（每秒刷新）
watch -n 1 'cat /proc/interrupts | grep eth0'

# 使用 irqtop 工具
irqtop
```

## 7.2 中断负载均衡

### 7.2.1 irqbalance 机制

irqbalance 是一个系统守护进程，用于自动在CPU核心之间均衡中断负载。它的工作原理如下：

1. **基于负载的均衡**：irqbalance 会监控各CPU的中断处理负载，将高负载的中断迁移到空闲CPU
2. **考虑缓存亲和性**：优先将中断保留在当前处理的CPU上
3. **NUMA感知**：尽量保持中断处理在本地NUMA节点
4. **节能模式**：在系统空闲时合并中断，减少唤醒的CPU数量

**配置和使用：**

```bash
# 启动 irqbalance
systemctl start irqbalance
systemctl enable irqbalance

# 查看 irqbalance 状态
systemctl status irqbalance

# 临时禁用 irqbalance（用于手动设置亲和性）
systemctl stop irqbalance

# 配置 irqbalance 级别
# 0 - 禁用，完全由手动控制
# 1 - 普通负载均衡
# 2 - 偏向性均衡（更激进）
echo 1 > /proc/sys/irqbalance/level

# 设置排除列表（某些中断不参与均衡）
# 在 /etc/irqbalance.conf 中添加
# IRQPROF=0
# ExcludeList=0,1,8
```

### 7.2.2 手动均衡

在某些场景下，手动设置中断亲和性比自动均衡效果更好：

**1. 高性能网络场景**

```bash
# 假设有4个网卡的队列，分别绑定到不同的CPU
# eth0-RX-0 -> CPU 0
echo 1 > /proc/irq/$(grep eth0-RX-0 /proc/interrupts | cut -d: -f1 | tr -d ' ')/smp_affinity
# eth0-RX-1 -> CPU 1
echo 2 > /proc/irq/$(grep eth0-RX-1 /proc/interrupts | cut -d: -f1 | tr -d ' ')/smp_affinity
# eth0-RX-2 -> CPU 2
echo 4 > /proc/irq/$(grep eth0-RX-2 /proc/interrupts | cut -d: -f1 | tr -d ' ')/smp_affinity
# eth0-RX-3 -> CPU 3
echo 8 > /proc/irq/$(grep eth0-RX-3 /proc/interrupts | cut -d: -f1 | tr -d ' ')/smp_affinity
```

**2. 持久化配置**

创建 udev 规则实现重启后自动应用：

```bash
# /etc/udev/rules.d/60-net-affinity.rules
ACTION=="add", SUBSYSTEM=="net", KERNEL=="eth*", \
    RUN+="/usr/local/bin/set-irq-affinity.sh %k"
```

## 7.3 软中断调优

### 7.3.1 ksoftirqd 机制

Linux将中断处理分为两部分：硬件中断处理程序（硬中断）和软件中断处理（软中断）。软中断包括：

- NET_RX（网络包接收）
- NET_TX（网络包发送）
- TIMER（定时器）
- TASKLET（任务延迟执行）
- SCHED（调度）
- HRTIMER（高精度定时器）

当软中断负载过高时，内核会唤醒 ksoftirqd 线程来协助处理：

```bash
# 查看 ksoftirqd 状态
ps -ef | grep ksoftirqd

# 查看各CPU的 ksoftirqd 负载
cat /proc/softirqs

# 示例输出：
# CPU0                    CPU1                   CPU2                   CPU3
#       NET_RX:  12345678   NET_RX:  87654321   NET_RX:  45678901   NET_RX:  23456789
```

### 7.3.2 软中断参数调优

**1. 调整软中断处理权重**

```bash
# 设置每个CPU每jiffy可处理的软中断次数
# 默认值：10
sysctl -w net.core.netdev_budget=600

# 设置软中断时间片（每个ksoftirqd单次运行时间）
# 默认值：2 jiffies
sysctl -w net.core.netdev_budget_usecs=8000
```

**2. 调整 ksoftirqd 优先级**

对于需要低延迟的场景，可以调整 ksoftirqd 的优先级：

```bash
# 查看当前 ksoftirqd 优先级
ps -eo pid,comm,ni,pri | grep ksoftirqd

# 降低nice值提高优先级（需要root权限）
# 降低到 -10
renice -10 -p $(pgrep -f "ksoftirqd/0")
renice -10 -p $(pgrep -f "ksoftirqd/1")
```

**3. 中断合并优化**

网络驱动程序中的中断合并（Interrupt Coalescing）可以减少中断次数：

```bash
# 查看网卡中断合并设置
ethtool -c eth0

# 启用中断合并
ethtool -C eth0 rx-usecs 100 tx-usecs 100

# 禁用中断合并（低延迟场景）
ethtool -C eth0 rx-usecs 0 tx-usecs 0

# 参数说明：
# rx-usecs: 收到包后多少微秒触发一次中断
# tx-usecs: 发送多少微秒后触发中断
# rx-frames: 收到多少帧后触发中断
```

## 7.4 中断延迟分析

### 7.4.1 ftrace 工具

ftrace 是 Linux 内置的强大的跟踪工具，可用于分析中断延迟：

**1. 基本配置**

```bash
# 挂载 debugfs（如果未挂载）
mount -t debugfs debugfs /sys/kernel/debug

# 启用功能
cd /sys/kernel/debug/tracing

# 查看可用跟踪器
cat available_tracers

# 启用中断延迟跟踪
echo irqsoff > current_tracer

# 设置跟踪缓冲区大小
echo 1000000 > buffer_size_kb
```

**2. 跟踪中断禁用时间**

```bash
# 启用跟踪
echo 1 > tracing_on

# 执行要测试的操作
# ...

# 停止跟踪
echo 0 > tracing_on

# 查看结果
cat trace | head -50
cat trace | grep "max"  # 查看最大延迟
```

**3. 跟踪软中断延迟**

```bash
# 使用 function_graph 跟踪
echo function_graph > current_tracer

# 只跟踪中断相关函数
echo 'irq*' > set_ftrace_filter

# 查看结果
cat trace
```

**4. 常用 ftrace 命令脚本**

```bash
#!/bin/bash
# 分析中断延迟

TRACING_PATH=/sys/kernel/debug/tracing

# 清理之前的跟踪
echo nop > $TRACING_PATH/current_tracer
echo 0 > $TRACING_PATH/tracing_on

# 设置跟踪器
echo irqsoff > $TRACING_PATH/current_tracer
echo 0 > $TRACING_PATH/tracing_on

# 启用跟踪
echo 1 > $TRACING_PATH/tracing_on

# 执行测试
sleep 1

# 停止并查看
echo 0 > $TRACING_PATH/tracing_on
cat $TRACING_PATH/trace | head -30

# 查看最大延迟
cat $TRACING_PATH/tracing_max_latency
```

### 7.4.2 latencytop 工具

latencytop 用于识别导致系统延迟的根源：

```bash
# 安装（如果未安装）
# apt-get install latencytop

# 运行
latencytop

# 内核需要启用 CONFIG_LATENCYTOP
# 查看是否支持
cat /proc/latency_stats

# 实时查看
cat /proc/latency_stats | head -20
```

### 7.4.3 其他延迟分析工具

```bash
# perf 工具分析中断
perf record -g -a -e irq:irq_handler_entry -e irq:irq_handler_exit
perf report

# 查看硬中断耗时分布
cat /proc/interrupts

# 使用 hwlatdetect 检测硬件延迟
hwlatdetect --duration=10
```

## 7.5 Real-time 相关优化（PREEMPT_RT）

### 7.5.1 PREEMPT_RT 概述

PREEMPT_RT 是 Linux 内核的实时补丁，实现了以下关键特性：

1. **完全抢占**：将所有内核代码变为可抢占
2. **优先级继承**：解决优先级反转问题
3. **中断线程化**：将中断处理改为内核线程
4. **抢占延迟优化**：减少内核抢占延迟

### 7.5.2 中断处理优化

**1. 中断线程化配置**

```bash
# 查看当前中断线程化状态
cat /proc/irq/1/spurious

# 将特定中断线程化
echo thread > /proc/irq/1/sp

# 设置中断线程优先级
chrt -f -p 50 $(cat /proc/irq/1/chained_handler 2>/dev/null || echo "N/A")

# 查看线程化中断
ps -eo pid,comm,policy,pri | grep -E "irq/|SoftIRQ"
```

**2. 实时内核启动参数**

在 GRUB 中添加：

```
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash preempt=voluntary nohz_full=0-3 rcu_nocbs=0-3"
```

参数说明：
- `preempt=voluntary`: 主动抢占
- `preempt=full`: 完全抢占（PREEMPT_RT）
- `nohz_full`: 指定无Tick CPU
- `rcu_nocbs`: 指定不执行RCU回调的CPU

**3. 实时任务与中断亲和性**

```c
#include <sched.h>
#include <pthread.h>
#include <stdio.h>

void set_realtime_priority(void) {
    struct sched_param param;
    param.sched_priority = 99;  // 最高实时优先级

    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler");
    }
}

void pin_to_cpu(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == -1) {
        perror("pthread_setaffinity_np");
    }
}

int main() {
    set_realtime_priority();
    pin_to_cpu(2);  // 绑定到CPU 2

    // 实时任务主循环
    while (1) {
        // 任务逻辑
    }

    return 0;
}
```

### 7.5.3 实时性能验证

```bash
# cyclictest - 实时性能测试标准工具
# 安装
# apt-get install rt-tests

# 运行基本测试
cyclictest -p 90 -n -h 30 -l 10000

# 参数说明：
# -p 90: 线程优先级90
# -n: 使用clock_nanosleep
# -h 30: 直方图最大30微秒
# -l 10000: 迭代10000次

# 多核测试
cyclictest -p 90 -n -S -t -l 10000

# 网络延迟测试
socktest -t 1000

# 内核延迟测试
hackbench -l 1000
```

## 本章面试题

### 1. 什么是中断亲和性？如何配置？

**参考答案**：中断亲和性是将特定硬件中断绑定到特定CPU核心的机制。可以通过 `/proc/irq/[irq_num]/smp_affinity` 文件以十六进制掩码方式设置，或使用 `set_irq_affinity.sh` 脚本。目的是减少缓存失效、提高NUMA本地性、避免单核过载。

### 2. irqbalance 的工作原理是什么？

**参考答案**：irqbalance 是一个守护进程，通过监控各CPU的中断负载，自动在CPU核心之间迁移中断。它考虑缓存亲和性（优先保留在当前CPU）、NUMA拓扑（保持本地性）和节能需求（空闲时合并中断）。

### 3. 软中断（SoftIRQ）和硬中断的区别是什么？

**参考答案**：硬中断由硬件触发，执行时间短，主要完成时间关键操作；软中断在硬中断处理程序中触发，处理耗时较长的任务（如网络包处理）。软中断运行在中断上下文中，但某些softirq（如tasklet）可以在进程上下文中运行。

### 4. 如何使用 ftrace 分析中断延迟？

**参考答案**：首先挂载 debugfs，然后设置跟踪器为 `irqsoff`（跟踪中断禁用时间）或 `preemptoff`（跟踪抢占延迟），启用跟踪后执行测试，最后查看 `/sys/kernel/debug/tracing/trace` 和 `tracing_max_latency`。

### 5. PREEMPT_RT 补丁对中断处理做了哪些优化？

**参考答案**：PREEMPT_RT 将所有中断处理线程化，实现完全内核抢占；添加优先级继承机制解决优先级反转；优化了中断处理路径以降低延迟；通过 `nohz_full` 和 `rcu_nocbs` 参数进一步减少系统抖动。

### 6. 如何优化高流量网络环境下的中断处理？

**参考答案**：启用网卡的多队列功能并将不同队列绑定到不同CPU；调整 `net.core.netdev_budget` 和 `netdev_budget_usecs` 参数；根据场景调整中断合并参数（低延迟场景禁用合并）；使用 `irqbalance` 或手动设置亲和性；考虑使用 DPDK 等用户态网络框架。

### 7. 什么是中断风暴？如何检测和处理？

**参考答案**：中断风暴指某硬件或软件错误导致中断频率异常高的现象。检测方法包括：`/proc/interrupts` 中观察计数增长速度、`mpstat` 查看CPU使用率、`top` 观察软中断占比。处理方法：暂时禁用该中断、设置中断速率限制、排查硬件故障或驱动bug。

### 8. 解释一下 RTF (Real-Time) 任务的中断延迟测试方法？

**参考答案**：使用 `cyclictest` 工具测试实时延迟，参数包括优先级、迭代次数等；通过 `hwlatdetect` 检测硬件引起的延迟；使用 `perf` 工具进行中断事件采样；分析 `/proc/latency_stats` 查看延迟热点。

---

**本章总结**：中断性能优化是系统调优的核心内容，需要综合运用中断亲和性设置、负载均衡策略、软中断调优和延迟分析工具。对于实时性要求高的系统，PREEMPT_RT 补丁配合合理的配置可以显著降低中断延迟。

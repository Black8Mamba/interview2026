# 第二章：Linux ARM64 进程调度

> 本章深入探讨Linux进程调度机制，从调度器架构到ARM64特定实现

## 目录

- [2.1 Linux调度器架构](#21-linux调度器架构)
- [2.2 CFS调度器原理](#22-cfs调度器原理)
- [2.3 实时调度器RT](#23-实时调度器rt)
- [2.4 ARM64调度特定实现](#24-arm64调度特定实现)
- [2.5 调度策略与系统调用](#25-调度策略与系统调用)
- [2.6 常见面试题](#26-常见面试题)

---

## 2.1 Linux调度器架构

### 调度器设计原则

Linux调度器设计遵循以下核心原则：

1. **公平性**：确保所有进程获得合理的CPU时间
2. **效率**：调度开销最小化
3. **低延迟**：快速响应交互式任务
4. **吞吐量**：最大化系统吞吐量
5. **功耗**：在移动设备上考虑功耗优化

### 调度器层次结构

```
+--------------------------------------------------+
|              用户空间 API                         |
|     sched_setaffinity() / nice() / chrt()       |
+--------------------------------------------------+
                     |
                     v
+--------------------------------------------------+
|            调度类 (Scheduler Classes)             |
|  +------------+  +------------+  +------------+  |
|  |  Stop      |  |  DL       |  |  RT        |  |
|  |  Scheduler |  |  Scheduler|  |  Scheduler |  |
|  +------------+  +------------+  +------------+  |
|                      |                            |
|                      v                            |
|              +------------+                      |
|              |   CFS     |                      |
|              | Scheduler |                      |
|              +------------+                      |
+--------------------------------------------------+
                     |
                     v
+--------------------------------------------------+
|            主调度器 (Main Scheduler)              |
|              schedule()                          |
+--------------------------------------------------+
                     |
                     v
+--------------------------------------------------+
|         上下文切换 (Context Switch)               |
|     __switch_to() / switch_mm()                  |
+--------------------------------------------------+
```

### 调度类 (Scheduler Class)

Linux调度器采用模块化设计，通过调度类实现不同的调度策略：

```c
// include/linux/sched.h
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task) (struct rq *rq);
    bool (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    void (*set_curr_task)(struct rq *rq);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);
    void (*switched_from)(struct rq *this_rq, struct task_struct *task);
    void (*switched_to)(struct rq *this_rq, struct task_struct *task);
    void (*prio_changed)(struct rq *this_rq, struct task_struct *task,
                 int oldprio);

    unsigned int (*get_rr_interval)(struct rq *rq, struct task_struct *task);

    void (*update_curr)(struct rq *rq);
};
```

### 调度类优先级

```c
// kernel/sched/deadline.c, rt.c, fair.c, stop.c
/*
 * 调度类优先级顺序(从高到低):
 *
 * 1. stop_sched_class  - 最高优先级，用于CPU隔离
 * 2. dl_sched_class   - Deadline调度类
 * 3. rt_sched_class   - 实时调度类
 * 4. fair_sched_class - 完全公平调度类(CFS)
 * 5. idle_sched_class - 空闲调度类
 */
const struct sched_class stop_sched_class = { ... };
const struct sched_class dl_sched_class = { ... };
const struct sched_class rt_sched_class = { ... };
const struct sched_class fair_sched_class = { ... };
const struct sched_class idle_sched_class = { ... };
```

---

## 2.2 CFS调度器原理

### CFS核心概念

CFS(Completely Fair Scheduler)即完全公平调度器，是Linux默认的调度器。它的核心思想是**虚拟运行时间(virtual runtime)**。

**CFS目标**：让每个进程获得"公平"的CPU时间。

### 虚拟运行时间 (vruntime)

```c
// kernel/sched/fair.c
struct sched_entity {
    struct load_weight load;      // 权重
    unsigned long               run_node;   // 红黑树节点
    u64                         vruntime;   // 虚拟运行时间
    u64                         sum_exec_runtime;  // 总运行时间
    u64                         prev_sum_exec_runtime;
    u64                         nr_migrations;

    int                         depth;
    struct sched_entity         *parent;
    struct cfs_rq               *cfs_rq;    // 所属CFS运行队列
    struct cfs_rq               *my_q;     // 管理的子运行队列
};
```

**vruntime计算公式**：
```
vruntime += 实际运行时间 × (NICE_0_LOAD / 进程权重)
```

其中NICE_0_LOAD为nice=0的基准权重(1024)。

```c
// 计算vruntime增量
static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
    if (unlikely(se->load.weight != NICE_0_LOAD))
        delta = calc_delta_mine(delta, NICE_0_LOAD, &se->load);

    return delta;
}
```

### 红黑树调度

CFS使用**红黑树**来管理就绪进程，vruntime最小的进程排在最前面：

```c
// kernel/sched/fair.c
/*
 * CFS红黑树操作
 *
 * 键值: se->vruntime
 * 最左节点: 下一个要运行的进程
 */

static void enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int flags)
{
    /* 更新虚拟运行时间 */
    update_curr(cfs_rq);

    /* 更新调度实体的负载权重信息 */
    update_load_avg(se, UPDATE_TG | UPDATE_PREEMPT);

    /* 更新运行时间统计 */
    account_entity_enqueue(cfs_rq, se);

    /* 加入红黑树 */
    if (!(flags & ENTRY_WAKEUP) || (flags & ENQUEUE_MIGRATED))
        place_entity(cfs_rq, se, 0);

    add_se_to_cfs_rq(se);
}

static struct sched_entity *pick_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *curr)
{
    /* 选择vruntime最小的进程(红黑树最左节点) */
    struct sched_entity *left = __pick_first_entity(cfs_rq);

    /*
     * 如果curr的vruntime小于left，
     * 说明curr应该继续运行(保持CPU亲和性)
     */
    if (curr && (!left || curr->vruntime < left->vruntime))
        left = curr;

    return left;
}
```

### CFS运行队列

```c
// kernel/sched/sched.h
struct cfs_rq {
    struct load_weight load;           // 总负载权重
    unsigned long nr_running;         // 可运行进程数

    u64 min_vruntime;                  // 最小vruntime
    struct rb_root_cached tasks_timeline;  // 红黑树根

    /*
     * 当前正在运行的调度实体
     * (不属于红黑树)
     */
    struct sched_entity *curr;

    /*
     * 每个CPU的cfs_rq
     * 还有任务组对应的cfs_rq
     */
    struct rq *rq;                     // 所属CPU运行队列

    /* 统计信息 */
    unsigned long nr_spread_over;

#ifdef CONFIG_SMP
    unsigned long h_nr_running;        // 层次化nr_running
#endif
};
```

### CFS调度流程

```
schedule() 主调度流程:
    |
    v
1. 清理当前进程
    put_prev_task(rq, prev)
    |
    v
2. 选择下一个进程
    next = pick_next_task(rq, ...)
    |
    v
3. 上下文切换
    if (next != prev)
        context_switch(rq, prev, next)
    |
    v
4. 返回

context_switch():
    |
    v
1. 切换地址空间
    switch_mm(oldmm, newmm)
    |
    v
2. 切换内核栈
    switch_to(prev, next)
    |
    v
3. 返回新进程
```

---

## 2.3 实时调度器RT

### 实时调度策略

Linux支持两种实时调度策略：

1. **SCHED_FIFO**：先进先出，没有时间片限制
2. **SCHED_RR**：时间片轮转，有固定时间片
3. **SCHED_DEADLINE**：基于EDF的 deadline 调度

### 实时优先级

```c
// include/uapi/linux/sched.h
#define MAX_USER_RT_PRIO    100    // 用户可用的RT优先级范围
#define MAX_RT_PRIO         MAX_USER_RT_PRIO

#define MIN_NICE        -20
#define MAX_NICE        19

/* 实时优先级: 0(低) - 99(高) */
#define MAX_RT_PRIO     100
```

### RT调度类实现

```c
// kernel/sched/rt.c
struct rt_prio_array {
    DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1);  // 优先级位图
    struct list_head queue[MAX_RT_PRIO+1];   // 每个优先级的队列
};
```

**RT调度选择算法**：
1. 查找最高优先级位图(最快O(1))
2. 在该优先级队列中选择队首进程
3. FIFO：始终选择队首
4. RR：在队首运行完时间片后移到队尾

### Deadline调度

```c
// kernel/sched/deadline.c
/*
 * Earliest Deadline First (EDF) 调度
 *
 * 调度决策基于:
 * - runtime: 每个周期内可运行的时间
 * - deadline: 完成截止时间
 * - period: 调度周期
 */
struct sched_dl_entity {
    u64             runtime;       // 分配的时间
    u64             deadline;      // 截止时间
    u64             period;        // 调度周期
    u64             runtime_based; // 基于runtime的调度
};
```

**Deadline调度保证**：
- 当系统负载 < 100% 时，保证所有任务的deadline
- 使用EDF算法选择下一个运行任务

---

## 2.4 ARM64调度特定实现

### ARM64调度架构特点

ARM64架构在调度方面有以下特点：

1. **多核支持**：最多256个CPU核心
2. **动态频率调节**：DVFS(Dynamic Voltage and Frequency Scaling)
3. **Big.LITTLE架构**：大小核调度优化
4. **抢占式调度**：支持抢占

### ARM64上下文切换

```c
// arch/arm64/kernel/switch_to.S
/*
 * __switch_to - 切换到新的task_struct
 *
 * 参数:
 * x0: prev (当前进程)
 * x1: next (下一个进程)
 *
 * 保存/恢复:
 * - SP_EL0 (用户栈指针)
 * - TPIDR_EL0 (TLS)
 * - 通用寄存器
 * - FP/SIMD状态
 */
__switch_to(struct task_struct *prev,
        struct task_struct *next)
{
    /* 保存当前进程的FP/SIMD状态 */
    fpsimd_save_task(prev);

    /* 切换内核栈 */
    __switch_stack(next->cpu_context.sp);

    /* 切换页表(ASID) */
    switch_mm_irqs_off(prev->mm, next->mm, next);

    /* 切换TLS */
    write_tls(next->thread.tp_value);

    /* 恢复新进程的CPU上下文 */
    cpu_switch_to(prev, next);
}
```

### ARM64 CPU上下文

```c
// arch/arm64/include/asm/processor.h
struct cpu_context {
    unsigned long x19;    // callee-saved registers
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    unsigned long fp;      // frame pointer
    unsigned long sp;      // stack pointer
    unsigned long pc;      // program counter
};

struct thread_struct {
    struct cpu_context cpu_context;  // CPU上下文
    unsigned long    tls_base;       // TLS基址
    unsigned long    tp_value;       // TLS值
    unsigned long    stack_cookie;   // 栈保护
    ...
};
```

### ARM64调度相关系统调用

```c
// ARM64系统调用号 (arch/arm64/include/asm/unistd.h)
#define __NR_sched_setattr        160
#define __NR_sched_getattr        161
#define __NR_sched_setparam       162
#define __NR_sched_getparam       163
#define __NR_sched_getaffinity    160
#define __NR_sched_setaffinity    160
```

### Big.LITTLE调度优化

ARM64设备常采用Big.LITTLE架构，调度器需要考虑：

1. **CPU能力(Capacity)**：不同核心的计算能力不同
2. **任务迁移**：避免频繁跨簇迁移
3. **能耗优化**：优先使用小核

```c
// kernel/sched/topology.c
/*
 * CPU capacity计算
 *
 * capacity = cpu_capacity * 1024 / max_capacity
 *
 * 大核: 1024
 * 小核: 512 或更低
 */
static void update_cpu_capacity(struct sched_domain *sd, int cpu)
{
    struct cpufreq_policy *policy;
    struct sched_group *sg = sd->groups;

    capacity = arch_scale_cpu_capacity(cpu);

    /* 考虑当前频率 */
    if (policy) {
        capacity = capacity * policy->cur / policy->max;
    }

    cpu_rq(cpu)->cpu_capacity = capacity;
}
```

---

## 2.5 调度策略与系统调用

### 调度策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| SCHED_NORMAL | CFS普通调度 | 普通进程 |
| SCHED_BATCH | 批处理调度 | CPU密集型任务 |
| SCHED_IDLE | 空闲调度 | 极低优先级任务 |
| SCHED_FIFO | 实时FIFO | 实时应用 |
| SCHED_RR | 实时轮转 | 实时应用 |
| SCHED_DEADLINE | Deadline调度 | 硬实时应用 |

### 设置调度策略

```c
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    struct sched_param param;

    /* 设置为实时调度 SCHED_FIFO，优先级50 */
    param.sched_priority = 50;

    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        perror("pthread_setschedparam");
        return 1;
    }

    /* 或者使用sched_setscheduler */
    sched_setscheduler(0, SCHED_FIFO, &param);

    /* 或者使用chrt命令 */
    /* chrt -f 50 ./program */

    while (1) {
        /* 实时任务 */
    }

    return 0;
}
```

### 调度亲和性

```c
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // 绑定到CPU 0

    /* 设置线程亲和性 */
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    /* 设置进程亲和性 */
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    /* 查询当前CPU */
    printf("Running on CPU: %d\n", sched_getcpu());

    return 0;
}
```

### nice值与优先级

```c
#include <sys/resource.h>
#include <unistd.h>

int main() {
    /* 设置nice值 */
    int nice_val = 10;
    setpriority(PRIO_PROCESS, 0, nice_val);

    /* 或使用setpriority系统调用 */
    nice(10);  // 相对增加nice值

    /* 获取当前nice值 */
    int current_nice = getpriority(PRIO_PROCESS, 0);

    return 0;
}
```

---

## 2.6 常见面试题

### 面试题1：CFS调度器的核心思想

**问题**：CFS(Completely Fair Scheduler)调度器的核心思想是什么？

**答案**：

CFS的核心思想是**完全公平调度**，通过虚拟运行时间(vruntime)实现：

1. **每个进程都有vruntime**，记录进程应该获得的"公平时间"
2. **vruntime增长速率**与进程权重成反比
   - 权重越高(优先级越高)，vruntime增长越慢
   - 权重越低(优先级越低)，vruntime增长越快
3. **调度器总是选择vruntime最小的进程**执行
4. **vruntime最小的进程运行后**，其vruntime会增加

```c
// vruntime计算
vruntime += delta_time * (NICE_0_LOAD / process_weight)

// 这意味着：
// - 高优先级(高权重)进程：vruntime增长慢 -> 获得更多CPU时间
// - 低优先级(低权重)进程：vruntime增长快 -> 获得更少CPU时间
```

---

### 面试题2：Linux调度器的分类和优先级

**问题**：Linux中有哪些调度类？它们的优先级顺序是怎样的？

**答案**：

Linux调度器按优先级从高到低：

1. **stop_sched_class** - 最高优先级
   - 用于CPU隔离和任务迁移
   - 可抢占所有其他任务

2. **dl_sched_class** - Deadline调度
   - 硬实时任务
   - 使用EDF算法保证deadline

3. **rt_sched_class** - 实时调度
   - SCHED_FIFO：无时间片限制
   - SCHED_RR：固定时间片轮转

4. **fair_sched_class** - CFS普通调度
   - SCHED_NORMAL、SCHED_BATCH、SCHED_IDLE

5. **idle_sched_class** - 空闲调度
   - CPU空闲时运行

---

### 面试题3：调度延迟与吞吐量的平衡

**问题**：Linux如何平衡调度延迟(响应时间)和吞吐量？

**答案**：

1. **时间片分配**：
   - 交互式进程：较小时间片，频繁切换，低延迟
   - CPU密集型进程：较大时间片，减少切换开销，高吞吐量

2. **唤醒抢占**：
   - 交互式进程唤醒时，立即抢占当前任务
   - 避免等待时间片到期

3. **调度延迟控制**：
   - CFS通过vruntime保证延迟
   - RT调度保证响应时间

4. **负载均衡**：
   - 定期在CPU核心间迁移任务
   - 保证各核心负载均衡

---

### 面试题4：ARM64调度器的特点

**问题**：ARM64架构下的调度器有哪些特点？

**答案**：

1. **Big.LITTLE支持**：
   - 大核和小核的计算能力不同
   - 调度器考虑CPU capacity 进行任务分配

2. **抢占式调度**：
   - ARM64支持抢占
   - 支持tickless减少功耗

3. **上下文切换优化**：
   - ARM64使用专用寄存器
   - TLS通过TPIDR_EL0管理

4. **SPE/PMU支持**：
   - 利用性能监控单元进行调度决策

5. **能耗管理**：
   - 与DVFS协同工作
   - 考虑功耗进行任务迁移

---

### 面试题5：实时系统对调度器的要求

**问题**：硬实时系统对调度器有哪些要求？

**答案**：

1. **确定性**：
   - 最坏情况执行时间(WCET)可预测
   - 无锁调度算法

2. **优先级抢占**：
   - 高优先级任务必须立即抢占低优先级任务
   - 调度延迟有上限

3. **Deadline保证**：
   - SCHED_DEADLINE使用EDF算法
   - 确保在系统负载<100%时保证deadline

4. **避免优先级反转**：
   - 使用优先级继承协议(PI)
   - 防止低优先级任务阻塞高优先级任务

```c
// SCHED_DEADLINE使用示例
struct sched_attr {
    __u32 size;
    __u32 sched_policy;
    __u64 sched_flags;
    __s64 sched_nice;
    __u32 sched_priority;
    __u64 sched_runtime;
    __u64 sched_deadline;
    __u64 sched_period;
};

struct sched_attr attr = {
    .size = sizeof(attr),
    .sched_policy = SCHED_DEADLINE,
    .sched_runtime = 10 * 1000 * 1000,  // 10ms
    .sched_deadline = 20 * 1000 * 1000, // 20ms
    .sched_period = 20 * 1000 * 1000,   // 20ms周期
};

sched_setattr(0, &attr, 0);
```

---

### 面试题6：调度器相关的系统调用

**问题**：有哪些系统调用与进程调度相关？

**答案**：

1. **sched_setscheduler/sched_getscheduler** - 设置/获取调度策略
2. **sched_setparam/sched_getparam** - 设置/获取调度参数
3. **sched_get_priority_max/sched_get_priority_min** - 获取优先级范围
4. **sched_setaffinity/sched_getaffinity** - 设置/获取CPU亲和性
5. **sched_yield** - 主动让出CPU
6. **sched_getcpu** - 获取当前CPU编号
7. **setpriority/getpriority** - 设置/获取nice值
8. **nice** - 调整nice值

---

### 面试题7：调度器的性能优化

**问题**：如何分析和优化调度器的性能？

**答案**：

1. **使用perf**：
```bash
# 测量调度延迟
perf sched latency

# 测量调度器事件
perf sched record -- ./program
perf sched latency

# 分析上下文切换
perf sched context-switches
```

2. **使用ftrace**：
```bash
# 跟踪调度器
echo 'sched:sched_switch' > /sys/kernel/debug/tracing/events/sched/enable
echo 'sched:sched_wakeup' > /sys/kernel/debug/tracing/events/sched/enable
cat /sys/kernel/debug/tracing/trace
```

3. **使用sched_debug**：
```bash
cat /proc/sched_debug
cat /proc/[pid]/sched
```

4. **常见优化**：
   - 减少不必要的上下文切换
   - 合理设置进程优先级
   - 使用CPU亲和性绑定
   - 避免过度唤醒

---

## 本章小结

本章深入分析了Linux进程调度机制：

1. **调度器架构**：模块化设计，多调度类协同
2. **CFS调度器**：基于vruntime的完全公平调度
3. **实时调度**：FIFO、RR、Deadline调度策略
4. **ARM64实现**：特有的上下文切换、Big.LITTLE支持
5. **调度API**：系统调用和调度策略设置

调度是内核最核心的子系统之一，理解调度机制对系统性能优化至关重要。

---

*上一页：[第一章：进程创建](./01-process-creation.md)*
*下一页：[第三章：进程管理](./03-process-management.md)*

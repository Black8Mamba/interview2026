# 第 9 章：调度器设计与实现

## 9.1 调度器架构

### 9.1.1 概述

调度器是 Hypervisor 的核心组件，负责管理多个虚拟机的运行。Stellar SDK 的调度器采用时间片轮转调度算法，支持最多 12 个 VM。

### 9.1.2 调度器架构图

```
+-------------------+
|    Scheduler      |
+-------------------+
|  - VM Table      |
|  - Time Slice    |
|  - Priority      |
|  - State Machine |
+-------------------+
         |
         v
+-------------------+
|   VM Contexts    |
+-------------------+
| VM0 | VM1 | ... |
+-------------------+
```

### 9.1.3 关键参数

参考源码：`parts/virt/st_hypervisor/Scheduler/include/sched.h`

```c
/** @brief Scheduling time slice in microseconds. */
#define SCHEDULER_TIMEOUT         1000u

/** @brief Maximum number of VMs supported. */
#define VM_MAX_NUMBER            12

/** @brief Maximum length of VM description string. */
#define VM_DESCRIPTION_MAXLENGTH 59
```

## 9.2 VM 状态管理

### 9.2.1 VM 状态定义

参考源码：`parts/virt/st_hypervisor/Scheduler/include/sched.h`

```c
typedef enum {
    SCHED_VM_NONE,        /**< VM only initialized */
    SCHED_VM_STARTED,     /**< VM Started */
    SCHED_VM_STOPPED,     /**< VM Stopped */
    SCHED_VM_SUSPENDED,   /**< VM Suspended */
    SCHED_VM_RESUMED,     /**< VM Resumed */
    SCHED_VM_ERROR        /**< VM in Error */
} sched_vm_status_t;
```

### 9.2.2 状态转换

```
SCHED_VM_NONE
     |
     v
SCHED_VM_STARTED <---> SCHED_VM_STOPPED
     |
     v
SCHED_VM_SUSPENDED <---> SCHED_VM_RESUMED
     |
     v
SCHED_VM_ERROR
```

### 9.2.3 VM 控制块

```c
typedef struct {
    uint32_t vmid;                 // VM ID
    uint32_t priority;             // 调度优先级
    uint32_t time_slice;          // 时间片大小
    sched_vm_status_t status;     // VM 状态
    uint32_t entry_point;         // 入口地址
    uint32_t stack_ptr;           // 栈指针
    uint32_t context_id;          // 上下文 ID
} vm_control_block_t;
```

## 9.3 调度算法

### 9.3.1 时间片轮转

Stellar SDK 调度器采用时间片轮转（Round-Robin）调度算法：

```
时间片 1 (1000us)     时间片 2 (1000us)     时间片 3 (1000us)
+----------------+    +----------------+    +----------------+
|     VM 0      | -> |     VM 1      | -> |     VM 2      |
+----------------+    +----------------+    +----------------+
```

### 9.3.2 调度流程

```c
void scheduler_tick(void)
{
    // 1. 获取当前运行的 VM
    current_vm = get_current_vm();

    // 2. 更新 VM 运行时间
    update_vm_runtime(current_vm);

    // 3. 检查时间片是否用完
    if (current_vm.time_elapsed >= SCHEDULER_TIMEOUT) {
        // 4. 选择下一个 VM
        next_vm = select_next_vm();

        // 5. 切换上下文
        context_switch(current_vm, next_vm);
    }
}
```

### 9.3.3 调度选择

```c
vm_id_t select_next_vm(void)
{
    vm_id_t next_vm = INVALID_VM_ID;

    // 从当前 VM 之后开始查找
    for (int i = 1; i <= VM_MAX_NUMBER; i++) {
        vm_id_t candidate = (current_vm_id + i) % VM_MAX_NUMBER;

        // 检查 VM 是否可运行
        if (vm_table[candidate].status == SCHED_VM_STARTED) {
            next_vm = candidate;
            break;
        }
    }

    return next_vm;
}
```

## 9.4 QoS 机制

### 9.4.1 优先级

每个 VM 可以设置不同的优先级：

```c
typedef struct {
    uint32_t vmid;
    uint32_t priority;     // 0 = 最高优先级
    uint32_t budget;       // 时间片预算
    uint32_t period;       // 周期
} vm_qos_config_t;
```

### 9.4.2 带宽保护

```c
typedef struct {
    uint32_t vmid;
    uint32_t min_budget;   // 最小带宽保证
    uint32_t max_budget;   // 最大带宽限制
} vm_budget_t;
```

### 9.4.3 QoS 调度

```c
vm_id_t select_next_vm_qos(void)
{
    // 1. 首先检查需要带宽保证的 VM
    for (int i = 0; i < VM_MAX_NUMBER; i++) {
        if (vm_needs_budget(vm_table[i])) {
            if (vm_table[i].elapsed_budget < vm_table[i].min_budget) {
                return i;  // 优先调度需要带宽的 VM
            }
        }
    }

    // 2. 然后按优先级调度
    return select_next_vm_by_priority();
}
```

## 9.5 核心函数分析

### 9.5.1 调度器初始化

```c
void sched_init(void)
{
    // 1. 初始化 VM 表
    memset(vm_table, 0, sizeof(vm_table));

    // 2. 初始化调度器状态
    scheduler_state.status = SCHED_STOPPED;
    scheduler_state.current_vm = INVALID_VM_ID;

    // 3. 配置时间片
    scheduler_state.time_slice = SCHEDULER_TIMEOUT;
}
```

### 9.5.2 VM 启动

```c
int32_t sched_start_vm(vm_id_t vmid, vm_config_t *config)
{
    // 1. 检查 VM ID 有效
    if (vmid >= VM_MAX_NUMBER) {
        return -EINVAL;
    }

    // 2. 配置 VM
    vm_table[vmid].priority = config->priority;
    vm_table[vmid].time_slice = config->time_slice;
    vm_table[vmid].entry_point = config->entry_point;
    vm_table[vmid].status = SCHED_VM_STARTED;

    return 0;
}
```

### 9.5.3 VM 停止

```c
int32_t sched_stop_vm(vm_id_t vmid)
{
    if (vmid >= VM_MAX_NUMBER) {
        return -EINVAL;
    }

    vm_table[vmid].status = SCHED_VM_STOPPED;

    // 如果是当前运行的 VM，触发调度
    if (vmid == current_vm_id) {
        schedule();
    }

    return 0;
}
```

### 9.5.4 上下文切换

```c
void context_switch(vm_id_t from, vm_id_t to)
{
    // 1. 保存当前 VM 上下文
    save_vm_context(from);

    // 2. 加载新 VM 上下文
    load_vm_context(to);

    // 3. 更新当前 VM
    current_vm_id = to;
}
```

## 9.6 HVC 接口

### 9.6.1 调度器服务

参考源码：`parts/virt/st_hypervisor/Scheduler/src/sched_hyp.c`

调度器通过 HVC 接口提供服务：

```c
void sched_hyper_exec(uint32_t vm_id, plat_regs_t *args)
{
    uint32_t func_id = HYPER_GET_FUNCTION_ID(args->regs[0]);

    switch (func_id) {
    case SCHED_START_VM:
        // 启动 VM
        break;

    case SCHED_STOP_VM:
        // 停止 VM
        break;

    case SCHED_SUSPEND_VM:
        // 暂停 VM
        break;

    case SCHED_RESUME_VM:
        // 恢复 VM
        break;

    case SCHED_TERMINATEVM_ID:
        // 终止 VM
        break;

    default:
        break;
    }
}
```

### 9.6.2 服务调用示例

```c
// 启动 VM
void vm_start(uint32_t vmid)
{
    plat_regs_t args;
    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_SCHED_ID, 0, SCHED_START_VM);
    args.regs[1] = vmid;
    _hyper_trampoline(&args);
}

// 终止 VM
void vm_terminate(uint32_t vmid)
{
    plat_regs_t args;
    args.regs[0] = HYPER_MAKE_FUNCT_ID(HYPER_SCHED_ID, 0, SCHED_TERMINATEVM_ID);
    args.regs[1] = vmid;
    _hyper_trampoline(&args);
}
```

## 9.7 本章小结

本章详细介绍了调度器的设计与实现：

1. **调度器架构**：支持最多 12 个 VM 的时间片轮转调度
2. **VM 状态管理**：NONE/STARTED/STOPPED/SUSPENDED/RESUMED/ERROR 六种状态
3. **调度算法**：基于时间片的轮转调度
4. **QoS 机制**：支持优先级和带宽保护
5. **核心函数**：初始化、启动、停止、上下文切换
6. **HVC 接口**：通过 HVC 调用调度器服务

---

*参考资料：*
- `parts/virt/st_hypervisor/Scheduler/include/sched.h`
- `parts/virt/st_hypervisor/Scheduler/src/sched.c`
- `parts/virt/st_hypervisor/Scheduler/src/sched_hyp.c`

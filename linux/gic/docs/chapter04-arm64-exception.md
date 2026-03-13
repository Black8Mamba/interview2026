# 第4章 ARM64架构与异常处理

本章将深入讲解ARM64架构的异常处理机制，这是理解Linux内核中断子系统的硬件基础。ARM64（也称为AArch64）是ARMv8-A架构的64位执行状态，它引入了一套全新的异常处理模型，包括异常级别（Exception Level）、异常向量表和堆栈切换机制。理解这些底层机制对于驱动开发者和系统性能工程师至关重要。

## 4.1 ARM64异常级别

### 4.1.1 异常级别概述

ARM64架构定义了四个异常级别（Exception Level，EL），从EL0到EL3，每个级别有不同的权限和用途。这种分层设计实现了操作系统与应用程序的隔离，以及虚拟化技术的支持。

```
+-------------------+
|     EL3           |  安全_monitor模式，用于安全世界切换
+-------------------+
|     EL2           |  Hypervisor（虚拟化监视器）
+-------------------+
|     EL1           |  操作系统内核
+-------------------+
|     EL0           |  用户应用程序
+-------------------+
```

- **EL0（Application）**：用户空间代码执行级别，权限最低，不能直接访问硬件或修改系统控制寄存器
- **EL1（OS Kernel）**：操作系统内核执行级别，拥有更高的特权，可以访问所有硬件资源和系统控制寄存器
- **EL2（Hypervisor）**：虚拟化监视器级别，提供硬件虚拟化支持，多个虚拟机可以在EL1级别独立运行
- **EL3（Secure Monitor）**：安全监视器级别，负责安全世界与非安全世界之间的切换，是ARM TrustZone安全架构的核心

### 4.1.2 异常级别切换

异常级别之间的切换通过特定的指令和异常机制完成。从低权限级别到高权限级别的切换通常由异常触发，而从高到低的切换则通过ERET指令返回。

```asm
# 从EL1切换到EL0（系统调用返回）
mov     x0, #0
msr     spsr_el1, x0      # 设置目标状态（EL0）
msr     elr_el1, x30      # 设置返回地址
eret                     # 返回到EL0

# 触发异常进入EL1（系统调用）
mov     x8, #SYS_read     # 系统调用号
svc     #0                # 触发SVC异常，进入EL1
```

### 4.1.3 SCR_EL3寄存器

SCR_EL3（Secure Configuration Register）是一个关键的系统控制寄存器，用于配置安全状态和异常路由。在ARM64 Linux内核中，这个寄存器通常在内核启动时设置。

```c
// arch/arm64/include/asm/sysreg.h
#define SCR_EL3_NS        (1 << 0)   # Non-Secure位：0=安全世界，1=非安全世界
#define SCR_EL3_IRQ       (1 << 1)   # IRQ路由：将IRQ异常路由到EL3
#define SCR_EL3_FIQ       (1 << 2)   # FIQ路由：将FIQ异常路由到EL3
#define SCR_EL3_EA        (1 << 3)   # External Abort路由
#define SCR_EL3_SMD       (1 << 7)   # Secure Monitor Disable
#define SCR_EL3_HCE       (1 << 8)   # Hypervisor Call Enable
#define SCR_EL3_SIF       (1 << 9)   # Secure Instruction Fetch

// 内核启动时设置SCR_EL3
static void setup_scr(void)
{
    unsigned long scr = 0;

    scr |= SCR_EL3_NS;       # 启用非安全世界
    scr |= SCR_EL3_IRQ;      # 路由IRQ到EL3（如果需要）
    scr |= SCR_EL3_EA;       # 路由External Abort到EL3
    scr |= SCR_EL3_SMD;      # 禁用SMC指令（在EL2可用时）

    write_sysreg(scr, scr_el3);
}
```

### 4.1.4 EL2与虚拟化

在支持虚拟化的ARM64处理器上，Linux内核可以运行在EL2模式（如果可用），这提供了更好的虚拟化性能。KVM（Kernel-based Virtual Machine）正是利用这一特性来实现硬件辅助虚拟化。

```c
// 检查虚拟化支持
static bool has_vhe(void)
{
    #ifdef CONFIG_ARM64_VHE
    u64 id_aa64mmfr0 = read_sysreg_s(SYS_ID_AA64MMFR0_EL1);
    return ID_AA64MMFR0_VHE(id_aa64mmfr0) >= ID_AA64MMFR0_VHE_1_0;
    #else
    return false;
    #endif
}
```

当处理器支持VHE（Virtualization Host Extensions）时，Linux内核可以直接在EL2运行，这样hypervisor功能可以直接在内核态实现，而不需要单独的hypervisor层。

## 4.2 异常向量表

### 4.2.1 VBAR_EL1寄存器

在ARM64架构中，每个异常级别都有其对应的向量基地址寄存器（Vector Base Address Register）。对于EL1和EL0，向量表由VBAR_EL1寄存器指定。

```c
// 设置异常向量表基地址
void set_vbar(unsigned long vbar)
{
    write_sysreg(vbar, vbar_el1);
}

// 获取当前VBAR_EL1值
unsigned long get_vbar(void)
{
    return read_sysreg(vbar_el1);
}
```

Linux内核的异常向量表定义在`arch/arm64/kernel/entry.S`文件中，向量表的基地址在启动时通过`set_vbar`函数设置。

### 4.2.2 向量表布局

ARM64的异常向量表包含16个入口，每个入口对应一种特定的异常情况。向量表的组织基于两个维度：异常类型（同步/异步）和发生异常时的状态。

```asm
# arch/arm64/kernel/entry.S
/*
 * 异常向量表布局
 * 每个入口间隔128字节（0x80），总共需要2KB
 */
    .align 7
ENTRY(vectors)
    # 同步异常，从EL0发生（用户空间）
    kernel_entry 0
    b   ret_to_user

    # IRQ/FIQ同步异常，从EL0发生
    kernel_entry 0
    b   ret_to_user

    # 系统错误（Error），从EL0发生
    kernel_entry 0
    b   ret_to_user

    # 未定义指令，从EL0发生
    kernel_entry 0
    b   ret_to_user

    # 同步异常，从EL1发生（内核空间）
    kernel_entry 1
    b   el1_sync

    # IRQ/FIQ，从EL1发生
    kernel_entry 1
    b   el1_irq

    # 系统错误（Error），从EL1发生
    kernel_entry 1
    b   el1_error

    # 未定义指令，从EL1发生
    kernel_entry 1
    b   el1_undef

    # 同步异常，32位模式
    kernel_entry 0
    b   ret_to_user

    # 其他异常类型...
    .align 7
    # ... (更多向量入口)
END(vectors)
```

### 4.2.3 四种异常类型

ARM64架构将异常分为四大类，每类异常有不同的处理方式：

1. **同步异常（Synchronous）**：由指令执行直接导致的异常，包括系统调用（SVC）、未定义指令、数据访问中止等

2. **IRQ（Interrupt Request）**：普通中断请求，通常由外设触发，用于设备通知

3. **FIQ（Fast Interrupt Request）**：快速中断请求，优先级高于IRQ，在某些场景下使用

4. **SError（System Error）**：系统错误，包括异步数据中止等硬件错误

```asm
# 异常类型对应的向量偏移
# VBAR_EL1 + 0x000 - 同步异常（64位，EL0）
# VBAR_EL1 + 0x080 - IRQ/FIQ（64位，EL0）
# VBAR_EL1 + 0x100 - SError（64位，EL0）
# VBAR_EL1 + 0x180 - 未定义（64位，EL0）
# VBAR_EL1 + 0x200 - 同步异常（64位，EL1）
# VBAR_EL1 + 0x280 - IRQ/FIQ（64位，EL1）
# VBAR_EL1 + 0x300 - SError（64位，EL1）
# VBAR_EL1 + 0x380 - 未定义（64位，EL1）
```

### 4.2.4 异常原因寄存器

当发生同步异常时，ESR_EL1（Exception Syndrome Register）寄存器包含了关于异常的详细信息，用于确定异常的具体原因。

```c
// arch/arm64/include/asm/esr.h
#define ESR_EL1_EC_SHIFT          26
#define ESR_EL1_EC_MASK          (0x3F << ESR_EL1_EC_SHIFT)
#define ESR_EL1_ISS_MASK         0x1FFFFFF

// 异常类型枚举
enum {
    ESR_EL1_EC_UNKNOWN         = 0x00,
    ESR_EL1_EC_WFI_WFE         = 0x01,
    ESR_EL1_EC_MCR_MRC_CP15    = 0x03,
    ESR_EL1_EC_MCRR_MRRC_CP15  = 0x04,
    ESR_EL1_EC_MCR_MRC_CP14    = 0x05,
    ESR_EL1_EC_LDC_STC_CP14    = 0x06,
    ESR_EL1_EC_SVC             = 0x11,
    ESR_EL1_EC_HVC            = 0x12,
    ESR_EL1_EC_SMC            = 0x13,
    ESR_EL1_EC_SERROR         = 0x1F,
    ESR_EL1_EC_IABT_LOW       = 0x20,
    ESR_EL1_EC_IABT_CUR       = 0x21,
    ESR_EL1_EC_DABT_LOW       = 0x24,
    ESR_EL1_EC_DABT_CUR       = 0x25,
    ESR_EL1_EC_UNDEFINSTR     = 0x30,
};

// 获取异常原因
static inline u32 esr_el1_read(void)
{
    return read_sysreg_s(SYS_ESR_EL1);
}

static inline int esr_exception_class(u32 esr)
{
    return esr >> ESR_EL1_EC_SHIFT & 0x3F;
}
```

## 4.3 异常处理入口

### 4.3.1 入口代码概述

当CPU发生异常时，硬件会自动保存部分状态（PSTATE到SPSR_ELx，返回地址到ELR_ELx），然后跳转到对应的向量入口。Linux内核在`entry.S`中定义了完整的异常处理入口代码，主要任务是保存现场并分派到适当的处理函数。

```asm
# arch/arm64/kernel/entry.S
# kernel_entry宏：保存通用寄存器
.macro    kernel_entry, el, regsize = 64
    # 检查异常级别
    .if \el == 0
        # 从EL0进入：sp_el0是用户栈指针
        mrs    x21, sp_el0
    .else
        # 从EL1进入：使用内核栈
        mov    x21, sp
    .endif

    # 为pt_regs结构分配栈空间
    sub    sp, sp, #PT_REGS_SIZE

    # 保存通用寄存器x0-x29（每个寄存器8字节）
    stp    x0, x1, [sp, #S_X0]
    stp    x2, x3, [sp, #S_X2]
    stp    x4, x5, [sp, #S_X4]
    stp    x6, x7, [sp, #S_X6]
    stp    x8, x9, [sp, #S_X8]
    stp    x10, x11, [sp, #S_X10]
    stp    x12, x13, [sp, #S_X12]
    stp    x14, x15, [sp, #S_X14]
    stp    x16, x17, [sp, #S_X16]
    stp    x18, x19, [sp, #S_X18]
    stp    x20, x21, [sp, #S_X20]
    stp    x22, x23, [sp, #S_X22]
    stp    x24, x25, [sp, #S_X24]
    stp    x26, x27, [sp, #S_X26]
    stp    x28, x29, [sp, #S_X28]

    # 保存栈指针
    mov    x9, sp
    str    x9, [sp, #S_SP]

    # 保存返回地址（x30，即LR）
    str    x30, [sp, #S_LR]

    # 保存PSTATE（从spsr_el1）
    mrs    x9, spsr_el1
    str    x9, [sp, #S_PSTATE]

    # 保存ELR_EL1（异常返回地址）
    mrs    x9, elr_el1
    str    x9, [sp, #S_PC]
.endm
```

### 4.3.2 pt_regs结构

`pt_regs`结构用于保存异常发生时的完整寄存器状态，这个结构在中断处理和系统调用返回等场景中广泛使用。

```c
// arch/arm64/include/asm/ptrace.h
struct pt_regs {
    union {
        struct user_pt_regs user_regs;
        struct {
            u64 regs[31];
            u64 sp;
            u64 pc;
            u64 pstate;
        };
    };
    u64 orig_x0;
    #ifdef CONFIG_ARM64_SW_TTBR0_PAN
    u64 ttbr0;
    #endif
    u64 syscallno;
    u64 orig_addr_limit;
    u64 stackframe[2];
    unsigned long fault_address;
    unsigned long far;
    u64 esr;
};

#define user_regs(r)    ((r)->user_regs)
#define pt_regs_task(r) ((void *)(r)->sp)

/* 栈帧中各寄存器的偏移量 */
#define S_X0        0
#define S_X1        8
#define S_X2        16
#define S_X3        24
#define S_X4        32
#define S_X5        40
#define S_X6        48
#define S_X7        56
#define S_X8        64
#define S_X9        72
#define S_X10       80
#define S_X11       88
#define S_X12       96
#define S_X13       104
#define S_X14       112
#define S_X15       120
#define S_X16       128
#define S_X17       136
#define S_X18       144
#define S_X19       152
#define S_X20       160
#define S_X21       168
#define S_X22       176
#define S_X23       184
#define S_X24       192
#define S_X25       200
#define S_X26       208
#define S_X27       216
#define S_X28       224
#define S_LR        232
#define S_SP        240
#define S_PC        248
#define S_PSTATE    256
```

### 4.3.3 中断处理分发

在保存完现场后，异常处理代码会根据异常类型跳转到相应的处理函数。对于IRQ中断，主要的处理流程如下：

```asm
# EL1 IRQ处理入口
el1_irq:
    kernel_entry 1
    mov    x0, sp
    bl    handle_arch_exception
    b    ret_to_kernel

# handle_arch_exception函数
ENTRY(handle_arch_exception)
    # x0 = sp (pt_regs指针)
    mrs    x1, esr_el1          # 获取异常综合寄存器
    lsr    x1, x1, #26          # 提取异常类别
    cmp    x1, #0x18            # IRQ异常类别
    b.eq   el1_irq_handler
    cmp    x1, #0x11            # SVC异常
    b.eq   el1svc_handler
    # 处理其他异常...
    ret
ENDPROC(handle_arch_exception)
```

```c
// arch/arm64/kernel/irq.c
asmlinkage void handle_arch_exception(struct pt_regs *regs)
{
    u64 esr = regs->esr;
    u64 ec = esr >> 26;

    switch (ec) {
    case 0x18:  // IRQ/FIQ
    case 0x1C:  // IRQ from current exception level
        handle_irq(regs);
        break;
    case 0x11:  // SVC
        do_syscall(regs);
        break;
    case 0x20:  // Instruction Abort
    case 0x21:  // Instruction Abort from lower level
    case 0x24:  // Data Abort
    case 0x25:  // Data Abort from lower level
        do_translation_fault(regs);
        break;
    default:
        unhandled_exception(regs);
        break;
    }
}
```

### 4.3.4 保存现场的重要性

保存现场是异常处理的关键步骤，确保被中断的代码可以在异常处理完成后正确恢复执行。保存的内容包括：

- 所有通用寄存器（x0-x29）
- 栈指针（sp）
- 返回地址（x30/LR）
- 程序计数器（pc）
- 处理器状态（pstate）

```asm
# 恢复现场（返回时使用）
.macro    kernel_exit, el
    # 恢复通用寄存器
    ldp    x0, x1, [sp, #S_X0]
    ldp    x2, x3, [sp, #S_X2]
    # ... 恢复更多寄存器

    # 恢复栈指针
    ldr    x9, [sp, #S_SP]
    mov    sp, x9

    # 恢复PSTATE到spsr_el1
    ldr    x9, [sp, #S_PSTATE]
    msr    spsr_el1, x9

    # 恢复ELR_EL1
    ldr    x9, [sp, #S_PC]
    msr    elr_el1, x9

    # 返回
    eret
.endm
```

## 4.4 SP_EL0/EL1切换

### 4.4.1 堆栈切换机制

ARM64架构提供了独立的栈指针寄存器用于不同异常级别。当发生异常时，硬件会自动切换到对应级别的栈指针寄存器。具体规则如下：

- 当发生异常进入EL1时：sp_el0的值被保存，sp切换到sp_el1
- 当异常返回到EL0时：sp_el0被恢复为之前保存的值

```asm
# 异常发生时的栈指针行为
# 假设当前在EL0执行（用户空间），发生IRQ中断

# 进入异常前：
#   - sp指向用户栈（sp_el0的内容）
#   - sp_el1保持内核栈指针不变

# 触发IRQ异常后（硬件自动完成）：
#   - sp_el0 = 当前sp（硬件自动保存）
#   - sp = sp_el1（切换到内核栈）

# 在EL1处理中断：
#   - 使用sp（此时是内核栈）进行栈操作

# 返回EL0时（执行eret）：
#   - sp = sp_el0（硬件自动恢复）
```

### 4.4.2 kernel_mode_sp

Linux内核使用`kernel_mode_sp`宏来检查当前使用的是内核栈还是用户栈。在中断处理中，这一机制用于确定栈指针的合法性。

```c
// arch/arm64/include/asm/ptrace.h
static inline bool kernel_mode(enum pt_regs_mode r)
{
    return r == PT_REGS_MODE_KERNEL;
}

static inline bool user_mode(enum pt_regs_mode r)
{
    return r == PT_REGS_MODE_USER;
}

/* 检查栈指针是否在内核空间 */
static inline bool on_thread_stack(void)
{
    return current_stack_pointer & ~(THREAD_SIZE - 1);
}
```

### 4.4.3 内核态与用户态栈

Linux为每个进程分配独立的用户栈和内核栈。内核栈用于系统调用和中断处理，用户栈用于用户空间执行。

```c
// 每个进程的栈结构
struct task_struct {
    #ifdef CONFIG_THREAD_INFO_IN_TASK
        struct thread_info      thread_info;
    #endif
    void                      *stack;        /* 指向内核栈 */
    /* ... */
};

#define THREAD_SIZE  16384   /* 16KB内核栈 */

/* 获取当前任务的内核栈起始地址 */
static inline void *task_stack_page(const struct task_struct *task)
{
    return task->stack;
}

/* 获取当前任务的内核栈顶 */
static inline void *task_pt_regs(struct task_struct *task)
{
    return task->stack + THREAD_SIZE - PT_REGS_SIZE;
}
```

### 4.4.4 栈切换实现

在实际的异常处理中，栈切换涉及到多个步骤。内核在启动时会为每个CPU设置栈空间，并在上下文切换时管理栈指针。

```asm
# 初始化CPU内核栈
.macro init_stack, tmp1, tmp2
    ldr    \tmp1, =init_task
    add    \tmp2, \tmp1, #THREAD_SIZE
    msr    sp_el1, \tmp2
.endm

# 切换到内核栈（在异常入口中）
.macro switch_to_kernel_stack
    # 检查是否需要切换到内核栈
    # 如果当前已经在内核栈上，不需要切换
    tbnz    x21, #63, .L_already_kernel

    # 从sp_el0获取用户栈，切换到内核栈
    # x21保存了sp_el0的值
    mov    sp, x21

.L_already_kernel:
.endm
```

### 4.4.5 实践示例

以下示例展示如何在驱动代码中理解和使用栈切换机制：

```c
// 在中断处理中获取正确的栈信息
void dump_stack_info(void)
{
    struct pt_regs *regs = get_irq_regs();

    if (!regs)
        return;

    /* 判断异常发生时的栈类型 */
    bool from_user = user_mode(regs);

    printk("Exception from: %s\n", from_user ? "user" : "kernel");

    /* 获取保存的栈指针 */
    unsigned long sp = regs->sp;
    unsigned long pc = regs->pc;

    printk("PC: 0x%lx, SP: 0x%lx\n", pc, sp);
}

/* 在调度器中切换栈 */
__visible void __sched notrace schedule(void)
{
    struct task_struct *prev, *next;
    unsigned long *switch_stack;

    /* ... 调度代码 ... */

    /* 切换到新任务的栈 */
    switch_stack = (unsigned long *)(next->stack + THREAD_SIZE);
    asm volatile(
        "mov    sp, %0"
        : "r" (switch_stack)
    );

    /* 继续执行新任务 */
}
```

理解SP_EL0和SP_EL1的切换机制对于调试中断相关问题非常重要。当系统出现栈溢出或栈指针错误时，首先需要确定异常发生时的栈状态，区分是用户态还是内核态触发的异常。

---

## 本章面试题

### 1. ARM64的四个异常级别分别是什么？它们的权限关系如何？

**参考答案**：ARM64的四个异常级别从低到高依次是EL0（用户空间）、EL1（操作系统内核）、EL2（Hypervisor虚拟化监视器）和EL3（安全监视器）。EL0权限最低，只能执行普通指令；EL1拥有操作系统特权；EL2提供虚拟化支持；EL3负责安全世界切换。权限从EL0到EL3逐级升高，高级别可以访问低级别的资源，反之则需要通过异常机制。

### 2. 什么是VBAR_EL1寄存器？它在异常处理中起什么作用？

**参考答案**：VBAR_EL1（Vector Base Address Register）是ARM64架构中EL1级别的向量表基地址寄存器。它指定了异常向量表的起始地址，当在EL0或EL1发生异常时，CPU会根据异常类型和当前状态跳转到该表中对应的入口。Linux内核在启动时设置这个寄存器，指向预先定义好的异常向量表。

### 3. 描述ARM64异常向量表的结构，它包含哪些类型的异常入口？

**参考答案**：ARM64异常向量表包含16个入口（每个间隔128字节），分为四个象限：同步异常、IRQ/FIQ、SError和未定义指令。每个象限又根据发生异常时的状态（64位/32位，来自EL0/EL1）进一步细分。向量表基地址由VBAR_EL1寄存器指定，内核通过设置这个寄存器来配置异常处理入口。

### 4. 异常发生时，硬件自动保存哪些状态？Linux内核的entry.S做了什么额外工作？

**参考答案**：硬件自动保存PSTATE到SPSR_ELx、返回地址到ELR_ELx，并切换栈指针到对应级别的sp。Linux内核在entry.S中通过kernel_entry宏保存所有通用寄存器（x0-x29）、栈指针、返回地址和程序计数器到pt_regs结构中，并保存异常原因寄存器（ESR_EL1）等信息，以便后续处理和恢复。

### 5. 解释EL0和EL1之间的栈切换机制，sp_el0和sp_el1的作用是什么？

**参考答案**：ARM64为不同异常级别提供独立的栈指针寄存器。当发生异常从EL0进入EL1时，硬件自动将当前sp（用户栈）保存到sp_el1（实际是临时保存机制），然后sp切换到sp_el1（内核栈）。当从EL1返回EL0时，sp自动恢复为之前保存的用户栈。Linux内核利用这一机制确保中断处理始终在内核栈上执行，同时不破坏用户栈的状态。

### 6. ESR_EL1寄存器的作用是什么？如何通过它获取异常原因？

**参考答案**：ESR_EL1（Exception Syndrome Register）是异常综合寄存器，包含关于当前异常的详细信息。其中EC字段（Exception Class）指示异常类型（如同步异常、IRQ、数据中止等），ISS字段包含具体的异常原因（如系统调用号、访问错误类型等）。内核通过读取这个寄存器来区分不同的异常并采取相应的处理措施。

### 7. 在ARM64中，如何从异常处理返回到原来的执行上下文？

**参考答案**：从异常返回使用ERET指令，该指令会从SPSR_ELx恢复处理器状态到PSTATE，并从ELR_ELx恢复程序计数器。如果要返回到EL0，需要确保SPSR中的EL位设置为0。Linux内核在完成中断处理后，通过kernel_exit宏恢复所有保存的寄存器，然后执行ERET指令返回被中断的代码继续执行。

### 8. pt_regs结构在中断处理中的作用是什么？它保存了哪些信息？

**参考答案**：pt_regs是保存异常发生时完整寄存器状态的数据结构，用于中断处理函数访问被中断代码的寄存器值，以及在返回时恢复现场。它保存了所有通用寄存器（x0-x30）、栈指针sp、程序计数器pc、处理器状态pstate，以及异常特有的信息如orig_x0（系统调用原始参数）、fault_address（错误地址）、far（错误地址寄存器）和esr（异常原因寄存器）。

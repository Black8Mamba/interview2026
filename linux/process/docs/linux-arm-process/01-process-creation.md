# 第一章：Linux ARM64 进程创建

> 本章深入探讨Linux进程创建机制，从基础概念到ARM64架构特定实现

## 目录

- [1.1 进程与线程基础概念](#11-进程与线程基础概念)
- [1.2 Linux进程描述符task_struct](#12-linux进程描述符task_struct)
- [1.3 fork系统调用深度解析](#13-fork系统调用深度解析)
- [1.4 vfork与fork的区别](#14-vfork与fork的区别)
- [1.5 clone系统调用与线程创建](#15-clone系统调用与线程创建)
- [1.6 ARM64进程创建特定实现](#16-arm64进程创建特定实现)
- [1.7 常见面试题](#17-常见面试题)

---

## 1.1 进程与线程基础概念

### 进程的定义

进程是Linux系统中最核心的抽象概念之一。它是**执行中的程序**，是一个正在执行的程序实例。每个进程拥有独立的地址空间、文件描述符、信号处理等资源。

### 进程与线程的关系

| 特性 | 进程 | 线程 |
|------|------|------|
| 地址空间 | 独立 | 共享 |
| 资源 | 独立 | 共享 |
| 创建开销 | 大 | 小 |
| 通信方式 | IPC | 直接共享 |
| 独立性 | 高 | 低 |

在Linux中，**线程被实现为轻量级进程**。线程与进程使用相同的底层数据结构(task_struct)，主要区别在于是否共享地址空间。

### 进程ID与PID

每个进程在系统中都有唯一的标识符——PID(Process ID)。Linux通过pid_type枚举和pid结构来管理进程ID：

```c
// include/linux/pid.h
enum pid_type {
    PIDTYPE_PID,      // 进程ID
    PIDTYPE_TGID,    // 线程组ID(主线程PID)
    PIDTYPE_PGID,    // 进程组ID
    PIDTYPE_SID,     // 会话ID
    PIDTYPE_MAX
};
```

---

## 1.2 Linux进程描述符task_struct

### task_struct数据结构

task_struct是Linux内核中最重要的数据结构之一，定义在`include/linux/sched.h`中。它包含了进程的所有信息：

```c
// include/linux/sched.h
struct task_struct {
    volatile long state;          // 进程状态
    void *stack;                  // 指向内核栈的指针
    atomic_t usage;              // 引用计数
    int prio;                    // 动态优先级
    int static_prio;             // 静态优先级
    int normal_prio;             // 正常优先级
    unsigned int policy;          // 调度策略
    struct sched_entity se;      // CFS调度实体
    struct sched_rt_entity rt;   // RT调度实体
    struct sched_dl_entity dl;   // deadline调度实体

    /* 进程标识 */
    pid_t pid;                    // 进程ID
    pid_t tgid;                   // 线程组ID

    /* 进程关系 */
    struct task_struct __rcu *real_parent;  // 实际父进程
    struct task_struct __rcu *parent;       // 当前的父进程(可被ptrace改变)
    struct list_head children;               // 子进程链表
    struct list_head sibling;               // 兄弟链表节点

    /* 命名空间 */
    struct nsproxy *nsproxy;

    /* 凭证 */
    struct cred __rcu *real_cred;   // 实际凭证
    struct cred __rcu *cred;        // 可切换凭证

    /* 内存管理 */
    struct mm_struct *mm;          // 内存描述符
    struct mm_struct *active_mm;   // 活动内存描述符

    /* 文件系统信息 */
    struct fs_struct *fs;          // 文件系统信息
    struct files_struct *files;   // 文件描述符表

    /* 信号处理 */
    struct signal_struct *signal;  // 信号描述符
    struct sighand_struct *sighand;  // 信号处理程序

    /* 调试和审计 */
    int exit_code;                 // 退出码
    int exit_signal;               // 退出信号
    ...
} __randomize_layout;
```

### 内核栈与task_struct的关系

在ARM64架构中，每个进程都有一个独立的内核栈。当进程进入内核态时，使用这个栈。task_struct通过`stack`指针指向内核栈的底部。

```c
// ARM64内核栈布局(高地址向低地址增长)
+------------------------+ 0xffffffxxxxxxx000 (栈顶)
|   pt_regs (中断帧)     |  // 系统调用、中断保存的寄存器
+------------------------+
|   stack frame          |  // 函数调用栈帧
+------------------------+
|   task_struct         |  // task_struct位于内核栈底部
+------------------------+
```

---

## 1.3 fork系统调用深度解析

### fork系统调用流程

fork()是创建新进程的主要方式。其实现流程如下：

```
用户空间                    内核空间
   |                          |
fork()                      |
   |--> sys_fork()           |
   |      |                  |
   |   copy_process()        |
   |      |                  |
   |   dup_task_struct()     |  复制task_struct和内核栈
   |      |                  |
   |   copy_mm()            |  复制内存描述符
   |      |                  |
   |   copy_threads()       |  复制内核线程信息
   |      |                  |
   |   copy_signal()        |  复制信号描述符
   |      |                  |
   |   copy_files()         |  复制文件描述符
   |      |                  |
   |   sched_fork()         |  调度相关初始化
   |      |                  |
   |   ret_from_fork()      |  返回到用户空间
   |                          |
<--返回(new pid)            |
   |                          |
```

### 核心函数：copy_process()

```c
// kernel/fork.c
static __latent_entropy struct task_struct *copy_process(
    struct pid *pid,
    int trace,
    int node,
    struct kernel_clone_args *args)
{
    int pidfd;
    struct task_struct *p;
    struct multiprocess_signals delayed;

    /* 1. 复制task_struct和内核栈 */
    p = dup_task_struct(current, node);
    if (!p)
        return ERR_PTR(-ENOMEM);

    /* 2. 初始化调度实体 */
    se->vruntime = 0;
    se->exec_start = se->sum_exec_runtime = 0;

    /* 3. 复制进程资源 */
    if (args->flags & CLONE_THREAD) {
        /* 线程：共享信号描述符 */
        p->signal = current->signal;
    } else {
        /* 进程：创建新的信号描述符 */
        retval = copy_signal(p);
        if (retval)
            goto bad_fork_free;
    }

    /* 4. 复制内存空间 */
    if (args->flags & CLONE_VM) {
        /* 线程：共享内存空间 */
        p->mm = current->mm;
        p->active_mm = current->active_mm;
    } else {
        /* 进程：复制内存空间 */
        retval = copy_mm(p, args);
        if (retval)
            goto bad_fork_cleanup_signal;
    }

    /* 5. 复制文件描述符表 */
    retval = copy_files(args, p);
    if (retval)
        goto bad_fork_cleanup_mm;

    /* 6. 复制进程间通信 */
    retval = copy_sockargs(p);
    if (retval)
        goto bad_fork_cleanup_files;

    /* 7. 复制命名空间 */
    retval = copy_namespaces(args->flags, p);
    if (retval)
        goto bad_fork_cleanup_sock;

    /* 8. 调度相关初始化 */
    retval = sched_fork(p);
    if (retval)
        goto bad_fork_cleanup_namespaces;

    /* 9. 分配PID */
    pid = alloc_pid(p->nsproxy->pid_ns_for_children);
    if (IS_ERR(pid)) {
        retval = PTR_ERR(pid);
        goto bad_fork_cleanup_sched;
    }

    /* 10. 将新进程加入调度队列 */
    wake_up_new_task(p);

    return p;
}
```

### dup_task_struct()实现

```c
// kernel/fork.c
static struct task_struct *dup_task_struct(struct task_struct *orig, int node)
{
    struct task_struct *tsk;
    unsigned long *stack;
    int err;

    /* 分配task_struct */
    tsk = alloc_task_struct_node(node);
    if (!tsk)
        return NULL;

    /* 分配内核栈 */
    stack = __vmalloc_node_range(THREAD_SIZE, THREAD_SIZE,
                  VMALLOC_START, VMALLOC_END,
                  GFP_KERNEL, PAGE_KERNEL,
                  0, node,
                  __builtin_return_address(0));

    /* 设置task_struct的stack指针指向栈底 */
    tsk->stack = task_stack_page(tsk);

    /* 复制task_struct内容 */
    err = arch_dup_task_struct(tsk, orig);
    if (err)
        goto free_stack;

    /* 设置内核栈结束标记 */
    set_task_stack_end_magic(tsk);

    /* 引用计数初始化 */
    atomic_set(&tsk->usage, 2);

    return tsk;
}
```

---

## 1.4 vfork与fork的区别

### vfork系统调用

vfork()是fork()的一个变体，主要用于创建子进程立即exec()新程序的场景。

```c
// kernel/fork.c
SYSCALL_DEFINE0(vfork)
{
    return kernel_clone(&vfork_args);
}

static struct kernel_clone_args vfork_args = {
    .flags      = CLONE_VFORK | CLONE_VM | SIGCHLD,
    .exit_signal   = SIGCHLD,
    .stack      = 0,
    .parent_tidptr = NULL,
    .child_tidptr  = NULL,
    .tls        = 0,
};
```

### fork vs vfork对比

| 特性 | fork() | vfork() |
|------|--------|---------|
| 内存复制 | 完整复制(写时复制) | 共享父进程内存 |
| 执行顺序 | 父子进程不确定 | 子进程先执行 |
| 数据安全性 | 独立 | 共享(需小心) |
| 性能 | 较慢(需复制页表) | 快(直接共享) |
| 现代用法 | 推荐使用 | 不推荐(已淡化) |

### vfork的问题与演进

vfork()存在严重的数据安全问题：
1. 子进程修改父进程数据会导致不可预测结果
2. 子进程调用exec()前，父进程被阻塞
3. 子进程修改父进程的栈数据会导致错误

**Linux 2.2之后**：vfork()实现改为基于fork()，但设置了CLONE_VFORK标志，让写时复制(COW)机制在exec()前不真正复制物理页面。

---

## 1.5 clone系统调用与线程创建

### clone系统调用

clone()是Linux最灵活的进程/线程创建接口，可以精确控制哪些资源被共享。

```c
// include/uapi/linux/sched.h
#define CLONE_VM    0x00000200  /* 共享内存空间(线程) */
#define CLONE_FS    0x00000800  /* 共享文件系统信息 */
#define CLONE_FILES 0x00000400  /* 共享文件描述符表 */
#define CLONE_SIGHAND  0x00000800  /* 共享信号处理 */
#define CLONE_PIDFD  0x00002000  /* 返回pidfd */
#define CLONE_THREAD 0x00010000  /* 共享线程组 */
#define CLONE_NEWNS  0x00020000  /* 新挂载命名空间 */
#define CLONE_SYSVSEM 0x00040000  /* 共享System V semaphores */
#define CLONE_SETTLS  0x00080000  /* 设置TLS */
#define CLONE_PARENT_SETTID 0x00100000  /* 在父进程设置TID */
#define CLONE_CHILD_CLEARTID 0x00200000  /* 清除子进程TID */
#define CLONE_CHILD_SETTID 0x00400000  /* 在子进程设置TID */
#define CLONE_NEWCGROUP  0x00800000  /* 新cgroup命名空间 */
#define CLONE_NEWUTS    0x01000000  /* 新UTS命名空间 */
#define CLONE_NEWIPC    0x02000000  /* 新IPC命名空间 */
#define CLONE_NEWUSER   0x04000000  /* 新用户命名空间 */
#define CLONE_NEWPID    0x08000000  /* 新PID命名空间 */
#define CLONE_NEWNET    0x10000000  /* 新网络命名空间 */
```

### clone系统调用实现

```c
// kernel/fork.c
SYSCALL_DEFINE5(clone, unsigned long, flags, unsigned long, stack,
         int __user *, parent_tidptr,
         int __user *, child_tidptr,
         unsigned long, tls)
{
    struct kernel_clone_args args = {
        .flags      = flags,
        .stack      = stack,
        .parent_tidptr = parent_tidptr,
        .child_tidptr  = child_tidptr,
        .tls        = tls,
        .exit_signal = (flags & CSIGNAL),
        .node       = NUMA_NO_NODE,
    };

    return kernel_clone(&args);
}
```

### 线程创建示例

```c
// 使用pthread创建线程(底层调用clone)
#include <pthread.h>

void* thread_func(void* arg) {
    printf("New thread running\n");
    return NULL;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, NULL, thread_func, NULL);
    pthread_join(thread, NULL);
    return 0;
}
```

pthread_create()内部通过clone()系统调用创建线程，设置以下标志：
- CLONE_VM：共享虚拟内存空间
- CLONE_FS：共享文件系统信息
- CLONE_FILES：共享文件描述符表
- CLONE_SIGHAND：共享信号处理函数
- CLONE_THREAD：加入同一线程组
- CLONE_SYSVSEM：共享System V信号量

---

## 1.6 ARM64进程创建特定实现

### ARM64架构特点

ARM64(AArch64)是64位架构，具有以下特点：
- 64位通用寄存器(X0-X30, W0-W30)
- 独立的SP寄存器(SP_EL0, SP_EL1, SP_EL2, SP_EL3)
- 特权级别(EL0-EL3)
- 新的指令集(AArch64)

### ARM64进程创建关键差异

#### 1. 内核栈布局

```c
// arch/arm64/include/asm/stacktrace.h

/*
 * ARM64内核栈布局(从高地址向低地址增长):
 *
 * +----------------------+  <-- current task stack (THREAD_SIZE)
 * |   struct pt_regs    |  32个通用寄存器 + sp, pc, pstate
 * +----------------------+
 * |   saved fp          |  frame pointer
 * +----------------------+
 * |   saved lr          |  link register (return addr)
 * +----------------------+
 * |   local variables   |
 * +----------------------+
 * |   ...                |
 * +----------------------+
 * |   struct task_struct|  <-- task_stack_page(tsk)
 * +----------------------+
 */
```

#### 2. 系统调用入口

ARM64使用SVC指令触发系统调用：

```asm
// arch/arm64/kernel/entry.S
/*
 * ARM64系统调用入口
 * 用户空间: svc #0 触发系统调用
 *
 * 进入内核时保存的寄存器:
 * x0-x7: 系统调用参数
 * x8: 系统调用号
 * sp: 用户栈指针
 * pc: 返回地址
 * pstate: 处理器状态
 */

ENTRY(ret_from_fork)
    bl    schedule_tail
    cbz   x19, 1f           // x19 = ret_value
    mov   x0, x19
    bl   j__entry_trampoline  // 调用syscall返回函数
1:
    ret
```

#### 3. ARM64特有的task_struct成员

```c
// arch/arm64/include/asm/task_struct.h
struct task_struct {
    ...
    /* ARM64特定 */
    struct thread_info thread_info;  // 线程信息
    u64            stack_cookie;   // 栈保护cookie
    /* 浮点和SIMD状态 */
    struct fpsimd_state fpsimd_state;  // NEON/VFP寄存器状态
    /* TLS (Thread Local Storage) */
    struct user_fpsimd_state *fp_state;
    unsigned int    fpsimd_cpu;
    /* 系统调用 */
    int         syscallno;
    /* 断点调试 */
    struct debug_info  debug;
    ...
};
```

#### 4. 线程信息结构

```c
// arch/arm64/include/asm/thread_info.h
struct thread_info {
    unsigned long    flags;       // TIF_xxx标志
    int        preempt_count;   // 抢占计数
    mm_segment_t    addr_limit;  // 地址空间限制
    struct task_struct *task;    // 关联的task_struct
    int        cpu;             // CPU编号
};
```

### ARM64进程创建核心代码

```c
// arch/arm64/kernel/process.c
int copy_thread(unsigned long clone_flags,
        unsigned long stack_start,
        unsigned long stack_size,
        struct task_struct *p,
        unsigned long tls)
{
    struct pt_regs *childregs;
    struct fork_frame *frame;

    /* 获取子进程的内核栈 */
    frame = (struct fork_frame *)task_stack_page(p);
    childregs = (struct pt_regs *)frame + 1;

    /* 复制父进程的寄存器上下文 */
    *childregs = *current_pt_regs();

    /* 设置子进程的返回值为0(在父进程中返回子进程PID) */
    childregs->regs[0] = 0;

    /* 设置子进程的栈指针 */
    if (stack_start)
        childregs->sp = stack_start;

    /* 设置线程本地存储(TLS) */
    if (clone_flags & CLONE_SETTLS)
        childregs->tp_value = tls;
    else
        childregs->tp_value = (unsigned long)task_thread_info(p);

    /* 设置子进程的内核栈 */
    p->thread.cpu_context.sp = (unsigned long)childregs;
    p->thread.cpu_context.pc = (unsigned long)ret_from_fork;

    return 0;
}
```

### ARM64上下文切换

```c
// arch/arm64/kernel/switch_to.S
/*
 * ARM64上下文切换
 * 切换处理器状态: 寄存器、栈、TTBR0(页表基址)
 */
__switch_to(struct task_struct *prev,
        struct task_struct *next)
{
    /* 保存前一个进程的FP/SIMD状态 */
    fpsimd_save_task(prev);

    /* 切换内核栈 */
    __switch_stack(next->thread.cpu_context.sp);

    /* 切换页表基址(ASID) */
    switch_mm_irqs_off(prev->mm, next->mm, next);

    /* 恢复新进程的CPU上下文 */
    cpu_switch_to(prev, next);

    return prev;
}
```

---

## 1.7 常见面试题

### 面试题1：fork()返回值是什么？

**问题**：fork()函数返回值是什么？在父子进程中分别返回什么？

**答案**：
fork()返回两次：
- **父进程中**：返回子进程的PID（大于0）
- **子进程中**：返回0
- **失败时**：返回-1，并设置errno

```c
pid_t pid = fork();
if (pid < 0) {
    // fork失败
    perror("fork");
    exit(1);
} else if (pid == 0) {
    // 子进程
    printf("I am child process, my PID = %d\n", getpid());
} else {
    // 父进程
    printf("I am parent, child PID = %d\n", pid);
}
```

---

### 面试题2：fork()的写时复制(COW)机制

**问题**：fork()后父子进程共享内存，如何实现写时复制？

**答案**：
1. fork()时只复制页表，不复制物理内存
2. 父子进程共享同一物理页，标记为只读
3. 当任一进程写入时，触发页错误(page fault)
4. 内核分配新物理页，复制内容，更新页表
5. 将新页标记为可写

**优点**：延迟复制，只有真正写入时才复制，提高性能。

```c
// COW关键代码 (mm/memory.c)
static vm_fault_t do_wp_page(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;

    /* 检查页面是否可写 */
    if (vma->vm_flags & VM_WRITE) {
        /* 分配新页面 */
        new_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, address);

        /* 复制原页面内容 */
        copy_user_highpage(new_page, old_page, vma, address);

        /* 替换页表项 */
        page_add_new_anon_rmap(new_page, vma, address);

        /* 设置新页面为可写 */
        set_page_accessed(new_page);
        set_page_dirty(new_page);
    }

    return VM_FAULT_WRITE;
}
```

---

### 面试题3：fork()、vfork()、clone()的区别

**问题**：fork()、vfork()、clone()三个系统调用的区别是什么？

**答案**：

| 特性 | fork() | vfork() | clone() |
|------|--------|---------|---------|
| 内存 | COW复制 | 共享(直到exec) | 可配置 |
| 父子执行顺序 | 不确定 | 子先于父 | 不确定 |
| 用途 | 通用 | 配合exec使用 | 线程/容器 |
| 标志位 | 无 | CLONE_VFORK | 可自定义 |
| 安全性 | 高 | 低 | 可配置 |

---

### 面试题4：Linux中的线程实现机制

**问题**：Linux中线程是如何实现的？与进程有什么区别？

**答案**：
Linux中**线程是轻量级进程**：
- 线程和进程使用相同的task_struct
- 线程通过共享资源区分于进程
- 共享：内存空间、文件描述符、信号处理
- 独立：栈、寄存器上下文、线程ID

```c
// pthread_create内部实现
int clone_flags = CLONE_VM | CLONE_FS | CLONE_FILES |
                  CLONE_SIGHAND | CLONE_THREAD |
                  CLONE_SYSVSEM;

clone(do_fork,
      child_stack,
      clone_flags,
      NULL,       // parent_tidptr
      NULL,       // child_tidptr
      tls);       // tls
```

---

### 面试题5：ARM64架构下进程创建的特殊之处

**问题**：在ARM64架构下，进程创建有哪些特定实现？

**答案**：

1. **系统调用方式**：使用SVC #0指令，而不是x86的int 0x80

2. **寄存器约定**：
   - x8: 系统调用号
   - x0-x7: 系统调用参数
   - x0: 返回值

3. **栈布局**：ARM64使用sp作为栈指针，进程切换需要保存完整的SP_EL0/EL1

4. **TLS实现**：
   - 使用TPIDR_EL0 (Thread Pointer ID Register)
   - 通过TPIDR_RO_EL0保存线程本地存储

5. **浮点/NEON状态**：
   - 进程切换需要保存/恢复fpsimd_state
   - 使用SP_EL0指向thread_info

---

### 面试题6：fork后子进程继承了父进程的哪些资源？

**问题**：fork()后子进程继承了父进程的哪些资源？

**答案**：

**继承的资源**：
- 虚拟内存布局(通过页表共享)
- 文件描述符表(共享file结构引用计数+1)
- 工作目录、根目录
- 进程组ID、会话ID
- 环境变量
- 资源限制(rlimit)
- 调度策略和优先级
- 打开的文件描述符对应的文件偏移

**独立/复制的资源**：
- PID
- 内存(COW机制)
- 寄存器上下文
- 栈
- 统计信息

---

## 本章小结

本章深入分析了Linux进程创建机制：

1. **进程与线程**：Linux将线程实现为轻量级进程，共享地址空间
2. **task_struct**：核心数据结构，包含进程所有信息
3. **fork()**：通过copy_process()完整复制进程资源
4. **vfork()**：共享内存，现代实现已基于COW
5. **clone()**：最灵活的创建接口，支持自定义资源分享
6. **ARM64实现**：特有的寄存器约定、栈布局、TLS机制

理解进程创建是理解Linux内核的第一步，后续章节将进一步探讨进程调度和管理。

---

*下一页：[第二章：进程调度](./02-process-scheduling.md)*

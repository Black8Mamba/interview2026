# 第三章：Linux ARM64 进程管理

> 本章深入探讨Linux进程管理机制，包括生命周期、状态转换、退出处理和信号机制

## 目录

- [3.1 进程生命周期](#31-进程生命周期)
- [3.2 进程状态转换](#32-进程状态转换)
- [3.3 进程退出与资源回收](#33-进程退出与资源回收)
- [3.4 信号机制](#34-信号机制)
- [3.5 进程组与会话](#35-进程组与会话)
- [3.6 ARM64进程管理特定](#36-arm64进程管理特定)
- [3.7 常见面试题](#37-常见面试题)

---

## 3.1 进程生命周期

### 进程生命周期阶段

```
+----------------------------------------------------------+
|                    进程生命周期                            |
+----------------------------------------------------------+
|                                                          |
|    +--------+                                           |
|    | 创建   |---- fork() / clone()                      |
|    +--------+                                           |
|         |                                                |
|         v                                                |
|    +--------+                                           |
|    | 就绪   |---- 加入就绪队列                           |
|    +--------+                                           |
|         |                                                |
|         v                                                |
|    +--------+                                           |
|    | 运行   |---- 调度器选中                            |
|    +--------+                                           |
|         |                                                |
|    +--------+    +--------+                              |
|    | 阻塞   |----| 睡眠   | 等待I/O/锁/信号              |
|    +--------+    +--------+                              |
|         |            |                                    |
|         v            v                                    |
|    +--------+    +--------+                              |
|    | 停止   |    | 可中断|                              |
|    +--------+    +--------+                              |
|         |            |                                    |
|         +----+----+-+                                    |
|              |      |                                     |
|              v      v                                     |
|         +--------+                                       |
|         | 退出   |---- exit() / _exit()                  |
|         +--------+                                       |
|              |                                            |
|              v                                            |
|         +--------+                                       |
|         | 僵尸   |---- 等待父进程收集退出信息              |
|         +--------+                                       |
|              |                                            |
|              v                                            |
|         +--------+                                       |
|         | 释放   |---- 父进程调用wait()                  |
|         +--------+                                       |
|                                                          |
+----------------------------------------------------------+
```

### 进程创建到退出的完整流程

```c
// 进程创建
pid_t pid = fork();

if (pid == 0) {
    // 子进程
    // 可以选择exec()加载新程序

    // 或者直接退出
    exit(0);
} else if (pid > 0) {
    // 父进程
    int status;
    wait(&status);  // 等待子进程退出
} else {
    // fork失败
    perror("fork");
}
```

---

## 3.2 进程状态转换

### 进程状态定义

```c
// include/linux/sched.h
/*
 * 进程状态
 * 使用volatile确保编译器不会优化掉状态检查
 */
#define TASK_RUNNING            0x0000  // 可运行(就绪或运行中)
#define TASK_INTERRUPTIBLE     0x0001  // 可中断睡眠
#define TASK_UNINTERRUPTIBLE   0x0002  // 不可中断睡眠
#define __TASK_STOPPED         0x0004  // 已停止
#define __TASK_TRACED          0x0008  // 被跟踪

/* 退出状态 */
#define EXIT_DEAD              0x0020  // 即将退出
#define EXIT_ZOMBIE            0x0040  // 僵尸进程
#define EXIT_TRACE             EXIT_ZOMBIE

/* 进程退出组合状态 */
#define EXIT_NORMAL            0x0100
#define EXIT_SIGNALED          0x0200
```

### ARM64进程状态存储

```c
// arch/arm64/include/asm/thread_info.h
struct thread_info {
    unsigned long    flags;       // 存储进程状态标志
    mm_segment_t    addr_limit;
    struct task_struct *task;
    int        preempt_count;
    int        cpu;
};

/*
 * thread_info.flags中存储的状态:
 * TIF_SIGPENDING - 有信号待处理
 * TIF_NEED_RESCHED - 需要重新调度
 * TIF_NOTIFY_RESUME - 需要通知恢复
 * TIF_UPROBE - 上探针
 */
```

### 状态转换代码分析

```c
// kernel/sched/core.c
/*
 * 设置进程状态
 */
static inline void set_task_state(struct task_struct *tsk, unsigned long state)
{
    set_tsk_thread_flag(tsk, TIF_NEED_RESCHED);
}

/*
 * 进程进入睡眠
 */
static long __sched sleep_on_timeout(wait_queue_head_t *q, long timeout)
{
    long timeout;

    do {
        prepare_to_wait(q, &wait, TASK_UNINTERRUPTIBLE);
        timeout = schedule_timeout(timeout);
    } while (timeout);

    return timeout;
}

/*
 * 可中断睡眠
 */
long __sched wait_for_completion_interruptible_timeout(struct completion *done, long timeout)
{
    might_sleep();

    if (!completion_done(done)) {
        DECLARE_WAITQUEUE(wait, current);

        __add_wait_queue_entry(&done->wait, &wait);
        do {
            if (signal_pending(current))
                return timeout ? -ERESTARTSYS : timeout;

            __set_current_state(TASK_INTERRUPTIBLE);
            timeout = schedule_timeout(timeout);
        } while (!completion_done(done) && timeout);

        __remove_wait_queue(&done->wait, &wait);
        if (!timeout)
            completion_done(done) ? -ETIME : timeout;
    }

    return timeout < 0 ? timeout : 0;
}
```

---

## 3.3 进程退出与资源回收

### 进程退出流程

```c
// kernel/exit.c
/*
 * do_exit - 进程退出主函数
 *
 * 主要工作:
 * 1. 清理进程资源
 * 2. 处理退出码
 * 3. 通知父进程
 * 4. 转换为僵尸状态
 */
void __noreturn do_exit(long code)
{
    struct task_struct *tsk = current;
    int group_dead;

    /* 检查是否为内核线程 */
    if (unlikely(tsk->flags & PF_EXITING)) {
        printk(KERN_ALERT
            "Fixing recursive fault but reboot is needed!\n");
        /*
         * We can do this unlocked:
         * - task count is 0, so no races
         * - the task is on the death list, so no races
         * - the task is zapped and exit_signal is set to -1,
         *   so no races
         */
        for (;;) {
            cpu_relax();
        }
    }

    /* 检查是否有ptrace跟踪 */
    if (unlikely(tsk->ptrace)) {
        /* 处理被ptrace跟踪的情况 */
    }

    /* 清理资源 */
    exit_signals(tsk);  // 发送SIGCHLD给父进程
    exit_shm(tsk);
    exit_files(tsk);
    exit_fs(tsk);
    exit_mm(tsk);

    /* 设置退出码 */
    if (code > 128)
        tsk->exit_code = code | 0x80;  // core dump标志
    else
        tsk->exit_code = code;

    /* 从调度器移除 */
    exit_task_namespaces(tsk);
    exit_task_work(tsk);
    exit_to_user_mode_prepare();

    /* 转换为僵尸状态 */
    tsk->exit_state = EXIT_ZOMBIE;

    /* 释放进程 */
    if (tsk->exit_signal == -1 && tsk->pdeath_signal == 0) {
        /* 线程组最后一个成员，检查是否需要通知父进程 */
    }

    /* 进入idle循环，不再返回 */
    for (;;)
        cpu_idle();
}
```

### 僵尸进程与wait/waitpid

```c
// kernel/exit.c
/*
 * wait4 - 等待子进程退出
 */
SYSCALL_DEFINE4(wait4, pid_t, upid, int __user *, stat_addr,
    struct rusage __user *, ru, int, options)
{
    struct wait_opts wo = {
        .wo_type    = PIDTYPE_PID,
        .wo_pid     = upid,
        .wo_stat    = stat_addr,
        .wo_rusage  = ru,
        .wo_options = options,
    };

    return kernel_wait4(&wo);
}

/*
 * 收集子进程退出信息
 */
static int wait_task_zombie(struct wait_opts *wo, struct task_struct *p)
{
    int status;
    pid_t pid = p->pid;
    uid_t uid = from_kuid_munged(current_user_ns(), task_uid(p));

    /* 获取退出码和信号 */
    if (WIFEXITED(status))
        wo->wo_stat = WEXITSTATUS(status);
    else
        wo->wo_stat = WTERMSIG(status) | 0x80;

    /* 释放进程描述符 */
    release_task(p);

    return pid;
}
```

### 孤儿进程与托孤

当父进程先于子进程退出时，子进程成为**孤儿进程**，需要被init进程((pid=1)收养：

```c
// kernel/exit.c
/*
 * 寻找替代父进程
 * 当进程退出时，如果其子进程的父进程是自己，
 * 需要将这些子进程的父进程设置为init或其他进程
 */
static struct task_struct *find_new_reaper(struct task_struct *father,
                     struct task_struct *child_reaper)
{
    struct pid_namespace *ns = child_reaper->nsproxy->pid_ns_for_children;
    struct task_struct *reaper = child_reaper;

    /* 如果线程组中有其他线程，使用它们作为新的父进程 */
    if (father->signal->group_exit_task && thread_group_empty(father)) {
        return father->signal->group_exit_task;
    }

    /* 否则使用init进程 */
    if (unlikely(pid_alive(father))) {
        /* 检查父进程是否存活 */
    }

    return reaper;
}
```

---

## 3.4 信号机制

### 信号基本概念

信号是Linux进程间通信的一种异步通知机制。

```c
// include/uapi/asm-generic/signal.h
/*
 * 标准信号定义
 * 1-31: 标准信号
 * 34-64: 实时信号
 */
#define SIGHUP       1    // 挂起
#define SIGINT       2    // 中断(Ctrl+C)
#define SIGQUIT      3    // 退出(Ctrl+\)
#define SIGILL       4    // 非法指令
#define SIGTRAP      5    // 陷阱
#define SIGABRT      6    // 中止
#define SIGBUS       7    // 总线错误
#define SIGFPE       8    // 浮点异常
#define SIGKILL      9    // 杀死(不能捕获)
#define SIGUSR1     10    // 用户定义信号1
#define SIGSEGV     11    // 段错误
#define SIGUSR2     12    // 用户定义信号2
#define SIGPIPE     13    // 管道破裂
#define SIGALRM     14    // 闹钟
#define SIGTERM     15    // 终止(可捕获)
#define SIGSTKFLT   16    // 栈错误
#define SIGCHLD     17    // 子进程停止/结束
#define SIGCONT     18    // 继续
#define SIGSTOP     19    // 停止(不能捕获)
#define SIGTSTP     20    // 停止(Ctrl+Z)
#define SIGTTIN     21    // 后台读
#define SIGTTOU     22    // 后台写
#define SIGURG      23    // 紧急数据
#define SIGXCPU     24    // CPU限制
#define SIGXFSZ     25    // 文件大小限制
#define SIGVTALRM   26    // 虚拟闹钟
#define SIGPROF     27    // profile时钟
#define SIGWINCH    28    // 窗口大小改变
#define SIGIO       29    // I/O可用
#define SIGPWR      30    // 电源故障
#define SIGSYS      31    // 系统调用错误
/* 32 reserved */
#define SIGRTMIN    32    // 实时信号起始
#define SIGRTMAX    64    // 实时信号结束
```

### 信号处理

```c
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

/* 信号处理函数 */
void sig_handler(int signo) {
    if (signo == SIGINT) {
        printf("Received SIGINT, exiting...\n");
        _exit(0);
    }
}

int main() {
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // 或SA_RESTART自动重启系统调用

    /* 设置SIGINT处理函数 */
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }

    /* 无限循环 */
    while (1) {
        sleep(1);
    }

    return 0;
}
```

### 信号内核实现

```c
// kernel/signal.c
/*
 * 发送信号的核心函数
 */
int send_signal(int sig, struct kernel_siginfo *info,
    struct task_struct *t, enum pid_type type)
{
    struct sigpending *pending;
    struct sigqueue *q;

    /* 检查信号是否被阻塞 */
    if (sigismember(&t->blocked, sig))
        return 0;

    /* 为实时信号分配队列项 */
    if (sig >= SIGRTMIN && is_si_special(info))
        return -ENOMEM;

    /* 创建信号队列项 */
    q = __sigqueue_alloc(sig, t, GFP_ATOMIC, override_rlimit);
    if (q) {
        /* 填充信号信息 */
        copy_siginfo(&q->info, info);
        q->flags = 0;
    } else {
        /* 如果是标准信号，且已有待处理信号，忽略 */
        if (sig < SIGRTMIN && !sigismember(&t->sigpending, sig))
            return 0;
        /* 否则使用共享队列 */
        q = &sigqueue;
    }

    /* 添加到信号待处理队列 */
    pending = (type == PIDTYPE_TGID) ? &t->signal->shared_pending
                      : &t->pending;
    sigaddset(&pending->signal, sig);
    list_add_tail(&q->list, &pending->list);

    /* 唤醒进程处理信号 */
    signal_wake_up(t, sig == SIGKILL);

    return 0;
}
```

### ARM64信号处理

```c
// arch/arm64/kernel/signal.c
/*
 * ARM64信号帧布局
 *
 * +-----------------------+
 * |  siginfo              |  附加信息
 * +-----------------------+
 * |  ucontext            |  用户上下文
 * +-----------------------+
 * |  stack frame          |  栈帧
 * +-----------------------+
 */
struct sigframe {
    struct ucontext uc;
    unsigned long stack[0];
};

/*
 * ARM64信号返回
 * 使用rt_sigreturn系统调用
 */
static long restore_sigframe(struct pt_regs *regs, struct sigframe *frame)
{
    sigset_t set;
    int err;

    /* 恢复信号掩码 */
    err = __copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set));

    /* 恢复寄存器 */
    err |= __copy_from_user(regs, &frame->uc.uc_regspace,
                sizeof(regs->regs));

    /* 恢复TLS */
    if (err == 0)
        write_tls(regs->tp_value);

    return err;
}
```

---

## 3.5 进程组与会话

### 进程组

```c
// include/linux/sched.h
struct task_struct {
    ...
    /* 进程组 */
    struct task_struct *group_leader;   // 进程组组长
    struct list_head sibling;           // 兄弟链表
    struct list_head children;         // 子进程链表
    ...
};

struct signal_struct {
    ...
    /* 进程组ID */
    struct pid *pgrp;
    /* 会话ID */
    struct pid *tty_session;
    /* 进程组组长 */
    struct pid *group_leader;
    ...
};
```

### 会话

```c
// include/linux/cred.h
struct cred {
    ...
    /* 会话ID */
    gid_t   egid;       // 有效GID
    gid_t   sgid;       // 保存GID
    uid_t   euid;       // 有效UID
    uid_t   suid;       // 保存UID
    ...
};
```

### 进程组/会话相关系统调用

```c
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t pid = getpid();      // 获取进程ID
    pid_t pgid = getpgid(0);   // 获取进程组ID
    pid_t sid = getsid(0);     // 获取会话ID

    /* 创建新进程组 */
    pid_t new_pgid = setpgid(0, getpid());

    /* 创建新会话 */
    pid_t new_sid = setsid();

    return 0;
}
```

---

## 3.6 ARM64进程管理特定

### ARM64进程状态寄存器

ARM64使用PSTATE(Processor State)存储进程状态：

```asm
# ARM64 PSTATE位
# NZCV: 条件标志
# D: Debug mask
# A: SError mask
# I: IRQ mask
# F: FIQ mask
# M: 执行状态(0=EL0, 1=EL1, 2=EL2, 3=EL3)
```

### ARM64系统调用返回

```c
// arch/arm64/kernel/entry.S
/*
 * 系统调用返回
 * x0: 返回值
 * sp: 用户栈指针
 * pc: 返回地址
 * pstate: 恢复后的状态
 */
ret_to_user:
    disable_irq
    ldr     x1, [tsk, #TI_FLAGS]
    and     x2, x1, #_TIF_WORK_SYSCALL_EXIT
    cbnz    x2, ret_to_user_work

ret_to_user_no_work:
    restore_syscall_regs
    ldr     x1, [tsk, #TI_FLAGS]
    mov     x0, sp
    bl      arm64_ret_to_user
    b       ret_to_user
```

### ARM64上下文保存

```c
// arch/arm64/kernel/entry.S
/*
 * 保存中断/系统调用的寄存器
 * 进入内核时保存完整寄存器上下文
 */
.macro kernel_entry, el, regs_save=0
    /*
     * 保存通用寄存器
     * x0-x30, sp, pc, pstate
     */
    stp x0, x1, [sp, #16 * 0]
    stp x2, x3, [sp, #16 * 1]
    ...
    stp x29, x30, [sp, #16 * 15]

    /* 保存栈指针(用户) */
    mov x0, sp
    add x0, x0, #(PT_REGS_SIZE)
    str x0, [sp, #S_SP]

    /* 保存进程状态 */
    mrs x0, nzcv
    mrs x1, daif
    orr x0, x0, x1, lsl #32
    str x0, [sp, #S_PSTATE]
.endm
```

---

## 3.7 常见面试题

### 面试题1：僵尸进程和孤儿进程的区别

**问题**：什么是僵尸进程？什么是孤儿进程？它们有什么区别？

**答案**：

**僵尸进程(Zombie)**：
- 进程退出后，父进程尚未调用wait()收集退出信息
- 此时进程称为僵尸进程，其task_struct仍保留在内存中
- 僵尸进程不占用任何资源(PID除外)
- 如果父进程不调用wait()，僵尸进程会一直存在，占用PID

**孤儿进程(Orphan)**：
- 父进程先于子进程退出
- 子进程的父进程变为init进程(pid=1)
- init会自动调用wait()收集子进程的退出信息
- 孤儿进程不会占用系统资源

**处理方法**：
- 僵尸进程：杀死父进程或让父进程调用wait()
- 孤儿进程：无需处理，由init收养

---

### 面试题2：进程状态转换

**问题**：描述Linux中进程的主要状态及其转换过程。

**答案**：

| 状态 | 说明 | 转换条件 |
|------|------|----------|
| RUNNING | 正在运行 | 调度器选中 |
| READY | 就绪 | 加入就绪队列 |
| SLEEPING | 睡眠 | 等待事件 |
| STOPPED | 停止 | 收到SIGSTOP |
| ZOMBIE | 僵尸 | 退出但父未wait |

**转换图**：
```
RUNNING → READY : 时间片用完
READY → RUNNING : 调度器选中
RUNNING → SLEEPING : 等待I/O/锁/信号
SLEEPING → READY : 事件发生
RUNNING → STOPPED : 收到SIGSTOP
STOPPED → READY : 收到SIGCONT
SLEEPING → ZOMBIE : 调用exit()
ZOMBIE → DEAD : 父进程调用wait()
```

---

### 面试题3：信号的处理流程

**问题**：Linux中信号的处理流程是什么？

**答案**：

1. **发送信号**：通过kill()、raise()、sigqueue()等发送信号
2. **添加信号队列**：内核将信号添加到进程的pending队列
3. **唤醒进程**：设置TIF_SIGPENDING标志，唤醒进程
4. **处理信号**：
   - 从系统调用/中断返回时检查信号
   - 调用do_signal()处理
   - 根据信号掩码判断是否忽略
   - 调用用户注册的处理函数
5. **返回**：处理完成后返回用户空间继续执行

```c
// 信号处理流程
// 1. 发送信号
kill(pid, SIGUSR1);

// 2. 内核处理
// kernel/signal.c
send_signal(SIGUSR1, info, task, PIDTYPE_PID);

// 3. 进程被唤醒处理
// 进入信号处理函数
asmlinkage void do_signal(struct pt_regs *regs)
{
    struct k_sigaction ka;
    sigset_t *set = sigmask_to_save();

    if (signal_pending(current)) {
        siggetmask(set);
        handle_signal(&ka, regs);
    }
}
```

---

### 面试题4：wait()和waitpid()的区别

**问题**：wait()和waitpid()有什么区别？

**答案**：

| 特性 | wait() | waitpid() |
|------|--------|-----------|
| 参数 | 无参数 | 可指定PID |
| 阻塞 | 阻塞等待任意子进程 | 可指定WNOHANG非阻塞 |
| 状态 | 阻塞 | 可选阻塞/非阻塞 |
| PID | 等待任意子进程 | 等待指定PID |

```c
// wait - 阻塞等待任意子进程
int status;
wait(&status);

// waitpid - 可选阻塞/非阻塞
waitpid(-1, &status, 0);      // 阻塞等待任意子进程
waitpid(pid, &status, 0);     // 阻塞等待指定PID
waitpid(pid, &status, WNOHANG);  // 非阻塞
waitpid(-1, &status, WNOHANG);  // 非阻塞等待任意子进程
```

---

### 面试题5：进程退出的方式

**问题**：Linux中进程退出的方式有哪些？有什么区别？

**答案**：

1. **exit() - 标准C库函数**
   - 会调用atexit()注册的清理函数
   - 刷新标准I/O缓冲区
   - 发送SIGCHLD信号给父进程

2. **_exit() - 系统调用**
   - 直接进入内核
   - 不刷新I/O缓冲区
   - 立即终止进程

3. **return - main函数返回**
   - 等同于调用exit()

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void cleanup() {
    printf("cleanup called\n");
}

int main() {
    atexit(cleanup);
    printf("Hello");

    // return 0; 等同于 exit(0)
    // _exit(0) 不会打印cleanup，也不会刷新"Hello"
    return 0;
}
```

---

### 面试题6：SIGKILL和SIGSTOP为什么不能捕获

**问题**：为什么SIGKILL和SIGSTOP信号不能被捕获或忽略？

**答案**：

这是内核设计的安全考虑：

1. **SIGKILL**：
   - 用于强制终止进程
   - 如果进程可以忽略或捕获，就无法保证杀死进程
   - 系统需要一种可靠的方式终止失控进程

2. **SIGSTOP**：
   - 用于停止进程(类似于Ctrl+Z)
   - 如果可以忽略，作业控制将无法工作
   - 需要保证停止信号一定生效

**实现**：
```c
// kernel/signal.c
int sig_task_ignored(struct task_struct *t, int sig, bool force)
{
    /* SIGKILL和SIGSTOP总是被处理 */
    if (sig == SIGKILL || sig == SIGSTOP)
        return force;

    /* 其他信号可能被忽略 */
}
```

---

### 面试题7：ARM64进程管理的特点

**问题**：ARM64架构下进程管理有哪些特定实现？

**答案**：

1. **信号处理**：
   - 使用rt_sigreturn系统调用返回
   - 信号帧保存在用户栈上

2. **寄存器保存**：
   - 进入内核时保存所有通用寄存器
   - PSTATE保存进程状态

3. **TLS实现**：
   - 使用TPIDR_EL0寄存器
   - 每个线程独立

4. **系统调用约定**：
   - x8: 系统调用号
   - x0-x7: 参数
   - x0: 返回值

5. **上下文切换**：
   - 保存/恢复FP/SIMD寄存器
   - 通过TPIDR_EL0切换TLS

---

## 本章小结

本章深入分析了Linux进程管理机制：

1. **生命周期**：从创建到退出的完整流程
2. **状态转换**：RUNNING/READY/SLEEPING/ZOMBIE等状态
3. **退出处理**：exit()、_exit()、wait()资源回收
4. **信号机制**：信号发送、处理、阻塞、掩码
5. **进程组会话**：进程关系管理
6. **ARM64特定**：PSTATE、信号帧、TLS

进程管理是理解Linux内核行为的关键，后续将探讨调试技术。

---

*上一页：[第二章：进程调度](./02-process-scheduling.md)*
*下一页：[第四章：进程调试](./04-process-debugging.md)*

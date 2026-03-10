# Futex 原理

## 概述

Futex（Fast Userspace Mutex）是 Linux 内核为用户态程序提供的高性能同步机制。它是 POSIX 线程 mutex、条件变量等同步原语的底层实现。

Futex 的设计目标是：**在常见情况下（无竞争）避免进入内核，只有在真正需要等待时才进入内核**。

## 工作原理

### 两种状态

```
Futex 状态：
- 0: 锁未被持有
- 1: 锁被持有（无等待者）
- 2+: 锁被持有，且有等待者
```

### 获取锁流程

```
用户态（尝试 CAS）:
  if (CAS(&lock, 0, 1) == 0) {
      // 成功获取锁 - 快速路径
      return;
  }

  // CAS 失败，锁已被持有

用户态（自旋等待）:
  while (CAS(&lock, 1, 2) != 1) {
      // 尝试将状态从 1 改为 2（有等待者）
      cpu_relax();
  }

内核态（阻塞）:
  // 进入内核，将自己加入等待队列
  futex(lock, FUTEX_WAIT, 2, NULL, NULL, 0);
```

### 释放锁流程

```
用户态:
  if (CAS(&lock, 1, 0) == 1) {
      // 没有等待者，直接释放 - 快速路径
      return;
  }

  // 有等待者，需要唤醒
  CAS(&lock, 2, 1);  // 标记有等待者

内核态:
  futex(lock, FUTEX_WAKE, 1, NULL, NULL, 0);
  // 唤醒一个等待者
```

## 系统调用

```c
#include <linux/futex.h>
#include <sys/syscall.h>

long futex(int *uaddr, int futex_op, int val,
           const struct timespec *timeout, int *uaddr2, int val3);

/* 操作类型 */
#define FUTEX_WAIT         0  /* 等待：val == *uaddr 时睡眠 */
#define FUTEX_WAKE         1  /* 唤醒：唤醒 val 个等待者 */
#define FUTEX_FD           2  /* 文件描述符（已废弃） */
#define FUTEX_REQUEUE      3  /* 重新排队 */
#define FUTEX_CMP_REQUEUE  4  /* 比较并重新排队 */
#define FUTEX_WAKE_OP      5  /* 唤醒并操作 */
```

## 使用示例

### 用户态 mutex 实现

```c
typedef struct {
    int lock;
} mutex_t;

void mutex_init(mutex_t *m)
{
    m->lock = 0;
}

void mutex_lock(mutex_t *m)
{
    /* 快速路径：尝试原子获取 */
    if (__sync_bool_compare_and_swap(&m->lock, 0, 1))
        return;

    /* 慢速路径：自旋 + 内核等待 */
    while (__sync_val_compare_and_swap(&m->lock, 1, 2) != 1) {
        futex(&m->lock, FUTEX_WAIT, 2, NULL, NULL, 0);
    }
}

void mutex_unlock(mutex_t *m)
{
    if (__sync_fetch_and_sub(&m->lock, 1) != 1) {
        /* 有等待者，唤醒一个 */
        m->lock = 0;
        futex(&m->lock, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}
```

### 内核中的 Futex

```c
/* 内核使用 futex 系统调用 */
SYSCALL_DEFINE6(futex, int __user *, uaddr, int, op, int, val,
                struct timespec __user *, utime, int __user *, uaddr2,
                int, val3)
{
    /* 内核实现 */
}
```

## 典型应用

### pthread_mutex

```c
/* pthread_mutex 的底层实现就是 futex */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_lock(&mutex);   // 内部使用 futex
pthread_mutex_unlock(&mutex); // 内部使用 futex
```

### POSIX 信号量

```c
/* POSIX 信号量也使用 futex */
sem_t sem;
sem_init(&sem, 0, 1);
sem_wait(&sem);  // futex(FUTEX_WAIT)
sem_post(&sem);  // futex(FUTEX_WAKE)
```

## 性能优势

### 快速路径 vs 慢速路径

| 场景 | 操作 | 开销 |
|------|------|------|
| 无竞争 | CAS 用户态 | ~10 ns |
| 短期自旋 | CAS + 自旋 | ~100 ns - 1 us |
| 长期竞争 | futex 系统调用 | ~1 us - 10 ms |

### 为什么 Futex 快

1. **避免不必要的系统调用**：无竞争时完全在用户态
2. **减少内核参与**：只在真正需要等待时进入内核
3. **高效的唤醒**：内核直接操作等待队列

## 与普通系统调用对比

| 特性 | Futex | 普通 mutex |
|------|-------|------------|
| 无竞争 | 用户态 CAS | 系统调用 |
| 竞争 | futex 系统调用 | 系统调用 |
| 内存占用 | 低 | 低 |
| 可移植性 | Linux | 跨平台 |

---

*上一页：[Futex 概述](./README.md) | 下一页：[面试题](./interview.md)*

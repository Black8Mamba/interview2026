# 第5章 内存模型

> **📖 基础层** | **⚙️ 原理层** | **🔧 实战层**

---

## 📖 基础层：内存顺序概念

### 为什么需要内存模型

在现代处理器中，为了提高性能，编译器优化和CPU乱序执行可能导致内存操作的顺序与代码顺序不一致。如果没有明确的内存模型定义，多线程程序的正确性将无法保证。

**常见重排现象：**
```c
// 线程A：
flag = 1;        // Store 1
data = 42;       // Store 42

// 线程B：
if (flag == 1) { // Load flag
    print(data); // Load data
}
```

期望：flag=1时data必为42
实际：由于Store Buffer或乱序执行，可能data仍为0

### 内存顺序问题的影响

| 问题 | 描述 | 后果 |
|------|------|------|
| **Store Load重排** | Store后于Load完成 | 读到旧数据 |
| **Store Store重排** | 写操作顺序颠倒 | 状态不一致 |
| **Load Load重排** | 读操作顺序颠倒 | 逻辑错误 |
| **Load Store重排** | 读写顺序颠倒 | 死锁/饥饿 |

> **术语对照**
> - Memory Order: 内存顺序
> - Memory Model: 内存模型
> - Reordering: 重排
> - Happens-Before: 发生前关系

---

## ⚙️ 原理层：Arm内存模型

### Arm弱序模型（Weakly Ordered）

Arm处理器采用**弱序内存模型**，仅保证以下顺序：

1. **程序顺序的依赖**：如果A操作依赖B的结果，则A一定在B之后
2. **释放一致性**：同步操作按预期顺序执行
3. **数据依赖**：LOAD-LOAD/LOAD-STORE满足数据依赖

**不保证**：
- Store-Store顺序（除非使用内存屏障）
- Load-Load顺序（除非有依赖）
- Store-Load顺序（除非使用内存屏障或锁）

### Acquire/Release语义

Acquire和Release是Arm64的内存序修饰符：

**Load-Acquire (LDAR)**
```asm
    LDAR   x0, [x1]    // Load + acquire barrier
    // LDAR之后的指令不会被重排到LDAR之前
```

**Store-Release (STLR)**
```asm
    STR    x0, [x1]    // Store + release barrier
    // STR之前的指令不会被重排到STR之后
```

**语义对比：**
```
┌─────────────────────────────────────────────────────┐
│           Release Store (STLR)                       │
│  之前的指令 ──────────────────────────→ STLR ─→ 后续指令│
│              (不能重排到后面)           (可重排)      │
├─────────────────────────────────────────────────────┤
│           Acquire Load (LDAR)                        │
│  之前的指令 ──────────────→ LDAR ─────────────────→ │
│            (可重排)          (不能重排到前面)         │
└─────────────────────────────────────────────────────┘
```

### LDREX/STREX原子指令

Arm提供专用的原子操作指令对：

```asm
// LDREX: 加载并标记
LDREX   x0, [x1]    // 加载x1地址的值，标记该地址为"独占"

// STREX: 条件存储（仅当独占标记有效时）
STREX   x2, x0, [x1] // x2=0表示成功，x2=1表示失败

// 完整原子操作示例：原子加法
    // x0 = address, x1 = value to add
retry:
    LDREX  x2, [x0]      // 读取旧值
    ADD    x3, x2, x1     // 计算新值
    STREX  w4, x3, [x0]   // 尝试写入
    CBNZ   w4, retry      // 失败则重试
```

**LDREX/STREX原理：**
```
┌─────────────────────────────────────────────────────┐
│              LDREX/STREX 机制                         │
│  1. LDREX加载值，标记该地址为"独占访问"               │
│  2. 如果其他核/处理器访问该地址，独占标记失效          │
│  3. STREX检查独占标记：                              │
│     - 有效：写入成功，返回0                          │
│     - 失效：写入失败，返回1                           │
│  4. 失败时通常需要重试                               │
└─────────────────────────────────────────────────────┘
```

### C11/C++11内存序与Arm对应

| C11内存序 | Arm64指令 | 描述 |
|-----------|-----------|------|
| `memory_order_relaxed` | 无 | 仅保证原子性，无顺序 |
| `memory_order_acquire` | LDAR | 之前所有读写不能重排到之前 |
| `memory_order_release` | STLR | 之后所有读写不能重排到之后 |
| `memory_order_acq_rel` | LDAR+STLR | 兼具acquire和release |
| `memory_order_seq_cst` | DMB+LDAR/STLR | 强顺序（默认） |

**代码对应示例：**
```c
#include <stdatomic.h>

// Relaxed: 仅保证原子性
atomic_int counter = ATOMIC_VAR_INIT(0);
atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);

// Acquire: 读操作获取
if (atomic_load_explicit(&flag, memory_order_acquire)) {
    // 这里能看到release之前的所有写入
    process(data);
}

// Release: 写操作释放
atomic_store_explicit(&flag, 1, memory_order_release);
data = new_value;

// Seq_cst: 强顺序（默认）
atomic_store(&flag, 1);  // 编译为 STLR
int value = atomic_load(&flag);  // 编译为 LDAR
```

> **术语对照**
> - Acquire: 获取语义
> - Release: 释放语义
> - LDREX: Load-Exclusive，独占加载
> - STREX: Store-Exclusive，条件存储
> - Sequential Consistency: 顺序一致性

---

## 🔧 实战层：原子操作与无锁数据结构

### 原子计数器实现

```c
#include <stdatomic.h>

// 原子计数器
typedef struct {
    atomic_int count;
} atomic_counter;

void counter_init(atomic_counter *c) {
    atomic_init(&c->count, 0);
}

void counter_inc(atomic_counter *c) {
    atomic_fetch_add_explicit(&c->count, 1, memory_order_relaxed);
}

int counter_get(atomic_counter *c) {
    return atomic_load_explicit(&c->count, memory_order_relaxed);
}

// 使用DMB实现更强的顺序保证
void counter_inc_seq(atomic_counter *c) {
    atomic_fetch_add_explicit(&c->count, 1, memory_order_seq_cst);
}
```

### 自旋锁实现

```c
#include <stdatomic.h>

typedef atomic_flag spinlock = ATOMIC_FLAG_INIT;

void spin_lock(spinlock *lock) {
    // 使用test-and-set实现
    while (atomic_flag_test_and_set_explicit(
               lock, memory_order_acquire)) {
        // 忙等，可添加pause_hint
        __asm__ volatile("yield" ::: "memory");
    }
}

void spin_unlock(spinlock *lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}

// 使用
spinlock lock = ATOMIC_FLAG_INIT;
spin_lock(&lock);
// 临界区
spin_unlock(&lock);
```

### 无锁队列（Treiber Stack）

```c
#include <stdatomic.h>
#include <stdlib.h>

typedef struct node {
    int value;
    _Atomic(struct node *) next;
} node_t;

typedef struct {
    _Atomic(node_t *) head;
} lockfree_stack_t;

void stack_init(lockfree_stack_t *stack) {
    atomic_store(&stack->head, NULL);
}

void stack_push(lockfree_stack_t *stack, int value) {
    node_t *new_node = malloc(sizeof(node_t));
    new_node->value = value;

    node_t *old_head;
    do {
        old_head = atomic_load(&stack->head);
        atomic_store(&new_node->next, old_head);
    } while (!atomic_compare_exchange_weak(
                 &stack->head,
                 &old_head,
                 new_node,
                 memory_order_release,
                 memory_order_relaxed));
}

int stack_pop(lockfree_stack_t *stack, int *value) {
    node_t *old_head;
    node_t *new_head;
    do {
        old_head = atomic_load(&stack->head);
        if (old_head == NULL) {
            return 0;  // 空栈
        }
        new_head = atomic_load(&old_head->next);
    } while (!atomic_compare_exchange_weak(
                 &stack->head,
                 &old_head,
                 new_head,
                 memory_order_acquire,
                 memory_order_relaxed));

    *value = old_head->value;
    free(old_head);
    return 1;
}
```

### 常见陷阱与调试

**1. 丢失的唤醒（Lost Wakeup）**
```c
// 错误：可能丢失信号
lock();
while (condition) {
    unlock();
    // 等待...
    lock();
}
condition = true;
unlock();

// 修正：使用原子变量或条件变量
```

**2. 内存序错误**
```c
// 错误：relaxed导致可见性问题
atomic_store(&ready, 1, memory_order_relaxed);
process(data);  // 可能重排

// 修正：使用release
atomic_store(&ready, 1, memory_order_release);
```

**3. 死锁风险**
```c
// 错误：多锁顺序不一致
// 线程1: lock(A) → lock(B)
// 线程2: lock(B) → lock(A)

// 修正：固定加锁顺序
```

---

## 本章小结

- Arm采用弱序内存模型，允许除依赖外的重排
- Acquire/Release语义提供轻量级同步
- LDREX/STREX实现原子读-改-写操作
- C11/C++11内存序与Arm指令有明确对应
- 无锁数据结构需要仔细处理ABA问题

---

**下一章**：我们将学习 [内存屏障](./chapter6-memory-barriers.md)，了解如何强制保证内存顺序。

# 原子位操作

## 概述

原子位操作提供在不使用锁的情况下对整数的特定位进行原子设置、清除、翻转和测试的能力。这些操作在位图管理、状态标志设置、设备驱动开发等场景中非常有用。

Linux 内核提供了两类位操作接口：
1. **普通位操作**：不保证内存顺序
2. **SMP 位操作**：在 SMP 系统中保证跨 CPU 的可见性

## 常用 API

### 测试位

```c
/* 测试第 nr 位是否为 1（普通版本） */
int test_bit(unsigned long nr, const volatile unsigned long *addr);

/* 测试第 nr 位是否为 1（SMP 安全版本） */
int test_bit(unsigned long nr, const volatile unsigned long *addr);

/* 测试并返回该位的旧值（设置位） */
int test_and_set_bit(unsigned long nr, volatile unsigned long *addr);

/* 测试并返回该位的旧值（清除位） */
int test_and_clear_bit(unsigned long nr, volatile unsigned long *addr);

/* 测试并返回该位的旧值（翻转位） */
int test_and_change_bit(unsigned long nr, volatile unsigned long *addr);
```

### 设置位

```c
/* 将第 nr 位设置为 1 */
void set_bit(unsigned long nr, volatile unsigned long *addr);

/* 将第 nr 位设置为 0 */
void clear_bit(unsigned long nr, volatile unsigned long *addr);

/* 将第 nr 位翻转（0变1，1变0） */
void change_bit(unsigned long nr, volatile unsigned long *addr);
```

### 非原子版本（需配合锁使用）

```c
/* 非原子版本，使用时需要外部同步 */
void __set_bit(unsigned long nr, volatile unsigned long *addr);
void __clear_bit(unsigned long nr, volatile unsigned long *addr);
void __change_bit(unsigned long nr, volatile unsigned long *addr);
```

### 查找位

```c
/* 查找第一个置位位的索引（从 0 开始）
 * 返回值：第一个置位位的索引，如果没有置位位则返回 -1 */
int find_first_bit(const unsigned long *addr, unsigned int size);

/* 查找下一个置位位的索引 */
unsigned long find_next_bit(const unsigned long *addr,
                            unsigned long size,
                            unsigned long offset);

/* 查找第一个清零位的索引 */
int find_first_zero_bit(const unsigned long *addr, unsigned int size);

/* 查找下一个清零位的索引 */
unsigned long find_next_zero_bit(const unsigned long *addr,
                                 unsigned long size,
                                 unsigned long offset);
```

## 使用示例

### 状态标志管理

```c
#define FLAG_READY     0
#define FLAG_BUSY      1
#define FLAG_ERROR     2
#define FLAG_COMPLETE  3

volatile unsigned long device_flags;

void set_device_ready(void)
{
    set_bit(FLAG_READY, &device_flags);
}

bool is_device_ready(void)
{
    return test_bit(FLAG_READY, &device_flags);
}

void set_device_busy(void)
{
    set_bit(FLAG_BUSY, &device_flags);
}

/* 尝试开始操作：确保设备既不忙也没有错误 */
bool try_start_operation(void)
{
    /* 如果 READY 且不 BUSY 且不 ERROR，则开始 */
    if (test_bit(FLAG_READY, &device_flags) &&
        !test_bit(FLAG_BUSY, &device_flags) &&
        !test_bit(FLAG_ERROR, &device_flags)) {
        set_bit(FLAG_BUSY, &device_flags);
        return true;
    }
    return false;
}

void complete_operation(void)
{
    clear_bit(FLAG_BUSY, &device_flags);
    set_bit(FLAG_COMPLETE, &device_flags);
}
```

### 资源分配（位图）

```c
#define MAX_RESOURCES 64
static unsigned long resource_bitmap[BITS_TO_LONGS(MAX_RESOURCES)];

int allocate_resource(void)
{
    int bit;

    /* 原子地查找并分配一个资源 */
    bit = find_first_zero_bit(resource_bitmap, MAX_RESOURCES);
    if (bit >= MAX_RESOURCES)
        return -ENOSPC;

    set_bit(bit, resource_bitmap);
    return bit;
}

void release_resource(int bit)
{
    if (bit >= 0 && bit < MAX_RESOURCES)
        clear_bit(bit, resource_bitmap);
}

bool is_resource_available(int bit)
{
    if (bit >= 0 && bit < MAX_RESOURCES)
        return !test_bit(bit, resource_bitmap);
    return false;
}
```

### 设备中断状态管理

```c
#define IRQ_BIT(n)  (1 << (n))
volatile unsigned long pending_irqs;

void handle_irq(int irq)
{
    /* 检查并清除中断标志 */
    if (test_and_clear_bit(irq, &pending_irqs)) {
        /* 处理中断 */
        process_irq(irq);
    }
}

void mark_irq_pending(int irq)
{
    /* 设置中断挂起标志 */
    set_bit(irq, &pending_irqs);
}
```

## 底层实现 (x86)

### 单 CPU 位操作

在单处理器系统上，位操作只需要禁止中断即可实现原子性：

```c
static inline void __set_bit(long nr, volatile unsigned long *addr)
{
    __asm__ __volatile__(
        "bts %1,%0"
        : "+m" (*addr)
        : "Ir" (nr)
        : "memory"
    );
}
```

### 多 CPU 位操作

在多处理器系统上，需要使用 lock 前缀确保原子性：

```c
static inline void set_bit(long nr, volatile unsigned long *addr)
{
    __asm__ __volatile__(
        "lock; bts %1,%0"
        : "+m" (*addr)
        : "Ir" (nr)
        : "memory"
    );
}
```

### 常用指令

| 指令 | 描述 |
|------|------|
| BTS | Bit Test and Set - 测试并设置位 |
| BTR | Bit Test and Reset - 测试并清除位 |
| BTC | Bit Test and Complement - 测试并翻转位 |

### BTS 指令示例

```asm
; set_bit(nr, addr) 在 x86 上的实现
lock; bts %nr, (addr)

; 说明：
; lock   - 锁定总线，确保原子性
; bts    - 测试并设置位（将指定位设为1，返回旧值）
```

## 注意事项

1. **位索引从 0 开始**：第 0 位是最低有效位
2. **位操作只保证单比特原子性**：多比特操作需要锁保护
3. **内存顺序**：test_and_set_bit 等操作使用 lock 前缀，提供了完整的内存顺序保证
4. **性能**：位操作比锁轻量，但在高并发下仍可能成为瓶颈
5. **跨平台**：位操作接口是平台无关的，内核会自动选择最优实现

## 与 atomic_t 的对比

| 特性 | atomic_t | 位操作 |
|------|----------|--------|
| 操作粒度 | 整个整数 | 单个比特 |
| 典型用途 | 计数器、引用计数 | 标志位、位图 |
| API 复杂度 | 较多 | 较少 |
| 性能 | 较好 | 最好 |

## 扩展：非原子变体

内核还提供了下划线开头的非原子版本，它们是位操作的基础实现：

```c
/* 这些函数不是原子的，使用时必须配合锁 */
void __set_bit(int nr, volatile unsigned long *addr);
void __clear_bit(int nr, volatile unsigned long *addr);
void __change_bit(int nr, volatile unsigned long *addr);
int __test_and_set_bit(int nr, volatile unsigned long *addr);
int __test_and_clear_bit(int nr, volatile unsigned long *addr);
int __test_and_change_bit(int nr, volatile unsigned long *addr);
```

这些变体通常在内核内部使用，当调用者已经持有锁时可以避免不必要的原子操作开销。

---

*上一页：[atomic_t](./atomic_t.md) | 下一页：[面试题](./interview.md)*

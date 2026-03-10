# 编译乱序屏障

## 概述

除了 CPU 硬件导致的内存访问重排外，**编译器**也会为了优化目的重排代码中的内存访问顺序。编译乱序屏障用于阻止编译器优化导致的指令重排，但不影响 CPU 硬件的执行顺序。

## 编译器优化导致的乱序

现代编译器（GCC、Clang）会进行多种优化：

### 1. 指令重排

```c
int a = 1;
int b = 2;

/* 编译器可能优化为：先读内存，再写回 */
/* 如果 a 和 b 都在内存中，可能重排以提高缓存效率 */
```

### 2. 循环优化

```c
/* 编译器可能将多次内存访问合并 */
for (int i = 0; i < 100; i++) {
    arr[i] = i;  /* 编译器可能使用向量化优化 */
}
```

### 3. 死代码消除

```c
int x = get_value();
/* 如果编译器判断 x 从未被使用，会删除这行代码 */
```

## barrier() 宏

`barrier()` 是最基础的编译屏障：

```c
#define barrier() __asm__ __volatile__("" ::: "memory")
```

### 作用

1. **阻止编译器重排**：告诉编译器不要重排 barrier() 周围的内存访问
2. **防止寄存器缓存**：强制编译器重新读取内存中的变量值

### 使用示例

```c
int flag = 0;
int data = 0;

void producer(void)
{
    data = 42;        /* 写入数据 */
    barrier();        /* 阻止编译器重排 */
    flag = 1;         /* 设置标志 */
}

void consumer(void)
{
    while (flag == 0) /* 等待标志 */
        barrier();    /* 防止编译器将 flag 读放入寄存器缓存 */
    /* 此时 data 一定是 42 */
    print(data);
}
```

### barrier() 的特性

| 特性 | 说明 |
|------|------|
| 性能开销 | 极低（只有一个编译器 hint） |
| CPU 影响 | 不影响 CPU 重排 |
| 寄存器 | 强制刷新寄存器缓存 |
| 内联汇编 | 使用空的内存 clobber |

## READ_ONCE() 和 WRITE_ONCE()

这两个宏提供了更精细的控制：

```c
#define READ_ONCE(x) \
    ({ typeof(x) __x = *(volatile typeof(x) *)&(x); __x; })

#define WRITE_ONCE(x, val) \
    (*(volatile typeof(x) *)&(x) = (val))
```

### 作用

1. **READ_ONCE()**：强制从内存读取，不使用缓存值
2. **WRITE_ONCE()**：强制写入内存，不被优化掉

### 与 barrier() 的区别

| 特性 | barrier() | READ_ONCE()/WRITE_ONCE() |
|------|-----------|-------------------------|
| 读内存 | 无 | 强制读取 |
| 写内存 | 无 | 强制写入 |
| 阻止重排 | 是 | 部分（读写各自） |
| 数据类型 | 不关心 | 关心（类型安全） |

### 使用示例

#### 示例 1：防止寄存器缓存

```c
volatile int flag = 0;

/* 没有 READ_ONCE - flag 可能被缓存 */
while (flag == 0)
    ;  /* 无限循环，编译器可能认为 flag 永远为 0 */

/* 使用 READ_ONCE - 强制每次重新读取 */
while (READ_ONCE(flag) == 0)
    cpu_relax();
```

#### 示例 2：单写者多读者

```c
/* 只有一个写入者 */
void writer(int value)
{
    WRITE_ONCE(data, value);  /* 强制写入 */
    WRITE_ONCE(ready, 1);     /* 强制设置标志 */
}

/* 多个读者 */
int reader(void)
{
    if (READ_ONCE(ready)) {   /* 强制读取 */
        /* data 一定已经写入 */
        return READ_ONCE(data);
    }
    return 0;
}
```

#### 示例 3：配合内存屏障

```c
void process(void)
{
    /* 确保在获取锁之前所有写操作完成 */
    smp_mb();

    /* 写入操作 */
    WRITE_ONCE(ptr, new_node);

    /* 确保 ptr 写入完成后再释放锁 */
    smp_mb();
    spin_unlock(&lock);
}
```

## volatile 的局限

### volatile 的作用

1. 防止编译器优化掉变量
2. 强制每次访问都从内存读写

### volatile 不能替代 barrier

```c
/* volatile 不足以保证顺序 */
volatile int flag = 0;
volatile int data = 0;

void producer(void)
{
    data = 42;   /* volatile 只防止这行不被优化掉 */
    flag = 1;    /* 但不能阻止 flag 和 data 的重排 */
}

void consumer(void)
{
    while (flag == 0)
        ;        /* 可能读到 data 的旧值 */
    print(data); /* 未定义行为！ */
}
```

### 正确做法

```c
volatile int flag = 0;
int data = 0;

void producer(void)
{
    data = 42;
    barrier();        /* 阻止编译器重排 */
    flag = 1;         /* flag 现在一定是最后写入 */
}

void consumer(void)
{
    while (READ_ONCE(flag) == 0)
        ;
    /* 现在 data 一定是 42 */
    print(data);
}
```

## smp_* 屏障的编译层面

SMP 内存屏障同时也包含编译屏障：

```c
/* smp_mb() 在 x86 上的实现 */
#define smp_mb() __asm__ __volatile__("lock; addl $0,0(%esp)" ::: "memory")

/* "memory" clobber 告诉编译器：
 * 1. 内存被修改
 * 2. 重新读取所有在 barrier 之前加载的内存变量
 */
```

## 实际使用场景

### 1. 自旋等待

```c
while (!READ_ONCE(done)) {
    cpu_relax();
    /* 防止编译器将 done 放入寄存器 */
}
```

### 2. 中断处理

```c
irqreturn_t handler(int irq, void *dev_id)
{
    /* 读取设备寄存器 */
    status = readl(STATUS_REG);

    /* 确保状态读取在处理之前完成 */
    barrier();

    /* 处理逻辑 */
    if (status & ERROR_FLAG)
        handle_error();
}
```

### 3. DMA 缓冲区

```c
void setup_dma(struct dma_desc *desc, void *buf)
{
    desc->buffer = buf;
    desc->size = BUFFER_SIZE;

    /* 确保描述符写入完成后再启动 DMA */
    wmb();

    /* 启动 DMA */
    start_dma(desc->channel);
}
```

## 注意事项

1. **barrier() 不是免费的**：它会阻止编译器优化，影响性能
2. **过度使用的风险**：滥用会导致代码变慢
3. **只在必要时使用**：只在需要确保特定访问顺序时使用
4. **配合硬件屏障**：需要同时保证 CPU 顺序时，使用 `smp_mb()` 等

---

*上一页：[硬件内存屏障](./barrier-types.md) | 下一页：[面试题](./interview.md)*

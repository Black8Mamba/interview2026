# 原子变量 (atomic_t)

## 概述

`atomic_t` 是 Linux 内核中用于表示原子整数的类型定义。它封装了一个整数值，并提供了一系列原子操作函数，确保对该整数的操作是原子执行的，不会被中断或其他 CPU 打断。

```c
typedef struct {
    int counter;
} atomic_t;
```

对于 64 位原子操作，内核提供了 `atomic64_t` 类型：

```c
typedef struct {
    long counter;
} atomic64_t;
```

## 常用 API

### 初始化

```c
/* 静态初始化 */
atomic_t counter = ATOMIC_INIT(0);
atomic64_t counter64 = ATOMIC64_INIT(0);

/* 动态初始化 */
atomic_set(&counter, 10);
atomic64_set(&counter64, 100);
```

### 读写操作

```c
/* 读取原子变量的值 */
int val = atomic_read(&counter);
long val64 = atomic64_read(&counter64);

/* 设置原子变量的值 */
atomic_set(&counter, 10);
atomic64_set(&counter64, 100);
```

### 算术运算

```c
/* 加法 */
atomic_add(5, &counter);              /* counter += 5 */
int ret = atomic_add_return(5, &counter);  /* 返回新值 */

/* 减法 */
atomic_sub(3, &counter);               /* counter -= 3 */
int ret = atomic_sub_return(3, &counter); /* 返回新值 */

/* 递增 */
atomic_inc(&counter);                  /* counter++ */
atomic_inc_return(&counter);           /* 返回新值 */

/* 递减 */
atomic_dec(&counter);                  /* counter-- */
atomic_dec_return(&counter);           /* 返回新值 */

/* 条件递增/递减（不为0时递增，不为1时递减） */
atomic_inc_not_zero(&counter);
atomic_dec_not_zero(&counter);
```

### 交换与比较交换

```c
/* 原子交换 - 将值设置为新值，返回旧值 */
int old = atomic_xchg(&counter, new_val);

/* CAS - 如果当前值等于预期值，则设置为新值
 * 返回值：true 表示成功，false 表示失败 */
bool success = atomic_cmpxchg(&counter, old_val, new_val);

/* 64位版本 */
long old64 = atomic64_xchg(&counter64, new_val64);
bool success64 = atomic64_cmpxchg(&counter64, old_val64, new_val64);
```

### 其他操作

```c
/* 加法并返回之前的值（先返回旧值，再加法） */
int old = atomic_fetch_add(5, &counter);

/* 减法并返回之前的值 */
int old = atomic_fetch_sub(3, &counter);

/* 递增并返回之前的值 */
int old = atomic_fetch_inc(&counter);

/* 与运算 */
int old = atomic_fetch_and(mask, &counter);

/* 或运算 */
int old = atomic_fetch_or(mask, &counter);

/* 异或运算 */
int old = atomic_fetch_xor(mask, &counter);
```

## 使用示例

### 引用计数

```c
struct my_object {
    atomic_t refcount;
    void *data;
};

void my_object_get(struct my_object *obj)
{
    atomic_inc(&obj->refcount);
}

void my_object_put(struct my_object *obj)
{
    if (atomic_dec_and_test(&obj->refcount)) {
        kfree(obj->data);
        kfree(obj);
    }
}
```

### 简单计数器

```c
atomic_t request_count = ATOMIC_INIT(0);

void record_request(void)
{
    atomic_inc(&request_count);
}

int get_request_count(void)
{
    return atomic_read(&request_count);
}
```

### 无锁更新

```c
atomic_t stats = ATOMIC_INIT(0);

/* 线程安全的统计更新 */
void update_stats(int value)
{
    atomic_add(value, &stats);
}

/* 尝试更新：如果当前值等于预期值，则更新 */
bool try_update_stats(int expect, int new_val)
{
    return atomic_cmpxchg(&stats, expect, new_val) == expect;
}
```

## 底层实现 (x86)

### 原子操作原理

在 x86 架构上，某些操作本身就是原子的：

1. **单字节、16位、32位读写**：对齐的字节、字、双字的读写是原子的
2. **X86 指令前缀**：使用 `lock` 前缀可以将指令锁定在总线上，确保原子性

### x86 实现示例

```c
/* atomic_add 的 x86 实现 */
static inline void atomic_add(int i, atomic_t *v)
{
    __asm__ __volatile__(
        "lock; addl %1,%0"
        : "+m" (v->counter)
        : "ir" (i)
    );
}

/* atomic_cmpxchg 的 x86 实现 */
static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
    int ret;
    __asm__ __volatile__(
        "lock; cmpxchgl %2,%1"
        : "=a" (ret), "+m" (v->counter)
        : "r" (new), "0" (old)
        : "memory"
    );
    return ret;
}
```

### lock 前缀

`lock` 前缀是 x86 实现原子操作的关键：

- 它会锁定北桥芯片，确保在指令执行期间其他处理器无法访问内存
- 对于某些操作（如 INC），会自动使用隐式的 lock 前缀
- 在现代处理器上，lock 前缀会导致总线流量和延迟

### 64 位原子操作

在 32 位 x86 架构上，64 位原子操作需要使用 CMPXCHG8B 指令：

```c
/* atomic64_read 的 x86 实现 */
static inline long atomic64_read(const atomic64_t *v)
{
    long ret;
    __asm__ __volatile__(
        "movq %%mm0,%0\n\t"
        "movq %1,%%mm0\n\t"
        "pcmpgtq %%mm0,%%mm0\n\t"
        "movq %2,%%mm0\n\t"
        "psubq %%mm0,%%mm0\n\t"
        "movq %1,%%mm0\n\t"
        "lock; cmpxchg8b %0\n\t"
        "movq %%mm0,%0\n\t"
        "emms\n\t"
        : "=r" (ret)
        : "m" (*v), "m" (v->counter)
        : "memory", "cc"
    );
    return ret;
}
```

在 64 位 x86-64 架构上，64 位操作直接支持原子访问。

## 与 atomic64_t 的区别

| 特性 | atomic_t | atomic64_t |
|------|----------|------------|
| 数据大小 | 32 位 | 64 位 |
| 适用场景 | 普通计数器、标志位 | 大数值、指针（64位） |
| 性能 | 更快（直接原子指令） | 稍慢（需要 8 字节对齐） |
| 跨平台 | 广泛支持 | 部分 32 位平台支持有限 |

## 注意事项

1. **对齐要求**：64 位原子操作要求 8 字节对齐
2. **内存顺序**：原子操作本身保证原子性，但不保证内存顺序（需要配合内存屏障）
3. **返回值**：带返回值的原子操作在某些架构上可能使用更慢的指令
4. **64 位指针**：在 32 位系统上存储指针应使用 `atomic64_t` 或 `unsigned long`

---

*上一页：[原子操作概述](./README.md) | 下一页：[位操作](./atomic-bitops.md)*

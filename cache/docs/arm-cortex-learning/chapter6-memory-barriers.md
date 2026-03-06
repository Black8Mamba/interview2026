# 第6章 内存屏障

> **📖 基础层** | **⚙️ 原理层** | **🔧 实战层**

---

## 📖 基础层：内存屏障概念

### 为什么需要内存屏障

如前章所述，Arm处理器允许指令重排以提高性能。但在多线程/多核环境中，某些操作必须按特定顺序执行，否则会导致错误。

**典型问题场景：**
```c
// 线程A（生产者）
data = 42;          // Store 1
flag = 1;           // Store 2

// 线程B（消费者）
if (flag == 1) {   // Load 1
    print(data);   // Load 2 - 可能打印0！
}
```

没有内存屏障时，flag的写入可能在data之前完成，或者CPU/Bus的延迟导致数据不一致。

### 内存屏障的作用

内存屏障（Memory Barrier）是一种指令，用于：
1. 阻止CPU对内存访问进行重排
2. 强制刷新Store Buffer到主存
3. 确保多核间内存操作的可见性顺序

> **术语对照**
> - Memory Barrier: 内存屏障
> - Memory Fence: 内存栅栏（内存屏障的另一种称呼）
> - Store Buffer: 存储缓冲

---

## ⚙️ 原理层：Arm内存屏障指令

### DMB (Data Memory Barrier)

DMB确保在屏障之前的内存访问**全部完成**后才执行屏障之后的内存访问。

```asm
DMB ISHST    // 存储屏障（Store Barrier）
DMB ISHLD    // 加载屏障（Load Barrier）
DMB ISH      // 完整屏障（系统内所有核可见）
```

**作用范围：**
```
DMB ISH (系统内所有核):
    核A:           核B:
    Store A ──────────────→ (屏障)
    ───────── DMB ISH ─────────
    Store B ──────────────→ (等待A完成后)
```

### DSB (Data Synchronization Barrier)

DSB比DMB更强：除了阻止重排，还等待所有内存访问**真正完成**（包括Cache和Store Buffer）。

```asm
DSB ISHST    // 存储同步
DSB ISHLD    // 加载同步
DSB ISH      // 完整同步
```

**DMB vs DSB：**
```
DMB:  "前后的内存访问不重排"
DSB:  "前面的都完成了，后面的才开始"

应用场景：
- DMB: 性能敏感，只需顺序保证
- DSB: 切换上下文、执行协处理器指令、MMU配置
```

### ISB (Instruction Synchronization Barrier)

ISB刷新流水线，重新获取指令缓存内容：

```asm
ISB           // 刷新流水线
```

**典型用途：**
- 修改页表后（需要TLB失效）
- 修改系统寄存器后
- 从低功耗唤醒后

### 三种屏障对比

| 指令 | 功能 | 典型用途 |
|------|------|----------|
| **DMB** | 阻止内存访问重排 | 多线程同步、驱动开发 |
| **DSB** | 等待所有内存访问完成 | 页表修改、协处理器切换 |
| **ISB** | 刷新流水线 | TLB失效、系统寄存器修改 |

### 屏障选项

```asm
; 完整系统（所有核）
DMB ISH
DSB ISH

; 当前核（不保证其他核看到）
DMB SY
DSB SY

; 存储/加载特定类型
DMB ISHST   ; Store Barrier - 只影响Store
DMB ISHLD   ; Load Barrier - 只影响Load

; 设备内存（I/O）
DMB LD      ; 设备读取
DMB ST      ; 设备写入
```

> **术语对照**
> - DMB: Data Memory Barrier，数据内存屏障
> - DSB: Data Synchronization Barrier，数据同步屏障
> - ISB: Instruction Synchronization Barrier，指令同步屏障
> - ISH: Inner Shareable Domain，内部共享域

---

## 🔧 实战层：驱动开发与多核同步

### Linux内核内存屏障

Linux内核提供平台无关的内存屏障接口：

```c
#include <asm/barrier.h>

// 编译时屏障
barrier();  // 阻止编译器重排

// 内存屏障
mb();       // DMB/DSB（根据架构）
rmb();      // 读屏障 (DMB LD)
wmb();      // 写屏障 (DMB ST)

// Arm64特定
dma_mb();   // DMA相关
dma_rmb();  // DMA读
dma_wmb();  // DMA写
```

### 驱动开发示例

**设备寄存器访问：**
```c
// 正确：确保寄存器写入顺序
void write_reg(u32 val) {
    writel(val, REG_ADDR);    // 写寄存器
    wmb();                     // 写屏障，确保完成
}

u32 read_reg(void) {
    rmb();                     // 读屏障
    return readl(REG_ADDR);    // 读寄存器
}

// DMA缓冲区同步
void dma_sync_cpu(void *buf, size_t size) {
    // CPU写DMA缓冲区后，同步给设备
    dma_wmb();
    // 或者使用Linux API
    dma_sync_single_for_device(NULL, dma_addr, size, DMA_TO_DEVICE);
}

void dma_sync_device(void *buf, size_t size) {
    // 设备写DMA缓冲区后，同步给CPU
    dma_rmb();
    // 或者
    dma_sync_single_for_cpu(NULL, dma_addr, size, DMA_FROM_DEVICE);
}
```

**中断处理：**
```c
// 中断处理中的内存顺序
irqreturn_t handler(int irq, void *dev_id) {
    struct mydev *dev = dev_id;

    // 确保中断状态读取在数据读取之前
    rmb();
    u32 status = readl(dev->regs + STATUS);

    if (status & DATA_READY) {
        // 处理数据
        process_data(dev->buffer);
    }

    // 清除中断前确保处理完成
    writel(STATUS_CLEAR, dev->regs + STATUS);
    mb();

    return IRQ_HANDLED;
}
```

### 多核同步示例

**自旋锁实现：**
```c
typedef struct {
    _Atomic int lock;
} spinlock_t;

void spin_lock(spinlock_t *s) {
    while (atomic_exchange_explicit(&s->lock, 1,
                                     memory_order_acquire)) {
        // 等待时让出CPU
        cpu_relax();
    }
}

void spin_unlock(spinlock_t *s) {
    atomic_store_explicit(&s->lock, 0, memory_order_release);
}
```

**双检查锁定（Double-Checked Locking）：**
```c
// 单例模式
static _Atomic struct singleton *instance;

struct singleton *get_instance(void) {
    struct singleton *tmp = atomic_load(&instance);

    if (tmp == NULL) {
        spin_lock(&lock);
        tmp = atomic_load(&instance);  // 第二次检查
        if (tmp == NULL) {
            tmp = create_singleton();
            // 关键：release写入，确保完全构造
            atomic_store_explicit(&instance, tmp,
                                 memory_order_release);
        }
        spin_unlock(&lock);
    }

    return tmp;
}
```

### 常见错误与调试

**1. 缺少屏障导致数据不一致**
```c
// 错误
void *producer(void *arg) {
    data = 42;
    ready = 1;    // 可能在data之前被其他核看到
}

// 正确
void *producer(void *arg) {
    data = 42;
    dmb_st();     // 写屏障
    ready = 1;
}
```

**2. 屏障过度使用**
```c
// 过度：每次访问都加屏障
for (int i = 0; i < N; i++) {
    mb();         // 错误：严重性能影响
    sum += array[i];
}

// 优化：只在必要时使用
for (int i = 0; i < N; i++) {
    sum += array[i];  // 单线程不需要屏障
}
```

**3. 调试方法**
```bash
# 使用Helgrind检测数据竞争
valgrind --tool=helgrind ./program

# 使用ThreadSanitizer
gcc -fsanitize=thread program.c

# 使用Arm DS调试器
# 设置内存访问断点
```

---

## 本章小结

- 内存屏障强制保证内存访问顺序
- DMB阻止重排，DSB等待完成，ISB刷新流水线
- 选择合适的屏障类型以平衡正确性和性能
- Linux内核提供平台抽象的屏障接口
- 多核编程中需注意数据一致性和可见性

---

**下一章**：我们将进入 [综合实战](./chapter7-practice.md)，通过实际案例加深对这些概念的理解。

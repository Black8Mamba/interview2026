# Linux驱动开发面试题汇总

## 1. Linux内核基础

### 1.1 Linux内核架构是怎样的？

**答案：**

**Linux内核整体架构：**

```
┌─────────────────────────────────────────────┐
│           用户空间 (User Space)              │
│  ┌─────────────────────────────────────┐    │
│  │  应用程序 │ 库函数(glibc) │ Shell   │    │
│  └─────────────────────────────────────┘    │
└────────────────┬────────────────────────────┘
                 │ 系统调用
┌────────────────┴────────────────────────────┐
│           内核空间 (Kernel Space)            │
│  ┌────────────────────────────────────────┐ │
│  │         系统调用接口 (SCI)              │ │
│  └────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────┐ │
│  │      进程调度 (Process Scheduler)      │ │
│  │      内存管理 (Memory Manager)         │ │
│  │      虚拟文件系统 (VFS)                │ │
│  │      网络协议栈 (Network Stack)        │ │
│  │      设备驱动 (Device Drivers)         │ │
│  └────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────┐ │
│  │      体系结构相关代码 (Arch)           │ │
│  └────────────────────────────────────────┘ │
└─────────────────────────────────────────────┘
```

**各子系统职责：**

| 子系统 | 职责 |
|--------|------|
| 进程调度 | CPU时间片分配，上下文切换 |
| 内存管理 | 虚拟内存，页面分配，交换 |
| VFS | 统一文件系统接口 |
| 网络协议栈 | TCP/IP协议实现 |
| 设备驱动 | 硬件抽象，控制硬件 |
| IPC | 进程间通信机制 |

---

### 1.2 Linux内核的启动流程？

**答案：**

**ARM64 Linux启动流程：**

```
1. Bootloader阶段:
   - 上电 → Bootloader(U-Boot/BootROM)
   - 初始化时钟、DDR、外设
   - 加载内核镜像到内存
   - 传递设备树(DTB)
   - 跳转kernel

2. 内核启动阶段:
   arch/arm64/kernel/head.S:
   - __enable_mmu(): 开启MMU
   - __create_page_tables(): 创建临时页表
   - start_kernel(): C函数入口

3. start_kernel()流程:
   asmlinkage __visible void __init start_kernel(void)
   {
       setup_arch();           // 架构初始化
       trap_init();            // 异常向量初始化
       mm_init();              // 内存管理初始化
       sched_init();           // 调度器初始化
       init_IRQ();             // 中断初始化
       vfs_caches_init();     // VFS缓存初始化
       rest_init();            // 创建init进程
   }

4. 用户空间启动:
   - 创建kernel_init进程
   - 挂载rootfs
   - 执行init程序(/sbin/init)
   - 启动用户空间服务
   }
```

**关键函数说明：**

```c
// setup_arch() - 架构特定初始化
void __init setup_arch(char **cmdline) {
    // 解析设备树
    unflatten_device_tree();

    // 初始化内存
    paging_init();

    // 启动CPU hotplug
    smp_init_cpus();
}

// rest_init() - 启动init进程
static void __init rest_init(void) {
    // 创建kernel_init线程
    pid = kernel_thread(kernel_init, NULL, CLONE_FS);

    // 创建kthreadd线程
    pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
}
```

---

### 1.3 什么是系统调用？Linux如何实现？

**答案：**

**系统调用定义：**
用户空间程序请求内核服务的接口，是用户空间进入内核的唯一方式。

**系统调用流程：**

```c
/*
用户程序:
fd = open("/dev/myled", O_RDWR);

库函数(glibc):
.globl open
open:
    mov  $SYS_open, %eax   // 系统调用号
    mov  $filename, %ebx
    mov  $flags, %ecx
    int  $0x80             // 触发软中断
    // 或使用syscall指令(新架构)

内核处理:
asmlinkage long sys_open(const char __user *filename, int flags)
{
    // 检查参数
    // 调用VFS层
    // 分配文件描述符
    // 返回fd
}
*/
```

**ARM64系统调用：**

```assembly
// 用户空间
mov x8, #56     // openat系统调用号
mov x0, x21     // pathname
mov x1, #0      // flags
mov x2, #0644   // mode
svc 0           // 触发SVC异常

// 内核处理
ENTRY(handle_arch_exception)
    // 保存现场
    // 调用系统调用处理函数
    // 返回
```

**常用系统调用：**

| 调用 | 号 | 作用 |
|------|-----|------|
| fork | 57 | 创建进程 |
| execve | 59 | 执行程序 |
| read | 63 | 读文件/设备 |
| write | 64 | 写文件/设备 |
| openat | 56 | 打开文件 |
| ioctl | 29 | 设备控制 |
| mmap | 222 | 内存映射 |

---

## 2. 进程管理

### 2.1 进程与线程的区别？

**答案：**

**进程 vs 线程：**

| 特性 | 进程 | 线程 |
|------|------|------|
| 资源拥有 | 独立地址空间 | 共享进程资源 |
| 创建开销 | 大(需复制页表) | 小(共享资源) |
| 切换开销 | 大(切换地址空间) | 小(共享资源) |
| 通信方式 | 复杂(IPC) | 简单(共享内存) |
| 独立性 | 独立 | 依赖进程 |
| 崩溃影响 | 仅自身 | 导致整个进程崩溃 |

**Linux中的实现：**

```c
// 进程创建 - fork()
pid_t pid = fork();
if(pid == 0) {
    // 子进程
    printf("I'm child\r\n");
} else if(pid > 0) {
    // 父进程
    printf("Child pid: %d\r\n", pid);
} else {
    // 错误
}

// 线程创建 - pthread_create()
#include <pthread.h>
void* thread_func(void* arg) {
    printf("Thread running\r\n");
    return NULL;
}

pthread_t thread;
pthread_create(&thread, NULL, thread_func, NULL);
pthread_join(thread, NULL);
```

**进程/线程选择原则：**
- 需要独立资源 → 进程
- 高性能、轻量级 → 线程
- 多核并行 → 多线程
- 崩溃隔离 → 进程

---

### 2.2 进程状态有哪些？

**答案：**

**Linux进程状态：**

```
       ┌──────────┐
       │   TASK_NEW   │ 新创建
       └──────┬──────┘
              │
              ▼
       ┌──────────┐
       │TASK_READY │ 就绪(等待CPU)
       └──────┬──────┘
              │ 被调度
       ┌──────┴──────┐
       │              │
       ▼              ▼
┌──────────┐    ┌──────────┐
│TASK_RUNNING│◄───│   调度   │
│  运行中   │    └──────────┘
└──────┬──────┘
       │ 时间片用完/被抢占
       ▼
┌──────────────┐
│ TASK_INTERRUPTIBLE │ 可中断睡眠(等待事件)
└──────┬───────┘
       │ 事件发生/信号
       ▼
┌──────────────┐
│ TASK_UNINTERRUPTIBLE │ 不可中断睡眠(等待IO)
└──────┬───────┘
       │ IO完成/信号
       ▼
       │
       ▼
┌──────────────┐
│   TASK_STOPPED │ 暂停(调试信号)
└──────────────┘
       │
       ▼
┌──────────────┐
│  TASK_TRACED │ 被跟踪(ptrace)
└──────────────┘

退出:
TASK_DEAD → 等待父进程回收
```

**状态查看：**
```bash
# 查看进程状态
ps aux

# 状态说明
# R: Running/Runnable
# S: Interruptible Sleep
# D: Uninterruptible Sleep(通常IO)
# T: Stopped
# Z: Zombie
# X: Dead

# 查看D状态进程(IO等待)
ps aux | awk '$8 ~ /D/ {print}'
```

---

### 2.3 僵尸进程和孤儿进程是什么？

**答案：**

**僵尸进程(Zombie)：**

```c
// 子进程退出后，父进程未回收其退出状态
// 子进程变为僵尸进程
// 特点:
// - 已退出，但task_struct仍存在
// - 等待父进程读取退出状态
// - 不占用内存，只占用进程描述符

// 危害:
// - 大量僵尸进程会耗尽PID
// - 进程号是有限资源

// 示例:
int main() {
    pid_t pid = fork();
    if(pid > 0) {
        // 父进程不回收，直接退出
        // 子进程变成僵尸
        exit(0);
    } else {
        sleep(10);  // 子进程10秒后退出
        exit(0);
    }
}
```

**孤儿进程(Orphan)：**

```c
// 父进程先于子进程退出
// 子进程被init(进程1)收养
// init会回收子进程退出状态

// 示例:
int main() {
    pid_t pid = fork();
    if(pid > 0) {
        // 父进程立即退出
        exit(0);
    } else {
        // 子进程继续运行，成为孤儿
        sleep(100);
    }
}
```

**处理方法：**
```c
// 1. 父进程调用wait()/waitpid()回收
int status;
pid_t pid = wait(&status);

// 2. 忽略子进程退出
signal(SIGCHLD, SIG_IGN);

// 3. 使用waitpid()非阻塞回收
while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    // 回收
}
```

---

## 3. 内存管理

### 3.1 Linux内存管理机制？

**答案：**

**Linux内存管理层次：**

```
用户空间:
┌────────────────────────────────────────┐
│  应用层 malloc()                        │
│  ↓                                      │
│  glibc malloc (ptmalloc2/tcmalloc)     │
│  ↓                                      │
│  brk()/mmap() 系统调用                  │
└────────────────┬───────────────────────┘
                 │
内核空间:
┌────────────────┴───────────────────────┐
│  内存管理系统调用 (brk, mmap, munmap)    │
│  ↓                                      │
│  页面分配器 (Buddy System)              │
│  ↓                                      │
│  Slab/Slub分配器 (kmalloc)             │
│  ↓                                      │
│  物理内存管理 (页帧管理)                 │
│  ↓                                      │
│  硬件: MMU + 页表                       │
└────────────────────────────────────────┘
```

**Buddy System(伙伴系统)：**
```c
/*
Buddy System特点:
- 按2的幂次分配内存块
- 阶(order): 0=4KB, 1=8KB, 2=16KB, ...
- 只能分配2^n大小的块
- 合并时必须是"伙伴"(同一阶，相邻)

分配流程:
- 请求8KB (order=1)
- order=1有空闲 → 分配
- 无 → 尝试拆分更大的块
*/
```

**Slab/Slub分配器：**
```c
/*
Slab分配器:
- 用于分配小于页的内存对象
- 缓存常用对象(sock, task_struct, dentry等)
- 减少内存碎片

kmalloc实现:
- 基于Slab，64B-1MB
- GFP_KERNEL / GFP_ATOMIC 标志
*/
```

---

### 3.2 虚拟内存与分页机制？

**答案：**

**虚拟内存原理：**

```
虚拟地址 → MMU → 物理地址

每个进程有独立的虚拟地址空间
内核有独立的内核地址空间
```

**ARM64分页机制：**

```c
/*
ARM64支持:
- 4级页表 (4KB页): 39位虚拟地址
- 5级页表 (4KB页): 48位虚拟地址
- 2MB/1GB大页

虚拟地址布局(48位):
┌────────┬────────┬────────┬────────┬────────┐
│  PTE   │  PUD  │  PMD  │  PGD  │  Offset│
│  63:39 │ 38:30 │ 29:21 │ 20:12 │  11:0  │
└────────┴────────┴────────┴────────┴────────┘
*/
```

**页表项标志：**

```c
// PTE (页表项) 标志
#define _PAGE_PRESENT    (1 << 0)   // 页存在
#define _PAGE_RW         (1 << 1)   // 可写
#define _PAGE_USER       (1 << 2)   // 用户态可访问
#define _PAGE_ACCESSED   (1 << 5)   // 已访问
#define _PAGE_DIRTY      (1 << 6)   // 已修改
#define _PAGE_HUGE       (1 << 7)   // 大页
```

**缺页异常处理：**
```c
/*
缺页异常类型:
1. 匿名页缺页 → 分配物理页
2. 文件映射缺页 → 从磁盘读取
3. 写时复制(COW) → 复制页面
4. 越界访问 → 发送SIGSEGV
*/
```

---

### 3.3 页面置换算法？

**答案：**

**常见置换算法：**

| 算法 | 原理 | 优点 | 缺点 |
|------|------|------|------|
| FIFO | 最早进入先置换 | 简单 | 性能差 |
| LRU | 最近最少使用 | 性能好 | 开销大 |
| Clock(NRU) | 最近未使用 | 近似LRU，开销小 | 近似 |
| OPT | 最远未来使用 | 最优 | 无法实现 |

**Linux使用的LRU：**

```c
/*
Linux LRU实现:
- 分为active和inactive两个链表
- 活跃页频繁访问，移到active尾部
- 不活跃页长期未访问，可能被回收

回收策略:
- 内存压力时，从inactive回收
- 优先回收干净页
- 脏页需写回后才能回收
*/
```

---

## 4. 中断与异常

### 4.1 Linux中断处理机制？

**答案：**

**中断分类：**

| 类型 | 特点 | 例子 |
|------|------|------|
| 硬中断 | 硬件产生，异步 | 定时器、UART |
| 软中断 | 内核产生，同步 | 系统调用异常 |
| 异常 | CPU产生，同步 | 缺页、非法指令 |

**Linux中断处理流程：**

```
硬件中断发生
    ↓
CPU执行中断向量
    ↓
进入中断处理(保存上下文)
    ↓
┌────────────────────────────────────┐
│  1. 顶半部(Top Half)              │
│     - 快速处理(读取状态，清中断)   │
│     - 必须原子操作                 │
│     - 耗时工作延后到下半部         │
└────────────────────────────────────┘
    ↓
┌────────────────────────────────────┐
│  2. 底半部(Bottom Half)           │
│     - Softirq / Tasklet / Workqueue│
│     - 可延迟处理                   │
│     - 允许中断嵌套                 │
└────────────────────────────────────┘
    ↓
退出中断处理(恢复上下文)
    ↓
继续执行
```

**底半部机制对比：**

| 机制 | 上下文 | 优先级 | 性能 |
|------|--------|--------|------|
| Softirq | 中断上下文 | 高 | 最快 |
| Tasklet | 中断上下文 | 中 | 快 |
| Workqueue | 进程上下文 | 低 | 慢 |

---

### 4.2 软中断(Softirq)机制？

**答案：**

**Softirq定义：**
Linux中断处理框架的一部分，用于处理可延迟的中断处理工作。

```c
/*
预定义的软中断类型:
enum {
    HI_SOFTIRQ=0,
    TIMER_SOFTIRQ,
    NET_TX_SOFTIRQ,
    NET_RX_SOFTIRQ,
    BLOCK_SOFTIRQ,
    IRQ_POLL_SOFTIRQ,
    TASKLET_SOFTIRQ,
    SCHED_SOFTIRQ,
    HRTIMER_SOFTIRQ,
    RCU_SOFTIRQ,
    NR_SOFTIRQS
};
*/

// 注册softirq
void open_softirq(int nr, softirq_handler_t handler) {
    softirq_vec[nr].action = handler;
}

// 触发softirq
void raise_softirq(unsigned int nr) {
    unsigned long flags;
    local_irq_save(flags);
    softirq_pending(nr) |= (1UL << nr);
    local_irq_restore(flags);
    // 在中断返回时检查并执行
}

// softirq处理
asmlinkage void do_softirq(void) {
    unsigned int pending = local_softirq_pending();

    while(pending) {
        struct softirq_action *h;
        int softirq_nr = __builtin_ctz(pending);

        pending &= ~(1UL << softirq_nr);
        h = &softirq_vec[softirq_nr];
        h->action(h);
    }
}
```

---

### 4.3 Tasklet与Workqueue的区别？

**答案：**

**对比：**

| 特性 | Tasklet | Workqueue |
|------|---------|-----------|
| 上下文 | 中断上下文 | 进程上下文 |
| 调度 | 不可延迟 | 可延迟 |
| 阻塞操作 | 不可 | 可以 |
| 调度CPU | 不能指定 | 可以指定 |
| 并发 | 同一类型不能并发 | 可配置 |
| 开销 | 更小 | 更大 |

**Tasklet使用：**
```c
#include <linux/interrupt.h>

static void my_tasklet_func(unsigned long data) {
    printk("Tasklet running, data=%ld\r\n", data);
}

DECLARE_TASKLET(my_tasklet, my_tasklet_func, 123);

// 中断处理中调度
void interrupt_handler(int irq, void *dev_id) {
    // 顶半部: 必须快速
    // ...

    // 调度底半部
    tasklet_schedule(&my_tasklet);
}
```

**Workqueue使用：**
```c
#include <linux/workqueue.h>

static void my_work_func(struct work_struct *work) {
    printk("Workqueue running\r\n");
}

DECLARE_WORK(my_work, my_work_func);

// 调度工作
schedule_work(&my_work);

// 或使用自定义workqueue
struct workqueue_struct *my_wq = create_workqueue("my_wq");
queue_work(my_wq, &my_work);
flush_workqueue(my_wq);
destroy_workqueue(my_wq);
```

---

## 5. 设备驱动

### 5.1 Linux设备驱动模型？

**答案：**

**设备模型层次：**

```
┌─────────────────────────────────────┐
│           Bus (总线)                 │
│    (PCI, USB, I2C, SPI, Platform)   │
└──────────────┬──────────────────────┘
               │
┌──────────────┴──────────────────────┐
│         Device (设备)                │
└──────────────┬──────────────────────┘
               │
┌──────────────┴──────────────────────┐
│       Driver (驱动)                  │
└─────────────────────────────────────┘
```

**关键数据结构：**

```c
// 设备驱动结构
struct device_driver {
    const char *name;
    struct bus_type *bus;
    struct module *owner;
    const struct of_device_id *of_match_table;
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    // ...
};

// 设备结构
struct device {
    struct device *parent;
    struct bus_type *bus;
    struct device_driver *driver;
    void *platform_data;
    struct device_node *of_node;
    // ...
};

// 总线类型
struct bus_type {
    const char *name;
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    // ...
};
```

---

### 5.2 字符设备驱动如何编写？

**答案：**

**字符设备驱动框架：**

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mydevice"
#define CLASS_NAME  "myclass"

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static int major;

// 打开设备
static int mydev_open(struct inode *inode, struct file *file) {
    printk("Device opened\r\n");
    return 0;
}

// 关闭设备
static int mydev_release(struct inode *inode, struct file *file) {
    printk("Device closed\r\n");
    return 0;
}

// 读设备
static ssize_t mydev_read(struct file *file, char __user *buf,
                          size_t count, loff_t *ppos) {
    char data[] = "Hello from kernel!";
    size_t len = strlen(data);

    if(*ppos >= len) return 0;
    if(count > len - *ppos) count = len - *ppos;

    if(copy_to_user(buf, data + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

// 写设备
static ssize_t mydev_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos) {
    char kbuf[256];

    if(count > 255) count = 255;
    if(copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';
    printk("Received: %s\r\n", kbuf);
    return count;
}

// 设备操作
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = mydev_open,
    .release = mydev_release,
    .read = mydev_read,
    .write = mydev_write,
};

// 模块初始化
static int __init mydev_init(void) {
    // 分配设备号
    alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    major = MAJOR(dev_num);

    // 初始化cdev
    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    // 添加cdev
    cdev_add(&my_cdev, dev_num, 1);

    // 创建设备节点
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);

    printk("Device registered, major=%d\r\n", major);
    return 0;
}

// 模块退出
static void __exit mydev_exit(void) {
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk("Device unregistered\r\n");
}

module_init(mydev_init);
module_exit(mydev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("My character device driver");
```

---

### 5.3 Platform设备驱动模型？

**答案：**

**Platform设备驱动架构：**

```c
/*
Platform设备:
- 不在传统总线上的设备
- 通常是SoC内部外设
- 通过设备树或platform_device定义
*/

// 1. 设备定义(设备树方式)
&soc {
    mydevice: mydevice@40000000 {
        compatible = "vendor,mydevice";
        reg = <0x40000000 0x1000>;
        interrupts = <0 60 IRQ_TYPE_LEVEL_HIGH>;
        clocks = <&clk>;
    };
};

// 2. 驱动实现
static const struct of_device_id mydev_of_match[] = {
    { .compatible = "vendor,mydevice", },
    { }
};
MODULE_DEVICE_TABLE(of, mydev_of_match);

static int mydev_probe(struct platform_device *pdev) {
    struct resource *res;
    void __iomem *base;
    int irq;

    // 获取资源
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base = devm_ioremap_resource(&pdev->dev, res);

    irq = platform_get_irq(pdev, 0);
    devm_request_irq(&pdev->dev, irq, mydev_isr,
                     0, "mydev", NULL);

    // 保存私有数据
    struct mydev_data *data = devm_kzalloc(&pdev->dev,
                                           sizeof(*data), GFP_KERNEL);
    data->base = base;
    platform_set_drvdata(pdev, data);

    return 0;
}

static int mydev_remove(struct platform_device *pdev) {
    // 清理资源
    return 0;
}

static struct platform_driver mydev_driver = {
    .probe = mydev_probe,
    .remove = mydev_remove,
    .driver = {
        .name = "mydevice",
        .of_match_table = mydev_of_match,
    },
};

module_platform_driver(mydev_driver);
```

---

### 5.4 I2C驱动如何编写？

**答案：**

**I2C驱动架构：**

```c
/*
I2C驱动三层:
1. I2C适配器驱动: 控制器驱动(厂家提供)
2. I2C核心: 内核提供
3. I2C设备驱动: 具体设备驱动
*/

// 1. I2C设备驱动
static int myi2c_probe(struct i2c_client *client,
                       const struct i2c_device_id *id) {
    struct my_device *data;

    // 分配私有数据
    data = devm_kzalloc(&client->dev, sizeof(struct my_device),
                        GFP_KERNEL);
    if(!data) return -ENOMEM;

    i2c_set_clientdata(client, data);
    data->client = client;

    // 初始化设备
    // ...

    return 0;
}

static int myi2c_remove(struct i2c_client *client) {
    return 0;
}

static const struct i2c_device_id myi2c_id[] = {
    { "myi2c_device", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, myi2c_id);

static const struct of_device_id myi2c_of_match[] = {
    { .compatible = "vendor,myi2c", },
    { }
};
MODULE_DEVICE_TABLE(of, myi2c_of_match);

static struct i2c_driver myi2c_driver = {
    .probe = myi2c_probe,
    .remove = myi2c_remove,
    .id_table = myi2c_id,
    .driver = {
        .name = "myi2c_driver",
        .of_match_table = myi2c_of_match,
    },
};

module_i2c_driver(myi2c_driver);

// 2. I2C通信API
static int myi2c_read_reg(struct i2c_client *client,
                          u8 reg, u8 *val) {
    int ret;
    u8 buf[2] = { reg };

    // 连续读写
    struct i2c_msg msgs[] = {
        { .addr = client->addr,
          .flags = 0,
          .len = 1,
          .buf = buf },
        { .addr = client->addr,
          .flags = I2C_M_RD,
          .len = 1,
          .buf = val }
    };

    ret = i2c_transfer(client->adapter, msgs, 2);
    return (ret == 2) ? 0 : ret;
}

static int myi2c_write_reg(struct i2c_client *client,
                           u8 reg, u8 val) {
    u8 buf[2] = { reg, val };
    return i2c_master_send(client, buf, 2);
}
```

---

### 5.5 SPI驱动如何编写？

**答案：**

**SPI驱动实现：**

```c
// 1. SPI设备驱动
static int myspi_probe(struct spi_device *spi) {
    struct my_spi *data;

    // 配置SPI模式
    spi->mode = SPI_MODE_0;
    spi->max_speed_hz = 10 * 1000 * 1000;  // 10MHz
    spi->bits_per_word = 8;
    spi_setup(spi);

    // 分配数据
    data = devm_kzalloc(&spi->dev, sizeof(*data), GFP_KERNEL);
    if(!data) return -ENOMEM;

    spi_set_drvdata(spi, data);
    data->spi = spi;

    // 可以使用spi_read/spi_write简化操作
    u8 rx_buf[4];
    u8 tx_buf[4] = { 0x01, 0x02, 0x03, 0x04 };

    spi_write(spi, tx_buf, 4);
    spi_read(spi, rx_buf, 4);

    return 0;
}

static int myspi_remove(struct spi_device *spi) {
    return 0;
}

static const struct spi_device_id myspi_id[] = {
    { "myspi", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, myspi_id);

static struct spi_driver myspi_driver = {
    .probe = myspi_probe,
    .remove = myspi_remove,
    .id_table = myspi_id,
    .driver = {
        .name = "myspi",
    },
};

module_spi_driver(myspi_driver);

// 2. 使用SPI消息传输
static int myspi_transfer(struct spi_device *spi) {
    u8 tx_buf[] = { CMD_READ, addr };
    u8 rx_buf[4];

    struct spi_message msg;
    struct spi_transfer xfer[] = {
        {
            .tx_buf = tx_buf,
            .len = 2,
            .cs_change = 0,
        },
        {
            .rx_buf = rx_buf,
            .len = 4,
            .cs_change = 1,  // 传输后改变CS
        },
    };

    spi_message_init_with_transfers(&msg, xfer, 2);
    return spi_sync(spi, &msg);
}
```

---

## 6. 网络驱动

### 6.1 Linux网络驱动架构？

**答案：**

**网络驱动层次：**

```
┌─────────────────────────────────────────┐
│         应用层 (Socket)                  │
└────────────────┬────────────────────────┘
                 │
┌────────────────┴────────────────────────┐
│         套接字层 (BSD Socket)           │
└────────────────┬────────────────────────┘
                 │
┌────────────────┴────────────────────────┐
│         协议层 (TCP/UDP/IP)              │
└────────────────┬────────────────────────┘
                 │
┌────────────────┴────────────────────────┐
│         设备无关层 (NDI)                 │
│      (netdevice, sk_buff)               │
└────────────────┬────────────────────────┘
                 │
┌────────────────┴────────────────────────┐
│         设备驱动层 (网卡驱动)             │
└─────────────────────────────────────────┘
```

**网络设备结构：**

```c
struct net_device {
    char name[IFNAMSIZ];           // 设备名
    unsigned long base_addr;       // I/O地址
    unsigned int irq;              // 中断号

    // 设备操作
    int (*open)(struct net_device *);
    int (*stop)(struct net_device *);
    netdev_tx_t (*start_xmit)(struct sk_buff *, struct net_device *);
    irqreturn_t (*interrupt)(int, void *);

    // 统计
    struct net_device_stats stats;

    // 特性
    unsigned long features;
    unsigned int flags;
    // ...
};

// 发送数据包
static netdev_tx_t mynet_xmit(struct sk_buff *skb,
                              struct net_device *dev) {
    // 获取数据
    char *data = skb->data;
    int len = skb->len;

    // 通过DMA/寄存器发送
    // ...

    // 更新统计
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += len;

    // 释放skb
    dev_kfree_skb(skb);

    return NETDEV_TX_OK;
}
```

**NAPI机制：**

```c
/*
NAPI: New API
- 轮询+中断结合
- 高负载时轮询，避免中断风暴
*/

static int mynet_poll(struct napi_struct *napi, int budget) {
    struct net_device *dev = napi_to_dev(napi);
    int work_done = 0;

    // 接收数据包
    while(work_done < budget) {
        struct sk_buff *skb = mynet_rx();
        if(!skb) break;

        netif_receive_skb(skb);
        work_done++;
    }

    // 处理完则关闭轮询
    if(work_done < budget) {
        napi_complete(napi);
        enable_irq(dev->irq);
    }

    return work_done;
}

static irqreturn_t mynet_interrupt(int irq, void *dev_id) {
    struct net_device *dev = dev_id;

    // 关闭中断，开启NAPI
    disable_irq_nosync(irq);
    napi_schedule(&dev->napi);

    return IRQ_HANDLED;
}
```

---

## 7. 调试技巧

### 7.1 如何使用printk调试？

**答案：**

**printk日志级别：**

```c
/*
日志级别:
KERN_EMERG   - 紧急情况，系统崩溃
KERN_ALERT   - 需要立即处理
KERN_CRIT    - 严重错误
KERN_ERR     - 错误
KERN_WARNING - 警告
KERN_NOTICE  - 正常但重要
KERN_INFO    - 信息
KERN_DEBUG   - 调试信息
*/

// 使用示例
printk(KERN_ERR "Error: %d\r\n", error_code);
printk(KERN_INFO "Device initialized\r\n");
printk(KERN_DEBUG "Value = %d\r\n", value);

// 简写形式
printk(KERN_ERR "Error: ...\r\n");
printk(KERN_INFO "Info: ...\r\n");

// 输出到控制台
// 注意: 低于console_loglevel的消息不会显示
```

**日志级别配置：**

```bash
# 查看当前控制台日志级别
cat /proc/sys/kernel/printk
# 4  <n>  <t>  <o>
# ^  当前级别

# 设置级别(0-7)
echo 8 > /proc/sys/kernel/printk  # 显示所有
echo 3 > /proc/sys/kernel/printk  // 只显示>=3

# 动态调整
dmesg -n 8   # 设置
dmesg        # 查看
```

---

### 7.2 ftrace如何使用？

**答案：**

**ftrace功能：**
- 函数调用追踪
- 函数耗时分析
- 中断/调度分析
- 延迟分析

**基本使用：**

```bash
# 查看可用追踪器
cat /sys/kernel/debug/tracing/available_tracers

# 设置追踪器
echo function > /sys/kernel/debug/tracing/current_tracer

# 启用函数追踪
echo 1 > /sys/kernel/debug/tracing/options/func_stack_trace

# 设置过滤器(只追踪指定函数)
echo my_function > /sys/kernel/debug/tracing/set_ftrace_filter
echo '*uart*' > /sys/kernel/debug/tracing/set_ftrace_filter

# 开始追踪
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 触发事件(如发送网络包)
# ...

# 停止追踪
echo 0 > /sys/kernel/debug/tracing/tracing_on

# 查看结果
cat /sys/kernel/debug/tracing/trace

# 清除
echo > /sys/kernel/debug/tracing/trace
```

**常用追踪器：**

| 追踪器 | 用途 |
|--------|------|
| function | 函数调用 |
| function_graph | 函数调用图 |
| wakeup | 调度延迟 |
| irqsoff | 中断延迟 |
| preemptoff | 抢占延迟 |

---

### 7.3 perf如何使用？

**答案：**

**perf性能分析工具：**

```bash
# CPU热点分析
perf top -a

# 记录数据
perf record -a -g -- sleep 10

# 查看报告
perf report

# 查看特定事件
perf stat -e cycles,instructions,cache-misses -- ./program

# 系统级分析
perf record -g -p <pid>
perf report

# 函数级分析
perf record -g
perf annotate <function>
```

**常用事件：**

| 事件 | 说明 |
|------|------|
| cycles | CPU周期 |
| instructions | 指令数 |
| cache-references | 缓存引用 |
| cache-misses | 缓存未命中 |
| context-switches | 上下文切换 |
| cpu-migrations | CPU迁移 |

---

### 7.4 kgdb如何使用？

**答案：**

**kgdb内核调试配置：**

```bash
# 内核配置
CONFIG_DEBUG_INFO=y
CONFIG_KGDB=y
CONFIG_KGDB_SERIAL_CONSOLE=y
CONFIG_DEBUG_KERNEL=y

# 启动参数
# 在bootargs中添加:
kgdboc=ttyAMA0,115200 kgdbwait
```

**使用流程：**

```bash
# 主机端(需要串口)
# 1. 打开串口
screen /dev/ttyUSB0 115200

# 2. 触发调试
# 在目标机执行:
echo g > /proc/sysrq-trigger

# 3. 主机连接
arm-none-eabi-gdb vmlinux
(gdb) target remote /dev/ttyUSB0

# 调试命令
(gdb) bt              # 栈回溯
(gdb) p variable     # 打印变量
(gdb) c               # 继续
(gdb) n               # 单步
(gdb) s               # 步入
(gdb) list            # 显示源码
```

**常见调试场景：**

```c
// 1. 在内核中添加断点
void my_function(void) {
    // 编译时带有调试信息
    // ...
    // kgdb会停止
}

// 2. 使用KGDB_ASSERT
#include <linux/kgdb.h>
KGDB_BREAKPOINT();

// 3. 查看崩溃信息
// 在内核panic时，kgdb会自动进入
```

---

## 8. 面试场景题

### 8.1 驱动中如何处理并发？

**答案：**

**并发场景：**
- 多核CPU并发访问
- 中断与进程并发
- 多进程并发

**解决方案：**

**1. 自旋锁**
```c
#include <linux/spinlock.h>

DEFINE_SPINLOCK(my_lock);
unsigned long flags;

spin_lock_irqsave(&my_lock, flags);
// 临界区
spin_unlock_irqrestore(&my_lock, flags);
```

**2. 互斥锁**
```c
#include <linux/mutex.h>

DEFINE_MUTEX(my_mutex);

mutex_lock(&my_mutex);
// 临界区
mutex_unlock(&my_mutex);
```

**3. 原子操作**
```c
#include <linux/atomic.h>

atomic_t counter;
atomic_inc(&counter);
atomic_dec(&counter);
int val = atomic_read(&counter);
```

**4. 信号量**
```c
#include <linux/semaphore.h>

struct semaphore sem;
sema_init(&sem, 1);

down(&sem);
// 临界区
up(&sem);
```

**选择原则：**
- 中断上下文 → 自旋锁
- 进程上下文 → 互斥锁/信号量
- 需要睡眠 → 信号量
- 原子操作 → 原子变量

---

### 8.2 设备驱动中内存如何分配？

**答案：**

**内存分配方式：**

```c
// 1. 驱动私有数据
struct mydev {
    void *kmalloc_ptr;
    void *vmalloc_ptr;
    void *dma_buf;
};

// kmalloc: 小块连续内存
void *buf = kmalloc(256, GFP_KERNEL);
// 常用标志:
GFP_KERNEL: 睡眠等待，可能阻塞
GFP_ATOMIC: 不睡眠，高优先级
GFP_DMA: 分配DMA内存

// vmalloc: 大块非连续内存
void *buf = vmalloc(4096);

// DMA内存
// 使用DMA API
struct device *dev = &pci_dev->dev;
dma_addr_t handle;
void *buf = dma_alloc_coherent(dev, size, &handle, GFP_KERNEL);
dma_free_coherent(dev, size, buf, handle);

// 设备树内存资源
struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
void __iomem *base = devm_ioremap_resource(&pdev->dev, res);

// 内存池
struct mempool_t *pool = mempool_create(10, mempool_alloc_slab, mempool_free_slab, (void *)SLAB_DMA);
void *buf = mempool_alloc(pool, GFP_KERNEL);
mempool_free(buf, pool);
```

---

### 8.3 Linux驱动调试常见问题？

**答案：**

**常见问题及解决：**

**1. 模块加载失败**
```bash
# 查看错误
dmesg | tail
# 常见错误:
# - Unknown symbol (缺少依赖)
# - Device or resource busy (资源被占用)
# - Invalid parameter (参数错误)
```

**2. 设备节点不存在**
```bash
# 手动创建
mknod /dev/mynode c 240 0
# 或检查udev规则
```

**3. 内存泄漏**
```bash
# 使用kmemleak检测
echo scan > /sys/kernel/debug/kmemleak
cat /sys/kernel/debug/kmemleak
```

**4. 死锁**
```bash
# 使用lockdep检测
# 内核配置: CONFIG_LOCKDEP
# 运行时:
deadlock: 尝试各种锁顺序组合
```

**调试技巧：**
```c
// 1. 添加调试信息
#define DEBUG
printk(KERN_DEBUG "debug info\r\n");

// 2. 使用WARN_ON/WARN_ON_ONCE
WARN_ON(condition);
WARN_ON_ONCE(condition);

// 3. 使用dump_stack
dump_stack();

// 4. 检查返回值
int ret = some_function();
if(ret) {
    printk(KERN_ERR "error: %d\r\n", ret);
    return ret;
}
```

---

### 8.4 项目中遇到的最有挑战性的问题？

**建议回答结构：**

**问题示例1: DMA数据丢失**
> "在开发网络驱动时遇到DMA数据丢失问题。分析发现是缓存一致性问题，通过在DMA缓冲区使用dma_alloc_coherent()和正确的sync操作解决了问题。"

**问题示例2: 中断风暴**
> "设备驱动在高负载下产生中断风暴，导致系统卡顿。通过使用NAPI轮询机制，将中断处理改为轮询+中断结合的方式，限制了最大处理速率。"

**问题示例3: 驱动加载顺序**
> "多个驱动存在依赖关系，加载顺序错误导致崩溃。通过使用module_init的优先级参数和设备树定义，明确了依赖关系。"

**问题示例4: 实时性优化**
> "项目需要实时响应，但普通Linux调度延迟太高。通过PREEMPT_RT补丁、设置实时调度策略、绑核等优化，将延迟从几十毫秒降到1毫秒以内。"

---

### 8.5 你为什么选择这个驱动方案？

**答案：**

**选型考量：**

```c
/*
需求:
- ARM Cortex-A系列SoC
- 需要I2C、SPI、GPIO、UART驱动
- 需要以太网驱动
- 维护周期3年

选型对比:
1. 设备树 + Platform驱动
+ 标准方式，社区支持好
+ 与内核解耦
- 代码量稍多

2. 传统Platform device
+ 简单
- 不符合新内核趋势

3. 混合方式
+ 结合两者优点

最终选择: 设备树 + Platform
原因: 主流方式，代码复用好，易维护
*/
```

**示例回答：**
> "选择设备树+Platform驱动方案，主要考虑：1) 主流方式，内核文档完善；2) 与硬件解耦，换平台只需改设备树；3) 社区活跃，踩坑少；4) 符合内核发展趋势。"

---

## 9. 综合问题

### 9.1 Linux驱动与RTOS驱动有什么区别？

**答案：**

**对比：**

| 方面 | RTOS驱动 | Linux驱动 |
|------|----------|-----------|
| 编程模型 | 简单函数 | 复杂框架 |
| 并发 | 单核为主 | 多核多线程 |
| 内存管理 | 简单 | 复杂(Slab/VM) |
| API风格 | 厂商定制 | 标准内核API |
| 调试 | 简单 | 复杂(需要工具) |
| 实时性 | 好 | 需要PREEMPT_RT |
| 生态 | 依赖MCU厂商 | 社区丰富 |

---

### 9.2 如何保证驱动的稳定性？

**答案：**

**稳定性保障措施：**

```c
// 1. 完善错误处理
int mydev_probe(struct platform_device *pdev) {
    struct resource *res;
    void __iomem *base;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if(!res) {
        dev_err(&pdev->dev, "No memory resource\r\n");
        return -ENODEV;
    }

    base = devm_ioremap_resource(&pdev->dev, res);
    if(IS_ERR(base))
        return PTR_ERR(base);

    return 0;
}

// 2. 资源管理(使用devm_*函数)
int mydev_probe(...) {
    // 自动释放
    devm_request_irq(...);
    devm_clk_get(...);
    devm_regulator_get(...);
}

// 3. 运行时检查
int mydev_read(...) {
    if(!device_ready)
        return -ENODEV;
    // ...
}

// 4. 锁保护
DEFINE_MUTEX(lock);
mutex_lock(&lock);
// 操作
mutex_unlock(&lock);

// 5. 统计与监控
struct dentry *debugfs;
// 创建调试接口
```

---

### 9.3 未来Linux驱动发展趋势？

**答案：**

**发展趋势：**

**1. 统一驱动框架**
- ACPI/设备树统一
- 跨平台驱动
- 虚拟设备驱动

**2. 智能化**
- eBPF驱动验证
- AI辅助驱动开发
- 自动化测试

**3. 安全**
- 安全启动
- 驱动签名
- 隔离执行

**4. 实时性**
- PREEMPT_RT主线化
- 实时虚拟化
-确定性网络

**技术热点：**
- USB4/Thunderbolt
- Wi-Fi 6E / 7
- PCIe 5.0 / 6.0
- CXL内存
- 高速接口(112G Ethernet)

---

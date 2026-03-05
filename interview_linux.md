# 嵌入式Linux面试知识点（详细版）

> 适用于高级嵌入式工程师岗位（智能硬件大厂）
> 本文档涵盖Linux内核、驱动、ARM64架构等核心技术点

---

## 目录

1. [内核子系统](#1-内核子系统)
2. [驱动子系统](#2-驱动子系统)
3. [任务调度](#3-任务调度)
4. [ARM64架构](#4-arm64架构)
5. [调试技巧](#5-调试技巧)
6. [性能优化](#6-性能优化)

---

## 1. 内核子系统

### 1.1 中断子系统

#### 硬件中断处理流程

Linux中断处理采用分层架构：

```
┌─────────────────────────────────────────────────────────────────┐
│                    Linux中断处理架构                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   外设          中断控制器           内核                        │
│   ─────        ───────────        ──────                       │
│                                                                  │
│   IRQ ──────→  GIC ──────→  Linux IRQ Domain ──→ ISR       │
│                   │                                              │
│                   ├──→ 硬件处理(顶半部)                        │
│                   │        ↓                                    │
│                   ├──→ 软中断/Tasklet(底半部)                 │
│                   │        ↓                                    │
│                   └──→ Workqueue(延迟处理)                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 中断描述符

```c
struct irq_desc {
    struct irq_data             irq_data;
    irq_flow_handler_t          handle_irq;       // 流向处理函数
    struct irqaction            *action;          // 中断动作链表
    const char                  *name;            // 中断名称
    raw_spinlock_t              lock;             // 保护锁
    struct irq_domain          *domain;          // IRQ域
    void                        *handler_data;    // 私有数据
    // ...
};

struct irqaction {
    irq_handler_t             handler;           // 中断处理函数
    void                       *dev_id;         // 设备ID
    void                       *handler_data;
    const char                 *name;            // 中断名
    struct irqaction           *next;           // 下一个动作
    int                        irq;             // IRQ号
    unsigned int               flags;           // 标志
};
```

#### 注册中断

```c
// 申请中断
int request_irq(unsigned int irq,
                irq_handler_t handler,
                unsigned long flags,
                const char *name,
                void *dev);

static irqreturn_t my_irq_handler(int irq, void *dev_id) {
    // 检查中断是否来自本设备
    if(!(reg->STATUS & MY_IRQ_FLAG))
        return IRQ_NONE;

    // 处理中断
    handle_my_irq();

    return IRQ_HANDLED;
}

// 在驱动初始化中
static int __init my_driver_init(void) {
    int ret;

    ret = request_irq(irq_num,
                       my_irq_handler,
                       IRQF_SHARED | IRQF_ONESHOT,
                       "my_driver",
                       &my_device);

    return ret;
}

// 释放中断
void free_irq(unsigned int irq, void *dev_id);
```

### 1.2 软中断（Softirq）

#### 软中断类型

```c
// 定义在 linux/interrupt.h
enum {
    HI_SOFTIRQ,          // 高优先级tasklet
    TIMER_SOFTIRQ,        // 定时器
    NET_TX_SOFTIRQ,      // 网络发送
    NET_RX_SOFTIRQ,      // 网络接收
    BLOCK_SOFTIRQ,        // 块设备
    IRQ_POLL_SOFTIRQ,     // IRQ轮询
    TASKLET_SOFTIRQ,      // 普通tasklet
    SCHED_SOFTIRQ,        // 调度
    HRTIMER_SOFTIRQ,      // 高精度定时器
    RCU_SOFTIRQ,          // RCU回调
    NR_SOFTIRQS
};

// 注册软中断
void open_softirq(int nr, void (*action)(struct softirq_action *));

// 触发软中断
void raise_softirq(unsigned int nr);

// 处理软中断
asmlinkage void do_softirq(void);
```

#### 软中断处理流程

```c
// 内核启动软中断处理
void __do_softirq(void) {
    struct softirq_action *h;
    unsigned long pending;
    unsigned int max_restart = MAX_SOFTIRQ_RESTART;

    pending = local_softirq_pending();

restart:
    // 清除挂起标志
    local_softirq_disable();
    local_softirq_enable();

    // 遍历软中断向量
    h = softirq_vec;
    do {
        if(pending & 1) {
            h->action(h);  // 执行处理函数
            rcu_bh_qs(current);
        }
        h++;
        pending >>= 1;
    } while(pending);

    // 检查是否有新的软中断
    pending = local_softirq_pending();
    if(pending && --max_restart)
        goto restart;
}
```

### 1.3 Tasklet机制

```c
// 定义tasklet
void my_tasklet_func(unsigned long data);
DECLARE_TASKLET(my_tasklet, my_tasklet_func, data);

// 或者
DECLARE_TASKLET_DISABLED(my_tasklet, my_tasklet_func, data);

// 在中断处理中调度tasklet
void interrupt_handler(int irq, void *dev) {
    // 只做简单处理
    tasklet_schedule(&my_tasklet);  // 调度到软中断上下文
}

// tasklet处理函数
void my_tasklet_func(unsigned long data) {
    // 可以睡眠的处理
    process_data();
}
```

### 1.4 工作队列

#### 专用工作队列

```c
// 创建工作
struct work_struct my_work;
INIT_WORK(&my_work, work_handler);

void work_handler(struct work_struct *work) {
    // 在进程上下文执行，可以睡眠
    do_something();
}

// 调度工作
schedule_work(&my_work);

// 延迟工作
schedule_delayed_work(&my_delayed_work, delay);

// 刷新队列
flush_scheduled_work();
```

#### 共享工作队列

```c
// 创建workqueue
struct workqueue_struct *my_wq;
my_wq = create_workqueue("my_wq");

// 创建工作
INIT_WORK(&my_work, work_handler);
queue_work(my_wq, &my_work);

// 销毁
destroy_workqueue(my_wq);
```

### 1.5 进程管理

#### 进程创建

```c
// fork系统调用
pid_t pid = fork();
if(pid == 0) {
    // 子进程
    execve("/path/to/program", argv, envp);
} else if(pid > 0) {
    // 父进程
    waitpid(pid, &status, 0);
}

// clone系统调用
int clone(int (*fn)(void *), void *stack, int flags, void *arg, ...);

// pthread创建
pthread_t thread;
pthread_create(&thread, NULL, thread_func, arg);
pthread_join(thread, NULL);
```

#### 进程状态

```
                  ┌──────────┐
                  │  运行    │
                  │ (Running)│
                  └────┬─────┘
                       │
         ┌─────────────┼─────────────┐
         │             │             │
    ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
    │ 就绪     │ │ 睡眠    │ │ 停止    │
    │(Ready)  │ │(Sleep)  │ │(Stopped)│
    └────┬────┘ └────┬────┘ └────┬────┘
         │             │             │
         └─────────────┼─────────────┘
                       │
                  ┌────▼────┐
                  │ 僵尸    │
                  │(Zombie) │
                  └─────────┘
```

### 1.6 内存管理

#### 页表结构（ARM64）

```c
// ARM64页表项
typedef struct {
    unsigned long pte;  // 页表项值
} pte_t;

#define PTE_VALID        (_AT(pte_t, 1) << 0)
#define PTE_USER         (_AT(pte_t, 1) << 6)
#define PTE_AF           (_AT(pte_t, 1) << 10)   // Accessed
#define PTE_SHARED       (_AT(pte_t, 3) << 8)    // Shareable
#define PTE_ATTRINDX(n)  (_AT(pte_t, n) << 2)   // 内存类型索引
#define PTE_NS           (_AT(pte_t, 1) << 5)     // Non-secure

// 页表级别
// 4级页表: PGD -> PUD -> PMD -> PTE
// VA: [63:48][47:39][38:30][29:21][20:12][11:0]
//       |      |      |      |      |
//      PGD    PUD    PMD   PTE   Offset
```

#### Buddy System

```c
// 伙伴系统分配
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);
void __free_pages(struct page *page, unsigned int order);

// 分配页面
struct page *page = alloc_pages(GFP_KERNEL, 0);  // 4KB
void *page_addr = page_address(page);

// 释放
__free_pages(page, 0);
```

#### Slab分配器

```c
// 创建slab缓存
struct kmem_cache *my_cache;
my_cache = kmem_cache_create("my_cache",
                              sizeof(struct my_obj),
                              align,
                              SLAB_HWCACHE_ALIGN,
                              ctor,
                              dtor);

// 分配对象
struct my_obj *obj = kmem_cache_alloc(my_cache, GFP_KERNEL);

// 释放对象
kmem_cache_free(my_cache, obj);

// 销毁缓存
kmem_cache_destroy(my_cache);
```

### 1.7 文件系统

#### VFS数据结构

```
┌─────────────────────────────────────────────────────────────────┐
│                        VFS架构                                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   ┌──────────┐   ┌──────────┐   ┌──────────┐               │
│   │  应用     │   │  应用     │   │  应用     │               │
│   └────┬─────┘   └────┬─────┘   └────┬─────┘               │
│        │               │               │                       │
│        └───────────────┼───────────────┘                       │
│                        ↓                                       │
│               ┌────────────────┐                              │
│               │  系统调用接口   │                              │
│               │  open/read/write                              │
│               └────────┬────────┘                              │
│                        ↓                                       │
│               ┌────────────────┐                              │
│               │   inode_operations                             │
│               │   file_operations                             │
│               └────────┬────────┘                              │
│                        ↓                                       │
│   ┌──────────────────────────────────────────────────────┐    │
│   │                   VFS层                                 │    │
│   │  super_block → inode → dentry → file                │    │
│   └────────┬─────────────────────────────────────────────┘    │
│            ↓                                                   │
│   ┌──────────────────────────────────────────────────────┐    │
│   │              具体文件系统 (ext4/f2fs/ubifs)            │    │
│   └──────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 文件系统挂载

```c
// 注册文件系统
register_filesystem(&my_fs_type);

static struct file_system_type my_fs_type = {
    .name = "myfs",
    .mount = my_mount,
    .kill_sb = kill_block_super,
    .owner = THIS_MODULE,
};

// 挂载
mount("/dev/mtdblock0", "/mnt", "myfs", 0, NULL);

// 卸载
umount("/mnt");
```

---

## 2. 驱动子系统

### 2.1 字符设备驱动

```c
// 字符设备结构
static int mydev_open(struct inode *inode, struct file *filp);
static int mydev_release(struct inode *inode, struct file *filp);
static ssize_t mydev_read(struct file *filp, char __user *buf,
                         size_t count, loff_t *ppos);
static ssize_t mydev_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *ppos);

static struct file_operations mydev_fops = {
    .owner   = THIS_MODULE,
    .open    = mydev_open,
    .release = mydev_release,
    .read    = mydev_read,
    .write   = mydev_write,
    .llseek  = default_llseek,
};

// 注册字符设备
static int __init mydev_init(void) {
    dev_t devno;
    int ret;

    // 分配设备号
    ret = alloc_chrdev_region(&devno, 0, 1, "mydev");
    if(ret < 0) return ret;

    // 初始化cdev
    cdev_init(&mydev_cdev, &mydev_fops);
    mydev_cdev.owner = THIS_MODULE;

    // 添加cdev
    ret = cdev_add(&mydev_cdev, devno, 1);
    if(ret < 0) goto fail;

    return 0;

fail:
    unregister_chrdev_region(devno, 1);
    return ret;
}

// 设备节点创建
// mknod /dev/mydev c 240 0
```

### 2.2 Platform设备驱动

```c
// Platform驱动
static int my_probe(struct platform_device *dev);
static int my_remove(struct platform_device *dev);

static const struct of_device_id my_of_match[] = {
    { .compatible = "my,device", },
    { }
};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my_driver",
        .of_match_table = my_of_match,
    }
};

module_platform_driver(my_driver);

// Platform设备(设备树)
&i2c1 {
    status = "okay";
    my_device@48 {
        compatible = "my,device";
        reg = <0x48>;
        interrupts = <0 IRQ_TYPE_EDGE_FALLING>;
        my-gpios = <&gpio1 0 GPIO_ACTIVE_HIGH>;
    };
};
```

### 2.3 I2C驱动

```c
// I2C驱动
static int my_i2c_probe(struct i2c_client *client,
                        const struct i2c_device_id *id);
static int my_i2c_remove(struct i2c_client *client);

static const struct i2c_device_id my_i2c_id[] = {
    { "my_i2c_device", 0 },
    { }
};

static struct i2c_driver my_i2c_driver = {
    .probe    = my_i2c_probe,
    .remove   = my_i2c_remove,
    .id_table = my_i2c_id,
    .driver   = {
        .name = "my_i2c",
        .of_match_table = of_match_ptr(my_of_match),
    },
};

module_i2c_driver(my_i2c_driver);

// I2C通信
static int i2c_read_reg(struct i2c_client *client,
                       u8 reg, u8 *val) {
    int ret;
    u8 buf[1] = { reg };

    struct i2c_msg msgs[2] = {
        { .addr = client->addr, .flags = 0, .len = 1, .buf = buf },
        { .addr = client->addr, .flags = I2C_M_RD, .len = 1, .buf = val },
    };

    ret = i2c_transfer(client->adapter, msgs, 2);
    return (ret == 2) ? 0 : ret;
}

static int i2c_write_reg(struct i2c_client *client,
                        u8 reg, u8 val) {
    u8 buf[2] = { reg, val };
    return i2c_master_send(client, buf, 2);
}
```

### 2.4 SPI驱动

```c
// SPI驱动
static int my_spi_probe(struct spi_device *spi);
static int my_spi_remove(struct spi_device *spi);

static const struct of_device_id my_spi_of_match[] = {
    { .compatible = "my,spi_device", },
    { }
};

static struct spi_driver my_spi_driver = {
    .probe    = my_spi_probe,
    .remove   = my_spi_remove,
    .driver   = {
        .name = "my_spi",
        .of_match_table = my_spi_of_match,
    },
};

module_spi_driver(my_spi_driver);

// SPI数据传输
static int spi_read_reg(struct spi_device *spi, u8 reg, u8 *val) {
    int ret;
    u8 tx_buf[2] = { reg, 0 };
    u8 rx_buf[2];

    struct spi_transfer tr[] = {
        { .tx_buf = tx_buf, .rx_buf = rx_buf, .len = 2 },
    };

    ret = spi_sync_transfer(spi, tr, 1);
    *val = rx_buf[1];
    return ret;
}

static int spi_write_reg(struct spi_device *spi, u8 reg, u8 val) {
    u8 tx_buf[2] = { reg, val };
    return spi_write(spi, tx_buf, 2);
}
```

### 2.5 GPIO子系统

```c
// 申请GPIO
int gpio_request(unsigned gpio, const char *label);
void gpio_free(unsigned gpio);

// 设置方向
int gpio_direction_input(unsigned gpio);
int gpio_direction_output(unsigned gpio, int value);

// 设置/获取值
int gpio_set_value(unsigned gpio, int value);
int gpio_get_value(unsigned gpio);

// 使用GPIO描述符(Linux 4.8+)
struct gpio_desc *gpiod_get(struct device *dev,
                            const char *con_id,
                            enum gpiod_flags flags);
int gpiod_direction_input(struct gpio_desc *desc);
int gpiod_direction_output(struct gpio_desc *desc, int value);
int gpiod_get_value(struct gpio_desc *desc);
void gpiod_set_value(struct gpio_desc *desc, int value);
```

### 2.6 pinctrl子系统

```c
// 获取pinctrl
struct pinctrl *pinctrl = devm_pinctrl_get_select_default(&dev);
if(IS_ERR(pinctrl)) return PTR_ERR(pinctrl);

// 获取特定状态
struct pinctrl_state *state = pinctrl_lookup_state(pinctrl, "sleep");
pinctrl_select_state(pinctrl, state);

// Device Tree配置
&pinctrl {
    my_pins: my-pins {
        pins = "PA0", "PA1";
        function = "gpio";
        bias-pull-up;
        drive-strength = <8>;
    };

    uart1_pins: uart1-pins {
        pins = "PA9", "PA10";
        function = "uart1";
    };
};
```

### 2.7 网络驱动

```c
// 网络设备结构
static int my_net_open(struct net_device *dev);
static int my_net_stop(struct net_device *dev);
static netdev_tx_t my_net_start_xmit(struct sk_buff *skb,
                                      struct net_device *dev);
static struct net_device_stats *my_net_get_stats(struct net_device *dev);

static const struct net_device_ops my_netdev_ops = {
    .ndo_open        = my_net_open,
    .ndo_stop        = my_net_stop,
    .ndo_start_xmit = my_net_start_xmit,
    .ndo_get_stats  = my_net_get_stats,
};

static int my_net_probe(struct platform_device *pdev) {
    struct net_device *ndev;
    int ret;

    ndev = alloc_etherdev(sizeof(struct my_priv));
    ndev->netdev_ops = &my_netdev_ops;
    ndev->ethtool_ops = &my_ethtool_ops;

    ret = register_netdev(ndev);
    if(ret) goto fail;

    return 0;

fail:
    free_netdev(ndev);
    return ret;
}
```

---

## 3. 任务调度

### 3.1 CFS调度器

#### 原理

CFS(Completely Fair Scheduler)使用红黑树管理任务，按虚拟运行时间(vruntime)排序：

```
┌─────────────────────────────────────────────────────────────────┐
│                    CFS红黑树                                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│                         [vruntime=80]                         │
│                        /         \                              │
│              [60]                       [100]                 │
│             /     \                   /      \                │
│         [40]       [55]           [90]        [120]         │
│                                                                  │
│  任务运行时间 = 实际运行时间 * (nice0权重 / 任务权重)          │
│  vruntime += delta_exec * (nice0_weight / task_weight)        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 权重表

```c
// nice值到权重映射
const int sched_prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548, 7620, 6100, 4504, 3700,
    /*  -5 */ 2901, 2301, 1845, 1490, 1225,
    /*   0 */ 1024, 820, 655, 526, 423,
    /*   5 */ 335, 272, 215, 172, 137,
    /*  10 */ 110, 90, 75, 63, 52, 42, 32
};
```

### 3.2 实时调度

```c
// 设置实时调度策略
struct sched_param param;
param.sched_priority = 99;  // 最高优先级

// FIFO调度
sched_setscheduler(pid, SCHED_FIFO, &param);

// RR调度
sched_setscheduler(pid, SCHED_RR, &param);

// Deadline调度
struct sched_attr attr = {
    .size = sizeof(attr),
    .sched_policy = SCHED_DEADLINE,
    .sched_runtime = 10000000,   // 10ms
    .sched_period = 10000000,   // 周期10ms
    .sched_deadline = 10000000, // 截止时间10ms
};
sched_setattr(pid, &attr, 0);
```

### 3.3 CPU亲和性

```c
// 设置CPU亲和性
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);
CPU_SET(1, &cpuset);

sched_setaffinity(0, sizeof(cpuset), &cpuset);

// 获取CPU亲和性
sched_getaffinity(0, sizeof(cpuset), &cpuset);

// 每个CPU运行独立任务
for(i = 0; i < num_cpus; i++) {
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    sched_setaffinity(tids[i], sizeof(cpuset), &cpuset);
}
```

---

## 4. ARM64架构

### 4.1 异常级别

```
┌─────────────────────────────────────────────────────────────────┐
│                    ARM64异常级别                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  EL3 (Exception Level 3)                                       │
│  ───────────────────────                                       │
│  Secure Monitor                                                 │
│  - TrustZone安全状态切换                                        │
│  - 最高特权级                                                   │
│                                                                  │
│  EL2 (Exception Level 2)                                       │
│  ───────────────────────                                       │
│  Hypervisor                                                     │
│  - 虚拟化支持                                                   │
│  - 虚拟机管理                                                   │
│                                                                  │
│  EL1 (Exception Level 1)                                       │
│  ───────────────────────                                       │
│  OS Kernel                                                      │
│  - 操作系统内核                                                 │
│  - 特权模式                                                     │
│                                                                  │
│  EL0 (Exception Level 0)                                        │
│  ───────────────────────                                       │
│  Application                                                    │
│  - 用户应用程序                                                 │
│  - 非特权模式                                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 寄存器

```c
// 通用寄存器 X0-X30 (64位) / W0-W30 (32位)
//
// X0-X7: 函数参数和返回值
// X0:   返回值
// X29:  帧指针(FP)
// X30:  链接寄存器(LR)
// SP:   堆栈指针
// PC:   程序计数器

// 特殊寄存器
// SPSR (Saved Program Status Register)
// ELR (Exception Link Register)
// SP_EL0 / SP_EL1 / SP_EL2 / SP_EL3
```

### 4.3 系统调用

```c
// ARM64系统调用
// #define __NR_read 63

// 用户态发起系统调用
mov x8, #63     // 系统调用号 -> X8
mov x0, fd      // 参数1
mov x1, buf     // 参数2
mov x2, count   // 参数3
svc #0          // 触发系统调用

// 内核端系统调用处理
asmlinkage long sys_read(unsigned int fd,
                        char __user *buf,
                        size_t count);
```

### 4.4 页表

```c
// ARM64页表配置
// 4级页表: PGD -> PUD -> PMD -> PTE

// VA布局 (48位VA)
//
// [63:48] [47:39] [38:30] [29:21] [20:12] [11:0]
//  VA[47:39] -> PGD索引
//  VA[38:30] -> PUD索引
//  VA[29:21] -> PMD索引
//  VA[20:12] -> PTE索引
//  VA[11:0 ] -> 页内偏移

// TTBR0 (用户空间)
// TTBR1 (内核空间)
```

### 4.5 中断控制器(GIC)

```c
// GICv2基本编程
// GICDistributor
#define GICD_CTLR      (GIC_DIST_BASE + 0x000)
#define GICD_ISENABLER (GIC_DIST_BASE + 0x100)
#define GICD_IPRIORITY (GIC_DIST_BASE + 0x400)
#define GICD_ITARGETSR (GIC_DIST_BASE + 0x800)

// 使能中断
void enable_irq(unsigned int irq) {
    writel(1 << (irq % 32), GICD_ISENABLER(irq / 32));
}

// GICCPUInterface
#define GICC_CTLR      (GIC_CPU_BASE + 0x00)
#define GICC_PMR       (GIC_CPU_BASE + 0x04)

// 设置中断优先级掩码
writel(0xF0, GICC_PMR);  // 允许所有优先级

// 使能CPU接口
writel(1, GICC_CTLR);
```

---

## 5. 调试技巧

### 5.1 printk日志

```c
// 日志级别
printk(KERN_EMERG "Emergency\n");    // 0
printk(KERN_ALERT "Alert\n");          // 1
printk(KERN_CRIT "Critical\n");        // 2
printk(KERN_ERR "Error\n");           // 3
printk(KERN_WARNING "Warning\n");     // 4
printk(KERN_NOTICE "Notice\n");       // 5
printk(KERN_INFO "Info\n");           // 6
printk(KERN_DEBUG "Debug\n");         // 7

// 动态调试
#define DEBUG 1
#ifdef DEBUG
#define pr_debug(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) no_printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#endif
```

### 5.2 ftrace

```bash
# 启用function追踪
echo function > /sys/kernel/debug/tracing/current_tracer

# 设置过滤器
echo "schedule" > /sys/kernel/debug/tracing/set_ftrace_filter

# function_graph
echo function_graph > /sys/kernel/debug/tracing/current_tracer

# 启用
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 查看
cat /sys/kernel/debug/tracing/trace

# 停止
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo nop > /sys/kernel/debug/tracing/current_tracer
```

### 5.3 perf

```bash
# CPU周期采样
perf record -g -a ./program
perf report

# 热点函数
perf top -p $(pidof process)

# 系统调用追踪
perf record -e syscalls:sys_enter_read -a
perf report

# 调度分析
perf sched record -- ./program
perf sched timehist
```

### 5.4 crash分析

```bash
# 分析vmcore
crash vmlinux vmcore

# 查看进程
crash> ps

# 查看内核日志
crash> log

# 查看调用栈
crash> bt

# 查看内存
crash> kmem -i

# 查看任务结构
crash> task 0xffff9000xxxxx
```

---

## 6. 性能优化

### 6.1 CPU优化

```c
// 1. 避免分支预测失败
// 错误
if(likely(condition)) { } else { }

// 正确
if(unlikely(error)) {
    // 处理错误
}

// 2. 缓存对齐
struct my_struct {
    u64 field1;    // 8字节
    u32 field2;   // 4字节
} __attribute__((aligned(64)));  // 64字节对齐

// 3. 预取
prefetch(address);
prefetchw(address);  // 预取用于写
```

### 6.2 内存优化

```c
// 1. DMA缓冲区对齐
void *dma_buf;
dma_buf = kmalloc(size + align, GFP_KERNEL | __GFP_ZERO);
if(!IS_ALIGNED((unsigned long)dma_buf, alignment))
    dma_buf = PTR_ALIGN(dma_buf, alignment);

// 2. 减少内存复制
// 使用零拷贝
// io_uring, splice, vmsplice

// 3. 大页内存
ret = syscall(SYS_madvise, addr, length, MADV_HUGEPAGE);
```

### 6.3 锁优化

```c
// 1. 读写锁
DEFINE_RWLOCK(my_rwlock);

read_lock(&my_rwlock);
// 读操作
read_unlock(&my_rwlock);

write_lock(&my_rwlock);
// 写操作
write_unlock(&my_rwlock);

// 2. RCU (Read-Copy-Update)
rcu_read_lock();
// 只读操作
rcu_read_unlock();

synchronize_rcu();
// 更新操作
```

---

## 附录

### 常见面试问题汇总

1. **Linux中断处理流程？**
   - 硬件中断 → GIC → 顶半部 → 软中断/Tasklet → Workqueue

2. **进程调度算法？**
   - CFS完全公平调度，vruntime红黑树管理

3. **内存管理机制？**
   - 页表、Buddy System、Slab分配器

4. **VFS数据结构？**
   - super_block、inode、dentry、file

5. **设备驱动模型？**
   - platform、i2c、spi、net、char驱动

6. **ARM64异常级别？**
   - EL0用户、EL1内核、EL2虚拟化、EL3安全

7. **如何分析性能问题？**
   - perf、ftrace、crash工具使用

---

*文档版本：v2.0 详细版*
*更新时间：2026-03-05*

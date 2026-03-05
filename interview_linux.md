# 嵌入式Linux面试知识点

> 适用于高级嵌入式工程师岗位（智能硬件大厂）

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
```
1. 外设产生中断
   ↓
2. 中断控制器接收（GIC）
   ↓
3. CPU响应中断，跳转到向量表
   ↓
4. 执行中断处理函数（顶半部）
   ↓
5. 触发软中断/工作队列（底半部）
   ↓
6. 中断返回
```

#### 软中断（Softirq）
- 优先级高于进程
- 不可睡眠
- 静态分配（32个）

**常见软中断类型**：
| 序号 | 名称 | 说明 |
|------|------|------|
| 0 | HI_SOFTIRQ | 高优先级tasklet |
| 1 | TIMER_SOFTIRQ | 定时器 |
| 2 | NET_TX_SOFTIRQ | 网络发送 |
| 3 | NET_RX_SOFTIRQ | 网络接收 |
| 4 | BLOCK_SOFTIRQ | 块设备 |
| 5 | IRQ_POLL_SOFTIRQ | IRQ轮询 |
| 6 | TASKLET_SOFTIRQ | 普通tasklet |
| 7 | SCHED_SOFTIRQ | 调度软中断 |
| 8 | HRTIMER_SOFTIRQ | 高精度定时器 |
| 9 | RCU_SOFTIRQ | RCU回调 |

#### Tasklet机制
- 基于软中断实现
- 动态创建
- 不可并发

```c
// Tasklet示例
void my_tasklet_func(unsigned long data) {
    /* 底半部处理 */
}

DECLARE_TASKLET(my_tasklet, my_tasklet_func, data);
tasklet_schedule(&my_tasklet);
```

#### 工作队列（Workqueue）
- 在进程上下文执行
- 可以睡眠
- 支持延迟执行

```c
// 工作队列示例
void my_work_func(struct work_struct *work) {
    /* 工作处理 */
}

DECLARE_WORK(my_work, my_work_func);
schedule_work(&my_work);
```

#### 顶半部/底半部机制
- **顶半部**：紧急处理，必须快
- **底半部**：耗时处理，可以延迟

#### 中断上下文与进程上下文
| 上下文 | 特点 | 允许操作 |
|--------|------|----------|
| 中断上下文 | 紧急、原子、不可睡眠 | 简单处理 |
| 进程上下文 | 正常执行、可睡眠 | 所有操作 |

#### 中断线程化（IRQF_THREADED）
- 中断处理作为内核线程
- 可调度、可睡眠
- 优先级可配置

```c
request_threaded_irq(irq, handler, thread_fn,
                     IRQF_ONESHOT, "my_irq", dev);
```

#### 中断亲和性
```c
// 设置中断亲和性
irq_set_affinity_hint(irq, cpumask_of(cpu));

// 查看中断统计
cat /proc/interrupts
```

### 1.2 进程管理

#### 进程/线程创建

**fork()**：
- 创建子进程
- 复制父进程资源
- 返回两次

**exec()**：
- 替换进程映像
- 加载新程序

**clone()**：
- 创建轻量级进程/线程
- 可共享资源

**pthread_create()**：
```c
int pthread_create(pthread_t *thread,
                   const pthread_attr_t *attr,
                   void *(*start_routine)(void *),
                   void *arg);
```

#### 进程状态
| 状态 | 说明 |
|------|------|
| R (Running) | 运行或就绪 |
| S (Sleeping) | 可中断睡眠 |
| D (Disk Sleep) | 不可中断睡眠 |
| T (Stopped) | 停止 |
| Z (Zombie) | 僵尸 |
| X (Dead) | 死亡 |

#### 进程调度器（CFS原理）
- **完全公平调度器**
- 基于虚拟运行时间(vruntime)
- 红黑树管理

**vruntime计算**：
```c
vruntime += delta_exec * NICE_0_LOAD / weight
```

#### 实时调度策略
| 策略 | 说明 |
|------|------|
| SCHED_FIFO | 先进先出，无时间片 |
| SCHED_RR | 时间片轮询 |
| SCHED_DEADLINE | 截止时间优先 |

```c
struct sched_attr {
    __u32 size;
    __u32 sched_policy;
    __u64 sched_flags;
    __s64 sched_runtime;
    __u64 sched_deadline;
    __u64 sched_period;
};
```

#### 进程间通信

**管道/命名管道**：
- 字节流
- 单向/双向
- 亲缘进程

**消息队列**：
- 链表实现
- 消息类型
- 容量有限

**共享内存**：
- 最高效IPC
- 需要同步
- mmap/shmget

**信号量**：
- 计数器
- P/V操作
- 互斥/同步

**信号（signal）**：
- 异步通知
- 常见信号：SIGKILL/SIGSTOP/SIGINT

**Socket**：
- 本地套接字
- 网络套接字
- 支持不同协议

#### 进程组与会话
- 进程组：相关进程集合
- 会话：进程组集合
- 守护进程：独立会话

#### 用户/内核态切换
- 系统调用
- 异常
- 中断

#### 系统调用机制

**x86_64系统调用**：
```asm
mov rax, [syscall_number]
mov rdi, [arg1]
mov rsi, [arg2]
mov rdx, [arg3]
syscall
ret
```

**ARM64系统调用**：
```asm
mov x8, [syscall_number]
mov x0, [arg1]
mov x1, [arg2]
svc #0
ret
```

### 1.3 内存管理

#### 虚拟内存原理
- 每个进程独立虚拟地址空间
- 页表映射到物理内存
- 缺页异常加载数据

#### 页表结构

**多级页表**：
- x86_64：4级页表（PGD→PUD→PMD→PTE）
- ARM64：4级/5级页表

**页表项内容**：
- 页帧号
- 访问权限
- 缓存属性

#### 页面分配器（Buddy System）

**原理**：
- 按阶（order）管理
- 相邻空闲页合并
- 减少外部碎片

**阶（Order）**：
- order 0：1页（4KB）
- order 1：2页（8KB）
- order n：2^n页

#### Slab分配器

**kmem_cache**：
- 专用对象缓存
- 减少碎片
- 提高分配效率

**常用缓存**：
- task_struct
- dentry
- inode

#### 内存映射（mmap）

```c
void *mmap(void *addr, size_t length,
           int prot, int flags, int fd, off_t offset);

munmap(void *addr, size_t length);
```

#### 缺页异常处理

**页面不在内存**：
1. 查找页表
2. 分配物理页
3. 读取数据
4. 更新页表
5. 继续执行

#### 内存回收（LRU、kswapd）

**LRU（最近最少使用）**：
- active_list
- inactive_list
- 按年龄排序

**kswapd**：
- 后台页面回收
- 页面置换
- 保持内存水位

#### 内存压缩（compaction）
- 移动页面合并
- 解决内部碎片

#### 大页内存（HugeTLB）
- 2MB/1GB页面
- 减少页表开销

```bash
# 配置大页
echo 20 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

#### 内存控制组（memcg）
- 限制内存使用
- 统计内存使用
- cgroup v2

### 1.4 文件系统

#### VFS虚拟文件系统

**四大对象**：
- super_block：超级块
- inode：索引节点
- dentry：目录项
- file：文件

**文件系统注册**：
```c
static struct file_system_type my_fs_type = {
    .name = "myfs",
    .mount = my_mount,
    .kill_sb = kill_block_super,
};
register_filesystem(&my_fs_type);
```

#### 常见文件系统

| 文件系统 | 特点 | 应用场景 |
|----------|------|----------|
| ext4 | 日志文件系统，成熟稳定 | 通用 |
| f2fs | 闪存优化 | 嵌入式存储 |
| ubifs | 大容量闪存 | NAND Flash |
| squashfs | 只读压缩 | 嵌入式系统 |
| tmpfs | 内存文件系统 | 临时文件 |

#### 文件描述符
- 进程私有
- 指向file结构
- 整数标识

#### 索引节点（inode）
- 文件元数据
- 唯一编号
- 磁盘/内存缓存

#### 块设备层

**bio结构**：
- 面向块的I/O
- 分散/聚集I/O

**request结构**：
- 队列请求
- 合并优化
- 调度算法

#### IO调度器

| 调度器 | 特点 | 场景 |
|--------|------|------|
| noop | 先进先出 | SSD |
| deadline | 截止时间 | 实时 |
| cfq | 完全公平 | 通用 |
| mq-deadline | 多队列 | NVMe |

---

## 2. 驱动子系统

### 2.1 基础框架

#### Character/Block/Network驱动区别

| 类型 | 特点 | 示例 |
|------|------|------|
| Character | 字节流/字符 | 串口、输入设备 |
| Block | 块设备 | 磁盘、U盘 |
| Network | 网络数据包 | 网卡 |

#### Platform设备驱动模型
```c
// 平台设备
struct platform_device {
    const char *name;
    int id;
    struct device dev;
    struct resource *resource;
    // ...
};

// 平台驱动
struct platform_driver {
    int (*probe)(struct platform_device *);
    int (*remove)(struct platform_device *);
    struct device_driver driver;
    // ...
};
```

#### Device Tree（OF解析）

**节点结构**：
```dts
&i2c1 {
    status = "okay";
    temp-sensor@48 {
        compatible = "ti,tmp105";
        reg = <0x48>;
    };
};
```

**OF API**：
```c
struct device_node *np = of_find_node_by_path("/soc/i2c1");
of_property_read_u32(np, "clock-frequency", &val);
of_get_named_gpio(np, "reset-gpios", 0);
```

#### Linux设备模型

**核心结构**：
- kobject：基础对象
- kset：kobject集合
- device：设备
- driver：驱动
- bus：总线

**关系**：
```
bus
├── driver
└── device
```

#### 模块加载与卸载
```c
module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("Driver Description");
```

### 2.2 常见驱动

#### GPIO子系统（gpiolib）

```c
// 申请GPIO
int gpio_request(unsigned gpio, const char *label);

// 设置方向
int gpio_direction_input(unsigned gpio);
int gpio_direction_output(unsigned gpio, int value);

// 设置/获取值
int gpio_set_value(unsigned gpio, int value);
int gpio_get_value(unsigned gpio);

// 释放
void gpio_free(unsigned gpio);

// GPIO映射（ARM）
gpiochip_add_data_with_key();
```

#### Pinctrl子系统

```c
// 获取pinctrl
struct pinctrl *p = devm_pinctrl_get_select_default(&dev);

// 设置状态
pinctrl_lookup_state();
pinctrl_select_state();
```

**Device Tree配置**：
```dts
&pinctrl {
    uart1_pins: uart1-pins {
        pins = "PA9", "PA10";
        function = "uart1";
    };
};
```

#### I2C驱动框架

```c
// I2C适配器驱动
struct i2c_driver {
    int (*probe)(struct i2c_client *, const struct i2c_device_id *);
    int (*remove)(struct i2c_client *);
    struct device_driver driver;
};

// I2C设备
struct i2c_client {
    unsigned short addr;
    struct i2c_adapter *adapter;
    struct device dev;
};

// 发送/接收
i2c_master_send();
i2c_master_recv();
i2c_transfer();
i2c_smbus_read_byte_data();
```

#### SPI驱动框架

```c
// SPI驱动
struct spi_driver {
    int (*probe)(struct spi_device *);
    int (*remove)(struct spi_device *);
    struct device_driver driver;
};

// SPI设备
struct spi_device {
    struct spi_master *master;
    u32 max_speed_hz;
    u8 chip_select;
    u8 mode;
    u8 bits_per_word;
};

// 传输
spi_sync();
spi_write();
spi_read();
spi_write_and_read();
```

#### MMC/SD驱动

**SDHCI**：
- SD Host Controller Interface
- 标准接口

**主要操作**：
- 识别卡类型
- 读取CID/CSD
- 块读写

#### USB驱动

**设备端驱动**：
- Gadget驱动
- Function驱动

**主机端驱动**：
- Hub驱动
- 类驱动（HID/CDC/MSC）

```c
// USB驱动
struct usb_driver {
    int (*probe)(struct usb_interface *, const struct usb_device_id *);
    void (*disconnect)(struct usb_interface *);
    struct device_driver driver;
};

// 端点操作
usb_bulk_msg();
usb_control_msg();
```

#### 网络驱动（NAPI、sk_buff）

**NAPI**：
- 轮询+中断混合
- 高性能收包

```c
static int my_poll(struct napi_struct *napi, int budget) {
    while (received < budget) {
        skb = my_receive_skb();
        netif_receive_skb(skb);
        received++;
    }
    if (received < budget)
        napi_complete(napi);
    return received;
}
```

**sk_buff**：
- 网络数据包结构
- 零拷贝支持
- 头部操作

#### Framebuffer驱动

```c
struct fb_info {
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    struct fb_ops *fbops;
    char __iomem *screen_base;
};

fb_set_par();
fb_setcolreg();
fb_fillrect();
fb_copyarea();
fb_imageblit();
```

#### Input子系统

```c
struct input_dev {
    const char *name;
    unsigned long evbit[BITS_TO_LONGS(EV_CNT)];
    // ...
};

input_allocate_device();
input_register_device();
input_event(input, EV_KEY, KEY_A, 1);
input_sync(input);
```

#### ALSA声卡驱动

```c
struct snd_card {
    struct device *dev;
    char *id;
    struct snd_card *next;
    // ...
};

struct snd_pcm {
    struct snd_pcm_ops *ops;
    struct snd_pcm_substream *streams;
};

snd_pcm_new();
snd_pcm_ops();
```

---

## 3. 任务调度

### 3.1 CFS调度器原理

#### 红黑树
- 自平衡二叉树
- O(log n)查找
- 按vruntime排序

#### vruntime
```c
// 计算
vruntime += delta_exec * NICE_0_LOAD / weight;

// 权重表
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

### 3.2 O(1)调度器
- 已废弃
- 2.6内核使用
- 优先级数组实现

### 3.3 实时调度类

**SCHED_FIFO**：
- 一直运行直到完成
- 更高优先级抢占

**SCHED_RR**：
- 时间片轮询
- 同优先级轮换

**SCHED_DEADLINE**：
- Earliest Deadline First
- 实时任务保障

### 3.4 调度延迟分析

**延迟组成**：
1. 中断延迟
2. 调度延迟
3. 上下文切换

### 3.5 CPU亲和性

```c
// 设置亲和性
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);
sched_setaffinity(0, sizeof(cpuset), &cpuset);

// 获取亲和性
sched_getaffinity(0, sizeof(cpuset), &cpuset);
```

### 3.6 调度域与负载均衡

**调度域层次**：
- NUMA域
- LLC域（最后一级缓存）
- CPU核

**负载均衡**：
- 定期检查
- 跨核迁移

### 3.7 组调度
- 控制组(Cgroup)调度
- 公平调度组
- 资源限制

---

## 4. ARM64架构

### 4.1 ARM64基础

#### AArch64指令集
- 64位架构
- 32位兼容模式（AArch32）
- 新增64位指令

#### 通用寄存器（X0-X30, SP, PC）

| 寄存器 | 别名 | 用途 |
|--------|------|------|
| X0-X7 | - | 参数传递 |
| X0 | - | 返回值 |
| X29 | FP | 帧指针 |
| X30 | LR | 链接寄存器 |
| SP | - | 堆栈指针 |
| PC | - | 程序计数器 |

#### 异常级别（EL0-EL3）

| 级别 | 名称 | 说明 |
|------|------|------|
| EL0 | 应用层 | 用户程序 |
| EL1 | 内核层 | 操作系统 |
| EL2 | 虚拟机 | 虚拟化 |
| EL3 | 安全层 | 安全监控 |

#### 运行状态
- AArch64：64位
- AArch32：32位（兼容）

### 4.2 内存管理

#### 页表结构（4级/5级页表）

**4级页表**：
- PGD（Page Global Directory）
- PUD（Page Upper Directory）
- PMD（Page Middle Directory）
- PTE（Page Table Entry）

**页大小**：
- 4KB（默认）
- 2MB（段页）
- 1GB（大页）

#### TTBR0/TTBR1
- TTBR0：用户空间
- TTBR1：内核空间

#### 转换后备缓冲区（TLB）
- 缓存虚拟地址映射
- ASID区分进程

#### ASID（地址空间ID）
- 进程标识
- 避免TLB刷洗

### 4.3 系统调用

#### SVC指令
```asm
svc #0  // 触发系统调用
```

#### 系统调用号
| 号 | 系统调用 |
|----|----------|
| 0 | read |
| 1 | write |
| 2 | open |
| 3 | close |
| ... | ... |

#### 参数传递（X0-X7）
- X0-X5：参数
- X0：返回值
- X8：系统调用号

### 4.4 中断与异常

#### 异常向量表
- 向量基址寄存器
- 各类异常入口

#### 中断控制器（GIC）

| 版本 | 特点 |
|------|------|
| GICv2 | 最多8核 |
| GICv3 | 支持更多核，ITS |
| GICv4 | 虚拟化支持 |

#### FIQ vs IRQ
- FIQ：快速中断
- IRQ：普通中断
- ARM64中差异减小

### 4.5 缓存与内存

#### 缓存架构
- L1：指令/数据分离
- L2：统一缓存
- L3：可选

#### DSB/ISB/DMB指令

| 指令 | 功能 |
|------|------|
| DSB | 数据同步屏障 |
| ISB | 指令同步屏障 |
| DMB | 数据内存屏障 |

#### 内存属性

| 类型 | 特点 |
|------|------|
| Normal | 可缓存 |
| Device | 不可缓存 |
| Strongly Ordered | 顺序访问 |

### 4.6 电源管理

#### PSCI协议
- Power State Coordination Interface
- CPUidle
- CPUhotplug
- 系统关机/重启

#### CPU idle驱动
- cpuidle框架
- 状态管理

#### 系统挂起与恢复
- suspend to RAM
- hibernation

---

## 5. 调试技巧

### 5.1 printk与日志级别

```c
printk(KERN_DEBUG "Debug message\n");
printk(KERN_INFO "Info message\n");
printk(KERN_WARNING "Warning\n");
printk(KERN_ERR "Error\n");
printk(KERN_CRIT "Critical\n");
```

**级别定义**：
| 级别 | 值 | 字符串 |
|------|-----|--------|
| KERN_EMERG | 0 | "" |
| KERN_ALERT | 1 | "" |
| KERN_CRIT | 2 | "" |
| KERN_ERR | 3 | "" |
| KERN_WARNING | 4 | "" |
| KERN_NOTICE | 5 | "" |
| KERN_INFO | 6 | "" |
| KERN_DEBUG | 7 | "" |

### 5.2 dmesg使用

```bash
dmesg                           # 查看所有消息
dmesg -C                        # 清空
dmesg -c                        # 查看后清空
dmesg -T                        # 时间戳
dmesg -l err                   # 过滤级别
dmesg | grep "keyword"         # 过滤关键字
```

### 5.3 ftrace函数追踪

```bash
# 启用function追踪
echo function > /sys/kernel/debug/tracing/current_tracer

# 设置过滤器
echo "schedule" > /sys/kernel/debug/tracing/set_ftrace_filter

# 启用
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 查看
cat /sys/kernel/debug/tracing/trace

# function_graph
echo function_graph > /sys/kernel/debug/tracing/current_tracer

# 关闭
echo nop > /sys/kernel/debug/tracing/current_tracer
```

### 5.4 性能分析

#### perf工具使用

```bash
# 采样CPU
perf record -g ./program
perf report

# 指定事件
perf stat -e cycles,instructions ./program

# 热点分析
perf top -p $(pidof process)

# 函数分析
perf probe --add schedule
perf record -e probe:schedule -a -- sleep 10
perf report
```

#### CPU热点分析
- 定位热点函数
- 分析调用链

#### 锁竞争分析
```bash
perf lock record ./program
perf lock report
```

#### 内存分配分析
```bash
perf mem record ./program
perf mem report
```

### 5.5 kgdb内核调试

**配置**：
```
# 内核配置
CONFIG_KGDB=y
CONFIG_KGDB_SERIAL_CONSOLE=y
```

**连接**：
```bash
# 目标机
echo g > /proc/sysrq-trigger

# 主机
arm-none-eabi-gdb vmlinux
(gdb) target remote /dev/ttyUSB0
```

### 5.6 crash/akdump分析

**crash工具**：
```bash
crash vmcore vmmlinux
crash> ps
crash> log
crash> bt
```

### 5.7 高级调试工具

#### BCC/eBPF动态追踪

```python
# execsnoop.py示例
from bcc import BPF

bpf_text = """
TRACEPOINT:syscalls:sys_enter_execve {
    @comm = comm;
}
"""
```

**常用工具**：
- execsnoop：追踪exec调用
- opensnoop：追踪open
- funclatency：函数延迟
- offcputime：off-CPU时间

#### SystemTap
```stap
probe kernel.function("schedule") {
    printf("%s\n", execname());
}
```

#### valgrind内存分析

```bash
valgrind --leak-check=full ./program
```

#### strace/ltrace系统调用追踪

```bash
strace -tt -T ./program
strace -e openat ./program
ltrace -f ./program
```

### 5.8 /proc文件系统

| 文件 | 说明 |
|------|------|
| /proc/cpuinfo | CPU信息 |
| /proc/meminfo | 内存信息 |
| /proc/interrupts | 中断统计 |
| /proc/softirqs | 软中断统计 |
| /proc/vmstat | 虚拟内存统计 |
| /proc/[pid]/status | 进程状态 |
| /proc/[pid]/maps | 内存映射 |

### 5.9 /sys文件系统

| 路径 | 说明 |
|------|------|
| /sys/class | 设备类 |
| /sys/device | 设备 |
| /sys/bus | 总线 |
| /sys/module | 内核模块 |
| /sys/kernel | 内核参数 |

### 5.10 内存调试

```bash
# slabinfo
slabtop

# meminfo分析
cat /proc/meminfo

# 内存分配跟踪
echo 1 > /proc/sys/vm/drop_caches
```

---

## 6. 性能优化

### 6.1 CPU性能优化

#### 热点函数分析
- perf top
- flame graph

#### 编译器优化选项
| 选项 | 说明 |
|------|------|
| -O0 | 无优化 |
| -O1 | 基本优化 |
| -O2 | 更多优化 |
| -O3 | 激进优化 |
| -Os | 空间优先 |
| -Ofast | 最快（可能违反标准） |

#### 指令级优化
- 循环展开
- 避免分支
- SIMD指令

### 6.2 内存性能优化

#### 内存泄漏检测
- kmemleak
- valgrind

#### 缓存命中率优化
- 数据对齐
- 预取
- 减少跨缓存行

#### 大页内存使用
- hugetlbfs
- THP（透明大页）

### 6.3 IO性能优化

#### IO调度器选择
- SSD：noop
- NVMe：mq-deadline
- 机械硬盘：deadline/cfq

#### 文件系统选择
- 日志文件系统
- 顺序写入优化

#### Direct IO vs Buffered IO
- Direct IO：绕过缓存
- Buffered IO：页缓存

### 6.4 锁竞争优化

#### 自旋锁 vs 互斥锁

| 锁类型 | 适用场景 |
|--------|----------|
| spin_lock | 短临界区 |
| mutex | 长临界区 |

#### 无锁编程（RCU）

**Read-Copy-Update**：
- 读无锁
- 写时复制
- 延迟释放

```c
rcu_read_lock();
/* 读操作 */
rcu_read_unlock();

synchronize_rcu();
/* 写操作 */
```

#### 减少临界区
- 缩小锁范围
- 读写锁
- 乐观锁

---

## 附录

### 常见面试问题

1. **Linux中断处理流程？**
2. **进程调度算法？**
3. **内存管理机制？**
4. **VFS数据结构？**
5. **设备驱动模型？**
6. **系统调用过程？**
7. **ARM64页表结构？**
8. **如何分析性能问题？**

---

*文档版本：v1.0*
*更新时间：2026-03-05*

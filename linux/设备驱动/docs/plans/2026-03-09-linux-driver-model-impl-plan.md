# Linux 驱动模型学习文档 - 实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task.

**目标:** 创建一份详细的 Linux 驱动模型学习文档，涵盖底层原理、总线设备驱动模型、设备树和驱动案例

**架构:** 文档采用混合结构，知识点与实践案例交替组织，包含 8 章核心内容和面试题专题

**技术栈:** Markdown 文档格式、内核源码分析 (Linux 5.x)、ARM 架构

---

## 实施计划

### Task 1: 创建文档目录结构和章节目录

**Files:**
- Create: `docs/00-table-of-contents.md`
- Create: `docs/chapter00-overview.md`
- Create: `docs/chapter01-kernel-objects.md`
- Create: `docs/chapter02-bus-device-driver.md`
- Create: `docs/chapter03-device-tree.md`
- Create: `docs/chapter04-char-device.md`
- Create: `docs/chapter05-platform-device.md`
- Create: `docs/chapter06-i2c-spi.md`
- Create: `docs/chapter07-advanced-topics.md`
- Create: `docs/chapter08-interview-qa.md`

**Step 1: 创建章节目录文件**

创建 `docs/00-table-of-contents.md`，包含完整的目录结构：

```markdown
# Linux 驱动模型学习指南 - 目录

## 第0章 概述
- [0.1 Linux 驱动模型全景图](./chapter00-overview.md)

## 第1章 底层核心机制
- [1.1 kobject 核心对象](./chapter01-kernel-objects.md#11-kobject-核心对象)
- [1.2 kset 对象集合](./chapter01-kernel-objects.md#12-kset-对象集合)
- [1.3 ktype 对象类型](./chapter01-kernel-objects.md#13-ktype-对象类型)
- [1.4 uevent 事件机制](./chapter01-kernel-objects.md#14-uevent-事件机制)
- [1.5 class 设备类](./chapter01-kernel-objects.md#15-class-设备类)
- [1.6 sysfs 文件系统原理](./chapter01-kernel-objects.md#16-sysfs-文件系统原理)
- [本章面试题](./chapter01-kernel-objects.md#本章面试题)

... (完整目录)
```

**Step 2: 提交变更**

```bash
cd E:/resume/linux_driver_model
git add docs/
git commit -m "docs: 创建文档目录结构"
```

---

### Task 2: 编写第0章 概述

**Files:**
- Modify: `docs/chapter00-overview.md`

**Step 1: 编写章节内容**

```markdown
# 第0章 概述

## 0.1 Linux 驱动模型全景图

### 0.1.1 驱动模型的发展历程

- 早期驱动模型（2.4内核）：直接操作硬件，缺乏统一抽象
- 2.6 内核引入 sysfs 和设备模型
- 设备树的引入：从板级文件到设备树的演进

### 0.1.2 驱动模型的核心组件

1. **kobject/kste/ktype**: 内核对象系统的基础
2. **class**: 设备类抽象
3. **bus**: 总线类型
4. **device**: 设备实例
5. **driver**: 驱动程序

### 0.1.3 驱动模型层次结构

[配图：驱动模型层次结构]

用户空间 → 系统调用 → 字符/块/网络设备驱动 → 总线驱动 → 硬件
                                    ↓
                               设备模型 (sysfs)
                                    ↓
                               kobject/kset/ktype
```

**Step 2: 提交变更**

```bash
git add docs/chapter00-overview.md
git commit -m "docs: 完成第0章概述"
```

---

### Task 3: 编写第1章 底层核心机制 - kobject/kset/ktype

**Files:**
- Modify: `docs/chapter01-kernel-objects.md`

**Step 1: 编写 kobject 核心对象**

```markdown
# 第1章 底层核心机制

## 1.1 kobject 核心对象

### 1.1.1 kobject 概述

kobject 是 Linux 内核设备模型的基础结构体，提供统一的 对象抽象和管理机制。

### 1.1.2 kobject 结构体定义

```c
// include/linux/kobject.h
struct kobject {
    const char              *name;           // 对象名称
    struct list_head        entry;           // 链表节点
    struct kobject         *parent;          // 父对象指针
    struct kset            *kset;            // 所属 kset
    struct kobj_type       *ktype;           // 对象类型
    struct kref            kref;             // 引用计数
    struct sysfs_dirent    *sd;              // sysfs 目录项
    unsigned int state_initialized:1;       // 初始化状态
    unsigned int state_in_sysfs:1;           // 是否在 sysfs 中
    unsigned int state_add_uevent_sent:1;    // uevent 发送状态
    unsigned int state_remove_uevent_sent:1; // uevent 移除状态
    unsigned int uelapsed;                    // 事件耗时
    struct delayed_work    deferred;          // 延迟工作
    ...
};
```

### 1.1.3 kobject 核心函数

1. **kobject_init()** - 初始化 kobject
2. **kobject_add()** - 添加到内核对象层次结构
3. **kobject_del()** - 从层次结构中移除
4. **kobject_get()/kobject_put()** - 引用计数管理

### 1.1.4 kobject 生命周期

[配图：kobject 生命周期]
```

**Step 2: 编写 kset 对象集合**

```markdown
## 1.2 kset 对象集合

### 1.2.1 kset 概述

kset 是 kobject 的集合，提供对一组相关 kobject 的统一管理。

### 1.2.2 kset 结构体定义

```c
// include/linux/kobject.h
struct kset {
    struct list_head    list;                // kobject 链表
    spinlock_t         list_lock;            // 链表锁
    struct kobject     kobj;                 // 内嵌的 kobject
    const struct kset_uevent_ops *uevent_ops; // uevent 回调
};
```

### 1.2.3 kset 与 kobject 的关系

kset 本身包含一个 kobject，kset 下的所有 kobject 形成链表。
```

**Step 3: 编写 ktype 对象类型**

```markdown
## 1.3 ktype 对象类型

### 1.3.1 ktype 概述

ktype 定义了 kobject 的行为属性，包括析构函数、sysfs 属性等。

### 1.3.2 ktype 结构体定义

```c
// include/linux/kobject.h
struct kobj_type {
    void (*release)(struct kobject *kobj);   // 释放函数
    struct sysfs_ops    *sysfs_ops;           // sysfs 操作
    struct attribute   **default_attrs;      // 默认属性
    struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
    const void *(*namespace)(struct kobject *kobj);
    void (*get_ownership)(struct kobject *kobj, kuid_t *uid, kgid_t *gid);
};
```
```

**Step 4: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: 什么是 kobject？它的作用是什么？

**参考答案：**
kobject 是 Linux 内核设备模型的基础结构体，提供统一的面向对象机制...

### 面试题2: kobject 和 kset 的区别是什么？

**参考答案：**
kobject 是单个对象，kset 是一组 kobject 的集合...
```

**Step 5: 提交变更**

```bash
git add docs/chapter01-kernel-objects.md
git commit -m "docs: 完成第1章底层核心机制"
```

---

### Task 4: 编写第1章续 - uevent/class/sysfs

**Files:**
- Modify: `docs/chapter01-kernel-objects.md`

**Step 1: 编写 uevent 事件机制**

```markdown
## 1.4 uevent 事件机制

### 1.4.1 uevent 概述

uevent 是内核向用户空间通知设备状态变化的机制。

### 1.4.2 uevent 触发时机

- 设备添加 (ADD)
- 设备移除 (REMOVE)
- 设备绑定 (BIND)
- 设备解绑 (UNBIND)
- 设备变化 (CHANGE)

### 1.4.3 uevent 实现原理

[代码分析：uevent 的产生和发送流程]
```

**Step 2: 编写 class 设备类**

```markdown
## 1.5 class 设备类

### 1.5.1 class 概述

class 是相同类型设备的抽象，如 input、spi、i2c 等。

### 1.5.2 class 结构体

```c
// include/linux/device.h
struct class {
    const char      *name;
    struct module   *owner;
    struct class_attribute    **class_attrs;
    struct device_attribute   **dev_attrs;
    struct kobject           *dev_kobj;
    int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
    char *(*devnode)(struct device *dev, umode_t *mode);
    void (*class_release)(struct class *class);
    void (*dev_release)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    ...
};
```
```

**Step 3: 编写 sysfs 文件系统原理**

```markdown
## 1.6 sysfs 文件系统原理

### 1.6.1 sysfs 概述

sysfs 是内核对象在用户空间的呈现，通过文件系统方式访问内核对象。

### 1.6.2 sysfs 目录结构

```
/sys
├── block/
├── bus/
│   ├── platform/
│   ├── spi/
│   └── i2c/
├── class/
│   ├── input/
│   ├── leds/
│   └── ...
├── devices/
├── kernel/
│   └── uevent_env
└── fs/
```

### 1.6.3 sysfs 与驱动模型的关系

sysfs 是驱动模型的用户空间接口...
```

**Step 4: 提交变更**

```bash
git add docs/chapter01-kernel-objects.md
git commit -m "docs: 完成第1章 uevent/class/sysfs"
```

---

### Task 5: 编写第2章 总线设备驱动模型

**Files:**
- Modify: `docs/chapter02-bus-device-driver.md`

**Step 1: 编写 device 结构体与设备模型**

```markdown
# 第2章 总线设备驱动模型

## 2.1 device 结构体与设备模型

### 2.1.1 device 结构体定义

```c
// include/linux/device.h
struct device {
    struct device       *parent;         // 父设备
    struct device_private   *p;          // 私有数据
    struct kobject      kobj;            // 内嵌 kobject
    const char          *init_name;      // 初始名称
    const struct device_type *type;      // 设备类型
    struct bus_type     *bus;            // 所属总线
    struct device_driver *driver;        // 绑定驱动
    void                *platform_data;  // 平台数据
    void                *driver_data;    // 驱动私有数据
    struct dev_links_info   links;      // 设备链接
    ...
};
```
```

**Step 2: 编写 bus 总线子系统**

```markdown
## 2.2 bus 总线子系统

### 2.2.1 bus_type 结构体

```c
// include/linux/device.h
struct bus_type {
    const char      *name;               // 总线名称
    const char      *dev_name;           // 设备名称格式
    struct device   *dev_root;           // 根设备
    struct bus_attribute    *bus_attrs;  // 总线属性
    struct device_attribute *dev_attrs;  // 设备属性
    struct driver_attribute *drv_attrs;   // 驱动属性
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    int (*online)(struct device *dev);
    int (*offline)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    ...
};
```

### 2.2.2 match 机制

match 函数是总线匹配设备与驱动的核心...
```

**Step 3: 编写 driver 驱动结构和绑定机制**

```markdown
## 2.3 driver 驱动结构

### 2.3.1 device_driver 结构体

```c
// include/linux/device.h
struct device_driver {
    const char          *name;           // 驱动名称
    struct bus_type     *bus;            // 所属总线
    struct module       *owner;
    const char          *mod_name;       // 模块名
    const struct of_device_id   *of_match_table;  // 设备树匹配表
    const struct acpi_device_id  *acpi_match_table; // ACPI 匹配表
    int (*probe)(struct device *dev);    // 探测函数
    int (*remove)(struct device *dev);   // 移除函数
    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    ...
};
```

## 2.4 device_driver 与 driver 的绑定机制

### 2.4.1 驱动绑定流程

1. 驱动注册 (driver_register)
2. 总线遍历设备 (bus_for_each_dev)
3. 调用 match 函数匹配
4. 调用 probe 函数探测
5. 创建设备节点

### 2.4.2 设备树匹配机制

驱动通过 of_match_table 指定兼容字符串...
```

**Step 4: 编写热插拔与动态设备管理**

```markdown
## 2.5 热插拔与动态设备管理

### 2.5.1 热插拔支持

### 2.5.2 设备生命周期管理
```

**Step 5: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: 描述 Linux 设备模型的层次结构

### 面试题2: 驱动和设备是如何匹配的？
```

**Step 6: 提交变更**

```bash
git add docs/chapter02-bus-device-driver.md
git commit -m "docs: 完成第2章总线设备驱动模型"
```

---

### Task 6: 编写第3章 设备树

**Files:**
- Modify: `docs/chapter03-device-tree.md`

**Step 1: 编写设备树概述与语法**

```markdown
# 第3章 设备树 (Device Tree)

## 3.1 设备树概述与语法

### 3.1.1 设备树发展历史

- 起源：OpenFirmware, SPARC
- ARM 引入：解决板级文件碎片化
- 标准化：DTS (Device Tree Source) / DTB (Device Tree Blob)

### 3.1.2 设备树节点语法

```dts
/ {
    compatible = "arm,board";

    model = "My ARM Board";
    serial = <0x12345678>;

    chosen {
        bootargs = "console=ttyS0,115200";
    };

    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x10000000>;
    };

    soc {
        compatible = "arm,cortex-a9", "simple-bus";

        interrupt-parent = <&gic>;

        gic: interrupt-controller@48200000 {
            compatible = "arm,cortex-a9-gic";
            #interrupt-cells = <3>;
            #address-cells = <1>;
            #size-cells = <1>;
            reg = <0x48200000 0x1000>,
                  <0x48201000 0x100>;
        };
    };
};
```

### 3.1.3 常用设备树属性

- compatible: 匹配字符串
- reg: 地址和大小
- interrupts: 中断信息
- interrupt-parent: 中断父节点
- phandle: 节点引用
- status: 状态 (okay/disabled)
```

**Step 2: 编写设备树在内核中的解析**

```markdown
## 3.2 设备树在内核中的解析

### 3.2.1 解析流程

1. Bootloader 加载 dtb 到内存
2. 内核启动时解析 dtb
3. 创建 of_platform_device
4. 驱动通过 of_device_id 匹配

### 3.2.2 关键 API

```c
// 查找节点
struct device_node *of_find_node_by_path(const char *path);
struct device_node *of_find_compatible_node(...);
struct device_node *of_find_matching_node(...);

// 读取属性
int of_property_read_u32(const struct device_node *np,
                         const char *propname, u32 *out_value);
int of_property_read_string(const struct device_node *np,
                            const char *propname, const char **out_string);

// 获取资源
int of_address_to_resource(struct device_node *dev, int index,
                           struct resource *r);
int of_irq_to_resource(struct device_node *dev, int index,
                       struct resource *r);
```

### 3.2.3 源码分析：of_platform_device_create
```

**Step 3: 编写设备树与 platform 设备**

```markdown
## 3.3 设备树与 platform 设备

### 3.3.1 设备树节点到 platform 设备的转换

设备树节点 → of_platform_device → platform_device

### 3.3.2 驱动匹配方式

1. of_match_table（设备树兼容字符串）
2. platform_device_id（传统方式）
3. ACPI match（ACPI 方式）
```

**Step 4: 编写 Device Tree Overlay**

```markdown
## 3.4 Device Tree Overlay

### 3.4.1 Overlay 概述

Overlay 允许在不修改原始 DTB 的情况下动态添加/修改设备节点。

### 3.4.2 Overlay 使用方法

```bash
# 加载 overlay
echo overlay.dtbo > /sys/kernel/config/device-tree/overlays/overlayid/path

# 卸载
echo 1 > /sys/kernel/config/device-tree/overlays/overlayid/remove
```
```

**Step 5: 编写 dtb 编译与打包**

```markdown
## 3.5 dtb 编译与打包

### 3.5.1 编译工具

```bash
# DTS 编译为 DTB
dtc -I dts -O dtb -o board.dtb board.dts

# 反编译
dtc -I dtb -O dts -o board.dts board.dtb
```

### 3.5.2 内核配置

CONFIG_OF=y
CONFIG_OF_DYNAMIC=y
CONFIG_OF_EARLY_FLATTREE=y
```

**Step 6: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: 设备树中 compatible 属性的作用是什么？

### 面试题2: 设备树和平台设备的绑定过程是怎样的？
```

**Step 7: 提交变更**

```bash
git add docs/chapter03-device-tree.md
git commit -m "docs: 完成第3章设备树"
```

---

### Task 7: 编写第4章 字符设备驱动案例

**Files:**
- Modify: `docs/chapter04-char-device.md`

**Step 1: 编写字符设备驱动框架**

```markdown
# 第4章 字符设备驱动案例

## 4.1 字符设备驱动框架

### 4.1.1 字符设备结构体

```c
// include/linux/cdev.h
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    dev_t dev;
    unsigned int count;
    struct list_head list;
};
```

### 4.1.2 注册流程

1. 分配设备号 (alloc_chrdev_region / register_chrdev_region)
2. 初始化 cdev (cdev_init)
3. 添加 cdev (cdev_add)
4. 创建类 (class_create)
5. 创建设备节点 (device_create)

### 4.1.3 file_operations

```c
static const struct file_operations hello_fops = {
    .owner   = THIS_MODULE,
    .open    = hello_open,
    .read    = hello_read,
    .write   = hello_write,
    .release = hello_release,
};
```
```

**Step 2: 编写 LED 驱动实例**

```markdown
## 4.2 LED 驱动实例

### 4.2.1 硬件描述

LED 连接到 GPIO，GPx 作为输出。

### 4.2.2 驱动代码

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#define LED_NAME        "my_led"
#define LED_CLASS_NAME  "led_class"

static int led_major;
static struct cdev led_cdev;
static struct class *led_class;
static struct device *led_device;

struct led_info {
    int gpio;
    struct device *dev;
};

static int led_open(struct inode *inode, struct file *filp)
{
    struct led_info *info;
    info = kmalloc(sizeof(struct led_info), // 获取 GFP_KERNEL);
    GPIO 并配置为输出
    filp->private_data = info;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *ppos)
{
    // 读取 LED 状态
    return count;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    int value;
    struct led_info *info = filp->private_data;

    copy_from_user(&value, buf, 1);
    gpio_set_value(info->gpio, value);
    return count;
}

static int led_release(struct inode *inode, struct file *filp)
{
    kfree(filp->private_data);
    return 0;
}

static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .read    = led_read,
    .write   = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    dev_t devno;
    int ret;

    // 分配设备号
    ret = alloc_chrdev_region(&devno, 0, 1, LED_NAME);
    led_major = MAJOR(devno);

    // 初始化 cdev
    cdev_init(&led_cdev, &led_fops);
    led_cdev.owner = THIS_MODULE;
    cdev_add(&led_cdev, devno, 1);

    // 创建类
    led_class = class_create(THIS_MODULE, LED_CLASS_NAME);
    led_device = device_create(led_class, NULL, devno, NULL, LED_NAME);

    return 0;
}

static void __exit led_exit(void)
{
    device_destroy(led_class, MKDEV(led_major, 0));
    class_destroy(led_class);
    cdev_del(&led_cdev);
    unregister_chrdev_region(MKDEV(led_major, 0), 1);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("LED Driver");
```

### 4.2.3 添加 sysfs 调试接口

```c
// 在 led_init 中添加
device_create_file(led_device, &dev_attr_led.attr);

// 属性定义
static ssize_t led_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
    struct led_info *info = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", gpio_get_value(info->gpio));
}

static ssize_t led_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    struct led_info *info = dev_get_drvdata(dev);
    int value;
    sscanf(buf, "%d", &value);
    gpio_set_value(info->gpio, value);
    return count;
}

static DEVICE_ATTR(led, 0644, led_show, led_store);
```
```

**Step 3: 编写按键驱动实例**

```markdown
## 4.3 按键驱动实例

### 4.3.1 硬件描述

按键连接到 GPIO，使用中断方式。

### 4.3.2 驱动代码（带中断和 sysfs）

```c
// 简化代码结构
static irqreturn_t button_isr(int irq, void *dev_id)
{
    struct button_info *info = dev_id;
    info->pressed = true;
    wake_up_interruptible(&info->waitq);
    return IRQ_HANDLED;
}
```

### 4.3.3 sysfs 调试接口

提供轮询方式读取按键状态。
```

**Step 4: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: 字符设备和块设备的主要区别是什么？

### 面试题2: 描述字符设备驱动注册的过程。
```

**Step 5: 提交变更**

```bash
git add docs/chapter04-char-device.md
git commit -m "docs: 完成第4章字符设备驱动案例"
```

---

### Task 8: 编写第5章 平台设备驱动案例

**Files:**
- Modify: `docs/chapter05-platform-device.md`

**Step 1: 编写 Platform 驱动模型**

```markdown
# 第5章 平台设备驱动案例

## 5.1 Platform 驱动模型

### 5.1.1 platform 设备与驱动

```c
// include/linux/platform_device.h
struct platform_device {
    const char      *name;
    int             id;
    bool            id_auto;
    struct device   dev;
    u32             num_resources;
    struct resource *resource;

    const struct platform_device_id *id_entry;
    struct platform_driver *driver;
    ...
};

struct platform_driver {
    int (*probe)(struct platform_device *);
    int (*remove)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    int (*suspend)(struct platform_device *, pm_message_t state);
    int (*resume)(struct platform_device *);
    struct device_driver driver;
    const struct platform_device_id *id_table;
    ...
};
```

### 5.1.2 注册流程

```c
// 驱动注册
platform_driver_register(&my_driver);
platform_driver_unregister(&my_driver);

// 设备注册
platform_device_register(&my_device);
platform_device_unregister(&my_device);
```
```

**Step 2: 编写设备树节点到 platform 设备的转换**

```markdown
## 5.2 设备树节点到 platform 设备的转换

### 5.2.1 of_platform_default_bus_init

设备树解析时，自动为每个节点创建 platform_device。

### 5.2.2 驱动匹配

驱动通过 of_match_table 进行匹配：

```c
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-device", },
    { },
};
MODULE_DEVICE_TABLE(of, my_of_match);

static int my_probe(struct platform_device *dev)
{
    struct resource *res;
    int irq;

    // 获取资源
    res = platform_get_resource(dev, IORESOURCE_MEM, 0);
    irq = platform_get_irq(dev, 0);

    // 获取设备树属性
    of_property_read_u32(dev->dev.of_node, "custom-property", &value);

    return 0;
}
```
```

**Step 3: 编写典型 Platform 驱动开发**

```markdown
## 5.3 典型 Platform 驱动开发

### 5.3.1 设备树节点示例

```dts
my_device: my-device@50000000 {
    compatible = "vendor,my-device";
    reg = <0x50000000 0x1000>;
    interrupts = <0 59 4>;
    clocks = <&clk 0>;
    clock-names = "apb";
};
```

### 5.3.2 完整驱动框架
```

**Step 4: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: platform 设备和 platform 驱动的匹配过程

### 面试题2: 设备树节点转换为 platform_device 的过程
```

**Step 5: 提交变更**

```bash
git add docs/chapter05-platform-device.md
git commit -m "docs: 完成第5章平台设备驱动案例"
```

---

### Task 9: 编写第6章 I2C/SPI 总线驱动案例

**Files:**
- Modify: `docs/chapter06-i2c-spi.md`

**Step 1: 编写 I2C 主机驱动与设备驱动**

```markdown
# 第6章 I2C/SPI 总线驱动案例

## 6.1 I2C 主机驱动与设备驱动

### 6.1.1 I2C 核心结构

```c
// include/linux/i2c.h
struct i2c_adapter {
    struct device dev;
    int nr;
    struct i2c_algorithm *algo;
    void *algo_data;
    struct completion dev_released;
    ...
};

struct i2c_algorithm {
    int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
    int (*smbus_xfer)(struct i2c_adapter *adap, u16 addr,
                      unsigned short flags, char read_write,
                      u8 command, int size, union i2c_smbus_data *data);
    u32 (*functionality)(struct i2c_adapter *adap);
    ...
};

struct i2c_driver {
    unsigned int class;
    int (*attach_adapter)(struct i2c_adapter *) __deprecated;
    int (*probe)(struct i2c_client *, const struct i2c_device_id *);
    int (*remove)(struct i2c_client *);
    void (*shutdown)(struct i2c_client *);
    struct device_driver driver;
    const struct i2c_device_id *id_table;
    ...
};

struct i2c_client {
    unsigned short flags;
    unsigned short addr;
    char name[I2C_NAME_SIZE];
    struct i2c_adapter *adapter;
    struct device dev;
    int irq;
    ...
};
```

### 6.1.2 I2C 设备驱动示例

```c
static int my_i2c_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    struct my_data *data;

    data = devm_kzalloc(&client->dev, sizeof(struct my_data), GFP_KERNEL);
    i2c_set_clientdata(client, data);

    // 初始化设备
    return 0;
}

static int my_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static const struct i2c_device_id my_id_table[] = {
    { "my_i2c_device", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, my_id_table);

static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-i2c-device" },
    { }
};
MODULE_DEVICE_TABLE(of, my_of_match);

static struct i2c_driver my_i2c_driver = {
    .driver = {
        .name = "my_i2c",
        .of_match_table = of_match_ptr(my_of_match),
    },
    .probe = my_i2c_probe,
    .remove = my_i2c_remove,
    .id_table = my_id_table,
};

module_i2c_driver(my_i2c_driver);
```
```

**Step 2: 编写 SPI 主机驱动与设备驱动**

```markdown
## 6.2 SPI 主机驱动与设备驱动

### 6.2.1 SPI 核心结构

```c
struct spi_master {
    struct device dev;
    s16 bus_num;
    u16 num_chipselect;
    u8 bits_per_word_mask;
    u32 max_speed_hz;

    int (*setup)(struct spi_device *spi);
    int (*transfer)(struct spi_device *spi, struct spi_message *mesg);
    ...
};

struct spi_device {
    struct device dev;
    struct spi_master *master;
    u32 max_speed_hz;
    u8 chip_select;
    u8 bits_per_word;
    u16 mode;
    const char *modalias;
    int irq;
    ...
};

struct spi_driver {
    int (*probe)(struct spi_device *);
    int (*remove)(struct spi_device *);
    void (*shutdown)(struct spi_device *);
    struct device_driver driver;
    const struct spi_device_id *id_table;
    ...
};
```

### 6.2.2 SPI 设备驱动示例
```

**Step 3: 编写传感器驱动实例**

```markdown
## 6.3 传感器驱动实例

### 6.3.1 温湿度传感器 (I2C)

### 6.3.2 SPI 传感器
```

**Step 4: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: I2C 和 SPI 总线的区别

### 面试题2: 描述 I2C 设备驱动的注册过程
```

**Step 5: 提交变更**

```bash
git add docs/chapter06-i2c-spi.md
git commit -m "docs: 完成第6章I2C/SPI总线驱动案例"
```

---

### Task 10: 编写第7章 进阶话题

**Files:**
- Modify: `docs/chapter07-advanced-topics.md`

**Step 1: 编写电源管理与驱动**

```markdown
# 第7章 进阶话题

## 7.1 电源管理与驱动

### 7.1.1 PM 框架

### 7.1.2 Runtime PM

### 7.1.3 系统睡眠
```

**Step 2: 编写 Linux 5.x 到 6.x 驱动模型变化**

```markdown
## 7.2 Linux 5.x 到 6.x 驱动模型变化

### 7.2.1 主要变化

- device link 的改进
- bus_type 的变化
- 设备属性的变化
```

**Step 3: 编写内核模块签名与安全**

```markdown
## 7.3 内核模块签名与安全

### 7.3.1 模块签名机制

### 7.3.2 CONFIG_MODULE_SIG
```

**Step 4: 编写调试技巧与工具**

```markdown
## 7.4 调试技巧与工具

### 7.4.1 sysfs 调试接口系统性方法

1. 查看设备信息
2. 控制设备状态
3. 调试参数

### 7.4.2 uevent 事件监控与调试

```bash
# 监控 uevent
udevadm monitor

# 查看设备 uevent
udevadm info /dev/sda
cat /sys/block/sda/uevent
```

### 7.4.3 内核调试工具

- dynamic_debug
- tracepoint
- ftrace

### 7.4.4 常见问题与排查思路
```

**Step 5: 添加本章面试题**

```markdown
## 本章面试题

### 面试题1: 驱动中如何实现电源管理？

### 面试题2: 如何调试驱动中的问题？
```

**Step 6: 提交变更**

```bash
git add docs/chapter07-advanced-topics.md
git commit -m "docs: 完成第7章进阶话题"
```

---

### Task 11: 编写第8章 面试题专题解析

**Files:**
- Modify: `docs/chapter08-interview-qa.md`

**Step 1: 编写驱动模型核心面试题**

```markdown
# 第8章 面试题专题解析

## 8.1 驱动模型核心面试题

### 8.1.1 深入理解 kobject

**问题**: 请详细解释 kobject 的作用和实现机制

**参考答案**: ...

### 8.1.2 sysfs 与驱动模型

**问题**: sysfs 在 Linux 设备模型中的作用是什么？

**参考答案**: ...
```

**Step 2: 编写设备树高频面试题**

```markdown
## 8.2 设备树高频面试题

### 8.2.1 设备树匹配机制

### 8.2.2 设备树属性解析
```

**Step 3: 编写驱动开发实战面试题**

```markdown
## 8.3 驱动开发实战面试题

### 8.3.1 字符设备驱动开发

### 8.3.2 Platform 驱动开发
```

**Step 4: 编写调试与性能优化面试题**

```markdown
## 8.4 调试与性能优化面试题

### 8.4.1 驱动调试方法

### 8.4.2 性能优化
```

**Step 5: 提交变更**

```bash
git add docs/chapter08-interview-qa.md
git commit -m "docs: 完成第8章面试题专题解析"
```

---

### Task 12: 更新目录并最终审查

**Files:**
- Modify: `docs/00-table-of-contents.md`

**Step 1: 更新目录链接**

确保所有章节链接正确。

**Step 2: 最终提交**

```bash
git add docs/
git commit -m "docs: 完成Linux驱动模型学习文档"
```

---

## 执行选择

**Plan complete and saved to `docs/plans/2026-03-09-linux-driver-model-design.md`. Two execution options:**

**1. Subagent-Driven (this session)** - I dispatch fresh subagent per task, review between tasks, fast iteration

**2. Parallel Session (separate)** - Open new session with executing-plans, batch execution with checkpoints

**Which approach?**

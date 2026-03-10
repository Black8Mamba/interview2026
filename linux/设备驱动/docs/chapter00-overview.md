# 第0章 概述

Linux 驱动模型是操作系统与硬件设备之间的桥梁，它的设计直接影响着系统的稳定性、可扩展性和可维护性。本章将从宏观角度出发，带你建立对 Linux 驱动模型的全面认知，了解其发展历程、核心组件以及层次结构，为后续深入学习各个模块打下坚实的基础。

## 0.1 Linux 驱动模型全景图

Linux 驱动模型（Linux Driver Model）是 Linux 内核为了统一管理各种硬件设备而建立的一套抽象层机制。它将纷繁复杂的硬件设备纳入一个统一的框架中进行管理，实现了设备的热插拔支持、电源管理、设备分类等高级功能。理解驱动模型的设计思想，对于开发高质量的 Linux 设备驱动至关重要。

### 0.1.1 驱动模型的发展历程

Linux 驱动模型的发展历程是一部持续演进的历史，每一代内核都在前一版本的基础上进行了重大改进。了解这段历史，可以帮助我们理解当前驱动模型的设计取舍和未来发展方向。

#### 早期驱动模型（2.4 内核）：直接操作硬件，缺乏统一抽象

在 Linux 2.4 内核时期，驱动开发采用的是最原始的方式。驱动程序直接与硬件寄存器进行交互，通过内存映射 I/O 或端口 I/O 来控制设备。这种方式虽然直接高效，但带来了诸多问题。

首先是代码复用性差的问题。由于每个驱动都独立实现自己的设备管理逻辑，相同类型的设备驱动之间几乎没有代码共享。例如，假设有多个不同型号的网卡，它们都需要实现类似的打开、关闭、发送、接收等功能，但开发者往往需要为每个型号从头编写这些逻辑。

其次是设备管理混乱。2.4 内核没有统一的设备抽象层，所有的设备节点都依赖于开发者手动创建和管理。系统无法自动追踪哪些设备已经加载、哪些设备已经移除，这就为系统资源的动态管理带来了困难。

再者是热插拔支持薄弱。虽然 Linux 很早就支持某些设备的热插拔，但这种支持往往是硬编码在特定驱动中的，缺乏通用性。用户插入一个 USB 设备后，系统需要依赖用户空间的脚本来完成设备节点的创建和权限设置，这个过程既不可靠也不灵活。

最后是电源管理能力缺失。2.4 内核的驱动几乎不考虑设备的电源状态，无法实现像待机、唤醒这样的高级电源管理功能。这对于移动设备来说是一个重大的缺陷。

```c
// Linux 2.4 内核风格的简单字符设备驱动示例（概念性代码）
// 这种写法直接操作硬件，缺乏任何抽象层

static int my_device_open(struct inode *inode, struct file *file)
{
    // 直接操作硬件寄存器
    volatile unsigned int *hw_reg = (volatile unsigned int *)0x48000000;
    *hw_reg = 0x1;  // 直接打开设备

    printk("Device opened\n");
    return 0;
}

static int my_device_write(struct file *file, const char *buf,
                           size_t count, loff_t *ppos)
{
    // 直接将数据写入硬件
    volatile unsigned int *hw_data = (volatile unsigned int *)0x48000010;
    copy_from_user(hw_data, buf, count);

    return count;
}

// 没有统一的设备结构，没有类概念，所有的都要自己管理
```

这段代码展示了早期驱动开发的典型模式：直接操作物理地址、手动管理设备状态、缺乏错误处理和资源管理机制。虽然这种简单直接的方式在某些场景下仍然有效，但在复杂的现代系统中，这种做法已经难以为继。

#### 2.6 内核引入 sysfs 和设备模型

Linux 2.6 内核的发布标志着驱动开发进入了一个新时代。从 2.6 版本开始，内核引入了 sysfs 文件系统和统一的设备模型，这是驱动架构的一次重大变革。

2002 年，随着 2.6 内核的发布，Greg Kroah-Hartman 等内核开发者引入了 sysfs 文件系统。sysfs 是一个基于内存的文件系统，它将内核中的设备模型导出到用户空间，让用户可以像浏览普通文件系统一样查看和操作系统中的设备。

sysfs 的引入带来了几个革命性的变化。第一是设备层次结构的可视化。在 sysfs 中，设备以树形结构组织，每个设备都有对应的目录，目录中包含了设备的属性文件。通过读取这些属性，用户空间程序可以获取设备的详细信息；通过写入属性，可以动态修改设备的配置。

第二是热插拔机制的标准化。sysfs 与 udev（设备管理器）配合，实现了真正的即插即用。当有新设备插入时，内核在 sysfs 中创建相应的条目，并发送 uevent 事件通知用户空间；udev 收到事件后，根据预定义的规则创建设备节点、设置权限、加载固件等。整个过程完全自动化，无需用户干预。

第三是驱动的注册和绑定机制标准化。2.6 内核引入了总线（bus）、设备（device）、驱动（driver）三个核心概念。每种类型的设备都挂在某条总线上，每条总线都有对应的驱动子系统和设备子系统的管理代码。驱动在注册时会向总线提出匹配申请，当有设备注册时，总线会负责将合适的驱动与设备进行绑定。

```c
// Linux 2.6+ 内核的设备模型示例

// 1. 定义一个平台设备（platform device）
static struct platform_device my_device = {
    .name = "my-device",
    .id = -1,
    .resource = {
        .start = 0x48000000,
        .end = 0x48000010,
        .flags = IORESOURCE_MEM,
    },
    .num_resources = 1,
};

// 2. 定义设备驱动
static int my_driver_probe(struct platform_device *dev)
{
    struct resource *res;
    void __iomem *base;

    // 获取设备的资源（内存区域）
    res = platform_get_resource(dev, IORESOURCE_MEM, 0);
    if (!res)
        return -ENODEV;

    // 映射内存
    base = ioremap(res->start, resource_size(res));
    if (!base)
        return -ENOMEM;

    // 将映射的地址保存到设备私有数据中
    dev_set_drvdata(&dev->dev, base);

    printk("Device probed successfully\n");
    return 0;
}

static int my_driver_remove(struct platform_device *dev)
{
    void __iomem *base = dev_get_drvdata(&dev->dev);
    if (base)
        iounmap(base);

    printk("Device removed\n");
    return 0;
}

static struct platform_driver my_driver = {
    .probe = my_driver_probe,
    .remove = my_driver_remove,
    .driver = {
        .name = "my-device",
    },
};

// 3. 模块加载和卸载时注册/注销驱动
static int __init my_driver_init(void)
{
    return platform_driver_register(&my_driver);
}

static void __exit my_driver_exit(void)
{
    platform_driver_unregister(&my_driver);
}

module_init(my_driver_init);
module_exit(my_driver_exit);
```

这个示例展示了 2.6+ 内核驱动开发的典型模式：使用内核提供的抽象接口（platform_device、platform_driver）、通过资源结构体管理硬件信息、使用 dev_set_drvdata/dev_get_drvdata 管理设备私有数据。这种开发方式大大提高了驱动的可移植性和可维护性。

#### 设备树的引入：从板级文件到设备树的演进

设备树（Device Tree）是驱动模型发展史上的另一个重要里程碑。它的引入彻底改变了 ARM 架构等嵌入式平台的设备描述方式。

在设备树出现之前，ARM 平台的设备描述是嵌入在代码中的。每当硬件设计发生变化（比如更换一个 GPIO 控制器），开发者都需要修改内核代码中的板级文件（board file）。这种做法的缺点是显而易见的：内核代码与硬件配置紧密耦合，硬件的微小变化都需要重新编译内核。

设备树是一种描述硬件的数据结构，它独立于操作系统，可以用统一的语法（DTS，Device Tree Source）来描述系统的硬件配置。内核在启动时读取设备树 blob（DTB），根据其中的描述来发现和配置硬件设备。这样，同样的内核镜像可以支持不同的硬件平台，只需要提供不同的设备树文件即可。

设备树的概念最初来源于 Open Firmware（OpenBoot），它是 SPARC 架构平台上的标准固件接口。2005 年左右，设备树被引入 Linux 内核，最初主要用于 PowerPC 平台。随着 ARM 架构在嵌入式领域的广泛应用，2011 年，ARM 社区正式决定将设备树作为描述 ARM 硬件的标准方式，取代原有的板级文件。

设备树的引入带来了几个显著的优势。首先是硬件描述与内核代码的解耦。开发者可以在不修改内核代码的情况下，通过修改设备树来支持新的硬件配置或调整现有硬件的参数。这大大加速了嵌入式系统的开发周期。

其次是促进了社区的协作。设备树采用文本格式（DTS）进行描述，便于分享和审查。硬件厂商可以提供设备的设备树绑定（Device Tree Binding）文档，社区开发者可以据此编写和调试驱动，而无需了解具体的芯片手册细节。

再者是简化了内核的维护。在此之前，ARM 内核中充斥着大量与特定开发板相关的代码，这些代码难以维护且随着时间推移越来越混乱。设备树将硬件描述从内核代码中分离出来，使得内核代码可以专注于驱动逻辑的实现，硬件配置则由设备树负责。

```dts
// 设备树示例：描述一个简单的 LED 设备

/ {
    compatible = " vendor,my-board";
    model = "My Board Description";

    // 描述一个 GPIO LED
    leds {
        compatible = "gpio-leds";

        status-led {
            label = "status";
            gpios = <&gpio1 0 GPIO_ACTIVE_HIGH>;
            default-state = "on";
        };

        error-led {
            label = "error";
            gpios = <&gpio1 1 GPIO_ACTIVE_HIGH>;
            default-state = "off";
        };
    };

    // 描述一个 I2C 温度传感器
    i2c1 {
        compatible = "i2c-gpio";
        scl-gpios = <&gpio2 5 (GPIO_ACTIVE_HIGH | GPIO_OPEN_DRAIN)>;
        sda-gpios = <&gpio2 4 (GPIO_ACTIVE_HIGH | GPIO_OPEN_DRAIN)>;

        #address-cells = <1>;
        #size-cells = <0>;

        temp-sensor@48 {
            compatible = "manufacturer,temp-sensor";
            reg = <0x48>;
            interrupt-parent = <&gpio1>;
            interrupts = <2 IRQ_TYPE_EDGE_FALLING>;
        };
    };
};
```

这个设备树示例展示了典型的硬件描述方式：使用节点（node）描述各个外设，使用属性（property）描述外设的参数和配置，使用引用（phandle）建立节点之间的关联关系。

### 0.1.2 驱动模型的核心组件

Linux 设备模型建立在几个核心组件之上，这些组件相互协作，共同构成了一个层次分明、结构清晰的设备管理体系。理解这些组件的概念和它们之间的关系，是深入学习 Linux 驱动模型的前提。

#### 1. kobject/kset/ktype：内核对象系统的基础

kobject 是 Linux 设备模型的最底层构建块，它是内核对象系统（Kernel Object System）的核心。理解 kobject，对于理解整个设备模型至关重要。

kobject 本质上是一个结构体，它包含了引用计数、名字、所属集合（kset）、类型（ktype）等基本信息。但 kobject 本身很少单独使用，它通常嵌入到其他更大的结构体中，为这些结构体提供统一的管理能力。

```c
// kobject 结构体定义（Linux 5.x 内核源码）
// 文件位置：include/linux/kobject.h

struct kobject {
    const char              *name;           // 对象名称
    struct list_head        entry;           // 链表节点，用于加入 kset
    struct kobject         *parent;         // 父对象指针，形成层次结构
    struct kset            *kset;            // 所属的 kset 集合
    struct kobj_type       *ktype;          // 对象的类型定义
    struct kref            kref;             // 引用计数
    struct sysfs_dirent    *sd;              // 对应的 sysfs 目录项
    unsigned int state_initialized:1;        // 初始化状态
    unsigned int state_in_sysfs:1;           // 是否已在 sysfs 中显示
    unsigned int state_add_uevent_sent:1;    // 是否已发送添加 uevent
    unsigned int state_remove_uevent_sent:1;// 是否已发送移除 uevent
    unsigned int uevent_suppress:1;          // 是否压制 uevent 事件
};
```

kobject 的主要功能包括以下几个方面。首先是引用计数管理：通过 kref 机制，kobject 可以追踪对象被引用的次数，当引用计数降为 0 时，对象会被自动释放。这避免了内存泄漏的问题。

其次是层次结构管理：通过 parent 指针和 entry 链表，kobject 可以形成树形的层次结构。这个层次结构直接映射到 sysfs 文件系统的目录结构。

第三是 sysfs 集成：每个 kobject 在 sysfs 中都对应一个目录，目录中包含了对象的属性文件。通过这些属性文件，用户空间可以与内核对象进行交互。

第四是热插拔事件支持：kobject 状态变化时（如添加、移除），可以触发 uevent 事件，通知用户空间的守护进程。

kset 是 kobject 的集合，它本身也包含一个 kobject，因此 ksset 也是一个层次结构中的节点。kset 的主要作用是将相关的 kobject 组织在一起，便于统一管理。例如，所有的块设备都属于 block kset，所有的网络设备都属于 net kset。

```c
// kset 结构体定义
// 文件位置：include/linux/kobject.h

struct kset {
    struct list_head list;              // 包含的所有 kobject 的链表
    spinlock_t list_lock;                // 保护链表的锁
    struct kobject kobj;                 // kset 自身的 kobject
    const struct kset_uevent_ops *uevent_ops;  // 热插拔事件处理函数
};
```

ktype 定义了 kobject 的类型特性，它指定了对象释放时调用的析构函数（release）、默认属性（default_attrs）以及属性操作函数（sysfs_ops）。通过 ktype，开发者可以为不同类型的对象定义不同的行为。

```c
// kobj_type 结构体定义
// 文件位置：include/linux/kobject.h

struct kobj_type {
    void (*release)(struct kobject *kobj);          // 释放函数
    const struct sysfs_ops *sysfs_ops;               // sysfs 操作函数
    struct attribute **default_attrs;                // 默认属性
    const struct kobj_ns_type_oper *ns_type;        // 命名空间类型
    const void *(*namespace)(struct kobject *kobj);  // 命名空间回调
};
```

kobject/kset/ktype 三者的关系可以这样理解：kobject 是基本对象单元，kset 是对象的容器（类似于面向对象中的集合类），ktype 是对象的类型定义（类似于面向对象中的类）。通过这三者的配合，内核构建起了完整的对象管理系统。

#### 2. class：设备类抽象

设备类（class）是对一类设备的抽象，它代表具有相似功能的设备集合。例如，block 类包含所有的块设备，net 类包含所有的网络设备，input 类包含所有的输入设备。

设备类的引入，使得设备的管理更加符合人类的认知习惯。开发者不需要知道设备的具体型号，只需要按照设备类提供的统一接口来操作设备即可。

```c
// class 结构体定义（简化版）
// 文件位置：include/linux/device.h

struct class {
    const char      *name;                // 类名称
    struct module   *owner;                // 所属模块

    struct class_attribute      **class_attrs;     // 类的属性
    struct device_attribute     **class_dev_attrs; // 设备属性
    struct bin_attribute        **class_bin_attrs; // 二进制属性

    int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
    char *(*devnode)(struct device *dev, umode_t *mode);

    void (*class_release)(struct class *class);
    void (*dev_release)(struct device *dev);

    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    const struct dev_pm_ops *pm;

    struct subsys_private *p;
};
```

类（class）提供了几个重要的功能。首先是统一接口：每个类都定义了一组标准的操作接口（如 open、close、read、write），驱动只需要实现这些接口，就可以被系统统一管理。

其次是设备节点创建：类可以在 /sys/class/ 目录下创建对应的目录，并在用户空间创建设备节点（如 /dev 下的设备文件）。这个过程通常由 udev 自动完成。

第三是生命周期管理：类提供了 release 函数，当设备被移除时，类负责执行清理工作。

在实际开发中，开发者通常使用 class_create() 或 class_register() 来创建一个设备类。Linux 内核提供了许多预定义的类，开发者也可以创建自定义的类。

```c
// 创建设备类的示例

// 方法1：使用 class_create（适用于简单场景）
struct class *my_class;
my_class = class_create(THIS_MODULE, "my_device_class");
if (IS_ERR(my_class)) {
    pr_err("Failed to create class\n");
    return PTR_ERR(my_class);
}

// 方法2：使用 class_register（适用于需要更多控制的场景）
static struct class my_class = {
    .name = "my_device_class",
    .owner = THIS_MODULE,
    .dev_release = my_device_release,
};

ret = class_register(&my_class);
```

#### 3. bus：总线类型

总线（bus）是设备模型中连接设备和驱动的桥梁。在 Linux 设备模型中，总线是一种特殊的子系统，它负责管理挂载在它上面的所有设备和驱动，并负责设备和驱动的匹配工作。

常见的总线类型包括：platform 总线（用于描述片上系统中的外设）、PCI 总线（用于连接扩展卡）、USB 总线（用于连接外设）、I2C 总线（用于连接传感器）、SPI 总线（用于连接高速外设）等。

```c
// bus_type 结构体定义（简化版）
// 文件位置：include/linux/device.h

struct bus_type {
    const char      *name;                // 总线名称
    const char      *dev_name;           // 设备名称
    struct device   *dev_root;           // 根设备

    struct bus_attribute     *bus_attrs;        // 总线属性
    struct device_attribute  *dev_attrs;        // 设备属性
    struct driver_attribute  *drv_attrs;        // 驱动属性

    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);

    int (*suspend)(struct device *dev, pm_message_t state);
    int (*suspend_late)(struct device *dev, pm_message_t state);
    int (*resume_early)(struct device *dev);
    int (*resume)(struct device *dev);

    const struct dev_pm_ops *pm;

    struct subsys_private *p;
};
```

总线最核心的功能是设备和驱动的匹配（match）。当有新的设备注册时，总线会遍历所有已注册的驱动，调用 match 函数进行匹配；如果有新的驱动注册，总线会遍历所有已注册的设备进行匹配。匹配成功后，系统会调用驱动的 probe 函数来完成设备的初始化。

```c
// platform 总线的匹配函数示例

static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    // 1. 按设备名称匹配（已废弃，主要用于兼容）
    if (of_driver_match_device(dev, drv))
        return 1;
    if (acpi_driver_match_device(dev, drv))
        return 1;

    // 2. 按设备名称匹配（传统方式）
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    // 3. 按驱动名称匹配（fallback）
    return strcmp(pdev->name, drv->name) == 0;
}
```

这个匹配函数展示了 Linux 设备模型的多层次匹配策略：设备树匹配（of_driver_match_device）、ACPI 匹配（acpi_driver_match_device）、ID 表匹配（pdrv->id_table）、名称匹配。不同的匹配方式有不同的优先级，系统会按照这个顺序依次尝试，直到找到匹配的驱动。

#### 4. device：设备实例

device 结构体代表一个具体的硬件设备。它包含了设备的所有信息，如设备名称、设备 ID、所属总线、所属驱动、设备资源等。

```c
// device 结构体定义（简化版）
// 文件位置：include/linux/device.h

struct device {
    struct device       *parent;          // 父设备
    struct device_private   *p;           // 私有数据

    const char          *init_name;       // 初始名称
    const struct device_type *type;       // 设备类型

    struct bus_type     *bus;             // 所属总线
    struct device_driver *driver;         // 绑定的驱动

    void                *platform_data;   // 平台数据
    void                *driver_data;     // 驱动私有数据

    struct dev_links_info links;           // 设备链接信息

    struct kobject kobj;                   // 内嵌的 kobject
    u32             id;                    // 设备 ID
    unsigned int        is_registered:1;  // 是否已注册
    unsigned int        uevent_suppress:1;

    enum device_state   state;            // 设备状态

    struct attribute_group  **groups;     // 属性组
    void (*release)(struct device *dev);  // 释放函数

    int (*pm)(struct device *dev);        // 电源管理
    const struct dev_pm_ops *pm;          // 电源操作
};
```

device 结构体通过内嵌的 kobject 接入到内核的对象系统中，这使得设备可以参与 sysfs 的层次结构，可以触发 uevent 事件，可以利用引用计数进行生命周期管理。

设备的注册通常使用 device_register() 函数完成。在注册之前，开发者需要初始化 device 结构体的各个字段，特别是 parent（父设备）、bus（所属总线）、release（释放函数）等关键字段。

```c
// 设备注册示例

static struct device my_device = {
    .init_name = "my_device",
    .parent = NULL,  // 可以设置为父设备，如 platform 总线的根设备
    .bus = &platform_bus_type,
    .release = my_device_release,
};

int ret = device_register(&my_device);
if (ret) {
    pr_err("Failed to register device: %d\n", ret);
    return ret;
}
```

#### 5. driver：驱动程序

driver 结构体代表一个设备驱动程序。它包含了驱动的名称、所属总线、驱动所支持的设备列表、以及驱动对设备的具体操作函数。

```c
// device_driver 结构体定义（简化版）
// 文件位置：include/linux/device.h

struct device_driver {
    const char              *name;           // 驱动名称
    struct bus_type         *bus;            // 所属总线

    struct driver_private   *p;               // 私有数据

    const char              *mod_name;       // 模块名称
    struct module           *owner;          // 所属模块

    const struct of_device_id   *of_match_table;  // 设备树匹配表
    const struct acpi_device_id *acpi_match_table; // ACPI 匹配表

    int (*probe)(struct device *dev);        // 探测函数
    int (*remove)(struct device *dev);       // 移除函数
    void (*shutdown)(struct device *dev);    // 关闭函数
    int (*suspend)(struct device *dev, pm_message_t state);  // 挂起
    int (*resume)(struct device *dev);       // 恢复

    const struct dev_pm_ops *pm;            // 电源操作

    struct driver_attribute *drv_attrs;     // 驱动属性
    struct attribute_group **groups;         // 属性组

    int (*bind)(struct device *dev);
    int (*unbind)(struct device *dev);
};
```

驱动结构体的核心是 probe 和 remove 函数。probe 函数在驱动与设备匹配成功后被调用，负责初始化设备、申请资源、注册中断等工作；remove 函数在设备移除或驱动注销时被调用，负责释放资源、注销中断等清理工作。

驱动的注册使用 driver_register() 函数完成。在注册驱动之前，需要初始化 driver 结构体的 name、bus、probe、remove 等字段。

```c
// 驱动注册示例

static struct device_driver my_driver = {
    .name = "my_driver",
    .bus = &platform_bus_type,
    .probe = my_driver_probe,
    .remove = my_driver_remove,
    .of_match_table = my_of_match,
    .pm = &my_driver_pm_ops,
};

static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-device", },
    { },
};

int ret = driver_register(&my_driver);
if (ret) {
    pr_err("Failed to register driver: %d\n", ret);
    return ret;
}
```

### 0.1.3 驱动模型层次结构

Linux 驱动模型采用分层架构，每一层都有明确的职责，层与层之间通过清晰的接口进行通信。这种分层设计使得系统具有良好的可维护性和可扩展性。

```
用户空间 → 系统调用 → 字符/块/网络设备驱动 → 总线驱动 → 硬件
                                    ↓
                               设备模型 (sysfs)
                                    ↓
                               kobject/kset/ktype
```

从下往上分析这个层次结构：

最底层是 kobject/kset/ktype，这是整个设备模型的基础设施层。它们提供了对象管理、引用计数、sysfs 导出、热插拔事件等基础功能。所有的设备、总线、驱动最终都嵌入了一个 kobject，利用这个基础架构进行管理。

设备模型层（sysfs）是承上启下的关键层。它将内核中的设备层次结构映射到用户空间，让用户可以直观地看到系统中有哪些设备，每个设备的状态如何。同时，用户空间可以通过修改 sysfs 中的属性来影响内核设备的状态。

设备驱动层包括字符设备驱动、块设备驱动和网络设备驱动。这是开发者最常接触的一层。字符设备以字节流的方式访问，块设备以数据块的方式访问，网络设备则通过 socket 接口进行访问。这一层直接与总线驱动交互，将对设备的操作转发到具体的硬件。

总线驱动层负责管理特定类型的设备。不同的总线（platform、USB、PCI、I2C、SPI 等）有不同的通信协议和访问方式，总线驱动封装了这些细节，为上层驱动提供统一的接口。

硬件是最底层，代表真实的物理设备。CPU、内存、各种外设控制器、传感器等都是硬件的一部分。

从功能角度，驱动模型可以分为三个子系统：设备子系统（device）、总线子系统（bus）、驱动子系统（driver）。设备子系统负责管理所有的设备实例，记录设备的状态和属性；总线子系统负责设备和驱动的匹配，以及总线的管理；驱动子系统负责驱动的加载、卸载和设备操作。

## 本章面试题

### 面试题1：简述 Linux 驱动模型的发展历程

**参考答案：**

Linux 驱动模型的发展历程可以划分为以下几个主要阶段：

**第一阶段：2.4 内核及以前（直接硬件操作时期）**

在早期 Linux 2.4 内核时期，驱动程序采用最直接的方式与硬件交互。开发者通过内存映射 I/O 或端口 I/O 直接操作硬件寄存器，没有任何抽象层。这种方式虽然简单直接，但存在明显的缺陷：代码复用性差，每个驱动都要从头实现相同的功能；设备管理混乱，缺乏统一的设备抽象和生命周期管理；热插拔支持薄弱，需要依赖用户空间脚本手动处理；电源管理能力缺失，无法实现高级的电源管理功能。

**第二阶段：2.6 内核（sysfs 和统一设备模型时期）**

Linux 2.6 内核引入了革命性的变革——sysfs 文件系统和统一的设备模型。sysfs 将内核中的设备模型导出到用户空间，使得设备管理变得透明可访问。这一时期引入了 kobject/kset/ktype 作为对象系统的基础，设备类（class）、总线（bus）、设备（device）、驱动（driver）等核心抽象概念也在此时确立。设备的热插拔支持、驱动的自动绑定等高级功能得以实现。

**第三阶段：设备树引入（ARM 架构的变革）**

随着 ARM 架构在嵌入式领域的广泛应用，传统的板级文件方式无法满足需求。设备树（Device Tree）被引入 Linux 内核，成为描述硬件的标准方式。设备树将硬件配置从内核代码中分离出来，使得同一份内核代码可以支持不同的硬件平台。这大大简化了嵌入式系统的开发和维护工作。

**第四阶段：现代驱动模型（Linux 5.x 及以后）**

现代 Linux 驱动模型在继承 2.6 时代核心概念的基础上，持续演进。增加了设备树覆盖（Device Tree Overlay）支持，实现了运行时动态修改设备树；完善了 ACPI 支持，使得 x86 平台也能充分利用设备模型的优点；强化了电源管理，引入了更细粒度的电源控制机制；增强了安全机制，包括模块签名、安全启动支持等。

### 面试题2：Linux 设备模型的核心组件有哪些？

**参考答案：**

Linux 设备模型由以下五个核心组件构成：

**1. kobject/kset/ktype（内核对象系统）**

kobject 是整个设备模型的基础构建块，它提供了引用计数、层次结构管理、sysfs 集成、热插拔事件支持等基础功能。kobject 很少单独使用，它通常嵌入到其他更大的结构体中，为这些结构体提供统一的管理能力。

kset 是 kobject 的集合，它本身也包含一个 kobject，因此可以形成层次结构。kset 用于将相关的 kobject 组织在一起进行统一管理。

ktype 定义了 kobject 的类型特性，包括析构函数（release）、默认属性（default_attrs）以及属性操作函数（sysfs_ops）。通过 ktype，不同类型的对象可以有不同的行为。

**2. class（设备类）**

设备类是对一类设备的抽象，代表具有相似功能的设备集合。例如，block 类代表所有块设备，net 类代表所有网络设备。设备类的主要作用是提供统一接口、在 /sys/class/ 下创建目录、配合 udev 自动创建设备节点。

**3. bus（总线）**

总线是连接设备和驱动的桥梁。Linux 中有多种总线类型，如 platform 总线、PCI 总线、USB 总线、I2C 总线、SPI 总线等。总线负责管理挂载在其上的所有设备和驱动，并负责设备和驱动的匹配工作。当新设备注册时，总线会遍历已注册的驱动寻找匹配；当新驱动注册时，总线会遍历已注册的设备寻找匹配。

**4. device（设备）**

device 结构体代表一个具体的硬件设备。它包含了设备的所有信息，如设备名称、设备 ID、所属总线、所属驱动、设备资源等。设备通过内嵌的 kobject 接入到内核的对象系统中，可以参与 sysfs 层次结构，可以触发 uevent 事件。

**5. driver（驱动程序）**

driver 结构体代表一个设备驱动程序。它包含了驱动的名称、所属总线、驱动所支持的设备列表、以及驱动对设备的具体操作函数。最核心的是 probe 和 remove 函数：probe 函数在驱动与设备匹配成功后被调用，负责初始化设备；remove 函数在设备移除或驱动注销时被调用，负责清理工作。

这五个组件相互协作：kobject/kset/ktype 提供底层基础设施，class、bus、device、driver 则在其上构建了丰富的设备管理体系。总线负责设备和驱动的匹配，设备类提供用户空间的统一视图，驱动定义设备的操作方式，设备则是具体硬件的抽象表示。

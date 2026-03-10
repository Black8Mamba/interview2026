# 第2章 总线设备驱动模型

Linux 设备模型的核心在于建立起设备（device）、驱动（driver）和总线（bus）之间的抽象层次关系。这一章我们将深入探讨总线设备驱动模型的核心概念，包括 device 结构体、bus_type 总线抽象、device_driver 驱动抽象，以及设备与驱动之间的绑定机制。理解这些概念是掌握 Linux 驱动开发的关键所在。

## 2.1 device 结构体与设备模型

### 2.1.1 device 结构体定义

在 Linux 设备模型中，`struct device` 是代表系统中每个设备的核心数据结构。它是所有设备驱动的基石，几乎所有的硬件设备在 Linux 内核中都对应一个 device 结构体实例。device 结构体不仅包含设备本身的信息，还包含了设备在驱动模型中的层次关系、引用计数、以及与其他组件的关联。

device 结构体定义位于 Linux 内核源码的 `include/linux/device.h` 文件中。以下是 Linux 5.x 内核中 struct device 的完整定义：

```c
// 文件位置：include/linux/device.h
struct device {
    struct device       *parent;            // 指向父设备的指针，形成设备层次结构
    struct device_private   *p;            // 设备私有数据，包含驱动相关信息
    struct kobject      kobj;              // 内嵌的 kobject，用于对象管理和 sysfs 导出
    const char          *init_name;        // 设备的初始名称
    const struct device_type *type;        // 设备类型定义
    struct bus_type     *bus;              // 设备所在的总线类型
    struct device_driver *driver;          // 绑定到该设备的驱动
    void                *platform_data;    // 平台数据，通常用于存储总线特定的数据
    void                *driver_data;      // 驱动私有数据，通过 dev_get_drvdata() 访问
    struct dev_links_info   links;         // 设备链接信息

    // 电源管理相关成员
    struct dev_pm_info  power;             // 电源管理信息
    struct dev_pm_domain *pm_domain;       // 电源域

    // 调试和诊断相关
    unsigned int        stateInitialized:1;
    unsigned int        state_in_sysfs:1;
    unsigned int        state_add_uevent_sent:1;
    unsigned int        uevent_suppress:1;

    // 设备属性组
    const struct attribute_group **groups;  // 设备属性组

    // 释放回调
    void (*release)(struct device *dev);   // 设备释放函数

    // 初始化和探测
    int (*init)(struct device *dev);       // 设备初始化函数
    int (*probe)(struct device *dev);     // 设备探测函数
    int (*remove)(struct device *dev);    // 设备移除函数

    // 热插拔支持
    struct class            *class;       // 设备所属的类
    const char              *uevent_ops;  // uevent 操作函数

    // DMA 相关
    struct bus_type         *bus;         // 重复定义，实际使用上面的 bus
    struct device_node      *of_node;     // 设备树节点指针
    struct fwnode_handle    *fwnode;      // 固件节点句柄

    // 设备标识
    dev_t                   devt;         // 设备号（字符/块设备）
    u32                     id;           // 设备 ID
    spinlock_t              lock;         // 保护设备的自旋锁

    // 调试
    atomic_t                uevent_ref;   // uevent 引用计数
    ...
};
```

下面对 device 结构体的各个核心成员进行详细解析：

**parent**：指向父设备的指针，用于形成设备的层次结构。在 Linux 设备模型中，设备是以树形结构组织的。例如，一个 USB 控制器下面是 USB 设备，USB 设备下面可能是 USB Hub，Hub 下面才是具体的 USB 设备。这种层次结构直接反映了硬件的物理连接关系。

**p**：指向 device_private 结构体的指针，封装了设备的私有数据。这些私有数据通常包括：
- 驱动信息（driver 指针）
- 设备链表节点
- 设备探测状态
- 延迟绑定相关数据

```c
// 文件位置：include/linux/device_private.h
struct device_private {
    struct device           *device;
    struct klist           knode_class;
    struct list_head       deferred_probe;
    enum device_removable  removable;
    void                    *driver_data;
    ...
};
```

**kobj**：这是 device 结构体中最重要的成员之一。它是设备模型与内核对象系统（kobject）连接的桥梁。通过这个内嵌的 kobject，device 自动获得了以下能力：
- 引用计数管理（通过 kref）
- sysfs 导出（在 /sys/devices/ 下创建目录）
- 热插拔事件支持（uevent）

这正是 Linux 设备模型设计优雅之处：device 只需内嵌一个 kobject，就可以复用整个 kobject 基础设施。

**bus**：指向设备所在总线类型的指针。例如，对于一个 USB 设备，这个成员指向 usb 总线类型。bus 成员使得设备可以与其所在的总线进行交互，获取总线相关的信息和操作。

**driver**：指向绑定到该设备的驱动的指针。当设备与驱动成功匹配后，这个成员会被设置为指向对应的 device_driver 结构体。

**platform_data**：这是一个 void 指针，用于存储平台相关的数据。对于不同总线的设备，platform_data 的内容和含义各不相同。例如，对于 SPI 设备，它可能包含 SPI 配置信息；对于 I2C 设备，它可能包含 I2C 地址等。

**driver_data**：同样是 void 指针，用于存储驱动私有数据。驱动可以使用以下函数来访问这个数据：

```c
// 获取驱动私有数据
static inline void *dev_get_drvdata(const struct device *dev)
{
    return dev->driver_data;
}

// 设置驱动私有数据
static inline void dev_set_drvdata(struct device *dev, void *data)
{
    dev->driver_data = data;
}
```

**of_node**：指向设备树（Device Tree）节点的指针。当设备通过设备树描述时，这个成员指向对应的设备树节点，驱动可以通过它获取设备树中的属性信息。

**class**：指向设备所属类（class）的指针。类代表了一类具有相似功能的设备，如 input、tty、net 等。通过 class，用户空间可以按功能类型来查找和访问设备。

**devt**：dev_t 类型的设备号，用于字符设备和块设备。这个成员标识了设备在系统中的设备节点号（如 /dev/xxx）。

#### device 结构体的典型用法

在实际驱动开发中，我们很少直接创建 struct device 结构体，而是使用更具体的设备结构体（如 struct platform_device、struct usb_device 等），这些结构体都内嵌了 struct device。以下是创建一个简单设备并注册的示例：

```c
// 定义一个包含 device 的自定义设备结构体
struct my_device {
    struct device dev;         // 必须作为第一个成员
    const char *name;
    int irq;
    void __iomem *regs;
};

static void my_device_release(struct device *dev)
{
    struct my_device *my_dev = to_my_device(dev);
    pr_info("Releasing device: %s\n", my_dev->name);
    kfree(my_dev);
}

static int my_device_probe(struct device *dev)
{
    struct my_device *my_dev = to_my_device(dev);
    pr_info("Probing device: %s\n", my_dev->name);
    // 初始化设备
    return 0;
}

static struct device_type my_device_type = {
    .name = "my_device",
    .release = my_device_release,
};

static int __init my_device_init(void)
{
    struct my_device *my_dev;
    int ret;

    // 分配设备内存
    my_dev = kzalloc(sizeof(*my_dev), GFP_KERNEL);
    if (!my_dev)
        return -ENOMEM;

    // 初始化设备
    my_dev->name = "my_device";
    my_dev->irq = -1;
    my_dev->dev.type = &my_device_type;
    my_dev->dev.parent = NULL;        // 可以设置为父设备
    my_dev->dev.bus = &platform_bus_type;  // 指定总线类型
    my_dev->dev.release = my_device_release;

    // 设置设备名称
    dev_set_name(&my_dev->dev, "my_dev");

    // 注册设备
    ret = device_register(&my_dev->dev);
    if (ret) {
        pr_err("Failed to register device: %d\n", ret);
        kfree(my_dev);
        return ret;
    }

    pr_info("Device registered successfully\n");
    return 0;
}
```

### 2.1.2 device 的层次结构

Linux 设备模型中的设备并非孤立存在，而是形成了一个复杂的层次结构。这个层次结构直接反映了硬件系统的物理拓扑，同时也为设备管理提供了清晰的组织方式。理解设备的层次结构对于调试设备问题、理解设备之间的依赖关系至关重要。

#### sysfs 中的设备层次

在 sysfs 文件系统中，设备的层次结构以目录树的形式直观呈现：

```
/sys/
├── devices/                    # 所有设备的根目录
│   ├── platform/              # 平台总线设备
│   │   ├── rtc@00000000/     # RTC 设备
│   │   ├── i2c-gpio.0/       # GPIO I2C 适配器
│   │   └── ...
│   ├── pci0000:00/            # PCI 根总线
│   │   ├── 0000:00:1f.2/     # PCI 设备
│   │   │   └── ata1/         # AHCI 控制器
│   │   └── ...
│   └── system/                # 系统设备
│       ├── cpu/               # CPU 设备
│       └── memory/            # 内存设备
│
├── class/                     # 按类组织的设备视图
│   ├── input/                 # 输入设备
│   ├── leds/                  # LED 设备
│   ├── net/                   # 网络设备
│   └── ...
│
└── bus/                       # 按总线组织的视图
    ├── platform/
    │   ├── devices/           # 平台总线设备（链接）
    │   └── drivers/           # 平台总线驱动
    ├── usb/
    │   ├── devices/
    │   └── drivers/
    ├── pci/
    │   ├── devices/
    │   └── drivers/
    └── ...
```

#### 设备层次结构的形成

设备层次结构的形成主要通过以下几种方式：

**1. 父子设备关系（parent 指针）**

每个 device 结构体都有一个 parent 指针，指向其父设备。在注册设备时，可以通过设置 parent 来建立设备之间的父子关系：

```c
// 文件位置：drivers/base/core.c
int device_register(struct device *dev)
{
    device_initialize(dev);
    return device_add(dev);
}

void device_initialize(struct device *dev)
{
    dev->kobj.kset = devices_kset;  // 设置默认 kset
    kobject_init(&dev->kobj, &device_ktype);
    INIT_LIST_HEAD(&dev->p->node);
    INIT_LIST_HEAD(&dev->p->deferred_probe);
    dev->release = device_release;
    device_pm_init(dev);
    ...
}
```

**2. 总线拓扑（bus 层级）**

设备通常按照其所连接的总线形成层次结构。以 USB 为例：

```
USB 主控制器 (PCI 设备)
    └── USB 核心
        └── USB Hub
            └── USB 设备（如键盘、鼠标）
```

在 Linux 中，这种关系通过以下方式建立：
- USB 主控制器是一个 PCI 设备
- USB 核心是平台设备
- USB Hub 是 USB 总线上的设备
- 其他 USB 设备是 USB Hub 的子设备

**3. 类归属（class 成员）**

设备可以同时属于某个类（class），这提供了另一种组织视角。例如，一个网络设备既位于 /sys/devices/ 的层次结构中，又可以通过 /sys/class/net/ 访问。

```c
// 设备添加到类
static int device_add_class_symlinks(struct device *dev)
{
    int error;

    if (!dev->class)
        return 0;

    // 在 /sys/class/<class_name>/ 下创建设备链接
    error = sysfs_create_link(&dev->class->p->class_devices.kobj,
                  &dev->kobj, dev_name(dev));
    if (error)
        return error;

    // 创建到设备的符号链接（class -> device）
    error = sysfs_create_link(&dev->kobj,
                  &dev->class->p->class_devices.kobj, "subsystem");
    ...
}
```

#### 设备查找与遍历

Linux 设备模型提供了多种方式来查找和遍历设备：

**1. 根据名称查找设备**

```c
// 文件位置：drivers/base/core.c
struct device *device_find_child(struct device *parent, void *data,
                 int (*match)(struct device *dev, void *data))
{
    struct klist_iter i;
    struct device *dev;

    if (!parent)
        return NULL;

    klist_iter_init(&parent->p->children, &i);
    while ((dev = next_device(&i)) != NULL)
        if (match(dev, data) && get_device(dev))
            break;
    klist_iter_exit(&i);

    return dev;
}
```

**2. 根据设备号查找设备**

```c
// 根据 dev_t 查找设备
struct device *find_device_by_devt(dev_t devt)
{
    struct device *dev;
    dev = bus_find_device_by_devt(&platform_bus_type, devt);
    if (!dev)
        dev = bus_find_device_by_devt(&pci_bus_type, devt);
    // ... 其他总线
    return dev;
}
```

**3. 遍历特定总线上的所有设备**

```c
// 遍历平台总线上的所有设备
static int print_platform_devices(void)
{
    struct device *dev;
    struct bus_type *bus = &platform_bus_type;

    // 使用 bus_for_each_dev 遍历
    bus_for_each_dev(bus, NULL, &dev, {
        printk("Platform device: %s\n", dev_name(dev));
    });

    return 0;
}
```

#### 设备生命周期的管理

设备的生命周期从创建开始，经历注册、使用，最终到注销。每个阶段都有对应的 API：

**1. 设备创建**

```c
// 方法1：使用 device_create() 动态创建设备
struct device *device_create(struct class *class, struct device *parent,
                 dev_t devt, void *drvdata, const char *fmt, ...);

// 方法2：直接分配和初始化设备结构体
struct my_device {
    struct device dev;
    // 私有数据
};
struct my_device *my_dev = kzalloc(sizeof(*my_dev), GFP_KERNEL);
device_initialize(&my_dev->dev);
```

**2. 设备注册**

```c
// 注册设备到设备模型
int device_register(struct device *dev);

// 简化版本：一步完成创建和注册
int device_add(struct device *dev);
```

**3. 设备注销**

```c
// 从设备模型中移除设备
void device_unregister(struct device *dev);

// 内部实现
void device_del(struct device *dev)
{
    // 从 sysfs 中移除
    if (dev->class) {
        sysfs_remove_link(&dev->class->p->class_devices.kobj,
                  dev_name(dev));
        sysfs_remove_link(&dev->kobj, "subsystem");
    }

    // 从设备链表中移除
    device_remove_file(dev, &dev_attr_uevent);
    device_remove_properties(dev);

    // 从总线上移除
    if (dev->bus)
        bus_probe_device(dev);

    // 发送移除 uevent
    kobject_uevent(&dev->kobj, KOBJ_REMOVE);

    // 清理引用
    put_device(dev);
}
```

**4. 设备释放**

当设备的引用计数降为 0 时，会调用 release 函数释放设备资源：

```c
// 定义设备的 release 函数
static void my_device_release(struct device *dev)
{
    struct my_device *my_dev = to_my_device(dev);

    // 释放 IO 区域
    if (my_dev->regs)
        iounmap(my_dev->regs);

    // 释放中断
    if (my_dev->irq >= 0)
        free_irq(my_dev->irq);

    // 释放设备结构体
    kfree(my_dev);
}
```

## 2.2 bus 总线子系统

### 2.2.1 bus_type 结构体

在 Linux 设备模型中，总线（bus）是连接设备和驱动的桥梁。bus_type 结构体抽象了总线类型的概念，它定义了同一种总线的公共属性和行为。无论是物理总线（如 PCI、USB、I2C、SPI）还是虚拟总线（如 platform 总线），都可以用 bus_type 来表示。

bus_type 结构体定义位于 Linux 内核源码的 `include/linux/device.h` 文件中：

```c
// 文件位置：include/linux/device.h
struct bus_type {
    const char      *name;                // 总线名称，如 "usb", "pci", "platform"
    const char      *dev_name;            // 设备名称前缀
    struct device   *dev_root;            // 总线根设备

    // 属性定义
    struct bus_attribute    *bus_attrs;   // 总线属性
    struct device_attribute *dev_attrs;   // 设备属性（该总线上的设备）
    struct driver_attribute *drv_attrs;    // 驱动属性（该总线上的驱动）

    // 核心回调函数
    int (*match)(struct device *dev, struct device_driver *drv);   // 设备-驱动匹配
    int (*probe)(struct device *dev);       // 设备探测
    int (*remove)(struct device *dev);      // 设备移除
    void (*shutdown)(struct device *dev);  // 设备关闭

    // 电源管理回调
    int (*online)(struct device *dev);      // 设备上线
    int (*offline)(struct device *dev);    // 设备离线
    int (*suspend)(struct device *dev, pm_message_t state);  // 挂起
    int (*resume)(struct device *dev);     // 恢复

    // DMA 相关
    int (*dma_configure)(struct device *dev);

    // 总线探测和初始化
    int (*setup)(struct device *dev);       // 设备初始化
    int (*dx_probe)(struct device *dev);   // 深度探测

    // uevent 支持
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);

    // 总线生命周期管理
    int (*bus_register)(struct bus_type *bus);
    void (*bus_unregister)(struct bus_type *bus);

    // 内部数据结构
    struct bus_type_private *p;            // 总线私有数据

    // 热插拔支持
    struct subsys_private *subsys;

    // 电源域
    struct dev_pm_domain *pm_domain;

    ...
};
```

下面对 bus_type 结构体的核心成员进行详细解析：

**name**：总线的名称，这是最关键的成员。每个总线类型都有一个唯一的名称，如 "usb"、"pci"、"platform"、"spi"、"i2c" 等。这个名称用于在 sysfs 中创建总线目录，例如 /sys/bus/usb/。

**dev_name**：设备名称的前缀。某些总线（如 USB）使用总线特定的设备命名方式，这个前缀用于构建设备名称。

**dev_root**：指向总线根设备的指针。对于某些有层级结构的总线（如 USB Host 控制器），根设备是总线的入口点。

**bus_attrs**：总线级别的属性数组。这些属性对应 /sys/bus/<bus_name>/ 目录下的文件。

**dev_attrs**：该总线上设备的默认属性。当设备注册到该总线时，这些属性会自动添加到设备中。

**drv_attrs**：该总线上驱动的默认属性。当驱动注册到该总线时，这些属性会自动添加到驱动中。

**match**：这是总线最核心的回调函数，用于匹配设备与驱动。当有新设备或新驱动注册时，总线会遍历已有的驱动或设备，调用 match 函数进行匹配。match 函数返回非零值表示匹配成功。

**probe**：设备探测函数。当设备与驱动成功匹配后，会调用驱动的 probe 函数（或总线的 probe 函数）来初始化设备。

**remove**：设备移除函数。当设备从总线上移除时调用。

**suspend/resume**：电源管理相关的回调函数，用于设备的挂起和恢复。

**uevent**：总线特定的 uevent 处理函数，可以在发送 uevent 事件前修改环境变量。

**p**：指向 bus_type_private 的指针，包含总线的内部实现细节，如设备链表、驱动链表、sysfs 目录等。

#### 常见总线类型定义

Linux 内核中定义了多种总线类型，以下是一些常见总线的定义：

**1. platform 总线**

```c
// 文件位置：drivers/base/platform.c
struct bus_type platform_bus_type = {
    .name       = "platform",
    .dev_attrs  = platform_dev_attrs,
    .match      = platform_match,
    .uevent     = platform_uevent,
    .probe      = platform_probe,
    .remove     = platform_remove,
    .suspend    = platform_suspend,
    .resume     = platform_resume,
    .num_attrs  = -1,  // 使用默认属性
};
```

**2. PCI 总线**

```c
// 文件位置：drivers/pci/bus.c
struct bus_type pci_bus_type = {
    .name       = "pci",
    .match      = pci_bus_match,
    .uevent     = pci_uevent,
    .probe      = pci_device_probe,
    .remove     = pci_stop_dev,
    .suspend    = pci_pme_active,
    .resume     = pci_restore_dev_state,
    .num_attrs  = -1,
};
```

**3. USB 总线**

```c
// 文件位置：drivers/usb/core/driver.c
struct bus_type usb_bus_type = {
    .name       = "usb",
    .match      = usb_device_match,
    .probe      = usb_probe_device,
    .disconnect = usb_unbind_and_disconnect,
    .suspend    = usb_suspend_device,
    .resume     = usb_resume_device,
    .num_attrs  = -1,
};
```

#### 注册新的总线类型

在内核模块中可以注册新的总线类型：

```c
static struct bus_type my_bus_type = {
    .name = "my_bus",
    .match = my_bus_match,
    .probe = my_bus_probe,
    .remove = my_bus_remove,
};

static int __init my_bus_init(void)
{
    int ret;

    // 注册总线
    ret = bus_register(&my_bus_type);
    if (ret) {
        pr_err("Failed to register my_bus: %d\n", ret);
        return ret;
    }

    pr_info("my_bus registered successfully\n");
    return 0;
}

static void __exit my_bus_exit(void)
{
    bus_unregister(&my_bus_type);
}

module_init(my_bus_init);
module_exit(my_bus_exit);
```

### 2.2.2 match 机制

match 机制是 Linux 设备模型中最核心的机制之一，它负责将设备与驱动进行匹配。当设备或驱动注册到总线时，内核会自动触发匹配过程，只有匹配成功的设备与驱动才能绑定在一起工作。

#### match 函数的原型

```c
// 文件位置：include/linux/device.h
int (*match)(struct device *dev, struct device_driver *drv);
```

match 函数接收两个参数：
- **dev**：指向要匹配的设备
- **drv**：指向要匹配的驱动

返回值为 0 表示不匹配，非零表示匹配成功。

#### 匹配策略

Linux 设备模型支持多种匹配策略，不同的总线类型可以实现不同的匹配逻辑。以下是常见的匹配策略：

**1. 名称匹配**

最简单的匹配方式是将设备名称与驱动名称进行比较：

```c
// 文件位置：drivers/base/platform.c
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    /* 驱动有 of_match_table，优先使用设备树匹配 */
    if (pdrv->driver.of_match_table)
        return of_driver_match_device(dev, pdrv->driver.of_match_table);

    /* 按 ID 表匹配 */
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    /* 按名称匹配（兼容老式驱动） */
    return strcmp(pdev->name, drv->name) == 0;
}
```

**2. 设备树（OF）匹配**

现代 ARM 和嵌入式系统普遍使用设备树来描述硬件。设备树匹配通过比较驱动的 of_match_table 与设备的 compatible 属性：

```c
// 文件位置：drivers/of/device.c
int of_driver_match_device(struct device *dev,
               const struct of_device_id *match)
{
    const struct of_device_id *matches;

    if (!match)
        return 0;

    matches = dev->of_node ? match->data : NULL;
    if (!matches)
        return 0;

    return of_match_device(matches, dev->of_node) != NULL;
}

// 文件位置：drivers/of/device.c
const struct of_device_id *of_match_device(const struct of_device_id *matches,
                         const struct device_node *dev)
{
    if (!matches)
        return NULL;

    while (matches->name[0] || matches->type[0] || matches->compatible[0]) {
        int match = 0;

        if (matches->name[0])
            match = of_node_name_eq(dev, matches->name);
        if (!match && matches->type[0])
            match = of_node_type_eq(dev, matches->type);
        if (!match && matches->compatible[0])
            match = of_device_is_compatible(dev, matches->compatible);

        if (match)
            return matches;

        matches++;
    }

    return NULL;
}
```

设备树匹配通常是最优先的匹配方式，因为它提供了最精确的硬件描述。

**3. ACPI 匹配**

对于 x86 平台，ACPI（高级配置和电源接口）也提供了设备描述信息：

```c
// 文件位置：drivers/acpi/device.c
static int acpi_device_match(struct device *dev, struct device_driver *drv)
{
    struct acpi_device *acpi_dev = to_acpi_device(dev);
    struct acpi_driver *acpi_drv = to_acpi_driver(drv);
    const struct acpi_device_id *acpi_match;

    /* 匹配 ACPI ID 表 */
    if (acpi_drv->ids) {
        for (acpi_match = acpi_drv->ids;
             acpi_match->id[0] || acpi_match->cls[0];
             acpi_match++) {
            if (acpi_match_id(acpi_match, acpi_dev))
                return 1;
        }
    }

    return 0;
}
```

**4. ID 表匹配**

驱动可以定义一个设备 ID 表，列出它支持的所有设备：

```c
// 驱动定义 ID 表
static const struct platform_device_id my_id_table[] = {
    { "my-device-1", 0 },
    { "my-device-2", 1 },
    { },
};

// 在 match 函数中使用
static int platform_match_id(const struct platform_device_id *id,
                struct platform_device *dev)
{
    int i;
    for (i = 0; id[i].name[0]; i++) {
        if (strcmp(dev->name, id[i].name) == 0)
            return &id[i];
    }
    return NULL;
}
```

#### 匹配流程分析

设备与驱动的匹配发生在以下几种情况：

**情况1：驱动注册时**

当驱动注册到总线时，会遍历总线上的所有设备，尝试与驱动匹配：

```c
// 文件位置：drivers/base/driver.c
int driver_register(struct device_driver *drv)
{
    int ret;
    struct device_driver *other;

    other = driver_find(drv->name, drv->bus);
    if (other) {
        pr_err("Driver name already in use\n");
        return -EBUSY;
    }

    ret = bus_add_driver(drv);
    if (ret)
        return ret;

    ret = driver_attach(drv);  // 触发匹配
    if (ret)
        bus_remove_driver(drv);

    return ret;
}

// 文件位置：drivers/base/dd.c
int driver_attach(struct device_driver *drv)
{
    return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
}

// 文件位置：drivers/base/dd.c
static int __driver_attach(struct device *dev, void *data)
{
    struct device_driver *drv = data;
    int ret;

    /* 如果设备已经有驱动，跳过 */
    if (dev->driver)
        return 0;

    /* 尝试匹配设备与驱动 */
    ret = driver_probe_device(drv, dev);
    ...
    return ret;
}
```

**情况2：设备注册时**

当设备注册到总线时，会遍历总线上的所有驱动，尝试与设备匹配：

```c
// 文件位置：drivers/base/core.c
int device_add(struct device *dev)
{
    ...
    bus_probe_device(dev);  // 触发驱动匹配
    ...
}

// 文件位置：drivers/base/dd.c
void bus_probe_device(struct device *dev)
{
    struct bus_type *bus = dev->bus;

    if (!bus)
        return;

    if (bus->p->drivers_autoprobe) {
        ret = device_initial_probe(dev, __driver_probe_device);
    }
}
```

**情况3：热插拔时**

当设备热插拔时，系统会自动进行驱动匹配：

```
设备插入
    ↓
内核检测到新设备
    ↓
创建 device 结构体
    ↓
调用 bus_probe_device()
    ↓
遍历总线上的驱动，调用 match()
    ↓
match 成功，调用 probe()
    ↓
probe 成功，驱动绑定到设备
    ↓
发送 KOBJ_BIND uevent
    ↓
udev 创建设备节点
```

#### 匹配优先级

在实际系统中，可能存在多种匹配方式。为了确保正确的匹配顺序，驱动通常按照以下优先级定义匹配表：

```c
static const struct of_device_id my_driver_of_match[] = {
    { .compatible = "vendor,my-device-v2", },
    { .compatible = "vendor,my-device-v1", },
    { /* sentinel */ }
};

static const struct acpi_device_id my_driver_acpi_match[] = {
    { "VEN0001", 0 },
    { }
};

static struct platform_driver my_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my-driver",
        .of_match_table = my_driver_of_match,  // 设备树匹配（最高优先级）
        .acpi_match_table = my_driver_acpi_match,  // ACPI 匹配
    },
};
```

推荐的匹配方式优先级是：
1. 设备树（Device Tree）匹配 - 现代 ARM/嵌入式系统
2. ACPI 匹配 - x86 平台
3. ID 表匹配 - 传统驱动
4. 名称匹配 - 兼容性考虑

## 2.3 driver 驱动结构

### 2.3.1 device_driver 结构体

在 Linux 设备模型中，`struct device_driver` 是代表设备驱动的核心数据结构。它封装了驱动所必需的所有信息，包括驱动名称、所属总线、匹配表、探测和移除函数等。与 device 结构体类似，device_driver 也内嵌了 kobject，从而可以参与设备模型的对象层次结构。

device_driver 结构体定义位于 Linux 内核源码的 `include/linux/device.h` 文件中：

```c
// 文件位置：include/linux/device.h
struct device_driver {
    const char          *name;           // 驱动名称，必须唯一
    struct bus_type     *bus;           // 驱动所属的总线类型

    // 模块信息
    struct module       *owner;         // 所属模块
    const char          *mod_name;     // 模块名称（用于内置驱动）

    // 匹配表
    const struct of_device_id   *of_match_table;   // 设备树匹配表
    const struct acpi_device_id  *acpi_match_table; // ACPI 匹配表

    // 设备 ID 表
    const struct device_device_id *id_table;

    // 核心回调函数
    int (*probe)(struct device *dev);     // 设备探测函数
    int (*remove)(struct device *dev);   // 设备移除函数
    void (*shutdown)(struct device *dev);  // 设备关闭
    int (*suspend)(struct device *dev, pm_message_t state);  // 挂起
    int (*resume)(struct device *dev);   // 恢复

    // 驱动生命周期
    int (*active_low);                    // 激活状态
    int (*shutdown_child)(struct device *dev);

    // 属性
    struct driver_attribute *drv_attrs;  // 驱动属性
    const struct attribute_group **groups;  // 属性组

    // 电源管理
    struct dev_pm_ops *pm;               // 电源管理操作集

    // 热插拔
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);

    // 内部数据
    struct driver_private *p;             // 驱动私有数据
    struct kobject          kobj;       // 内嵌的 kobject

    // 探测优先级
    int (*probe_err);                    // 探测错误处理

    ...
};
```

下面对 device_driver 结构体的核心成员进行详细解析：

**name**：驱动的名称，这是驱动的唯一标识符。驱动名称在所属总线上必须唯一，并且会被用于在 sysfs 中创建驱动目录（如 /sys/bus/platform/drivers/<driver_name>/）。

**bus**：指向驱动所属总线类型的指针。这个成员指定了驱动将注册到哪条总线上，并决定了驱动可以匹配哪些设备。

**owner**：指向模块的指针，用于引用计数管理。当驱动作为模块编译时，这个成员通常设置为 THIS_MODULE，确保模块在使用期间不会被卸载。

**of_match_table**：设备树匹配表，这是一个数组，以 NULL 结尾。每个数组元素指定一个 compatible 字符串，驱动可以通过设备树中的 compatible 属性来匹配设备。

**acpi_match_table**：ACPI 匹配表，用于 x86 平台通过 ACPI 表匹配设备。

**id_table**：设备 ID 表，这是一个数组，以 NULL 结尾。驱动可以使用这个表来指定它支持的所有设备 ID。

**probe**：设备探测函数，这是驱动中最重要的函数之一。当设备与驱动成功匹配后，系统会调用这个函数来初始化设备。在 probe 函数中，驱动通常会：
- 获取设备的资源（如 I/O 内存、中断等）
- 初始化设备硬件
- 注册设备到系统（如申请设备号、创建字符设备等）
- 设置驱动私有数据

**remove**：设备移除函数，当设备从系统中移除或驱动被卸载时调用。在 remove 函数中，驱动应该释放 probe 中分配的所有资源。

**suspend/resume**：电源管理相关的回调函数，分别在设备挂起和恢复时调用。

**pm**：指向 dev_pm_ops 结构体的指针，提供了更高级的电源管理功能。

**drv_attrs**：驱动属性数组，定义了驱动在 sysfs 中的属性文件。

**groups**：属性组数组，提供了更灵活的属性定义方式。

**p**：指向 driver_private 的指针，包含驱动的内部实现细节。

#### driver_private 结构体

driver_private 封装了驱动的内部状态：

```c
// 文件位置：include/linux/device_private.h
struct driver_private {
    struct kobject kobj;                  // 驱动的 kobject
    struct klist klist_devices;           // 驱动管理的设备链表
    struct klist_node knode_bus;          // 总线链表节点
    struct module *owner;                  // 所属模块
    struct device_driver *driver;          // 指向驱动的指针
};
```

#### 驱动注册与注销

在 Linux 中，驱动需要注册到总线才能生效。以下是驱动注册和注销的基本流程：

**1. 驱动结构体定义**

```c
// 定义平台驱动
static struct platform_driver my_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my-driver",
        .of_match_table = my_of_match,
        .pm = &my_pm_ops,
    },
};
```

**2. 注册驱动**

```c
// 在模块初始化函数中注册驱动
static int __init my_driver_init(void)
{
    int ret;

    ret = platform_driver_register(&my_driver);
    if (ret) {
        pr_err("Failed to register driver: %d\n", ret);
        return ret;
    }

    pr_info("Driver registered successfully\n");
    return 0;
}
```

**3. 注销驱动**

```c
// 在模块退出函数中注销驱动
static void __exit my_driver_exit(void)
{
    platform_driver_unregister(&my_driver);
    pr_info("Driver unregistered\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);
```

#### 完整的驱动示例

以下是一个完整的平台驱动示例，展示了 device_driver 的典型用法：

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

// 设备私有数据结构
struct my_device_data {
    struct platform_device *pdev;
    void __iomem *regs;
    int irq;
    struct my_device_config *cfg;
};

// 设备树匹配表
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-device", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_of_match);

// probe 函数
static int my_probe(struct platform_device *dev)
{
    struct my_device_data *data;
    struct resource *res;
    int ret;

    pr_info("Probing driver for %s\n", dev_name(&dev->dev));

    // 分配私有数据
    data = devm_kzalloc(&dev->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->cfg = dev_get_platdata(&dev->dev);
    data->regs = NULL;
    data->irq = -1;

    // 获取 I/O 内存资源
    res = platform_get_resource(dev, IORESOURCE_MEM, 0);
    if (res) {
        data->regs = devm_ioremap_resource(&dev->dev, res);
        if (IS_ERR(data->regs))
            return PTR_ERR(data->regs);
    }

    // 获取中断资源
    res = platform_get_resource(dev, IORESOURCE_IRQ, 0);
    if (res) {
        data->irq = res->start;
        ret = devm_request_irq(&dev->dev, data->irq, my_irq_handler,
                       IRQF_SHARED, dev_name(&dev->dev), data);
        if (ret)
            return ret;
    }

    // 保存私有数据
    data->pwd = dev;
    dev_set_drvdata(&dev->dev, data);

    // 初始化硬件
    writel(0x1, data->regs + MY_CTRL_REG);

    pr_info("Driver probed successfully\n");
    return 0;
}

// remove 函数
static int my_remove(struct platform_device *dev)
{
    struct my_device_data *data = dev_get_drvdata(&dev->dev);

    pr_info("Removing driver for %s\n", dev_name(&dev->dev));

    // 禁用硬件
    if (data->regs)
        writel(0x0, data->regs + MY_CTRL_REG);

    // 资源会在驱动 detach 时自动释放
    return 0;
}

// 驱动定义
static struct platform_driver my_driver = {
    .driver = {
        .name = "my-device-driver",
        .of_match_table = my_of_match,
    },
    .probe = my_probe,
    .remove = my_remove,
};

module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example Author");
MODULE_DESCRIPTION("Example platform driver");
```

## 2.4 device_driver 与 driver 的绑定机制

### 2.4.1 驱动绑定流程

驱动绑定是 Linux 设备模型中将设备与驱动关联起来的过程。这个过程涉及多个步骤，包括驱动注册、设备探测、匹配和 probe 调用。理解绑定机制对于调试设备驱动问题和开发新驱动都至关重要。

#### 驱动注册的完整流程

当驱动注册到总线时，会触发以下流程：

```
driver_register()
    ↓
bus_add_driver()          // 添加驱动到总线
    ↓
driver_attach()           // 触发设备匹配
    ↓
bus_for_each_dev()        // 遍历总线上的设备
    ↓
__driver_attach()         // 对每个设备尝试匹配
    ↓
driver_probe_device()     // 执行匹配和绑定
    ↓
driver_bind()             // 绑定设备与驱动
    ↓
dev->driver = drv        // 设置驱动指针
    ↓
drv->probe(dev)          // 调用驱动的 probe 函数
```

**1. 驱动注册到总线**

```c
// 文件位置：drivers/base/driver.c
int driver_register(struct device_driver *drv)
{
    int ret;

    // 检查驱动是否已注册
    if (!drv->bus) {
        pr_err("Driver '%s' does not have a bus\n", drv->name);
        return -EINVAL;
    }

    // 检查是否已有同名驱动
    if (drv->bus->p && driver_find(drv->name, drv->bus))
        return -EBUSY;

    // 添加驱动到总线
    ret = bus_add_driver(drv);
    if (ret)
        return ret;

    // 触发驱动与设备的匹配
    ret = driver_attach(drv);
    if (ret)
        bus_remove_driver(drv);

    return ret;
}
```

**2. 驱动附加到设备**

```c
// 文件位置：drivers/base/dd.c
int driver_attach(struct device_driver *drv)
{
    // 遍历总线上的每个设备，尝试匹配
    return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
}

// 文件位置：drivers/base/dd.c
static int __driver_attach(struct device *dev, void *data)
{
    struct device_driver *drv = data;
    int ret;

    // 如果设备已经绑定驱动，跳过
    if (dev->driver)
        return 0;

    // 如果驱动未就绪（需要先 probe），跳过
    if (!drv->p->kobj.subsys)
        return 0;

    // 尝试匹配并绑定
    ret = driver_probe_device(drv, dev);

    return ret ? -ENODEV : 0;
}
```

**3. 设备与驱动的绑定**

```c
// 文件位置：drivers/base/dd.c
static int driver_probe_device(struct device_driver *drv, struct device *dev)
{
    int ret = 0;

    // 检查驱动是否支持此设备
    if (!drv->bus->match || !drv->bus->match(dev, drv))
        return -ENODEV;

    // 调用驱动的 probe 函数
    if (drv->probe) {
        ret = drv->probe(dev);
        if (ret)
            goto probe_failed;
    }

    // 绑定成功
    dev_set_driver(dev, drv);

    // 绑定设备到驱动
    driver_bound(dev);

    return ret;

probe_failed:
    dev_set_driver(dev, NULL);
    return ret;
}

// 文件位置：drivers/base/dd.c
static void driver_bound(struct device *dev)
{
    struct device_driver *drv = dev->driver;

    pr_debug("driver: %s: bound to %s\n", dev_name(dev), drv->name);

    // 将设备添加到驱动的设备链表
    klist_add_tail(&dev->p->knode_driver, &drv->p->klist_devices);

    // 发送 uevent
    kobject_uevent(&dev->kobj, KOBJ_BIND);

    // 如果设备有 class 属性，创建 class 链接
    if (dev->class) {
        device_bind_class(dev);
    }
}
```

#### 设备注册时的绑定流程

当设备注册到总线时，同样会触发驱动匹配：

```
device_add()
    ↓
bus_probe_device()        // 探测设备
    ↓
device_initial_probe()    // 初始探测
    ↓
__driver_probe_device()   // 驱动匹配
    ↓
driver_probe_device()     // 执行绑定（与上面相同）
```

```c
// 文件位置：drivers/base/dd.c
void bus_probe_device(struct device *dev)
{
    struct bus_type *bus = dev->bus;

    if (!bus)
        return;

    // 启用驱动自动探测
    if (bus->p->drivers_autoprobe) {
        ret = device_initial_probe(dev, __driver_probe_device);
    }
}

// 文件位置：drivers/base/dd.c
int device_initial_probe(struct device *dev, bool check_ready)
{
    struct device_private *dev_p = dev->p;
    int ret = 0;

    if (check_ready)
        device_lock(dev);

    dev->driver = NULL;
    dev_p->dead = false;

    ret = __driver_probe_device(dev->driver, dev);

    if (check_ready)
        device_unlock(dev);

    return ret;
}
```

#### 设备节点的创建

设备与驱动成功绑定后，用户空间的 udev 守护进程会收到 KOBJ_ADD 或 KOBJ_BIND 事件，并创建设备节点：

```c
// 绑定成功后的 uevent
static void driver_bound(struct device *dev)
{
    // 发送绑定事件
    kobject_uevent(&dev->kobj, KOBJ_BIND);
}
```

udev 规则通常会：
1. 根据设备的主次设备号创建设备节点（如 /dev/xxx）
2. 设置适当的权限
3. 创建符号链接（如 /dev/input/event0 -> ../event0）

#### 解绑流程

设备与驱动的解绑发生在以下情况：
- 设备从系统中移除
- 驱动被卸载
- 驱动主动解绑设备

```c
// 文件位置：drivers/base/dd.c
int device_driver_detach(struct device *dev)
{
    struct device_driver *drv = dev->driver;
    int ret = 0;

    if (!drv)
        return 0;

    if (drv->remove)
        drv->remove(dev);

    // 发送解绑事件
    kobject_uevent(&dev->kobj, KOBJ_UNBIND);

    // 从驱动的设备链表中移除
    driver_detach(drv);

    dev_set_driver(dev, NULL);

    return ret;
}
```

### 2.4.2 设备树匹配机制

设备树（Device Tree）是描述硬件设备的一种数据结构，最初由 Open Firmware 项目引入，现在广泛用于 ARM、PowerPC 等架构的嵌入式系统中。设备树匹配是 Linux 设备模型中最重要的匹配机制之一，它允许驱动通过设备树节点描述来匹配硬件。

#### 设备树基础

设备树是一个树形数据结构，描述了系统的硬件拓扑：

```dts
/ {
    compatible = "vendor,board";

    soc {
        compatible = "vendor,soc";
        ranges;

        serial0: serial@5000000 {
            compatible = "vendor,uart";
            reg = <0x5000000 0x100>;
            interrupts = <0 1 2>;
            clock-frequency = <115200>;
        };

        gpio0: gpio@6000000 {
            compatible = "vendor,gpio";
            reg = <0x6000000 0x1000>;
            #gpio-cells = <2>;
        };
    };
};
```

#### 设备树匹配表

驱动通过 of_match_table 来指定它支持的设备：

```c
// 文件位置：include/linux/mod_devicetable.h
struct of_device_id {
    char    name[32];           // 设备名称（可选）
    char    type[32];           // 设备类型（可选）
    char    compatible[128];   // 兼容字符串（必须）
    const void *data;           // 匹配后的私有数据
};
```

驱动中的设备树匹配表定义：

```c
static const struct of_device_id my_driver_of_match[] = {
    {
        .compatible = "vendor,my-device-v2",
        .data = &my_device_v2_config,
    },
    {
        .compatible = "vendor,my-device-v1",
        .data = &my_device_v1_config,
    },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_driver_of_match);
```

#### 设备树匹配过程

设备树匹配发生在设备注册时：

```c
// 文件位置：drivers/base/platform.c
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device * pdev = to_platform_device(dev);
    struct platform_driver * pdrv = to_platform_driver(drv);

    /* 1. 优先匹配设备树 */
    if (of_driver_match_device(dev, drv))
        return 1;

    /* 2. 匹配 ID 表 */
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    /* 3. 按名称匹配（向后兼容） */
    return strcmp(pdev->name, drv->name) == 0;
}

// 文件位置：drivers/of/device.c
int of_driver_match_device(struct device *dev,
               const struct of_device_id *match)
{
    const struct of_device_id *matches;

    /* 设备没有设备树节点，无法匹配 */
    if (!dev->of_node)
        return 0;

    /* 驱动没有匹配表，无法匹配 */
    if (!match)
        return 0;

    /* 遍历匹配表 */
    matches = match->data ? match : match->data;
    if (!matches)
        return 0;

    return of_match_device(matches, dev->of_node) != NULL;
}
```

#### 设备树节点的获取

驱动在 probe 函数中可以通过 device 结构体获取设备树节点：

```c
static int my_probe(struct platform_device *dev)
{
    struct device *device = &dev->dev;
    struct device_node *node = device->of_node;
    const char *compatible;
    u32 reg[2];
    int irq;
    int ret;

    /* 获取 compatible 属性 */
    ret = of_property_read_string(node, "compatible", &compatible);
    if (ret) {
        dev_err(device, "Missing compatible property\n");
        return ret;
    }

    /* 获取 reg 属性 */
    ret = of_property_read_u32_array(node, "reg", reg, 2);
    if (ret) {
        devMissing reg property\n");
        return ret;
    }

   _err(device, " /* 获取中断号 */
    irq = of_irq_get(node, 0);
    if (irq < 0) {
        dev_err(device, "Failed to get irq\n");
        return irq;
    }

    /* 获取时钟 */
    struct clk *clk = of_clk_get(node, 0);
    if (IS_ERR(clk))
        return PTR_ERR(clk);

    /* 获取 DMA 通道 */
    struct dma_chan *dma = of_dma_request_slave_channel(node, "rx");
    if (IS_ERR(dma))
        return PTR_ERR(dma);

    /* 获取 GPIO */
    enum gpio_flags flags = GPIO_ACTIVE_HIGH | GPIO_OPEN_DRAIN;
    int gpio = of_get_named_gpio(node, "enable-gpios", 0);
    if (gpio_is_valid(gpio)) {
        ret = gpio_request(gpio, "my-device-enable");
        if (ret)
            return ret;
        gpio_direction_output(gpio, 0);
    }

    return 0;
}
```

#### 常用设备树属性解析 API

Linux 内核提供了丰富的设备树属性解析函数：

```c
// 读取字符串属性
int of_property_read_string(struct device_node *np, const char *propname,
                const char **out_string);

// 读取 u32 整型属性
int of_property_read_u32(struct device_node *np, const char *propname,
              u32 *out_value);

// 读取 u32 数组属性
int of_property_read_u32_array(struct device_node *np, const char *propname,
                   u32 *out_values, size_t sz);

// 读取布尔属性
bool of_property_read_bool(struct device_node *np, const char *propname);

// 读取 u8/u16/u64 数组
int of_property_read_u8_array(struct device_node *np, const char *propname,
                   u8 *out_values, size_t sz);
int of_property_read_u16_array(struct device_node *np, const char *propname,
                   u16 *out_values, size_t sz);
int of_property_read_u64_array(struct device_node *np, const char *propname,
                   u64 *out_values, size_t sz);

// 获取中断号
int of_irq_get(struct device_node *dev, int index);

// 获取 GPIO
int of_get_named_gpio(struct device_node *np, const char *propname, int index);
int of_get_named_gpio_flags(struct device_node *np, const char *propname,
                int index, enum gpio_flags *flags);

// 获取时钟
struct clk *of_clk_get(struct device_node *node, int index);
struct clk *of_clk_get_by_name(struct device_node *node, const char *name);

// 获取 DMA 通道
struct dma_chan *of_dma_request_slave_channel(struct device_node *node,
                        const char *name);

// 获取phy
int of_phy_get(struct device_node *node, const char *binding);
```

#### 完整的设备树驱动示例

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/reset.h>

#define MY_DEVICE_MAX_CLKS 4
#define MY_DEVICE_MAX_GPIOS 4

struct my_device_config {
    u32 version;
    const char *name;
};

struct my_device_private {
    struct platform_device *pwd;
    void __iomem *base;
    int irq;
    struct clk *clks[MY_DEVICE_MAX_CLKS];
    int num_clks;
    struct reset_control *rstc;
    struct gpio_descs *gpios;
    const struct my_device_config *config;
};

static const struct my_device_config my_device_v2_config = {
    .version = 2,
    .name = "my-device-v2",
};

static const struct my_device_config my_device_v1_config = {
    .version = 1,
    .name = "my-device-v1",
};

static const struct of_device_id my_driver_of_match[] = {
    { .compatible = "vendor,my-device-v2", .data = &my_device_v2_config },
    { .compatible = "vendor,my-device-v1", .data = &my_device_v1_config },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_driver_of_match);

static int my_probe(struct platform_device *dev)
{
    struct device *device = &dev->dev;
    struct device_node *node = device->of_node;
    struct my_device_private *priv;
    struct resource *res;
    int i;
    int ret;

    priv = devm_kzalloc(device, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pwd = dev;

    /* 1. 获取匹配配置 */
    if (node) {
        const struct of_device_id *match;
        match = of_match_device(my_driver_of_match, device);
        if (match && match->data)
            priv->config = match->data;
    }

    /* 2. 获取内存资源 */
    res = platform_get_resource(dev, IORESOURCE_MEM, 0);
    priv->base = devm_ioremap_resource(device, res);
    if (IS_ERR(priv->base))
        return PTR_ERR(priv->base);

    /* 3. 获取中断资源 */
    priv->irq = platform_get_irq(dev, 0);
    if (priv->irq > 0) {
        ret = devm_request_irq(device, priv->irq, my_irq_handler,
                       IRQF_ONESHOT, dev_name(device), priv);
        if (ret)
            return ret;
    }

    /* 4. 获取时钟资源 */
    priv->num_clks = of_clk_get_parent_count(node);
    for (i = 0; i < priv->num_clks && i < MY_DEVICE_MAX_CLKS; i++) {
        priv->clks[i] = of_clk_get(node, i);
        if (IS_ERR(priv->clks[i])) {
            ret = PTR_ERR(priv->clks[i]);
            goto err_clk;
        }
        ret = clk_prepare_enable(priv->clks[i]);
        if (ret)
            goto err_clk;
    }

    /* 5. 获取复位控制 */
    priv->rstc = of_reset_control_get(node, NULL);
    if (!IS_ERR(priv->rstc)) {
        reset_control_deassert(priv->rstc);
    }

    /* 6. 获取 GPIO 资源 */
    priv->gpios = devm_gpiod_get_array_optional(node, "enable", GPIOD_OUT_LOW);
    if (IS_ERR(priv->gpios)) {
        ret = PTR_ERR(priv->gpios);
        goto err_gpio;
    }

    /* 7. 获取 DMA 通道 */
    struct dma_chan *dma_rx = of_dma_request_slave_channel(node, "rx");
    struct dma_chan *dma_tx = of_dma_request_slave_channel(node, "tx");

    /* 8. 初始化硬件 */
    writel(0x1, priv->base + MY_DEVICE_CTRL);

    dev_set_drvdata(device, priv);
    return 0;

err_gpio:
    if (!IS_ERR(priv->rstc))
        reset_control_put(priv->rstc);
err_clk:
    for (i = 0; i < priv->num_clks; i++) {
        if (!IS_ERR(priv->clks[i])) {
            clk_disable_unprepare(priv->clks[i]);
            clk_put(priv->clks[i]);
        }
    }
    return ret;
}

static int my_remove(struct platform_device *dev)
{
    struct my_device_private *priv = dev_get_drvdata(&dev->dev);
    int i;

    /* 禁用硬件 */
    writel(0, priv->base + MY_DEVICE_CTRL);

    /* 释放资源 */
    if (!IS_ERR(priv->rstc))
        reset_control_put(priv->rstc);

    for (i = 0; i < priv->num_clks; i++) {
        if (!IS_ERR(priv->clks[i])) {
            clk_disable_unprepare(priv->clks[i]);
            clk_put(priv->clks[i]);
        }
    }

    return 0;
}

static struct platform_driver my_driver = {
    .probe = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my-device-driver",
        .of_match_table = my_driver_of_match,
    },
};

module_platform_driver(my_driver);
```

## 2.5 热插拔与动态设备管理

### 2.5.1 热插拔支持

热插拔（Hot Plug）是指在系统运行时可以添加或移除硬件设备的功能。在 Linux 系统中，热插拔的实现依赖于 uevent 事件机制，它是内核与用户空间进行通信的重要桥梁。

#### uevent 事件机制

uevent（User-space Event）是 Linux 内核向用户空间通知设备状态变化的核心机制。当设备发生添加、移除、绑定、解绑等事件时，内核会通过 netlink 套接字向用户空间发送 uevent 事件，用户空间的 udev 守护进程接收到事件后执行相应的操作。

```c
// uevent 事件类型
enum kobject_action {
    KOBJ_ADD,       // 设备添加
    KOBJ_REMOVE,    // 设备移除
    KOBJ_BIND,      // 驱动绑定到设备
    KOBJ_UNBIND,    // 驱动从设备解绑
    KOBJ_CHANGE,    // 设备状态变化
    KOBJ_MOVE,      // 设备移动
    KOBJ_ONLINE,    // 设备上线
    KOBJ_OFFLINE,   // 设备离线
    KOBJ_MAX
};
```

#### 热插拔事件流程

```
硬件事件（设备插入/移除）
    ↓
总线驱动检测到硬件变化
    ↓
创建/移除 device 结构体
    ↓
device_add() / device_del()
    ↓
kobject_add() / kobject_del()
    ↓
kobject_uevent() 发送 uevent
    ↓
netlink_broadcast() 发送到用户空间
    ↓
udevd 接收事件
    ↓
根据规则文件匹配
    ↓
执行操作（创建设备节点、加载驱动等）
```

#### uevent 事件内容

每个 uevent 事件都包含一组标准的环境变量：

```
ACTION=add                    # 事件类型
DEVPATH=/devices/platform/... # 设备路径
SUBSYSTEM=platform           # 子系统（总线类型）
DEVNAME=my_device            # 设备名称
DRIVER=my_driver             # 驱动名称（如果有）
...
```

驱动可以通过 uevent 回调函数添加自定义的环境变量：

```c
// 在 bus_type 中定义 uevent 回调
static int my_bus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    /* 添加自定义环境变量 */
    if (dev->driver)
        return add_uevent_var(env, "DRIVER=%s", dev->driver->name);

    return 0;
}

static struct bus_type my_bus_type = {
    .name = "my_bus",
    .uevent = my_bus_uevent,
};
```

#### 热插拔的驱动支持

驱动需要正确处理热插拔场景：

```c
static int my_probe(struct device *dev)
{
    int ret;

    /* 初始化硬件 */
    ret = my_hw_init(dev);
    if (ret)
        return ret;

    /* 注册中断处理程序 */
    ret = request_irq(irq, my_isr, IRQF_SHARED, dev_name(dev), my_data);
    if (ret)
        goto err_irq;

    return 0;

err_irq:
    my_hw_deinit(dev);
    return ret;
}

static int my_remove(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);

    /* 释放中断 */
    free_irq(data->irq, my_data);

    /* 关闭硬件 */
    my_hw_deinit(dev);

    return 0;
}
```

### 2.5.2 设备生命周期管理

设备的生命周期从创建开始，经历注册、使用、最终到注销。理解设备生命周期对于编写可靠的驱动至关重要。

#### 设备生命周期状态

设备在生命周期中可能处于以下状态：

```
未初始化 → 初始化 → 已注册 → 已探测 → 已绑定
    ↓                ↓           ↓        ↓
  错误            已删除      探测失败   解绑
```

#### 设备注册与注销

**1. 静态设备（设备信息在内核中硬编码）**

```c
// 定义平台设备
static struct platform_device my_device = {
    .name = "my-device",
    .id = -1,
    .resource = ...,
    .num_resources = ...,
    .dev = {
        .platform_data = &my_device_data,
    },
};

// 在总线初始化时注册设备
static int __init my_bus_init(void)
{
    return platform_device_register(&my_device);
}
```

**2. 动态设备（设备信息从设备树或 ACPI 获取）**

```c
static int my_probe(struct platform_device *dev)
{
    struct device *device = &dev->dev;
    struct device_node *node = device->of_node;
    struct my_device_data *data;

    /* 从设备树动态创建设备 */
    data = devm_kzalloc(device, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    /* 解析设备树节点 */
    of_property_read_u32(node, "vendor,my-reg", &data->reg);
    of_property_read_u32(node, "vendor,my-irq", &data->irq);

    /* 保存配置 */
    dev_set_platdata(device, data);

    return 0;
}
```

#### 延迟绑定机制

在实际系统中，设备可能在驱动加载之前就已经存在。Linux 设备模型提供了延迟绑定机制来处理这种情况：

```c
// 文件位置：drivers/base/dd.c
static LIST_HEAD(deferred_probe_pending);
static LIST_HEAD(deferred_probe_active);

static void driver_deferred_probe_force_trigger(void)
{
    struct device_private *dev_priv;
    struct device *dev;

    /* 强制探测延迟列表中的设备 */
    list_for_each_entry(dev_priv, &deferred_probe_pending, deferred_probe) {
        dev = dev_priv->device;
        device_unlock(dev);
        driver_probe_device(dev->driver, dev);
        device_lock(dev);
    }
}
```

驱动可以使用 deferred_probe 机制来处理依赖：

```c
static int my_probe(struct platform_device *dev)
{
    struct resource *res;

    res = platform_get_resource(dev, IORESOURCE_MEM, 0);
    if (!res) {
        /* 资源不可用，加入延迟探测队列 */
        dev_err(&dev->dev, "Memory resource not available, deferring probe\n");
        return -EPROBE_DEFER;
    }

    /* 继续正常探测 */
    ...
}
```

#### 引用计数管理

设备使用引用计数来管理生命周期：

```c
// 增加设备引用
struct device *get_device(struct device *dev)
{
    return dev ? kobj_get(&dev->kobj) : NULL;
}

// 减少设备引用
void put_device(struct device *dev)
{
    if (dev)
        kobject_put(&dev->kobj);
}
```

驱动应该正确管理设备引用：

```c
static int my_probe(struct platform_device *dev)
{
    struct device *dev_ref;

    /* 获取设备引用 */
    dev_ref = get_device(&dev->dev);
    if (!dev_ref)
        return -ENODEV;

    /* ... 使用设备 ... */

    /* 使用完毕后释放引用 */
    put_device(dev_ref);

    return 0;
}
```

#### 设备资源的自动管理

Linux 设备模型提供了多种资源自动管理机制，减少内存泄漏的风险：

```c
static int my_probe(struct platform_device *dev)
{
    struct device *device = &dev->dev;
    void __iomem *base;
    struct clk *clk;
    struct gpio_desc *gpio;
    int irq;

    /* 使用 devm_* 函数自动管理资源 */
    /* 这些资源会在驱动移除时自动释放 */

    /* 内存映射 */
    base = devm_ioremap_resource(device,
        platform_get_resource(dev, IORESOURCE_MEM, 0));
    if (IS_ERR(base))
        return PTR_ERR(base);

    /* 时钟 */
    clk = devm_clk_get(device, NULL);
    if (IS_ERR(clk))
        return PTR_ERR(clk);

    /* GPIO */
    gpio = devm_gpiod_get_optional(device, "enable", GPIOD_OUT_LOW);
    if (IS_ERR(gpio))
        return PTR_ERR(gpio);

    /* 中断 */
    irq = platform_get_irq(dev, 0);
    if (irq > 0) {
        int ret = devm_request_irq(device, irq, my_isr,
                      IRQF_ONESHOT, dev_name(device), priv);
        if (ret)
            return ret;
    }

    return 0;
}

/* 无需在 remove 中显式释放资源，devm 会自动处理 */
static int my_remove(struct platform_device *dev)
{
    /* 硬件会被自动禁用，资源会被自动释放 */
    return 0;
}
```

## 本章面试题

### 面试题1：描述 Linux 设备模型的层次结构

**参考答案：**

Linux 设备模型采用层次化的结构来组织系统中的所有设备。这种层次结构主要通过以下几个关键组件实现：

**1. 设备层次结构（Device Hierarchy）**

Linux 设备模型以树形结构组织所有设备，每个设备都有父设备（parent）和可能的子设备（children）。例如：
- USB 主控制器是 USB 设备的父设备
- USB Hub 是连接在其上的 USB 设备的父设备
- 设备层次结构在 sysfs 的 /sys/devices/ 目录中以目录树的形式体现

**2. 总线层次（Bus Layer）**

总线是连接设备和驱动的桥梁。总线类型（bus_type）代表一种总线的抽象，如 PCI、USB、I2C、SPI、platform 等。总线在 sysfs 中对应 /sys/bus/<bus_name>/ 目录，包含：
- devices/：该总线上的所有设备
- drivers/：该总线上的所有驱动

**3. 驱动层次（Driver Layer）**

驱动（device_driver）是管理设备的软件实体。每种驱动都与特定的总线类型关联，可以与该总线上匹配的设备进行绑定。

**4. 类层次（Class Layer）**

类（class）按功能类型对设备进行归类，不依赖于物理总线。例如：
- input 类包含所有输入设备
- net 类包含所有网络设备
- leds 类包含所有 LED 设备

类的引入使得用户空间可以按功能统一访问设备，而无需关心设备的具体总线类型。

**整体架构图：**

```
/sys/
├── devices/           # 所有设备的物理层次结构
│   ├── platform/
│   ├── pci0000:00/
│   └── ...
├── bus/              # 按总线类型组织的设备和驱动
│   ├── platform/
│   ├── usb/
│   ├── pci/
│   └── ...
├── class/            # 按功能类组织的设备
│   ├── input/
│   ├── net/
│   ├── leds/
│   └── ...
└── module/            # 加载的内核模块
```

这种层次结构的设计使得：
- 设备管理更加清晰和有序
- 驱动可以方便地找到需要管理的设备
- 用户空间可以通过统一的接口访问设备
- 热插拔事件可以正确地传递和处理

### 面试题2：驱动和设备是如何匹配的？

**参考答案：**

驱动和设备的匹配是 Linux 设备模型中最核心的机制之一。整个匹配过程可以概括为以下几个步骤：

**1. 匹配触发时机**

匹配过程在以下情况会被触发：
- 驱动注册到总线时（driver_register）
- 设备注册到总线时（device_add）
- 设备或驱动热插拔时

**2. 匹配流程**

```
1. 调用总线的 match() 函数
   ├── 设备树匹配（OF Match）    // 最高优先级
   ├── ACPI 匹配                 // x86 平台
   ├── ID 表匹配                 // 驱动定义的设备 ID 表
   └── 名称匹配                  // 兼容旧式驱动

2. match() 返回非零值表示匹配成功

3. 调用驱动的 probe() 函数进行进一步探测

4. probe() 成功，绑定设备与驱动
   - 设置 dev->driver = drv
   - 将设备加入驱动的设备链表
   - 发送 KOBJ_BIND uevent

5. probe() 失败，尝试下一个驱动
```

**3. 匹配方法详解**

**设备树匹配（推荐方式）：**
```c
// 驱动定义设备树匹配表
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-device", },
    { /* sentinel */ }
};

// 总线的 match 函数会调用 of_driver_match_device()
// 比较驱动的 of_match_table 与设备的 compatible 属性
```

**ACPI 匹配（x86 平台）：**
```c
// 驱动定义 ACPI 匹配表
static const struct acpi_device_id my_acpi_match[] = {
    { "VEN0001", 0 },
    { }
};
```

**ID 表匹配：**
```c
// 驱动定义 ID 表
static const struct platform_device_id my_id_table[] = {
    { "my-device-1", 0 },
    { "my-device-2", 1 },
    { },
};
```

**名称匹配（兼容性）：**
```c
// 简单地比较驱动名称和设备名称
strcmp(dev->name, drv->name) == 0
```

**4. 关键代码分析**

```c
// drivers/base/dd.c
int driver_probe_device(struct device_driver *drv, struct device *dev)
{
    int ret = 0;

    // 步骤1：检查总线是否有 match 函数
    if (!drv->bus->match || !drv->bus->match(dev, drv))
        return -ENODEV;  // 不匹配

    // 步骤2：调用驱动的 probe 函数
    if (drv->probe) {
        ret = drv->probe(dev);
        if (ret)
            goto probe_failed;
    }

    // 步骤3：绑定设备与驱动
    dev_set_driver(dev, drv);
    driver_bound(dev);

    return ret;

probe_failed:
    dev_set_driver(dev, NULL);
    return ret;
}
```

**5. 匹配优先级**

在实际系统中，匹配的优先级顺序为：
1. 设备树（Device Tree）- 现代嵌入式系统推荐使用
2. ACPI - x86 平台
3. ID 表 - 驱动定义的设备列表
4. 名称匹配 - 向后兼容

这种设计确保了：
- 新式描述方式（设备树）优先于旧式方式
- 驱动的可移植性和兼容性
- 用户空间的设备管理更加灵活


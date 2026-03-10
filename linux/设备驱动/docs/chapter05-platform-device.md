# 第5章 平台设备驱动案例

Platform 设备驱动是 Linux 驱动开发中最常见也是最基础的驱动类型之一。它为那些无法通过传统总线（如 PCI、USB、I2C 等）描述的设备提供了一种统一的抽象方式。在本章中，我们将深入探讨 Platform 驱动模型的核心概念，包括 platform_device 和 platform_driver 结构体的定义、注册流程、设备树与 platform 设备的转换机制，以及典型驱动开发的完整实践。通过本章的学习，读者将能够掌握 Platform 驱动开发的核心技能，理解设备树到 platform 设备的转换原理，并能够独立开发基于设备树的 Platform 驱动。

## 5.1 Platform 驱动模型

Platform 驱动模型是 Linux 内核为片上系统（SoC）外设设计的一种虚拟总线机制。在嵌入式系统中，特别是 ARM 架构的 SoC 中，存在大量无法通过传统物理总线描述的外设，如 GPIO 控制器、定时器、Watchdog、RTC 等。这些外设通常通过内存映射 I/O（MMIO）访问，没有标准的总线协议，因此 Linux 内核引入了 Platform 总线来统一管理这些设备。Platform 驱动模型的核心思想是将这些"虚拟平台设备"与对应的驱动程序进行绑定，实现设备的初始化、探测和资源管理。

### 5.1.1 platform 设备与驱动

Platform 设备驱动模型的核心数据结构是 `struct platform_device` 和 `struct platform_driver`。这两个结构体分别代表平台设备和平台驱动，它们在内核中通过特定的匹配机制进行绑定。下面我们详细分析这两个关键结构体的定义和各个成员的含义。

`struct platform_device` 结构体定义位于 Linux 内核源码的 `include/linux/platform_device.h` 文件中。以下是 Linux 5.x 内核中的核心定义：

```c
// 文件位置：include/linux/platform_device.h
struct platform_device {
    // name 是设备的名称，用于与驱动进行匹配
    // 驱动通过这个名字来识别需要管理的设备
    const char      *name;

    // id 用于区分多个同名的设备
    // 当 id >= 0 时，设备名称为 "name{id}"，如 "gpio.0"、"gpio.1"
    // 当 id = -1 时，使用设备本身的名字，不添加数字后缀
    int             id;

    // id_auto 表示是否自动分配 id
    // 当设置为 true 时，内核会自动为设备分配唯一的 id
    bool            id_auto;

    // dev 是内嵌的 device 结构体
    // 这是 Linux 设备模型的核心，包含了设备的所有通用信息
    // 通过这个成员，platform_device 可以接入到 Linux 的设备层次结构中
    struct device   dev;

    // num_resources 表示资源数量
    // 资源包括 I/O 内存区域、中断请求等
    u32             num_resources;

    // resource 指向资源数组
    // 每个元素描述一个 I/O 内存区域或中断请求等资源
    struct resource *resource;

    // id_entry 指向平台设备 ID 表项
    // 通常在设备通过 ACPI 或其他固件描述时使用
    const struct platform_device_id *id_entry;

    // driver 指向绑定的平台驱动
    struct platform_driver *driver;

    // 以下成员在较新的内核版本中可能存在
    ...
};
```

下面对 platform_device 结构体的核心成员进行详细解析，帮助读者深入理解每个成员的作用和意义。

**name 成员**是 platform 设备最重要的标识符，用于与驱动进行匹配。在设备树出现之前，这个名字通常是驱动和设备之间的唯一关联点。驱动在定义匹配表时，会指定它支持哪些名字的设备。例如，一个 GPIO 驱动可能支持 "samsung,gpio" 和 "samsung,exynos-gpio" 等多种设备名称。

**dev 成员**是内嵌的 device 结构体，这是 Linux 设备模型的核心。通过这个成员，platform_device 自动获得了 Linux 设备模型的所有特性，包括：sysfs 导出（在 /sys/devices/platform/ 下创建目录）、引用计数管理、热插拔事件支持、电源管理等。在实际开发中，我们通常通过 `platform_get_resource()` 系列函数来获取设备的资源，这些函数实际上是对 device 成员相关功能的封装。

**resource 成员**是 platform 设备最独特的部分，它用于描述设备的硬件资源。在没有设备树的年代，这些资源通常是静态定义的，包括：IORESOURCE_MEM 类型的内存映射区域、IORESOURCE_IRQ 类型的中断请求、IORESOURCE_DMA 类型的 DMA 通道等。每个资源通过 `struct resource` 结构体来描述：

```c
// 文件位置：include/linux/ioport.h
struct resource {
    // 资源的起始地址或起始向量号
    resource_size_t start;

    // 资源的结束地址或结束向量号
    resource_size_t end;

    // 资源的名称，通常设置为设备名
    const char *name;

    // 资源标志，标识资源类型
    unsigned long flags;

    // 指向父资源的指针，用于构建资源树
    struct resource *parent, *sibling, *child;

    ...
};
```

资源标志（flags）定义了资源的类型，常见的类型包括：IORESOURCE_MEM 表示内存映射 I/O 区域、IORESOURCE_IRQ 表示中断请求、IORESOURCE_DMA 表示 DMA 通道、IORESOURCE_IO 表示 I/O 端口（x86 架构）。通过这些资源，驱动可以获取设备的物理地址、中断号等关键信息。

`struct platform_driver` 结构体代表平台设备的驱动程序，它定义了驱动如何与平台设备进行交互。以下是 Linux 5.x 内核中的核心定义：

```c
// 文件位置：include/linux/platform_device.h
struct platform_driver {
    // probe 是驱动探测函数，当设备与驱动匹配成功时被调用
    // 在这个函数中，驱动通常会进行以下操作：
    // 1. 获取设备的资源（内存、中断等）
    // 2. 映射 I/O 内存
    // 3. 申请中断
    // 4. 初始化硬件
    // 5. 创建设备节点或注册字符设备
    int (*probe)(struct platform_device *);

    // remove 是驱动移除函数，当设备从系统中移除或驱动卸载时被调用
    // 在这个函数中，驱动通常会：
    // 1. 释放中断
    // 2. 取消映射 I/O 内存
    // 3. 注销字符设备
    // 4. 释放所有分配的资源
    int (*remove)(struct platform_device *);

    // shutdown 是关闭函数，在系统关机时调用
    void (*shutdown)(struct platform_device *);

    // suspend 是挂起函数，在系统进入睡眠模式前调用
    // 用于保存设备状态并停止设备
    int (*suspend)(struct platform_device *, pm_message_t state);

    // resume 是恢复函数，在系统从睡眠模式唤醒后调用
    // 用于恢复设备状态并重新启动设备
    int (*resume)(struct platform_device *);

    // driver 是内嵌的 device_driver 结构体
    // 这是 Linux 驱动模型的核心，包含了驱动的通用信息
    struct device_driver driver;

    // id_table 是平台设备 ID 表
    // 用于非设备树平台的驱动匹配
    // 数组以 { } 结束
    const struct platform_device_id *id_table;

    // 以下成员在较新的内核版本中可能存在
    ...
};
```

下面对 platform_driver 结构体的核心回调函数进行详细解析。

**probe 函数**是平台驱动最核心的函数，它在设备与驱动成功匹配后被调用。驱动的初始化工作主要在这里完成。一个典型的 probe 函数通常包含以下步骤：首先通过 `platform_get_resource()` 获取设备的内存资源和中断资源；然后使用 `ioremap()` 或 `devm_ioremap_resource()` 映射 I/O 内存；接着使用 `platform_get_irq()` 获取中断号并使用 `request_irq()` 申请中断；之后进行硬件初始化；最后注册相应的字符设备、网络设备或其他类型的设备。probe 函数返回 0 表示成功，返回负值表示失败。

**remove 函数**是 probe 函数的逆操作，负责释放所有在 probe 中分配的资源。当设备从系统中移除或者驱动模块被卸载时，这个函数会被调用。重要的是，remove 函数必须能够处理设备正在使用的情况，确保所有资源都能正确释放。

**suspend 和 resume 函数**是电源管理相关的回调。在系统进入低功耗状态（如休眠、待机）时，suspend 函数会被调用，驱动需要保存设备状态并停止设备；在系统恢复正常运行时，resume 函数会被调用，驱动需要恢复设备状态。现代 Linux 内核推荐使用更先进的电源管理机制，如 Runtime PM 和系统睡眠回调，这些内容将在第7章详细讨论。

platform_driver 结构体中内嵌的 `struct device_driver` 是 Linux 驱动模型的核心结构体，它包含了驱动与总线、设备模型交互所需的所有通用信息：

```c
// 文件位置：include/linux/device.h
struct device_driver {
    // name 是驱动的名称
    const char      *name;

    // owner 指向拥有该驱动的模块，通常设置为 THIS_MODULE
    struct module   *owner;

    // bus 指向驱动所属的总线类型
    struct bus_type     *bus;

    // 驱动在设备模型中的 kobject
    struct kobject          *kobj;

    // 驱动支持的所有设备的列表
    struct list_head    devices;

    // 驱动的探针函数指针（通用设备模型中使用）
    int (*probe) (struct device *dev);

    // 驱动的移除函数指针
    int (*remove) (struct device *dev);

    // 驱动的关闭函数指针
    void (*shutdown) (struct device *dev);

    // 驱动的挂起函数指针
    int (*suspend) (struct device *dev, pm_message_t state);

    // 驱动的恢复函数指针
    int (*resume) (struct device *dev);

    // 驱动属性组
    const struct attribute_group **groups;

    // 设备树匹配表
    const struct of_device_id   *of_match_table;

    // ACPI 匹配表
    const struct acpi_device_id *acpi_match_table;

    // 驱动私有数据
    void            *driver_data;

    ...
};
```

其中 `of_match_table` 是设备树匹配表，它在设备树驱动的匹配过程中起着关键作用。我们将在 5.2 节详细讨论设备树匹配机制。

### 5.1.2 注册流程

Platform 设备驱动的注册流程是 Linux 设备模型的重要组成部分。驱动需要通过 `platform_driver_register()` 注册到内核，设备需要通过 `platform_device_register()` 注册到内核，然后内核通过匹配机制将设备与驱动绑定在一起。下面我们详细分析注册流程的实现原理。

驱动注册使用 `platform_driver_register()` 函数，该函数定义在 `drivers/base/platform.c` 中：

```c
// 文件位置：drivers/base/platform.c
int platform_driver_register(struct platform_driver *drv)
{
    // 初始化驱动的 device_driver 结构体
    drv->driver.bus = &platform_bus_type;

    // 如果驱动没有设置探测函数，使用默认的探测函数
    // 这个默认函数会进一步调用驱动自己的 probe 函数
    if (drv->probe)
        drv->driver.probe = platform_drv_probe;

    if (drv->remove)
        drv->driver.remove = platform_drv_remove;

    if (drv->shutdown)
        drv->driver.shutdown = platform_drv_shutdown;

    if (drv->suspend)
        drv->driver.suspend = platform_drv_suspend;

    if (drv->resume)
        drv->driver.resume = platform_drv_resume;

    // 将驱动添加到平台总线
    return driver_register(&drv->driver);
}
```

从上述代码可以看到，`platform_driver_register()` 函数主要完成以下工作：首先设置驱动的总线类型为 platform_bus_type，然后为驱动的各个回调函数设置包装函数（这些包装函数最终会调用驱动自己的回调），最后调用 `driver_register()` 将驱动注册到内核。

`driver_register()` 函数是 Linux 设备模型的核心函数，它负责将驱动添加到总线的驱动链表中，并尝试与已经存在的设备进行匹配：

```c
// 文件位置：drivers/base/driver.c
int driver_register(struct device_driver *drv)
{
    int ret;
    struct device_driver *other;

    // 检查驱动是否已经注册
    other = driver_find(drv->name, drv->bus);
    if (other) {
        printk(KERN_ERR "Driver: %s was already registered.\n", drv->name);
        return -EBUSY;
    }

    // 将驱动添加到总线的驱动链表中
    ret = bus_add_driver(drv);
    if (ret)
        return ret;

    // 如果驱动有属性文件，创建属性文件
    ret = driver_add_groups(drv, drv->groups);
    if (ret)
        bus_remove_driver(drv);

    // 尝试与已经存在的设备进行匹配
    // 这是关键步骤，内核会遍历所有已注册的设备
    // 检查是否有设备与该驱动匹配
    driver_attach(drv);

    return 0;
}
```

驱动注册的核心步骤是 `driver_attach()`，这个函数会遍历总线上所有已注册的设备，尝试与当前驱动进行匹配：

```c
// 文件位置：drivers/base/dd.c
int driver_attach(struct device_driver *drv)
{
    // 调用 bus 的 match 函数进行匹配
    // 如果匹配成功，调用驱动的 probe 函数
    return driver_probe_device(drv, NULL);
}

static int driver_probe_device(struct device_driver *drv, struct device *dev)
{
    int ret = 0;

    if (!device_is_registered(dev))
        return -ENODEV;

    // 获取驱动拥有的模块引用，防止驱动在探测期间被卸载
    get_module(drv->owner);

    // 调用总线的 probe 函数
    // 对于 platform 总线，这会调用 platform_bus_type 的 probe
    ret = drv->bus->probe(dev);

    if (ret) {
        // 探测失败，释放模块引用
        module_put(drv->owner);
        return ret;
    }

    // 探测成功，将设备与驱动关联
    driver_bound(dev, drv);

    return ret;
}
```

设备注册使用 `platform_device_register()` 函数，它将平台设备添加到内核并尝试与已注册的驱动进行匹配：

```c
// 文件位置：drivers/base/platform.c
int platform_device_register(struct platform_device *pdev)
{
    // 初始化设备的 device 结构体
    device_initialize(&pdev->dev);

    // 设置设备的总线类型
    pdev->dev.bus = &platform_bus_type;
    pdev->dev.type = &platform_device_type;

    // 如果设备没有设置名称，使用设备名称
    if (! pdev->dev.init_name)
        pdev->dev.init_name = pdev->name;

    // 将设备添加到平台总线
    return platform_device_add(pdev);
}

int platform_device_add(struct platform_device *pdev)
{
    int i, ret;

    // 如果设备有资源，检查资源是否有效
    if (drv && !dev_get_platdata(dev)) {
        // ...
    }

    // 将设备添加到平台总线的设备链表中
    ret = device_add(&pdev->dev);
    if (ret == 0)
        return ret;

    // 如果添加失败，释放资源
    platform_device_put(pdev);

    return ret;
}
```

设备添加的核心是 `device_add()` 函数，它会将设备添加到总线的设备链表中，并尝试与已注册的驱动进行匹配。这个过程与驱动注册时的匹配过程是对称的。

匹配机制的核心是 `bus_probe_device()` 函数，当新设备添加到总线时，它会遍历总线上所有已注册的驱动，尝试找到与设备匹配的驱动：

```c
// 文件位置：drivers/base/dd.c
void bus_probe_device(struct device *dev)
{
    struct bus_type *bus = dev->bus;
    int ret;

    // 如果总线支持驱动自动探测
    if (bus && bus->p && bus->p->drivers_autoprobe) {
        // 尝试与已注册的驱动进行匹配
        ret = device_attach(dev);

        // 如果有驱动成功绑定，device_attach 返回 > 0
        // 如果没有找到匹配的驱动，返回 0 或负值
    }

    // 创建设备的 uevent 事件，通知用户空间
    kobject_uevent(&dev->kobj, KOBJ_ADD);
}
```

`device_attach()` 函数遍历总线上所有已注册的驱动，调用总线的 match 函数进行匹配：

```c
// 文件位置：drivers/base/dd.c
int device_attach(struct device *dev)
{
    struct device_driver *drv;
    int ret = 0;

    if (dev->driver) {
        // 设备已经有驱动的，直接绑定
        driver_probe_device(dev->driver, dev);
        return 1;
    }

    // 遍历总线上的所有驱动
    down(&dev->bus->p->dynids.lock);
    list_for_each_entry(drv, &dev->bus->p->dynids.list, dynids_node) {
        ret = driver_probe_device(drv, dev);
        if (ret < 0)
            goto out;
    }

    // 遍历总线上的所有驱动（静态列表）
    bus_for_each_drv(dev->bus, NULL, &dev->driver, __driver_attach);

out:
    return ret;
}
```

整个注册和匹配流程可以概括为以下几个关键步骤：第一步，驱动调用 `platform_driver_register()` 注册到内核，内核调用 `driver_attach()` 尝试与已注册的设备匹配；第二步，设备调用 `platform_device_register()` 注册到内核，内核调用 `device_attach()` 尝试与已注册的驱动匹配；第三步，匹配成功后，内核调用驱动的 probe 函数进行设备探测。

## 5.2 设备树节点到 platform 设备的转换

设备树（Device Tree）是 ARM 架构 Linux 内核中描述硬件配置的重要机制。在现代嵌入式系统中，大多数外设都通过设备树来描述，而不是使用传统的 platform_device 静态定义方式。理解设备树节点如何转换为 platform 设备，对于开发基于设备树的驱动至关重要。本节将深入分析设备树解析的完整流程，以及驱动如何通过设备树匹配表与设备进行绑定。

### 5.2.1 of_platform_default_bus_init

设备树到 platform 设备的转换是 Linux 内核在启动阶段完成的一项重要工作。当内核启动并解析设备树（dtb）后，需要为设备树中的每个节点创建对应的 platform_device。这个过程涉及多个内核子系统的协同工作，包括设备树解析子系统、platform 总线子系统等。

在 Linux 5.x 内核中，设备树节点的解析和 platform 设备的创建主要发生在以下两个阶段：第一阶段是在系统初始化时创建默认的 platform 总线，第二阶段是遍历设备树节点并为每个节点创建 platform 设备。

`of_platform_default_bus_init()` 函数是设备树初始化过程中的关键函数，它负责初始化默认的 platform 总线并开始设备树的遍历和转换。以下是该函数的核心实现：

```c
// 文件位置：drivers/of/platform.c
int __init of_platform_default_bus_init(void)
{
    // 初始化 platform 总线类型
    // 确保 platform_bus_type 已经正确初始化
    // 这是设备树节点转换为 platform 设备的前提条件

    // 创建根级别的 platform 设备
    // 遍历设备树的根节点，为每个子节点创建 platform_device
    of_platform_bus_create(NULL, NULL, NULL, NULL, true);

    return 0;
}
```

`of_platform_bus_create()` 函数是递归创建 platform 设备的核心函数：

```c
// 文件位置：drivers/of/platform.c
static int of_platform_bus_create(struct device_node *bus,
                  const struct of_device_id *matches,
                  struct device *parent, void *data, bool strict)
{
    struct device_node *child;
    struct platform_device *dev;
    const struct of_device_id *match;
    int rc = 0;

    // 为当前节点创建 platform_device
    // 但首先检查这个节点是否应该被创建为 platform 设备

    // 遍历当前节点的所有子节点
    for_each_child_of_node(bus, child) {
        // 检查子节点是否应该创建为 platform 设备
        // 某些节点可能被其他总线驱动处理（如 I2C、SPI 等）
        rc = of_platform_bus_create(child, matches, &dev->dev, data, strict);
        if (rc) {
            of_node_put(child);
            break;
        }
    }

    return rc;
}
```

`of_platform_device_create_pdata()` 函数是实际创建 platform_device 的函数：

```c
// 文件位置：drivers/of/platform.c
static struct platform_device *of_platform_device_create_pdata(
    struct device_node *np,
    const char *platform_data,
    void *platform_data_size,
    struct device *parent)
{
    struct platform_device *dev;

    // 检查节点是否标记为 "status = "disabled""
    // 如果是，则不创建设备
    if (!of_device_is_available(np))
        return NULL;

    // 检查节点是否有 "compatible" 属性
    // 这是设备匹配的必要条件
    if (!of_get_property(np, "compatible", NULL))
        return NULL;

    // 分配 platform_device 结构体
    dev = platform_device_alloc(np->name, PLATFORM_DEVID_NONE);
    if (!dev)
        goto err_out;

    // 设置设备树节点指针
    dev->dev.of_node = of_node_get(np);

    // 设置父设备
    if (parent)
        dev->dev.parent = parent;

    // 设置平台数据
    if (platform_data) {
        ret = platform_device_add_data(dev, platform_data, platform_data_size);
        if (ret)
            goto err_dev_put;
    }

    // 将资源从设备树节点解析到 platform_device
    // 这是关键步骤，将设备树的 reg、interrupts 等属性转换为资源
    ret = of_device_add_resources(dev);
    if (ret)
        goto err_dev_put;

    // 添加设备到内核
    ret = platform_device_add(dev);
    if (ret)
        goto err_dev_put;

    return dev;

err_dev_put:
    platform_device_put(dev);
err_out:
    return NULL;
}
```

设备树资源解析的核心函数是 `of_device_add_resources()`，它将设备树中的以下属性转换为 platform_device 的资源：`reg` 属性转换为 IORESOURCE_MEM 类型的资源、`interrupts` 属性转换为 IORESOURCE_IRQ 类型的资源、`clocks` 属性被特殊处理为时钟资源、`dma-ranges` 属性被处理为 DMA 资源。

```c
// 文件位置：drivers/of/device.c
static int of_device_add_resources(struct platform_device *dev)
{
    struct device_node *np = dev->dev.of_node;
    struct resource *r;
    int i, tnum, proplen;

    // 计算资源数量
    // 统计 I/O 内存区域数量（reg 属性）
    // 统计中断数量（interrupts 属性）

    if (of_can_translate_address(np))
        num_reg = of_address_to_resource(np, 0, NULL);

    num_irq = of_irq_to_resource_table(np, NULL, 0);

    // 分配资源数组
    num_resources = num_reg + num_irq;
    if (num_resources == 0)
        return 0;

    r = devm_kzalloc(&dev->dev, sizeof(*r) * num_resources, GFP_KERNEL);
    if (!r)
        return -ENOMEM;

    // 填充资源
    dev->resource = r;
    dev->num_resources = num_resources;

    // 解析 I/O 内存资源
    if (num_reg)
        of_address_to_resource(np, 0, &r[num_reg++]);

    // 解析中断资源
    if (num_irq)
        of_irq_to_resource_table(np, &r[num_irq], num_irq);

    return 0;
}
```

`of_address_to_resource()` 函数负责将设备树的 `reg` 属性转换为 `struct resource`：

```c
// 文件位置：drivers/of/address.c
int of_address_to_resource(struct device_node *dev, int index,
                struct resource *r)
{
    struct resource tmp;

    // 调用 __of_address_to_resource 进行实际转换
    // 考虑不同的总线地址空间（如 MMIO、PCI 等）

    // 将转换结果拷贝到传入的 resource 结构体
    *r = tmp;

    return 0;
}
EXPORT_SYMBOL(of_address_to_resource);
```

设备树节点到 platform 设备的转换流程可以概括为以下步骤：第一步，内核在启动时调用 `of_platform_default_bus_init()` 初始化 platform 总线；第二步，遍历设备树节点，对每个包含 "compatible" 属性的节点调用 `of_platform_device_create_pdata()` 创建 platform_device；第三步，解析设备树的 `reg` 属性创建 IORESOURCE_MEM 资源，解析 `interrupts` 属性创建 IORESOURCE_IRQ 资源；第四步，将创建的 platform_device 注册到内核，触发设备与驱动的匹配过程。

### 5.2.2 驱动匹配

设备树驱动的匹配机制是连接设备与驱动的桥梁。在基于设备树的驱动中，驱动通过 `of_device_id` 匹配表来声明它支持哪些设备，内核在设备注册时根据设备的 "compatible" 属性与驱动的匹配表进行比对，找到匹配的驱动后调用其 probe 函数。

`struct of_device_id` 结构体定义了设备树匹配表中的每一项：

```c
// 文件位置：include/linux/mod_devicetable.h
struct of_device_id {
    // compatible 属性值，用于匹配设备
    char    name[32];

    // 类型属性值，用于更精确的匹配
    char    type[32];

    // 设备树节点路径，用于精确匹配特定节点
    char    compatible[128];

    // 指向下一个匹配项的指针（内核内部使用）
    const void *data;
};
```

在实际的驱动代码中，通常使用简化版的宏来定义匹配表：

```c
// 定义设备树匹配表
static const struct of_device_id my_driver_of_match[] = {
    // 第一个匹配项：完全匹配
    { .compatible = "vendor,my-device-1", },

    // 第二个匹配项：另一个兼容设备
    { .compatible = "vendor,my-device-2", },

    // 第三个匹配项：通配符匹配（vendor 级别）
    { .compatible = "vendor,my-device-generic", },

    // 结束标记
    { },
};
MODULE_DEVICE_TABLE(of, my_driver_of_match);
```

驱动匹配的核心函数是 `of_match_device()`，它根据设备的设备树节点在驱动的匹配表中查找匹配的项：

```c
// 文件位置：drivers/of/device.c
const struct of_device_id *of_match_device(const struct of_device_id *matches,
                         const struct device *dev)
{
    const struct of_device_id *match;
    const char *const *compat;

    if (!matches)
        return NULL;

    // 遍历匹配表中的每一项
    for (match = matches; match->compatible[0]; match++) {
        // 获取设备的 compatible 属性
        compat = of_get_property(dev->of_node, "compatible", NULL);
        if (!compat)
            continue;

        // 逐个比较 compatible 属性值
        // 支持一个设备有多个 compatible 值的情况
        for ( ; *compat; compat++) {
            // 使用字符串比较
            // 也可以使用更高效的 of_match_string
            if (of_match_string(match->compatible, *compat) == 0)
                return match;
        }
    }

    return NULL;
}
```

在 platform 总线的 match 函数中，会调用上述函数进行设备树匹配：

```c
// 文件位置：drivers/base/platform.c
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    // 1. 优先检查设备是否有 id_entry（ACPI 匹配）
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    // 2. 尝试设备树匹配
    if (of_driver_match_device(dev, drv))
        return of_match_device(pdrv->driver.of_match_table, dev) != NULL;

    // 3. 尝试 ACPI 匹配
    if (ACPI_HANDLE(dev))
        return platform_acpi_match(pdev, pdrv);

    // 4. 传统的名称匹配
    return strcmp(pdev->name, drv->name) == 0;
}
```

从上述代码可以看到，platform 总线的匹配顺序是：首先检查 id_entry（主要用于 ACPI 匹配），然后尝试设备树匹配（of_driver_match_device），接着尝试 ACPI 匹配，最后进行传统的名称匹配。这种优先级顺序确保了新的设备树机制能够优先于传统的名称匹配。

设备树匹配的完整流程如下：当 platform_device 注册到内核时，内核会调用 `platform_match()` 函数进行匹配；在 `platform_match()` 函数中，首先调用 `of_driver_match_device()` 检查驱动是否有设备树匹配表；然后调用 `of_match_device()` 在驱动的设备树匹配表中查找与设备 compatible 属性匹配的项；如果匹配成功，返回非零值，触发驱动的 probe 函数调用。

在驱动的 probe 函数中，可以通过以下方式获取设备树中的资源：

```c
static int my_probe(struct platform_device *dev)
{
    struct resource *res;
    struct device_node *np = dev->dev.of_node;
    u32 value;
    int irq;
    const char *str;
    struct clk *clk;
    int ret;

    /* 获取内存资源 */
    res = platform_get_resource(dev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&dev->dev, "Failed to get memory resource\n");
        return -ENODEV;
    }

    /* 获取中断资源 */
    irq = platform_get_irq(dev, 0);
    if (irq < 0) {
        dev_err(&dev->dev, "Failed to get IRQ\n");
        return irq;
    }

    /* 映射 I/O 内存 */
    void __iomem *regs = devm_ioremap_resource(&dev->dev, res);
    if (IS_ERR(regs))
        return PTR_ERR(regs);

    /* 读取设备树属性 - 整数类型 */
    ret = of_property_read_u32(np, "custom-property", &value);
    if (ret) {
        /* 属性不存在，使用默认值 */
        value = DEFAULT_VALUE;
    }

    /* 读取设备树属性 - 字符串类型 */
    ret = of_property_read_string(np, "mode-str", &str);
    if (ret) {
        /* 属性不存在 */
    }

    /* 读取设备树属性 - 布尔类型 */
    if (of_property_read_bool(np, "has-feature"))
        /* 启用特性 */;

    /* 获取时钟 */
    clk = devm_clk_get(&dev->dev, "apb");
    if (IS_ERR(clk))
        return PTR_ERR(clk);

    /* 获取复位控制器 */
    /* ... */

    return 0;
}
```

`platform_get_resource()` 和 `platform_get_irq()` 是获取设备资源的常用函数：

```c
// 文件位置：drivers/base/platform.c
struct resource *platform_get_resource(struct platform_device *dev,
                       unsigned int type, unsigned int num)
{
    int i;

    for (i = 0; i < dev->num_resources; i++) {
        struct resource *r = &dev->resource[i];

        if ((r->flags & IORESOURCE_TYPE_BITS) == type && num-- == 0)
            return r;
    }

    return NULL;
}
EXPORT_SYMBOL_GPL(platform_get_resource);

int platform_get_irq(struct platform_device *dev, unsigned int num)
{
    struct resource *r = platform_get_resource(dev, IORESOURCE_IRQ, num);

    if (!r)
        return -ENXIO;

    return r->start;
}
EXPORT_SYMBOL_GPL(platform_get_irq);
```

## 5.3 典型 Platform 驱动开发

通过前两节的学习，我们已经掌握了 Platform 驱动模型的理论知识。本节将把这些知识付诸实践，展示一个完整的 Platform 驱动开发流程。我们将使用一个虚拟的硬件设备作为示例，该设备包含内存映射寄存器、中断控制器和时钟输入。通过这个示例，读者将学习到如何定义设备树节点、如何编写驱动代码、如何正确处理资源获取和释放，以及如何遵循内核编码规范进行开发。

### 5.3.1 设备树节点示例

在开始编写驱动之前，首先需要定义设备树节点。设备树节点描述了硬件的连接情况和配置参数，是驱动获取硬件信息的主要来源。一个完整的设备树节点通常包含以下属性：`compatible` 属性用于驱动匹配、`reg` 属性描述 I/O 内存区域、`interrupts` 属性描述中断配置、`clocks` 属性描述时钟源、`status` 属性控制设备是否启用。

以下是一个典型的 Platform 设备设备树节点：

```dts
/*
 * 设备树节点示例
 *
 * 这个设备节点描述了一个名为 "my-device" 的硬件设备：
 * - 基地址：0x50000000，大小：0x1000 (4KB)
 * - 中断号：59，触发类型：边沿触发（4 表示下降沿）
 * - 使用 APB 时钟
 */

my_device: my-device@50000000 {
    /* 设备兼容性描述，用于驱动匹配 */
    compatible = "vendor,my-device";

    /* 设备名称，必须与驱动中的 compatible 匹配 */
    /* 通常格式为 "manufacturer,device-name" */

    /* 内存映射区域 */
    /* 第一个值：起始地址，第二个值：大小 */
    reg = <0x50000000 0x1000>;

    /* 中断描述 */
    /* 格式：<中断控制器  中断号  触发类型> */
    /* 0 表示中断控制器（GIC），59 是中断号，4 是触发类型 */
    interrupts = <0 59 4>;

    /* 时钟源 */
    clocks = <&clk 0>;
    clock-names = "apb";

    /* 设备状态："okay" 表示启用，"disabled" 表示禁用 */
    status = "okay";

    /* 自定义属性 - 驱动可以读取这些属性 */
    vendor,custom-property = <0x100>;        /* 32位整数 */
    vendor,custom-string = "example";         /* 字符串 */
    vendor,custom-bool;                      /* 布尔属性（存在即为 true） */

    /* 多个中断的情况 */
    /* interrupts-extended = <&gpio 24>, <&timer0 3>; */

    /* 多个内存区域的情况 */
    /* reg = <0x50000000 0x1000>, <0x50001000 0x1000>; */

    /* 子节点 - 描述设备的子组件 */
    /* 可以在此处添加子节点描述，如 GPIO 控制器等 */
};
```

设备树节点通常被添加到对应的设备树源文件（.dts 或 .dtsi）中。在 ARM 架构中，这些文件通常位于 `arch/arm/boot/dts/` 目录下。设备树节点需要正确添加到 SoC 设备树文件中，放在相应的总线节点下。

设备树节点的关键属性说明如下：

**compatible 属性**是驱动匹配的核心，它是一个字符串列表，每个字符串表示一种设备类型。驱动会按照列表顺序匹配，第一个匹配的 compatible 值会被使用。推荐的命名格式是 "manufacturer,device-name"，例如 "samsung,exynos4210-gpio"。

**reg 属性**描述设备的 I/O 内存区域，格式为 `<起始地址 大小> [<起始地址 大小> ...]`。每个 I/O 区域使用一个 `<address size>` 对表示。驱动通过 `platform_get_resource(dev, IORESOURCE_MEM, index)` 来获取每个区域。

**interrupts 属性**描述设备使用的中断，格式为 `<中断控制器 中断号 触发类型>`。中断号的含义取决于中断控制器，触发类型的值也因中断控制器而异。对于 GIC 中断控制器，4 通常表示下降沿触发。

**clock-names 和 clocks 属性**用于描述设备的时钟输入。`clocks` 引用时钟源节点的 phandle，`clock-names` 提供时钟名称供驱动使用。

**status 属性**控制设备是否启用。"okay" 或 "ok" 表示启用，"disabled" 表示禁用。在调试时，可以通过修改此属性来启用或禁用设备。

### 5.3.2 完整驱动框架

下面是一个完整的 Platform 驱动框架示例。这个驱动遵循 Linux 内核的编码规范，使用了 devm 系列函数进行资源管理，并实现了基本的 probe、remove 和 file_operations。

```c
/*
 * Platform 驱动完整框架示例
 *
 * 文件：my_device_driver.c
 *
 * 这个驱动演示了以下功能：
 * 1. 设备树匹配
 * 2. 资源获取（内存、中断、时钟）
 * 3. 字符设备注册
 * 4. 文件操作实现
 * 5. 资源管理（使用 devm 函数）
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>

/* 模块信息 */
#define DRV_NAME    "my-device"
#define DRV_VERSION "1.0"

/* 设备私有数据结构 */
struct my_device_priv {
    /* 平台设备指针 */
    struct platform_device *pdev;

    /* 设备树节点 */
    struct device_node *np;

    /* 寄存器基地址（已映射） */
    void __iomem *regs;

    /* 物理地址（用于释放映射） */
    phys_addr_t phys_addr;

    /* 中断号 */
    int irq;

    /* 时钟 */
    struct clk *clk;

    /* 字符设备相关 */
    dev_t devno;
    struct cdev cdev;
    struct class *class;
    struct device *device;

    /* 设备状态 */
    bool is_open;

    /* 自定义属性值 */
    u32 custom_value;
};

/* ==================== 文件操作实现 ==================== */

static int my_device_open(struct inode *inode, struct file *filp)
{
    struct my_device_priv *priv;

    /* 从文件私有数据获取设备私有数据 */
    priv = container_of(inode->i_cdev, struct my_device_priv, cdev);
    filp->private_data = priv;

    if (priv->is_open)
        return -EBUSY;

    priv->is_open = true;
    pr_info("%s: Device opened\n", DRV_NAME);

    return 0;
}

static int my_device_release(struct inode *inode, struct file *filp)
{
    struct my_device_priv *priv = filp->private_data;

    priv->is_open = false;
    pr_info("%s: Device closed\n", DRV_NAME);

    return 0;
}

static ssize_t my_device_read(struct file *filp, char __user *buf,
                   size_t count, loff_t *f_pos)
{
    struct my_device_priv *priv = filp->private_data;
    u32 value;

    /* 读取寄存器值（示例） */
    value = readl(priv->regs + 0x00);

    /* 复制到用户空间 */
    if (copy_to_user(buf, &value, sizeof(value)))
        return -EFAULT;

    return sizeof(value);
}

static ssize_t my_device_write(struct file *filp, const char __user *buf,
                    size_t count, loff_t *f_pos)
{
    struct my_device_priv *priv = filp->private_data;
    u32 value;

    if (count < sizeof(value))
        return -EINVAL;

    /* 从用户空间复制数据 */
    if (copy_from_user(&value, buf, sizeof(value)))
        return -EFAULT;

    /* 写入寄存器（示例） */
    writel(value, priv->regs + 0x04);

    return sizeof(value);
}

static long my_device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct my_device_priv *priv = filp->private_data;
    int ret = 0;

    switch (cmd) {
    case 0x100:  /* 自定义命令：获取状态 */
        /* 示例：返回设备状态 */
        break;
    case 0x101:  /* 自定义命令：设置参数 */
        /* 示例：从 arg 获取参数值 */
        break;
    default:
        ret = -ENOTTY;
    }

    return ret;
}

/* 文件操作集 */
static const struct file_operations my_device_fops = {
    .owner          = THIS_MODULE,
    .open           = my_device_open,
    .release        = my_device_release,
    .read           = my_device_read,
    .write          = my_device_write,
    .unlocked_ioctl = my_device_ioctl,
};

/* ==================== 驱动探测和移除 ==================== */

/*
 * 设备树匹配表
 * 必须以空项结束
 */
static const struct of_device_id my_device_of_match[] = {
    { .compatible = "vendor,my-device", },
    { }  /* 结束标记 */
};
MODULE_DEVICE_TABLE(of, my_device_of_match);

/*
 * 驱动探测函数
 * 当设备与驱动匹配成功时被调用
 */
static int my_device_probe(struct platform_device *pdevice)
{
    struct my_device_priv *priv;
    struct resource *res;
    int ret;

    pr_info("%s: Probing device %s\n", DRV_NAME, pdevice->name);

    /* 分配私有数据结构 */
    priv = devm_kzalloc(&pdevice->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) {
        dev_err(&pdevice->dev, "Failed to allocate private data\n");
        return -ENOMEM;
    }

    /* 保存平台设备指针 */
    priv->np = pdevice->dev.of_node;
    priv->pdev = pdevice;

    /* 设置驱动私有数据，之后可通过 dev_get_drvdata() 获取 */
    platform_set_drvdata(pdevice, priv);

    /* 获取内存资源 */
    res = platform_get_resource(pdevice, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdevice->dev, "Failed to get memory resource\n");
        return -ENODEV;
    }
    priv->phys_addr = res->start;

    /* 映射 I/O 内存 */
    /* 使用 devm_ioremap_resource 进行映射，它会检查资源有效性并自动处理错误 */
    priv->regs = devm_ioremap_resource(&pdevice->dev, res);
    if (IS_ERR(priv->regs)) {
        dev_err(&pdevice->dev, "Failed to ioremap\n");
        return PTR_ERR(priv->regs);
    }

    dev_info(&pdevice->dev, "Registers mapped at %pR\n", res);

    /* 获取中断资源 */
    priv->irq = platform_get_irq(pdevice, 0);
    if (priv->irq < 0) {
        dev_err(&pdevice->dev, "Failed to get IRQ\n");
        return priv->irq;
    }

    dev_info(&pdevice->dev, "IRQ = %d\n", priv->irq);

    /* 获取时钟 - 使用名称查找 */
    priv->clk = devm_clk_get(&pdevice->dev, "apb");
    if (IS_ERR(priv->clk)) {
        dev_warn(&pdevice->dev, "Failed to get clock: %ld\n",
                 PTR_ERR(priv->clk));
        priv->clk = NULL;
    } else {
        /* 使能时钟 */
        ret = clk_prepare_enable(priv->clk);
        if (ret) {
            dev_err(&pdevice->dev, "Failed to enable clock\n");
            return ret;
        }
        dev_info(&pdevice->dev, "Clock enabled\n");
    }

    /* 读取设备树属性 */
    ret = of_property_read_u32(priv->np, "vendor,custom-property",
                               &priv->custom_value);
    if (ret) {
        /* 属性不存在，使用默认值 */
        priv->custom_value = 0x100;
        dev_info(&pdevice->dev, "Using default custom-value: 0x%x\n",
                 priv->custom_value);
    } else {
        dev_info(&pdevice->dev, "custom-property = 0x%x\n",
                 priv->custom_value);
    }

    /* 注册字符设备 */
    /* 动态分配设备号 */
    ret = alloc_chrdev_region(&priv->devno, 0, 1, DRV_NAME);
    if (ret < 0) {
        dev_err(&pdevice->dev, "Failed to allocate chrdev region\n");
        goto err_clk_disable;
    }

    /* 初始化字符设备 */
    cdev_init(&priv->cdev, &my_device_fops);
    priv->cdev.owner = THIS_MODULE;

    /* 添加字符设备到系统 */
    ret = cdev_add(&priv->cdev, priv->devno, 1);
    if (ret < 0) {
        dev_err(&pdevice->dev, "Failed to add cdev\n");
        goto err_unreg_chrdev;
    }

    /* 创建设备类 */
    priv->class = class_create(THIS_MODULE, DRV_NAME);
    if (IS_ERR(priv->class)) {
        dev_err(&pdevice->dev, "Failed to create class\n");
        ret = PTR_ERR(priv->class);
        goto err_cdev_del;
    }

    /* 创建设备节点 /dev/my-device */
    priv->device = device_create(priv->class, NULL, priv->devno,
                                  NULL, DRV_NAME);
    if (IS_ERR(priv->device)) {
        dev_err(&pdevice->dev, "Failed to create device\n");
        ret = PTR_ERR(priv->device);
        goto err_class_destroy;
    }

    dev_info(&pdevice->dev, "Driver initialized successfully\n");
    dev_info(&pdevice->dev, "Device node: /dev/%s\n", DRV_NAME);

    return 0;

err_class_destroy:
    class_destroy(priv->class);
err_cdev_del:
    cdev_del(&priv->cdev);
err_unreg_chrdev:
    unregister_chrdev_region(priv->devno, 1);
err_clk_disable:
    if (priv->clk)
        clk_disable_unprepare(priv->clk);

    return ret;
}

/*
 * 驱动移除函数
 * 当设备移除或驱动卸载时被调用
 */
static int my_device_remove(struct platform_device *pdevice)
{
    struct my_device_priv *priv = platform_get_drvdata(pdevice);

    pr_info("%s: Removing device\n", DRV_NAME);

    /* 销毁设备节点 */
    if (priv->class)
        device_destroy(priv->class, priv->devno);

    /* 销毁设备类 */
    if (priv->class)
        class_destroy(priv->class);

    /* 删除字符设备 */
    cdev_del(&priv->cdev);

    /* 注销设备号 */
    unregister_chrdev_region(priv->devno, 1);

    /* 禁用时钟 */
    if (priv->clk)
        clk_disable_unprepare(priv->clk);

    /* 注意：I/O 内存映射由 devm 自动释放，不需要手动 unmapping */

    pr_info("%s: Driver removed\n", DRV_NAME);

    return 0;
}

/* ==================== 平台驱动定义 ==================== */

static struct platform_driver my_device_platform_driver = {
    .probe      = my_device_probe,
    .remove     = my_device_remove,
    .shutdown   = NULL,   /* 可选：系统关机时的回调 */
    .suspend    = NULL,   /* 可选：系统挂起时的回调 */
    .resume     = NULL,   /* 可选：系统恢复时的回调 */

    /* 驱动信息 */
    .driver = {
        .name   = DRV_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = my_device_of_match,
        /* 可以添加其他匹配表，如 ACPI */
    },
};

/* ==================== 模块加载和卸载 ==================== */

static int __init my_device_init(void)
{
    pr_info("%s: Driver version %s initializing\n", DRV_NAME, DRV_VERSION);

    return platform_driver_register(&my_device_platform_driver);
}

static void __exit my_device_exit(void)
{
    pr_info("%s: Driver exiting\n", DRV_NAME);

    platform_driver_unregister(&my_device_platform_driver);
}

module_init(my_device_init);
module_exit(my_device_exit);

/* ==================== 模块信息 ==================== */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name <your.email@example.com>");
MODULE_DESCRIPTION("My Device Driver for Platform Bus");
MODULE_VERSION(DRV_VERSION);

/*
 * 驱动开发注意事项：
 *
 * 1. 资源管理：优先使用 devm_ 系列函数，它们会在设备移除时自动释放资源
 *    - devm_kzalloc, devm_kfree
 *    - devm_ioremap_resource, devm_iounmap
 *    - devm_clk_get, devm_request_irq, devm_gpio_request 等
 *
 * 2. 错误处理：每个可能失败的操作都需要检查返回值，并进行适当的错误恢复
 *
 * 3. 设备树：使用 of_ 系列函数读取设备树属性，确保兼容没有该属性的旧设备
 *
 * 4. 调试：使用 pr_info, pr_warn, pr_err 进行调试输出
 *
 * 5. 锁：如果设备需要并发访问，需要实现适当的锁机制
 *
 * 6. 电源管理：如果需要实现电源管理，需要实现 suspend 和 resume 函数
 *
 * 7. 模块依赖：确保在 Makefile 中正确设置模块依赖关系
 */
```

这个驱动框架展示了完整的 Platform 驱动开发流程：第一步，在设备树匹配表中定义驱动支持的设备；第二步，在 probe 函数中获取所有必要的资源（内存、中断、时钟等）；第三步，读取设备树属性进行配置；第四步，注册字符设备并创建设备节点；第五步，在 remove 函数中按照相反的顺序释放所有资源。

驱动开发中的一些最佳实践包括：使用 `devm_` 系列函数进行资源管理，它们会在设备移除时自动释放资源，大大简化了错误处理和资源释放的代码；使用 `platform_set_drvdata()` 和 `platform_get_drvdata()` 来存储和获取驱动私有数据；使用设备树属性读取函数时要提供默认值，以兼容没有该属性的旧设备；在 probe 函数中要按照相反的顺序处理错误，确保资源不会泄漏。

## 本章面试题

### 面试题1: platform 设备和 platform 驱动的匹配过程

**问题描述**：请详细描述 Linux 内核中 platform 设备和 platform 驱动的匹配过程，包括匹配的触发时机、匹配的具体步骤、以及匹配失败时的处理方式。

**参考答案**：

Linux 内核中 platform 设备和 platform 驱动的匹配过程是设备模型的核心机制之一。当新设备添加到系统或新驱动注册到内核时，内核都会尝试进行设备与驱动的匹配。匹配过程涉及多个关键函数和数据结构，下面详细分析整个流程。

**匹配的触发时机**有两种情况：第一种是驱动注册时触发，当调用 `platform_driver_register()` 时，内核会遍历总线上所有已注册的设备，尝试与新注册的驱动进行匹配；第二种是设备注册时触发，当调用 `platform_device_register()` 时，内核会遍历总线上所有已注册的驱动，尝试与新注册的设备进行匹配。

**匹配的具体步骤**如下（以设备注册时为例）：

第一步，设备注册到 platform 总线。当调用 `platform_device_register()` 时，会执行 `platform_device_add()` 函数，该函数调用 `device_add()` 将设备添加到 platform 总线的设备链表中。

第二步，总线探测设备。在 `device_add()` 函数的最后，会调用 `bus_probe_device()` 函数：

```c
// 文件位置：drivers/base/dd.c
void bus_probe_device(struct device *dev)
{
    struct bus_type *bus = dev->bus;

    if (bus && bus->p && bus->p->drivers_autoprobe) {
        ret = device_attach(dev);
    }

    kobject_uevent(&dev->kobj, KOBJ_ADD);
}
```

第三步，遍历驱动进行匹配。`device_attach()` 函数会遍历 platform 总线上所有已注册的驱动，调用驱动的探测函数进行匹配：

```c
// 文件位置：drivers/base/dd.c
int device_attach(struct device *dev)
{
    struct device_driver *drv;
    int ret = 0;

    if (dev->driver) {
        driver_probe_device(dev->driver, dev);
        return 1;
    }

    // 遍历总线上的所有驱动
    bus_for_each_drv(dev->bus, NULL, &dev->driver, __driver_attach);

    return ret;
}
```

第四步，执行平台匹配。`__driver_attach()` 函数会调用 platform 总线的 `probe()` 函数：

```c
// 文件位置：drivers/base/platform.c
static int platform_drv_probe(struct platform_driver *drv, struct platform_device *dev)
{
    // 调用驱动的 probe 函数
    if (drv->probe)
        return drv->probe(dev);

    return 0;
}
```

在调用驱动的 probe 函数之前，platform 总线会执行匹配检查，这就是关键的 `platform_match()` 函数：

```c
// 文件位置：drivers/base/platform.c
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    // 1. 匹配 id_table（ACPI 匹配）
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    // 2. 设备树匹配（of_driver_match_device 内部调用 of_match_device）
    if (of_driver_match_device(dev, drv))
        return of_match_device(pdrv->driver.of_match_table, dev) != NULL;

    // 3. ACPI 匹配
    if (ACPI_HANDLE(dev))
        return platform_acpi_match(pdev, pdrv);

    // 4. 传统名称匹配
    return strcmp(pdev->name, drv->name) == 0;
}
```

从上述代码可以看到，platform 总线的匹配顺序是：首先检查 id_table（主要用于 ACPI/静态表匹配），然后尝试设备树匹配，接着尝试 ACPI 匹配，最后进行传统的名称匹配。

**设备树匹配的具体过程**如下：`of_driver_match_device()` 函数检查驱动是否有设备树匹配表，如果有，则调用 `of_match_device()` 进行匹配：

```c
// 文件位置：drivers/of/device.c
const struct of_device_id *of_match_device(const struct of_device_id *matches,
                         const struct device *dev)
{
    const struct of_device_id *match;
    const char *const *compat;

    if (!matches)
        return NULL;

    for (match = matches; match->compatible[0]; match++) {
        // 获取设备的 compatible 属性
        compat = of_get_property(dev->of_node, "compatible", NULL);
        if (!compat)
            continue;

        // 比较 compatible 值
        for ( ; *compat; compat++) {
            if (of_match_string(match->compatible, *compat) == 0)
                return match;
        }
    }

    return NULL;
}
```

设备树匹配的核心是比较驱动的 `of_device_id.compatible` 与设备的设备树节点中的 `compatible` 属性。匹配成功后，返回匹配的 `of_device_id` 项，然后内核会调用驱动的 probe 函数。

**匹配失败的处理方式**：如果匹配失败（返回 NULL 或匹配函数返回 0），内核会继续遍历下一个驱动，直到找到匹配的驱动或者遍历完所有驱动。如果最终没有找到匹配的驱动，设备会保持未绑定状态，相关信息会通过 sysfs 导出，用户空间可以手动触发绑定。

**总结**：platform 设备和驱动的匹配过程是 Linux 设备模型的核心机制，它确保了正确的驱动能够管理对应的设备。匹配过程支持多种机制（设备树、ACPI、传统名称），内核会按照优先级依次尝试，直到找到匹配的驱动。这种设计既保证了向后兼容性，又支持了现代的设备树机制。

### 面试题2: 设备树节点转换为 platform_device 的过程

**问题描述**：请详细描述 Linux 内核如何将设备树（Device Tree）中的节点转换为 platform_device，包括转换的触发时机、转换的具体步骤、以及关键的数据结构转换。

**参考答案**：

设备树是现代 ARM 架构 Linux 内核描述硬件配置的主要方式。理解设备树节点如何转换为 platform_device，是进行基于设备树的驱动开发的基础。下面详细分析整个转换过程。

**设备树到 platform_device 转换的时机**是在内核启动阶段。当内核初始化时，会解析设备树二进制文件（dtb），然后调用设备树初始化函数开始创建 platform 设备。这个过程发生在 `start_kernel()` 函数中，具体在设备树子系统初始化时执行。

**转换的整体流程**可以概括为以下几个阶段：

**第一阶段：设备树子系统初始化**

在 Linux 5.x 内核中，设备树初始化入口是 `of_platform_default_bus_init()` 函数，它在设备模型初始化后期被调用：

```c
// 文件位置：drivers/of/platform.c
int __init of_platform_default_bus_init(void)
{
    // 初始化默认的 platform 总线
    // 创建根级别的 platform 设备
    of_platform_bus_create(NULL, NULL, NULL, NULL, true);

    return 0;
}
```

这个函数会为设备树的根节点创建一个虚拟的 bus 节点，然后递归遍历整个设备树，为每个节点创建相应的 platform_device。

**第二阶段：递归创建设备**

`of_platform_bus_create()` 函数是递归创建 platform 设备的核心函数：

```c
// 文件位置：drivers/of/platform.c
static int of_platform_bus_create(struct device_node *bus,
                  const struct of_device_id *matches,
                  struct device *parent, void *data, bool strict)
{
    struct device_node *child;
    struct platform_device *dev;
    const struct of_device_id *match;
    int rc = 0;

    // 为当前节点创建 platform_device
    dev = of_platform_device_create_pdata(bus, NULL, NULL, parent);

    if (!dev || !of_get_child_count(bus))
        return 0;

    // 遍历当前节点的所有子节点
    for_each_child_of_node(bus, child) {
        // 对于某些特殊总线（如 I2C、SPI），可能会被跳过
        // 因为它们有自己的总线驱动处理
        rc = of_platform_bus_create(child, matches, &dev->dev, data, strict);
        if (rc) {
            of_node_put(child);
            break;
        }
    }

    return rc;
}
```

这个函数会检查节点是否应该创建为 platform 设备，然后递归处理所有子节点。

**第三阶段：创建具体的 platform_device**

`of_platform_device_create_pdata()` 函数负责创建具体的 platform_device：

```c
// 文件位置：drivers/of/platform.c
static struct platform_device *of_platform_device_create_pdata(
    struct device_node *np,
    const char *platform_data,
    void *platform_data_size,
    struct device *parent)
{
    struct platform_device *dev;

    // 1. 检查设备是否可用（status != "disabled"）
    if (!of_device_is_available(np))
        return NULL;

    // 2. 检查是否有 compatible 属性（必要条件）
    if (!of_get_property(np, "compatible", NULL))
        return NULL;

    // 3. 分配 platform_device
    dev = platform_device_alloc(np->name, PLATFORM_DEVID_NONE);
    if (!dev)
        goto err_out;

    // 4. 设置设备树节点指针
    dev->dev.of_node = of_node_get(np);

    // 5. 设置父设备
    if (parent)
        dev->dev.parent = parent;

    // 6. 从设备树解析资源
    ret = of_device_add_resources(dev);
    if (ret)
        goto err_dev_put;

    // 7. 将设备添加到内核
    ret = platform_device_add(dev);
    if (ret)
        goto err_dev_put;

    return dev;

err_dev_put:
    platform_device_put(dev);
err_out:
    return NULL;
}
```

**第四阶段：资源解析**

从设备树节点创建 platform_device 时，最重要的步骤之一是将设备树的属性转换为平台设备的资源。这个工作由 `of_device_add_resources()` 函数完成：

```c
// 文件位置：drivers/of/device.c
static int of_device_add_resources(struct platform_device *dev)
{
    struct device_node *np = dev->dev.of_node;
    struct resource *r;
    int i, num_reg, num_irq;

    // 1. 计算 I/O 内存区域数量
    num_reg = of_address_to_resource(np, 0, NULL);

    // 2. 计算中断数量
    num_irq = of_irq_to_resource_table(np, NULL, 0);

    // 3. 如果没有资源，直接返回
    if (num_reg == 0 && num_irq == 0)
        return 0;

    // 4. 分配资源数组
    r = devm_kzalloc(&dev->dev, sizeof(*r) * (num_reg + num_irq), GFP_KERNEL);
    if (!r)
        return -ENOMEM;

    dev->resource = r;
    dev->num_resources = num_reg + num_irq;

    // 5. 解析 I/O 内存资源（从 reg 属性）
    if (num_reg)
        of_address_to_resource(np, 0, r);

    // 6. 解析中断资源（从 interrupts 属性）
    if (num_irq)
        of_irq_to_resource_table(np, r + num_reg, num_irq);

    return 0;
}
```

`of_address_to_resource()` 函数将设备树的 `reg` 属性转换为 `struct resource`：

```c
// 文件位置：drivers/of/address.c
int of_address_to_resource(struct device_node *dev, int index,
                struct resource *r)
{
    struct resource tmp;

    // 获取 "reg" 属性的第 index 个条目
    // 解析地址和大小
    // 考虑不同的地址空间（如 MMIO、PCI 等）

    // 将解析结果填充到 resource 结构体
    r->start = tmp.start;
    r->end = tmp.end;
    r->flags = IORESOURCE_MEM;
    r->name = dev->name;

    return 0;
}
```

`of_irq_to_resource_table()` 函数将设备树的 `interrupts` 属性转换为中断资源：

```c
// 文件位置：drivers/of/irq.c
int of_irq_to_resource_table(struct device_node *dev, struct resource *res,
                int num_resources)
{
    int i, irq;
    struct of_phandle_args oirq;

    for (i = 0; i < num_resources; i++) {
        // 解析 interrupts 属性的第 i 个条目
        irq = of_irq_parse_one(dev, i, &oirq);
        if (irq)
            break;

        // 映射中断号
        irq = irq_create_of_mapping(&oirq);
        if (irq == 0)
            break;

        // 填充 resource 结构体
        res[i].start = irq;
        res[i].end = irq;
        res[i].flags = IORESOURCE_IRQ;
        res[i].name = "irq";
    }

    return i;
}
```

**设备树到 platform_device 转换的详细流程图**如下：

1. 内核启动，解析设备树（dtb）
2. 调用 `of_platform_default_bus_init()` 初始化 platform 总线
3. 调用 `of_platform_bus_create()` 遍历设备树根节点
4. 对每个子节点调用 `of_platform_device_create_pdata()`：
   a. 检查节点是否启用（status != "disabled"）
   b. 检查节点是否有 compatible 属性
   c. 分配 platform_device 结构体
   d. 关联设备树节点（dev.of_node = np）
   e. 解析 reg 属性，创建 IORESOURCE_MEM 资源
   f. 解析 interrupts 属性，创建 IORESOURCE_IRQ 资源
   g. 调用 `platform_device_add()` 注册设备
5. 在 `platform_device_add()` 中调用 `device_add()`：
   a. 将设备添加到 platform 总线的设备链表
   b. 调用 `bus_probe_device()` 触发设备与驱动的匹配
6. 匹配成功后，调用驱动的 probe 函数，驱动获取资源并进行初始化

**关键数据结构转换**总结：

| 设备树属性 | 转换为 | platform_device 成员 |
|-----------|--------|----------------------|
| compatible | 字符串 | 用于驱动匹配 |
| reg | 数组 | resource[] (IORESOURCE_MEM) |
| interrupts | 数组 | resource[] (IORESOURCE_IRQ) |
| status | 布尔 | 决定是否创建设备 |
| name | 字符串 | device.dev.init_name |

理解设备树到 platform_device 的转换过程，对于调试设备驱动问题、理解驱动与硬件的绑定机制、以及开发新的设备树兼容驱动都非常重要。这个转换过程确保了 Linux 内核能够统一处理各种不同的硬件设备，无论是通过传统方式定义的还是通过现代设备树定义的。

# 第3章 设备树 (Device Tree)

设备树（Device Tree）是 Linux 内核中一种描述硬件配置的数据结构，它将板级硬件信息从内核代码中分离出来，使得同一份内核镜像可以支持不同的硬件平台。本章将深入探讨设备树的发展历史、语法结构、内核解析机制以及实际应用，帮助读者全面理解设备树在 Linux 驱动开发中的重要地位。

## 3.1 设备树概述与语法

### 3.1.1 设备树发展历史

设备树的概念起源于 OpenFirmware（OF）架构，最初被用于 SPARC 架构的工作站和服务器。OpenFirmware 是一种固件接口规范，它定义了一种与硬件平台无关的方式来描述系统硬件，设备树就是其中的核心数据结构。在 SPARC 系统中，固件会在系统启动时将设备树信息传递给操作系统内核，内核通过解析这些信息来了解硬件配置。

在早期的 ARM 架构中，情况完全不同。每个 ARM 开发板都需要在 Linux 内核的板级文件（board file）中硬编码硬件配置信息。这些板级文件通常位于 `arch/arm/mach-xxx/` 目录下，包含了大量与特定开发板紧密耦合的代码。例如，对于一款特定的 ARM 开发板，需要在板级文件中定义各个外设的基地址、中断号、GPIO 配置等信息。

这种做法很快就暴露出了严重的问题。随着 ARM 芯片的普及，市面上出现了数以千计的不同开发板，每款开发板都有略微不同的硬件配置。这导致了内核代码的急剧膨胀和碎片化——维护者需要为每款新开发板添加大量的板级文件代码，而这些代码之间存在大量重复。据统计，在鼎盛时期，ARM 相关的板级文件占据了内核代码库中的极大比例，这严重影响了内核的可维护性和可扩展性。

为了解决这一问题，Linus Torvalds 在 2011 年提出了一个具有里程碑意义的建议：将设备树（Device Tree）机制引入 ARM 架构。这一建议得到了 ARM 社区的广泛响应，随后设备树被正式引入 Linux 内核，并逐步成为描述硬件配置的标准方式。设备树的核心思想是将硬件配置信息从内核代码中分离出来，存储在一个独立的数据结构中（设备树源文件，DTS），内核在启动时读取这个数据结构来了解硬件配置。

设备树的引入带来了诸多好处。首先，它大大减少了 ARM 相关的内核代码量，因为不再需要为每款开发板编写专门的板级文件。其次，它提高了内核的可移植性，同一份内核镜像可以通过加载不同的设备树文件来支持不同的硬件平台。此外，设备树还为内核提供了更清晰的硬件描述方式，便于驱动的开发和维护。

设备树技术主要涉及三种文件格式：

**DTS（Device Tree Source）**：设备树源文件，是一种人类可读的文本格式，类似于 C 语言的语法结构。DTS 文件描述了系统的硬件拓扑结构，包括 CPU、内存、外设等所有硬件信息。

**DTSI（Device Tree Source Include）**：设备树源文件包含，类似于 C 语言的头文件。DTSI 文件定义了一些可被多个 DTS 文件共用的设备树节点和属性，方便代码复用。

**DTB（Device Tree Blob）**：设备树二进制文件，是 DTS 文件经过编译后生成的二进制格式。DTB 文件是内核实际读取的文件格式，它比 DTS 文件更紧凑，解析效率更高。

设备树的数据结构在内存中以平面形式存储（flattened tree），这是考虑到固件传递和内存效率的结果。设备树从根节点开始，通过子节点形成树形结构，每个节点包含若干属性来描述硬件的具体特性。

### 3.1.2 设备树节点语法

设备树采用树形结构来描述硬件系统的拓扑关系。树的根节点用斜杠（/）表示，所有其他节点都是根节点的子孙节点。每个节点由节点名称和一对大括号组成，节点内部可以包含属性定义和子节点定义。

下面是一个典型的设备树源文件示例，展示了设备树的基本语法结构：

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

下面对设备树的基本语法进行详细说明：

**节点命名规范**：设备树中的每个节点都有一个名称，节点名称由一个字符串和一个可选的单元地址组成，中间用 @ 分隔。例如，`memory@80000000` 表示一个名为 memory 的节点，其单元地址是 0x80000000。单元地址通常用来唯一标识同类型的多个节点。如果节点没有单元地址，可以直接使用字符串作为节点名称，如 `chosen` 节点。

**节点引用**：在设备树中，可以使用标签（label）来引用其他节点。上例中的 `&gic` 就是对带有 `gic:` 标签的节点的引用。这种引用方式在定义中断关系、设置中断父节点时特别有用。

**属性赋值**：节点内部可以包含属性定义，属性赋值使用等号（=）表示。属性值可以是多种类型，包括字符串、整数、布尔值等。

**数组表示**：在设备树中，整数值可以用尖括号括起来表示数组，如 `<0x80000000 0x10000000>` 表示一个包含两个 32 位整数的数组。这种表示方法在定义寄存器地址、地址范围等时非常常用。

**多行数组**：对于较长的数组，可以分成多行书写，如上例中的 reg 属性，分两行表示两个地址范围。

### 3.1.3 常用设备树属性

设备树通过属性来描述硬件的具体特性。下面详细介绍设备树中最常用的一些属性及其用法。

**compatible 属性**：compatible 属性是设备树中最重要的属性之一，它用于描述设备的兼容性信息。驱动通过读取这个属性来判断硬件是否是自己支持的设备。compatible 属性的值是一个字符串列表，按优先级从高到低排列。例如：

```dts
compatible = "arm,cortex-a9-gic", "arm,cortex-a9-gic-v2";
```

这个属性表示设备首先尝试匹配 "arm,cortex-a9-gic"，如果匹配失败，则尝试匹配 "arm,cortex-a9-gic-v2"。驱动在定义匹配表时，会按照这个顺序进行匹配。

在内核源码中，驱动通过 `of_match_table` 来指定支持的 compatible 字符串：

```c
// 文件位置：include/linux/of.h
static const struct of_device_id gpio_keys_of_match[] = {
    { .compatible = "gpio-keys", },
    { }
};
MODULE_DEVICE_TABLE(of, gpio_keys_of_match);
```

**reg 属性**：reg 属性用于描述设备的寄存器地址和大小信息。它的值是一个数组，每两个元素为一组，分别表示地址和长度。例如：

```dts
reg = <0x48000000 0x1000>;
```

这表示设备的寄存器起始地址是 0x48000000，长度是 0x1000（4KB）。如果设备有多个地址区域，可以用多组值：

```dts
reg = <0x48000000 0x1000>,
      <0x48001000 0x100>;
```

**interrupts 属性**：interrupts 属性用于描述设备产生的中断信息。它的值也是一个数组，每个中断用一个或多个整数来表示，具体含义取决于中断控制器的规格。常见的格式是：

```dts
interrupts = <0 1 4>;
```

这三个数字分别表示：中断号（0）、触发方式（1，上升沿触发）、中断类型（4，共享中断）。这些数字的含义由中断控制器节点的 `#interrupt-cells` 属性决定。

**interrupt-parent 属性**：interrupt-parent 属性用于指定设备的中断父节点。如果没有显式指定，设备会继承父节点的中断父节点。这个属性通常使用节点引用的方式设置：

```dts
interrupt-parent = <&gic>;
```

**phandle 属性**：phandle 是设备树中用于唯一标识节点的值。在设备树源码中，可以使用标签来引用节点，编译时编译器会将标签转换为 phandle 值。phandle 实际上是一个 32 位的整数，在设备树二进制文件中用于标识目标节点。

**status 属性**：status 属性用于描述设备的当前状态。常用的值包括：

- `"okay"` 或 `"ok"`：设备处于激活状态
- `"disabled"`：设备被禁用
- `"reserved"`：设备被保留，不使用
- `"fail"` 或 `"fail-${reason}"`：设备初始化失败

```dts
status = "okay";    // 启用设备
status = "disabled"; // 禁用设备
```

当设备被禁用时，内核不会为该设备创建 platform 设备，驱动也不会尝试访问该硬件。

**#address-cells 和 #size-cells 属性**：这两个属性用于指定子节点中 reg 属性的格式。`#address-cells` 指定子节点 reg 属性中地址部分需要多少个 32 位整数，`#size-cells` 指定长度部分需要多少个 32 位整数。

```dts
soc {
    #address-cells = <1>;
    #size-cells = <1>;

    serial@48000000 {
        reg = <0x48000000 0x1000>;
    };
};
```

在这个例子中，`#address-cells = <1>` 表示地址用一个 32 位整数表示，`#size-cells = <1>` 表示长度用一个 32 位整数表示。

## 3.2 设备树在内核中的解析

### 3.2.1 解析流程

设备树在内核中的解析是一个复杂的过程，涉及从固件传递 DTB 到内核创建 platform 设备的多个步骤。理解这个解析流程对于调试设备树相关问题非常重要。

设备树解析的完整流程如下：

**第一步：Bootloader 加载 DTB**

在系统启动阶段，Bootloader（如 U-Boot）负责将设备树 Blob（DTB）加载到内存中的特定地址。这个地址通过启动参数传递给 Linux 内核。常见的加载方式有两种：

1. **ATAG 方式**（较老）：通过 ATAG 列表传递设备树地址，ATAG_CORE 之后的 ATAG_DTB 包含 DTB 的物理地址。

2. **FDT 方式**（现代）：通过设备树_blob（dt_phys）内核启动参数传递 DTB 地址。这是目前主流的方式。

在内核命令行中，可以这样指定设备树：

```
bootz 0x10000000 - 0x18000000
```

其中 0x18000000 是设备树 blob 的加载地址。

**第二步：内核启动时解析 DTB**

当 Linux 内核开始启动时，它会解析传入的 DTB 数据。这个过程在内核初始化的早期阶段完成，主要涉及以下文件中的代码：

```c
// 文件位置：arch/arm/kernel/setup.c
static void __init setup_machine_fdt(phys_addr_t dt_phys)
{
    void *dt_virt;
    struct boot_param_header *hdr;

    // 将物理地址转换为虚拟地址
    dt_virt = ioremap_cache(dt_phys, fdt_totalsize(dt_phys));

    // 验证设备树的魔数
    if (fdt_check_header(dt_virt)) {
        pr_err("Invalid device tree blob at physical address %pa\n", &dt_phys);
        while (1);
    }

    // 获取设备树根节点
    initial_boot_params = dt_virt;

    // 设置机器描述符
    machine_desc = mdesc;
}
```

内核首先验证设备树的有效性（检查魔数、版本等），然后将 DTB 的虚拟地址保存到 `initial_boot_params` 变量中，供后续解析使用。

**第三步：创建 of_platform_device**

解析完设备树后，内核会遍历设备树节点，为每个节点创建相应的 platform_device。这个过程通过 `of_platform_bus_create()` 函数完成：

```c
// 文件位置：drivers/of/platform.c
static int of_platform_bus_create(struct device_node *bus,
                  const struct of_device_id *matches,
                  bool strict)
{
    struct platform_device *dev;
    const char *status;
    int rc = 0;

    // 检查节点是否应该被跳过
    status = of_get_property(bus, "status", NULL);
    if (status && strcmp(status, "disabled") == 0)
        return 0;

    // 检查兼容性
    if (strict && !of_match_node(matches, bus))
        return 0;

    // 创建 platform_device
    dev = of_platform_device_create(bus, NULL, &platform_bus);
    if (!dev || !of_node_check_flag(bus, OF_POPULATED))
        return 0;

    // 递归处理子节点
    for_each_child_of_node(bus, child) {
        rc = of_platform_bus_create(child, matches, strict);
        if (rc) {
            of_node_put(child);
            break;
        }
    }

    return rc;
}
```

这个函数会递归遍历设备树中的每个节点，对于每个符合条件的节点，调用 `of_platform_device_create()` 创建对应的 platform_device。

**第四步：驱动匹配**

当 platform_device 和 platform_driver 都注册到系统后，内核会自动进行匹配。匹配成功后，驱动的 `probe` 函数会被调用，驱动可以在这个函数中获取设备的资源信息，并进行初始化。

设备树到 platform 设备的转换过程可以用下图表示：

```
+------------------+     +-------------------+     +------------------+
|  设备树源文件    | --> |  设备树二进制    | --> | of_platform_    |
|  (DTS/DTSI)      |     |  (DTB)           |     | device          |
+------------------+     +-------------------+     +------------------+
                                                             |
                                                             v
                                                    +------------------+
                                                    | platform_device  |
                                                    +------------------+
```

### 3.2.2 关键 API

Linux 内核提供了一套丰富的设备树 API，驱动开发者可以使用这些 API 在运行时获取设备树中的信息。以下是一些最常用的 API：

**查找节点相关 API**

设备树节点查找是最基本的操作，内核提供了多种方式来查找节点：

```c
// 文件位置：include/linux/of.h

// 通过路径查找节点
struct device_node *of_find_node_by_path(const char *path);

// 通过属性值查找节点
struct device_node *of_find_compatible_node(
    struct device_node *from,
    const char *type,
    const char *compatible
);

// 通过设备ID数组查找节点
struct device_node *of_find_matching_node(
    struct device_node *from,
    const struct of_device_id *matches
);

// 通过phandle查找节点
struct device_node *of_find_node_by_phandle(phandle handle);

// 释放节点引用
void of_node_put(struct device_node *node);
```

以下是这些 API 的使用示例：

```c
// 通过路径查找节点
struct device_node *np = of_find_node_by_path("/soc/gic");
if (!np) {
    pr_err("Failed to find /soc/gic node\n");
    return -ENODEV;
}

// 通过compatible字符串查找节点
struct device_node *gpio_np = of_find_compatible_node(
    NULL,           // 从根节点开始搜索
    NULL,          // 不限制节点类型
    "arm,gpio-keys" // compatible字符串
);

// 通过设备ID数组查找
static const struct of_device_id my_driver_of_match[] = {
    { .compatible = "vendor,my-device", },
    { }
};

struct device_node *match_np = of_find_matching_node(
    NULL,
    my_driver_of_match
);

// 使用完后释放引用
of_node_put(np);
```

**读取属性相关 API**

获取设备树节点后，可以通过以下 API 读取节点的属性：

```c
// 文件位置：include/linux/of.h

// 读取32位整数属性
int of_property_read_u32(
    const struct device_node *np,
    const char *propname,
    u32 *out_value
);

// 读取32位整数数组
int of_property_read_u32_array(
    const struct device_node *np,
    const char *propname,
    u32 *out_values,
    size_t sz
);

// 读取64位整数
int of_property_read_u64(
    const struct device_node *np,
    const char *propname,
    u64 *out_value
);

// 读取字符串属性
int of_property_read_string(
    const struct device_node *np,
    const char *propname,
    const char **out_string
);

// 读取字符串数组
int of_property_read_string_array(
    const struct device_node *np,
    const char *propname,
    const char **out_strings,
    size_t sz
);

// 检查属性是否存在
bool of_property_read_bool(
    const struct device_node *np,
    const char *propname
);

// 读取属性
struct property *of_find_property(
    const struct device_node *np,
    const char *name,
    int *lenp
);
```

这些 API 的使用示例：

```c
// 读取单个32位整数
u32 reg_offset;
ret = of_property_read_u32(np, "reg-offset", &reg_offset);
if (ret) {
    pr_err("Failed to read reg-offset property\n");
    return ret;
}

// 读取32位整数数组
u32 reg[2];
ret = of_property_read_u32_array(np, "reg", reg, 2);
if (ret) {
    pr_err("Failed to read reg property\n");
    return ret;
}

// 读取64位整数（用于地址高位）
u64 phys_addr;
ret = of_property_read_u64(np, "phys-addr", &phys_addr);

// 读取字符串
const char *compatible;
ret = of_property_read_string(np, "compatible", &compatible);
if (!ret) {
    pr_info("Device compatible: %s\n", compatible);
}

// 读取布尔属性
if (of_property_read_bool(np, "wakeup-source")) {
    device_set_wakeup_capable(dev, true);
}

// 检查属性是否存在
if (of_find_property(np, "interrupt-names", NULL)) {
    // 属性存在
}
```

**获取资源相关 API**

设备树中的地址和中断信息需要转换为内核的资源结构体：

```c
// 文件位置：include/linux/of.h
#include <linux/ioport.h>

// 将设备树的地址信息转换为resource结构体
int of_address_to_resource(
    struct device_node *dev,
    int index,
    struct resource *r
);

// 将设备树的中断信息转换为resource结构体
int of_irq_to_resource(
    struct device_node *dev,
    int index,
    struct resource *r
);

// 获取设备的IO内存资源
int of_dma_configure(
    struct device *dev,
    struct device_node *np,
    bool force_dma32
);
```

使用示例：

```c
// 获取设备的寄存器地址
struct resource r;
int ret = of_address_to_resource(np, 0, &r);
if (ret) {
    pr_err("Failed to get device address\n");
    return ret;
}

pr_info("Device address: %pa, size: %pa\n", &r.start, &r.end);

// 映射IO内存
void __iomem *base = ioremap(r.start, resource_size(&r));
if (!base) {
    pr_err("Failed to ioremap\n");
    return -ENOMEM;
}

// 获取中断号
struct resource irq_r;
ret = of_irq_to_resource(np, 0, &irq_r);
if (ret == 0) {
    int irq = irq_r.start;
    pr_info("Device IRQ: %d\n", irq);

    // 请求中断
    ret = request_irq(irq, my_irq_handler, IRQF_SHARED,
                      "my-device", dev);
    if (ret) {
        pr_err("Failed to request IRQ\n");
        return ret;
    }
}
```

**遍历设备树相关 API**

有时需要遍历设备树中的所有子节点：

```c
// 文件位置：include/linux/of.h

// 获取第一个子节点
struct device_node *of_get_child_by_name(
    const struct device_node *node,
    const char *name
);

// 遍历所有子节点（推荐方式）
#define for_each_child_of_node(parent, child) \
    for (child = of_get_next_child(parent, NULL); child != NULL; \
         child = of_get_next_child(parent, child))

// 获取下一个兄弟节点
struct device_node *of_get_next_sibling(
    const struct device_node *node,
    const struct device_node *prev
);

// 获取下一个子节点
struct device_node *of_get_next_child(
    const struct device_node *node,
    struct device_node *prev
);
```

使用示例：

```c
// 遍历所有子节点
struct device_node *child;
for_each_child_of_node(parent_np, child) {
    const char *name = child->name;
    const char *compatible;

    if (of_property_read_string(child, "compatible", &compatible))
        continue;

    pr_info("Child node: %s, compatible: %s\n", name, compatible);
}
```

### 3.2.3 源码分析：of_platform_device_create

`of_platform_device_create()` 函数是设备树转换为 platform 设备的核心函数。深入理解这个函数的实现对于掌握设备树机制非常重要。

```c
// 文件位置：drivers/of/platform.c
static struct platform_device *of_platform_device_create(
    struct device_node *np,
    const char *bus_id,
    struct device *parent)
{
    struct platform_device *dev;
    int rc;

    // 检查status属性，如果是disabled则不创建设备
    if (of_node_test_and_set_flag(np, OF_POPULATED))
        return NULL;

    rc = of_platform_check_children(np);
    if (rc) {
        of_node_clear_flag(np, OF_POPULATED);
        return NULL;
    }

    // 分配platform_device结构体
    dev = of_platform_device_alloc(np, bus_id, parent);
    if (!dev)
        goto err_clear_flag;

    // 设置DMA配置
    rc = of_dma_configure(&dev->dev, np, false);
    if (rc) {
        platform_device_put(dev);
        goto err_clear_flag;
    }

    // 添加设备到系统中
    rc = platform_device_add(dev);
    if (rc)
        goto err_dma_unconfigure;

    return dev;

err_dma_unconfigure:
    of_dma_deconfigure(&dev->dev);
    platform_device_put(dev);
err_clear_flag:
    of_node_clear_flag(np, OF_POPULATED);
    return NULL;
}
```

这个函数的执行流程如下：

**第一步：检查节点状态**

```c
// 检查设备树节点是否已经被处理过
if (of_node_test_and_set_flag(np, OF_POPULATED))
    return NULL;
```

设备树节点通过 OF_POPULATED 标志来标记是否已经被处理过。如果这个标志已经被设置，说明这个节点已经创建了对应的设备，不需要重复处理。

**第二步：检查子节点**

```c
rc = of_platform_check_children(np);
if (rc) {
    of_node_clear_flag(np, OF_POPULATED);
    return NULL;
}
```

这个函数检查节点是否有子节点需要处理。如果有子节点，会递归处理子节点，然后返回。

**第三步：分配 platform_device**

```c
dev = of_platform_device_alloc(np, bus_id, parent);
```

这个函数分配并初始化 platform_device 结构体：

```c
// 文件位置：drivers/of/platform.c
static struct platform_device *of_platform_device_alloc(
    struct device_node *np,
    const char *bus_id,
    struct device *parent)
{
    struct platform_device *dev;
    const char *name;
    int id;

    // 获取设备名称
    name = of_get_property(np, "name", NULL);
    if (!name)
        name = np->name;

    // 获取设备ID
    id = of_alias_get_id((struct device_node *)np, name);
    if (id < 0)
        id = 0;

    // 分配platform_device
    dev = platform_device_alloc(name, id);
    if (!dev)
        return NULL;

    // 设置父设备
    if (parent)
        dev->dev.parent = parent;

    // 设置OF节点
    dev->dev.of_node = of_node_get(np);

    // 从设备树获取资源
    of_platform_device_create_populate(np, dev);

    return dev;
}
```

在这个函数中，关键的一步是 `of_platform_device_create_populate()`，它负责从设备树节点中提取资源信息并填充到 platform_device 中：

```c
static void of_platform_device_create_populate(
    struct device_node *np,
    struct platform_device *dev)
{
    // 添加中断资源
    of_irq_count = of_property_count_elems_of_size(np, "interrupts", sizeof(u32));
    if (of_irq_count) {
        // 为每个中断分配resource
        // ...
    }

    // 添加内存资源
    of_address_count = of_property_count_elems_of_size(np, "reg", sizeof(u32));
    if (of_address_count >= 2) {
        // 为每个地址范围分配resource
        // ...
    }
}
```

**第四步：设置 DMA 配置**

```c
rc = of_dma_configure(&dev->dev, np, false);
```

这个函数从设备树中读取 DMA 相关属性（如 dma-ranges、dma-coherent 等）并配置设备的 DMA 参数。

**第五步：添加到系统**

```c
rc = platform_device_add(dev);
```

最后，将创建好的 platform_device 添加到平台总线的设备链表中，等待与驱动进行匹配。

## 3.3 设备树与 platform 设备

### 3.3.1 设备树节点到 platform 设备的转换

设备树节点转换为 platform 设备的过程涉及多个步骤，每个步骤都有特定的函数负责。下面详细分析这个转换过程。

**设备树节点的结构**

在 Linux 内核中，设备树节点通过 `struct device_node` 结构体表示：

```c
// 文件位置：include/linux/of.h
struct device_node {
    const char *name;              // 节点名称
    const char *full_name;         // 完整路径名称
    struct property *properties;  // 属性链表
    struct device_node *parent;   // 父节点
    struct device_node *child;    // 子节点
    struct device_node *sibling;  // 兄弟节点
    struct kobj_uevent_env *env;   // uevent环境变量
    phandle phandle;              // phandle值
    unsigned long _flags;         // 标志位
    void *data;
};
```

`device_node` 结构体通过 `_flags` 中的 `OF_POPULATED` 和 `OF_POPULATING` 标志来跟踪设备创建的状态。

**从节点到设备的转换**

当内核初始化时，它会调用 `of_platform_populate()` 函数来启动设备树的转换过程：

```c
// 文件位置：drivers/of/platform.c
int of_platform_populate(struct device_node *root,
            const struct of_device_id *matches,
            const struct of_platform_driver *driver,
            struct device *parent)
{
    struct device_node *node;
    int rc = 0;

    // 如果没有指定根节点，使用默认的设备树根节点
    if (!root)
        root = of_find_node_by_path("/");
    if (!root)
        return -EINVAL;

    // 遍历根节点的直接子节点
    for_each_child_of_node(root, node) {
        rc = of_platform_bus_create(node, matches, driver != NULL);
        if (rc) {
            of_node_put(node);
            break;
        }
    }

    of_node_put(root);
    return rc;
}
```

这个函数从设备树的根节点开始，遍历所有子节点，对每个节点调用 `of_platform_bus_create()`。`of_platform_bus_create()` 会进一步递归处理所有子节点。

**资源的收集与转换**

设备树节点中的地址和中断信息需要被转换为内核可以使用的资源格式：

```c
// 文件位置：drivers/of/base.c
int of_device_request_resources(struct platform_device *dev)
{
    struct resource *r;
    int i;

    // 遍历所有资源
    for (i = 0; i < dev->num_resources; i++) {
        r = &dev->resource[i];

        // 根据资源类型进行处理
        switch (resource_type(r)) {
        case IORESOURCE_MEM:
            // 内存映射IO
            break;
        case IORESOURCE_IRQ:
            // 中断请求
            break;
        case IORESOURCE_DMA:
            // DMA通道
            break;
        }
    }

    return 0;
}
```

驱动在 `probe` 函数中可以通过以下方式获取这些资源：

```c
static int my_driver_probe(struct platform_device *dev)
{
    struct resource *r;
    void __iomem *base;
    int irq;

    // 获取内存资源
    r = platform_get_resource(dev, IORESOURCE_MEM, 0);
    if (!r) {
        dev_err(&dev->dev, "Failed to get memory resource\n");
        return -ENODEV;
    }

    base = devm_ioremap_resource(&dev->dev, r);
    if (IS_ERR(base))
        return PTR_ERR(base);

    // 获取中断资源
    irq = platform_get_irq(dev, 0);
    if (irq < 0) {
        dev_err(&dev->dev, "Failed to get IRQ\n");
        return irq;
    }

    dev_info(&dev->dev, "Probe successful\n");
    return 0;
}
```

### 3.3.2 驱动匹配方式

Linux 内核支持多种驱动匹配机制，设备树匹配是其中最重要的一种。驱动可以通过三种方式与设备进行匹配：

**1. of_match_table（设备树兼容字符串匹配）**

这是设备树环境下最主要的匹配方式。驱动定义一个 `of_device_id` 数组，列出所有支持的设备兼容字符串：

```c
// 文件位置：include/linux/mod_devicetable.h
struct of_device_id {
    char    name[32];          // 节点名称（可选）
    char    type[32];          // 设备类型（可选）
    char    compatible[128];  // 兼容字符串
    const void *data;          // 匹配后的私有数据
};
```

驱动定义示例：

```c
// 定义匹配表
static const struct of_device_id my_driver_of_match[] = {
    { .compatible = "vendor,my-device-v1" },
    { .compatible = "vendor,my-device-v2" },
    { .compatible = "vendor,my-device" },
    { }
};
MODULE_DEVICE_TABLE(of, my_driver_of_match);

// 定义platform_driver
static struct platform_driver my_driver = {
    .probe = my_driver_probe,
    .remove = my_driver_remove,
    .driver = {
        .name = "my-driver",
        .of_match_table = my_driver_of_match,
    },
};
```

在内核中，匹配过程通过以下函数实现：

```c
// 文件位置：drivers/of/device_id.c
static int of_device_match(struct device *dev,
               struct device_driver *drv)
{
    const struct of_device_id *matches = drv->of_match_table;

    if (!matches)
        return 0;

    // 遍历所有匹配项
    for (; matches->compatible[0]; matches++) {
        // 检查compatible字符串是否匹配
        if (of_device_compatible_match(dev->of_node, matches->compatible)) {
            return 1;
        }
    }

    return 0;
}
```

匹配时，内核会比较设备的 `compatible` 属性与驱动 `of_match_table` 中的每个条目。如果找到匹配项，`probe` 函数会被调用。

**2. platform_device_id（传统平台设备匹配）**

这是一种传统的匹配方式，主要用于非设备树的平台设备。驱动定义一个 `platform_device_id` 数组：

```c
// 文件位置：include/linux/mod_devicetable.h
struct platform_device_id {
    char name[PLATFORM_NAME_SIZE];
    kernel_ulong_t driver_data;
};

static const struct platform_device_id my_id_table[] = {
    { "my-device-v1", (kernel_ulong_t)&device_v1_data },
    { "my-device-v2", (kernel_ulong_t)&device_v2_data },
    { }
};
MODULE_DEVICE_TABLE(platform, my_id_table);

static struct platform_driver my_driver = {
    .probe = my_driver_probe,
    .id_table = my_id_table,
    .driver = {
        .name = "my-driver",
    },
};
```

在内核启动早期（设备树普及之前），板级文件会静态创建 platform_device，这些设备使用 `platform_device_id` 来指定设备名称。驱动通过设备名称进行匹配。

**3. ACPI match（ACPI 方式）**

ACPI（Advanced Configuration and Power Interface）是 x86 架构的一种硬件配置标准。在某些场景下，内核也会使用 ACPI 来获取硬件信息。驱动可以通过 ACPI ID 进行匹配：

```c
static const struct acpi_device_id my_acpi_match[] = {
    { "ABCD1234", 0 },
    { }
};
MODULE_DEVICE_TABLE(acpi, my_acpi_match);

static struct platform_driver my_driver = {
    .driver = {
        .acpi_match_table = my_acpi_match,
    },
};
```

在内核中，匹配优先级通常是：设备树匹配 > ACPI 匹配 > 传统平台设备匹配。

## 3.4 Device Tree Overlay

### 3.4.1 Overlay 概述

Device Tree Overlay（设备树覆盖）是一种动态修改设备树的机制，允许在系统运行时添加、修改或删除设备树节点，而无需重新编译设备树或重启系统。这对于模块化硬件（如可插拔的扩展板、热插拔设备）特别有用。

Overlay 的概念最初来源于 FPGA 开发领域，开发者需要在运行时动态加载.bit 文件来配置 FPGA。随着嵌入式系统的发展，Overlay 机制被引入到设备树中，用于支持更灵活的硬件配置。

Device Tree Overlay 的主要应用场景包括：

- **模块化扩展板**：树莓派（HAT - Hardware Attached on Top）使用 Overlay 来定义扩展板的外设信息
- **热插拔设备**：在系统运行时动态添加新的硬件设备
- **硬件配置变更**：根据不同的硬件配置加载不同的设备参数
- **调试与开发**：在开发过程中动态调整设备参数，而无需重新编译设备树

Overlay 的工作原理是在现有的设备树基础上应用"补丁"，修改或添加特定的节点。每个 Overlay 文件也是一个设备树源文件（DTS），但它包含的是需要修改或添加的内容。

### 3.4.2 Overlay 使用方法

**Overlay 文件格式**

Overlay 文件通常包含以下几个部分：

```dts
// 包含原始设备树的头部信息
/dts-v1/;
/plugin/;

/ {
    // 片段版本信息
    fragment@0 {
        target-path = "/";

        __overlay__ {
            // 添加新的节点
            my-new-device {
                compatible = "vendor,my-device";
                reg = <0x48000000 0x1000>;
                status = "okay";
            };
        };
    };
};
```

另一种常用的方式是使用 target 引用：

```dts
/dts-v1/;
/plugin/;

/ {
    fragment@0 {
        target = <&soc>;

        __overlay__ {
            new-gpio-controller {
                compatible = "vendor,new-gpio";
                #gpio-cells = <2>;
                status = "okay";
            };
        };
    };
};
```

**编译 Overlay**

Overlay 文件需要先编译成 DTBO（Device Tree Blob Overlay）格式：

```bash
# 编译 Overlay
dtc -I dts -O dtb -o overlay.dtbo overlay.dts

# 编译多个 Overlay
dtc -I dts -O dtb -o overlay1.dtbo overlay1.dts
dtc -I dts -O dtb -o overlay2.dtbo overlay2.dts
```

**加载 Overlay**

在 Linux 系统中，可以通过以下方式加载 Overlay：

**方式一：使用 configfs（推荐）**

```bash
# 创建 overlay 实例
mkdir /sys/kernel/config/device-tree/overlays/overlay1

# 加载 overlay 文件
cat overlay.dtbo > /sys/kernel/config/device-tree/overlays/overlay1/dtbo

# 查看状态
cat /sys/kernel/config/device-tree/overlays/overlay1/status

# 卸载 overlay
echo 1 > /sys/kernel/config/device-tree/overlays/overlay1/remove
```

**方式二：使用 device tree overlay API**

在内核模块中，可以使用设备树 overlay API：

```c
#include <linux/of.h>
#include <linux/of_fdt.h>

// 加载设备树 overlay
int of_overlay_fdt_apply(void *overlay_fdt, int overlay_fdt_size,
                         int *out_nodeid);

// 移除设备树 overlay
int of_overlay_remove(int tree_id);

// 移除所有 overlay
int of_overlay_remove_all(void);
```

示例代码：

```c
static int load_overlay(struct device *dev, const char *overlay_path)
{
    struct device_node *overlay_np;
    int ret;
    void *blob;
    loff_t size;

    // 读取 overlay 文件
    struct file *fp = filp_open(overlay_path, O_RDONLY, 0);
    if (IS_ERR(fp))
        return PTR_ERR(fp);

    size = i_size_read(fp->f_path.dentry->d_inode);
    blob = vmalloc(size);
    if (!blob) {
        filp_close(fp, NULL);
        return -ENOMEM;
    }

    kernel_read(fp, blob, size, &fp->f_pos);
    filp_close(fp, NULL);

    // 应用 overlay
    ret = of_overlay_fdt_apply(blob, size, NULL);
    vfree(blob);

    return ret;
}
```

**Overlay 的状态管理**

每个 overlay 都有对应的状态，可以通过以下方式查询：

```bash
# 查看所有 overlay
ls -la /sys/kernel/config/device-tree/overlays/

# 查看单个 overlay 的详细信息
cat /sys/kernel/config/device-tree/overlays/overlay1/status
```

状态值说明：

- `"0"` 或空：overlay 未应用
- `"1"`：overlay 已成功应用
- `"-1"`：overlay 应用失败

**Overlay 应用的限制**

使用 Overlay 时需要注意以下几点：

1. **不能删除原始节点**：Overlay 只能添加新节点或修改现有节点的属性，但不能完全删除原始节点

2. **不能修改某些属性**：某些关键属性（如 compatible）不能通过 Overlay 修改

3. **依赖关系**：如果 Overlay 依赖于其他节点，需要确保目标节点存在

4. **资源冲突**：添加的设备不能与现有设备使用相同的资源（如地址、中断号）

## 3.5 DTB 编译与打包

### 3.5.1 编译工具

设备树编译工具 `dtc`（Device Tree Compiler）是编译设备树源文件的必备工具。它可以将人类可读的 DTS 格式转换为内核需要的 DTB 格式，也可以进行反向转换。

**dtc 工具安装**

在大多数 Linux 发行版中，可以通过包管理器安装：

```bash
# Ubuntu/Debian
sudo apt-get install device-tree-compiler

# CentOS/RHEL
sudo yum install dtc

# 编译安装（获取最新版本）
git clone https://github.com/dgibson/dtc.git
cd dtc
make
sudo make install
```

**基本编译命令**

```bash
# DTS 编译为 DTB
dtc -I dts -O dtb -o output.dtb input.dts

# DTB 反编译为 DTS
dtc -I dtb -O dts -o output.dts input.dtb
```

常用参数说明：

- `-I <format>`：输入格式（dts、dtb、fs）
- `-O <format>`：输出格式（dts、dtb、fs）
- `-o <file>`：输出文件
- `-i <dir>`：包含文件搜索目录（用于处理 include）
- `-@`：启用标签处理
- `-W <warning>`：禁用特定警告

**完整编译示例**

假设有如下设备树文件结构：

```
project/
  board.dts          # 主设备树文件
  board-pins.dtsi    # 引脚配置
  soc.dtsi           # SoC 定义
  imx6ul.dtsi        # 芯片定义
```

主文件 board.dts 内容：

```dts
/dts-v1/;
#include "soc.dtsi"
#include "imx6ul.dtsi"
#include "board-pins.dtsi"

/ {
    model = "My ARM Board";
    compatible = "vendor,my-board", "fsl,imx6ul";

    chosen {
        bootargs = "console=ttymxc0,115200";
    };
};
```

编译命令：

```bash
# 编译
dtc -I dts -O dtb -o board.dtb -i . board.dts

# 或者使用 -@ 选项（如果需要标签引用）
dtc -I dts -O dtb -o board.dtb -i . -@ board.dts

# 编译多个文件
dtc -I dts -O dtb -o board.dtb \
    -i . \
    -i /path/to/kernel/scripts/dtc \
    board.dts
```

**dtc 的高级用法**

```bash
# 生成符号表（用于调试）
dtc -I dts -O dtb -o board.dtb -s board.dts -p 1000

# 检查语法错误
dtc -I dts -O dtb -W no-unit_address_vs_reg -o board.dtb board.dts

# 输出更详细的警告
dtc -I dts -O dtb -W all -o board.dtb board.dts

# 输出到标准输出
dtc -I dts -O dts board.dts

# 使用 FS（文件系统）格式
dtc -I fs -O dts /proc/device-tree
```

### 3.5.2 内核配置

要在 Linux 内核中使用设备树，需要启用相关的配置选项。以下是设备树相关的内核配置项：

**核心配置选项**

```makefile
# 设备树支持（必须启用）
CONFIG_OF=y
CONFIG_OF_DYNAMIC=y
CONFIG_OF_EARLY_FLATTREE=y
CONFIG_OF_FLATTREE=y
CONFIG_OF_ADDRESS=y
CONFIG_OF_IRQ=y
CONFIG_OF_NET=y
CONFIG_OF_MDIO=y
CONFIG_OF_RESOLVE=y

# 设备树 overlay 支持
CONFIG_OF_OVERLAY=y
CONFIG CONFIG_OF_CONFIGFS=y
```

**配置说明**

**CONFIG_OF=y**：设备树核心支持，启用设备树的基础功能。

**CONFIG_OF_DYNAMIC=y**：动态设备树支持，允许在运行时修改设备树（overlay 需要此选项）。

**CONFIG_OF_FLATTREE=y**：扁平化设备树支持，内核需要将设备树从树形结构转换为扁平数组进行解析。

**CONFIG_OF_EARLY_FLATTREE=y**：早期扁平化设备树支持，在内核初始化早期阶段解析设备树。

**CONFIG_OF_ADDRESS=y**：设备树地址转换支持，提供从设备树地址到物理地址的转换函数。

**CONFIG_OF_IRQ=y**：设备树中断支持，提供从设备树中断描述到中断号的转换函数。

**CONFIG_OF_NET=y**：设备树网络设备支持，内置以太网控制器的设备树支持。

**CONFIG_OF_MDIO=y**：MDIO 总线设备树支持，内置 MDIO 总线的设备树支持。

**CONFIG_OF_OVERLAY=y**：设备树 Overlay 支持，允许动态加载设备树 Overlay。

**CONFIG_OF_CONFIGFS=y**：Configfs 设备树 Overlay 支持，通过 configfs 接口管理 Overlay。

**启用设备树后的内核启动参数**

当内核配置了设备树支持后，启动参数有以下变化：

1. **内核镜像与设备树分离**：内核镜像（zImage、Image）不再包含设备树，需要单独传递

2. **启动参数变化**：使用 FDT 方式传递设备树地址

```
# ARM32
bootz <kernel_addr> - <dtb_addr>

# ARM64
booti <kernel_addr> - <dtb_addr>
```

**设备树在内核中的位置**

设备树相关代码在内核源码中的分布：

```
arch/arm/boot/dts/          # ARM 设备树源文件
arch/arm64/boot/dts/        # ARM64 设备树源文件
arch/riscv/boot/dts/        # RISC-V 设备树源文件

drivers/of/                 # 设备树核心驱动
drivers/of/of_*.c          # 各种设备树 API 实现
drivers/of/fdt_*.c         # 设备树扁平化相关
drivers/of/platform.c      # 平台设备创建

include/linux/of*.h         # 设备树 API 头文件

scripts/dtc/                # 设备树编译器源码
```

## 本章面试题

### 面试题1：设备树中 compatible 属性的作用是什么？

**参考答案：**

compatible 属性是设备树中最核心的属性之一，它的主要作用是描述设备的兼容性信息，为内核驱动提供设备识别的依据。具体来说，compatible 属性的作用包括以下几点：

**1. 设备识别与驱动匹配**

compatible 属性用于驱动与设备的匹配过程。当 platform_device 和 platform_driver 注册到系统后，内核会比较设备的 compatible 属性与驱动 `of_match_table` 中定义的匹配字符串。如果找到匹配项，内核就会调用驱动的 probe 函数。

```c
// 设备树节点
compatible = "vendor,my-device-v2", "vendor,my-device-v1";

// 驱动匹配表
static const struct of_device_id my_driver_of_match[] = {
    { .compatible = "vendor,my-device-v2" },
    { .compatible = "vendor,my-device-v1" },
    { }
};
```

匹配顺序是从列表的第一个字符串开始，优先匹配更具体的版本。这种设计允许驱动支持多个版本的硬件，通过驱动数据（driver_data）区分不同版本的硬件特性。

**2. 驱动数据传递**

通过 compatible 匹配，驱动可以获取对应的驱动数据：

```c
static int my_probe(struct platform_device *dev)
{
    const struct of_device_id *match;

    match = of_match_device(my_driver_of_match, &dev->dev);
    if (match && match->data) {
        const struct my_driver_data *data = match->data;
        // 使用驱动数据
    }
}
```

**3. 层次化硬件描述**

compatible 属性通常包含供应商前缀（如 "vendor,device"），这有助于避免不同供应商之间的命名冲突。推荐的命名格式是：`"供应商,设备型号"` 或 `"供应商,设备型号-版本"`。

**4. 总线级联识别**

在复杂的 SoC 系统中，compatible 属性还用于识别总线控制器（如 AMBA、PCIe、USB），帮助内核构建正确的设备层次结构。

### 面试题2：设备树和平台设备的绑定过程是怎样的？

**参考答案：**

设备树到平台设备的绑定过程是一个从硬件描述到内核设备对象的转换过程，涉及多个关键步骤。下面详细分析这个过程：

**第一步：设备树加载**

在系统启动阶段，Bootloader 将 DTB（Device Tree Blob）加载到内存指定地址，并将地址通过启动参数传递给内核。内核在初始化早期会解析这个 DTB：

```c
// 内核解析设备树
void *dt_virt = ioremap_cache(dt_phys, fdt_totalsize(dt_phys));
initial_boot_params = dt_virt;
unflatten_device_tree();
```

**第二步：设备树节点遍历**

解析完成后，内核会遍历设备树节点，为每个节点创建对应的 platform_device：

```c
// 遍历设备树并创建平台设备
int of_platform_populate(struct device_node *root,
            const struct of_device_id *matches,
            const struct of_platform_driver *driver,
            struct device *parent)
{
    // 从根节点开始遍历
    for_each_child_of_node(root, node) {
        of_platform_bus_create(node, matches, strict);
    }
}
```

**第三步：创建 platform_device**

对于每个需要创建的设备节点，`of_platform_device_create()` 函数会：

```c
static struct platform_device *of_platform_device_create(
    struct device_node *np,
    const char *bus_id,
    struct device *parent)
{
    // 1. 检查状态属性
    status = of_get_property(np, "status", NULL);
    if (status && strcmp(status, "disabled") == 0)
        return NULL;  // 禁用的设备不创建

    // 2. 分配 platform_device
    dev = of_platform_device_alloc(np, bus_id, parent);

    // 3. 从节点提取资源（reg、interrupts 等）
    of_platform_device_create_populate(np, dev);

    // 4. 添加到系统
    platform_device_add(dev);

    return dev;
}
```

**第四步：资源提取与设置**

在创建 platform_device 时，会从设备树节点中提取各种资源信息：

- **内存资源**：从 `reg` 属性提取，转换为 `IORESOURCE_MEM` 类型的 resource
- **中断资源**：从 `interrupts` 属性提取，转换为 `IORESOURCE_IRQ` 类型的 resource
- **DMA 资源**：从 `dma-ranges` 等属性提取，配置 DMA

```c
static void of_platform_device_create_populate(
    struct device_node *np,
    struct platform_device *dev)
{
    // 添加内存资源
    of_address_count = of_property_count_elems_of_size(np, "reg", sizeof(u32));
    // ...

    // 添加中断资源
    of_irq_count = of_property_count_elems_of_size(np, "interrupts", sizeof(u32));
    // ...
}
```

**第五步：驱动匹配与绑定**

当 platform_device 和 platform_driver 都注册到系统后，内核会自动进行匹配：

```c
// 总线驱动匹配过程
static int platform_match(struct device *dev, struct device_driver *drv)
{
    // 1. 设备树匹配（优先）
    if (of_driver_match_device(dev, drv))
        return 1;

    // 2. ACPI 匹配
    if (acpi_driver_match_device(dev, drv))
        return 1;

    // 3. 传统 ID 表匹配
    if (platform_match_id(drv->id_table, dev))
        return 1;

    // 4. 名称匹配（已不推荐）
    return strcmp(dev_name(dev), drv->name) == 0;
}
```

匹配成功后，调用驱动的 probe 函数：

```c
static int really_probe(struct device *dev, struct device_driver *drv)
{
    // 调用驱动的 probe 函数
    ret = drv->probe(dev);
    if (ret)
        goto probe_failed;

    // 绑定成功后，驱动可以通过以下方式获取设备资源
    struct resource *r = platform_get_resource(dev, IORESOURCE_MEM, 0);
    int irq = platform_get_irq(dev, 0);
}
```

**整个绑定过程的流程图**

```
+---------------------+     +-------------------------+
|    Bootloader       |     |     Linux Kernel       |
+---------------------+     +-------------------------+
|                     |     |                         |
|  加载 DTB 到内存    | --> |  解析 DTB               |
|  传递 DTB 地址     |     |  unflatten_device_tree  |
|                     |     |                         |
+---------------------+     +-------------------------+
                                    |
                                    v
                          +-------------------------+
                          |  of_platform_populate   |
                          +-------------------------+
                                    |
                                    v
+---------------------+     +-------------------------+
|  设备树节点         | --> |  of_platform_device_    |
|  /soc/uart@48000000|     |  create()               |
+---------------------+     +-------------------------+
                                    |
                                    v
                          +-------------------------+
                          |  platform_device        |
                          |  - name                 |
                          |  - resource[]          |
                          |  - of_node              |
                          +-------------------------+
                                    |
                                    v
+---------------------+     +-------------------------+
|  设备树 compatible  | --> |  of_match_table        |
|  "vendor,uart-v2"   |     |  匹配                  |
+---------------------+     +-------------------------+
                                    |
                                    v
                          +-------------------------+
                          |  driver->probe()        |
                          |  获取资源，初始化硬件   |
                          +-------------------------+
```

理解这个绑定过程对于调试设备树相关问题非常重要。常见的问题包括：设备没有创建、驱动没有匹配、资源获取失败等，都可以通过跟踪这个流程来定位问题。


# 第6章 设备树与中断映射

本章将深入讲解设备树（Device Tree）中中断的描述方式和Linux内核如何将这些描述映射为可用的中断资源。设备树是现代ARM/ARM64系统中描述硬件配置的核心机制，正确理解其中的中断属性对于驱动开发和系统移植至关重要。

## 6.1 DTS中断属性

设备树使用特定的属性来描述中断硬件的连接关系。这些属性遵循设备树规范定义的语法，使内核能够解析中断控制器的层级结构和每个外设的中断请求。

### 6.1.1 interrupt-cells属性

`interrupt-cells`属性定义了中断控制器节点中描述一个中断所需使用的单元格（cell）数量。每个中断控制器可以定义自己需要的单元格数量，这取决于硬件的复杂度。

```dts
/* GIC中断控制器定义 */
gic: interrupt-controller@fee00000 {
    compatible = "arm,gic-v2";
    #interrupt-cells = <3>;  /* 3个cell描述一个中断 */
    interrupt-controller;
    reg = <0x0 0xfee00000 0x0 0x10000>,
          <0x0 0xfee01000 0x0 0x2000>;
};

/* 常用的interrupt-cells配置 */
#interrupt-cells = <1>;  /* 简单控制器，仅需要中断号 */
#interrupt-cells = <2>;  /* 中断号 + 触发类型 */
#interrupt-cells = <3>;  /* 中断号 + 触发类型 + 优先级(GIC) */
```

对于GICv2中断控制器，三个cell的含义分别是：

```dts
interrupts = <interrupt_num trigger_type priority>;

/* 第一个cell：中断号
 * 0-15:   SGI (Software Generated Interrupt)
 * 16-31:  PPI (Private Peripheral Interrupt)
 * 32-1019: SPI (Shared Peripheral Interrupt)
 */

/* 第二个cell：触发类型
 * 1: 上升沿触发
 * 2: 下降沿触发
 * 4: 高电平触发
 * 8: 低电平触发
 */

/* 第三个cell：优先级
 * 0-255，数值越小优先级越高
 */
```

### 6.1.2 interrupt-parent属性

`interrupt-parent`属性指定当前设备的中断请求连接到哪个中断控制器。如果一个设备节点没有显式指定`interrupt-parent`，则会继承父节点的设置。这建立了一个中断控制器树形结构。

```dts
/* 根节点指定默认中断控制器 */
/ {
    interrupt-parent = <&gic>;

    /* GIC作为系统默认的中断父设备 */
};

/* 串口设备 */
uart0: serial@fe001000 {
    compatible = "ns16550a";
    reg = <0x0 0xfe001000 0x0 0x100>;
    interrupts = <1 4 0>;  /* 省略interrupt-parent，继承根节点的设置 */
};

/* GPIO控制器可能有自己独特的中断控制器 */
gpio: gpio@fe002000 {
    compatible = "arm,pl061";
    interrupt-parent = <&gic>;  /* 显式指定中断父设备 */
    interrupts = <5 4 0>;
};

/* 级联中断控制器示例 */
pinctrl: pinctrl@fe003000 {
    compatible = "arm,pl080";
    interrupt-parent = <&gpio>;  /* 连接到GPIO控制器 */
    interrupts = <0 4 0>, <1 4 0>, <2 4 0>;  /* 多个中断 */
};
```

### 6.1.3 interrupts属性

`interrupts`属性用于描述设备产生的中断。一个设备可以有多个中断，使用逗号分隔的列表表示。

```dts
/* 单个中断 */
timer@fe004000 {
    interrupts = <27 4 0>;  /* SPI 27，边沿触发，优先级0 */
};

/* 多个中断 */
中断设备: multi-irq-device@fe005000 {
    interrupts = <30 4 0>,   /* 第一个中断 */
                <31 4 0>;   /* 第二个中断 */
};
```

### 6.1.4 完整的中断设备树示例

```dts
/ {
    compatible = "arm,vexpress,v2p-a15", "arm,vexpress";
    interrupt-parent = <&gic>;

    /* GIC中断控制器 */
    gic: interrupt-controller@fee00000 {
        compatible = "arm,gic-v2";
        #interrupt-cells = <3>;
        interrupt-controller;
        reg = <0x0 0xfee00000 0x0 0x10000>,
              <0x0 0xfee01000 0x0 0x2000>;
    };

    /* ARM通用定时器 (PPI) */
    timer {
        compatible = "arm,armv7-timer";
        interrupt-parent = <&gic>;
        interrupts = <GIC_PPI 13 IRQ_TYPE_LEVEL_HIGH>,
                    <GIC_PPI 14 IRQ_TYPE_LEVEL_HIGH>,
                    <GIC_PPI 11 IRQ_TYPE_LEVEL_HIGH>,
                    <GIC_PPI 10 IRQ_TYPE_LEVEL_HIGH>;
    };

    /* 串口设备 */
    uart0: serial@fe001000 {
        compatible = "arm,pl011", "arm,primecell";
        reg = <0x0 0xfe001000 0x0 0x1000>;
        interrupts = <1 4 0>;  /* SPI 1，优先级4 */
        interrupt-parent = <&gic>;
    };

    /* 以太网控制器 */
    ethernet@fe002000 {
        compatible = "smsc,lan9115";
        reg = <0x0 0xfe002000 0x0 0x10000>;
        interrupts = <2 4 0>;  /* SPI 2 */
        interrupt-parent = <&gic>;
    };
};
```

## 6.2 interrupt-parent与级联

在实际系统中，中断控制器经常以级联（cascade）的方式组织。级联允许系统使用多个中断控制器，每个控制器负责管理一部分中断，再将它们连接到上一级控制器。

### 6.2.1 为什么要使用级联

级联中断控制器主要用于以下场景：

1. **扩展中断数量**：主中断控制器的中断数量有限，通过级联可以连接更多外设
2. **硬件架构设计**：某些外设通过GPIO或专用中断控制器连接
3. **功能模块化**：将中断按照功能域进行分组管理

```dts
/* 两级级联示例：第一级GIC，第二级GPIO */

/ {
    /* 根级中断控制器 */
    interrupt-parent = <&gic>;

    gic: interrupt-controller@fee00000 {
        compatible = "arm,gic-v2";
        #interrupt-cells = <3>;
        interrupt-controller;
        reg = <0x0 0xfee00000 0x0 0x10000>;
    };

    /* GPIO中断控制器 - 级联到GIC */
    gpio0: gpio@fe003000 {
        compatible = "arm,pl061";
        #interrupt-cells = <2>;  /* 只需要2个cell */
        interrupt-controller;
        interrupt-parent = <&gic>;
        interrupts = <5 4 0>;  /* GPIO控制器本身使用GIC的SPI 5 */
        reg = <0x0 0xfe003000 0x0 0x1000>;
    };

    /* 使用GPIO中断的设备 */
    button@0 {
        compatible = "gpio-keys";
        interrupt-parent = <&gpio0>;
        /* 通过GPIO控制器获取中断 */
        interrupts = <0 IRQ_TYPE_EDGE_FALLING>;
    };
};
```

### 6.2.2 级联中断控制器配置

级联中断控制器的配置需要在设备树中正确设置中断映射关系。以下是一个典型的级联配置：

```dts
/* 级联中断控制器节点 */
intc: interrupt-controller@fe010000 {
    compatible = "arm,pl190";
    #interrupt-cells = <1>;
    interrupt-controller;  /* 标记为中断控制器 */
    interrupt-parent = <&gic>;
    /* PL190连接到GIC的SPI 10 */
    interrupts = <10 4 0>;
    reg = <0x0 0xfe010000 0x0 0x1000>;
};

/* 使用级联中断控制器的外设 */
uart1: serial@fe011000 {
    compatible = "arm,pl011";
    reg = <0x0 0xfe011000 0x0 0x1000>;
    /* 连接到PL190（intc）的中断号3 */
    interrupt-parent = <&intc>;
    interrupts = <3>;
};
```

### 6.2.3 多级级联示例

在复杂的嵌入式系统中，可能会遇到多级级联的情况：

```dts
/ {
    /* 第一级：GIC */
    interrupt-parent = <&gic>;

    gic: interrupt-controller@fee00000 {
        compatible = "arm,gic-v2";
        #interrupt-cells = <3>;
        interrupt-controller;
        reg = <0x0 0xfee00000 0x0 0x10000>;
    };

    /* 第二级：GPIO控制器 */
    gpio: gpio@fe002000 {
        compatible = "arm,pl061";
        #interrupt-cells = <2>;
        interrupt-controller;
        interrupt-parent = <&gic>;
        /* GPIO控制器使用GIC的SPI 20 */
        interrupts = <20 4 0>;
    };

    /* 第三级：GPIO扩展器（通过I2C连接） */
    i2c-gpio {
        compatible = "i2c-gpio";
        /* 模拟I2C GPIO扩展器的中断 */
    };

    /* 通过GPIO连接的外设按键 */
    keys {
        compatible = "gpio-keys";
        interrupt-parent = <&gpio>;
        /* GPIO引脚0，低电平触发 */
        interrupts = <0 8>;  /* <pin trigger> */
    };

    /* 直接连接到GIC的SPI设备 */
    watchdog: watchdog@fe003000 {
        compatible = "arm,sp805";
        reg = <0x0 0xfe003000 0x0 0x1000>;
        interrupt-parent = <&gic>;
        interrupts = <25 4 0>;  /* SPI 25 */
    };
};
```

### 6.2.4 级联中断在Linux中的处理

Linux内核通过irq_domain机制处理级联中断。当中断控制器级联时，上一级的中断处理函数会调用下一级的中断芯片操作。

```c
/* 级联中断处理示例 */
static void irq_cascade_handle_irq(struct irq_desc *desc)
{
    struct irq_chip *chip = irq_desc_get_chip(desc);
    unsigned int irq = irq_desc_get_irq(desc);
    unsigned int hwirq;

    /* 调用级联控制器的级联函数获取下级中断号 */
    hwirq = chip->cascade();  /* 通常读取级联控制器的状态寄存器 */

    /* 将下级中断分发给对应的处理函数 */
    generic_handle_irq(irq_find_mapping(domain, hwirq));
}

/* 级联中断控制器驱动示例 */
static int __init secondary_intc_init(struct device_node *node,
                                      struct device_node *parent)
{
    struct irq_domain *domain;
    struct irq_chip chip;

    /* 创建irqdomain */
    domain = irq_domain_add_linear(node, NR_IRQS,
                                   &secondary_intc_domain_ops, NULL);

    /* 配置级联到父中断控制器 */
    irq_set_chained_handler(parent_irq, handle_simple_irq);

    return 0;
}
```

## 6.3 irqdomain解析流程

irqdomain是Linux内核中断子系统的核心机制，它负责将设备树中的中断描述转换为Linux内核可用的中断号（IRQ number）和中断描述符（irq_desc）。

### 6.3.1 irqdomain概述

irqdomain提供了一种将硬件中断号（hwirq）映射到Linux IRQ号的机制。在设备树系统中，设备节点中的`interrupts`属性描述了硬件中断号，而驱动需要知道对应的Linux IRQ号才能请求中断处理。

```c
/* irqdomain核心数据结构 */
struct irq_domain {
    struct list_head link;
    const char *name;
    const struct irq_domain_ops *ops;  /* 域操作函数集 */
    void *host_data;                   /* 主机私有数据 */
    unsigned int hint_level;            /* 分配提示级别 */
    unsigned int map_count;             /* 已映射的中断数 */
    struct irq_kernel_ipi_info *kernel_ipi_info;

    /* 中断号范围信息 */
    struct irq_domain_chip_generic *gc;
    struct device_node *of_node;       /* 对应的设备树节点 */
    enum irq_domain_bus_token bus_token;
    struct fwnode_handle *fwnode;      /* firmware节点句柄 */
    enum irq_domain_type type;         /* 域类型：LINEAR、TREE、NONE等 */

    /* 中断号范围 */
    unsigned int hwirq_base;
    unsigned int size;
    unsigned long flags;
};
```

### 6.3.2 设备树中断解析流程

从设备树到irq_desc的完整解析流程如下：

```c
/* 设备树中断解析流程 */

/* 1. 内核启动时初始化中断控制器 */
static int __init irqchip_init(void)
{
    of_platform_init_bus(NULL, of_platform_bus_type, NULL);
    return 0;
}

/* 2. 中断控制器驱动初始化，创建irqdomain */
static int __init gic_of_init(struct device_node *node,
                              struct device_node *parent)
{
    struct gic_chip_data *gic;
    struct irq_domain *domain;

    gic = kzalloc(sizeof(*gic), GFP_KERNEL);

    /* 创建线性irqdomain */
    domain = irq_domain_add_linear(node,
                                   gic->nr_irqs,
                                   &gic_irq_domain_ops,
                                   gic);
    gic->domain = domain;

    /* 注册中断控制器 */
    set_handle_irq(gic_handle_irq);

    return 0;
}

/* 3. 设备驱动解析中断属性 */
int of_irq_get(struct device_node *dev, int index)
{
    struct irq_of_parser parser;
    int ret;

    /* 解析interrupt-parent和interrupts属性 */
    ret = of_irq_parse_one(dev, index, &parser);
    if (ret)
        return ret;

    /* 创建irq_fwspec */
    return irq_create_of_mapping(&parser.fwspec);
}

/* 4. 解析interrupt-parent */
int of_irq_parse_one(struct device_node *device, int index,
                     struct of_phandle_args *out_irq)
{
    struct device_node *p;
    const __be32 *addr;
    u32 intsize;

    /* 查找interrupt-parent */
    p = of_parse_phandle(device, "interrupt-parent", index);
    if (!p)
        p = device->parent;

    /* 获取interrupt-cells数量 */
    of_property_read_u32(p, "#interrupt-cells", &intsize);

    /* 解析interrupts属性 */
    addr = of_get_property(device, "interrupts", NULL);
    if (!addr)
        return -EINVAL;

    /* 填充fwspec */
    out_irq->np = p;
    out_irq->args_count = intsize;
    /* 复制中断参数 */
    memcpy(out_irq->args, addr, intsize * sizeof(__be32));

    return 0;
}

/* 5. 通过irqdomain映射到Linux IRQ号 */
unsigned int irq_create_of_mapping(struct irq_fwspec *fwspec)
{
    struct irq_domain *domain;
    unsigned int hwirq, type = IRQ_TYPE_NONE;
    unsigned int virq;

    /* 根据fwspec查找对应的irqdomain */
    domain = irq_find_matching_fwspec(fwspec, DOMAIN_BUS_WIRED);
    if (!domain)
        return 0;

    /* 调用domain的translate函数解析hwirq和type */
    if (domain->ops->translate)
        domain->ops->translate(domain, fwspec, &hwirq, &type);
    else
        hwirq = fwspec->param[0];

    /* 分配Linux IRQ号 */
    virq = irq_create_mapping(domain, hwirq);

    return virq;
}

/* 6. 分配Linux IRQ号（irq_create_mapping） */
unsigned int irq_create_mapping(struct irq_domain *domain,
                                  unsigned int hwirq)
{
    unsigned int hint, virq;

    /* 检查是否已经有映射 */
    virq = irq_find_mapping(domain, hwirq);
    if (virq)
        return virq;

    /* 尝试分配一个新的IRQ号 */
    hint = hwirq - domain->hwirq_base;
    if (hint < domain->size)
        virq = idr_alloc(&domain->idr, NULL,
                        hint, hint + 1, GFP_ATOMIC);

    /* 如果hint失败，使用通用分配 */
    if (virq < 0)
        virq = idr_alloc(&domain->idr, NULL,
                        domain->hwirq_base,
                        domain->hwirq_base + domain->size,
                        GFP_ATOMIC);

    /* 创建irq_desc并关联到domain */
    irq_domain_set_info(domain, virq, hwirq,
                        &gic_chip, domain->host_data,
                        handle_fasteoi_irq, NULL, NULL);

    return virq;
}
```

### 6.3.3 完整的解析流程图

```
设备树(DTS)                    Linux内核                    硬件
    |                            |                          |
    |  interrupts = <5 4 0>      |                          |
    |---------------------------->|                          |
    |                            |                          |
    |    of_irq_get()            |                          |
    |---------------------------->|                          |
    |                            |  解析interrupt-parent    |
    |                            |------------------------->|
    |                            |  查找对应irqdomain       |
    |                            |------------------------->|
    |                            |                          |
    |    irq_create_of_mapping() |                          |
    |---------------------------->|                          |
    |                            |  translate (解析hwirq)   |
    |                            |------------------------->|
    |                            |                          |
    |                            |  irq_create_mapping()    |
    |                            |------------------------->|
    |                            |                          |
    |                            |  idr_alloc() 分配IRQ号   |
    |                            |------------------------->|
    |                            |                          |
    |                            |  irq_domain_set_info()   |
    |                            |  创建irq_desc            |
    |                            |------------------------->|
    |                            |                          |
    |    返回Linux IRQ号(IRQ 34)  |                          |
    |<---------------------------|                          |
    |                            |                          |
```

### 6.3.4 驱动中使用中断

```c
/* 驱动中获取中断号并请求处理函数 */
static int my_device_probe(struct platform_device *p_dev)
{
    struct my_device *dev;
    int irq, ret;

    /* 方法1: 通过platform_get_irq获取中断号 */
    irq = platform_get_irq(p_dev, 0);
    if (irq < 0)
        return irq;

    /* 方法2: 通过设备树直接解析 */
    irq = of_irq_get(p_dev->dev.of_node, 0);

    /* 请求中断处理函数 */
    ret = devm_request_threaded_irq(&p_dev->dev, irq,
                                    my_irq_handler, my_irq_thread,
                                    IRQF_ONESHOT, dev_name(&p_dev->dev),
                                    dev);
    if (ret) {
        dev_err(&p_dev->dev, "Failed to request IRQ %d\n", irq);
        return ret;
    }

    return 0;
}

/* 中断处理函数 */
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    u32 status;

    /* 读取中断状态寄存器 */
    status = readl(dev->regs + STATUS_REG);

    if (status & IRQ_PENDING) {
        /* 清除中断状态 */
        writel(IRQ_PENDING, dev->regs + STATUS_REG);
        return IRQ_WAKE_THREAD;  /* 唤醒线程处理程序 */
    }

    return IRQ_NONE;
}

/* 线程化中断处理函数 */
static irqreturn_t my_irq_thread(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* 执行实际的中断处理工作 */
    process_device_events(dev);

    return IRQ_HANDLED;
}
```

## 6.4 典型设备中断配置

### 6.4.1 SPI中断配置示例

SPI（Shared Peripheral Interrupt）是GIC中最常用的中断类型，用于连接外设。

```dts
/* SPI中断配置示例 */

/ {
    gic: interrupt-controller@fee00000 {
        compatible = "arm,gic-v2";
        #interrupt-cells = <3>;
        interrupt-controller;
        reg = <0x0 0xfee00000 0x0 0x10000>;
    };

    /* UART0 - SPI中断，优先级为4 */
    uart0: serial@fe001000 {
        compatible = "arm,pl011";
        reg = <0x0 0xfe001000 0x0 0x1000>;
        /* SPI中断号32，触发类型为4(高电平)，优先级0 */
        interrupts = <32 4 0>;
        interrupt-parent = <&gic>;
    };

    /* UART1 - 不同优先级 */
    uart1: serial@fe002000 {
        compatible = "arm,pl011";
        reg = <0x0 0xfe002000 0x0 0x1000>;
        /* SPI中断号33 */
        interrupts = <33 4 5>;
        interrupt-parent = <&gic>;
    };

    /* SPI中断号计算：
     * GIC中SPI中断号 = 32 + (设备树中的中断号)
     * 因此DTS中的 <32 4 0> 对应hwirq = 32
     * DTS中的 <0 4 0> 对应hwirq = 32（第一个SPI）
     */
};

/* 设备驱动中的处理 */
static const struct of_device_id uart_of_match[] = {
    { .compatible = "arm,pl011", },
    { },
};

static int pl011_probe(struct platform_device *plat)
{
    struct uart_port *port;
    int irq;

    port = devm_kzalloc(&plat->dev, sizeof(*port), GFP_KERNEL);

    /* 获取中断号 - 内核会自动完成从DTS到Linux IRQ的映射 */
    irq = platform_get_irq(plat, 0);
    if (irq < 0)
        return irq;

    /* 请求中断 */
    ret = devm_request_irq(&plat->dev, irq, pl011_intr,
                           IRQF_SHARED, "pl011", port);

    return 0;
}
```

### 6.4.2 PPI中断配置示例

PPI（Private Peripheral Interrupt）是每个CPU私有的中断，主要用于定时器等per-CPU设备。

```dts
/* PPI中断配置示例 - ARM通用定时器 */

/ {
    /* ARM通用定时器 - 每个CPU核心都有独立的定时器中断 */
    timer {
        compatible = "arm,armv7-timer";
        interrupt-parent = <&gic>;

        /* 各个PPI中断定义
         * PPI中断号范围: 16-31
         * PPI 10: 虚拟定时器中断
         * PPI 11: 物理定时器中断
         * PPI 13: 虚拟化性能计数器中断
         * PPI 14: 性能计数器中断
         */
        interrupts = <GIC_PPI 13 IRQ_TYPE_LEVEL_HIGH>,  /* 虚拟化PMU */
                    <GIC_PPI 14 IRQ_TYPE_LEVEL_HIGH>,  /* PMU */
                    <GIC_PPI 11 IRQ_TYPE_LEVEL_HIGH>,  /* 物理定时器 */
                    <GIC_PPI 10 IRQ_TYPE_LEVEL_HIGH>;  /* 虚拟定时器 */

        /* 使用GNU扩展简化定义 */
        /*
         * GIC_PPI是Linux内核定义的宏：
         * #define GIC_PPI(n) ((n) + 16)
         */
    };

    /* 每CPU本地定时器中断设备树节点 */
    local_timer {
        compatible = "arm,armv7-timer";
        /* 直接使用PPI号 */
        interrupts = <10 0xF 0>,  /* 虚拟定时器 */
                    <11 0xF 0>,  /* 物理定时器 */
                    <13 0xF 0>,  /* 虚拟化PMU */
                    <14 0xF 0>;  /* PMU */
    };
};

/* 内核中PPI中断的处理 */
void __init gic_init_bases(void)
{
    /* 为每个CPU分配PPI中断 */
    struct irq_desc *desc;
    int i;

    for (i = 0; i < NR_GIC_PPI; i++) {
        int irq = IRQ_PPI_BASE + i;

        desc = irq_to_desc(irq);

        /* 设置每个CPU私有的中断处理 */
        desc->action->flags |= IRQF_PERCPU;
    }
}
```

### 6.4.3 复杂外设中断配置

```dts
/* 网络控制器中断配置示例 - 多队列网卡的典型配置 */

/ {
    gic: interrupt-controller@fee00000 {
        compatible = "arm,gic-v2";
        #interrupt-cells = <3>;
        interrupt-controller;
    };

    /* 假设有4个队列的网络控制器 */
    eth0: ethernet@fe100000 {
        compatible = "intel,ixgbe";
        reg = <0x0 0xfe100000 0x0 0x20000>;

        /* 多个中断：队列中断 + 管理中断
         * 队列0-3中断（边沿触发）
         * 管理中断（电平触发）
         */
        interrupts = <56 4 0>,   /* 队列0 */
                    <57 4 0>,   /* 队列1 */
                    <58 4 0>,   /* 队列2 */
                    <59 4 0>,   /* 队列3 */
                    <60 4 0>,   /* 管理/错误中断 */
                    <61 1 0>;   /* 唤醒中断 */

        /* 也可以使用数组形式 */
        interrupt-names = "queue0", "queue1", "queue2", "queue3",
                          "misc", "wakeup";

        /* 中断亲和性（可选） */
        /* 内核会自动设置或通过smp_affinity调整 */
    };
};

/* 多队列网络驱动的中断配置 */
static int ixgbe_request_irqs(struct ixgbe_adapter *adapter)
{
    struct net_device *netdev = adapter->netdev;
    int err;
    int i;

    /* 获取队列中断 */
    for (i = 0; i < adapter->num_tx_queues; i++) {
        int irq = pci_irq_vector(adapter->pci_dev, i);
        err = devm_request_irq(&adapter->pci_dev->dev, irq,
                               ixgbe_clean_tx_irq,
                               IRQF_SHARED,
                               netdev->name,
                               &adapter->q_vector[i]);
        if (err)
            return err;
    }

    /* 获取管理中断 */
    err = devm_request_irq(&adapter->pci_dev->dev,
                           pci_irq_vector(adapter->pci_dev,
                                         adapter->num_tx_queues),
                           ixgbe_msix_other,
                           IRQF_SHARED,
                           netdev->name,
                           adapter);
    return err;
}
```

### 6.4.4 GPIO中断设备树配置

```dts
/* GPIO控制器中断配置 */

gpio0: gpio@fe003000 {
    compatible = "arm,pl061";
    #interrupt-cells = <2>;  /* <中断号 触发类型> */
    interrupt-controller;
    reg = <0x0 0xfe003000 0x0 0x1000>;

    /* GPIO控制器本身连接到GIC */
    interrupt-parent = <&gic>;
    interrupts = <25 4 0>;
};

/* 使用GPIO中断的设备 - 按键示例 */
gpio-keys {
    compatible = "gpio-keys";
    #address-cells = <1>;
    #size-cells = <0>;

    /* 按键1 - 连接到GPIO0的pin 0 */
    button1 {
        label = "Button 1";
        linux,code = <KEY_VOLUMEUP>;
        gpios = <&gpio0 0 GPIO_ACTIVE_LOW>;
        interrupt-parent = <&gpio0>;
        interrupts = <0 IRQ_TYPE_EDGE_FALLING>;
        debounce-interval = <50>;
    };

    /* 按键2 - GPIO0的pin 1 */
    button2 {
        label = "Button 2";
        linux,code = <KEY_VOLUMEDOWN>;
        gpios = <&gpio0 1 GPIO_ACTIVE_LOW>;
        interrupt-parent = <&gpio0>;
        interrupts = <1 IRQ_TYPE_EDGE_FALLING>;
        debounce-interval = <50>;
    };
};

/* 触发类型常量定义
 * GPIO_ACTIVE_HIGH = 0x00
 * GPIO_ACTIVE_LOW = 0x01
 * IRQ_TYPE_EDGE_RISING = 0x00000001
 * IRQ_TYPE_EDGE_FALLING = 0x00000002
 * IRQ_TYPE_EDGE_BOTH = 0x00000003
 * IRQ_TYPE_LEVEL_HIGH = 0x00000004
 * IRQ_TYPE_LEVEL_LOW = 0x00000008
 */
```

### 6.4.5 设备树中断常用属性总结

| 属性名 | 说明 | 示例 |
|--------|------|------|
| interrupt-parent | 指定中断父设备 | interrupt-parent = <&gic>; |
| interrupts | 描述外设产生的中断 | interrupts = <32 4 0>; |
| #interrupt-cells | 中断控制器使用的cell数量 | #interrupt-cells = <3>; |
| interrupt-controller | 标记节点为中断控制器 | interrupt-controller; |
| #address-cells | 地址单元格数量 | #address-cells = <1>; |
| #size-cells | 大小单元格数量 | #size-cells = <1>; |

---

## 本章面试题

### 1. 设备树中interrupt-cells属性的作用是什么？

参考答案：`interrupt-cells`属性定义了中断控制器节点中描述一个中断请求所需的32位单元格数量。不同的中断控制器可能有不同的cell数量要求，例如GICv2使用3个cell（中断号、触发类型、优先级），而简单的GPIO控制器可能只需要2个cell（引脚号、触发类型）。这个属性告诉设备树解析器如何正确解析`interrupts`属性中的数据。

### 2. interrupt-parent属性在设备树中的作用是什么？

参考答案：`interrupt-parent`属性指定当前设备的中断请求连接到哪个中断控制器。如果不显式指定，设备会继承父节点的中断父设备。这建立了一个中断控制器树形结构，使得复杂的级联中断系统得以描述。例如，GPIO控制器可以连接到主GIC，而GPIO上的各个引脚中断则通过`interrupt-parent`属性连接到GPIO控制器。

### 3. 描述Linux内核中设备树中断解析的完整流程。

参考答案：完整的解析流程包括：1) 内核启动时，中断控制器驱动初始化并创建irqdomain；2) 设备驱动通过`platform_get_irq()`或`of_irq_get()`获取中断号；3) 内核解析设备树中的`interrupt-parent`找到对应的中断控制器；4) 解析`interrupts`属性获取硬件中断号和触发类型；5) 通过irqdomain的`translate`函数将硬件中断号映射到Linux IRQ号；6) 调用`irq_create_mapping`分配Linux IRQ号并创建irq_desc。

### 4. 什么是中断控制器级联？为什么要使用级联？

参考答案：级联是指多个中断控制器以层次结构组织的方式。主中断控制器（如GIC）负责管理所有顶层中断，而次级中断控制器（如GPIO控制器或专用中断控制器）连接到主控制器上，再管理其下的外设中断。使用级联的原因包括：1) 扩展主控制器的中断数量；2) 适应特定硬件架构（GPIO引脚中断）；3) 按功能域对中断进行分组管理；4) 支持 legacy 中断控制器。

### 5. irqdomain的作用是什么？它如何实现硬件中断号到Linux IRQ号的映射？

参考答案：irqdomain是Linux内核中断子系统的核心机制，负责将硬件中断号（hwirq）映射到Linux IRQ号。它提供了一套标准化的接口，使得不同架构和中断控制器可以使用统一的方式处理中断映射。映射过程包括：`irq_find_mapping()`查找已存在的映射；`irq_create_mapping()`创建新的映射并分配Linux IRQ号；`irq_domain_set_info()`设置irq_desc与domain的关联。线性域使用简单的数学映射，树形域使用基数树管理。

### 6. SPI和PPI中断有什么区别？它们在设备树中如何表示？

参考答案：SPI（Shared Peripheral Interrupt）是共享外设中断，可被路由到任意CPU核心，中断号32-1019，由GIC的Distributor统一管理。PPI（Private Peripheral Interrupt）是每个CPU私有的中断，如CPU本地定时器，中断号16-31，每个CPU看到相同的PPI中断号但对应不同的硬件。在设备树中，两者通过不同的中断号范围表示：SPI使用32及以上的号，PPI使用16-31的号。驱动通常不需要关心这些细节，内核会处理映射。

### 7. 如何在设备树中配置边沿触发和电平触发的中断？

参考答案：中断触发类型通过`interrupts`属性的第二个cell（对于GIC是第2个cell）表示。常用的触发类型值包括：`1`表示上升沿触发，`2`表示下降沿触发，`4`表示高电平触发，`8`表示低电平触发。示例代码：`<32 4 0>`表示SPI 32高电平触发；`<10 2 0>`表示PPI 10下降沿触发。在Linux代码中，可以使用`IRQ_TYPE_EDGE_RISING`等常量通过`irq_set_irq_type()`函数动态修改触发类型。

### 8. 驱动中如何正确获取设备树中定义的中断号？

参考答案：驱动获取设备树中断号的常用方法包括：1) 使用`platform_get_irq(platform_device, index)`函数，自动完成所有解析；2) 使用`of_irq_get(device_node, index)`直接解析设备树；3) 对于I2C/SPI设备，使用`irq_of_parse_and_map(node, index)`。获取中断号后，使用`request_irq()`或`devm_request_irq()`注册中断处理函数。重要的是要检查返回值以处理错误情况，并正确设置`IRQF_SHARED`、`IRQF_ONESHOT`等标志。

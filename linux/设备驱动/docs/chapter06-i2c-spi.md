# 第6章 I2C/SPI 总线驱动案例

I2C（Inter-Integrated Circuit）和 SPI（Serial Peripheral Interface）是嵌入式系统中最为常用的两种串行通信总线。它们在消费电子、工业控制、汽车电子等领域有着广泛的应用，特别是在传感器、外设扩展、存储设备等场景中。本章将深入讲解 I2C 和 SPI 总线驱动的核心结构、设备驱动开发方法，并通过实际的传感器驱动案例帮助读者掌握这两种总线的驱动开发技能。

## 6.1 I2C 主机驱动与设备驱动

I2C 总线是由 Philips 公司于 1980 年代初开发的一种同步串行通信协议。它使用两根信号线（时钟线 SCL 和数据线 SDA）即可完成主从设备之间的通信，具有引脚少、成本低、支持多主设备等优点。Linux 内核为 I2C 总线提供了完整的驱动框架，包括 I2C 主机控制器驱动（也称为适配器驱动）和 I2C 设备驱动（也称为客户端驱动）两部分。

### 6.1.1 I2C 核心结构

Linux 内核的 I2C 驱动框架建立在四个核心结构体之上：i2c_adapter、i2c_algorithm、i2c_driver 和 i2c_client。这四个结构体分别代表了 I2C 总线适配器、传输算法、I2C 设备和 I2C 客户端。理解这些结构体的定义和相互关系是掌握 I2C 驱动开发的关键。

`struct i2c_adapter` 结构体表示一个 I2C 总线适配器（即 I2C 主机控制器），它对应于物理上的一个 I2C 总线接口。这个结构体定义位于 Linux 内核源码的 `include/linux/i2c.h` 文件中：

```c
// 文件位置：include/linux/i2c.h
struct i2c_adapter {
    // device 结构体是 Linux 设备模型的核心
    // 通过这个成员，i2c_adapter 可以接入到 sysfs 文件系统中
    // 在 /sys/bus/i2c/devices/ 下可以看到系统中的所有 I2C 适配器
    struct device dev;

    // nr 是 I2C 适配器的总线编号
    // 第一个适配器的编号为 0，第二个为 1，以此类推
    // 可以在启动时通过命令行参数指定，如 i2c0=i2c-gpio,bus=0
    int nr;

    // algo 指向 I2C 传输算法结构体
    // 它封装了底层硬件的传输操作，包括 master_xfer 和 smbus_xfer
    struct i2c_algorithm *algo;

    // algo_data 是传给算法回调的私有数据
    // 通常用于存放与特定硬件相关的配置信息
    void *algo_data;

    // timeout 是默认的超时时间，用于 I2C 传输等待
    unsigned long timeout;

    // retries 是重试次数，当传输失败时的重试次数
    int retries;

    // dev_released 是设备释放完成的信号量
    // 用于同步设备的释放操作
    struct completion dev_released;

    // lock 用于保护适配器的并发访问
    struct mutex bus_lock;

    // name 是适配器的名称，会在 sysfs 中显示
    char name[48];

    ...
};
```

`struct i2c_adapter` 是 I2C 适配器的软件抽象，它封装了底层硬件控制器的所有信息。每个 I2C 适配器都有一个唯一的编号（nr），这个编号在系统初始化时由 I2C 核心分配。适配器通过 `algo` 成员关联到具体的传输算法，而传输算法则负责实现真正的 I2C 数据传输操作。

`struct i2c_algorithm` 结构体定义了 I2C 适配器的传输方法，它是连接软件层和硬件层的桥梁：

```c
// 文件位置：include/linux/i2c.h
struct i2c_algorithm {
    // master_xfer 是主要的 I2C 传输函数
    // 它负责执行一次完整的 I2C 传输，包括发送起始位、地址、数据和停止位
    // adap 是 I2C 适配器指针
    // msgs 是 I2C 消息数组，每个消息可以包含读或写操作
    // num 是消息数组的长度
    // 返回成功传输的消息数量，负值表示错误
    int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);

    // smbus_xfer 是 SMBus 协议传输函数
    // SMBus 是 I2C 的一个子集，具有更严格的时序要求
    // addr 是从设备地址
    // flags 是操作标志
    // read_write 指定是读(1)还是写(0)操作
    // command 是 SMBus 命令字节
    // size 是 SMBus 传输类型
    // data 是数据传输缓冲区
    int (*smbus_xfer)(struct i2c_adapter *adap, u16 addr,
                      unsigned short flags, char read_write,
                      u8 command, int size, union i2c_smbus_data *data);

    // functionality 返回适配器支持的功能
    // 返回值是一个位掩码，包括 I2C_FUNC_I2C、I2C_FUNC_SMBUS_BYTE 等
    u32 (*functionality)(struct i2c_adapter *adap);

    // 以下成员在较新的内核版本中可能存在
    ...
};
```

`master_xfer` 是 I2C 传输的核心函数，它接收一个 i2c_msg 数组，每个 i2c_msg 结构体描述一次 I2C 传输操作。i2c_msg 结构体的定义如下：

```c
// 文件位置：include/linux/i2c.h
struct i2c_msg {
    // addr 是从设备地址，7 位地址（不带 R/W 位）
    // 需要使用 I2C_CLIENT_ADDR_10BIT 标志来标识 10 位地址
    __u16 addr;

    // flags 是消息标志
    // I2C_M_RD 表示读操作，否则为写操作
    // I2C_M_TEN 表示使用 10 位地址
    // I2C_M_NOSTART 表示不发送起始位
    // I2C_M_STOP 表示在最后一条消息后发送停止位
    __u16 flags;

    // len 是数据缓冲区长度
    __u16 len;

    // buf 是数据缓冲区指针
    // 写操作时存放要发送的数据
    // 读操作时存放接收到的数据
    __u8 *buf;
};
```

在实际开发中，I2C 设备驱动通常不需要直接操作这些底层结构，而是使用 I2C 核心提供的辅助函数来完成数据传输。

`struct i2c_driver` 结构体代表 I2C 设备的驱动程序，它定义了驱动如何与 I2C 设备进行交互：

```c
// 文件位置：include/linux/i2c.h
struct i2c_driver {
    // class 是驱动支持的设备类别
    // 用于过滤匹配，驱动只会探测符合类别的设备
    // 如 I2C_CLASS_HWMON 表示硬件监控类设备
    unsigned int class;

    // attach_adapter 是旧版内核中的探测函数
    // 已废弃，在新内核中使用 probe 代替
    // 保留此成员仅为兼容旧驱动
    int (*attach_adapter)(struct i2c_adapter *) __deprecated;

    // probe 是驱动探测函数
    // 当 I2C 核心找到与驱动匹配的设备时被调用
    // client 是匹配的 I2C 客户端结构体
    // id 是匹配的设备 ID 表项
    int (*probe)(struct i2c_client *client, const struct i2c_device_id *id);

    // remove 是驱动移除函数
    // 当设备从系统中移除或驱动卸载时被调用
    int (*remove)(struct i2c_client *);

    // shutdown 是关闭函数，在系统关机时调用
    void (*shutdown)(struct i2c_client *);

    // alert 是 SMBus 报警处理函数
    // 当从设备发送 SMBus 报警信号时被调用
    void (*alert)(struct i2c_client *, enum i2c_alert_protocol protocol,
                  unsigned int data);

    // command 是驱动特定的命令处理函数
    // 允许用户空间通过 ioctl 发送特定命令给驱动
    int (*command)(struct i2c_client *client, unsigned int cmd, void *arg);

    // driver 是内嵌的 device_driver 结构体
    // 这是 Linux 驱动模型的核心
    struct device_driver driver;

    // id_table 是 I2C 设备 ID 表
    // 用于非设备树平台的驱动匹配
    // 数组以 { } 结束
    const struct i2c_device_id *id_table;

    // detect 是设备检测函数
    // 用于在不支持设备树的平台上自动检测设备
    int (*detect)(struct i2c_client *, struct i2c_board_info *);

    // address_data 是地址数据，用于自动检测设备
    const struct i2c_client_address_data *address_data;

    // list 是驱动链表节点，用于挂载到 I2C 驱动链表上
    struct list_head list;

    // 以下成员在较新的内核版本中可能存在
    ...
};
```

`struct i2c_client` 结构体代表一个具体的 I2C 设备，它对应于物理上的一个从设备芯片：

```c
// 文件位置：include/linux/i2c.h
struct i2c_client {
    // flags 是设备标志
    // I2C_CLIENT_PEC 表示支持 SMBus PEC（错误检查）
    // I2C_CLIENT_TEN 表示使用 10 位地址
    unsigned short flags;

    // addr 是 I2C 从设备地址
    // 7 位地址存储在低 7 位，10 位地址需要设置 I2C_CLIENT_PEC_10
    unsigned short addr;

    // name 是设备名称，最大长度为 I2C_NAME_SIZE（20 字节）
    // 这个名称会在 sysfs 中显示
    char name[I2C_NAME_SIZE];

    // adapter 指向所属的 I2C 适配器
    // 通过这个成员可以获取设备所在的总线信息
    struct i2c_adapter *adapter;

    // dev 是内嵌的 device 结构体
    // 这是 Linux 设备模型的核心
    // 设备会在 /sys/bus/i2c/devices/ 下创建对应的目录
    struct device dev;

    // irq 是设备的中断号
    // 如果设备使用中断方式工作，这个成员存储分配的中断号
    int irq;

    // driver 指向绑定的 I2C 驱动
    // 当设备与驱动匹配后，这个成员指向对应的驱动
    struct i2c_driver *driver;

    // dev_type 是设备类型标识
    // 用于模块加载时的设备匹配
    u32 dev_type;

    // list 是客户端链表节点
    struct list_head list;

    // detect 是设备检测回调（内部使用）
    const struct i2c_client_detection_data *detect;

    // adapter_info 是适配器信息（内部使用）
    void *adapter_info;

    // 以下成员在较新的内核版本中可能存在
    ...
};
```

i2c_client 结构体中的 `addr` 成员存储了 I2C 从设备的地址。这个地址是 7 位或 10 位的 I2C 地址，不包含读/写位。在实际使用中，地址的低位用于指示读/写操作，由 I2C 核心在传输时自动添加。

### 6.1.2 I2C 设备驱动示例

下面通过一个完整的 I2C 设备驱动示例来展示 I2C 驱动的开发方法。这个示例演示了如何编写一个通用的 I2C 设备驱动框架，读者可以在此基础上添加具体的设备操作逻辑。

```c
/*
 * I2C 设备驱动示例
 * 文件位置：drivers/i2c/bus/my_i2c_device.c
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/regmap.h>

// 定义设备私有数据结构
struct my_i2c_data {
    struct i2c_client *client;
    struct regmap *regmap;
    // 在这里添加设备特定的成员
    // 如：u8 status;
    //     struct mutex lock;
};

// 设备树匹配表
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-i2c-device", .data = NULL },
    { .compatible = "vendor,my-i2c-device-v2", .data = NULL },
    { }
};
MODULE_DEVICE_TABLE(of, my_of_match);

// ID 表（用于非设备树平台）
static const struct i2c_device_id my_id_table[] = {
    { "my_i2c_device", 0 },
    { "my_i2c_device_v2", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, my_id_table);

// probe 函数：设备与驱动匹配时被调用
static int my_i2c_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    struct my_i2c_data *data;
    struct device *dev = &client->dev;
    int ret;

    dev_info(dev, "Probing device %s\n", client->name);

    // 分配私有数据结构
    data = devm_kzalloc(dev, sizeof(struct my_i2c_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    // 将私有数据与 i2c_client 关联
    i2c_set_clientdata(client, data);
    data->client = client;

    // 在这里进行设备初始化
    // 例如：读取设备 ID、配置寄存器等
    // ret = my_device_init(data);
    // if (ret < 0)
    //     return ret;

    dev_info(dev, "Device probed successfully\n");
    return 0;
}

// remove 函数：设备移除或驱动卸载时被调用
static int my_i2c_remove(struct i2c_client *client)
{
    struct my_i2c_data *data = i2c_get_clientdata(client);

    dev_info(&client->dev, "Removing device %s\n", client->name);

    // 在这里进行设备清理
    // 例如：关闭设备、释放资源等
    // if (data->regmap)
    //     regmap_exit(data->regmap);

    return 0;
}

// shutdown 函数：系统关机时调用
static void my_i2c_shutdown(struct i2c_client *client)
{
    struct my_data *data = i2c_get_clientdata(client);

    // 在这里保存设备状态或关闭设备
    dev_info(&client->dev, "Shutting down device\n");
}

// 定义 I2C 驱动结构体
static struct i2c_driver my_i2c_driver = {
    .driver = {
        .name = "my_i2c",
        .of_match_table = of_match_ptr(my_of_match),
        // 可选：添加 PM 支持
        // .pm = &my_i2c_pm_ops,
    },
    .probe = my_i2c_probe,
    .remove = my_i2c_remove,
    .shutdown = my_i2c_shutdown,
    .id_table = my_id_table,
};

// 模块初始化函数
static int __init my_i2c_init(void)
{
    pr_info("My I2C driver init\n");
    return i2c_add_driver(&my_i2c_driver);
}

// 模块退出函数
static void __exit my_i2c_exit(void)
{
    pr_info("My I2C driver exit\n");
    i2c_del_driver(&my_i2c_driver);
}

module_init(my_i2c_init);
module_exit(my_i2c_exit);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("My I2C Device Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
```

在实际项目中，I2C 设备驱动还需要实现与具体硬件的交互。以下是更完整的示例，展示了如何使用 I2C 传输函数进行数据读写：

```c
/*
 * I2C 设备读写操作示例
 */

// 使用 i2c_transfer 进行读写
static int my_i2c_read_reg(struct i2c_client *client, u8 reg, u8 *val)
{
    struct i2c_msg msgs[2];
    u8 buf[1];
    int ret;

    // 第一个消息：发送寄存器地址
    msgs[0].addr = client->addr;
    msgs[0].flags = 0;  // 写操作
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    // 第二个消息：读取数据
    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;  // 读操作
    msgs[1].len = 1;
    msgs[1].buf = buf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0)
        return ret;
    if (ret != 2)
        return -EIO;

    *val = buf[0];
    return 0;
}

static int my_i2c_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
    u8 buf[2];
    struct i2c_msg msg;
    int ret;

    buf[0] = reg;
    buf[1] = val;

    msg.addr = client->addr;
    msg.flags = 0;  // 写操作
    msg.len = 2;
    msg.buf = buf;

    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0)
        return ret;
    if (ret != 1)
        return -EIO;

    return 0;
}

// 使用 SMBus 接口（更简单，推荐）
static int my_i2c_smbus_read_byte(struct i2c_client *client, u8 command)
{
    union i2c_smbus_data data;
    int ret;

    ret = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
                         I2C_SMBUS_READ, command,
                         I2C_SMBUS_BYTE, &data);
    if (ret < 0)
        return ret;

    return data.byte;
}

static int my_i2c_smbus_write_byte(struct i2c_client *client, u8 command, u8 value)
{
    union i2c_smbus_data data;
    int ret;

    data.byte = value;
    ret = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
                          I2C_SMBUS_WRITE, command,
                          I2C_SMBUS_BYTE_DATA, &data);
    if (ret < 0)
        return ret;

    return 0;
}
```

I2C 设备驱动的注册过程涉及到设备与驱动的匹配机制。当系统初始化 I2C 子系统时，会扫描 I2C 总线上的设备（或从设备树中获取设备信息），然后根据设备地址和驱动ID表进行匹配。匹配成功后，内核会自动创建设备节点，并调用驱动的 probe 函数完成设备的初始化。

## 6.2 SPI 主机驱动与设备驱动

SPI（Serial Peripheral Interface）是由 Motorola 公司开发的一种同步串行通信协议。相比 I2C，SPI 具有更高的传输速率（可达数十 MHz）、支持全双工通信、可配置时钟极性和相位等优点。SPI 在高速数据采集、显示驱动、存储设备等场景中应用广泛。

### 6.2.1 SPI 核心结构

Linux 内核的 SPI 驱动框架与 I2C 类似，也采用主机驱动与设备驱动分离的架构。核心结构体包括：spi_master、spi_device 和 spi_driver。

`struct spi_master` 结构体表示 SPI 主机控制器（SPI 总线适配器），定义位于 `include/linux/spi/spi.h`：

```c
// 文件位置：include/linux/spi/spi.h
struct spi_master {
    // dev 是 Linux 设备模型的核心结构体
    // 通过它 SPI 主机可以接入到 sysfs 文件系统
    struct device dev;

    // bus_num 是 SPI 总线编号
    // 用于区分系统中的多个 SPI 控制器
    s16 bus_num;

    // num_chipselect 是支持的片选信号数量
    // 它决定了最多可以连接多少个从设备
    u16 num_chipselect;

    // mode_mask 是支持的 SPI 模式掩码
    // 包括：SPI_CPHA、SPI_CPOL、SPI_LSB_FIRST、SPI_CS_HIGH 等
    u16 mode_mask;

    // bits_per_word_mask 是支持的每字位数掩码
    // 如：SPI_BPW_MASK(8) | SPI_BPW_MASK(16)
    u32 bits_per_word_mask;

    // min_speed_hz 是支持的最低时钟频率
    u32 min_speed_hz;

    // max_speed_hz 是支持的最高时钟频率
    u32 max_speed_hz;

    // flags 是主机控制器的标志
    u16 flags;

    // setup 是 SPI 设备初始化函数
    // 在每个 SPI 设备首次传输前调用
    // 用于配置 SPI 时钟、模式等参数
    int (*setup)(struct spi_device *spi);

    // transfer 是一个完整的 SPI 传输函数
    // 它同步地执行一次 SPI 传输
    // 传输完成后函数返回
    int (*transfer)(struct spi_device *spi, struct spi_message *mesg);

    // transfer_one 是单次传输函数
    // 用于驱动那些每次传输一个消息的控制器
    // 它在每个消息传输完成后返回
    int (*transfer_one)(struct spi_master *master, struct spi_device *spi,
                        struct spi_transfer *transfer);

    // transfer_one_message 是完整消息传输函数
    // 用于驱动那些需要管理消息队列的控制器
    int (*transfer_one_message)(struct spi_master *master,
                                struct spi_message *mesg);

    // unprepare_transfer_hardware 是传输完成后的清理函数
    // 在最后一次传输完成后调用，用于关闭时钟等
    int (*unprepare_transfer_hardware)(struct spi_master *master);

    // prepare_message 是消息预处理函数
    // 在处理每个消息前调用，用于配置控制器
    int (*prepare_message)(struct spi_master *master, struct spi_message *message);

    // message_pending 是待处理消息计数
    // 用于跟踪队列中的消息数量
    int queued;
    struct kthread_worker kworker;
    struct task_struct *kworker_task;
    struct spi_message *cur_msg;

    // ...
};
```

SPI 主机控制器与 I2C 适配器类似，都代表了物理上的一个通信接口。主要区别在于 SPI 支持更高的传输速率，并且每个从设备都需要独立的片选信号（chip select）。

`struct spi_device` 结构体代表一个 SPI 从设备：

```c
// 文件位置：include/linux/spi/spi.h
struct spi_device {
    // dev 是 Linux 设备模型的核心
    // 设备会在 /sys/bus/spi/devices/ 下创建对应的目录
    struct device dev;

    // master 指向所属的 SPI 主机控制器
    struct spi_master *master;

    // max_speed_hz 是设备支持的最大时钟频率
    // 不能超过 master 的 max_speed_hz
    u32 max_speed_hz;

    // chip_select 是片选信号编号
    // 对应硬件上的 CS 引脚
    // 从 0 开始编号
    u8 chip_select;

    // bits_per_word 是每字位数
    // 常见值是 8、16、32
    // 如果为 0，则使用 master 的默认值
    u8 bits_per_word;

    // mode 是 SPI 模式
    // 包括时钟极性(CPOL)和时钟相位(CPHA)的组合
    // 可能的值：
    // SPI_MODE_0 (CPOL=0, CPHA=0)
    // SPI_MODE_1 (CPOL=0, CPHA=1)
    // SPI_MODE_2 (CPOL=1, CPHA=0)
    // SPI_MODE_3 (CPOL=1, CPHA=1)
    // 还可以包含 SPI_LSB_FIRST、SPI_CS_HIGH 等
    u16 mode;

    // irq 是设备使用的中断号
    // 如果设备使用中断方式工作
    int irq;

    // modalias 是设备模块名称
    // 用于模块加载时的自动匹配
    const char *modalias;

    // controller_data 是控制器特定的配置数据
    // 可以在 setup 回调中使用
    void *controller_data;

    // controller_state 是控制器状态
    // 驱动内部使用
    void *controller_state;

    // 以下成员在较新的内核版本中可能存在
    ...
};
```

SPI 设备与 I2C 设备的一个重要区别是，SPI 设备需要指定片选信号编号（chip_select），因为多个 SPI 设备可以共享同一根 MOSI/MISO 时钟线，通过不同的片选信号来选择目标设备。

`struct spi_driver` 结构体代表 SPI 设备的驱动程序：

```c
// 文件位置：include/linux/spi/spi.h
struct spi_driver {
    // probe 是驱动探测函数
    // 当 SPI 核心找到与驱动匹配的设备时被调用
    int (*probe)(struct spi_device *spi);

    // remove 是驱动移除函数
    // 当设备从系统中移除或驱动卸载时被调用
    int (*remove)(struct spi_device *spi);

    // shutdown 是关闭函数，在系统关机时调用
    void (*shutdown)(struct spi_device *spi);

    // driver 是内嵌的 device_driver 结构体
    struct device_driver driver;

    // id_table 是 SPI 设备 ID 表
    const struct spi_device_id *id_table;

    // ...
};
```

spi_driver 与 i2c_driver 的结构非常相似，这使得熟悉 I2C 驱动的开发者可以快速上手 SPI 驱动的开发。

SPI 传输使用 `struct spi_message` 和 `struct spi_transfer` 结构体来描述：

```c
// 文件位置：include/linux/spi/spi.h
struct spi_transfer {
    // tx_buf 是发送数据缓冲区
    // 如果不需要发送数据，设置为 NULL
    const void *tx_buf;

    // rx_buf 是接收数据缓冲区
    // 如果不需要接收数据，设置为 NULL
    void *rx_buf;

    // len 是传输数据长度（字节数）
    unsigned int len;

    // tx_dma、rx_dma 是 DMA 缓冲区地址
    // 如果使用 DMA 传输，需要设置这些成员
    dma_addr_t tx_dma;
    dma_addr_t rx_dma;

    // speed_hz 是本次传输使用的时钟频率
    // 如果为 0，使用设备的默认频率
    unsigned int speed_hz;

    // bits_per_word 是本次传输使用的位数
    // 如果为 0，使用设备的默认值
    unsigned int bits_per_word;

    // delay_usecs 是传输完成后的延迟（微秒）
    unsigned int delay_usecs;

    // cs_change 表示传输完成后是否改变片选状态
    // true: 取消片选（拉高）
    // false: 保持片选选中状态
    bool cs_change;

    // transfer_one 是自定义传输函数
    // 用于特殊的传输场景
    int (*transfer_one)(struct spi_master *master, struct spi_device *spi,
                        struct spi_transfer *transfer);

    // ...
};

struct spi_message {
    // transfers 是传输链表
    struct list_head transfers;

    // spi 是目标 SPI 设备
    struct spi_device *spi;

    // is_dma_mapped 表示是否已经配置了 DMA 映射
    unsigned int is_dma_mapped:1;

    // complete 是传输完成回调函数
    void (*complete)(void *context);

    // context 是传递给完成回调的参数
    void *context;

    // status 是传输状态
    // 0 表示成功，负值表示错误
    int status;

    // queue 是消息队列节点（内部使用）
    struct list_head queue;

    // frame_length、remaining_words（内部使用）
    ...
};
```

一次 SPI 传输可以包含多个 spi_transfer，每个 transfer 可以有不同的发送/接收缓冲区、传输速度、位数等参数。SPI 核心会自动将这些 transfer 组合成一次完整的消息传输。

### 6.2.2 SPI 设备驱动示例

下面通过完整的代码示例展示 SPI 设备驱动的开发方法：

```c
/*
 * SPI 设备驱动示例
 * 文件位置：drivers/spi/spi-my-device.c
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/property.h>

// 定义设备私有数据结构
struct my_spi_data {
    struct spi_device *spi;
    // 在这里添加设备特定的成员
    // 如：u8 mode;
    //     struct regmap *regmap;
};

// 设备树匹配表
static const struct of_device_id my_spi_of_match[] = {
    { .compatible = "vendor,my-spi-device", .data = NULL },
    { .compatible = "vendor,my-spi-device-v2", .data = NULL },
    { }
};
MODULE_DEVICE_TABLE(of, my_spi_of_match);

// ID 表（用于非设备树平台）
static const struct spi_device_id my_spi_id_table[] = {
    { "my_spi_device", 0 },
    { "my_spi_device_v2", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, my_spi_id_table);

// probe 函数：设备与驱动匹配时被调用
static int my_spi_probe(struct spi_device *spi)
{
    struct my_spi_data *data;
    struct device *dev = &spi->dev;
    int ret;

    dev_info(dev, "Probing device %s\n", spi->modalias);

    // 检查并设置 SPI 模式
    // SPI 设备通常需要特定的时钟极性和相位
    if (spi->mode & SPI_CPHA)
        dev_info(dev, "Using SPI mode: CPHA\n");
    if (spi->mode & SPI_CPOL)
        dev_info(dev, "Using SPI mode: CPOL\n");
    if (spi->mode & SPI_LSB_FIRST)
        dev_info(dev, "Using SPI mode: LSB first\n");

    // 分配私有数据结构
    data = devm_kzalloc(dev, sizeof(struct my_spi_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    // 将私有数据与 spi_device 关联
    spi_set_drvdata(spi, data);
    data->spi = spi;

    // 设置 bits_per_word（如果需要）
    if (!spi->bits_per_word)
        spi->bits_per_word = 8;

    // 设置最大时钟频率（如果需要）
    // spi->max_speed_hz = 10000000;

    // 初始化 SPI 设备
    ret = spi_setup(spi);
    if (ret < 0) {
        dev_err(dev, "Failed to setup SPI device: %d\n", ret);
        return ret;
    }

    // 在这里进行设备初始化
    // 例如：读取设备 ID、配置寄存器等
    // ret = my_device_init(data);
    // if (ret < 0)
    //     return ret;

    dev_info(dev, "Device probed successfully\n");
    return 0;
}

// remove 函数：设备移除或驱动卸载时被调用
static int my_spi_remove(struct spi_device *spi)
{
    struct my_spi_data *data = spi_get_drvdata(spi);

    dev_info(&spi->dev, "Removing device %s\n", spi->modalias);

    // 在这里进行设备清理
    // 例如：关闭设备、释放资源等

    return 0;
}

// shutdown 函数：系统关机时调用
static void my_spi_shutdown(struct spi_device *spi)
{
    dev_info(&spi->dev, "Shutting down device\n");
}

// 定义 SPI 驱动结构体
static struct spi_driver my_spi_driver = {
    .driver = {
        .name = "my_spi",
        .of_match_table = of_match_ptr(my_spi_of_match),
    },
    .probe = my_spi_probe,
    .remove = my_spi_remove,
    .shutdown = my_spi_shutdown,
    .id_table = my_spi_id_table,
};

// 模块初始化函数
static int __init my_spi_init(void)
{
    pr_info("My SPI driver init\n");
    return spi_register_driver(&my_spi_driver);
}

// 模块退出函数
static void __exit my_spi_exit(void)
{
    pr_info("My SPI driver exit\n");
    spi_unregister_driver(&my_spi_driver);
}

module_init(my_spi_init);
module_exit(my_spi_exit);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("My SPI Device Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
```

在实际应用中，SPI 设备驱动需要实现具体的数据传输操作。以下是 SPI 读写操作的完整示例：

```c
/*
 * SPI 设备读写操作示例
 */

// 同步读写（阻塞方式）
static int my_spi_read(struct spi_device *spi, u8 reg, u8 *val)
{
    struct spi_message message;
    struct spi_transfer xfer[2];
    u8 tx_buf[2];
    u8 rx_buf[2];
    int ret;

    // 初始化消息和传输结构
    spi_message_init(&message);
    memset(xfer, 0, sizeof(xfer));

    // 第一个传输：发送寄存器地址
    tx_buf[0] = reg;
    xfer[0].tx_buf = tx_buf;
    xfer[0].len = 1;
    xfer[0].cs_change = false;  // 保持片选选中
    spi_message_add_tail(&xfer[0], &message);

    // 第二个传输：读取数据
    xfer[1].rx_buf = rx_buf;
    xfer[1].len = 1;
    xfer[1].cs_change = true;  // 取消片选
    spi_message_add_tail(&xfer[1], &message);

    // 执行传输
    ret = spi_sync(spi, &message);
    if (ret < 0)
        return ret;

    if (message.status != 0)
        return message.status;

    *val = rx_buf[0];
    return 0;
}

static int my_spi_write(struct spi_device *spi, u8 reg, u8 val)
{
    struct spi_message message;
    struct spi_transfer xfer;
    u8 tx_buf[2];
    int ret;

    // 初始化消息和传输结构
    spi_message_init(&message);
    memset(&xfer, 0, sizeof(xfer));

    // 发送寄存器地址和数据
    tx_buf[0] = reg;
    tx_buf[1] = val;
    xfer.tx_buf = tx_buf;
    xfer.len = 2;
    xfer.cs_change = true;  // 传输完成后取消片选
    spi_message_add_tail(&xfer, &message);

    // 执行传输
    ret = spi_sync(spi, &message);
    if (ret < 0)
        return ret;

    return message.status;
}

// 使用 spi_write_then_read（简化版）
static int my_spi_read_simple(struct spi_device *spi, u8 reg, u8 *val)
{
    int ret;

    ret = spi_write_then_read(spi, &reg, 1, val, 1);
    if (ret < 0)
        return ret;

    return 0;
}

// 批量传输（一次传输多个字节）
static int my_spi_read_buf(struct spi_device *spi, u8 reg, u8 *buf, size_t len)
{
    struct spi_message message;
    struct spi_transfer xfer[2];
    int ret;

    spi_message_init(&message);
    memset(xfer, 0, sizeof(xfer));

    // 发送寄存器地址
    xfer[0].tx_buf = &reg;
    xfer[0].len = 1;
    xfer[0].cs_change = false;
    spi_message_add_tail(&xfer[0], &message);

    // 读取数据
    xfer[1].rx_buf = buf;
    xfer[1].len = len;
    xfer[1].cs_change = true;
    spi_message_add_tail(&xfer[1], &message);

    ret = spi_sync(spi, &message);
    if (ret < 0)
        return ret;

    return message.status;
}

static int my_spi_write_buf(struct spi_device *spi, const u8 *buf, size_t len)
{
    struct spi_message message;
    struct spi_transfer xfer;
    int ret;

    spi_message_init(&message);
    memset(&xfer, 0, sizeof(xfer));

    xfer.tx_buf = buf;
    xfer.len = len;
    xfer.cs_change = true;
    spi_message_add_tail(&xfer, &message);

    ret = spi_sync(spi, &message);
    if (ret < 0)
        return ret;

    return message.status;
}
```

SPI 驱动与 I2C 驱动的主要区别在于传输方式：I2C 使用消息队列机制，而 SPI 使用 spi_message 和 spi_transfer 结构体来描述传输。此外，SPI 需要处理片选信号（chip select）的切换，以及时钟极性和相位的配置。

## 6.3 传感器驱动实例

在嵌入式系统中，I2C 和 SPI 总线最常见的应用场景之一就是连接各种传感器。本节将通过两个实际的传感器驱动案例，帮助读者将前面学到的理论知识应用到实际开发中。

### 6.3.1 温湿度传感器（I2C）

以常见的 DHT12 温湿度传感器为例（兼容 DHT11），演示 I2C 传感器驱动的开发。DHT12 是一款基于 I2C 总线的温湿度传感器，它采用单总线和 I2C 两种接口，本例使用 I2C 接口。

```c
/*
 * DHT12 I2C 温湿度传感器驱动
 * 文件位置：drivers/i2c/bus/dht12.c
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/device.h>

#define DHT12_NAME           "dht12"
#define DHT12_ADDR           0x5C

// 寄存器定义
#define DHT12_REG_HUMIDITY   0x00    // 湿度整数部分
#define DHT12_REG_TEMP       0x02    // 温度整数部分
#define DHT12_REG_HUMIDITY_DEC 0x01 // 湿度小数部分（可选）
#define DHT12_REG_TEMP_DEC  0x03    // 温度小数部分（可选）
#define DHT12_REG_MODEL     0x0D    // 传感器型号
#define DHT12_REG_STATUS    0x0F    // 传感器状态

// 私有数据结构
struct dht12_data {
    struct i2c_client *client;
    struct hwmon_chip_info chip_info;
    char name[32];
};

// 读取温湿度数据
static int dht12_read_humidity(struct i2c_client *client, int *val)
{
    int ret;
    u8 humidity;
    struct i2c_msg msgs[2];
    u8 reg = DHT12_REG_HUMIDITY;
    u8 buf;

    // 发送寄存器地址并读取湿度
    msgs[0].addr = client->addr;
    msgs[0].flags = 0;  // 写
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;  // 读
    msgs[1].len = 1;
    msgs[1].buf = &buf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0)
        return ret;
    if (ret != 2)
        return -EIO;

    humidity = buf;
    *val = humidity * 100;  // 转换为百分之一精度

    return 0;
}

static int dht12_read_temperature(struct i2c_client *client, int *val)
{
    int ret;
    u8 temp;
    struct i2c_msg msgs[2];
    u8 reg = DHT12_REG_TEMP;
    u8 buf;

    // 发送寄存器地址并读取温度
    msgs[0].addr = client->addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 1;
    msgs[1].buf = &buf;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret < 0)
        return ret;
    if (ret != 2)
        return -EIO;

    temp = buf;
    // 温度数据为补码形式，需要处理
    if (temp & 0x80) {
        temp = ~(temp - 1) & 0x7F;
        *val = -temp * 100;
    } else {
        *val = temp * 100;
    }

    return 0;
}

// hwmon 回调函数
static int dht12_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
                            u32 attr, int *val)
{
    struct dht12_data *data = dev_get_drvdata(dev);
    struct i2c_client *client = data->client;
    int ret;

    if (type == hwmon_humidity) {
        ret = dht12_read_humidity(client, val);
    } else if (type == hwmon_temp) {
        ret = dht12_read_temperature(client, val);
    } else {
        return -EINVAL;
    }

    return ret;
}

static const u32 dht12_chip_config[] = {
    HWMON_C_REGISTER_TZ,
    0
};

static const struct hwmon_channel_info dht12_chip_info = {
    .type = hwmon_chip,
    .config = dht12_chip_config,
};

static const u32 dht12_temp_config[] = {
    HWMON_T_INPUT,
    0
};

static const u32 dht12_humidity_config[] = {
    HWMON_H_INPUT,
    0
};

static const struct hwmon_channel_info dht12_temp_info = {
    .type = hwmon_temp,
    .config = dht12_temp_config,
};

static const struct hwmon_channel_info dht12_humidity_info = {
    .type = hwmon_humidity,
    .config = dht12_humidity_config,
};

static const struct hwmon_channel_info *dht12_info[] = {
    &dht12_chip_info,
    &dht12_temp_info,
    &dht12_humidity_info,
    NULL
};

static const struct hwmon_ops dht12_hwmon_ops = {
    .read = dht12_hwmon_read,
};

static const struct hwmon_chip_info dht12_chip_info_template = {
    .ops = &dht12_hwmon_ops,
    .info = dht12_info,
};

// 设备树匹配表
static const struct of_device_id dht12_of_match[] = {
    { .compatible = "dht12", .data = NULL },
    { .compatible = "aosong,dht12", .data = NULL },
    { }
};
MODULE_DEVICE_TABLE(of, dht12_of_match);

// ID 表
static const struct i2c_device_id dht12_id[] = {
    { "dht12", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, dht12_id);

// probe 函数
static int dht12_probe(struct i2c_client *client,
                       const struct i2c_device_id *id)
{
    struct device *dev = &client->dev;
    struct dht12_data *data;
    struct device *hwmon_dev;
    int ret;

    // 检查设备是否存在
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
        return -ENODEV;

    data = devm_kzalloc(dev, sizeof(struct dht12_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    i2c_set_clientdata(client, data);
    data->client = client;
    snprintf(data->name, sizeof(data->name), "%s", DHT12_NAME);

    // 注册 hwmon 设备
    hwmon_dev = devm_hwmon_device_register_with_info(dev, data->name, data,
                                                      &dht12_chip_info_template);
    if (IS_ERR(hwmon_dev))
        return PTR_ERR(hwmon_dev);

    dev_info(dev, "DHT12 sensor registered\n");
    return 0;
}

// remove 函数
static int dht12_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "DHT12 sensor removed\n");
    return 0;
}

// 驱动结构体
static struct i2c_driver dht12_driver = {
    .driver = {
        .name = DHT12_NAME,
        .of_match_table = dht12_of_match,
    },
    .probe = dht12_probe,
    .remove = dht12_remove,
    .id_table = dht12_id,
};

module_i2c_driver(dht12_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("DHT12 I2C Humidity and Temperature Sensor Driver");
MODULE_LICENSE("GPL");
```

这个驱动示例展示了如何将传感器数据通过 Linux hwmon（硬件监控）子系统暴露给用户空间。hwmon 是 Linux 内核用于统一管理硬件监控传感器（如温度、电压、电流等）的框架，用户可以通过 `/sys/class/hwmon/` 访问传感器数据。

### 6.3.2 SPI 传感器

以常见的 BMP280 气压温度传感器为例（也支持 I2C 接口，这里展示 SPI 接口），演示 SPI 传感器驱动的开发。BMP280 是 Bosch 公司生产的高精度气压传感器，广泛应用于无人机、气象站、可穿戴设备等场景。

```c
/*
 * BMP280 SPI 气压温度传感器驱动
 * 文件位置：drivers/spi/bmp280.c
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/regmap.h>

#define BMP280_NAME           "bmp280"

// 寄存器定义
#define BMP280_REG_ID         0xD0
#define BMP280_REG_RESET      0xE0
#define BMP280_REG_STATUS     0xF3
#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_CONFIG     0xF5
#define BMP280_REG_PRESS_MSB  0xF7
#define BMP280_REG_PRESS_LSB  0xF8
#define BMP280_REG_PRESS_XLSB 0xF9
#define BMP280_REG_TEMP_MSB  0xFA
#define BMP280_REG_TEMP_LSB  0xFB
#define BMP280_REG_TEMP_XLSB 0xFC
#define BMP280_REG_HUM_MSB   0xFD
#define BMP280_REG_HUM_LSB   0xFE

// 校准参数寄存器（从 0x88 开始，共 24 字节）
#define BMP280_REG_CALIB      0x88

// 设备 ID
#define BMP280_ID             0x58

// 工作模式
#define BMP280_MODE_SLEEP     0x00
#define BMP280_MODE_FORCED    0x01
#define BMP280_MODE_NORMAL    0x03

// 采样率
#define BMP280_SAMPLING_X1    0x01
#define BMP280_SAMPLING_X2    0x02
#define BMP280_SAMPLING_X4    0x04
#define BMP280_SAMPLING_X8    0x05
#define BMP280_SAMPLING_X16   0x06

// 私有数据结构
struct bmp280_data {
    struct spi_device *spi;
    struct regmap *regmap;
    struct mutex lock;

    // 校准参数
    s32 dig_T1, dig_T2, dig_T3;
    s32 dig_P1, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    s32 dig_H1, dig_H2, dig_H3, dig_H4, dig_H5, dig_H6;

    // 采样配置
    u8 osrs_t;
    u8 osrs_p;
    u8 mode;
};

// 读取校准参数
static int bmp280_read_calib_data(struct bmp280_data *data)
{
    unsigned char calib[24];
    int ret;

    // 读取校准参数
    ret = regmap_bulk_read(data->regmap, BMP280_REG_CALIB, calib, 24);
    if (ret < 0)
        return ret;

    // 解析温度校准参数
    data->dig_T1 = (calib[1] << 8) | calib[0];
    data->dig_T2 = (calib[3] << 8) | calib[2];
    data->dig_T3 = (calib[5] << 8) | calib[4];

    // 解析气压校准参数
    data->dig_P1 = (calib[7] << 8) | calib[6];
    data->dig_P2 = (calib[9] << 8) | calib[8];
    data->dig_P3 = (calib[11] << 8) | calib[10];
    data->dig_P4 = (calib[13] << 8) | calib[12];
    data->dig_P5 = (calib[15] << 8) | calib[14];
    data->dig_P6 = (calib[17] << 8) | calib[16];
    data->dig_P7 = (calib[19] << 8) | calib[18];
    data->dig_P8 = (calib[21] << 8) | calib[20];
    data->dig_P9 = (calib[23] << 8) | calib[22];

    return 0;
}

// 读取未补偿的温度值
static int bmp280_read_raw_temp(struct bmp280_data *data, s32 *raw)
{
    u8 reg = BMP280_REG_TEMP_MSB;
    u8 buf[3];
    int ret;

    // 强制测量
    ret = regmap_update_bits(data->regmap, BMP280_REG_CTRL_MEAS,
                              BMP280_MODE_MASK, BMP280_MODE_FORCED);
    if (ret < 0)
        return ret;

    // 等待测量完成（根据配置可能需要几十毫秒）
    usleep_range(2000, 5000);

    ret = regmap_bulk_read(data->regmap, reg, buf, 3);
    if (ret < 0)
        return ret;

    *raw = ((s32)buf[0] << 12) | ((s32)buf[1] << 4) | ((s32)buf[2] >> 4);

    return 0;
}

// 读取未补偿的气压值
static int bmp280_read_raw_pressure(struct bmp280_data *data, s32 *raw)
{
    u8 reg = BMP280_REG_PRESS_MSB;
    u8 buf[3];
    int ret;

    ret = regmap_bulk_read(data->regmap, reg, buf, 3);
    if (ret < 0)
        return ret;

    *raw = ((s32)buf[0] << 12) | ((s32)buf[1] << 4) | ((s32)buf[2] >> 4);

    return 0;
}

// 温度补偿算法（BMP280 数据手册中的公式）
static s32 bmp280_compensate_T(struct bmp280_data *data, s32 adc_T)
{
    s32 var1, var2, T;

    var1 = ((((adc_T >> 3) - ((s32)data->dig_T1 << 1))) * ((s32)data->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((s32)data->dig_T1)) * ((adc_T >> 4) - ((s32)data->dig_T1))) >> 12) * ((s32)data->dig_T3)) >> 14;

    data->t_fine = var1 + var2;
    T = (data->t_fine * 5 + 128) >> 8;

    return T;
}

// 气压补偿算法
static u32 bmp280_compensate_P(struct bmp280_data *data, s32 adc_P)
{
    s64 var1, var2, p;
    s32 T = data->t_fine;

    var1 = ((s64)T) - 128000;
    var2 = var1 * var1 * (s64)data->dig_P6;
    var2 = var2 + ((var1 * (s64)data->dig_P5) << 17);
    var2 = var2 + (((s64)data->dig_P4) << 35);
    var1 = ((var1 * var1 * (s64)data->dig_P3) >> 8) + ((var1 * (s64)data->dig_P2) << 12);
    var1 = ((((s64)1) << 47) + var1) * ((s64)data->dig_P1) >> 33;

    if (var1 == 0)
        return 0;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((s64)data->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((s64)data->dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((s64)data->dig_P7) << 4);

    return (u32)p;
}

// 读取温度（摄氏度）
static int bmp280_read_temperature(struct device *dev, long *val)
{
    struct bmp280_data *data = dev_get_drvdata(dev);
    s32 raw, comp;
    int ret;

    mutex_lock(&data->lock);

    ret = bmp280_read_raw_temp(data, &raw);
    if (ret < 0) {
        mutex_unlock(&data->lock);
        return ret;
    }

    comp = bmp280_compensate_T(data, raw);

    mutex_unlock(&data->lock);

    *val = comp;
    return 0;
}

// 读取气压（Pa）
static int bmp280_read_pressure(struct device *dev, long *val)
{
    struct bmp280_data *data = dev_get_drvdata(dev);
    s32 raw_press, raw_temp;
    u32 comp;
    int ret;

    mutex_lock(&data->lock);

    ret = bmp280_read_raw_temp(data, &raw_temp);
    if (ret < 0) {
        mutex_unlock(&data->lock);
        return ret;
    }
    bmp280_compensate_T(data, raw_temp);

    ret = bmp280_read_raw_pressure(data, &raw_press);
    if (ret < 0) {
        mutex_unlock(&data->lock);
        return ret;
    }

    comp = bmp280_compensate_P(data, raw_press);

    mutex_unlock(&data->lock);

    *val = comp;
    return 0;
}

// hwmon 操作函数
static int bmp280_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
                              u32 attr, int *val)
{
    int ret;

    if (type == hwmon_temp) {
        ret = bmp280_read_temperature(dev, val);
    } else if (type == hwmon_pressure) {
        ret = bmp280_read_pressure(dev, val);
    } else {
        return -EINVAL;
    }

    return ret;
}

static const u32 bmp280_chip_config[] = {
    HWMON_C_REGISTER_TZ,
    0
};

static const struct hwmon_channel_info bmp280_chip_info = {
    .type = hwmon_chip,
    .config = bmp280_chip_config,
};

static const u32 bmp280_temp_config[] = {
    HWMON_T_INPUT,
    0
};

static const u32 bmp280_pressure_config[] = {
    HWMON_P_INPUT,
    0
};

static const struct hwmon_channel_info bmp280_temp_info = {
    .type = hwmon_temp,
    .config = bmp280_temp_config,
};

static const struct hwmon_channel_info bmp280_pressure_info = {
    .type = hwmon_pressure,
    .config = bmp280_pressure_config,
};

static const struct hwmon_channel_info *bmp280_info[] = {
    &bmp280_chip_info,
    &bmp280_temp_info,
    &bmp280_pressure_info,
    NULL
};

static const struct hwmon_ops bmp280_hwmon_ops = {
    .read = bmp280_hwmon_read,
};

static const struct hwmon_chip_info bmp280_chip_info_template = {
    .ops = &bmp280_hwmon_ops,
    .info = bmp280_info,
};

// 设备树匹配表
static const struct of_device_id bmp280_of_match[] = {
    { .compatible = "bosch,bmp280", .data = NULL },
    { .compatible = "bosch,bmp280-spi", .data = NULL },
    { }
};
MODULE_DEVICE_TABLE(of, bmp280_of_match);

// SPI 设备 ID
static const struct spi_device_id bmp280_id[] = {
    { "bmp280", 0 },
    { }
};
MODULE_DEVICE_TABLE(spi, bmp280_id);

// regmap 配置
static int bmp280_regmap_spi_read(void *context, const void *reg,
                                   size_t reg_size, void *val,
                                   size_t val_size)
{
    struct device *dev = context;
    struct spi_device *spi = to_spi_device(dev);
    int ret;
    u8 tx_buf[2];

    if (reg_size != 1)
        return -EINVAL;

    tx_buf[0] = *(u8 *)reg | 0x80;  // SPI 读操作：最高位置 1

    ret = spi_write_then_read(spi, tx_buf, 1, val, val_size);
    return ret;
}

static int bmp280_regmap_spi_write(void *context, const void *data,
                                    size_t count)
{
    struct device *dev = context;
    struct spi_device *spi = to_spi_device(dev);
    const u8 *tx = data;

    /* 最高位清零表示写操作 */
    tx[0] &= 0x7F;

    return spi_write(spi, tx, count);
}

static const struct regmap_bus bmp280_regmap_bus = {
    .read = bmp280_regmap_spi_read,
    .write = bmp280_regmap_spi_write,
    .max_raw_read = 4,
    .max_raw_write = 4,
};

static const struct regmap_config bmp280_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0xFF,
};

// probe 函数
static int bmp280_probe(struct spi_device *spi)
{
    struct device *dev = &spi->dev;
    struct bmp280_data *data;
    struct device *hwmon_dev;
    unsigned int chip_id;
    int ret;

    data = devm_kzalloc(dev, sizeof(struct bmp280_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    spi_set_drvdata(spi, data);
    data->spi = spi;

    mutex_init(&data->lock);

    // 配置 regmap
    data->regmap = devm_regmap_init(dev, &bmp280_regmap_bus,
                                    dev, &bmp280_regmap_config);
    if (IS_ERR(data->regmap))
        return PTR_ERR(data->regmap);

    // 读取芯片 ID
    ret = regmap_read(data->regmap, BMP280_REG_ID, &chip_id);
    if (ret < 0)
        return ret;

    if (chip_id != BMP280_ID) {
        dev_err(dev, "Invalid chip ID: 0x%02x (expected 0x%02x)\n",
                chip_id, BMP280_ID);
        return -ENODEV;
    }

    dev_info(dev, "Found BMP280 chip, ID: 0x%02x\n", chip_id);

    // 读取校准数据
    ret = bmp280_read_calib_data(data);
    if (ret < 0)
        return ret;

    // 配置传感器
    data->osrs_t = BMP280_SAMPLING_X2;
    data->osrs_p = BMP280_SAMPLING_X16;
    data->mode = BMP280_MODE_NORMAL;

    ret = regmap_write(data->regmap, BMP280_REG_CTRL_MEAS,
                       (data->osrs_t << 5) | (data->osrs_p << 2) | data->mode);
    if (ret < 0)
        return ret;

    // 注册 hwmon 设备
    hwmon_dev = devm_hwmon_device_register_with_info(dev, "bmp280", data,
                                                      &bmp280_chip_info_template);
    if (IS_ERR(hwmon_dev))
        return PTR_ERR(hwmon_dev);

    dev_info(dev, "BMP280 sensor registered\n");
    return 0;
}

// remove 函数
static int bmp280_remove(struct spi_device *spi)
{
    struct bmp280_data *data = spi_get_drvdata(spi);

    // 进入睡眠模式
    regmap_write(data->regmap, BMP280_REG_CTRL_MEAS, BMP280_MODE_SLEEP);

    dev_info(&spi->dev, "BMP280 sensor removed\n");
    return 0;
}

// 驱动结构体
static struct spi_driver bmp280_driver = {
    .driver = {
        .name = BMP280_NAME,
        .of_match_table = bmp280_of_match,
    },
    .probe = bmp280_probe,
    .remove = bmp280_remove,
    .id_table = bmp280_id,
};

module_spi_driver(bmp280_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("BMP280 SPI Pressure and Temperature Sensor Driver");
MODULE_LICENSE("GPL");
```

这个 BMP280 驱动示例展示了 SPI 传感器驱动的完整开发流程，包括：使用 regmap 简化 SPI 寄存器访问、实现传感器数据的补偿算法、通过 hwmon 子系统向用户空间提供数据等关键技术点。

## 本章面试题

### 面试题1：I2C 和 SPI 总线的区别

I2C 和 SPI 是两种最常用的嵌入式系统串行通信协议，它们在硬件连接、通信特性、性能特点和适用场景等方面都有显著差异。

**硬件连接方面**：I2C 仅需两根信号线（SCL 时钟线和 SDA 数据线）即可完成通信，支持多主设备和多从设备挂在同一总线上，通过地址识别不同的从设备。而 SPI 需要至少四根信号线（SCK 时钟线、MOSI 主输出从输入、MISO 主输入从输出、CS 片选线），每个从设备都需要独立的片选信号，因此引脚数量随设备增加而增加。

**通信速率方面**：I2C 标准模式下速率为 100kbps，快速模式为 400kbps，快速模式+可达 1Mbps，高速模式可达 3.4Mbps。SPI 的速率则取决于硬件实现，理论可达数十 MHz，在高速数据传输场景中更有优势。

**通信模式方面**：I2C 是半双工通信，采用主从结构，支持多主设备但同一时刻只能有一个主设备发送数据。SPI 支持全双工通信，主设备可以同时发送和接收数据，通信效率更高。

**数据格式方面**：I2C 有严格的协议格式，包括起始位、设备地址、R/W 位、应答位、数据字节和停止位等。SPI 的数据格式相对简单，没有标准化的设备地址机制，通过片选信号选择从设备。

**从设备寻址方面**：I2C 通过 7 位或 10 位设备地址来寻址从设备，总线上最多可挂 128（7 位）或 1024（10 位）个设备。SPI 通过硬件片选线来选择从设备，片选数量受主设备 GPIO 引脚限制。

**适用场景方面**：I2C 适合连接低速外设、传感器、EEPROM、RTC 等设备，其简单的两根线接口和完善的协议使其在资源受限的场景中广泛应用。SPI 适合高速数据传输场景，如显示驱动、存储设备、音频编解码器、ADC/DAC 等，其全双工和高频率特性满足大数据量传输需求。

### 面试题2：描述 I2C 设备驱动的注册过程

I2C 设备驱动的注册过程涉及多个组件的协作，包括 I2C 核心、总线适配器和设备驱动。以下是详细的注册流程分析。

**驱动注册入口**：驱动通过 `module_i2c_driver()` 宏或直接调用 `i2c_add_driver()` 函数注册到内核。这个宏展开后会调用 `driver_register()` 和 `i2c_driver` 结构体的初始化函数。

**驱动注册到 I2C 总线**：在 `i2c_add_driver()` 函数中，首先将驱动添加到 I2C 总线的驱动链表中，然后遍历系统中所有已注册的 I2C 适配器（i2c_adapter），尝试为每个适配器上的现有设备进行匹配。

**设备与驱动的匹配机制**：I2C 核心提供了多种匹配方式，包括 OF 设备树匹配（通过 `of_match_table`）、ACPI 匹配（通过 `acpi_match_table`）、ID 表匹配（通过 `id_table`）和传统名称匹配。匹配过程会调用 `i2c_device_match()` 函数，该函数按照优先级依次尝试各种匹配方式。

**probe 函数调用**：当设备与驱动匹配成功后，I2C 核心会创建一个 i2c_client 结构体来代表这个设备，然后调用驱动的 `probe()` 函数。在 probe 函数中，驱动可以进行设备初始化、分配私有数据、注册字符设备或 hwmon 设备等操作。

**设备创建过程**：对于从设备树创建的 i2c_client，设备信息从设备树节点中解析，包括 I2C 地址、中断号、GPIO 等属性。I2C 核心会根据设备树节点信息自动创建对应的设备条目，在 `/sys/bus/i2c/devices/` 目录下可以看到系统中的所有 I2C 设备。

**用户空间可见性**：驱动成功注册后，设备会在 sysfs 中创建相应的节点。用户空间可以通过 `/sys/bus/i2c/devices/` 访问 I2C 设备，通过 `/sys/class/` 访问驱动创建的其他设备类节点（如 hwmon）。同时，I2C 设备文件 `/dev/i2c-N` 允许用户空间程序直接使用 I2C 总线进行通信。

理解 I2C 驱动的注册过程对于调试驱动问题和深入理解 Linux 设备模型都非常重要。驱动的注册不是孤立的过程，而是与设备模型、总线机制、sysfs 文件系统等多个内核子系统紧密协作的结果。

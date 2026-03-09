# Nor Flash 驱动框架设计

驱动框架是 Nor Flash 软件开发的基础设施。一个设计良好的驱动框架能够适配多种 Flash 芯片型号、支持不同的硬件接口（SPI、FSMC/FMC），并为上层应用提供统一、简洁的编程接口。本章将详细介绍驱动框架的分层架构设计、设备抽象层（Device Abstraction Layer, DAL）设计、传输层抽象、核心驱动模块以及回调机制。

---

## 1. 驱动架构概述

### 1.1 分层架构设计理念

Nor Flash 驱动采用分层架构设计，其核心理念是**关注点分离（Separation of Concerns）**和**接口隔离（Interface Segregation）**。通过将驱动代码划分为多个层次，每个层次只关注特定的功能，从而实现：

- **可移植性**：硬件相关代码集中在底层，上层逻辑与硬件解耦
- **可维护性**：各层职责清晰，便于调试和维护
- **可扩展性**：新增芯片支持或接口类型时，只需在对应层次进行扩展
- **代码复用**：通用逻辑在上层实现，不同芯片/接口共享

分层架构使得开发者可以在不修改上层代码的情况下：
- 更换不同厂商的 Flash 芯片
- 从 SPI 接口迁移到并行接口
- 适配新的微控制器平台

### 1.2 驱动层次结构

Nor Flash 驱动从上到下分为四个层次：应用层、驱动层、传输层和硬件层。各层的职责和交互关系如下：

```
┌─────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)            │
│              业务逻辑、文件系统、固件升级等                    │
└─────────────────────────────┬───────────────────────────────┘
                              │ 标准设备API
┌─────────────────────────────▼───────────────────────────────┐
│                      驱动层 (Driver Layer)                   │
│         设备初始化、读写操作、擦除、状态管理、错误处理           │
└─────────────────────────────┬───────────────────────────────┘
                              │ 传输接口抽象
┌─────────────────────────────▼───────────────────────────────┐
│                     传输层 (Transport Layer)                 │
│         SPI传输、GPIO模拟SPI、FSMC/FMC并行传输               │
└─────────────────────────────┬───────────────────────────────┘
                              │ GPIO/SPI寄存器操作
┌─────────────────────────────▼───────────────────────────────┐
│                      硬件层 (Hardware Layer)                  │
│              GPIO配置、SPI控制器、FSMC控制器                  │
└─────────────────────────────────────────────────────────────┘
```

**各层详细说明**：

| 层次 | 职责 | 包含内容 |
|------|------|----------|
| 应用层 | 业务逻辑实现 | 文件系统接口、固件升级、参数存储 |
| 驱动层 | Flash 设备操作 | 读/写/擦除逻辑、时序控制、状态机管理 |
| 传输层 | 数据传输抽象 | SPI发送/接收、GPIO模拟时序、FSMC读写 |
| 硬件层 | 硬件寄存器操作 | GPIO配置、SPI寄存器、FSMC寄存器 |

### 1.3 驱动设计原则

在设计 Nor Flash 驱动框架时，遵循以下核心原则：

**（1）接口抽象原则**

所有对外接口使用抽象函数指针或虚拟接口，不直接依赖具体实现。传输层通过统一的传输接口（传输函数指针结构体）来隔离硬件差异。

**（2）配置驱动原则**

芯片的具体参数（如页大小、扇区大小、命令码等）通过配置结构体或数据表提供，而非硬编码。驱动代码根据配置动态适配不同芯片。

**（3）状态机驱动原则**

Flash 的写和擦除操作不是立即完成的，需要轮询状态位或等待中断。驱动内部维护状态机来处理这些耗时操作的各个阶段。

**（4）错误处理原则**

所有可能失败的操作都返回错误码，并提供错误信息查询接口。关键操作支持重试机制，提高可靠性。

**（5）资源保护原则**

对共享资源（如 SPI 总线）使用互斥保护，防止并发访问导致数据错乱。

---

## 2. 设备抽象层（DAL）设计

设备抽象层是驱动框架的核心，它定义了 Nor Flash 设备的通用表示和操作接口。通过设备抽象层，上层应用可以使用统一的 API 操作不同型号的 Flash 芯片，而无需关心具体的硬件实现细节。

### 2.1 设备描述结构体

设备描述结构体是 Nor Flash 驱动的基础数据结构，它包含了芯片的所有关键参数和操作函数指针。

```c
/**
 * Nor Flash 设备描述结构体
 * 包含芯片参数、传输接口和操作函数指针
 */
typedef struct {
    /* 芯片识别信息 */
    uint8_t  manufacturer_id;      // 厂商ID
    uint16_t device_id;            // 器件ID
    char     chip_name[32];        // 芯片名称（如 "W25Q256JV"）

    /* 存储容量信息 */
    uint32_t total_size;           // 总容量（字节）
    uint32_t sector_size;          // 扇区大小（通常为4KB/32KB/64KB）
    uint32_t block_size;           // 块大小（通常为64KB）
    uint32_t page_size;            // 页大小（SPI Flash通常为256字节）

    /* 接口配置 */
    nor_iface_type_t iface_type;   // 接口类型：SPI/FSMC
    uint8_t  addr_bytes;           // 地址字节数（3或4字节）

    /* 传输接口 */
    const nor_transport_t *transport;  // 传输接口函数指针

    /* 芯片特性标志 */
    uint32_t flags;                // 特性标志
#define NOR_FLAG_4BYTE_ADDR    (1 << 0)  // 支持4字节地址模式
#define NOR_FLAG_DUAL_SPI      (1 << 1)  // 支持Dual SPI
#define NOR_FLAG_QUAD_SPI      (1 << 2)  // 支持Quad SPI
#define NOR_FLAG_OCTAL_SPI     (1 << 3)  // 支持Octal SPI
#define NOR_FLAG_HAS_RDID      (1 << 4)  // 支持Read ID
#define NOR_FLAG_HAS_SFDP      (1 << 5)  // 支持SFDP
#define NOR_FLAG_HAS_RESET     (1 << 6)  // 支持软件复位

    /* 通用命令码 */
    nor_cmd_t cmd;                 // 命令码集合

    /* 回调函数 */
    nor_callbacks_t callbacks;     // 回调函数结构体

    /* 运行时状态 */
    nor_state_t state;             // 当前状态
    uint8_t  write_in_progress;   // 写/擦除进行中标志

    /* 私有数据 */
    void    *priv;                 // 私有数据指针
} nor_device_t;
```

### 2.2 通用设备接口定义

通用设备接口定义了所有 Nor Flash 设备必须支持的标准操作。这些操作以函数指针的形式定义在设备结构体中，或者提供独立的 API 函数。

```c
/**
 * Nor Flash 通用操作接口
 */
typedef struct {
    /**
     * 初始化设备
     * @param dev 设备句柄
     * @return 0 success, -1 failed
     */
    int (*init)(nor_device_t *dev);

    /**
     * 读取数据
     * @param dev 设备句柄
     * @param addr 起始地址
     * @param buf 接收缓冲区
     * @param len 读取长度
     * @return 实际读取长度，负值表示错误
     */
    int (*read)(nor_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);

    /**
     * 页编程（写入）
     * @param dev 设备句柄
     * @param addr 起始地址（必须页对齐）
     * @param buf 数据缓冲区
     * @param len 写入长度
     * @return 实际写入长度，负值表示错误
     */
    int (*write)(nor_device_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

    /**
     * 扇区擦除（4KB）
     * @param dev 设备句柄
     * @param addr 扇区地址
     * @return 0 success, 负值表示错误
     */
    int (*erase_sector)(nor_device_t *dev, uint32_t addr);

    /**
     * 块擦除（32KB/64KB）
     * @param dev 设备句柄
     * @param addr 块地址
     * @return 0 success, 负值表示错误
     */
    int (*erase_block)(nor_device_t *dev, uint32_t addr);

    /**
     * 整片擦除
     * @param dev 设备句柄
     * @return 0 success, 负值表示错误
     */
    int (*erase_chip)(nor_device_t *dev);

    /**
     * 读取状态寄存器
     * @param dev 设备句柄
     * @param status 状态寄存器值
     * @return 0 success, 负值表示错误
     */
    int (*read_status)(nor_device_t *dev, uint8_t *status);

    /**
     * 等待操作完成（轮询状态位）
     * @param dev 设备句柄
     * @param timeout 超时时间（毫秒）
     * @return 0 success, -ETIMEDOUT 超时
     */
    int (*wait_ready)(nor_device_t *dev, uint32_t timeout);

    /**
     * 读取芯片ID
     * @param dev 设备句柄
     * @param mfg_id 厂商ID输出
     * @param dev_id 器件ID输出
     * @return 0 success, 负值表示错误
     */
    int (*read_id)(nor_device_t *dev, uint8_t *mfg_id, uint16_t *dev_id);

    /**
     * 软件复位
     * @param dev 设备句柄
     * @return 0 success, 负值表示错误
     */
    int (*reset)(nor_device_t *dev);

    /**
     * 进入4字节地址模式
     * @param dev 设备句柄
     * @return 0 success, 负值表示错误
     */
    int (*enter_4byte)(nor_device_t *dev);

    /**
     * 退出4字节地址模式
     * @param dev 设备句柄
     * @return 0 success, 负值表示错误
     */
    int (*exit_4byte)(nor_device_t *dev);
} nor_ops_t;
```

### 2.3 设备操作函数指针

除了上述结构体定义的接口外，还可以定义独立的操作函数指针类型，用于更灵活的操作：

```c
/**
 * 传输操作函数类型
 */
typedef int (*nor_transport_send_t)(void *handle, const uint8_t *tx_buf, uint32_t tx_len);
typedef int (*nor_transport_recv_t)(void *handle, uint8_t *rx_buf, uint32_t rx_len);
typedef int (*nor_transport_transfer_t)(void *handle, const uint8_t *tx_buf,
                                         uint8_t *rx_buf, uint32_t len);

/**
 * GPIO 操作函数类型（用于GPIO模拟SPI）
 */
typedef void (*nor_gpio_set_t)(void *handle, uint8_t state);
typedef void (*nor_gpio_toggle_t)(void *handle);
typedef uint8_t (*nor_gpio_get_t)(void *handle);

/**
 * 延时函数类型
 */
typedef void (*nor_delay_us_t)(uint32_t us);
typedef void (*nor_delay_ms_t)(uint32_t ms);
```

### 2.4 芯片参数存储

不同型号的 Nor Flash 芯片具有不同的参数（如容量、命令码、时序要求等）。这些参数可以通过静态配置表的方式存储，驱动在初始化时根据芯片 ID 查找对应的参数。

```c
/**
 * Nor Flash 芯片参数表项
 */
typedef struct {
    uint8_t  manufacturer_id;      // 厂商ID
    uint16_t device_id_start;      // 器件ID起始值
    uint16_t device_id_end;        // 器件ID结束值（用于ID范围匹配）
    const char *name;              // 芯片名称

    /* 容量参数 */
    uint32_t total_size;           // 总容量（字节）
    uint32_t sector_size;          // 最小擦除单元（扇区）
    uint32_t block_size;           // 块擦除单元
    uint32_t page_size;            // 页编程大小

    /* 地址配置 */
    uint8_t  addr_bytes;           // 地址字节数

    /* 特性标志 */
    uint32_t flags;                // 支持的特性

    /* 特定命令 */
    const nor_cmd_t *cmd;          // 命令码集合（可选）
} nor_flash_param_t;

/**
 * 预定义的芯片参数表
 */
static const nor_flash_param_t nor_flash_table[] = {
    /* Winbond W25Q256JV */
    {
        .manufacturer_id = 0xEF,
        .device_id_start = 0x4019,
        .device_id_end   = 0x4019,
        .name            = "W25Q256JV",
        .total_size      = 32 * 1024 * 1024,  // 256Mbit
        .sector_size     = 4 * 1024,          // 4KB扇区
        .block_size      = 64 * 1024,         // 64KB块
        .page_size       = 256,
        .addr_bytes      = 3,                 // 默认3字节，支持4字节
        .flags           = NOR_FLAG_4BYTE_ADDR | NOR_FLAG_QUAD_SPI | NOR_FLAG_HAS_SFDP,
    },
    /* GigaDevice GD25Q256C */
    {
        .manufacturer_id = 0xC8,
        .device_id_start = 0x4019,
        .device_id_end   = 0x4019,
        .name            = "GD25Q256C",
        .total_size      = 32 * 1024 * 1024,
        .sector_size     = 4 * 1024,
        .block_size      = 64 * 1024,
        .page_size       = 256,
        .addr_bytes      = 3,
        .flags           = NOR_FLAG_4BYTE_ADDR | NOR_FLAG_QUAD_SPI,
    },
    /* Macronix MX25L25645G */
    {
        .manufacturer_id = 0xC2,
        .device_id_start = 0x2019,
        .device_id_end   = 0x2019,
        .name            = "MX25L25645G",
        .total_size      = 32 * 1024 * 1024,
        .sector_size     = 4 * 1024,
        .block_size      = 64 * 1024,
        .page_size       = 256,
        .addr_bytes      = 3,
        .flags           = NOR_FLAG_4BYTE_ADDR | NOR_FLAG_QUAD_SPI | NOR_FLAG_HAS_SFDP,
    },
    /* 结束标记 */
    { .manufacturer_id = 0xFF }
};
```

通过这种参数表的方式，驱动可以自动识别芯片类型并加载正确的参数。当检测到新的芯片型号时，只需要在参数表中添加对应的条目即可。

---

## 3. 传输层抽象

传输层是驱动框架中负责数据实际传输的层次。由于 Nor Flash 可以通过多种接口连接（SPI、QSPI、FSMC/FMC并行接口、GPIO模拟SPI），传输层抽象使得驱动核心逻辑可以与具体的硬件接口解耦。

### 3.1 SPI传输接口

SPI传输接口是最常用的Nor Flash连接方式。传输接口结构体定义了SPI总线的操作方法：

```c
/**
 * 接口类型枚举
 */
typedef enum {
    NOR_IFACE_TYPE_SPI,        // 标准SPI
    NOR_IFACE_TYPE_DUAL_SPI,  // Dual SPI
    NOR_IFACE_TYPE_QUAD_SPI,  // Quad SPI
    NOR_IFACE_TYPE_OCTAL_SPI, // Octal SPI
    NOR_IFACE_TYPE_FSMC,       // FSMC/FMC并行接口
    NOR_IFACE_TYPE_GPIO_SPI   // GPIO模拟SPI
} nor_iface_type_t;

/**
 * SPI传输接口结构体
 */
typedef struct {
    nor_iface_type_t type;     // 接口类型

    /**
     * 发送数据（单线模式）
     */
    int (*send)(void *handle, const uint8_t *tx_buf, uint32_t tx_len);

    /**
     * 接收数据（单线模式）
     */
    int (*recv)(void *handle, uint8_t *rx_buf, uint32_t rx_len);

    /**
     * 同时发送和接收（用于全双工通信）
     */
    int (*transfer)(void *handle, const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);

    /**
     * 发送命令（仅发送命令码，不传输数据）
     */
    int (*send_cmd)(void *handle, uint8_t cmd);

    /**
     * 发送命令并接收响应
     */
    int (*send_cmd_recv)(void *handle, uint8_t cmd, uint8_t *resp, uint32_t len);

    /**
     * Quad SPI数据传输（4线模式）
     */
    int (*quad_transfer)(void *handle, const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);

    /**
     * 选中/取消选中芯片
     */
    void (*chip_select)(void *handle, uint8_t selected);

    /**
     * 等待传输完成
     */
    int (*wait_idle)(void *handle, uint32_t timeout_ms);

    /* 私有数据 */
    void *priv;
} nor_spi_transport_t;
```

**SPI传输实现示例**：

```c
/**
 * SPI传输接口实现（基于HAL库）
 */
static int spi_send(void *handle, const uint8_t *tx_buf, uint32_t tx_len)
{
    HAL_StatusTypeDef status = HAL_SPI_Transmit((SPI_HandleTypeDef *)handle,
                                                 (uint8_t *)tx_buf, tx_len, SPI_TIMEOUT);
    return (status == HAL_OK) ? (int)tx_len : -1;
}

static int spi_recv(void *handle, uint8_t *rx_buf, uint32_t rx_len)
{
    HAL_StatusTypeDef status = HAL_SPI_Receive((SPI_HandleTypeDef *)handle,
                                               rx_buf, rx_len, SPI_TIMEOUT);
    return (status == HAL_OK) ? (int)rx_len : -1;
}

static int spi_transfer(void *handle, const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len)
{
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive((SPI_HandleTypeDef *)handle,
                             (uint8_t *)tx_buf, rx_buf, len, SPI_TIMEOUT);
    return (status == HAL_OK) ? (int)len : -1;
}

/**
 * 获取标准SPI传输接口
 */
const nor_spi_transport_t* nor_spi_get_transport(SPI_HandleTypeDef *hspi)
{
    static nor_spi_transport_t transport = {
        .type          = NOR_IFACE_TYPE_SPI,
        .send          = spi_send,
        .recv          = spi_recv,
        .transfer      = spi_transfer,
        .chip_select   = NULL,  // 由外部控制CS
        .wait_idle     = NULL,
    };
    transport.priv = hspi;
    return &transport;
}
```

### 3.2 GPIO模拟SPI接口

在某些场景下（如资源受限的MCU没有SPI控制器，或需要更多控制灵活性），可以使用GPIO模拟SPI时序。传输层提供了统一的GPIO操作接口：

```c
/**
 * GPIO模拟SPI传输接口
 */
typedef struct {
    /* GPIO引脚配置 */
    void *sck_port; uint16_t sck_pin;   // 时钟引脚
    void *mosi_port; uint16_t mosi_pin; // MOSI引脚（主机输出）
    void *miso_port; uint16_t miso_pin; // MISO引脚（主机输入）
    void *cs_port;   uint16_t cs_pin;   // 片选引脚

    /* 延时函数（必须提供微秒级延时） */
    nor_delay_us_t delay_us;

    /* SPI模式配置 */
    uint8_t cpol: 1;   // 时钟极性
    uint8_t cpha: 1;    // 时钟相位
    uint8_t bit_order; // 位序：0=LSB first, 1=MSB first

    /* 内部状态 */
    void *priv;
} nor_gpio_spi_transport_t;

/**
 * GPIO模拟SPI传输函数实现
 */
static int gpio_spi_transfer(void *handle, const uint8_t *tx_buf,
                              uint8_t *rx_buf, uint32_t len)
{
    nor_gpio_spi_transport_t *transport = (nor_gpio_spi_transport_t *)handle;
    uint8_t bit_mask = 0x80;

    for (uint32_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx_buf ? tx_buf[i] : 0xFF;
        uint8_t rx_byte = 0;

        for (int bit = 7; bit >= 0; bit--) {
            /* 发送位 */
            GPIO_WritePin(transport->mosi_port, transport->mosi_pin,
                         (tx_byte & (1 << bit)) ? GPIO_PIN_SET : GPIO_PIN_RESET);

            /* 时钟上升沿 */
            GPIO_WritePin(transport->sck_port, transport->sck_pin, GPIO_PIN_SET);
            transport->delay_us(1);

            /* 读取位 */
            if (GPIO_ReadPin(transport->miso_port, transport->miso_pin) == GPIO_PIN_SET) {
                rx_byte |= (1 << bit);
            }

            /* 时钟下降沿 */
            GPIO_WritePin(transport->sck_port, transport->sck_pin, GPIO_PIN_RESET);
            transport->delay_us(1);
        }

        if (rx_buf) {
            rx_buf[i] = rx_byte;
        }
    }

    return (int)len;
}
```

### 3.3 FSMC/FMC并行接口

对于大容量的并行Nor Flash，通常使用FSMC（Flexible Static Memory Controller）或FMC（Flexible Memory Controller）接口。传输层同样提供了抽象接口：

```c
/**
 * FSMC/FMC并行传输接口
 */
typedef struct {
    /* 内存映射地址 */
    uint32_t base_addr;           // 基地址（由FSMC bank配置决定）

    /* 数据宽度 */
    uint8_t data_width;           // 8或16

    /* 读等待周期配置（根据芯片时序） */
    uint8_t read_wait_cycle;     // 读等待周期数

    /* 写等待周期配置 */
    uint8_t write_wait_cycle;    // 写等待周期数

    /* 同步传输（可选，用于同步Flash） */
    uint8_t sync_mode;           // 同步模式使能

    /* 直接内存访问 */
    int (*dma_read)(void *handle, uint32_t addr, uint8_t *buf, uint32_t len);
    int (*dma_write)(void *handle, uint32_t addr, const uint8_t *buf, uint32_t len);

    void *priv;
} nor_fsmc_transport_t;

/**
 * FSMC读操作实现
 */
static int fsmc_read(void *handle, uint8_t *rx_buf, uint32_t addr, uint32_t len)
{
    nor_fsmc_transport_t *transport = (nor_fsmc_transport_t *)handle;
    uint8_t *mem_ptr = (uint8_t *)(transport->base_addr + addr);

    /* 简单的内存拷贝（编译器会优化为总线访问） */
    memcpy(rx_buf, mem_ptr, len);

    return (int)len;
}

/**
 * FSMC写操作实现
 */
static int fsmc_write(void *handle, const uint8_t *tx_buf, uint32_t addr, uint32_t len)
{
    nor_fsmc_transport_t *transport = (nor_fsmc_transport_t *)handle;
    uint8_t *mem_ptr = (uint8_t *)(transport->base_addr + addr);

    /* 内存拷贝即写操作 */
    memcpy(mem_ptr, tx_buf, len);

    return (int)len;
}
```

### 3.4 统一的传输API

为了使上层驱动代码能够透明地使用不同的传输方式，传输层提供了统一的传输API：

```c
/**
 * 统一的传输接口
 */
typedef struct nor_transport {
    nor_iface_type_t type;

    /* 发送数据 */
    int (*send)(struct nor_transport *t, const uint8_t *buf, uint32_t len);

    /* 接收数据 */
    int (*recv)(struct nor_transport *t, uint8_t *buf, uint32_t len);

    /* 同时发送和接收 */
    int (*transfer)(struct nor_transport *t, const uint8_t *tx_buf,
                    uint8_t *rx_buf, uint32_t len);

    /* 片选控制 */
    void (*cs_select)(struct nor_transport *t);
    void (*cs_deselect)(struct nor_transport *t);

    /* 等待空闲 */
    int (*wait_idle)(struct nor_transport *t, uint32_t timeout_ms);

    /* 私有数据 */
    void *priv;
} nor_transport_t;

/**
 * 发送命令的便捷函数
 */
static inline int nor_transport_cmd(nor_transport_t *t, uint8_t cmd)
{
    return t->send(t, &cmd, 1);
}

/**
 * 发送命令并等待完成
 */
static inline int nor_transport_cmd_wait(nor_transport_t *t, uint8_t cmd,
                                          uint32_t timeout_ms)
{
    int ret = t->send(t, &cmd, 1);
    if (ret < 0) return ret;
    return t->wait_idle(t, timeout_ms);
}
```

---

## 4. 核心驱动模块

核心驱动模块实现了Nor Flash的各种操作，包括初始化、读、写、擦除和状态管理。这些模块构成了驱动的主体逻辑。

### 4.1 初始化模块

初始化是使用Nor Flash的第一步，负责配置硬件、检测芯片、加载参数。初始化流程通常包括以下步骤：

```c
/**
 * Nor Flash 初始化
 * @param dev 设备句柄
 * @param config 初始化配置
 * @return 0 success, 负值表示错误
 */
int nor_init(nor_device_t *dev, const nor_config_t *config)
{
    int ret;

    /* 参数检查 */
    if (!dev || !config || !config->transport) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 1. 初始化传输层 */
    ret = nor_transport_init(config->transport);
    if (ret < 0) {
        return ret;
    }

    /* 2. 软件复位（如果芯片支持） */
    if (dev->flags & NOR_FLAG_HAS_RESET) {
        ret = nor_reset(dev);
        if (ret < 0) {
            return ret;
        }
    }

    /* 3. 读取芯片ID */
    ret = nor_read_id(dev, &dev->manufacturer_id, &dev->device_id);
    if (ret < 0) {
        return -NOR_ERR_NO_DEVICE;
    }

    /* 4. 根据ID查找参数表 */
    const nor_flash_param_t *param = nor_find_param(dev->manufacturer_id,
                                                    dev->device_id);
    if (!param) {
        /* 未找到匹配参数，使用通用配置 */
        param = &nor_default_param;
    }

    /* 5. 加载芯片参数 */
    nor_load_param(dev, param);

    /* 6. 进入高速模式（如Quad SPI） */
    if (dev->flags & NOR_FLAG_QUAD_SPI) {
        ret = nor_enable_quad_mode(dev);
        if (ret < 0) {
            return ret;
        }
    }

    /* 7. 进入4字节地址模式（如果支持且需要） */
    if ((dev->flags & NOR_FLAG_4BYTE_ADDR) && config->use_4byte_addr) {
        ret = nor_enter_4byte_mode(dev);
        if (ret < 0) {
            return ret;
        }
        dev->addr_bytes = 4;
    }

    /* 8. 清除写保护 */
    ret = nor_disable_write_protect(dev);
    if (ret < 0) {
        return ret;
    }

    /* 9. 初始化状态 */
    dev->state = NOR_STATE_IDLE;
    dev->write_in_progress = 0;

    return 0;
}

/**
 * 查找匹配的芯片参数
 */
static const nor_flash_param_t* nor_find_param(uint8_t mfg_id, uint16_t dev_id)
{
    for (int i = 0; nor_flash_table[i].manufacturer_id != 0xFF; i++) {
        const nor_flash_param_t *p = &nor_flash_table[i];
        if (p->manufacturer_id == mfg_id &&
            dev_id >= p->device_id_start &&
            dev_id <= p->device_id_end) {
            return p;
        }
    }
    return NULL;
}
```

### 4.2 读/写模块

读操作相对简单，是最基本的Flash操作。写操作（页编程）需要先检查目标区域是否已擦除，并处理跨页的情况。

```c
/**
 * 读取数据
 * @param dev 设备句柄
 * @param addr 起始地址
 * @param buf 接收缓冲区
 * @param len 读取长度
 * @return 实际读取长度
 */
int nor_read(nor_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    int ret;

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 等待之前的写操作完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送读命令（0x03或更快的命令） */
    nor_transport_cmd(dev->transport, NOR_CMD_READ);

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 4);
    } else {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 3);
    }

    /* 读取数据 */
    ret = nor_transport_recv(dev->transport, buf, len);

    return ret;
}

/**
 * 页编程（写入数据）
 * 注意：写入前目标区域必须为0xFF（已擦除状态）
 * @param dev 设备句柄
 * @param addr 页起始地址（必须对齐）
 * @param buf 数据缓冲区
 * @param len 写入长度（不能超过页大小）
 */
int nor_page_program(nor_device_t *dev, uint32_t addr,
                     const uint8_t *buf, uint32_t len)
{
    int ret;

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 页对齐检查（某些芯片允许跨页） */
    if ((addr % dev->page_size) + len > dev->page_size) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 等待之前的操作完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    /* 发送页编程命令 */
    nor_transport_cmd(dev->transport, NOR_CMD_PAGE_PROGRAM);

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 4);
    } else {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 3);
    }

    /* 发送数据 */
    nor_transport_send(dev->transport, buf, len);

    /* 等待写入完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_PAGE_PROGRAM);
    dev->write_in_progress = 0;

    return ret;
}

/**
 * 连续写入（自动处理跨页）
 */
int nor_write(nor_device_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t page_size = dev->page_size;
    uint32_t wrote = 0;

    while (wrote < len) {
        /* 计算当前页剩余空间 */
        uint32_t page_offset = addr % page_size;
        uint32_t chunk = page_size - page_offset;

        /* 限制为剩余数据量 */
        if (chunk > (len - wrote)) {
            chunk = len - wrote;
        }

        /* 页编程 */
        int ret = nor_page_program(dev, addr, buf + wrote, chunk);
        if (ret < 0) {
            return ret;
        }

        addr += chunk;
        wrote += chunk;
    }

    return (int)wrote;
}
```

### 4.3 擦除模块

擦除操作是Nor Flash中耗时最长的操作，通常需要数百毫秒到数秒。擦除操作以扇区（4KB）、块（32KB/64KB）或整片为单位进行。

```c
/**
 * 扇区擦除（4KB）
 * @param dev 设备句柄
 * @param addr 扇区地址（必须4KB对齐）
 */
int nor_erase_sector(nor_device_t *dev, uint32_t addr)
{
    int ret;

    /* 对齐检查 */
    if (addr % dev->sector_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 地址边界检查 */
    if (addr >= dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 等待之前的操作完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    /* 发送扇区擦除命令 */
    nor_transport_cmd(dev->transport, NOR_CMD_ERASE_SECTOR);

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 4);
    } else {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 3);
    }

    /* 触发进度回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_STARTED, 0);
    }

    /* 等待擦除完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_SECTOR_ERASE);
    dev->write_in_progress = 0;

    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_COMPLETED, 100);
    }

    return ret;
}

/**
 * 块擦除（32KB或64KB）
 */
int nor_erase_block(nor_device_t *dev, uint32_t addr, uint32_t block_size)
{
    int ret;
    uint8_t cmd;

    /* 选择块擦除命令 */
    if (block_size == 32 * 1024) {
        cmd = NOR_CMD_ERASE_BLOCK_32K;
    } else {
        cmd = NOR_CMD_ERASE_BLOCK_64K;
    }

    /* 对齐检查 */
    if (addr % block_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 等待之前的操作完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    /* 发送块擦除命令 */
    nor_transport_cmd(dev->transport, cmd);

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 4);
    } else {
        nor_transport_send(dev->transport, (uint8_t[]){
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        }, 3);
    }

    /* 等待擦除完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_BLOCK_ERASE);
    dev->write_in_progress = 0;

    return ret;
}

/**
 * 整片擦除
 */
int nor_erase_chip(nor_device_t *dev)
{
    int ret;

    /* 等待之前的操作完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    /* 发送整片擦除命令 */
    nor_transport_cmd(dev->transport, NOR_CMD_ERASE_CHIP);

    /* 触发进度回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_STARTED, 0);
    }

    /* 等待擦除完成（整片擦除可能需要数十秒） */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_CHIP_ERASE);
    dev->write_in_progress = 0;

    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_COMPLETED, 100);
    }

    return ret;
}

/**
 * 批量擦除（用于大范围数据擦除）
 * 自动选择最优的擦除粒度
 */
int nor_erase(nor_device_t *dev, uint32_t addr, uint32_t len)
{
    uint32_t sector_size = dev->sector_size;
    uint32_t block_size = dev->block_size;
    uint32_t erased = 0;

    /* 起始地址对齐到扇区 */
    uint32_t start = addr;
    uint32_t end = addr + len;

    /* 计算需要擦除的扇区数 */
    uint32_t sector_start = start / sector_size;
    uint32_t sector_end = (end + sector_size - 1) / sector_size;
    uint32_t total_sectors = sector_end - sector_start;

    /* 进度计数 */
    uint32_t completed = 0;

    while (erased < len) {
        /* 确定当前擦除块大小 */
        uint32_t current_addr = start + erased;
        uint32_t remaining = len - erased;

        /* 优先使用块擦除 */
        if (remaining >= block_size && (current_addr % block_size) == 0) {
            int ret = nor_erase_block(dev, current_addr, block_size);
            if (ret < 0) {
                return ret;
            }
            erased += block_size;
        } else {
            /* 使用扇区擦除 */
            uint32_t sector_addr = (current_addr / sector_size) * sector_size;
            int ret = nor_erase_sector(dev, sector_addr);
            if (ret < 0) {
                return ret;
            }
            erased += sector_size;
        }

        /* 触发进度回调 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((erased * 100) / len);
            dev->callbacks.progress(NOR_EVENT_ERASE_PROGRESS, progress);
        }
    }

    return 0;
}
```

### 4.4 状态管理模块

Nor Flash的写和擦除操作是异步的，需要通过状态寄存器轮询来判断操作是否完成。状态管理模块负责这些操作。

```c
/**
 * 状态寄存器位定义
 */
#define NOR_STATUS_WIP   (1 << 0)  // Write In Progress
#define NOR_STATUS_WEL   (1 << 1)  // Write Enable Latch
#define NOR_STATUS_BP0   (1 << 2)  // Block Protect 0
#define NOR_STATUS_BP1   (1 << 3)  // Block Protect 1
#define NOR_STATUS_BP2   (1 << 4)  // Block Protect 2
#define NOR_STATUS_TB    (1 << 5)  // Top/Bottom Protect
#define NOR_STATUS_SEC   (1 << 6)  // Sector Protect
#define NOR_STATUS_SRP0  (1 << 7)  // Status Register Protect 0

/* 状态寄存器2（某些芯片） */
#define NOR_STATUS2_QE   (1 << 1)  // Quad Enable

/**
 * 读取状态寄存器
 */
int nor_read_status(nor_device_t *dev, uint8_t *status)
{
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;
    return nor_transport_transfer(dev->transport, &cmd, status, 1);
}

/**
 * 读取状态寄存器2
 */
int nor_read_status2(nor_device_t *dev, uint8_t *status)
{
    uint8_t cmd = NOR_CMD_READ_STATUS_REG_2;
    return nor_transport_transfer(dev->transport, &cmd, status, 1);
}

/**
 * 等待Flash就绪（轮询WIP位）
 * @param dev 设备句柄
 * @param timeout 超时时间（毫秒）
 * @return 0=就绪, -ETIMEDOUT=超时
 */
int nor_wait_ready(nor_device_t *dev, uint32_t timeout_ms)
{
    uint8_t status;
    uint32_t start = nor_get_tick();

    do {
        int ret = nor_read_status(dev, &status);
        if (ret < 0) {
            return ret;
        }

        /* 检查WIP位 */
        if (!(status & NOR_STATUS_WIP)) {
            return 0;
        }

        /* 超时检查 */
        if (nor_get_tick() - start > timeout_ms) {
            return -NOR_ERR_TIMEOUT;
        }

        /* 调用空闲回调 */
        if (dev->callbacks.idle) {
            dev->callbacks.idle();
        }

    } while (1);
}

/**
 * 写使能
 */
int nor_write_enable(nor_device_t *dev)
{
    nor_transport_cmd(dev->transport, NOR_CMD_WRITE_ENABLE);

    /* 验证WEL位 */
    uint8_t status;
    int ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    if (!(status & NOR_STATUS_WEL)) {
        return -NOR_ERR_WRITE_ENABLE;
    }

    return 0;
}

/**
 * 写禁用
 */
int nor_write_disable(nor_device_t *dev)
{
    nor_transport_cmd(dev->transport, NOR_CMD_WRITE_DISABLE);

    /* 验证WEL位已清除 */
    uint8_t status;
    int ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    return (status & NOR_STATUS_WEL) ? -NOR_ERR : 0;
}

/**
 * 设备状态查询
 */
nor_state_t nor_get_state(nor_device_t *dev)
{
    return dev->state;
}

/**
 * 获取设备信息
 */
int nor_get_info(nor_device_t *dev, nor_device_info_t *info)
{
    if (!dev || !info) {
        return -NOR_ERR_INVALID_PARAM;
    }

    info->manufacturer_id = dev->manufacturer_id;
    info->device_id = dev->device_id;
    strncpy(info->chip_name, dev->chip_name, sizeof(info->chip_name) - 1);
    info->total_size = dev->total_size;
    info->sector_size = dev->sector_size;
    info->block_size = dev->block_size;
    info->page_size = dev->page_size;
    info->is_busy = dev->write_in_progress;

    return 0;
}
```

---

## 5. 回调机制与事件处理

Nor Flash的某些操作（如擦除）耗时较长，为了提供更好的用户体验和系统响应性，驱动框架引入了回调机制。

### 5.1 进度回调

进度回调用于报告长时间操作的执行进度，主要用于擦除操作。

```c
/**
 * 事件类型定义
 */
typedef enum {
    NOR_EVENT_NONE = 0,
    NOR_EVENT_READ_STARTED,
    NOR_EVENT_READ_COMPLETED,
    NOR_EVENT_WRITE_STARTED,
    NOR_EVENT_WRITE_PROGRESS,
    NOR_EVENT_WRITE_COMPLETED,
    NOR_EVENT_ERASE_STARTED,
    NOR_EVENT_ERASE_PROGRESS,
    NOR_EVENT_ERASE_COMPLETED,
    NOR_EVENT_ERROR,
    NOR_EVENT_TIMEOUT,
} nor_event_t;

/**
 * 进度回调函数类型
 * @param event 事件类型
 * @param progress 进度值（0-100）
 */
typedef void (*nor_progress_callback_t)(nor_event_t event, uint8_t progress);

/**
 * 错误回调函数类型
 * @param error_code 错误码
 * @param error_msg 错误描述
 */
typedef void (*nor_error_callback_t)(int error_code, const char *error_msg);

/**
 * 空闲回调函数类型
 * 在等待Flash就绪期间调用，可用于执行后台任务
 */
typedef void (*nor_idle_callback_t)(void);

/**
 * 回调函数结构体
 */
typedef struct {
    nor_progress_callback_t progress;   // 进度回调
    nor_error_callback_t error;         // 错误回调
    nor_idle_callback_t idle;         // 空闲回调（轮询期间）
} nor_callbacks_t;
```

### 5.2 错误回调

错误回调用于报告驱动运行过程中的错误情况，帮助上层应用进行错误处理和日志记录。

```c
/**
 * 错误码定义
 */
typedef enum {
    NOR_ERR_NONE = 0,
    NOR_ERR_INVALID_PARAM = -1,
    NOR_ERR_NO_DEVICE = -2,
    NOR_ERR_OUT_OF_RANGE = -3,
    NOR_ERR_ALIGNMENT = -4,
    NOR_ERR_TIMEOUT = -5,
    NOR_ERR_WRITE_ENABLE = -6,
    NOR_ERR_PROTECTED = -7,
    NOR_ERR_VERIFY = -8,
    NOR_ERR_TRANSPORT = -9,
    NOR_ERR_HARDWARE = -10,
} nor_error_t;

/**
 * 错误描述表
 */
static const char *nor_error_strings[] = {
    [0]                     = "No error",
    [-NOR_ERR_INVALID_PARAM] = "Invalid parameter",
    [-NOR_ERR_NO_DEVICE]    = "No device found",
    [-NOR_ERR_OUT_OF_RANGE] = "Address out of range",
    [-NOR_ERR_ALIGNMENT]    = "Alignment error",
    [-NOR_ERR_TIMEOUT]      = "Operation timeout",
    [-NOR_ERR_WRITE_ENABLE] = "Write enable failed",
    [-NOR_ERR_PROTECTED]    = "Flash is protected",
    [-NOR_ERR_VERIFY]       = "Verify failed",
    [-NOR_ERR_TRANSPORT]    = "Transport error",
    [-NOR_ERR_HARDWARE]     = "Hardware error",
};

/**
 * 获取错误描述
 */
const char* nor_error_string(int err)
{
    int index = -err;
    if (index < 0 || index >= (int)(sizeof(nor_error_strings) / sizeof(nor_error_strings[0]))) {
        return "Unknown error";
    }
    return nor_error_strings[index];
}

/**
 * 调用错误回调的内部函数
 */
static void nor_invoke_error(nor_device_t *dev, int error_code)
{
    if (dev->callbacks.error) {
        dev->callbacks.error(error_code, nor_error_string(error_code));
    }
}
```

### 5.3 事件类型定义

完整的事件类型定义用于区分不同的操作阶段和状态变化：

```c
/**
 * 完整事件类型和掩码
 */
#define NOR_EVENT_CATEGORY_MASK   0xF0
#define NOR_EVENT_CATEGORY_NONE   0x00
#define NOR_EVENT_CATEGORY_READ   0x10
#define NOR_EVENT_CATEGORY_WRITE  0x20
#define NOR_EVENT_CATEGORY_ERASE  0x30
#define NOR_EVENT_CATEGORY_ERROR  0x40

/* 事件详细定义 */
typedef enum {
    /* 空操作 */
    NOR_EVENT_IDLE = NOR_EVENT_CATEGORY_NONE,

    /* 读取事件 */
    NOR_EVENT_READ_START = NOR_EVENT_CATEGORY_READ | 0x01,
    NOR_EVENT_READ_PROGRESS,
    NOR_EVENT_READ_COMPLETE,
    NOR_EVENT_READ_ERROR,

    /* 写入事件 */
    NOR_EVENT_WRITE_START = NOR_EVENT_CATEGORY_WRITE | 0x01,
    NOR_EVENT_WRITE_PROGRESS,
    NOR_EVENT_WRITE_COMPLETE,
    NOR_EVENT_WRITE_ERROR,

    /* 擦除事件 */
    NOR_EVENT_ERASE_START = NOR_EVENT_CATEGORY_ERASE | 0x01,
    NOR_EVENT_ERASE_PROGRESS,
    NOR_EVENT_ERASE_COMPLETE,
    NOR_EVENT_ERASE_ERROR,

    /* 错误事件 */
    NOR_EVENT_ERROR_GENERAL = NOR_EVENT_CATEGORY_ERROR | 0x01,
    NOR_EVENT_ERROR_TIMEOUT,
    NOR_EVENT_ERROR_HARDWARE,
} nor_event_ex_t;

/**
 * 获取事件分类
 */
static inline nor_event_t nor_event_category(nor_event_t event)
{
    return (nor_event_t)(event & NOR_EVENT_CATEGORY_MASK);
}

/**
 * 判断是否为完成事件
 */
static inline int nor_event_is_complete(nor_event_t event)
{
    return (event == NOR_EVENT_READ_COMPLETED ||
            event == NOR_EVENT_WRITE_COMPLETED ||
            event == NOR_EVENT_ERASE_COMPLETED);
}
```

---

## 6. 驱动代码示例框架

本节提供完整的驱动头文件框架，展示如何组织驱动代码的结构。

### 6.1 头文件结构

```c
/**
 * @file nor_driver.h
 * @brief Nor Flash 驱动框架头文件
 * @version 1.0.0
 */

#ifndef __NOR_DRIVER_H
#define __NOR_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 头文件包含
 *============================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*============================================================================
 * 常量定义
 *============================================================================*/

/* 超时时间配置（毫秒） */
#define NOR_TIMEOUT_DEFAULT         100
#define NOR_TIMEOUT_PAGE_PROGRAM    300
#define NOR_TIMEOUT_SECTOR_ERASE     400
#define NOR_TIMEOUT_BLOCK_ERASE      1000
#define NOR_TIMEOUT_CHIP_ERASE       120000

/* 传输缓冲区大小 */
#define NOR_TRANSFER_BUFFER_SIZE    256

/*============================================================================
 * 类型定义
 *============================================================================*/

/* 前向声明 */
typedef struct nor_device nor_device_t;
typedef struct nor_transport nor_transport_t;
typedef struct nor_config nor_config_t;

/* 接口类型 */
typedef enum {
    NOR_IFACE_SPI,
    NOR_IFACE_DUAL_SPI,
    NOR_IFACE_QUAD_SPI,
    NOR_IFACE_OCTAL_SPI,
    NOR_IFACE_FSMC,
    NOR_IFACE_GPIO_SPI,
} nor_iface_type_t;

/* 设备状态 */
typedef enum {
    NOR_STATE_IDLE = 0,
    NOR_STATE_READING,
    NOR_STATE_WRITING,
    NOR_STATE_ERASING,
    NOR_STATE_SUSPENDED,
    NOR_STATE_ERROR,
} nor_state_t;

/* 错误码 */
typedef enum {
    NOR_OK = 0,
    NOR_ERR = -1,
    NOR_ERR_INVALID_PARAM = -2,
    NOR_ERR_NO_DEVICE = -3,
    NOR_ERR_OUT_OF_RANGE = -4,
    NOR_ERR_ALIGNMENT = -5,
    NOR_ERR_TIMEOUT = -6,
    NOR_ERR_WRITE_ENABLE = -7,
    NOR_ERR_PROTECTED = -8,
    NOR_ERR_VERIFY = -9,
    NOR_ERR_TRANSPORT = -10,
    NOR_ERR_HARDWARE = -11,
} nor_err_t;

/*============================================================================
 * 命令码定义
 *============================================================================*/

typedef struct {
    uint8_t read;                  // 读命令
    uint8_t fast_read;             // 快速读命令
    uint8_t page_program;          // 页编程命令
    uint8_t sector_erase;          // 扇区擦除命令
    uint8_t block_erase_32k;      // 32KB块擦除
    uint8_t block_erase_64k;      // 64KB块擦除
    uint8_t chip_erase;            // 整片擦除命令
    uint8_t write_enable;          // 写使能
    uint8_t write_disable;         // 写禁用
    uint8_t read_status;           // 读状态寄存器
    uint8_t write_status;          // 写状态寄存器
    uint8_t read_id;               // 读ID命令
} nor_cmd_t;

/* 标准SPI命令码 */
#define NOR_CMD_READ               0x03
#define NOR_CMD_FAST_READ          0x0B
#define NOR_CMD_PAGE_PROGRAM       0x02
#define NOR_CMD_SECTOR_ERASE        0x20
#define NOR_CMD_BLOCK_ERASE_32K    0x52
#define NOR_CMD_BLOCK_ERASE_64K    0xD8
#define NOR_CMD_CHIP_ERASE         0xC7
#define NOR_CMD_WRITE_ENABLE       0x06
#define NOR_CMD_WRITE_DISABLE      0x04
#define NOR_CMD_READ_STATUS_REG    0x05
#define NOR_CMD_WRITE_STATUS_REG   0x01
#define NOR_CMD_READ_ID            0x9F

/* Quad SPI命令 */
#define NOR_CMD_QUAD_PAGE_PROGRAM  0x32
#define NOR_CMD_FAST_READ_QUAD      0x6B

/* 状态寄存器位 */
#define NOR_STATUS_WIP              (1 << 0)   // Write In Progress
#define NOR_STATUS_WEL              (1 << 1)   // Write Enable Latch
#define NOR_STATUS_BP0              (1 << 2)   // Block Protect 0
#define NOR_STATUS_BP1              (1 << 3)   // Block Protect 1
#define NOR_STATUS_BP2              (1 << 4)   // Block Protect 2
#define NOR_STATUS_QE               (1 << 6)   // Quad Enable
#define NOR_STATUS_SRP0             (1 << 7)   // Status Register Protect

/*============================================================================
 * 事件与回调定义
 *============================================================================*/

/* 事件类型 */
typedef enum {
    NOR_EVENT_NONE,
    NOR_EVENT_READ_STARTED,
    NOR_EVENT_READ_COMPLETED,
    NOR_EVENT_WRITE_STARTED,
    NOR_EVENT_WRITE_PROGRESS,
    NOR_EVENT_WRITE_COMPLETED,
    NOR_EVENT_ERASE_STARTED,
    NOR_EVENT_ERASE_PROGRESS,
    NOR_EVENT_ERASE_COMPLETED,
    NOR_EVENT_ERROR,
    NOR_EVENT_TIMEOUT,
} nor_event_t;

/* 回调函数类型 */
typedef void (*nor_progress_callback_t)(nor_event_t event, uint8_t progress);
typedef void (*nor_error_callback_t)(int error_code, const char *error_msg);
typedef void (*nor_idle_callback_t)(void);

/* 回调函数结构体 */
typedef struct {
    nor_progress_callback_t progress;
    nor_error_callback_t error;
    nor_idle_callback_t idle;
} nor_callbacks_t;

/*============================================================================
 * 传输层接口定义
 *============================================================================*/

/* 传输接口操作函数集 */
typedef struct nor_transport_ops {
    int (*send)(void *handle, const uint8_t *tx_buf, uint32_t tx_len);
    int (*recv)(void *handle, uint8_t *rx_buf, uint32_t rx_len);
    int (*transfer)(void *handle, const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);
    void (*cs_select)(void *handle);
    void (*cs_deselect)(void *handle);
    int (*wait_idle)(void *handle, uint32_t timeout_ms);
} nor_transport_ops_t;

/* 传输接口结构体 */
typedef struct nor_transport {
    nor_iface_type_t type;
    void *handle;                          // 传输句柄（如SPI句柄）
    const nor_transport_ops_t *ops;        // 操作函数集
    void *priv;                            // 私有数据
} nor_transport_t;

/*============================================================================
 * 芯片参数定义
 *============================================================================*/

/* 芯片参数表项 */
typedef struct nor_flash_param {
    uint8_t  manufacturer_id;
    uint16_t device_id;
    const char *name;
    uint32_t total_size;
    uint32_t sector_size;
    uint32_t block_size;
    uint32_t page_size;
    uint8_t  addr_bytes;
    uint32_t flags;
    const nor_cmd_t *cmd;
} nor_flash_param_t;

/* 特性标志 */
#define NOR_FLAG_4BYTE_ADDR    (1 << 0)
#define NOR_FLAG_DUAL_SPI      (1 << 1)
#define NOR_FLAG_QUAD_SPI      (1 << 2)
#define NOR_FLAG_OCTAL_SPI     (1 << 3)
#define NOR_FLAG_HAS_SFDP      (1 << 4)
#define NOR_FLAG_HAS_RESET     (1 << 5)

/*============================================================================
 * 设备结构体定义
 *============================================================================*/

/* 设备信息结构体 */
typedef struct nor_device_info {
    uint8_t  manufacturer_id;
    uint16_t device_id;
    char     chip_name[32];
    uint32_t total_size;
    uint32_t sector_size;
    uint32_t block_size;
    uint32_t page_size;
    bool     is_busy;
} nor_device_info_t;

/* Nor Flash 设备完整结构体 */
typedef struct nor_device {
    /* 识别信息 */
    uint8_t  manufacturer_id;
    uint16_t device_id;
    char     chip_name[32];

    /* 容量参数 */
    uint32_t total_size;
    uint32_t sector_size;
    uint32_t block_size;
    uint32_t page_size;

    /* 接口配置 */
    nor_iface_type_t iface_type;
    uint8_t  addr_bytes;

    /* 传输接口 */
    nor_transport_t *transport;

    /* 命令集 */
    nor_cmd_t cmd;

    /* 特性标志 */
    uint32_t flags;

    /* 回调函数 */
    nor_callbacks_t callbacks;

    /* 运行时状态 */
    nor_state_t state;
    uint8_t  write_in_progress;

    /* 锁（用于线程安全） */
    void *lock;

    /* 私有数据 */
    void *priv;
} nor_device_t;

/*============================================================================
 * 配置结构体
 *============================================================================*/

typedef struct nor_config {
    nor_transport_t *transport;
    bool use_4byte_addr;
    bool auto_quad_enable;
    const nor_callbacks_t *callbacks;
    const nor_flash_param_t *custom_param;
    void *priv;
} nor_config_t;

/*============================================================================
 * API函数声明
 *============================================================================*/

/* 初始化与复位 */
int nor_init(nor_device_t *dev, const nor_config_t *config);
int nor_deinit(nor_device_t *dev);
int nor_reset(nor_device_t *dev);

/* 读取操作 */
int nor_read(nor_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);
int nor_read_id(nor_device_t *dev, uint8_t *mfg_id, uint16_t *dev_id);
int nor_read_status(nor_device_t *dev, uint8_t *status);
int nor_read_uid(nor_device_t *dev, uint8_t *uid, uint8_t *len);

/* 写入操作 */
int nor_write(nor_device_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);
int nor_page_program(nor_device_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

/* 擦除操作 */
int nor_erase_sector(nor_device_t *dev, uint32_t addr);
int nor_erase_block(nor_device_t *dev, uint32_t addr);
int nor_erase_chip(nor_device_t *dev);
int nor_erase(nor_device_t *dev, uint32_t addr, uint32_t len);

/* 状态操作 */
int nor_wait_ready(nor_device_t *dev, uint32_t timeout_ms);
int nor_write_enable(nor_device_t *dev);
int nor_write_disable(nor_device_t *dev);
nor_state_t nor_get_state(nor_device_t *dev);
int nor_get_info(nor_device_t *dev, nor_device_info_t *info);

/* 特殊功能 */
int nor_enter_4byte_mode(nor_device_t *dev);
int nor_exit_4byte_mode(nor_device_t *dev);
int nor_enable_quad_mode(nor_device_t *dev);
int nor_disable_quad_mode(nor_device_t *dev);

/* 错误处理 */
const char* nor_error_string(int err);

#ifdef __cplusplus
}
#endif

#endif /* __NOR_DRIVER_H */
```

### 6.2 设备结构体定义

以下是完整的设备结构体定义示例，展示了驱动内部数据的管理方式：

```c
/**
 * 内部使用的设备扩展结构体
 */
typedef struct nor_device_ext {
    /* 传输层 */
    nor_transport_t *transport;

    /* 传输锁（用于多线程保护） */
    void *bus_lock;

    /* DMA支持（可选） */
    bool dma_enabled;
    void *dma_handle;

    /* 缓冲区 */
    uint8_t tx_buffer[32];
    uint8_t rx_buffer[32];

    /* 计时 */
    uint32_t tick_start;
    uint32_t tick_timeout;

    /* 错误计数 */
    uint32_t error_count;
    uint32_t last_error;
} nor_device_ext_t;

/**
 * 设备动态分配函数
 */
nor_device_t* nor_device_create(void)
{
    nor_device_t *dev = (nor_device_t *)malloc(sizeof(nor_device_t));
    if (dev) {
        memset(dev, 0, sizeof(nor_device_t));
        dev->priv = malloc(sizeof(nor_device_ext_t));
        if (dev->priv) {
            memset(dev->priv, 0, sizeof(nor_device_ext_t));
        }
    }
    return dev;
}

/**
 * 设备销毁函数
 */
void nor_device_destroy(nor_device_t *dev)
{
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
        }
        free(dev);
    }
}
```

### 6.3 接口函数声明

```c
/*============================================================================
 * 驱动层API（供应用层调用）
 *============================================================================*/

/**
 * @brief 初始化Nor Flash设备
 * @param dev 设备句柄
 * @param config 初始化配置
 * @return 0 success, 负值表示错误
 */
int nor_init(nor_device_t *dev, const nor_config_t *config);

/**
 * @brief 读取数据
 * @param dev 设备句柄
 * @param addr 起始地址
 * @param buf 接收缓冲区
 * @param len 读取长度
 * @return 实际读取长度，负值表示错误
 */
int nor_read(nor_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief 写入数据（自动处理跨页）
 * @param dev 设备句柄
 * @param addr 起始地址
 * @param buf 数据缓冲区
 * @param len 写入长度
 * @return 实际写入长度，负值表示错误
 */
int nor_write(nor_device_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief 擦除指定区域
 * @param dev 设备句柄
 * @param addr 起始地址
 * @param len 擦除长度
 * @return 0 success, 负值表示错误
 */
int nor_erase(nor_device_t *dev, uint32_t addr, uint32_t len);

/**
 * @brief 读取芯片ID
 */
int nor_read_id(nor_device_t *dev, uint8_t *mfg_id, uint16_t *dev_id);

/**
 * @brief 获取设备信息
 */
int nor_get_info(nor_device_t *dev, nor_device_info_t *info);

/*============================================================================
 * 传输层API（供驱动层内部使用）
 *============================================================================*/

/**
 * @brief 发送数据
 */
int nor_transport_send(nor_transport_t *t, const uint8_t *buf, uint32_t len);

/**
 * @brief 接收数据
 */
int nor_transport_recv(nor_transport_t *t, uint8_t *buf, uint32_t len);

/**
 * @brief 同时发送和接收
 */
int nor_transport_transfer(nor_transport_t *t, const uint8_t *tx_buf,
                           uint8_t *rx_buf, uint32_t len);

/**
 * @brief 发送命令
 */
static inline int nor_transport_cmd(nor_transport_t *t, uint8_t cmd)
{
    return t->ops->send(t->handle, &cmd, 1);
}
```

---

## 本章小结

本章详细介绍了Nor Flash驱动框架的设计方案，主要内容包括：

1. **驱动架构概述**
   - 采用分层架构设计（应用层、驱动层、传输层、硬件层）
   - 遵循接口抽象、配置驱动、状态机驱动等设计原则
   - 实现硬件解耦和代码复用

2. **设备抽象层（DAL）设计**
   - 定义了完整的设备描述结构体nor_device_t
   - 提供了通用设备接口nor_ops_t
   - 通过芯片参数表实现多芯片支持

3. **传输层抽象**
   - 统一nor_transport_t接口定义
   - 支持SPI、GPIO模拟SPI、FSMC/FMC并行等多种传输方式
   - 实现了不同接口的透明切换

4. **核心驱动模块**
   - 初始化模块：芯片检测、参数加载、模式配置
   - 读/写模块：支持跨页连续写入
   - 擦除模块：支持扇区、块、整片擦除
   - 状态管理模块：状态轮询、写保护控制

5. **回调机制**
   - 进度回调：报告长时间操作进度
   - 错误回调：统一错误处理
   - 事件类型：完整的生命周期事件定义

6. **驱动代码示例框架**
   - 完整的头文件结构
   - 设备结构体定义
   - API函数声明

通过本章的学习，开发者可以理解Nor Flash驱动框架的整体设计思路，为后续的具体驱动实现和上层应用开发奠定基础。

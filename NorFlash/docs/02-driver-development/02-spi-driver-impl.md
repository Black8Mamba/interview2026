# SPI Flash 驱动实现详解

SPI Flash 驱动是在驱动框架基础上，针对SPI接口Nor Flash芯片的具体实现。本章详细介绍SPI配置的关键技术细节、各种读/写/擦除操作的实现原理与代码示例，以及状态轮询机制。掌握这些内容，开发者能够根据具体的硬件平台和芯片型号，实现稳定高效的SPI Flash驱动。

---

## 1. SPI Flash 驱动概述

### 1.1 驱动设计目标

SPI Flash驱动的设计目标是提供一个与硬件平台无关、与芯片型号解耦的通用驱动实现。具体而言，驱动需要满足以下核心需求：

**兼容性**：驱动必须能够支持主流厂商的SPI Flash芯片，包括Winbond、GigaDevice、Macronix、Micron等厂商的产品。不同厂商的芯片在命令码、时序参数上存在细微差异，驱动需要通过参数表和灵活的配置机制来适应这些差异。

**可移植性**：驱动代码应与具体的硬件平台（STM32、ESP32、MM32等）和外设（硬件SPI、GPIO模拟SPI）解耦。传输层的抽象设计使得更换硬件平台时，只需实现相应的传输接口，而无需修改上层的Flash操作逻辑。

**性能优化**：SPI Flash的读写速度受SPI时钟频率和数据传输模式影响。驱动应支持多种读取模式（标准读取、快速读取、双线读取、四线读取）和DMA传输，以满足不同场景的性能需求。

**易用性**：驱动应提供简洁的上层API，隐藏底层实现细节。上层应用只需调用读、写、擦除等简单接口，无需关心命令序列、状态轮询等复杂流程。

### 1.2 支持的芯片型号

当前驱动支持的主流SPI Flash芯片型号如下表所示。这些芯片占据了市场的主要份额，具有代表性意义：

| 厂商 | 芯片型号 | 容量 | 特性 | 页大小 | 扇区大小 |
|------|----------|------|------|--------|----------|
| Winbond | W25Q256JV | 256Mbit | Quad SPI, 4字节地址 | 256B | 4KB |
| Winbond | W25Q128JV | 128Mbit | Quad SPI | 256B | 4KB |
| Winbond | W25Q64JV | 64Mbit | Quad SPI | 256B | 4KB |
| GigaDevice | GD25Q256C | 256Mbit | Quad SPI, 4字节地址 | 256B | 4KB |
| GigaDevice | GD25Q128C | 128Mbit | Quad SPI | 256B | 4KB |
| Macronix | MX25L25645G | 256Mbit | Quad SPI, 4字节地址 | 256B | 4KB |
| Macronix | MX25L12845G | 128Mbit | Quad SPI | 256B | 4KB |
| Micron | N25Q256A | 256Mbit | Quad SPI, 4字节地址 | 256B | 4KB |

对于其他型号的芯片，只要它们兼容常见的SPI Flash命令集（ 如JEDEC标准），驱动也能提供基本支持。开发者可以通过扩展芯片参数表来添加新芯片的支持。

---

## 2. SPI 配置详解

### 2.1 SPI 模式配置（Mode 0）

SPI Flash通常使用模式0（CPOL=0, CPHA=0），这是最常用的SPI工作模式。在模式0中，时钟空闲状态为低电平，数据在时钟的上升沿被采样。

```
            空闲    上升沿      下降沿      上升沿
    SCK  ────────┐       ┌───────┐       ┌───────┐
              ──┘       └───┘       └───┘
                 ↑       │   ↑       │
                 │       │   │       │
    MOSI  ────[D7]──────[D6]───────[D5]───────
                 │       │   │       │
                 │       ▼   │       ▼
                 │      采样  │      采样
```

在模式0中，SCK引脚在空闲时保持低电平。数据的MSB首先被放到MOSI线上，然后SCK产生一个上升沿，Flash在这个上升沿锁存数据。随后SCK产生下降沿，准备发送下一位数据。

```
/**
 * SPI模式配置参数
 */
typedef struct {
    uint32_t clock_polarity;  // CPOL: 0=低电平空闲, 1=高电平空闲
    uint32_t clock_phase;     // CPHA: 0=第一个边沿采样, 1=第二个边沿采样
    uint32_t clock_freq;      // SPI时钟频率
    uint32_t data_width;      // 数据位宽（通常为8位）
    uint32_t first_bit;        // 首位: SPI_FIRSTBIT_MSB/SPI_FIRSTBIT_LSB
} spi_mode_config_t;

/**
 * SPI Flash标准模式配置（Mode 0, CPOL=0, CPHA=0）
 */
static const spi_mode_config_t spi_flash_mode_0 = {
    .clock_polarity = SPI_POLARITY_LOW,    // 空闲时钟为低
    .clock_phase    = SPI_PHASE_1EDGE,      // 第一个边沿采样
    .clock_freq     = SPI_FLASH_MAX_FREQ,   // 最大频率（根据芯片和PCB设计）
    .data_width     = 8,                     // 8位数据宽度
    .first_bit      = SPI_FIRSTBIT_MSB,     // MSB优先
};

/**
 * 使用HAL库配置SPI为Flash模式
 */
int spi_flash_config(SPI_HandleTypeDef *hspi)
{
    hspi->Instance = SPI1;
    hspi->Init.Mode = SPI_MODE_MASTER;           // 主机模式
    hspi->Init.Direction = SPI_DIRECTION_2LINES; // 全双工
    hspi->Init.DataSize = SPI_DATASIZE_8BIT;      // 8位数据
    hspi->Init.CLKPolarity = SPI_POLARITY_LOW;    // CPOL=0
    hspi->Init.CLKPhase = SPI_PHASE_1EDGE;        // CPHA=0
    hspi->Init.NSS = SPI_NSS_SOFT;                // 软件片选
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; // 分频系数
    hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;       // MSB优先

    if (HAL_SPI_Init(hspi) != HAL_OK) {
        return -1;
    }

    __HAL_SPI_ENABLE(hspi);
    return 0;
}
```

### 2.2 时钟频率选择

SPI Flash的时钟频率受多个因素制约，包括芯片最高支持频率、MCU的SPI外设能力、以及PCB布线的电气特性。常见的时钟频率选择如下表所示：

| 芯片类型 | 最高频率 | 推荐频率 | 说明 |
|----------|----------|----------|------|
| 标准SPI Flash | 104MHz | 50-80MHz | 单线模式下的最高频率 |
| Dual SPI Flash | 104MHz x 2 | 50-80MHz | 双线模式下有效带宽翻倍 |
| Quad SPI Flash | 104MHz x 4 | 50-80MHz | 四线模式下有效带宽四倍 |
| QPI Flash | 133MHz | 80-100MHz | 四线命令+数据模式 |

在实际应用中，需要根据硬件条件选择合适的频率。对于PCB布线较长或信号完整性较差的情况，应适当降低时钟频率以确保数据传输的可靠性。

```
/**
 * SPI时钟频率配置
 */
typedef enum {
    SPI_SPEED_LOW = 0,       // 低速: 1-5MHz
    SPI_SPEED_MEDIUM,        // 中速: 10-25MHz
    SPI_SPEED_HIGH,          // 高速: 50-80MHz
    SPI_SPEED_VERY_HIGH,    // 极高频: 100MHz+
} spi_speed_level_t;

/**
 * 根据速度等级获取分频系数（基于SPI总线频率）
 */
static uint32_t spi_get_prescaler(spi_speed_level_t speed)
{
    uint32_t prescaler_table[] = {
        [SPI_SPEED_LOW]       = SPI_BAUDRATEPRESCALER_256,  // 168MHz/256 ~ 656KHz
        [SPI_SPEED_MEDIUM]    = SPI_BAUDRATEPRESCALER_8,    // 168MHz/8  ~ 21MHz
        [SPI_SPEED_HIGH]      = SPI_BAUDRATEPRESCALER_2,    // 168MHz/2  ~ 84MHz
        [SPI_SPEED_VERY_HIGH] = SPI_BAUDRATEPRESCALER_2,    // 需要硬件支持
    };
    return prescaler_table[speed];
}

/**
 * 配置SPI时钟频率
 */
int spi_set_speed(SPI_HandleTypeDef *hspi, spi_speed_level_t speed)
{
    MODIFY_REG(hspi->Instance->CR1,
               SPI_CR1_BR_Msk,
               spi_get_prescaler(speed));
    return 0;
}
```

### 2.3 位宽配置（Single/Dual/Quad）

SPI Flash支持多种数据位宽模式，从单线（Standard SPI）到四线（Quad SPI），数据传输效率逐级提升。

**Single SPI（标准SPI）**：使用一根MOSI发送数据，一根MISO接收数据。这是最基本的SPI模式，兼容所有SPI Flash芯片。

**Dual SPI（双线SPI）**：MOSI和MISO两根线同时用于数据传输。在发送命令和地址时仍然使用单线模式，但数据传输阶段两根线同时收发数据，有效带宽翻倍。

**Quad SPI（四线SPI）**：使用四根数据线（IO0、IO1、IO2、IO3）同时传输数据。这需要芯片支持Quad模式，并且通常需要将IO2和IO3引脚配置为功能模式。Quad模式的启用需要通过状态寄存器的QE位（Quad Enable）来控制。

```
/**
 * SPI位宽模式定义
 */
typedef enum {
    SPI_BIT_WIDTH_SINGLE = 1,  // 单线模式
    SPI_BIT_WIDTH_DUAL = 2,    // 双线模式
    SPI_BIT_WIDTH_QUAD = 4,    // 四线模式
} spi_bit_width_t;

/**
 * Quad SPI模式使能
 */
int spi_enable_quad_mode(nor_device_t *dev)
{
    int ret;
    uint8_t status;

    /* 读取当前状态寄存器 */
    ret = nor_read_status(dev, &status);
    if (ret < 0) return ret;

    /* 检查QE位是否已设置 */
    if (status & NOR_STATUS_QE) {
        return 0;  // 已经使能
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) return ret;

    /* 写状态寄存器，使能Quad模式 */
    status |= NOR_STATUS_QE;
    uint8_t cmd_buf[2] = { NOR_CMD_WRITE_STATUS_REG, status };
    ret = nor_transport_send(dev->transport, cmd_buf, 2);
    if (ret < 0) return ret;

    /* 等待写完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);

    return ret;
}

/**
 * 切换SPI位宽模式
 */
int spi_set_bit_width(nor_device_t *dev, spi_bit_width_t width)
{
    switch (width) {
        case SPI_BIT_WIDTH_SINGLE:
            dev->iface_type = NOR_IFACE_SPI;
            break;
        case SPI_BIT_WIDTH_DUAL:
            if (!(dev->flags & NOR_FLAG_DUAL_SPI)) {
                return -NOR_ERR_NOT_SUPPORTED;
            }
            dev->iface_type = NOR_IFACE_DUAL_SPI;
            break;
        case SPI_BIT_WIDTH_QUAD:
            if (!(dev->flags & NOR_FLAG_QUAD_SPI)) {
                return -NOR_ERR_NOT_SUPPORTED;
            }
            dev->iface_type = NOR_IFACE_QUAD_SPI;
            /* 使能Quad模式 */
            return spi_enable_quad_mode(dev);
        default:
            return -NOR_ERR_INVALID_PARAM;
    }
    return 0;
}
```

### 2.4 HAL 库配置示例

以下是针对STM32 HAL库的完整SPI配置示例，包括GPIO初始化和SPI外设配置：

```
/**
 * SPI Flash硬件配置
 */
typedef struct {
    SPI_HandleTypeDef *hspi;       // SPI句柄
    GPIO_TypeDef *cs_port;          // CS引脚端口
    uint16_t cs_pin;                // CS引脚编号
    GPIO_TypeDef *wp_port;          // WP引脚端口
    uint16_t wp_pin;                // WP引脚编号
    GPIO_TypeDef *hold_port;        // HOLD引脚端口
    uint16_t hold_pin;              // HOLD引脚编号
} spi_flash_hw_config_t;

/**
 * SPI Flash引脚配置
 */
static void spi_flash_gpio_init(spi_flash_hw_config_t *hw)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能GPIO时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* SCK引脚配置（PA5） */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* MISO引脚配置（PA6） */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* MOSI引脚配置（PA7） */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CS引脚配置（PA4）- 软件控制 */
    GPIO_InitStruct.Pin = hw->cs_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(hw->cs_port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(hw->cs_port, hw->cs_pin, GPIO_PIN_SET);

    /* WP引脚配置 - 禁用写保护 */
    GPIO_InitStruct.Pin = hw->wp_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(hw->wp_port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(hw->wp_port, hw->wp_pin, GPIO_PIN_SET);

    /* HOLD引脚配置 - 禁用HOLD */
    GPIO_InitStruct.Pin = hw->hold_pin;
    HAL_GPIO_Init(hw->hold_port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(hw->hold_port, hw->hold_pin, GPIO_PIN_SET);
}

/**
 * SPI外设初始化
 */
static void spi_flash_spi_init(SPI_HandleTypeDef *hspi)
{
    hspi->Instance = SPI1;
    hspi->Init.Mode = SPI_MODE_MASTER;
    hspi->Init.Direction = SPI_DIRECTION_2LINES;
    hspi->Init.DataSize = SPI_DATASIZE_8BIT;
    hspi->Init.CLKPolarity = SPI_POLARITY_LOW;    // Mode 0
    hspi->Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi->Init.NSS = SPI_NSS_SOFT;
    hspi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4; // 42MHz/4 = 10.5MHz
    hspi->Init.FirstBit = SPI_FIRSTBIT_MSB;

    if (HAL_SPI_Init(hspi) != HAL_OK) {
        Error_Handler();
    }

    /* 使能SPI */
    __HAL_SPI_ENABLE(hspi);
}

/**
 * SPI Flash完整初始化
 */
int spi_flash_hw_init(spi_flash_hw_config_t *hw)
{
    /* 初始化GPIO */
    spi_flash_gpio_init(hw);

    /* 初始化SPI外设 */
    spi_flash_spi_init(hw->hspi);

    return 0;
}
```

---

## 3. 读操作实现

SPI Flash的读操作是将数据从Flash芯片传输到MCU的过程。根据数据传输模式的不同，可以分为标准读取、快速读取、双线读取和四线读取等多种方式。不同的读取方式在命令序列、数据传输效率和适用场景上有所差异。

### 3.1 标准读取（0x03）

标准读取命令（0x03）是最基本也是兼容性最好的读取方式。它使用单线SPI模式，命令序列包括命令码、地址和dummy字节，之后Flash开始输出数据。

```
命令格式：
    +--------+--------+----------+----------+----------+
    | CMD    | A23-A16| A15-A8   | A7-A0    | DOUT     |
    +--------+--------+----------+----------+----------+
    | 0x03   | Address[23:16] | Address[15:8]  | Address[7:0] | Data...   |
    +--------+--------+----------+----------+----------+

时序图：
    CS# ─────┐                                ┌──────
              │                                │
    SCK  ────┘└────────┐            ┌─────────┘└────────
                       │            │
              ┌────────┴───┐       │
              │ CMD=0x03   │       │
              └────────────┘       │
                        ┌──────────┴─────┐
              Address   │ 24-bit Address │
                        └────────────────┘
                                    ┌──────┐   ┌──────┐
              DOUT                  │ D0   │...│ DN   │
                                    └──────┘   └──────┘
```

标准读取的特点是无需dummy字节，芯片从第一个字节开始就输出数据。这意味着它可以在较低的SPI时钟下稳定工作，具有最好的兼容性。

```c
/**
 * SPI Flash 标准读取（命令0x03）
 * @param dev 设备句柄
 * @param addr 读取地址
 * @param buf 接收缓冲区
 * @param len 读取长度
 * @return 实际读取长度
 */
int nor_flash_read_standard(nor_device_t *dev, uint32_t addr,
                           uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_buf[4];

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 等待Flash就绪 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送读命令和地址 */
    cmd_buf[0] = NOR_CMD_READ;  // 0x03
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 读取数据 */
    ret = dev->transport->ops->recv(dev->transport->handle, buf, len);

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    return ret;
}
```

### 3.2 快速读取（0x0B）

快速读取命令（0x0B）与标准读取类似，但需要8个dummy周期后才能开始输出数据。这个dummy周期用于Flash内部准备数据，使得在更高的时钟频率下也能正确读取。

```
命令格式：
    +--------+--------+----------+----------+------+----------+
    | CMD    | A23-A16| A15-A8   | A7-A0    | DUMMY| DOUT     |
    +--------+--------+----------+----------+------+----------+
    | 0x0B   | Address[23:16] | Address[15:8]  | Address[7:0] | 8 bits  | Data...   |
    +--------+--------+----------+----------+------+----------+

时序图：
    CS# ─────┐                                              ┌──────
              │                                              │
    SCK  ────┘└────────┐         ┌─────────┘└───────────────┘
                       │         │
              ┌────────┴───┐    ┌─┴────┐
              │ CMD=0x0B   │    │Dummy │
              └────────────┘    └──────┘
                        ┌───────┴────────┐
              Address   │ 24-bit Address │
                        └────────────────┘
                                         ┌──────┐   ┌──────┐
              DOUT                       │ D0   │...│ DN   │
                                         └──────┘   └──────┘
```

快速读取命令支持更高的时钟频率，但需要确保MCU的SPI外设能够正确处理8个dummy周期。

```c
/**
 * SPI Flash 快速读取（命令0x0B）
 * 支持更高的时钟频率
 */
int nor_flash_read_fast(nor_device_t *dev, uint32_t addr,
                       uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_buf[5];
    uint8_t dummy = 0xFF;

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 等待Flash就绪 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送快速读命令 */
    cmd_buf[0] = NOR_CMD_FAST_READ;  // 0x0B

    /* 根据地址模式发送3或4字节地址 */
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);

        /* 发送dummy字节（8个时钟周期） */
        dev->transport->ops->send(dev->transport->handle, &dummy, 1);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);

        /* 发送dummy字节 */
        dev->transport->ops->send(dev->transport->handle, &dummy, 1);
    }

    /* 读取数据 */
    ret = dev->transport->ops->recv(dev->transport->handle, buf, len);

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    return ret;
}
```

### 3.3 双线读取（0x3B）

双线读取命令（0x3B）利用Dual SPI模式，在数据输出阶段同时使用MOSI和MISO两根线传输数据，理论带宽是标准读取的两倍。

```
命令格式：
    +--------+----------+----------+------+----------+
    | CMD    | A23-A16  | A15-A8   | A7-A0| DUMMY|DOUT     |
    +--------+----------+----------+------+------+----------+
    | 0x3B   | Address  | Address  | Addr | 8bit | 2-line  |
    +--------+----------+----------+------+------+----------+

时序图：
    CS# ─────┐                                           ┌──────
              │                                           │
    SCK  ────┘└────────┐      ┌─────────┘└───────────────┘
                       │      │
              ┌────────┴───┐ ┌─┴────┐
              │ CMD=0x3B   │ │Dummy │
              └────────────┘ └──────┘
                        ┌───────┴────────┐
              Address   │ 24-bit Address │
                        └────────────────┘
                    ┌───────────────────────┐
    IO0/IO1         │ D0  D2 D4 D6 ...     │  ← 双线同时输出
                    │ D1  D3 D5 D7 ...     │
                    └───────────────────────┘
```

双线读取需要芯片支持Dual SPI模式，并且命令和地址阶段仍然使用单线传输。

```c
/**
 * SPI Flash 双线读取（命令0x3B）
 * 使用Dual SPI模式，数据线同时传输
 */
int nor_flash_read_dual(nor_device_t *dev, uint32_t addr,
                       uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_buf[4];
    uint8_t dummy = 0xFF;

    /* 检查是否支持Dual SPI */
    if (!(dev->flags & NOR_FLAG_DUAL_SPI)) {
        return -NOR_ERR_NOT_SUPPORTED;
    }

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 等待Flash就绪 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送命令和地址（单线模式） */
    cmd_buf[0] = NOR_CMD_DUAL_READ;  // 0x3B
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 发送dummy字节 */
    dev->transport->ops->send(dev->transport->handle, &dummy, 1);

    /* 切换到Dual SPI模式并读取数据 */
    if (dev->transport->ops->dual_recv) {
        ret = dev->transport->ops->dual_recv(dev->transport->handle, buf, len);
    } else {
        /* 如果传输层不支持dual，使用模拟方式 */
        ret = nor_flash_dual_read_simulate(dev, buf, len);
    }

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    return ret;
}

/**
 * Dual SPI模拟读取（当硬件不支持时使用）
 */
static int nor_flash_dual_read_simulate(nor_device_t *dev, uint8_t *buf, uint32_t len)
{
    uint8_t cmd;
    uint32_t i;

    /* 每次读取2字节 */
    for (i = 0; i < len - 1; i += 2) {
        /* 发送dummy以产生时钟 */
        cmd = 0xFF;
        dev->transport->ops->transfer(dev->transport->handle, &cmd, &buf[i], 2);
    }

    /* 处理奇数长度 */
    if (i < len) {
        cmd = 0xFF;
        dev->transport->ops->transfer(dev->transport->handle, &cmd, &buf[i], 1);
    }

    return (int)len;
}
```

### 3.4 四线读取（0x6B）

四线读取命令（0x6B）使用Quad SPI模式，在数据输出阶段同时使用四根数据线（IO0-IO3）传输数据，理论带宽是标准读取的四倍。

```
命令格式：
    +--------+----------+----------+------+------+----------+
    | CMD    | A23-A16  | A15-A8   | A7-A0| DUMMY| DOUT     |
    +--------+----------+----------+------+------+----------+
    | 0x6B   | Address  | Address  | Addr | 8bit | 4-line   |
    +--------+----------+----------+------+------+----------+

时序图：
    CS# ─────┐                                              ┌──────
              │                                              │
    SCK  ────┘└────────┐        ┌─────────┘└───────────────┘
                       │        │
              ┌────────┴───┐ ┌──┴───┐
              │ CMD=0x6B   │ │Dummy │
              └────────────┘ └──────┘
                        ┌───────┴────────┐
              Address   │ 24-bit Address │
                        └────────────────┘
              IO0 ─────── D0  D4 ... ───────────────────
              IO1 ─────── D1  D5 ... ───────────────────
              IO2 ─────── D2  D6 ... ───────────────────
              IO3 ─────── D3  D7 ... ───────────────────
```

```c
/**
 * SPI Flash 四线读取（命令0x6B）
 * 使用Quad SPI模式，四根数据线同时传输
 */
int nor_flash_read_quad(nor_device_t *dev, uint32_t addr,
                       uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_buf[4];
    uint8_t dummy = 0xFF;

    /* 检查是否支持Quad SPI */
    if (!(dev->flags & NOR_FLAG_QUAD_SPI)) {
        return -NOR_ERR_NOT_SUPPORTED;
    }

    /* 确认Quad模式已使能 */
    ret = nor_check_quad_enabled(dev);
    if (ret < 0) {
        return ret;
    }

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 等待Flash就绪 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送命令和地址 */
    cmd_buf[0] = NOR_CMD_QUAD_READ;  // 0x6B
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 发送dummy字节 */
    dev->transport->ops->send(dev->transport->handle, &dummy, 1);

    /* 切换到Quad SPI模式并读取数据 */
    if (dev->transport->ops->quad_recv) {
        ret = dev->transport->ops->quad_recv(dev->transport->handle, buf, len);
    } else {
        ret = -NOR_ERR_NOT_SUPPORTED;
    }

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    return ret;
}
```

### 3.5 四线IO读取（0xEB）

四线IO读取（0xEB）是最先进的读取模式，也称为QPI（Quad Peripheral Interface）模式。在这种模式下，命令、地址和数据的传输全部使用四根数据线，实现了最高的数据传输效率。

```
命令格式：
    +------+------+------+------+------+------+------+------+
    | CMD  | A23-A16| A15-A8| A7-A0| DUMMY| DUMMY| DATA    |
    +------+------+------+------+------+------+------+
    | 0xEB | IO0-IO3 | IO0-IO3| IO0-IO3| IO0-IO3| IO0-IO3| IO0-IO3|
    +------+------+------+------+------+------+------+
      4-line  4-line   4-line   4-line   4-line   4-line

时序图：
    CS# ─────┐                                                     ┌──────
              │                                                     │
    SCK  ────┘└────────┐         ┌─────────┘└───────────┘
                       │         │
              ┌────────┴───┐ ┌───┴────┐
              │ CMD=0xEB   │ │ 8 dummy│
              └────────────┘ └────────┘
                        ┌───────┴────────┐
              Address   │ 24-bit Address │
              IO0-IO3   │ (4-bit mode)   │
                        └────────────────┘
                        ┌────────────────┐
              DATA      │ 4-line output  │
              IO0-IO3   │                │
                        └────────────────┘
```

```c
/**
 * SPI Flash 四线IO读取（命令0xEB）
 * 命令、地址、数据全部使用四线传输
 */
int nor_flash_read_quad_io(nor_device_t *dev, uint32_t addr,
                          uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_addr[5];
    uint8_t dummy[2] = {0xFF, 0xFF};

    /* 检查是否支持Quad SPI */
    if (!(dev->flags & NOR_FLAG_QUAD_SPI)) {
        return -NOR_ERR_NOT_SUPPORTED;
    }

    /* 进入QPI模式（如果需要） */
    ret = nor_enter_qpi_mode(dev);
    if (ret < 0) {
        return ret;
    }

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 等待Flash就绪 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送命令和地址（四线模式） */
    cmd_addr[0] = NOR_CMD_QUAD_READ_IO;  // 0xEB
    if (dev->addr_bytes == 4) {
        cmd_addr[1] = (addr >> 24) & 0xFF;
        cmd_addr[2] = (addr >> 16) & 0xFF;
        cmd_addr[3] = (addr >> 8) & 0xFF;
        cmd_addr[4] = addr & 0xFF;
        dev->transport->ops->quad_send(dev->transport->handle, cmd_addr, 5);
    } else {
        cmd_addr[1] = (addr >> 16) & 0xFF;
        cmd_addr[2] = (addr >> 8) & 0xFF;
        cmd_addr[3] = addr & 0xFF;
        dev->transport->ops->quad_send(dev->transport->handle, cmd_addr, 4);
    }

    /* 发送dummy字节（6-8个时钟周期） */
    dev->transport->ops->quad_send(dev->transport->handle, dummy, 2);

    /* 使用Quad模式接收数据 */
    if (dev->transport->ops->quad_recv) {
        ret = dev->transport->ops->quad_recv(dev->transport->handle, buf, len);
    } else {
        ret = -NOR_ERR_NOT_SUPPORTED;
    }

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    /* 退出QPI模式（可选） */
    // nor_exit_qpi_mode(dev);

    return ret;
}

/**
 * 进入QPI模式
 */
int nor_enter_qpi_mode(nor_device_t *dev)
{
    uint8_t cmd = 0x35;  // Enter QPI mode command
    return dev->transport->ops->send(dev->transport->handle, &cmd, 1);
}
```

### 3.6 连续读取模式

连续读取模式（Continuous Read Mode）是某些Flash芯片提供的特性，允许在一次读取操作后保持Flash处于连续读取状态，无需重新发送命令，即可直接读取下一个地址的数据。

```
连续读取时序：
    第一次读取：
        CS# ─────┐                                    ┌──────
                  │                                    │
        CMD=0x0B  └──┐                                │
        24-bit Addr └──┐                              │
        Dummy        └──┴────────────────────────────┘
        DATA0        ────────────────[D0...Dn]────────

    连续读取（无需命令）：
        CS# ─────┐                                           ┌────
                  │                                           |
        24-bit Addr (直接发送地址)  ───────────────────────────
        DATA0         ────────────────[D0...Dn]───────────────
```

```c
/**
 * 连续读取模式配置
 */
#define NOR_FEATURE_CONTINUOUS_READ  0x01

/**
 * 设置连续读取模式
 */
int nor_set_continuous_read(nor_device_t *dev, uint8_t mode_bits)
{
    uint8_t status;
    int ret;

    /* 读取当前状态 */
    ret = nor_read_status(dev, &status);
    if (ret < 0) return ret;

    /* 设置或清除连续读取模式位 */
    if (mode_bits & NOR_FEATURE_CONTINUOUS_READ) {
        status |= 0x20;  // 假设连续读取模式位在某个位置
    } else {
        status &= ~0x20;
    }

    /* 写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) return ret;

    /* 写状态寄存器 */
    uint8_t cmd[2] = { NOR_CMD_WRITE_STATUS_REG, status };
    ret = dev->transport->ops->send(dev->transport->handle, cmd, 2);
    if (ret < 0) return ret;

    /* 等待完成 */
    return nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
}

/**
 * 连续读取（用于大块数据读取）
 * 优化读取性能，减少命令开销
 */
int nor_flash_read_continuous(nor_device_t *dev, uint32_t addr,
                              uint8_t *buf, uint32_t len)
{
    int ret;
    uint32_t chunk_size = 256;  // 每次读取的最大字节数

    /* 第一次读取：发送完整命令 */
    ret = nor_flash_read_fast(dev, addr, buf,
                              (len > chunk_size) ? chunk_size : len);
    if (ret < 0) return ret;

    uint32_t transferred = ret;
    addr += chunk_size;
    len -= chunk_size;

    /* 连续读取：只发送地址 */
    while (len > 0) {
        uint32_t this_len = (len > chunk_size) ? chunk_size : len;

        /* 等待Flash就绪 */
        ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
        if (ret < 0) return ret;

        /* 只发送地址 */
        dev->transport->ops->cs_select(dev->transport->handle);
        uint8_t addr_buf[3];
        addr_buf[0] = (addr >> 16) & 0xFF;
        addr_buf[1] = (addr >> 8) & 0xFF;
        addr_buf[2] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, addr_buf, 3);

        /* 读取数据 */
        ret = dev->transport->ops->recv(dev->transport->handle,
                                         buf + transferred, this_len);
        dev->transport->ops->cs_deselect(dev->transport->handle);

        if (ret < 0) return ret;

        transferred += ret;
        addr += this_len;
        len -= this_len;
    }

    return (int)transferred;
}
```

---

## 4. 写操作实现

SPI Flash的写操作（页编程）是将数据写入Flash芯片的过程。与读取操作不同，写操作需要遵循特定的规则，包括写入前必须执行擦除操作（Flash只能从1写成0）、页编程不能跨页边界、以及需要先发送写使能命令等。

### 4.1 页面编程（0x02）

页面编程（Page Program）是SPI Flash最基本的写入方式。命令0x02允许在一个页（256字节）范围内一次性写入数据。写入前，目标地址必须处于擦除状态（0xFF），否则写入的数据将与原有数据进行AND运算，导致数据错误。

```
命令格式：
    +--------+----------+----------+------+------------------+
    | CMD    | A23-A16  | A15-A8   | A7-A0| DATA              |
    +--------+----------+----------+------+------------------+
    | 0x02   | Address  | Address  | Addr | 1-256 bytes      |
    +--------+----------+----------+------+------------------+

时序图：
    CS# ─────┐                                              ┌──────
              │                                              │
    SCK  ────┘└────────┐          ┌─────────────────────────┘
                       │          │
              ┌────────┴───┐ ┌──────┴───────┐
              │ CMD=0x02   │ │ 24-bit Addr │
              └────────────┘ └──────────────┘
                                   ┌────────┴────────┐
              DATA (MOSI)          │ 1-256 bytes    │
                                   └────────────────┘

限制：
    - 写入长度不能超过页大小（256字节）
    - 写入不能跨页边界
    - 写入前必须发送写使能命令(0x06)
    - 写入过程中CS#必须保持低电平
```

```c
/**
 * SPI Flash 页面编程（命令0x02）
 * @param dev 设备句柄
 * @param addr 页起始地址（必须对齐到页边界）
 * @param buf 数据缓冲区
 * @param len 写入长度（不能超过页大小）
 * @return 实际写入长度
 */
int nor_flash_page_program(nor_device_t *dev, uint32_t addr,
                           const uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_buf[5];

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 长度限制 */
    if (len > dev->page_size) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 页边界检查（某些芯片允许跨页） */
    if ((addr % dev->page_size) + len > dev->page_size) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
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

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送页编程命令和地址 */
    cmd_buf[0] = NOR_CMD_PAGE_PROGRAM;  // 0x02
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 发送数据 */
    ret = dev->transport->ops->send(dev->transport->handle, buf, len);

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    if (ret < 0) {
        return ret;
    }

    /* 等待编程完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_PAGE_PROGRAM);
    dev->write_in_progress = 0;

    return ret;
}
```

### 4.2 四线页面编程（0x32）

四线页面编程（Quad Page Program）是使用Quad SPI模式进行数据写入的命令。相比标准页面编程，它在数据传输阶段使用四根数据线，能够显著提高写入速度。

```
命令格式：
    +--------+----------+----------+------+------------------+
    | CMD    | A23-A16  | A15-A8   | A7-A0| DATA (4-line)    |
    +--------+----------+----------+------+------------------+
    | 0x32   | Address  | Address  | Addr | 1-256 bytes      |
    +--------+----------+----------+------+------------------+

时序图：
    CS# ─────┐                                              ┌──────
              │                                              │
    SCK  ────┘└────────┐          ┌─────────────────────────┘
                       │          │
              ┌────────┴───┐ ┌──────┴───────┐
              │ CMD=0x32   │ │ 24-bit Addr │
              └────────────┘ └──────────────┘
                                   ┌────────┴────────┐
              IO0-IO3 (DATA)       │ 4-line output  │
                                   └────────────────┘
```

```c
/**
 * SPI Flash 四线页面编程（命令0x32）
 * @param dev 设备句柄
 * @param addr 页起始地址
 * @param buf 数据缓冲区
 * @param len 写入长度
 * @return 实际写入长度
 */
int nor_flash_page_program_quad(nor_device_t *dev, uint32_t addr,
                               const uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t cmd_buf[5];

    /* 检查是否支持Quad SPI */
    if (!(dev->flags & NOR_FLAG_QUAD_SPI)) {
        return -NOR_ERR_NOT_SUPPORTED;
    }

    /* 参数检查 */
    if (!dev || !buf || len == 0 || len > dev->page_size) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 确认Quad模式已使能 */
    ret = nor_check_quad_enabled(dev);
    if (ret < 0) {
        return ret;
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

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送四线页编程命令和地址（命令和地址阶段仍为单线） */
    cmd_buf[0] = NOR_CMD_QUAD_PAGE_PROGRAM;  // 0x32
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 使用四线模式发送数据 */
    if (dev->transport->ops->quad_send) {
        ret = dev->transport->ops->quad_send(dev->transport->handle, buf, len);
    } else {
        ret = -NOR_ERR_NOT_SUPPORTED;
    }

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    if (ret < 0) {
        return ret;
    }

    /* 等待编程完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_PAGE_PROGRAM);
    dev->write_in_progress = 0;

    return ret;
}
```

### 4.3 写入使能/禁止

写入使能（Write Enable）和写入禁止（Write Disable）命令用于控制Flash的写操作权限。每次执行页编程、擦除、写入状态寄存器等写操作前，必须先发送写入使能命令。

```
写入使能命令（0x06）：
    +--------+
    | CMD    |
    +--------+
    | 0x06   |
    +--------+

写入禁止命令（0x04）：
    +--------+
    | CMD    |
    +--------+
    | 0x04   |
    +--------+

时序图：
    CS# ─────┐   ┌──────
              │   │
    SCK  ────┘   └──┘
              │
    CMD=0x06   ├── 写使能
    (0x04)     │
```

**写使能锁存器（WEL）**：执行写使能命令后，状态寄存器的WEL位被置1，允许后续的写操作。执行写禁止命令、页编程完成或擦除完成后，WEL位自动清零，禁止写操作。

```c
/**
 * 发送写使能命令
 * @return 0 success, 负值表示错误
 */
int nor_write_enable(nor_device_t *dev)
{
    int ret;
    uint8_t status;

    /* 发送写使能命令 */
    ret = dev->transport->ops->send_cmd(dev->transport->handle,
                                         NOR_CMD_WRITE_ENABLE);
    if (ret < 0) {
        return ret;
    }

    /* 验证WEL位是否置1 */
    ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    if (!(status & NOR_STATUS_WEL)) {
        return -NOR_ERR_WRITE_ENABLE;
    }

    return 0;
}

/**
 * 发送写禁止命令
 * @return 0 success, 负值表示错误
 */
int nor_write_disable(nor_device_t *dev)
{
    int ret;
    uint8_t status;

    /* 发送写禁止命令 */
    ret = dev->transport->ops->send_cmd(dev->transport->handle,
                                        NOR_CMD_WRITE_DISABLE);
    if (ret < 0) {
        return ret;
    }

    /* 验证WEL位是否清零 */
    ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    if (status & NOR_STATUS_WEL) {
        return -NOR_ERR;
    }

    return 0;
}
```

### 4.4 跨页写入处理

由于页编程命令不能跨页边界，驱动程序需要处理跨页写入的情况。解决方案是将跨页的数据拆分成多个页编程命令，每个命令写入一个页的数据。

```
跨页写入示例（写入500字节，从地址0x80开始）：

    页大小: 256字节

    +-------+-------+-------+
    | 0x80  | ...   | 0xFF  |  ← 第一页：128字节
    +-------+-------+-------+

    +-------+-------+-------+
    | 0x100 | ...   | 0x1FF |  ← 第二页：256字节
    +-------+-------+-------+

    +-------+-------+-------+
    | 0x200 | ...   | 0x20D |  ← 第三页：116字节
    +-------+-------+-------+
```

```c
/**
 * 连续写入（自动处理跨页）
 * @param dev 设备句柄
 * @param addr 起始地址
 * @param buf 数据缓冲区
 * @param len 写入长度
 * @return 实际写入长度
 */
int nor_flash_write(nor_device_t *dev, uint32_t addr,
                   const uint8_t *buf, uint32_t len)
{
    uint32_t page_size = dev->page_size;
    uint32_t wrote = 0;
    int ret;

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 逐页写入 */
    while (wrote < len) {
        /* 计算当前页剩余空间 */
        uint32_t page_offset = addr % page_size;
        uint32_t chunk = page_size - page_offset;

        /* 限制为剩余数据量 */
        if (chunk > (len - wrote)) {
            chunk = len - wrote;
        }

        /* 页编程 */
        ret = nor_flash_page_program(dev, addr, buf + wrote, chunk);
        if (ret < 0) {
            /* 如果是跨页错误，使用更小的块重试 */
            if (ret == -NOR_ERR_ALIGNMENT) {
                chunk = page_size - page_offset;
                if (chunk > (len - wrote)) {
                    chunk = len - wrote;
                }
                ret = nor_flash_page_program(dev, addr, buf + wrote, chunk);
            }
            if (ret < 0) {
                return (wrote > 0) ? (int)wrote : ret;
            }
        }

        addr += chunk;
        wrote += chunk;

        /* 进度回调 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((wrote * 100) / len);
            dev->callbacks.progress(NOR_EVENT_WRITE_PROGRESS, progress);
        }
    }

    return (int)wrote;
}

/**
 * 带验证的写入（写入后读取验证）
 */
int nor_flash_write_verify(nor_device_t *dev, uint32_t addr,
                          const uint8_t *buf, uint32_t len)
{
    int ret;
    uint8_t *verify_buf;

    /* 先执行写入 */
    ret = nor_flash_write(dev, addr, buf, len);
    if (ret < 0) {
        return ret;
    }

    /* 分配验证缓冲区 */
    verify_buf = (uint8_t *)malloc(ret);
    if (!verify_buf) {
        return -NOR_ERR_NO_MEMORY;
    }

    /* 等待写入完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        free(verify_buf);
        return ret;
    }

    /* 读取并验证 */
    ret = nor_flash_read_fast(dev, addr, verify_buf, ret);
    if (ret < 0) {
        free(verify_buf);
        return ret;
    }

    /* 逐字节比较 */
    if (memcmp(buf, verify_buf, ret) != 0) {
        free(verify_buf);
        return -NOR_ERR_VERIFY;
    }

    free(verify_buf);
    return ret;
}
```

---

## 5. 擦除操作实现

擦除操作是将Flash中的数据恢复到0xFF（全1）的过程。在写入数据之前，必须先擦除目标区域，否则写入的数据将与原有数据进行AND运算，导致数据错误。SPI Flash支持多种粒度的擦除操作，从最小的扇区（4KB）到整片擦除。

### 5.1 扇区擦除（0x20）

扇区擦除（Sector Erase）是最小粒度的擦除操作，擦除一个扇区（通常为4KB）。扇区擦除后，该扇区内的所有字节都变为0xFF，可以重新写入数据。

```
命令格式：
    +--------+----------+----------+------+
    | CMD    | A23-A16  | A15-A8   | A7-A0|
    +--------+----------+----------+------+
    | 0x20   | Address  | Address  | Addr |
    +--------+----------+----------+------+

时序图：
    CS# ─────┐                                              ┌──────
              │                                              │
    SCK  ────┘└────────┐          ┌─────────────────────────┘
                       │          │
              ┌────────┴───┐ ┌──────┴───────┐
              │ CMD=0x20   │ │ 24-bit Addr │
              └────────────┘ └──────────────┘

注意事项：
    - 地址必须对齐到扇区边界
    - 擦除前必须发送写使能命令
    - 擦除是异步操作，需要等待WIP位变为0
    - 典型擦除时间：15-50ms
```

```c
/**
 * SPI Flash 扇区擦除（命令0x20）
 * @param dev 设备句柄
 * @param addr 扇区地址（必须4KB对齐）
 * @return 0 success, 负值表示错误
 */
int nor_flash_erase_sector(nor_device_t *dev, uint32_t addr)
{
    int ret;
    uint8_t cmd_buf[5];

    /* 参数检查 */
    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 扇区对齐检查 */
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

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送扇区擦除命令和地址 */
    cmd_buf[0] = NOR_CMD_ERASE_SECTOR;  // 0x20
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 片选拉高 - 此时擦除开始 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

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
```

### 5.2 32KB块擦除（0x52）

32KB块擦除（Block Erase 32KB）提供中等粒度的擦除操作。相比扇区擦除，块擦除的速度更快，但需要擦除的面积更大。某些芯片支持32KB块，而另一些可能只支持64KB块。

```
命令格式：
    +--------+----------+----------+------+
    | CMD    | A23-A16  | A15-A8   | A7-A0|
    +--------+----------+----------+------+
    | 0x52   | Address  | Address  | Addr |
    +--------+----------+----------+------+

典型擦除时间：100-300ms
```

```c
/**
 * SPI Flash 32KB块擦除（命令0x52）
 * @param dev 设备句柄
 * @param addr 块地址（必须32KB对齐）
 * @return 0 success, 负值表示错误
 */
int nor_flash_erase_block_32k(nor_device_t *dev, uint32_t addr)
{
    int ret;
    uint8_t cmd_buf[5];
    uint32_t block_size = 32 * 1024;  // 32KB

    /* 对齐检查 */
    if (addr % block_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 边界检查 */
    if (addr + block_size > dev->total_size) {
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

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送块擦除命令 */
    cmd_buf[0] = NOR_CMD_ERASE_BLOCK_32K;  // 0x52
    cmd_buf[1] = (addr >> 16) & 0xFF;
    cmd_buf[2] = (addr >> 8) & 0xFF;
    cmd_buf[3] = addr & 0xFF;
    dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    /* 等待擦除完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_BLOCK_ERASE);
    dev->write_in_progress = 0;

    return ret;
}
```

### 5.3 64KB块擦除（0xD8）

64KB块擦除（Block Erase 64KB）提供更大粒度的擦除操作，是大容量Flash芯片最常用的擦除方式。块擦除的速度比扇区擦除快得多，特别适合批量擦除场景。

```
命令格式：
    +--------+----------+----------+------+
    | CMD    | A23-A16  | A15-A8   | A7-A0|
    +--------+----------+----------+------+
    | 0xD8   | Address  | Address  | Addr |
    +--------+----------+----------+------+

典型擦除时间：200-1000ms
```

```c
/**
 * SPI Flash 64KB块擦除（命令0xD8）
 * @param dev 设备句柄
 * @param addr 块地址（必须64KB对齐）
 * @return 0 success, 负值表示错误
 */
int nor_flash_erase_block_64k(nor_device_t *dev, uint32_t addr)
{
    int ret;
    uint8_t cmd_buf[5];
    uint32_t block_size = 64 * 1024;  // 64KB

    /* 对齐检查 */
    if (addr % block_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 边界检查 */
    if (addr + block_size > dev->total_size) {
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

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送64KB块擦除命令 */
    cmd_buf[0] = NOR_CMD_ERASE_BLOCK_64K;  // 0xD8
    if (dev->addr_bytes == 4) {
        cmd_buf[1] = (addr >> 24) & 0xFF;
        cmd_buf[2] = (addr >> 16) & 0xFF;
        cmd_buf[3] = (addr >> 8) & 0xFF;
        cmd_buf[4] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 5);
    } else {
        cmd_buf[1] = (addr >> 16) & 0xFF;
        cmd_buf[2] = (addr >> 8) & 0xFF;
        cmd_buf[3] = addr & 0xFF;
        dev->transport->ops->send(dev->transport->handle, cmd_buf, 4);
    }

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    /* 等待擦除完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_BLOCK_ERASE);
    dev->write_in_progress = 0;

    return ret;
}
```

### 5.4 整片擦除（0xC7/0x60）

整片擦除（Chip Erase）将Flash芯片的所有存储单元恢复到擦除状态。这是耗时最长的擦除操作，对于大容量芯片可能需要数十秒。

```
命令格式（0xC7）：
    +--------+
    | CMD    |
    +--------+
    | 0xC7   |
    +--------+

命令格式（0x60）：
    +--------+
    | CMD    |
    +--------+
    | 0x60   |
    +--------+

典型擦除时间：
    - 64Mbit: 10-30秒
    - 128Mbit: 20-60秒
    - 256Mbit: 40-120秒
```

```c
/**
 * SPI Flash 整片擦除（命令0xC7或0x60）
 * @param dev 设备句柄
 * @return 0 success, 负值表示错误
 */
int nor_flash_erase_chip(nor_device_t *dev)
{
    int ret;
    uint8_t cmd;

    /* 参数检查 */
    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 检查是否有块保护 */
    ret = nor_check_protected(dev);
    if (ret < 0) {
        return ret;
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

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送整片擦除命令（0xC7或0x60） */
    cmd = NOR_CMD_ERASE_CHIP;  // 0xC7
    dev->transport->ops->send(dev->transport->handle, &cmd, 1);

    /* 片选拉高 - 此时擦除开始 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    /* 触发进度回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_STARTED, 0);
    }

    /* 等待擦除完成（整片擦除时间较长） */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_CHIP_ERASE);
    dev->write_in_progress = 0;

    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_COMPLETED, 100);
    }

    return ret;
}

/**
 * 检查并解除块保护
 */
static int nor_check_protected(nor_device_t *dev)
{
    uint8_t status;
    int ret;

    ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    /* 检查块保护位BP[2:0] */
    if (status & (NOR_STATUS_BP0 | NOR_STATUS_BP1 | NOR_STATUS_BP2)) {
        /* 需要解除保护 */
        ret = nor_write_enable(dev);
        if (ret < 0) {
            return ret;
        }

        /* 写状态寄存器清除保护位 */
        status &= ~(NOR_STATUS_BP0 | NOR_STATUS_BP1 | NOR_STATUS_BP2);
        uint8_t cmd[2] = { NOR_CMD_WRITE_STATUS_REG, status };
        ret = dev->transport->ops->send(dev->transport->handle, cmd, 2);
        if (ret < 0) {
            return ret;
        }

        /* 等待完成 */
        ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

/**
 * 通用擦除函数（自动选择最优擦除粒度）
 * @param dev 设备句柄
 * @param addr 起始地址
 * @param len 擦除长度
 * @return 0 success, 负值表示错误
 */
int nor_flash_erase(nor_device_t *dev, uint32_t addr, uint32_t len)
{
    uint32_t sector_size = dev->sector_size;
    uint32_t block_size = dev->block_size;
    uint32_t erased = 0;
    int ret;

    /* 参数检查 */
    if (!dev || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 计算擦除进度 */
    uint32_t total_len = len;

    while (erased < len) {
        uint32_t current_addr = addr + erased;
        uint32_t remaining = len - erased;

        /* 优先使用64KB块擦除 */
        if (remaining >= block_size && (current_addr % block_size) == 0) {
            ret = nor_flash_erase_block_64k(dev, current_addr);
            if (ret < 0) {
                return ret;
            }
            erased += block_size;
        }
        /* 32KB块擦除 */
        else if (remaining >= 32*1024 && (current_addr % (32*1024)) == 0) {
            ret = nor_flash_erase_block_32k(dev, current_addr);
            if (ret < 0) {
                return ret;
            }
            erased += 32 * 1024;
        }
        /* 扇区擦除 */
        else {
            /* 对齐到扇区边界 */
            uint32_t sector_addr = (current_addr / sector_size) * sector_size;
            ret = nor_flash_erase_sector(dev, sector_addr);
            if (ret < 0) {
                return ret;
            }
            erased += sector_size;
        }

        /* 进度回调 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((erased * 100) / total_len);
            dev->callbacks.progress(NOR_EVENT_ERASE_PROGRESS, progress);
        }
    }

    return 0;
}
```

---

## 6. 状态轮询与等待

SPI Flash的写（页编程）和擦除操作是内部执行的异步操作，主控芯片无法立即知道操作何时完成。状态轮询机制通过读取Flash的状态寄存器，判断操作是否完成。

### 6.1 WIP 位轮询

WIP（Write In Progress）位是状态寄存器（Status Register 1）的第0位。当Flash正在执行页编程、擦除或写入状态寄存器操作时，WIP位为1；操作完成后，WIP位自动变为0。

```
状态寄存器1：
    +------+------+------+------+------+------+------+------+
    | SRL  | SEC  | TB   | BP2  | BP1  | BP0  | WEL  | WIP  |
    +------+------+------+------+------+------+------+------+
    | 7    | 6    | 5    | 4    | 3    | 2    | 1    | 0    |

    WIP (Bit 0): Write In Progress
        0: 空闲状态，可以执行新操作
        1: 正在执行写/擦除操作

    WEL (Bit 1): Write Enable Latch
        0: 写禁止
        1: 写使能

    BP0-BP2 (Bits 2-4): Block Protect
        000: 无保护
        111: 全片保护
```

### 6.2 读状态寄存器（0x05）

读状态寄存器命令（0x05）用于读取Flash的状态信息，包括WIP位、WEL位和块保护状态。

```
命令格式：
    +--------+------+
    | CMD    | DOUT |
    +--------+------+
    | 0x05   | Status|
    +--------+------+

时序图：
    CS# ─────┐                                    ┌──────
              │                                    │
    SCK  ────┘└────────┐          ┌───────────────┘
                       │          │
              ┌────────┴───┐     │
              │ CMD=0x05    │     │ Dummy (可省略)
              └────────────┘     │
                                 ├── Status Register
```

```c
/**
 * 读取状态寄存器
 * @param dev 设备句柄
 * @param status 状态寄存器值输出
 * @return 0 success, 负值表示错误
 */
int nor_read_status(nor_device_t *dev, uint8_t *status)
{
    int ret;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;  // 0x05
    uint8_t resp;

    if (!dev || !status) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送读状态命令 */
    ret = dev->transport->ops->transfer(dev->transport->handle,
                                         &cmd, &resp, 1);

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    if (ret < 0) {
        return ret;
    }

    *status = resp;
    return 0;
}

/**
 * 读取状态寄存器2（某些芯片使用）
 */
int nor_read_status2(nor_device_t *dev, uint8_t *status)
{
    int ret;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG_2;  // 0x35
    uint8_t resp;

    if (!dev || !status) {
        return -NOR_ERR_INVALID_PARAM;
    }

    dev->transport->ops->cs_select(dev->transport->handle);
    ret = dev->transport->ops->transfer(dev->transport->handle,
                                         &cmd, &resp, 1);
    dev->transport->ops->cs_deselect(dev->transport->handle);

    if (ret < 0) {
        return ret;
    }

    *status = resp;
    return 0;
}

/**
 * 读取完整状态（包括两个状态寄存器）
 */
int nor_read_status_all(nor_device_t *dev, uint8_t *status1, uint8_t *status2)
{
    int ret;

    ret = nor_read_status(dev, status1);
    if (ret < 0) {
        return ret;
    }

    if (status2) {
        ret = nor_read_status2(dev, status2);
    }

    return ret;
}
```

### 6.3 轮询超时处理

在执行写或擦除操作后，需要轮询WIP位等待操作完成。轮询需要设置合理的超时时间，以避免程序死锁。

```
超时时间建议值：
    - 页编程: 5-50ms (通常25ms)
    - 扇区擦除(4KB): 20-100ms (通常50ms)
    - 32KB块擦除: 100-300ms
    - 64KB块擦除: 200-1000ms
    - 整片擦除: 10-120秒
```

```c
/**
 * 获取系统毫秒计数（需要平台实现）
 */
uint32_t nor_get_tick(void);

/**
 * 等待Flash就绪（轮询WIP位）
 * @param dev 设备句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 0=就绪, -ETIMEDOUT=超时
 */
int nor_wait_ready(nor_device_t *dev, uint32_t timeout_ms)
{
    uint8_t status;
    uint32_t start_time;
    uint32_t elapsed;
    int ret;

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    start_time = nor_get_tick();

    /* 轮询WIP位 */
    do {
        ret = nor_read_status(dev, &status);
        if (ret < 0) {
            /* 读失败时等待一小段时间后重试 */
            nor_delay_ms(1);
            continue;
        }

        /* 检查WIP位 */
        if (!(status & NOR_STATUS_WIP)) {
            return 0;
        }

        /* 计算已用时间 */
        elapsed = nor_get_tick() - start_time;

        /* 超时检查 */
        if (elapsed >= timeout_ms) {
            return -NOR_ERR_TIMEOUT;
        }

        /* 调用空闲回调（如果有） */
        if (dev->callbacks.idle) {
            dev->callbacks.idle();
        }

        /* 短暂延时后继续轮询 */
        nor_delay_ms(1);

    } while (1);
}

/**
 * 带进度的等待（用于长时间操作）
 */
int nor_wait_ready_with_progress(nor_device_t *dev, uint32_t timeout_ms,
                                  uint8_t *progress)
{
    uint8_t status;
    uint32_t start_time;
    uint32_t elapsed;
    int ret;

    start_time = nor_get_tick();

    do {
        ret = nor_read_status(dev, &status);
        if (ret < 0) {
            nor_delay_ms(1);
            continue;
        }

        if (!(status & NOR_STATUS_WIP)) {
            if (progress) *progress = 100;
            return 0;
        }

        elapsed = nor_get_tick() - start_time;

        /* 更新进度 */
        if (progress) {
            *progress = (uint8_t)((elapsed * 100) / timeout_ms);
            if (*progress > 100) *progress = 100;
        }

        if (elapsed >= timeout_ms) {
            return -NOR_ERR_TIMEOUT;
        }

        /* 调用空闲回调 */
        if (dev->callbacks.idle) {
            dev->callbacks.idle();
        }

        nor_delay_ms(10);  // 稍长的延时减少SPI通信次数

    } while (1);
}

/**
 * 简单的延时函数（毫秒）
 */
void nor_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);  // 基于HAL库的延时
}

/**
 * 轮询超时时间常量定义
 */
#define NOR_TIMEOUT_DEFAULT         100     // 默认超时（ms）
#define NOR_TIMEOUT_PAGE_PROGRAM   50      // 页编程超时
#define NOR_TIMEOUT_SECTOR_ERASE    100     // 扇区擦除超时
#define NOR_TIMEOUT_BLOCK_ERASE     1000    // 块擦除超时
#define NOR_TIMEOUT_CHIP_ERASE      120000  // 整片擦除超时（2分钟）
```

---

## 7. 驱动代码示例

本节提供完整的驱动代码示例，包括初始化、读取、写入和擦除等核心功能的实现。

### 7.1 初始化函数

```c
/**
 * @file nor_spi_driver.c
 * @brief SPI Flash驱动实现
 */

#include "nor_spi_driver.h"
#include <string.h>

/* 默认芯片参数 */
static const nor_flash_param_t default_param = {
    .manufacturer_id = 0,
    .device_id = 0,
    .name = "Unknown",
    .total_size = 16 * 1024 * 1024,   // 默认16Mbit
    .sector_size = 4 * 1024,          // 4KB扇区
    .block_size = 64 * 1024,           // 64KB块
    .page_size = 256,                  // 256字节页
    .addr_bytes = 3,                   // 默认3字节地址
    .flags = 0,
};

/**
 * 芯片参数查找表
 */
static const nor_flash_param_t flash_param_table[] = {
    /* Winbond W25Q256JV */
    {
        .manufacturer_id = 0xEF,
        .device_id = 0x4019,
        .name = "W25Q256JV",
        .total_size = 32 * 1024 * 1024,
        .sector_size = 4 * 1024,
        .block_size = 64 * 1024,
        .page_size = 256,
        .addr_bytes = 3,
        .flags = NOR_FLAG_4BYTE_ADDR | NOR_FLAG_QUAD_SPI,
    },
    /* Winbond W25Q128JV */
    {
        .manufacturer_id = 0xEF,
        .device_id = 0x4018,
        .name = "W25Q128JV",
        .total_size = 16 * 1024 * 1024,
        .sector_size = 4 * 1024,
        .block_size = 64 * 1024,
        .page_size = 256,
        .addr_bytes = 3,
        .flags = NOR_FLAG_QUAD_SPI,
    },
    /* GigaDevice GD25Q256C */
    {
        .manufacturer_id = 0xC8,
        .device_id = 0x4019,
        .name = "GD25Q256C",
        .total_size = 32 * 1024 * 1024,
        .sector_size = 4 * 1024,
        .block_size = 64 * 1024,
        .page_size = 256,
        .addr_bytes = 3,
        .flags = NOR_FLAG_4BYTE_ADDR | NOR_FLAG_QUAD_SPI,
    },
    /* 结束标记 */
    { .manufacturer_id = 0xFF }
};

/**
 * 查找芯片参数
 */
static const nor_flash_param_t* nor_find_param(uint8_t mfg_id, uint16_t dev_id)
{
    for (int i = 0; flash_param_table[i].manufacturer_id != 0xFF; i++) {
        const nor_flash_param_t *p = &flash_param_table[i];
        if (p->manufacturer_id == mfg_id && p->device_id == dev_id) {
            return p;
        }
    }
    return NULL;
}

/**
 * 读取Flash ID
 */
int nor_read_id(nor_device_t *dev, uint8_t *mfg_id, uint16_t *dev_id)
{
    int ret;
    uint8_t cmd = NOR_CMD_READ_ID;  // 0x9F
    uint8_t resp[3];

    if (!dev || !mfg_id || !dev_id) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 片选拉低 */
    dev->transport->ops->cs_select(dev->transport->handle);

    /* 发送读ID命令 */
    ret = dev->transport->ops->send(dev->transport->handle, &cmd, 1);
    if (ret < 0) {
        dev->transport->ops->cs_deselect(dev->transport->handle);
        return ret;
    }

    /* 读取3字节ID */
    ret = dev->transport->ops->recv(dev->transport->handle, resp, 3);

    /* 片选拉高 */
    dev->transport->ops->cs_deselect(dev->transport->handle);

    if (ret < 0) {
        return ret;
    }

    *mfg_id = resp[0];
    *dev_id = (resp[1] << 8) | resp[2];

    return 0;
}

/**
 * 软件复位
 */
int nor_reset(nor_device_t *dev)
{
    int ret;
    uint8_t cmd;

    /* 发送复位使能 */
    cmd = 0x66;
    ret = dev->transport->ops->send(dev->transport->handle, &cmd, 1);
    if (ret < 0) return ret;

    /* 发送复位命令 */
    cmd = 0x99;
    ret = dev->transport->ops->send(dev->transport->handle, &cmd, 1);
    if (ret < 0) return ret;

    /* 等待复位完成 */
    nor_delay_ms(30);

    return 0;
}

/**
 * 初始化Flash设备
 */
int nor_init(nor_device_t *dev, const nor_config_t *config)
{
    int ret;
    const nor_flash_param_t *param;

    /* 参数检查 */
    if (!dev || !config || !config->transport) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 保存传输接口 */
    dev->transport = config->transport;

    /* 尝试复位Flash */
    if (dev->flags & NOR_FLAG_HAS_RESET) {
        ret = nor_reset(dev);
        if (ret < 0) {
            /* 复位失败不一定是致命错误，继续尝试 */
        }
    }

    /* 读取芯片ID */
    ret = nor_read_id(dev, &dev->manufacturer_id, &dev->device_id);
    if (ret < 0) {
        return -NOR_ERR_NO_DEVICE;
    }

    /* 查找芯片参数 */
    param = nor_find_param(dev->manufacturer_id, dev->device_id);
    if (param) {
        /* 使用预定义的参数 */
        dev->total_size = param->total_size;
        dev->sector_size = param->sector_size;
        dev->block_size = param->block_size;
        dev->page_size = param->page_size;
        dev->addr_bytes = param->addr_bytes;
        dev->flags = param->flags;
        strncpy(dev->chip_name, param->name, sizeof(dev->chip_name) - 1);
    } else {
        /* 使用默认参数 */
        dev->total_size = default_param.total_size;
        dev->sector_size = default_param.sector_size;
        dev->block_size = default_param.block_size;
        dev->page_size = default_param.page_size;
        dev->addr_bytes = default_param.addr_bytes;
        dev->flags = default_param.flags;
    }

    /* 配置回调函数 */
    if (config->callbacks) {
        memcpy(&dev->callbacks, config->callbacks, sizeof(nor_callbacks_t));
    }

    /* 初始化状态 */
    dev->state = NOR_STATE_IDLE;
    dev->write_in_progress = 0;

    /* 使能Quad模式（如果支持且需要） */
    if ((dev->flags & NOR_FLAG_QUAD_SPI) && config->auto_quad_enable) {
        ret = spi_enable_quad_mode(dev);
        if (ret < 0) {
            /* Quad模式使能失败，可能不支持 */
        }
    }

    /* 进入4字节地址模式（如果支持且需要） */
    if ((dev->flags & NOR_FLAG_4BYTE_ADDR) && config->use_4byte_addr) {
        ret = nor_enter_4byte_mode(dev);
        if (ret < 0) {
            /* 进入4字节模式失败 */
        }
    }

    return 0;
}

/**
 * 进入4字节地址模式
 */
int nor_enter_4byte_mode(nor_device_t *dev)
{
    int ret;
    uint8_t cmd = 0xB7;  // Enter 4-byte address mode

    ret = nor_write_enable(dev);
    if (ret < 0) return ret;

    ret = dev->transport->ops->send(dev->transport->handle, &cmd, 1);
    if (ret < 0) return ret;

    dev->addr_bytes = 4;
    return 0;
}
```

### 7.2 读取函数

```c
/**
 * 通用读取函数（根据模式自动选择最快的读取方式）
 */
int nor_read(nor_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 根据接口类型选择读取方法 */
    switch (dev->iface_type) {
        case NOR_IFACE_QUAD_IO:
            return nor_flash_read_quad_io(dev, addr, buf, len);
        case NOR_IFACE_QUAD:
            return nor_flash_read_quad(dev, addr, buf, len);
        case NOR_IFACE_DUAL:
            return nor_flash_read_dual(dev, addr, buf, len);
        case NOR_IFACE_SPI:
        default:
            /* 默认使用快速读取 */
            return nor_flash_read_fast(dev, addr, buf, len);
    }
}

/**
 * 读取数据到缓冲区（简化版）
 */
int nor_read_data(nor_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len)
{
    /* 等待就绪 */
    int ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) return ret;

    /* 执行读取 */
    return nor_read(dev, addr, buf, len);
}
```

### 7.3 写入函数

```c
/**
 * 写入数据（自动处理跨页）
 */
int nor_write(nor_device_t *dev, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t page_size = dev->page_size;
    uint32_t written = 0;

    /* 参数检查 */
    if (!dev || !buf || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 进度回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_WRITE_STARTED, 0);
    }

    /* 逐页写入 */
    while (written < len) {
        uint32_t page_offset = addr % page_size;
        uint32_t chunk = page_size - page_offset;

        if (chunk > (len - written)) {
            chunk = len - written;
        }

        /* 页编程 */
        int ret = nor_flash_page_program(dev, addr, buf + written, chunk);
        if (ret < 0) {
            return (written > 0) ? (int)written : ret;
        }

        addr += chunk;
        written += chunk;

        /* 进度回调 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((written * 100) / len);
            dev->callbacks.progress(NOR_EVENT_WRITE_PROGRESS, progress);
        }
    }

    /* 完成回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_WRITE_COMPLETED, 100);
    }

    return (int)written;
}
```

### 7.4 擦除函数

```c
/**
 * 擦除指定区域（自动选择最优擦除粒度）
 */
int nor_erase(nor_device_t *dev, uint32_t addr, uint32_t len)
{
    uint32_t sector_size = dev->sector_size;
    uint32_t block_size = dev->block_size;
    uint32_t erased = 0;
    uint32_t total_len = len;

    /* 参数检查 */
    if (!dev || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 进度回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_STARTED, 0);
    }

    /* 自动选择擦除粒度 */
    while (erased < len) {
        uint32_t current_addr = addr + erased;
        uint32_t remaining = len - erased;
        int ret;

        /* 优先使用64KB块擦除 */
        if (remaining >= block_size && (current_addr % block_size) == 0) {
            ret = nor_flash_erase_block_64k(dev, current_addr);
            if (ret < 0) return ret;
            erased += block_size;
        }
        /* 32KB块擦除 */
        else if (remaining >= 32*1024 && (current_addr % (32*1024)) == 0) {
            ret = nor_flash_erase_block_32k(dev, current_addr);
            if (ret < 0) return ret;
            erased += 32 * 1024;
        }
        /* 扇区擦除 */
        else {
            uint32_t sector_addr = (current_addr / sector_size) * sector_size;
            ret = nor_flash_erase_sector(dev, sector_addr);
            if (ret < 0) return ret;
            erased += sector_size;
        }

        /* 进度回调 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((erased * 100) / total_len);
            dev->callbacks.progress(NOR_EVENT_ERASE_PROGRESS, progress);
        }
    }

    /* 完成回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_COMPLETED, 100);
    }

    return 0;
}

/**
 * 擦除指定地址的扇区
 */
int nor_erase_sector(nor_device_t *dev, uint32_t addr)
{
    /* 对齐检查 */
    if (addr % dev->sector_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    return nor_flash_erase_sector(dev, addr);
}

/**
 * 擦除指定地址的块（64KB）
 */
int nor_erase_block(nor_device_t *dev, uint32_t addr)
{
    /* 对齐检查 */
    if (addr % dev->block_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    return nor_flash_erase_block_64k(dev, addr);
}

/**
 * 整片擦除
 */
int nor_erase_chip(nor_device_t *dev)
{
    return nor_flash_erase_chip(dev);
}
```

---

## 本章小结

本章详细介绍了SPI Flash驱动的完整实现，包括以下几个关键方面：

**1. SPI配置详解**
- 详细说明了SPI Mode 0（CPOL=0, CPHA=0）的配置方法
- 介绍了时钟频率选择的原则和不同芯片的频率限制
- 解释了Single/Dual/Quad模式的切换机制
- 提供了基于STM32 HAL库的完整配置示例

**2. 读操作实现**
- 标准读取（0x03）：兼容性最好的基本读取方式
- 快速读取（0x0B）：支持更高频率，需要8个dummy周期
- 双线读取（0x3B）：利用Dual SPI模式提升带宽
- 四线读取（0x6B, 0xEB）：Quad SPI模式，最高传输效率
- 连续读取模式：减少命令开销，适合大块数据读取
- 每种读取方式都提供了完整的命令时序图和代码实现

**3. 写操作实现**
- 页面编程（0x02）：基本的写入方式，256字节页边界限制
- 四线页面编程（0x32）：Quad SPI模式的写入命令
- 写入使能/禁止：每次写操作前的必要步骤
- 跨页写入处理：自动拆分跨页数据为多个页编程命令
- 提供了带验证的写入函数确保数据正确性

**4. 擦除操作实现**
- 扇区擦除（0x20）：4KB最小擦除粒度
- 32KB块擦除（0x52）：中等擦除粒度
- 64KB块擦除（0xD8）：大容量芯片常用的大块擦除
- 整片擦除（0xC7/0x60）：耗时最长但最彻底的擦除方式
- 自动选择最优擦除粒度的通用擦除函数

**5. 状态轮询与等待**
- WIP位轮询：判断写/擦除操作是否完成的核心机制
- 读状态寄存器（0x05）：获取Flash状态的方法
- 超时处理：防止程序死锁的必备机制
- 带进度的等待函数：提升用户体验

**6. 驱动代码示例**
- 完整的初始化函数，包括芯片ID检测和参数自动识别
- 通用的读取/写入/擦除函数接口
- 完整的芯片参数查找表
- 与平台无关的抽象接口设计

通过本章的学习，开发者应能够掌握SPI Flash驱动的核心技术要点，根据具体的硬件平台和芯片型号实现稳定可靠的驱动代码。驱动框架的分层设计使得代码具有良好的可移植性和可扩展性，能够方便地适配新的芯片和平台。

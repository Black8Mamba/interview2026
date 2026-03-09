# Flash 算法详解

Flash 算法是 Nor Flash 驱动开发的核心内容，它涵盖了 Flash 芯片的基本操作流程和关键技术细节。本章将详细介绍 Flash 芯片的识别与参数读取、写入控制机制、编程与擦除流程、错误检测与处理以及状态寄存器操作。通过深入理解这些算法，开发者能够实现稳定可靠的 Flash 驱动，并为上层应用提供完善的数据存储服务。

---

## 1. Read ID 与参数读取

芯片识别是 Flash 驱动的第一步，也是确保硬件正常工作的基础。通过读取芯片的厂商 ID 和器件 ID，驱动可以确认芯片类型、加载正确的参数配置，并验证硬件连接是否正常。

### 1.1 JEDEC ID 读取原理

JEDEC ID 是由 JEDEC（固态技术协会）统一分配的芯片标识，每个 Flash 芯片制造商都有唯一的厂商 ID。JEDEC ID 的读取是 Flash 驱动的标准操作，几乎所有兼容 JEDEC 标准的 Flash 芯片都支持这一功能。

JEDEC ID 的数据结构包含三个字节：第一个字节是厂商 ID（Manufacturer ID），后续两个字节是器件 ID（Device ID）。厂商 ID 标识了芯片的制造商，器件 ID 则标识了具体的芯片型号。通过这两个 ID 的组合，驱动可以唯一确定芯片类型并加载对应的参数。

常见的 Flash 芯片厂商 ID 如表所示：

| 厂商 ID | 厂商名称 | 代表产品系列 |
|---------|----------|--------------|
| 0xEF | Winbond（华邦） | W25QxxJV 系列 |
| 0xC8 | GigaDevice（兆易创新） | GD25Q 系列 |
| 0xC2 | Macronix（旺宏） | MX25L/MX25Q 系列 |
| 0x20 | Micron（美光） | N25Q/MT25Q 系列 |
| 0x1F | Atmel（爱特梅尔） | AT25DF/AT26DF 系列 |
| 0x01 | AMD/Spansion | S25FL 系列 |

### 1.2 SPI Flash ID 读取实现

SPI Flash 的 ID 读取通过发送特定的命令码来实现。标准 SPI Flash 使用 0x9F（REID）命令读取 JEDEC ID，而某些芯片还支持扩展的 ID 读取命令。

```c
/**
 * SPI Flash JEDEC ID 读取
 * 命令格式: 0x9F [响应: MFG_ID(1B) | Device_ID(2B)]
 *
 * 时序图:
 * CS# ───┐     ┌─────────────────────────────────────┐
 *        │     │                                     │
 *        └─────┘                                     │
 * SCK ─────────┐    ┌────┐    ┌────┐    ┌────┐    ┌───│
 *              └────┘    └────┘    └────┘    └────│
 * MOSI ──[0x9F]────────────────────────────────────
 *              │
 *              │ [MFG_ID] [DEVICE_ID_HIGH] [DEVICE_ID_LOW]
 * MISO ─────────────────[0xEF]─────[0x40]─────[0x19]
 *
 * @param dev 设备句柄
 * @param mfg_id 厂商ID输出
 * @param dev_id 器件ID输出（16位）
 * @return 0 成功，负值错误
 */
int nor_read_jedec_id(nor_device_t *dev, uint8_t *mfg_id, uint16_t *dev_id)
{
    int ret;
    uint8_t id_buf[3];
    uint8_t cmd = NOR_CMD_READ_JEDEC_ID;

    if (!dev || !mfg_id || !dev_id) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 片选拉低 */
    dev->transport->cs_select(dev->transport);

    /* 发送读取ID命令 */
    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) {
        dev->transport->cs_deselect(dev->transport);
        return -NOR_ERR_TRANSPORT;
    }

    /* 读取3字节ID数据 */
    ret = dev->transport->recv(dev->transport, id_buf, 3);
    dev->transport->cs_deselect(dev->transport);

    if (ret != 3) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 解析ID数据 */
    *mfg_id = id_buf[0];
    *dev_id = ((uint16_t)id_buf[1] << 8) | id_buf[2];

    /* 记录设备信息 */
    dev->manufacturer_id = *mfg_id;
    dev->device_id = *dev_id;

    return 0;
}
```

### 1.3 设备 ID 读取命令变体

不同厂商和不同系列的 Flash 芯片可能支持多种 ID 读取方式。除了标准的 JEDEC ID 读取外，还有一些特定于厂商的命令。

**RDID 命令（0x9F）**：这是最标准的 JEDEC ID 读取命令，几乎所有兼容芯片都支持。它返回 3 字节的 ID 信息：厂商 ID（1 字节）、器件 ID 高字节（1 字节）、器件 ID 低字节（1 字节）。

**REMS 命令（0x90）**：部分芯片支持使用 REMS 命令读取更详细的识别信息，该命令可以返回厂商名称和器件名称字符串。

```c
/**
 * 使用 REMS 命令读取扩展设备信息
 * 命令格式: 0x90 + 3字节地址(0x000000) + Dummy → 返回 Device ID 和 MFG ID
 */
int nor_read_rems(nor_device_t *dev, uint8_t *mfg_id, uint8_t *dev_id)
{
    int ret;
    uint8_t cmd_buf[4] = { NOR_CMD_READ_ELECTRONIC_SIGNATURE, 0x00, 0x00, 0x00 };
    uint8_t resp[2];

    dev->transport->cs_select(dev->transport);

    /* 发送命令和地址 */
    ret = dev->transport->send(dev->transport, cmd_buf, 4);
    if (ret < 0) {
        dev->transport->cs_deselect(dev->transport);
        return ret;
    }

    /* 读取响应 */
    ret = dev->transport->recv(dev->transport, resp, 2);
    dev->transport->cs_deselect(dev->transport);

    if (ret != 2) {
        return -NOR_ERR_TRANSPORT;
    }

    if (mfg_id) *mfg_id = resp[0];
    if (dev_id) *dev_id = resp[1];

    return 0;
}
```

### 1.4 SFDP 参数读取

SFDP（Serial Flash Discoverable Parameters）是 JEDEC 标准定义的串行 Flash 参数发现机制。通过 SFDP，驱动可以自动获取芯片的详细参数，无需硬编码芯片信息。

SFDP 的读取使用 0x5A 命令，随后发送 24 位地址（通常为 0x000000 表示读取 SFDP 头部），然后读取返回的参数数据。

```c
/**
 * SFDP 头部结构
 */
typedef struct {
    uint32_t signature;      // SFDP签名: 0x50444653 ("SFDP")
    uint8_t  minor_version; // Minor版本号
    uint8_t  major_version; // Major版本号
    uint8_t  num_params;     // 参数头数量
    uint8_t  access_freq;    // 访问频率(单位:100KHz)
} sfdp_header_t;

/**
 * SFDP 参数头结构
 */
typedef struct {
    uint16_t param_id;       // 参数ID
    uint8_t  param_rev;      // 参数版本
    uint8_t  length;         // 参数表长度(单位:32位字)
    uint32_t table_ptr;      // 参数表指针
    uint32_t table_alt_ptr; // 备用指针
} sfdp_param_header_t;

/**
 * 读取 SFDP 头部信息
 * @param dev 设备句柄
 * @param header 输出头部信息
 * @return 0 成功
 */
int nor_read_sfdp_header(nor_device_t *dev, sfdp_header_t *header)
{
    int ret;
    uint8_t cmd = NOR_CMD_SFDP;
    uint8_t addr[3] = { 0x00, 0x00, 0x00 };  // SFDP起始地址
    uint8_t dummy = 0xFF;                    // 等待周期
    uint8_t buf[16];

    /* 发送SFDP读取命令 */
    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) goto exit;

    ret = dev->transport->send(dev->transport, addr, 3);
    if (ret < 0) goto exit;

    /* 发送dummy字节等待数据准备 */
    ret = dev->transport->send(dev->transport, &dummy, 1);
    if (ret < 0) goto exit;

    /* 读取SFDP头部(16字节) */
    ret = dev->transport->recv(dev->transport, buf, 16);

exit:
    dev->transport->cs_deselect(dev->transport);

    if (ret != 16) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 解析头部 */
    header->signature = ((uint32_t)buf[0]) | ((uint32_t)buf[1] << 8) |
                       ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    header->minor_version = buf[4];
    header->major_version = buf[5];
    header->num_params = buf[6];
    header->access_freq = buf[7];

    /* 验证SFDP签名 */
    if (header->signature != 0x50444653) {  // "SFDP"
        return -NOR_ERR_NO_DEVICE;
    }

    return 0;
}
```

### 1.5 唯一ID读取

许多 Flash 芯片提供唯一 ID（Unique ID）读取功能，用于获取芯片的唯一序列号。这在产品防伪、固件加密、设备绑定等场景中非常有用。

```c
/**
 * 读取芯片唯一ID (Unique ID / UID)
 * 每个芯片的UID都是唯一的，可用于产品识别
 *
 * @param dev 设备句柄
 * @param uid 输出缓冲区(至少8字节)
 * @param uid_len 输入:缓冲区长度, 输出:实际UID长度
 * @return 0 成功
 */
int nor_read_unique_id(nor_device_t *dev, uint8_t *uid, uint8_t *uid_len)
{
    int ret;
    uint8_t cmd[5] = {
        NOR_CMD_READ_UNIQUE_ID,
        0x00, 0x00, 0x00, 0x00  // 4字节地址
    };
    uint8_t dummy = 0xFF;
    uint8_t max_len = *uid_len;
    uint8_t actual_len = (max_len > 8) ? 8 : max_len;

    dev->transport->cs_select(dev->transport);

    /* 发送命令和地址 */
    ret = dev->transport->send(dev->transport, cmd, 5);
    if (ret < 0) goto exit;

    /* Dummy周期 */
    ret = dev->transport->send(dev->transport, &dummy, 1);
    if (ret < 0) goto exit;

    /* 读取UID数据 */
    ret = dev->transport->recv(dev->transport, uid, actual_len);

exit:
    dev->transport->cs_deselect(dev->transport);

    if (ret != (int)actual_len) {
        return -NOR_ERR_TRANSPORT;
    }

    *uid_len = actual_len;
    return 0;
}
```

---

## 2. 写使能与写禁止控制

写使能（Write Enable）和写禁止（Write Disable）控制是 Flash 编程的基础安全机制。Flash 芯片通过这两个命令来控制是否允许执行写操作和擦除操作，防止意外的数据修改。

### 2.1 写使能机制原理

Flash 存储单元的编程和擦除操作需要特定的高电压条件，这些操作只有在芯片内部准备好接受这些操作时才能执行。写使能命令（WREN，0x06）的作用是设置内部的状态标志位（WEL，Write Enable Latch），告知芯片接下来将执行写操作。

写使能是一个瞬时操作，不需要等待。执行写使能后，WEL 位被设置为 1，这个状态会一直保持，直到执行写禁止命令或者完成一次写/擦除操作。需要注意的是，每次执行页编程、扇区擦除、块擦除或整片擦除操作之前，都必须先执行写使能。

```c
/**
 * 写使能 (Write Enable) 操作时序
 *
 * 命令格式: 0x06
 *
 * 时序图:
 * CS# ───┐         ┌────────────────────
 *        │         │
 *        └─────────┘
 * SCK ─────────────┐    ┌────┐
 *                  └────┘
 * MOSI ────────────[0x06]
 *
 * 注意: 写使能是单字节命令，无需地址和数据
 *
 * @return 0 成功，负值错误
 */
int nor_write_enable(nor_device_t *dev)
{
    int ret;
    uint8_t cmd = NOR_CMD_WRITE_ENABLE;

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 等待之前的操作完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 片选拉低 */
    dev->transport->cs_select(dev->transport);

    /* 发送写使能命令 */
    ret = dev->transport->send(dev->transport, &cmd, 1);

    /* 片选拉高 */
    dev->transport->cs_deselect(dev->transport);

    if (ret < 0) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 可选: 验证WEL位是否设置成功 */
    ret = nor_verify_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

/**
 * 验证写使能是否成功
 * 读取状态寄存器的WEL位进行确认
 */
static int nor_verify_write_enable(nor_device_t *dev)
{
    int ret;
    uint8_t status;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;
    uint32_t timeout = 10;  // 10ms超时

    /* 轮询检查WEL位 */
    uint32_t start = nor_get_tick();

    while ((nor_get_tick() - start) < timeout) {
        dev->transport->cs_select(dev->transport);
        dev->transport->send(dev->transport, &cmd, 1);
        dev->transport->recv(dev->transport, &status, 1);
        dev->transport->cs_deselect(dev->transport);

        if (status & NOR_STATUS_WEL) {
            return 0;  // WEL位已设置
        }

        /* 让出CPU，避免busy-wait */
        nor_delay_us(10);
    }

    return -NOR_ERR_WRITE_ENABLE;
}
```

### 2.2 写禁止机制原理

写禁止命令（WRDI，0x04）用于清除 WEL 位，禁止后续的写和擦除操作。这是一个可选的操作，通常在完成一系列写操作后使用，以防止意外的数据修改。

执行写禁止后，WEL 位被清除为 0。此时芯片将忽略任何页编程、擦除和写状态寄存器命令。某些应用程序会在完成所有写操作后主动执行写禁止，以增加安全性。

```c
/**
 * 写禁止 (Write Disable) 操作时序
 *
 * 命令格式: 0x04
 *
 * 执行后清除WEL位，禁止写操作
 *
 * @return 0 成功，负值错误
 */
int nor_write_disable(nor_device_t *dev)
{
    int ret;
    uint8_t cmd = NOR_CMD_WRITE_DISABLE;

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 片选拉低 */
    dev->transport->cs_select(dev->transport);

    /* 发送写禁止命令 */
    ret = dev->transport->send(dev->transport, &cmd, 1);

    /* 片选拉高 */
    dev->transport->cs_deselect(dev->transport);

    if (ret < 0) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 等待WEL位清除 */
    ret = nor_wait_wel_clear(dev);

    return ret;
}

/**
 * 等待WEL位清除
 */
static int nor_wait_wel_clear(nor_device_t *dev)
{
    uint8_t status;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;
    uint32_t timeout = 10;
    uint32_t start = nor_get_tick();

    while ((nor_get_tick() - start) < timeout) {
        dev->transport->cs_select(dev->transport);
        dev->transport->send(dev->transport, &cmd, 1);
        dev->transport->recv(dev->transport, &status, 1);
        dev->transport->cs_deselect(dev->transport);

        if (!(status & NOR_STATUS_WEL)) {
            return 0;  // WEL位已清除
        }

        nor_delay_us(10);
    }

    return -NOR_ERR_TIMEOUT;
}
```

### 2.3 自动写禁止

某些 Flash 芯片支持自动写禁止功能（Auto-Write-Disable）。在执行完页编程或擦除操作后，芯片会自动清除 WEL 位，无需软件手动执行写禁止命令。这可以简化驱动代码，但也需要注意这种行为。

```c
/**
 * 检查芯片是否支持自动写禁止
 * 通过读取芯片特性标志判断
 */
bool nor_has_auto_write_disable(nor_device_t *dev)
{
    return (dev->flags & NOR_FLAG_AUTO_WRDI) != 0;
}

/**
 * 写操作后自动处理
 * 根据芯片特性决定是否需要写禁止
 */
int nor_post_write_action(nor_device_t *dev)
{
    /* 如果不支持自动写禁止，则手动执行 */
    if (!nor_has_auto_write_disable(dev)) {
        return nor_write_disable(dev);
    }

    return 0;
}
```

---

## 3. 编程与擦除流程

Flash 编程和擦除是 Flash 存储器的核心操作。与 RAM 不同，Flash 芯片在写入数据前必须先进行擦除操作，而且擦除操作通常以较大的块为单位进行。理解这些操作的流程和时序对于开发可靠的 Flash 驱动至关重要。

### 3.1 页编程（Page Program）流程

页编程是 Flash 最基本的写入操作。SPI Flash 的页大小通常为 256 字节，某些芯片支持 512 字节或更大的页。页编程只能在已擦除（值为 0xFF）的区域上执行，如果目标区域包含非 0xFF 的数据，写入操作可能失败或导致数据错误。

页编程的基本流程包括：等待芯片就绪、发送写使能、发送页编程命令、发送地址、发送数据、等待编程完成。下面的代码实现了完整的页编程流程。

```c
/**
 * 页编程 (Page Program) 操作时序
 *
 * 命令格式: 0x02 + 3/4字节地址 + 数据(1-256字节)
 *
 * 时序图:
 * CS# ──┐          ┌──────────────────────────────────────────
 *      │          │
 *      └──────────┘
 * SCK ───────────────────────┐  ┌────┐  ┌────┐       ┌────┐
 *                            └──┘    └──┘       └──┘
 * MOSI ──[CMD=0x02][ADDR ][ADDR ][ADDR ][DATA0]...[DATAN]
 *              0x02   A23-A16 A15-A8  A7-A0  D0     Dn
 *
 * 注意事项:
 * 1. 地址必须页对齐(低8位为0)
 * 2. 写入长度不能超过页大小
 * 3. 写入前目标区域必须为0xFF
 *
 * @param addr 起始地址(必须对齐)
 * @param data 数据缓冲区
 * @param len 写入长度(不能超过页大小)
 * @return 成功写入的字节数，负值错误
 */
int nor_page_program(nor_device_t *dev, uint32_t addr,
                     const uint8_t *data, uint32_t len)
{
    int ret;

    /* 参数检查 */
    if (!dev || !data || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 页对齐检查(低8位必须为0) */
    if ((addr % dev->page_size) + len > dev->page_size) {
        /* 某些芯片支持跨页写入，此处可根据芯片特性调整 */
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

    /* 开始页编程操作 */
    dev->transport->cs_select(dev->transport);

    /* 发送页编程命令 */
    uint8_t cmd = NOR_CMD_PAGE_PROGRAM;
    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) goto exit;

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        uint8_t addr_buf[4] = {
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        };
        ret = dev->transport->send(dev->transport, addr_buf, 4);
    } else {
        uint8_t addr_buf[3] = {
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        };
        ret = dev->transport->send(dev->transport, addr_buf, 3);
    }
    if (ret < 0) goto exit;

    /* 发送数据 */
    ret = dev->transport->send(dev->transport, data, len);

exit:
    dev->transport->cs_deselect(dev->transport);

    if (ret < 0) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 等待编程完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_PAGE_PROGRAM);
    dev->write_in_progress = 0;

    if (ret < 0) {
        return ret;
    }

    return (int)len;
}
```

### 3.2 连续写入（跨页写入）

由于页编程有大小限制，驱动需要实现连续写入功能来处理大于页大小的数据。连续写入的核心思想是将数据分割成多个页，分别进行编程。

```c
/**
 * 连续写入(跨页写入)
 * 自动处理跨页边界的数据
 *
 * @param addr 起始地址
 * @param data 数据缓冲区
 * @param len 写入长度
 * @return 成功写入的字节数
 */
int nor_write(nor_device_t *dev, uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t page_size = dev->page_size;
    uint32_t written = 0;

    if (!dev || !data || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    while (written < len) {
        /* 计算当前页的剩余空间 */
        uint32_t page_offset = addr % page_size;
        uint32_t chunk = page_size - page_offset;

        /* 限制为剩余数据量 */
        if (chunk > (len - written)) {
            chunk = len - written;
        }

        /* 执行单页编程 */
        int ret = nor_page_program(dev, addr, data + written, chunk);
        if (ret < 0) {
            return (written > 0) ? (int)written : ret;
        }

        addr += chunk;
        written += chunk;

        /* 报告进度 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((written * 100) / len);
            dev->callbacks.progress(NOR_EVENT_WRITE_PROGRESS, progress);
        }
    }

    return (int)written;
}
```

### 3.3 扇区擦除（Sector Erase）

扇区擦除是最小粒度的擦除操作，通常以 4KB 为单位。扇区擦除将指定扇区内的所有字节恢复为 0xFF 状态，是进行数据写入前的必要准备工作。

```c
/**
 * 扇区擦除 (Sector Erase) 操作时序
 *
 * 命令格式: 0x20 + 3/4字节地址
 *
 * 擦除粒度: 4KB (4096字节)
 * 典型擦除时间: 30-200ms
 *
 * 时序图:
 * CS# ──┐          ┌────────────────────────────────────────────
 *      │          │
 *      └──────────┘
 * SCK ───────────────────────┐  ┌────┐  ┌────┐
 *                              └──┘    └──┘
 * MOSI ──[CMD=0x20][ADDR ][ADDR ][ADDR ]
 *              0x20   A23-A16 A15-A8  A7-A0
 *
 * @param addr 扇区地址(必须4KB对齐)
 * @return 0 成功，负值错误
 */
int nor_erase_sector(nor_device_t *dev, uint32_t addr)
{
    int ret;

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

    /* 发送扇区擦除命令 */
    dev->transport->cs_select(dev->transport);

    uint8_t cmd = NOR_CMD_ERASE_SECTOR;
    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) goto exit;

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        uint8_t addr_buf[4] = {
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        };
        ret = dev->transport->send(dev->transport, addr_buf, 4);
    } else {
        uint8_t addr_buf[3] = {
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        };
        ret = dev->transport->send(dev->transport, addr_buf, 3);
    }

exit:
    dev->transport->cs_deselect(dev->transport);

    if (ret < 0) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 触发擦除开始回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_STARTED, 0);
    }

    /* 等待擦除完成 */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_SECTOR_ERASE);
    dev->write_in_progress = 0;

    /* 触发擦除完成回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_COMPLETED, 100);
    }

    return ret;
}
```

### 3.4 块擦除（Block Erase）

块擦除是较大粒度的擦除操作，常见的块大小有 32KB 和 64KB 两种。块擦除的速度比扇区擦除快，适合需要快速擦除大面积数据的场景。

```c
/**
 * 块擦除 (Block Erase) 操作
 *
 * 支持32KB块擦除(0x52)和64KB块擦除(0xD8)
 *
 * @param addr 块起始地址(必须块大小对齐)
 * @param block_size 块大小(32KB或64KB)
 * @return 0 成功，负值错误
 */
int nor_erase_block(nor_device_t *dev, uint32_t addr, uint32_t block_size)
{
    int ret;
    uint8_t cmd;

    /* 参数检查 */
    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 块大小和命令匹配 */
    if (block_size == 32 * 1024) {
        cmd = NOR_CMD_ERASE_BLOCK_32K;
    } else if (block_size == 64 * 1024) {
        cmd = NOR_CMD_ERASE_BLOCK_64K;
    } else {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 对齐检查 */
    if (addr % block_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

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

    /* 发送块擦除命令 */
    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) goto exit;

    /* 发送地址 */
    if (dev->addr_bytes == 4) {
        uint8_t addr_buf[4] = {
            (addr >> 24) & 0xFF,
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        };
        ret = dev->transport->send(dev->transport, addr_buf, 4);
    } else {
        uint8_t addr_buf[3] = {
            (addr >> 16) & 0xFF,
            (addr >> 8) & 0xFF,
            addr & 0xFF
        };
        ret = dev->transport->send(dev->transport, addr_buf, 3);
    }

exit:
    dev->transport->cs_deselect(dev->transport);

    if (ret < 0) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 等待擦除完成 */
    dev->write_in_progress = 1;
    uint32_t timeout = (block_size == 32 * 1024) ?
                       NOR_TIMEOUT_BLOCK_ERASE_32K :
                       NOR_TIMEOUT_BLOCK_ERASE_64K;
    ret = nor_wait_ready(dev, timeout);
    dev->write_in_progress = 0;

    return ret;
}
```

### 3.5 整片擦除（Chip Erase）

整片擦除将整个 Flash 芯片的所有存储单元恢复到初始状态（0xFF）。这是耗时最长的 Flash 操作，根据芯片容量不同，可能需要数十秒甚至更长时间。

```c
/**
 * 整片擦除 (Chip Erase) 操作
 *
 * 命令格式: 0xC7 或 0x60
 *
 * 警告: 此操作会擦除整个芯片，请谨慎使用！
 * 典型擦除时间: 10-100秒(根据芯片容量)
 *
 * @return 0 成功，负值错误
 */
int nor_erase_chip(nor_device_t *dev)
{
    int ret;
    uint8_t cmd = NOR_CMD_CHIP_ERASE;

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
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

    /* 发送整片擦除命令 */
    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, &cmd, 1);

    dev->transport->cs_deselect(dev->transport);

    if (ret < 0) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 触发擦除开始回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_STARTED, 0);
    }

    /* 等待擦除完成(超时时间较长) */
    dev->write_in_progress = 1;
    ret = nor_wait_ready(dev, NOR_TIMEOUT_CHIP_ERASE);
    dev->write_in_progress = 0;

    /* 触发擦除完成回调 */
    if (dev->callbacks.progress) {
        dev->callbacks.progress(NOR_EVENT_ERASE_COMPLETED, 100);
    }

    return ret;
}
```

### 3.6 批量擦除（智能擦除）

在实际应用中，经常需要擦除大块数据区域。为了提高效率，驱动可以实现智能批量擦除，根据擦除范围自动选择最优的擦除粒度。

```c
/**
 * 批量擦除 - 自动选择最优擦除粒度
 *
 * 算法:
 * 1. 对于大块连续区域，优先使用块擦除(64KB)
 * 2. 对于剩余部分，使用块擦除(32KB)
 * 3. 最后使用扇区擦除(4KB)处理边界
 *
 * @param addr 起始地址
 * @param len 擦除长度
 * @return 0 成功，负值错误
 */
int nor_erase(nor_device_t *dev, uint32_t addr, uint32_t len)
{
    uint32_t sector_size = dev->sector_size;
    uint32_t block_size_64k = 64 * 1024;
    uint32_t erased = 0;
    uint32_t total = len;

    if (!dev || len == 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    if (addr + len > dev->total_size) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 起始地址对齐调整 */
    while ((addr % sector_size != 0) && (erased < len)) {
        int ret = nor_erase_sector(dev, addr);
        if (ret < 0) return ret;
        addr += sector_size;
        erased += sector_size;
    }

    /* 使用64KB块擦除大块区域 */
    while ((len - erased) >= block_size_64k) {
        int ret = nor_erase_block(dev, addr, block_size_64k);
        if (ret < 0) return ret;
        addr += block_size_64k;
        erased += block_size_64k;

        /* 报告进度 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((erased * 100) / total);
            dev->callbacks.progress(NOR_EVENT_ERASE_PROGRESS, progress);
        }
    }

    /* 使用扇区擦除剩余部分 */
    while (erased < len) {
        /* 确保地址对齐 */
        uint32_t sector_addr = (addr / sector_size) * sector_size;
        int ret = nor_erase_sector(dev, sector_addr);
        if (ret < 0) return ret;

        addr += sector_size;
        erased += sector_size;

        /* 报告进度 */
        if (dev->callbacks.progress) {
            uint8_t progress = (uint8_t)((erased * 100) / total);
            dev->callbacks.progress(NOR_EVENT_ERASE_PROGRESS, progress);
        }
    }

    return 0;
}
```

---

## 4. 错误检测与处理

Flash 操作的可靠性至关重要，驱动必须实现完善的错误检测和处理机制。本节介绍 Flash 驱动中常见的错误类型、检测方法和处理策略。

### 4.1 编程/擦除错误检测

Flash 编程和擦除操作可能因为多种原因失败，包括电源不稳定、芯片损坏、写保护等。驱动需要能够检测这些错误并采取相应的处理措施。

```c
/**
 * 编程/擦除错误状态位
 * 位于状态寄存器的bit5和bit6
 */
#define NOR_STATUS_E_ERR    (1 << 5)  // Erase Error
#define NOR_STATUS_P_ERR    (1 << 6)  // Program Error

/**
 * 检查操作是否发生错误
 * 读取状态寄存器检查E_ERR和P_ERR位
 *
 * @return 0 无错误, -NOR_ERR_PROGRAM 编程错误, -NOR_ERR_ERASE 擦除错误
 */
int nor_check_operation_status(nor_device_t *dev)
{
    int ret;
    uint8_t status;

    ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    /* 检查错误位 */
    if (status & NOR_STATUS_P_ERR) {
        /* 清除错误位需要写入1到该位 */
        nor_clear_status_flags(dev, NOR_STATUS_P_ERR);
        return -NOR_ERR_PROGRAM;
    }

    if (status & NOR_STATUS_E_ERR) {
        nor_clear_status_flags(dev, NOR_STATUS_E_ERR);
        return -NOR_ERR_ERASE;
    }

    return 0;
}

/**
 * 清除状态寄存器标志位
 */
int nor_clear_status_flags(nor_device_t *dev, uint8_t flags)
{
    int ret;
    uint8_t cmd[2];

    ret = nor_write_enable(dev);
    if (ret < 0) return ret;

    /* 写1到对应位可清除(部分芯片) */
    cmd[0] = NOR_CMD_WRITE_STATUS_REG;
    cmd[1] = flags;  // 写入1清除对应标志

    dev->transport->cs_select(dev->transport);
    ret = dev->transport->send(dev->transport, cmd, 2);
    dev->transport->cs_deselect(dev->transport);

    return (ret == 2) ? 0 : -NOR_ERR_TRANSPORT;
}
```

### 4.2 数据验证

写入数据后，驱动应该提供数据验证功能，确保写入的数据与预期一致。虽然这会增加操作时间，但对于关键数据来说这是必要的。

```c
/**
 * 写入后验证
 * 读取写入的数据并与源数据对比
 *
 * @param addr 写入地址
 * @param data 源数据
 * @param len 验证长度
 * @return 0 验证成功，-NOR_ERR_VERIFY 验证失败
 */
int nor_verify_write(nor_device_t *dev, uint32_t addr,
                     const uint8_t *data, uint32_t len)
{
    int ret;
    uint8_t *read_buf;

    /* 分配临时缓冲区 */
    read_buf = (uint8_t *)malloc(len);
    if (!read_buf) {
        return -NOR_ERR_NO_MEMORY;
    }

    /* 等待Flash就绪 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        free(read_buf);
        return ret;
    }

    /* 读取数据 */
    ret = nor_read(dev, addr, read_buf, len);

    if (ret != (int)len) {
        free(read_buf);
        return -NOR_ERR_TRANSPORT;
    }

    /* 数据比对 */
    if (memcmp(data, read_buf, len) != 0) {
        free(read_buf);
        return -NOR_ERR_VERIFY;
    }

    free(read_buf);
    return 0;
}

/**
 * 带验证的写入操作
 */
int nor_write_with_verify(nor_device_t *dev, uint32_t addr,
                          const uint8_t *data, uint32_t len)
{
    int ret;

    /* 执行写入 */
    ret = nor_write(dev, addr, data, len);
    if (ret < 0) {
        return ret;
    }

    /* 执行验证 */
    ret = nor_verify_write(dev, addr, data, len);
    if (ret < 0) {
        /* 记录错误日志 */
        nor_log_error(dev, "Write verify failed at addr 0x%08X", addr);
    }

    return ret;
}
```

### 4.3 写保护检测与处理

Flash 芯片通常提供硬件和软件写保护功能。在执行写入操作前，驱动应该检查写保护状态，避免操作失败。

```c
/**
 * 写保护检测
 * 检查芯片是否被保护
 *
 * @return 0 未保护, -NOR_ERR_PROTECTED 被保护
 */
int nor_check_write_protection(nor_device_t *dev)
{
    int ret;
    uint8_t status;

    ret = nor_read_status(dev, &status);
    if (ret < 0) {
        return ret;
    }

    /* 检查块保护位(BP0, BP1, BP2) */
    if (status & (NOR_STATUS_BP0 | NOR_STATUS_BP1 | NOR_STATUS_BP2)) {
        /* 进一步判断保护范围 */
        uint8_t bp_level = (status & 0x1C) >> 2;
        if (bp_level > 0) {
            return -NOR_ERR_PROTECTED;
        }
    }

    /* 检查状态寄存器保护位 */
    if (status & NOR_STATUS_SRP0) {
        return -NOR_ERR_PROTECTED;
    }

    return 0;
}

/**
 * 解除所有写保护
 */
int nor_disable_all_protection(nor_device_t *dev)
{
    int ret;
    uint8_t cmd[2];

    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    /* 清除所有保护位 */
    cmd[0] = NOR_CMD_WRITE_STATUS_REG;
    cmd[1] = 0x00;  // 清除所有BP位和SRP位

    dev->transport->cs_select(dev->transport);
    ret = dev->transport->send(dev->transport, cmd, 2);
    dev->transport->cs_deselect(dev->transport);

    if (ret != 2) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 等待状态寄存器写入完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);

    return ret;
}
```

### 4.4 超时处理与重试机制

Flash 操作可能因为临时性故障（如总线冲突、电源波动）而失败。实现重试机制可以提高驱动在不利条件下的可靠性。

```c
/**
 * 带重试的写操作
 *
 * @param addr 写入地址
 * @param data 数据缓冲区
 * @param len 写入长度
 * @param max_retries 最大重试次数
 * @return 成功写入的字节数
 */
int nor_write_with_retry(nor_device_t *dev, uint32_t addr,
                         const uint8_t *data, uint32_t len,
                         uint8_t max_retries)
{
    int ret;
    uint8_t retries = 0;

    while (retries < max_retries) {
        ret = nor_write(dev, addr, data, len);

        if (ret >= 0) {
            /* 验证写入 */
            ret = nor_verify_write(dev, addr, data, len);
            if (ret == 0) {
                return (int)len;  // 成功
            }
        }

        retries++;

        /* 重试前等待一段时间 */
        nor_delay_ms(10 * retries);

        /* 记录重试信息 */
        nor_log_error(dev, "Write failed at 0x%08X, retry %d/%d",
                     addr, retries, max_retries);
    }

    return -NOR_ERR_WRITE_FAILED;
}

/**
 * 带重试的擦除操作
 */
int nor_erase_with_retry(nor_device_t *dev, uint32_t addr,
                         uint32_t len, uint8_t max_retries)
{
    int ret;
    uint8_t retries = 0;

    while (retries < max_retries) {
        ret = nor_erase(dev, addr, len);

        if (ret == 0) {
            /* 验证擦除: 读取数据应该为0xFF */
            uint8_t verify_buf[16];
            ret = nor_read(dev, addr, verify_buf, sizeof(verify_buf));

            if (ret > 0) {
                bool all_ff = true;
                for (size_t i = 0; i < sizeof(verify_buf); i++) {
                    if (verify_buf[i] != 0xFF) {
                        all_ff = false;
                        break;
                    }
                }
                if (all_ff) {
                    return 0;  // 擦除成功
                }
            }
        }

        retries++;

        /* 重试前执行软件复位恢复芯片状态 */
        nor_reset(dev);
        nor_delay_ms(50 * retries);
    }

    return -NOR_ERR_ERASE_FAILED;
}
```

### 4.5 错误统计与诊断

长期运行的系统应该记录 Flash 操作的错误统计信息，用于预测芯片寿命和诊断问题。

```c
/**
 * 错误统计结构
 */
typedef struct {
    uint32_t write_errors;     // 写入错误次数
    uint32_t erase_errors;    // 擦除错误次数
    uint32_t verify_errors;    // 验证失败次数
    uint32_t timeout_errors;  // 超时错误次数
    uint32_t total_writes;    // 总写入次数
    uint32_t total_erases;     // 总擦除次数
    uint32_t last_error_addr;  // 最后错误地址
    uint32_t last_error_time;  // 最后错误时间戳
} nor_error_stats_t;

/**
 * 记录错误统计
 */
void nor_record_error(nor_device_t *dev, int error_type, uint32_t addr)
{
    nor_error_stats_t *stats = (nor_error_stats_t *)dev->error_stats;

    switch (error_type) {
        case -NOR_ERR_PROGRAM:
        case -NOR_ERR_WRITE_FAILED:
            stats->write_errors++;
            break;
        case -NOR_ERR_ERASE:
        case -NOR_ERR_ERASE_FAILED:
            stats->erase_errors++;
            break;
        case -NOR_ERR_VERIFY:
            stats->verify_errors++;
            break;
        case -NOR_ERR_TIMEOUT:
            stats->timeout_errors++;
            break;
    }

    stats->last_error_addr = addr;
    stats->last_error_time = nor_get_tick();

    /* 触发错误回调 */
    if (dev->callbacks.error) {
        dev->callbacks.error(error_type,
            nor_error_string(error_type));
    }
}

/**
 * 获取错误统计信息
 */
int nor_get_error_stats(nor_device_t *dev, nor_error_stats_t *stats)
{
    if (!dev || !stats) {
        return -NOR_ERR_INVALID_PARAM;
    }

    memcpy(stats, dev->error_stats, sizeof(nor_error_stats_t));
    return 0;
}

/**
 * 重置错误统计
 */
int nor_reset_error_stats(nor_device_t *dev)
{
    if (!dev || !dev->error_stats) {
        return -NOR_ERR_INVALID_PARAM;
    }

    memset(dev->error_stats, 0, sizeof(nor_error_stats_t));
    return 0;
}
```

---

## 5. 状态寄存器操作

状态寄存器是 Flash 芯片的核心控制单元，它提供了关于芯片状态、保护信息和配置选项的实时数据。熟练掌握状态寄存器的操作是开发可靠 Flash 驱动的关键。

### 5.1 状态寄存器概述

SPI Flash 通常有一个或两个状态寄存器。状态寄存器 1 是最常用的，包含了操作进行中标志、写使能状态和块保护信息。某些芯片还有状态寄存器 2，用于 Quad 模式使能等其他功能。

```c
/**
 * SPI Flash 状态寄存器1 位定义
 *
 * +----+----+----+----+----+----+----+----+
 * | S7 | S6 | S5 | S4 | S3 | S2 | S1 | S0 |
 * +----+----+----+----+----+----+----+----+
 * |SRP0| P_ERR| E_ERR| BP2| BP1| BP0| WEL| WIP|
 *
 * Bit 7 (SRP0): Status Register Protect 0
 *               状态寄存器保护位
 * Bit 6 (P_ERR): Program Error
 *               编程错误标志
 * Bit 5 (E_ERR): Erase Error
 *               擦除错误标志
 * Bit 4 (BP2): Block Protect 2
 *              块保护位2
 * Bit 3 (BP1): Block Protect 1
 *              块保护位1
 * Bit 2 (BP0): Block Protect 0
 *              块保护位0
 * Bit 1 (WEL): Write Enable Latch
 *             写使能锁存器
 * Bit 0 (WIP): Write In Progress
 *             写操作进行中标志
 */

/* 状态寄存器1位掩码 */
#define NOR_STATUS_WIP       (1 << 0)   // 写操作进行中
#define NOR_STATUS_WEL       (1 << 1)   // 写使能锁存
#define NOR_STATUS_BP0       (1 << 2)   // 块保护位0
#define NOR_STATUS_BP1       (1 << 3)   // 块保护位1
#define NOR_STATUS_BP2       (1 << 4)   // 块保护位2
#define NOR_STATUS_E_ERR     (1 << 5)   // 擦除错误
#define NOR_STATUS_P_ERR     (1 << 6)   // 编程错误
#define NOR_STATUS_SRP0      (1 << 7)   // 状态寄存器保护

/* 状态寄存器2位定义(部分芯片) */
#define NOR_STATUS2_QE       (1 << 1)   // Quad Enable
#define NOR_STATUS2_WPS       (1 << 2)  // Write Protection Selection
#define NOR_STATUS2_DR0      (1 << 4)  // Dummy
#define NOR_STATUS2_DR1       (1 << 5)  // Dummy
```

### 5.2 读取状态寄存器

状态寄存器的读取是最高频的操作之一，主要用于轮询 WIP 位以判断 Flash 是否就绪。

```c
/**
 * 读取状态寄存器1
 *
 * 命令格式: 0x05 [响应: 状态寄存器值]
 *
 * 此命令可以在任何时候使用，包括Flash busy期间
 *
 * @param status 状态寄存器值输出
 * @return 0 成功
 */
int nor_read_status_reg(nor_device_t *dev, uint8_t *status)
{
    int ret;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;

    if (!dev || !status) {
        return -NOR_ERR_INVALID_PARAM;
    }

    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) goto exit;

    ret = dev->transport->recv(dev->transport, status, 1);

exit:
    dev->transport->cs_deselect(dev->transport);

    return (ret == 1) ? 0 : -NOR_ERR_TRANSPORT;
}

/**
 * 读取状态寄存器2
 *
 * 命令格式: 0x35 [响应: 状态寄存器2值]
 */
int nor_read_status_reg2(nor_device_t *dev, uint8_t *status)
{
    int ret;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG_2;

    if (!dev || !status) {
        return -NOR_ERR_INVALID_PARAM;
    }

    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, &cmd, 1);
    if (ret < 0) goto exit;

    ret = dev->transport->recv(dev->transport, status, 1);

exit:
    dev->transport->cs_deselect(dev->transport);

    return (ret == 1) ? 0 : -NOR_ERR_TRANSPORT;
}

/**
 * 同时读取两个状态寄存器
 */
int nor_read_both_status_regs(nor_device_t *dev, uint8_t *status1, uint8_t *status2)
{
    int ret;
    uint8_t cmd[2] = { NOR_CMD_READ_STATUS_REG, NOR_CMD_READ_STATUS_REG_2 };

    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, cmd, 2);
    if (ret < 0) goto exit;

    /* 读取两个状态寄存器的值 */
    uint8_t resp[2];
    ret = dev->transport->recv(dev->transport, resp, 2);

    if (status1) *status1 = resp[0];
    if (status2) *status2 = resp[1];

exit:
    dev->transport->cs_deselect(dev->transport);

    return (ret == 2) ? 0 : -NOR_ERR_TRANSPORT;
}
```

### 5.3 写入状态寄存器

状态寄存器的写入用于配置芯片的各种功能，如使能 Quad 模式、设置块保护等。

```c
/**
 * 写入状态寄存器
 *
 * 命令格式: 0x01 + 状态值
 *
 * 注意: 写入状态寄存器前需要先发送写使能命令(WREN)
 *
 * @param status 要写入的状态值
 * @return 0 成功
 */
int nor_write_status_reg(nor_device_t *dev, uint8_t status)
{
    int ret;
    uint8_t cmd[2] = { NOR_CMD_WRITE_STATUS_REG, status };

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, cmd, 2);

    dev->transport->cs_deselect(dev->transport);

    if (ret != 2) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 等待写入完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);

    return ret;
}

/**
 * 同时写入两个状态寄存器
 *
 * 部分芯片支持同时写入SR1和SR2
 */
int nor_write_both_status_regs(nor_device_t *dev, uint8_t status1, uint8_t status2)
{
    int ret;
    uint8_t cmd[3] = { NOR_CMD_WRITE_STATUS_REG, status1, status2 };

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    dev->transport->cs_select(dev->transport);

    ret = dev->transport->send(dev->transport, cmd, 3);

    dev->transport->cs_deselect(dev->transport);

    if (ret != 3) {
        return -NOR_ERR_TRANSPORT;
    }

    /* 等待写入完成 */
    ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);

    return ret;
}
```

### 5.4 等待 Flash 就绪（轮询机制）

Flash 的写和擦除操作是异步的，软件需要轮询状态寄存器的 WIP 位来判断操作是否完成。

```c
/**
 * 等待 Flash 就绪（轮询 WIP 位）
 *
 * 轮询机制比固定延时更高效，因为可以在Flash完成操作后立即返回
 * 同时提供更精确的超时控制
 *
 * @param timeout_ms 超时时间(毫秒)
 * @return 0 就绪, -NOR_ERR_TIMEOUT 超时
 */
int nor_wait_ready(nor_device_t *dev, uint32_t timeout_ms)
{
    uint8_t status;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;
    uint32_t start_tick;
    uint32_t elapsed;
    uint32_t poll_interval = 0;

    if (!dev) {
        return -NOR_ERR_INVALID_PARAM;
    }

    start_tick = nor_get_tick();

    /* 轮询直到WIP位为0或超时 */
    do {
        dev->transport->cs_select(dev->transport);

        dev->transport->send(dev->transport, &cmd, 1);
        dev->transport->recv(dev->transport, &status, 1);

        dev->transport->cs_deselect(dev->transport);

        /* 检查WIP位 */
        if (!(status & NOR_STATUS_WIP)) {
            /* 检查错误标志 */
            if (status & (NOR_STATUS_E_ERR | NOR_STATUS_P_ERR)) {
                /* 记录错误并清除 */
                nor_log_error(dev, "Operation error, status=0x%02X", status);
                return -NOR_ERR_OPERATION;
            }
            return 0;
        }

        elapsed = nor_get_tick() - start_tick;

        /* 可变轮询间隔: 初始快速轮询，后续逐渐增加间隔 */
        if (poll_interval < 1000) {
            poll_interval += 10;
        }

        if (elapsed >= timeout_ms) {
            return -NOR_ERR_TIMEOUT;
        }

        /* 调用空闲回调，可用于处理后台任务 */
        if (dev->callbacks.idle) {
            dev->callbacks.idle();
        }

        nor_delay_us(poll_interval);

    } while (1);
}

/**
 * 快速等待就绪（适合短操作）
 * 使用固定的短延时
 */
int nor_wait_ready_fast(nor_device_t *dev, uint32_t timeout_ms)
{
    uint8_t status;
    uint8_t cmd = NOR_CMD_READ_STATUS_REG;
    uint32_t start = nor_get_tick();

    do {
        dev->transport->cs_select(dev->transport);
        dev->transport->send(dev->transport, &cmd, 1);
        dev->transport->recv(dev->transport, &status, 1);
        dev->transport->cs_deselect(dev->transport);

        if (!(status & NOR_STATUS_WIP)) {
            return 0;
        }

        if ((nor_get_tick() - start) >= timeout_ms) {
            return -NOR_ERR_TIMEOUT;
        }

        nor_delay_us(10);

    } while (1);
}
```

### 5.5 状态位封装函数

为了简化状态寄存器的使用，驱动应该提供封装好的状态查询函数。

```c
/**
 * 检查 Flash 是否忙碌
 *
 * @return true 忙碌, false 就绪
 */
bool nor_is_busy(nor_device_t *dev)
{
    uint8_t status;
    nor_read_status_reg(dev, &status);
    return (status & NOR_STATUS_WIP) != 0;
}

/**
 * 检查写使能状态
 *
 * @return true 已使能, false 未使能
 */
bool nor_is_write_enabled(nor_device_t *dev)
{
    uint8_t status;
    nor_read_status_reg(dev, &status);
    return (status & NOR_STATUS_WEL) != 0;
}

/**
 * 获取块保护级别
 *
 * @return 保护级别(0-7)
 */
uint8_t nor_get_protection_level(nor_device_t *dev)
{
    uint8_t status;
    nor_read_status_reg(dev, &status);
    return (status & 0x1C) >> 2;
}

/**
 * 设置块保护级别
 *
 * @param level 保护级别(0-7)
 * @return 0 成功
 */
int nor_set_protection_level(nor_device_t *dev, uint8_t level)
{
    int ret;
    uint8_t status;

    /* 读取当前状态 */
    ret = nor_read_status_reg(dev, &status);
    if (ret < 0) return ret;

    /* 清除旧保护位并设置新值 */
    status = (status & ~0x1C) | ((level & 0x07) << 2);

    /* 写入状态寄存器 */
    return nor_write_status_reg(dev, status);
}
```

### 5.6 状态寄存器完整操作示例

以下是状态寄存器的完整操作示例，展示了如何配置 Quad 模式和块保护。

```c
/**
 * 配置 Quad SPI 模式
 *
 * 需要设置状态寄存器2的QE位(Quad Enable)
 *
 * @return 0 成功
 */
int nor_enable_quad_mode(nor_device_t *dev)
{
    int ret;
    uint8_t sr1, sr2;

    /* 检查是否已使能 */
    ret = nor_read_status_reg2(dev, &sr2);
    if (ret < 0) return ret;

    if (sr2 & NOR_STATUS2_QE) {
        return 0;  // 已经使能
    }

    /* 读取当前SR1 */
    ret = nor_read_status_reg(dev, &sr1);
    if (ret < 0) return ret;

    /* 写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) return ret;

    /* 同时写入SR1和SR2，使能QE */
    sr2 |= NOR_STATUS2_QE;
    ret = nor_write_both_status_regs(dev, sr1, sr2);
    if (ret < 0) return ret;

    /* 验证设置 */
    ret = nor_read_status_reg2(dev, &sr2);
    if (ret < 0) return ret;

    if (!(sr2 & NOR_STATUS2_QE)) {
        return -NOR_ERR_HARDWARE;
    }

    /* 更新设备标志 */
    dev->flags |= NOR_FLAG_QUAD_ENABLED;

    return 0;
}

/**
 * 锁定指定地址范围
 *
 * @param start_addr 起始地址
 * @param end_addr 结束地址
 * @return 0 成功
 */
int nor_lock_range(nor_device_t *dev, uint32_t start_addr, uint32_t end_addr)
{
    uint32_t total_size = dev->total_size;
    uint8_t level;

    /* 计算保护级别 */
    if (end_addr >= total_size - 1) {
        level = 7;  // 全部保护
    } else if (end_addr >= total_size / 2) {
        level = 6;  // 上半部分
    } else if (end_addr >= total_size / 4) {
        level = 4;  // 上1/4
    } else if (end_addr >= total_size / 8) {
        level = 2;  // 上1/8
    } else {
        level = 0;  // 不保护
    }

    return nor_set_protection_level(dev, level);
}

/**
 * 解除所有保护
 */
int nor_unlock_all(nor_device_t *dev)
{
    return nor_set_protection_level(dev, 0);
}
```

---

## 本章小结

本章详细介绍了 Nor Flash 驱动的核心算法，内容涵盖芯片识别、写入控制、编程擦除流程、错误处理和状态寄存器操作等关键技术点。

**在 Read ID 与参数读取方面**，本章介绍了 JEDEC ID 的读取原理和实现方法，包括标准 ID 读取命令（0x9F）、扩展 ID 读取命令（0x90）以及 SFDP 参数发现机制。通过 ID 识别，驱动可以自动识别芯片类型并加载正确的参数配置。

**在写使能与写禁止控制方面**，本章详细说明了 WREN（0x06）和 WRDI（0x04）命令的作用机制，以及 WEL 位的工作原理。每次执行写或擦除操作前都必须先发送写使能命令，这是保证操作正确性的关键步骤。

**在编程与擦除流程方面**，本章全面介绍了页编程（0x02）、扇区擦除（0x20）、块擦除（0x52/0xD8）和整片擦除（0xC7）等操作的时序和实现代码。连续写入和智能批量擦除的实现使得驱动能够高效处理大数据量操作。

**在错误检测与处理方面**，本章介绍了状态寄存器错误位检测、数据验证、写保护检测和超时重试等机制。完善的错误处理是保证 Flash 驱动可靠性的关键。

**在状态寄存器操作方面**，本章详细说明了状态寄存器的位定义、读取方法（0x05）、写入方法（0x01）以及轮询等待机制。状态寄存器是 Flash 芯片的核心控制接口，熟练掌握其操作对于开发高质量驱动至关重要。

通过本章的学习，开发者应该能够深入理解 Flash 芯片的工作原理，掌握驱动开发的核心技术，为构建稳定可靠的 Flash 存储系统奠定坚实基础。

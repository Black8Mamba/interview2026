# CFI接口驱动实现

CFI（Common Flash Interface，通用Flash接口）驱动是并行Nor Flash软件栈的核心组件，负责通过标准化的查询机制获取芯片参数、构建扇区地图，并向上层驱动提供统一的操作接口。本章详细介绍CFI驱动的架构设计、查询命令序列实现、设备识别与参数解析、扇区地图构建方法，以及完整的驱动代码示例。

---

## 1. CFI驱动架构设计

CFI驱动位于驱动框架的中间层，向上对接通用驱动接口，向下调用传输层完成硬件操作。良好的架构设计能够确保驱动的可扩展性、可维护性和跨平台兼容性。

### 1.1 驱动层次结构

CFI驱动在整体驱动架构中的位置决定了其职责范围。它需要处理来自上层驱动的参数查询请求，同时管理底层硬件访问的复杂性。

```
┌─────────────────────────────────────────────────────────────┐
│                      应用层 (Application Layer)              │
│              文件系统、固件升级、参数存储等业务逻辑            │
└─────────────────────────────┬───────────────────────────────┘
                              │ 标准设备API
┌─────────────────────────────▼───────────────────────────────┐
│                      通用驱动层 (Generic Driver Layer)        │
│         nor_read()/nor_write()/nor_erase() 等通用接口        │
└─────────────────────────────┬───────────────────────────────┘
                              │ 芯片特定参数查询
┌─────────────────────────────▼───────────────────────────────┐
│                    CFI驱动层 (CFI Driver Layer)              │
│    CFI查询、参数解析、扇区地图构建、设备识别                  │
└─────────────────────────────┬───────────────────────────────┘
                              │ 传输接口抽象
┌─────────────────────────────▼───────────────────────────────┐
│                    传输层 (Transport Layer)                  │
│              FSMC/FMC并行传输、GPIO模拟时序                   │
└─────────────────────────────┬───────────────────────────────┘
                              │ 硬件寄存器操作
┌─────────────────────────────▼───────────────────────────────┐
│                      硬件层 (Hardware Layer)                 │
│              FSMC控制器、GPIO引脚配置                         │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 模块划分设计

CFI驱动内部可划分为四个主要模块：查询控制模块、参数解析模块、扇区管理模块和设备识别模块。各模块职责清晰，通过明确定义的接口进行交互。

**查询控制模块**负责管理CFI查询的整个生命周期，包括进入查询模式、读取查询数据、退出查询模式等操作。该模块需要处理不同芯片的查询时序差异，并提供统一的查询接口供其他模块调用。

**参数解析模块**负责将原始查询数据转换为结构化的参数信息。该模块解析CFI数据结构中的各项参数，包括容量编码、接口类型、时序参数、电压范围等，并将解析结果存储到标准化的数据结构中。

**扇区管理模块**基于解析得到的擦除块区域信息构建扇区地图。扇区地图是Flash读写的关键基础，它记录了每个扇区的起始地址、大小和属性信息，用于地址到扇区的映射计算。

**设备识别模块**负责识别Flash芯片的厂商和型号。该模块读取厂商ID和设备ID，与已知芯片数据库进行匹配，以获取芯片的详细规格信息。

### 1.3 数据流设计

CFI驱动的数据流设计决定了信息的传递方式和处理顺序。典型的数据流从驱动初始化开始，依次完成查询触发、参数解析、扇区构建，最终形成可供上层使用的设备描述结构。

```c
/**
 * CFI驱动数据流结构
 */
typedef struct {
    /* 输入：硬件访问接口 */
    nor_transport_t *transport;         // 传输层接口
    uint32_t flash_base;                // Flash基址
    uint8_t addr_mode;                  // 地址模式：x8/x16

    /* 中间处理：查询控制 */
    uint8_t query_triggered;            // 查询是否已触发
    uint32_t cfi_data_base;             // CFI数据基址

    /* 输出：解析后的参数 */
    cfi_geometry_t geometry;            // 几何参数
    cfi_timing_t timing;                // 时序参数
    cfi_voltage_t voltage;              // 电压参数

    /* 输出：设备信息 */
    cfi_device_info_t device_info;      // 设备信息

    /* 输出：扇区地图 */
    nor_sector_map_t sector_map;       // 扇区地图

    /* 状态标志 */
    uint8_t initialized;                // 初始化完成标志
    uint8_t cfi_valid;                  // CFI数据有效标志
} cfi_driver_data_t;
```

---

## 2. CFI查询命令序列实现

CFI查询命令序列是获取芯片参数的关键操作。不同的Flash芯片可能采用略有不同的查询序列，但都遵循JEDEC标准定义的基本流程。本节详细介绍标准查询序列的实现方法。

### 2.1 查询进入序列

进入CFI查询模式需要按照特定的操作顺序执行，这个过程涉及地址信号、数据信号和控制信号的协同配合。

```c
/**
 * CFI查询命令定义
 */
#define CFI_CMD_QUERY           0x98    // CFI查询命令
#define CFI_CMD_RESET           0xF0    // 复位命令
#define CFI_CMD_READ_ID        0x90    // 读取ID命令（某些芯片）

/**
 * CFI查询地址定义（x8模式）
 */
#define CFI_QUERY_ADDR_X8       0x55    // x8模式查询地址

/**
 * CFI查询地址定义（x16模式）
 */
#define CFI_QUERY_ADDR_X16     0xAA    // x16模式查询地址

/**
 * 进入CFI查询模式
 * @param driver CFI驱动数据
 * @return 0 成功，负值失败
 */
int cfi_enter_query(cfi_driver_data_t *driver)
{
    uint32_t flash_base = driver->flash_base;
    uint32_t query_addr;
    int ret;

    /* 等待Flash就绪 */
    ret = cfi_wait_ready(driver, CFI_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 根据地址模式计算查询地址 */
    if (driver->addr_mode == 16) {
        query_addr = flash_base + CFI_QUERY_ADDR_X16;
    } else {
        query_addr = flash_base + CFI_QUERY_ADDR_X8;
    }

    /* 发送CFI查询命令 */
    /* 对于FSMC接口，直接写地址即可触发写操作 */
    nor_write_word(driver->transport, query_addr, CFI_CMD_QUERY);

    /* 等待查询数据就绪 */
    /* 某些芯片需要短暂的内部处理时间 */
    cfi_delay_us(1);

    driver->query_triggered = 1;

    return 0;
}
```

### 2.2 查询数据读取

成功进入查询模式后，可以从特定的地址读取CFI数据。查询数据的地址映射因芯片而异，但通常遵循一定的规律。

```c
/**
 * CFI数据结构偏移量定义
 */
#define CFI_OFFSET_QRY          0x00    // QRY签名
#define CFI_OFFSET_PID          0x03    // 主算法ID
#define CFI_OFFSET_AID          0x05    // 备用算法ID
#define CFI_OFFSET_VERSION      0x07    // 版本号
#define CFI_OFFSET_BOOT         0x08    // 引导配置
#define CFI_OFFSET_NUM_REGIONS  0x0C    // 擦除区域数量
#define CFI_OFFSET_REGION1      0x0D    // 区域1信息
#define CFI_OFFSET_INTERFACE    0x13    // 接口类型
#define CFI_OFFSET_SIZE         0x27    // 容量编码

/**
 * 读取CFI查询数据
 * @param driver CFI驱动数据
 * @param offset 偏移量
 * @return 读取的数据（字节）
 */
uint8_t cfi_read_byte(cfi_driver_data_t *driver, uint32_t offset)
{
    uint32_t addr;

    /* 计算实际读取地址 */
    /* CFI数据通常从基址+0x10开始 */
    if (!driver->cfi_data_base) {
        driver->cfi_data_base = driver->flash_base + 0x10;
    }

    addr = driver->cfi_data_base + offset;

    /* 通过传输层读取字节 */
    return nor_read_byte(driver->transport, addr);
}

/**
 * 读取CFI查询字数据（16位）
 * @param driver CFI驱动数据
 * @param offset 偏移量
 * @return 读取的数据（字）
 */
uint16_t cfi_read_word(cfi_driver_data_t *driver, uint32_t offset)
{
    uint16_t low, high;

    /* 读取低字节 */
    low = cfi_read_byte(driver, offset);

    /* 读取高字节 */
    high = cfi_read_byte(driver, offset + 1);

    /* 小端格式组合 */
    return (high << 8) | low;
}
```

### 2.3 查询退出序列

完成CFI参数读取后，必须正确退出查询模式才能进行正常的Flash读写操作。

```c
/**
 * 退出CFI查询模式
 * @param driver CFI驱动数据
 * @return 0 成功，负值失败
 */
int cfi_exit_query(cfi_driver_data_t *driver)
{
    uint32_t flash_base = driver->flash_base;

    /* 发送复位命令退出查询模式 */
    nor_write_byte(driver->transport, flash_base, CFI_CMD_RESET);

    /* 等待Flash恢复到正常模式 */
    cfi_delay_us(10);

    /* 等待Flash就绪 */
    int ret = cfi_wait_ready(driver, CFI_TIMEOUT_DEFAULT);

    driver->query_triggered = 0;

    return ret;
}

/**
 * 完整的CFI查询流程
 * @param driver CFI驱动数据
 * @return 0 成功，负值失败
 */
int cfi_perform_query(cfi_driver_data_t *driver)
{
    int ret;

    /* 进入查询模式 */
    ret = cfi_enter_query(driver);
    if (ret < 0) {
        return ret;
    }

    /* 验证QRY签名 */
    ret = cfi_verify_signature(driver);
    if (ret < 0) {
        cfi_exit_query(driver);
        return ret;
    }

    /* 读取并解析CFI参数 */
    ret = cfi_parse_parameters(driver);
    if (ret < 0) {
        cfi_exit_query(driver);
        return ret;
    }

    /* 退出查询模式 */
    ret = cfi_exit_query(driver);
    if (ret < 0) {
        return ret;
    }

    driver->cfi_valid = 1;

    return 0;
}
```

### 2.4 查询等待与超时处理

Flash芯片在执行擦除或编程操作时无法响应CFI查询，因此查询前需要确保芯片处于就绪状态。

```c
/**
 * 等待Flash就绪
 * @param driver CFI驱动数据
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 就绪，-ETIMEDOUT 超时
 */
int cfi_wait_ready(cfi_driver_data_t *driver, uint32_t timeout_ms)
{
    uint32_t start_time;
    uint8_t status;
    uint32_t flash_base = driver->flash_base;

    start_time = cfi_get_tick();

    while (1) {
        /* 读取状态寄存器 */
        /* 注意：某些CFI芯片在查询模式下无法读取状态 */
        /* 这种情况下需要先退出查询模式 */

        if (driver->query_triggered) {
            /* 在查询模式下，尝试读取特定地址的状态 */
            status = nor_read_byte(driver->transport, flash_base + 0x00);

            /* 检查状态位（因芯片而异） */
            if ((status & 0x80) || (status == 0xFF)) {
                return 0;
            }
        } else {
            /* 正常模式下读取状态寄存器 */
            status = nor_read_byte(driver->transport, flash_base);

            /* 检查就绪位 */
            if (status & 0x80) {
                return 0;
            }
        }

        /* 检查超时 */
        if (cfi_get_tick() - start_time >= timeout_ms) {
            return -NOR_ERR_TIMEOUT;
        }

        /* 让出CPU（可选） */
        cfi_delay_us(1);
    }
}

/**
 * 获取系统 tick（需平台实现）
 */
uint32_t cfi_get_tick(void);

/**
 * 微秒延时（需平台实现）
 */
void cfi_delay_us(uint32_t us);
```

---

## 3. 设备识别与参数解析

设备识别与参数解析是CFI驱动的核心功能。通过解析CFI数据，可以获取芯片的厂商信息、容量参数、接口类型、时序要求等关键信息。

### 3.1 QRY签名验证

QRY签名是判断Flash芯片是否支持CFI标准的首要依据。只有在查询数据中正确读取到"QRY"签名，才能继续进行参数解析。

```c
/**
 * QRY签名定义
 */
#define CFI_SIGNATURE_Q         0x51    // 'Q'的ASCII码
#define CFI_SIGNATURE_R         0x52    // 'R'的ASCII码
#define CFI_SIGNATURE_Y         0x59    // 'Y'的ASCII码

/**
 * 验证CFI QRY签名
 * @param driver CFI驱动数据
 * @return 0 签名有效，-NOR_ERR_NO_DEVICE 无效
 */
int cfi_verify_signature(cfi_driver_data_t *driver)
{
    uint8_t q, r, y;

    /* 读取QRY签名 */
    q = cfi_read_byte(driver, CFI_OFFSET_QRY + 0);
    r = cfi_read_byte(driver, CFI_OFFSET_QRY + 1);
    y = cfi_read_byte(driver, CFI_OFFSET_QRY + 2);

    /* 验证签名 */
    if (q != CFI_SIGNATURE_Q || r != CFI_SIGNATURE_R || y != CFI_SIGNATURE_Y) {
        return -NOR_ERR_NO_DEVICE;
    }

    return 0;
}
```

### 3.2 设备ID解析

设备ID解析用于识别具体的Flash芯片型号，包括厂商ID和设备ID两部分。这些信息对于查找芯片详细规格和兼容参数表非常重要。

```c
/**
 * 厂商ID定义（JEDEC分配）
 */
#define VENDOR_AMD              0x01    // AMD/Spansion
#define VENDOR_INTEL            0x89    // Intel
#define VENDOR_MICRON           0x20    // Micron
#define VENDOR_MACRONIX         0x37    // Macronix
#define VENDOR_WINBOND          0x40    // Winbond
#define VENDOR_EON              0x1C    // EON
#define VENDOR_ISSI             0x1D    // ISSI
#define VENDOR_BOYA             0x68    // Boya

/**
 * 厂商名称映射表
 */
static const char* vendor_names[] = {
    [0x01] = "AMD/Spansion",
    [0x1C] = "EON",
    [0x1D] = "ISSI",
    [0x1F] = "Atmel",
    [0x20] = "Micron",
    [0x37] = "Macronix",
    [0x40] = "Winbond",
    [0x68] = "Boya",
};

/**
 * CFI设备信息结构
 */
typedef struct {
    uint16_t vendor_id;              // 厂商ID
    uint16_t device_id;              // 设备ID
    uint16_t pri_algo_id;            // 主算法ID
    uint16_t alt_algo_id;            // 备用算法ID
    uint8_t version_major;           // 版本号（主）
    uint8_t version_minor;           // 版本号（次）
    const char *vendor_name;         // 厂商名称
    char device_name[32];            // 设备名称（预留）
} cfi_device_info_t;

/**
 * 解析设备识别信息
 * @param driver CFI驱动数据
 * @param info 设备信息输出
 * @return 0 成功，负值失败
 */
int cfi_parse_device_info(cfi_driver_data_t *driver, cfi_device_info_t *info)
{
    uint16_t vendor_id, device_id;

    /* 读取厂商ID和设备ID */
    vendor_id = cfi_read_byte(driver, CFI_OFFSET_PID);
    device_id = cfi_read_word(driver, CFI_OFFSET_PID + 1);

    /* 填充基本信息 */
    info->vendor_id = vendor_id;
    info->device_id = device_id;

    /* 读取算法ID */
    info->pri_algo_id = cfi_read_word(driver, CFI_OFFSET_PID);
    info->alt_algo_id = cfi_read_word(driver, CFI_OFFSET_AID);

    /* 读取版本号 */
    info->version_minor = cfi_read_byte(driver, CFI_OFFSET_VERSION);
    info->version_major = cfi_read_byte(driver, CFI_OFFSET_VERSION + 1);

    /* 获取厂商名称 */
    if (vendor_id < sizeof(vendor_names) / sizeof(vendor_names[0])) {
        info->vendor_name = vendor_names[vendor_id];
    } else {
        info->vendor_name = "Unknown";
    }

    /* 构建设备名称（可根据需要扩展） */
    snprintf(info->device_name, sizeof(info->device_name),
             "%s Device 0x%04X", info->vendor_name, device_id);

    return 0;
}
```

### 3.3 容量参数解析

芯片容量是Flash最基本的参数之一，CFI使用2的幂次方编码方式存储容量信息。

```c
/**
 * 解析芯片容量
 * @param driver CFI驱动数据
 * @return 容量（字节）
 */
uint32_t cfi_parse_capacity(cfi_driver_data_t *driver)
{
    uint8_t size_code;

    /* 读取容量编码 */
    size_code = cfi_read_byte(driver, CFI_OFFSET_SIZE);

    /* 计算实际容量: 2^N 字节 */
    return 1UL << size_code;
}

/**
 * 容量格式化输出
 * @param capacity 容量（字节）
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 */
void cfi_format_capacity(uint32_t capacity, char *buffer, size_t buffer_size)
{
    if (capacity >= (1UL << 30)) {
        snprintf(buffer, buffer_size, "%u.%u GB",
                 (capacity >> 30), ((capacity >> 20) % 1024));
    } else if (capacity >= (1UL << 20)) {
        snprintf(buffer, buffer_size, "%u.%u MB",
                 (capacity >> 20), ((capacity >> 10) % 1024));
    } else if (capacity >= (1UL << 10)) {
        snprintf(buffer, buffer_size, "%u KB", capacity >> 10);
    } else {
        snprintf(buffer, buffer_size, "%u B", capacity);
    }
}
```

### 3.4 接口类型解析

接口类型参数标识Flash芯片与主控系统之间的通信方式，包括x8、x16、x32等不同数据宽度。

```c
/**
 * 接口类型定义
 */
#define CFI_INTERFACE_X8        0x0000  // 仅8位
#define CFI_INTERFACE_X16       0x0001  // 仅16位
#define CFI_INTERFACE_X8_X16    0x0002  // 8位/16位可选
#define CFI_INTERFACE_X32       0x0003  // 32位

/**
 * 接口类型名称映射
 */
static const char* interface_names[] = {
    "x8", "x16", "x8/x16", "x32",
    "x16 Multiplexed", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "x32 Multiplexed"
};

/**
 * 解析接口类型
 * @param driver CFI驱动数据
 * @return 接口类型编码
 */
uint16_t cfi_parse_interface(cfi_driver_data_t *driver)
{
    return cfi_read_word(driver, CFI_OFFSET_INTERFACE);
}

/**
 * 获取接口类型名称
 * @param interface 接口类型编码
 * @return 接口类型名称
 */
const char* cfi_get_interface_name(uint16_t interface)
{
    if (interface < sizeof(interface_names) / sizeof(interface_names[0])) {
        return interface_names[interface];
    }
    return "Unknown";
}
```

### 3.5 时序参数解析

时序参数定义了Flash芯片对访问时间的要求，包括编程时间和擦除时间等。这些参数对于配置FSMC时序至关重要。

```c
/**
 * CFI时序参数结构
 */
typedef struct {
    uint8_t max_word_prog;           // 最大字编程时间编码
    uint16_t typ_word_prog;         // 典型字编程时间编码
    uint16_t typ_buf_prog;          // 典型缓冲编程时间编码
    uint16_t typ_block_erase;       // 典型块擦除时间编码
    uint16_t max_block_erase;       // 最大块擦除时间编码
    uint16_t typ_chip_erase;        // 典型整片擦除时间编码

    /* 解码后的时间值（微秒/毫秒） */
    uint32_t max_word_prog_us;
    uint32_t typ_word_prog_us;
    uint32_t typ_block_erase_ms;
    uint32_t max_block_erase_ms;
} cfi_timing_t;

/**
 * 解析时序参数
 * @param driver CFI驱动数据
 * @param timing 时序参数输出
 * @return 0 成功
 */
int cfi_parse_timing(cfi_driver_data_t *driver, cfi_timing_t *timing)
{
    /* 读取编码值 */
    timing->max_word_prog = cfi_read_byte(driver, 0x30);
    timing->typ_word_prog = cfi_read_word(driver, 0x31);
    timing->typ_buf_prog = cfi_read_word(driver, 0x2F);
    timing->typ_block_erase = cfi_read_word(driver, 0x2D);
    timing->max_block_erase = cfi_read_word(driver, 0x2B);
    timing->typ_chip_erase = cfi_read_word(driver, 0x29);

    /* 解码为实际时间值 */
    /* 编程时间：2^N 微秒 */
    timing->max_word_prog_us = 1UL << timing->max_word_prog;
    timing->typ_word_prog_us = 1UL << timing->typ_word_prog;

    /* 擦除时间：2^N 毫秒 */
    timing->typ_block_erase_ms = 1UL << timing->typ_block_erase;
    timing->max_block_erase_ms = 1UL << timing->max_block_erase;

    return 0;
}
```

### 3.6 电压参数解析

电压参数定义了Flash芯片的工作电压范围，用于电源设计和电压兼容性检查。

```c
/**
 * CFI电压参数结构
 */
typedef struct {
    uint8_t vcc_min;                // 最小Vcc（0.1V单位）
    uint8_t vcc_max;                // 最大Vcc（0.1V单位）
    uint8_t vpp_min;                // 最小Vpp（0.1V单位）
    uint8_t vpp_max;                // 最大Vpp（0.1V单位）
    uint8_t vol_range;              // 电压范围标志
} cfi_voltage_t;

/**
 * 解析电压参数
 * @param driver CFI驱动数据
 * @param voltage 电压参数输出
 * @return 0 成功
 */
int cfi_parse_voltage(cfi_driver_data_t *driver, cfi_voltage_t *voltage)
{
    voltage->vcc_min = cfi_read_byte(driver, 0x1B);
    voltage->vcc_max = cfi_read_byte(driver, 0x1D);
    voltage->vpp_min = cfi_read_byte(driver, 0x1F);
    voltage->vpp_max = cfi_read_byte(driver, 0x21);
    voltage->vol_range = cfi_read_byte(driver, 0x18);

    return 0;
}
```

---

## 4. 扇区地图构建

扇区地图是Flash读写的核心基础，它描述了Flash存储空间的划分方式，包括各扇区的起始地址、大小和属性。正确构建扇区地图对于实现正确的地址映射和擦除操作至关重要。

### 4.1 擦除块区域结构

CFI标准使用擦除块区域（Erase Block Region）的概念来描述Flash的划分方式。每个区域包含若干个大小相同的擦除块，整个Flash芯片可以包含多个不同块大小的区域。

```c
/**
 * 擦除块区域结构
 */
typedef struct {
    uint32_t block_size;           // 块大小（字节）
    uint32_t block_count;           // 块数量
    uint32_t region_size;           // 区域总大小
    uint32_t start_addr;            // 起始地址（构建后填充）
} cfi_erase_region_t;

/**
 * CFI几何参数结构
 */
typedef struct {
    uint32_t total_capacity;        // 总容量
    uint8_t num_regions;           // 区域数量
    cfi_erase_region_t regions[4]; // 最多4个区域
} cfi_geometry_t;
```

### 4.2 区域信息解析

从CFI数据中解析擦除块区域信息是构建扇区地图的关键步骤。

```c
/**
 * 解析擦除块区域信息
 * @param driver CFI驱动数据
 * @param geometry 几何参数输出
 * @return 0 成功，负值失败
 */
int cfi_parse_geometry(cfi_driver_data_t *driver, cfi_geometry_t *geometry)
{
    uint8_t num_regions;
    uint16_t block_size_code;
    uint16_t block_count;
    uint32_t offset;
    uint32_t current_addr = 0;

    /* 读取区域数量 */
    num_regions = cfi_read_byte(driver, CFI_OFFSET_NUM_REGIONS);
    geometry->num_regions = num_regions > 4 ? 4 : num_regions;

    /* 读取各区域信息 */
    offset = CFI_OFFSET_NUM_REGIONS + 1;
    geometry->total_capacity = 0;

    for (int i = 0; i < geometry->num_regions; i++) {
        /* 读取块大小编码 */
        block_size_code = cfi_read_byte(driver, offset);
        geometry->regions[i].block_size = 1UL << block_size_code;

        /* 读取块数量 */
        block_count = cfi_read_word(driver, offset + 1);
        geometry->regions[i].block_count = block_count;

        /* 计算区域大小 */
        geometry->regions[i].region_size =
            (uint32_t)block_count * geometry->regions[i].block_size;

        /* 设置起始地址 */
        geometry->regions[i].start_addr = current_addr;

        /* 累加到总容量 */
        geometry->total_capacity += geometry->regions[i].region_size;

        /* 更新当前地址 */
        current_addr += geometry->regions[i].region_size;

        /* 移动到下一个区域 */
        offset += 4;
    }

    return 0;
}

/**
 * 打印几何信息（调试用）
 * @param geometry 几何参数
 */
void cfi_dump_geometry(const cfi_geometry_t *geometry)
{
    printf("Flash Geometry:\n");
    printf("  Total Capacity: %u bytes (%.2f MB)\n",
           geometry->total_capacity,
           (float)geometry->total_capacity / (1024 * 1024));
    printf("  Number of Regions: %u\n", geometry->num_regions);

    for (int i = 0; i < geometry->num_regions; i++) {
        printf("  Region %d:\n", i + 1);
        printf("    Block Size: %u KB (0x%X)\n",
               geometry->regions[i].block_size >> 10,
               geometry->regions[i].block_size);
        printf("    Block Count: %u\n",
               geometry->regions[i].block_count);
        printf("    Region Size: %u KB\n",
               geometry->regions[i].region_size >> 10);
    }
}
```

### 4.3 扇区地图构建

基于解析得到的区域信息，构建完整的扇区地图，用于地址到扇区的快速映射。

```c
#define MAX_SECTORS  256  // 最大扇区数量

/**
 * 扇区信息结构
 */
typedef struct {
    uint32_t start_addr;            // 起始地址
    uint32_t end_addr;              // 结束地址（不含）
    uint32_t size;                  // 扇区大小
    uint16_t region_id;            // 所属区域ID
    uint16_t sector_index;         // 扇区在区域内的索引
    uint8_t  is_eraseable;         // 是否可擦除
} nor_sector_info_t;

/**
 * 扇区地图结构
 */
typedef struct {
    uint32_t total_sectors;         // 总扇区数
    uint32_t total_size;            // 总大小
    nor_sector_info_t sectors[MAX_SECTORS]; // 扇区信息数组
} nor_sector_map_t;

/**
 * 构建扇区地图
 * @param geometry 几何参数
 * @param sector_map 扇区地图输出
 * @return 0 成功，负值失败
 */
int cfi_build_sector_map(const cfi_geometry_t *geometry, nor_sector_map_t *sector_map)
{
    uint32_t sector_idx = 0;
    uint32_t current_addr = 0;

    sector_map->total_size = geometry->total_capacity;
    sector_map->total_sectors = 0;

    /* 遍历所有区域 */
    for (int r = 0; r < geometry->num_regions; r++) {
        const cfi_erase_region_t *region = &geometry->regions[r];

        /* 遍历区域内的所有块 */
        for (uint32_t b = 0; b < region->block_count; b++) {
            if (sector_idx >= MAX_SECTORS) {
                return -NOR_ERR_OUT_OF_RANGE;
            }

            /* 填充扇区信息 */
            sector_map->sectors[sector_idx].start_addr = current_addr;
            sector_map->sectors[sector_idx].end_addr = current_addr + region->block_size;
            sector_map->sectors[sector_idx].size = region->block_size;
            sector_map->sectors[sector_idx].region_id = r;
            sector_map->sectors[sector_idx].sector_index = b;
            sector_map->sectors[sector_idx].is_eraseable = 1;

            /* 更新地址和索引 */
            current_addr += region->block_size;
            sector_idx++;
        }
    }

    sector_map->total_sectors = sector_idx;

    return 0;
}

/**
 * 根据地址查找扇区
 * @param sector_map 扇区地图
 * @param addr 目标地址
 * @return 扇区索引，未找到返回-1
 */
int nor_find_sector_by_addr(const nor_sector_map_t *sector_map, uint32_t addr)
{
    /* 二分查找优化 */
    int left = 0;
    int right = sector_map->total_sectors - 1;

    while (left <= right) {
        int mid = (left + right) / 2;
        const nor_sector_info_t *sector = &sector_map->sectors[mid];

        if (addr < sector->start_addr) {
            right = mid - 1;
        } else if (addr >= sector->end_addr) {
            left = mid + 1;
        } else {
            return mid;
        }
    }

    return -1;
}

/**
 * 地址对齐检查
 * @param addr 目标地址
 * @param size 对齐大小
 * @return 对齐后的地址
 */
uint32_t nor_align_addr(uint32_t addr, uint32_t size)
{
    return (addr + size - 1) & ~(size - 1);
}

/**
 * 计算擦除所需的扇区范围
 * @param sector_map 扇区地图
 * @param addr 起始地址
 * @param len 长度
 * @param start_sector 起始扇区输出
 * @param end_sector 结束扇区输出
 * @return 0 成功，负值失败
 */
int nor_get_erase_range(const nor_sector_map_t *sector_map,
                        uint32_t addr, uint32_t len,
                        int *start_sector, int *end_sector)
{
    int start = nor_find_sector_by_addr(sector_map, addr);
    int end = nor_find_sector_by_addr(sector_map, addr + len - 1);

    if (start < 0 || end < 0) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    *start_sector = start;
    *end_sector = end;

    return 0;
}
```

### 4.4 扇区保护处理

某些Flash芯片支持扇区保护功能，可以将特定扇区设置为只读或不可擦除状态。驱动需要考虑这些保护属性。

```c
/**
 * 扇区属性标志
 */
#define SECTOR_FLAG_ERASEABLE    0x01   // 可擦除
#define SECTOR_FLAG_WRITABLE    0x02   // 可写
#define SECTOR_FLAG_PROTECTED   0x04   // 受保护
#define SECTOR_FLAG_BOOT        0x08   // 引导扇区

/**
 * 更新扇区属性
 * @param sector_map 扇区地图
 * @param sector_idx 扇区索引
 * @param flags 属性标志
 * @return 0 成功
 */
int nor_sector_set_flags(nor_sector_map_t *sector_map,
                        uint32_t sector_idx, uint8_t flags)
{
    if (sector_idx >= sector_map->total_sectors) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    sector_map->sectors[sector_idx].is_eraseable = (flags & SECTOR_FLAG_ERASEABLE) ? 1 : 0;

    return 0;
}

/**
 * 检查扇区是否可擦除
 * @param sector_map 扇区地图
 * @param sector_idx 扇区索引
 * @return 1 可擦除，0 不可擦除
 */
int nor_sector_is_eraseable(const nor_sector_map_t *sector_map, uint32_t sector_idx)
{
    if (sector_idx >= sector_map->total_sectors) {
        return 0;
    }

    return sector_map->sectors[sector_idx].is_eraseable;
}
```

---

## 5. 驱动代码示例

本节提供完整的CFI驱动实现代码，展示了如何将前述各个模块组合成一个功能完整的驱动组件。

### 5.1 驱动头文件定义

```c
/**
 * @file cfi_driver.h
 * @brief CFI接口驱动头文件
 * @version 1.0.0
 */

#ifndef __CFI_DRIVER_H
#define __CFI_DRIVER_H

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

/* CFI命令 */
#define CFI_CMD_QUERY           0x98
#define CFI_CMD_RESET           0xF0
#define CFI_CMD_READ_STATUS     0x70
#define CFI_CMD_CLEAR_STATUS    0x50

/* CFI地址 */
#define CFI_QUERY_ADDR_X8       0x55
#define CFI_QUERY_ADDR_X16      0xAA

/* CFI偏移量 */
#define CFI_OFFSET_QRY          0x00
#define CFI_OFFSET_PID          0x03
#define CFI_OFFSET_AID          0x05
#define CFI_OFFSET_VERSION      0x07
#define CFI_OFFSET_NUM_REGIONS  0x0C
#define CFI_OFFSET_INTERFACE    0x13
#define CFI_OFFSET_SIZE         0x27

/* 超时时间 */
#define CFI_TIMEOUT_DEFAULT     100     /* ms */
#define CFI_TIMEOUT_ERASE       30000   /* ms */
#define CFI_TIMEOUT_PROG        500     /* ms */

/*============================================================================
 * 类型定义
 *============================================================================*/

/* 前向声明 */
typedef struct cfi_driver cfi_driver_t;

/* 传输层接口 */
typedef struct {
    int (*read_byte)(void *handle, uint32_t addr, uint8_t *data);
    int (*write_byte)(void *handle, uint32_t addr, uint8_t data);
    int (*read_word)(void *handle, uint32_t addr, uint16_t *data);
    int (*write_word)(void *handle, uint32_t addr, uint16_t data);
    void (*delay_us)(uint32_t us);
    uint32_t (*get_tick)(void);
} cfi_transport_ops_t;

typedef struct {
    void *handle;
    const cfi_transport_ops_t *ops;
} cfi_transport_t;

/* 厂商信息 */
typedef struct {
    uint8_t id;
    const char *name;
} cfi_vendor_info_t;

/* 设备信息 */
typedef struct {
    uint8_t vendor_id;
    uint16_t device_id;
    uint16_t pri_algo_id;
    uint16_t alt_algo_id;
    uint8_t version_major;
    uint8_t version_minor;
    const char *vendor_name;
    char device_name[32];
} cfi_device_info_t;

/* 擦除区域 */
typedef struct {
    uint32_t block_size;
    uint32_t block_count;
    uint32_t region_size;
    uint32_t start_addr;
} cfi_erase_region_t;

/* 几何参数 */
typedef struct {
    uint32_t total_capacity;
    uint8_t num_regions;
    cfi_erase_region_t regions[4];
} cfi_geometry_t;

/* 时序参数 */
typedef struct {
    uint32_t typ_word_prog_us;
    uint32_t max_word_prog_us;
    uint32_t typ_block_erase_ms;
    uint32_t max_block_erase_ms;
} cfi_timing_t;

/* 电压参数 */
typedef struct {
    uint8_t vcc_min;
    uint8_t vcc_max;
} cfi_voltage_t;

/* 扇区信息 */
typedef struct {
    uint32_t start_addr;
    uint32_t end_addr;
    uint32_t size;
    uint16_t region_id;
    uint8_t is_eraseable;
} nor_sector_info_t;

/* 扇区地图 */
#define MAX_SECTORS  256

typedef struct {
    uint32_t total_sectors;
    uint32_t total_size;
    nor_sector_info_t sectors[MAX_SECTORS];
} nor_sector_map_t;

/* CFI驱动结构 */
struct cfi_driver {
    /* 传输层 */
    cfi_transport_t *transport;
    uint32_t flash_base;
    uint8_t addr_mode;          // 8或16

    /* 设备信息 */
    cfi_device_info_t device_info;

    /* 参数 */
    cfi_geometry_t geometry;
    cfi_timing_t timing;
    cfi_voltage_t voltage;

    /* 扇区地图 */
    nor_sector_map_t sector_map;

    /* 状态 */
    uint8_t initialized;
    uint8_t cfi_valid;
};

/*============================================================================
 * 函数声明
 *============================================================================*/

/* 驱动初始化与销毁 */
cfi_driver_t* cfi_driver_create(void);
void cfi_driver_destroy(cfi_driver_t *driver);
int cfi_driver_init(cfi_driver_t *driver, cfi_transport_t *transport,
                   uint32_t flash_base, uint8_t addr_mode);

/* CFI查询操作 */
int cfi_query(cfi_driver_t *driver);
int cfi_verify_signature(cfi_driver_t *driver);

/* 参数解析 */
int cfi_parse_device_info(cfi_driver_t *driver);
int cfi_parse_geometry(cfi_driver_t *driver);
int cfi_parse_timing(cfi_driver_t *driver);
int cfi_parse_voltage(cfi_driver_t *driver);
int cfi_parse_all(cfi_driver_t *driver);

/* 扇区地图 */
int cfi_build_sector_map(cfi_driver_t *driver);
int cfi_find_sector(cfi_driver_t *driver, uint32_t addr);

/* 工具函数 */
uint32_t cfi_get_capacity(const cfi_driver_t *driver);
const char* cfi_get_vendor_name(uint8_t vendor_id);
void cfi_dump_info(const cfi_driver_t *driver);

/* 底层传输（供内部使用） */
uint8_t cfi_read_byte(cfi_driver_t *driver, uint32_t offset);
uint16_t cfi_read_word(cfi_driver_t *driver, uint32_t offset);
int cfi_write_byte(cfi_driver_t *driver, uint32_t addr, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* __CFI_DRIVER_H */
```

### 5.2 驱动实现文件

```c
/**
 * @file cfi_driver.c
 * @brief CFI接口驱动实现
 * @version 1.0.0
 */

#include "cfi_driver.h"
#include <stdio.h>

/*============================================================================
 * 厂商信息表
 *============================================================================*/

static const cfi_vendor_info_t vendor_table[] = {
    {0x01, "AMD/Spansion"},
    {0x1C, "EON"},
    {0x1D, "ISSI"},
    {0x1F, "Atmel"},
    {0x20, "Micron"},
    {0x37, "Macronix"},
    {0x40, "Winbond"},
    {0x68, "Boya"},
    {0x00, NULL}  /* 结束标记 */
};

/*============================================================================
 * 内部函数实现
 *============================================================================*/

/**
 * 延时函数（使用传输层）
 */
static void cfi_delay_us(cfi_driver_t *driver, uint32_t us)
{
    if (driver->transport && driver->transport->ops->delay_us) {
        driver->transport->ops->delay_us(us);
    }
}

/**
 * 获取系统tick
 */
static uint32_t cfi_get_tick(cfi_driver_t *driver)
{
    if (driver->transport && driver->transport->ops->get_tick) {
        return driver->transport->ops->get_tick();
    }
    return 0;
}

/**
 * 读取字节数据
 */
uint8_t cfi_read_byte(cfi_driver_t *driver, uint32_t offset)
{
    uint8_t data = 0xFF;

    if (driver->transport && driver->transport->ops->read_byte) {
        driver->transport->ops->read_byte(driver->transport->handle,
                                         driver->flash_base + offset,
                                         &data);
    }
    return data;
}

/**
 * 读取字数据
 */
uint16_t cfi_read_word(cfi_driver_t *driver, uint32_t offset)
{
    uint16_t data = 0xFFFF;

    if (driver->transport && driver->transport->ops->read_word) {
        driver->transport->ops->read_word(driver->transport->handle,
                                         driver->flash_base + offset,
                                         &data);
    }
    return data;
}

/**
 * 写入字节数据
 */
int cfi_write_byte(cfi_driver_t *driver, uint32_t addr, uint8_t data)
{
    if (driver->transport && driver->transport->ops->write_byte) {
        return driver->transport->ops->write_byte(driver->transport->handle,
                                                   addr, data);
    }
    return -NOR_ERR_TRANSPORT;
}

/**
 * 等待Flash就绪
 */
static int cfi_wait_ready(cfi_driver_t *driver, uint32_t timeout_ms)
{
    uint32_t start = cfi_get_tick(driver);
    uint8_t status;

    while ((cfi_get_tick(driver) - start) < timeout_ms) {
        status = cfi_read_byte(driver, 0x00);
        if (status & 0x80) {
            return 0;
        }
        cfi_delay_us(driver, 100);
    }

    return -NOR_ERR_TIMEOUT;
}

/**
 * 进入CFI查询模式
 */
static int cfi_enter_query_mode(cfi_driver_t *driver)
{
    uint32_t query_addr;

    /* 计算查询地址 */
    query_addr = driver->flash_base +
                 (driver->addr_mode == 16 ? CFI_QUERY_ADDR_X16 : CFI_QUERY_ADDR_X8);

    /* 发送查询命令 */
    cfi_write_byte(driver, query_addr, CFI_CMD_QUERY);

    /* 等待就绪 */
    cfi_delay_us(driver, 1);

    return 0;
}

/**
 * 退出CFI查询模式
 */
static int cfi_exit_query_mode(cfi_driver_t *driver)
{
    /* 发送复位命令 */
    cfi_write_byte(driver, driver->flash_base, CFI_CMD_RESET);

    /* 等待恢复 */
    cfi_delay_us(driver, 10);

    return cfi_wait_ready(driver, CFI_TIMEOUT_DEFAULT);
}

/*============================================================================
 * 公共函数实现
 *============================================================================*/

/**
 * 创建CFI驱动实例
 */
cfi_driver_t* cfi_driver_create(void)
{
    cfi_driver_t *driver = (cfi_driver_t *)malloc(sizeof(cfi_driver_t));
    if (driver) {
        memset(driver, 0, sizeof(cfi_driver_t));
    }
    return driver;
}

/**
 * 销毁CFI驱动实例
 */
void cfi_driver_destroy(cfi_driver_t *driver)
{
    if (driver) {
        free(driver);
    }
}

/**
 * 初始化CFI驱动
 */
int cfi_driver_init(cfi_driver_t *driver, cfi_transport_t *transport,
                   uint32_t flash_base, uint8_t addr_mode)
{
    if (!driver || !transport) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 保存配置 */
    driver->transport = transport;
    driver->flash_base = flash_base;
    driver->addr_mode = addr_mode;

    /* 执行CFI查询 */
    int ret = cfi_query(driver);
    if (ret < 0) {
        return ret;
    }

    /* 解析所有参数 */
    ret = cfi_parse_all(driver);
    if (ret < 0) {
        return ret;
    }

    /* 构建扇区地图 */
    ret = cfi_build_sector_map(driver);
    if (ret < 0) {
        return ret;
    }

    driver->initialized = 1;

    return 0;
}

/**
 * 执行CFI查询
 */
int cfi_query(cfi_driver_t *driver)
{
    int ret;

    /* 进入查询模式 */
    ret = cfi_enter_query_mode(driver);
    if (ret < 0) {
        return ret;
    }

    /* 验证签名 */
    ret = cfi_verify_signature(driver);
    if (ret < 0) {
        cfi_exit_query_mode(driver);
        return ret /* 退出查询模式 */
    cfi_exit_query_mode(driver);

    driver;
    }

   ->cfi_valid = 1;

    return 0;
}

/**
 * 验证CFI签名
 */
int cfi_verify_signature(cfi_driver_t *driver)
{
    uint8_t q, r, y;

    q = cfi_read_byte(driver, CFI_OFFSET_QRY + 0);
    r = cfi_read_byte(driver, CFI_OFFSET_QRY + 1);
    y = cfi_read_byte(driver, CFI_OFFSET_QRY + 2);

    if (q != 0x51 || r != 0x52 || y != 0x59) {
        return -NOR_ERR_NO_DEVICE;
    }

    return 0;
}

/**
 * 解析设备信息
 */
int cfi_parse_device_info(cfi_driver_t *driver)
{
    cfi_device_info_t *info = &driver->device_info;

    info->vendor_id = cfi_read_byte(driver, CFI_OFFSET_PID);
    info->device_id = cfi_read_word(driver, CFI_OFFSET_PID + 1);
    info->pri_algo_id = cfi_read_word(driver, CFI_OFFSET_PID);
    info->alt_algo_id = cfi_read_word(driver, CFI_OFFSET_AID);
    info->version_minor = cfi_read_byte(driver, CFI_OFFSET_VERSION);
    info->version_major = cfi_read_byte(driver, CFI_OFFSET_VERSION + 1);

    /* 查找厂商名称 */
    info->vendor_name = cfi_get_vendor_name(info->vendor_id);

    /* 构建设备名称 */
    snprintf(info->device_name, sizeof(info->device_name),
             "%s Flash", info->vendor_name);

    return 0;
}

/**
 * 解析几何参数
 */
int cfi_parse_geometry(cfi_driver_t *driver)
{
    cfi_geometry_t *geo = &driver->geometry;
    uint8_t num_regions;
    uint32_t offset;
    uint32_t current_addr = 0;

    num_regions = cfi_read_byte(driver, CFI_OFFSET_NUM_REGIONS);
    geo->num_regions = num_regions > 4 ? 4 : num_regions;
    geo->total_capacity = 0;

    offset = CFI_OFFSET_NUM_REGIONS + 1;

    for (int i = 0; i < geo->num_regions; i++) {
        uint16_t size_code = cfi_read_byte(driver, offset);
        uint16_t block_count = cfi_read_word(driver, offset + 1);

        geo->regions[i].block_size = 1UL << size_code;
        geo->regions[i].block_count = block_count;
        geo->regions[i].region_size = (uint32_t)block_count * geo->regions[i].block_size;
        geo->regions[i].start_addr = current_addr;

        geo->total_capacity += geo->regions[i].region_size;
        current_addr += geo->regions[i].region_size;

        offset += 4;
    }

    return 0;
}

/**
 * 解析时序参数
 */
int cfi_parse_timing(cfi_driver_t *driver)
{
    cfi_timing_t *timing = &driver->timing;

    timing->max_word_prog_us = 1UL << cfi_read_byte(driver, 0x30);
    timing->typ_word_prog_us = 1UL << cfi_read_word(driver, 0x31);
    timing->typ_block_erase_ms = 1UL << cfi_read_word(driver, 0x2D);
    timing->max_block_erase_ms = 1UL << cfi_read_word(driver, 0x2B);

    return 0;
}

/**
 * 解析电压参数
 */
int cfi_parse_voltage(cfi_driver_t *driver)
{
    cfi_voltage_t *voltage = &driver->voltage;

    voltage->vcc_min = cfi_read_byte(driver, 0x1B);
    voltage->vcc_max = cfi_read_byte(driver, 0x1D);

    return 0;
}

/**
 * 解析所有参数
 */
int cfi_parse_all(cfi_driver_t *driver)
{
    int ret;

    /* 进入查询模式 */
    ret = cfi_enter_query_mode(driver);
    if (ret < 0) {
        return ret;
    }

    /* 解析各项参数 */
    cfi_parse_device_info(driver);
    cfi_parse_geometry(driver);
    cfi_parse_timing(driver);
    cfi_parse_voltage(driver);

    /* 退出查询模式 */
    cfi_exit_query_mode(driver);

    return 0;
}

/**
 * 构建扇区地图
 */
int cfi_build_sector_map(cfi_driver_t *driver)
{
    nor_sector_map_t *map = &driver->sector_map;
    cfi_geometry_t *geo = &driver->geometry;
    uint32_t sector_idx = 0;
    uint32_t current_addr = 0;

    map->total_size = geo->total_capacity;
    map->total_sectors = 0;

    for (int r = 0; r < geo->num_regions; r++) {
        for (uint32_t b = 0; b < geo->regions[r].block_count; b++) {
            if (sector_idx >= MAX_SECTORS) {
                return -NOR_ERR_OUT_OF_RANGE;
            }

            map->sectors[sector_idx].start_addr = current_addr;
            map->sectors[sector_idx].end_addr = current_addr + geo->regions[r].block_size;
            map->sectors[sector_idx].size = geo->regions[r].block_size;
            map->sectors[sector_idx].region_id = r;
            map->sectors[sector_idx].is_eraseable = 1;

            current_addr += geo->regions[r].block_size;
            sector_idx++;
        }
    }

    map->total_sectors = sector_idx;

    return 0;
}

/**
 * 根据地址查找扇区
 */
int cfi_find_sector(cfi_driver_t *driver, uint32_t addr)
{
    nor_sector_map_t *map = &driver->sector_map;

    for (uint32_t i = 0; i < map->total_sectors; i++) {
        if (addr >= map->sectors[i].start_addr &&
            addr < map->sectors[i].end_addr) {
            return (int)i;
        }
    }

    return -1;
}

/**
 * 获取芯片容量
 */
uint32_t cfi_get_capacity(const cfi_driver_t *driver)
{
    return driver->geometry.total_capacity;
}

/**
 * 获取厂商名称
 */
const char* cfi_get_vendor_name(uint8_t vendor_id)
{
    for (int i = 0; vendor_table[i].name != NULL; i++) {
        if (vendor_table[i].id == vendor_id) {
            return vendor_table[i].name;
        }
    }
    return "Unknown";
}

/**
 * 打印驱动信息
 */
void cfi_dump_info(const cfi_driver_t *driver)
{
    printf("=== CFI Flash Driver Info ===\n");
    printf("Vendor: %s\n", driver->device_info.vendor_name);
    printf("Device: %s\n", driver->device_info.device_name);
    printf("Device ID: 0x%04X\n", driver->device_info.device_id);
    printf("Capacity: %u bytes (%.2f MB)\n",
           driver->geometry.total_capacity,
           (float)driver->geometry.total_capacity / (1024 * 1024));
    printf("Interface: %s\n", "Parallel");
    printf("Erase Regions: %u\n", driver->geometry.num_regions);
    printf("Total Sectors: %u\n", driver->sector_map.total_sectors);

    for (int i = 0; i < driver->geometry.num_regions; i++) {
        printf("  Region %d: %u KB x %u = %u KB\n",
               i + 1,
               driver->geometry.regions[i].block_size >> 10,
               driver->geometry.regions[i].block_count,
               driver->geometry.regions[i].region_size >> 10);
    }
}
```

### 5.3 驱动使用示例

以下示例展示如何使用CFI驱动初始化Flash设备并进行基本操作。

```c
/**
 * FSMC传输层实现示例
 */
typedef struct {
    FSMC_NORSRAM_Typedef *fsmc_bank;
    void (*delay_us)(uint32_t us);
    uint32_t (*get_tick)(void);
} my_fsmc_transport_t;

static int fsmc_read_byte(void *handle, uint32_t addr, uint8_t *data)
{
    *data = *(volatile uint8_t *)addr;
    return 0;
}

static int fsmc_write_byte(void *handle, uint32_t addr, uint8_t data)
{
    *(volatile uint8_t *)addr = data;
    return 0;
}

static int fsmc_read_word(void *handle, uint32_t addr, uint16_t *data)
{
    *data = *(volatile uint16_t *)addr;
    return 0;
}

static int fsmc_write_word(void *handle, uint32_t addr, uint16_t data)
{
    *(volatile uint16_t *)addr = data;
    return 0;
}

static const cfi_transport_ops_t fsmc_ops = {
    .read_byte  = fsmc_read_byte,
    .write_byte = fsmc_write_byte,
    .read_word  = fsmc_read_word,
    .write_word = fsmc_write_word,
    .delay_us   = HAL_Delay_us,   /* 需实现微秒延时 */
    .get_tick   = HAL_GetTick,
};

/**
 * Flash驱动初始化示例
 */
int flash_driver_init(void)
{
    int ret;

    /* 创建传输层 */
    static my_fsmc_transport_t fsmc_transport = {
        .fsmc_bank = FSMC_BANK1,
        .delay_us  = HAL_Delay_us,
        .get_tick  = HAL_GetTick,
    };

    static cfi_transport_t transport = {
        .handle = &fsmc_transport,
        .ops    = &fsmc_ops,
    };

    /* 创建CFI驱动 */
    cfi_driver_t *cfi = cfi_driver_create();
    if (!cfi) {
        printf("Failed to create CFI driver\n");
        return -NOR_ERR;
    }

    /* 初始化CFI驱动 */
    /* Flash基址: 0x64000000 (根据FSMC配置) */
    ret = cfi_driver_init(cfi, &transport, 0x64000000, 16);
    if (ret < 0) {
        printf("Failed to init CFI driver: %d\n", ret);
        cfi_driver_destroy(cfi);
        return ret;
    }

    /* 打印驱动信息 */
    cfi_dump_info(cfi);

    /* 保存驱动句柄供后续使用 */
    g_flash_driver = cfi;

    printf("Flash driver initialized successfully\n");

    return 0;
}

/**
 * 读取Flash数据示例
 */
int flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    cfi_driver_t *cfi = g_flash_driver;

    if (!cfi || !buf || addr + len > cfi->geometry.total_capacity) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 直接通过内存映射读取 */
    memcpy(buf, (void *)(cfi->flash_base + addr), len);

    return (int)len;
}

/**
 * 扇区擦除示例
 */
int flash_erase_sector(uint32_t addr)
{
    cfi_driver_t *cfi = g_flash_driver;
    int sector_idx;

    if (!cfi) {
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 查找扇区 */
    sector_idx = cfi_find_sector(cfi, addr);
    if (sector_idx < 0) {
        return -NOR_ERR_OUT_OF_RANGE;
    }

    /* 检查是否可擦除 */
    if (!cfi->sector_map.sectors[sector_idx].is_eraseable) {
        return -NOR_ERR_PROTECTED;
    }

    /* 发送擦除命令序列（因芯片而异） */
    /* 这里需要根据具体芯片实现 */

    return 0;
}
```

---

## 6. 驱动集成与测试

将CFI驱动集成到完整系统中需要进行配置、初始化和测试等步骤。

### 6.1 驱动配置

```c
/**
 * CFI驱动配置
 */
typedef struct {
    uint32_t flash_base;        // Flash基址
    uint8_t addr_mode;         // 地址模式：8或16
    uint8_t bus_width;         // 总线宽度：8/16/32
    uint32_t timing_read;      // 读时序
    uint32_t timing_write;     // 写时序
    uint8_t auto_detect;       // 自动检测CFI
} cfi_config_t;

/**
 * 默认配置
 */
static const cfi_config_t default_cfi_config = {
    .flash_base   = 0x64000000,
    .addr_mode    = 16,
    .bus_width    = 16,
    .timing_read  = 0,
    .timing_write = 0,
    .auto_detect  = 1,
};
```

### 6.2 错误处理

```c
/**
 * 错误码定义
 */
typedef enum {
    NOR_ERR_NONE = 0,
    NOR_ERR_INVALID_PARAM = -1,
    NOR_ERR_NO_DEVICE = -2,
    NOR_ERR_OUT_OF_RANGE = -3,
    NOR_ERR_TIMEOUT = -4,
    NOR_ERR_PROTECTED = -5,
    NOR_ERR_VERIFY = -6,
    NOR_ERR_TRANSPORT = -7,
} nor_err_t;

/**
 * 错误描述
 */
static const char* nor_error_str[] = {
    [0] = "Success",
    [-1] = "Invalid parameter",
    [-2] = "No device found",
    [-3] = "Address out of range",
    [-4] = "Operation timeout",
    [-5] = "Sector protected",
    [-6] = "Verify failed",
    [-7] = "Transport error",
};

/**
 * 获取错误描述
 */
const char* nor_error_string(int err)
{
    int idx = -err;
    if (idx >= 0 && idx < (int)(sizeof(nor_error_str) / sizeof(nor_error_str[0]))) {
        return nor_error_str[idx];
    }
    return "Unknown error";
}
```

---

## 本章小结

本章详细介绍了CFI接口驱动的完整实现方案，主要内容包括：

**CFI驱动架构设计**部分阐述了驱动在整体架构中的位置和职责划分。驱动采用模块化设计，包括查询控制模块、参数解析模块、扇区管理模块和设备识别模块，各模块通过明确定义的接口进行交互。

**CFI查询命令序列实现**部分详细介绍了进入查询模式、读取查询数据、退出查询模式的完整流程。提供了标准化的查询命令实现代码，并考虑了不同地址模式（x8/x16）的兼容性处理。

**设备识别与参数解析**部分全面解析了CFI数据结构的各项参数，包括QRY签名验证、设备ID解析、容量参数解析、接口类型解析、时序参数解析和电压参数解析。这些参数的正确解析是驱动工作的基础。

**扇区地图构建**部分介绍了擦除块区域结构的解析方法和扇区地图的构建算法。扇区地图用于地址到扇区的映射计算，是Flash读写操作的核心基础。

**驱动代码示例**部分提供了完整的CFI驱动实现代码，包括头文件定义、驱动初始化、参数解析、扇区地图构建等核心功能的实现，并展示了驱动集成的基本方法。

通过本章的学习，开发者可以掌握CFI驱动的设计思路和实现方法，为并行Nor Flash的完整驱动开发奠定基础。


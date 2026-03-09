# SFDP 参数解析驱动设计

SFDP（Serial Flash Discoverable Parameters）解析驱动是 Nor Flash 驱动架构中的关键组件，负责从 Flash 芯片中读取并解析自描述参数。本章将详细介绍 SFDP 解析器的设计架构、实现细节和代码示例，帮助开发者构建功能完善的参数解析系统。

---

## 1. SFDP 驱动架构概述

### 1.1 解析器设计目标

SFDP 解析器的核心设计目标是实现对多种 Flash 芯片的自动识别和参数提取。一个优秀的 SFDP 解析器应当满足以下设计要求：

**通用性要求**

解析器需要能够处理不同厂商、不同容量的 Flash 芯片。JEDEC 标准定义了统一的数据结构，但各厂商在实现上可能存在细微差异。解析器应当采用标准化的解析流程，同时保留足够的灵活性以适应厂商特定参数。

**健壮性要求**

解析器必须具备完善的错误检测和恢复能力。SFDP 数据可能因芯片不支持、读取错误或数据损坏而无效。解析器应当通过签名验证、参数校验等机制确保解析结果的正确性，并在异常情况下提供有意义的错误信息。

**可扩展性要求**

SFDP 标准持续演进，新的参数类型和功能特性不断加入。解析器应当采用模块化设计，使得添加对新版本标准或新厂商参数的支持时，无需修改核心解析逻辑。

### 1.2 模块划分

SFDP 解析驱动按功能可划分为以下主要模块：

```
SFDP 解析驱动模块划分：

┌─────────────────────────────────────────────────────────────┐
│                      SFDP 解析驱动层                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │  SFDP 读取  │  │   头部解析   │  │  参数表遍历  │        │
│  │    模块     │  │    模块     │  │    模块     │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐        │
│  │  BFPT 解析  │  │ 扇区映射解析 │  │厂商参数处理 │        │
│  │    模块     │  │    模块     │  │    模块     │        │
│  └─────────────┘  └─────────────┘  └─────────────┘        │
├─────────────────────────────────────────────────────────────┤
│                    驱动适配层                               │
│              (向上层提供统一接口)                           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Flash 硬件抽象层                         │
│                  (SPI 通信接口)                             │
└─────────────────────────────────────────────────────────────┘
```

**SFDP 读取模块**

该模块负责与 SPI Flash 芯片通信，执行 SFDP 数据的读取操作。它封装了 SFDP 读取命令（0x5A）的发送和参数接收，是整个解析流程的数据源。该模块需要支持不同地址模式和传输速率的配置。

**头部解析模块**

头部解析模块验证 SFDP 数据的有效性，提取版本信息和参数头数量。它执行签名验证、版本号解析等基础检查，为后续的参数表遍历提供必要的元数据。

**参数表遍历模块**

该模块负责遍历所有参数头，计算参数表的实际地址偏移量，并调度相应的参数解析器处理各参数表。它是整个解析流程的协调中心。

**BFPT 解析模块**

BFPT（JEDEC Basic Flash Parameter Table）解析模块是最核心的组件，负责提取芯片容量、擦除参数、时序配置、传输模式等关键信息。这些参数直接用于配置 Flash 驱动和 SPI 控制器。

**扇区映射解析模块**

扇区映射解析模块处理可选的扇区映射表参数，构建完整的扇区地址布局。对于具有复杂层次结构的芯片，该模块提供的详细信息对于实现精确的存储管理至关重要。

**厂商参数处理模块**

该模块识别和处理厂商特定参数表，支持 Winbond、Micron、Macronix 等主要厂商的扩展参数。它为特定芯片提供额外的配置信息和功能支持。

---

## 2. SFDP 读取时序

### 2.1 读取命令序列

SFDP 读取使用 JEDEC 标准命令 0x5A（SFDP READ）。该命令的完整序列包括命令字节、地址字节、空闲周期（Dummy Cycles）和数据读取阶段。

```
SFDP 读取命令 0x5A 完整序列：

┌────────┬────────┬────────┬────────┬────────┬────────┬────────┐
│  CMD   │ ADDR2  │ ADDR1  │ ADDR0  │ DUMMY  │  DATA  │ DATA+1 │ ...
│ 0x5A   │ A[23:16]│ A[15:8]│ A[7:0] │   X    │  D[7:0]│  D[7:0]│
│  1 字节 │  1 字节 │  1 字节 │  1 字节 │  1-8 字节│  N 字节 │  N 字节 │
└────────┴────────┴────────┴────────┴────────┴────────┴────────┘

命令说明：
- CMD:     固定值 0x5A，表示 SFDP 读取操作
- ADDR:    24 位地址，指向 SFDP 内存映射区域
- DUMMY:   空闲周期数量，通常为 8 个时钟周期
- DATA:    返回的参数数据，LSB 在前（小端序）
```

SFDP 地址采用 24 位固定格式，即使某些 Flash 芯片支持 4 字节地址模式，SFDP 区域仍然使用 3 字节地址访问。SFDP 内存区域从地址 0x000000 开始，通常不超过 256 字节。

### 2.2 地址模式

SFDP 读取支持两种地址模式，具体使用哪种模式取决于芯片的容量和配置：

**3 字节地址模式**

传统的 SPI Flash 使用 3 字节（24 位）地址，可寻址最大 16MB（128Mbit）存储空间。大多数中小容量芯片的 SFDP 读取采用此模式。

```c
// 3 字节地址 SFDP 读取
uint8_t sfdp_read_3byte(spi_bus_t *bus, uint32_t address, uint8_t *buffer, uint16_t length)
{
    uint8_t cmd[4];
    cmd[0] = 0x5A;                              // SFDP READ 命令
    cmd[1] = (uint8_t)(address >> 16);          // 地址字节 2
    cmd[2] = (uint8_t)(address >> 8);           // 地址字节 1
    cmd[3] = (uint8_t)(address);                // 地址字节 0

    // 发送命令和地址，然后读取数据
    return spi_transfer(bus, cmd, 4, NULL, buffer, length);
}
```

**4 字节地址模式**

对于容量超过 128Mbit 的芯片，需要使用 4 字节地址模式访问完整的 SFDP 参数区域。某些芯片的 SFDP 区域可能需要先切换到 4 字节地址模式才能正确读取。

```c
// 4 字节地址 SFDP 读取
uint8_t sfdp_read_4byte(spi_bus_t *bus, uint32_t address, uint8_t *buffer, uint16_t length)
{
    uint8_t cmd[5];
    cmd[0] = 0x5A;                              // SFDP READ 命令
    cmd[1] = (uint8_t)(address >> 24);          // 地址字节 3
    cmd[2] = (uint8_t)(address >> 16);          // 地址字节 2
    cmd[3] = (uint8_t)(address >> 8);           // 地址字节 1
    cmd[4] = (uint8_t)(address);                // 地址字节 0

    // 发送命令和地址，然后读取数据
    return spi_transfer(bus, cmd, 5, NULL, buffer, length);
}
```

### 2.3 读取流程

完整的 SFDP 读取流程包括初始化、参数读取和数据验证三个阶段：

```c
// SFDP 读取流程
typedef struct {
    spi_bus_t *bus;
    uint8_t   address_mode;    // 3 或 4 字节地址
    uint8_t   dummy_cycles;    // 空闲周期数量
    uint8_t   mode_bits;        // 传输模式
} sfdp_config_t;

int sfdp_read_parameters(sfdp_config_t *config, uint8_t *buffer, uint16_t size)
{
    int ret;

    // 阶段 1: 初始化 SPI 接口
    ret = spi_set_mode(config->bus, SPI_MODE_0);
    if (ret != 0) return ret;

    ret = spi_set_frequency(config->bus, SFDP_MAX_FREQ);
    if (ret != 0) return ret;

    // 设置空闲周期
    ret = spi_set_dummy_cycles(config->bus, config->dummy_cycles);
    if (ret != 0) return ret;

    // 阶段 2: 读取 SFDP 数据
    // SFDP 区域通常从地址 0 开始
    if (config->address_mode == 4) {
        ret = sfdp_read_4byte(config->bus, 0, buffer, size);
    } else {
        ret = sfdp_read_3byte(config->bus, 0, buffer, size);
    }

    if (ret != 0) return ret;

    // 阶段 3: 验证数据有效性
    // 检查签名是否为 "SFDP" (0x50444653)
    if (buffer[0] != 0x53 || buffer[1] != 0x46 ||
        buffer[2] != 0x44 || buffer[3] != 0x50) {
        return -SFDP_ERR_INVALID_SIGNATURE;
    }

    return 0;
}
```

---

## 3. 头部解析与验证

### 3.1 Signature 验证

SFDP 签名验证是确保数据有效性的第一道防线。SFDP 头部的前 4 个字节必须为 ASCII 字符 "SFDP"，对应十六进制值 0x50、0x46、0x44、0x50（小端序存储为 0x50444653）。

```c
// SFDP 签名验证
#define SFDP_SIGNATURE          0x50444653
#define SFDP_SIGNATURE_OFFSET   0x00

/**
 * 验证 SFDP 签名
 * @param buffer 指向 SFDP 头部数据的缓冲区
 * @return 0 表示签名有效，负值表示错误
 */
int sfdp_verify_signature(const uint8_t *buffer)
{
    uint32_t signature;

    if (buffer == NULL) {
        return -SFDP_ERR_NULL_POINTER;
    }

    // 提取 4 字节签名（LSB 在前）
    signature = buffer[0] | (buffer[1] << 8) |
                (buffer[2] << 16) | (buffer[3] << 24);

    if (signature != SFDP_SIGNATURE) {
        printf("SFDP: Invalid signature 0x%08X, expected 0x%08X\r\n",
               signature, SFDP_SIGNATURE);
        return -SFDP_ERR_INVALID_SIGNATURE;
    }

    return 0;
}
```

签名验证失败可能有以下原因：

- 读取的地址位置不正确（SFDP 区域通常从地址 0 开始）
- Flash 芯片不支持 SFDP 标准
- SPI 通信错误导致数据读取失败
- Flash 芯片处于写入保护状态

### 3.2 版本号解析

SFDP 头部包含主版本号和次版本号，用于确定参数表的格式版本。驱动代码根据版本号选择相应的解析逻辑。

```c
// SFDP 版本定义
typedef struct {
    uint8_t major;      // 主版本号
    uint8_t minor;      // 次版本号
} sfdp_version_t;

// SFDP 次版本号与 JEDEC 标准对应关系
static const sfdp_version_t sfdp_std_versions[] = {
    {1, 0},   // JESD216
    {1, 6},   // JESD216A
    {1, 7},   // JESD216B
    {1, 8},   // JESD216B.01
    {1, 9},   // JESD216C
    {1, 10},  // JESD216D
};

/**
 * 解析 SFDP 版本号
 * @param buffer 指向 SFDP 头部数据的缓冲区
 * @param version 输出版本号结构
 */
void sfdp_parse_version(const uint8_t *buffer, sfdp_version_t *version)
{
    version->minor = buffer[4];  // Minor Revision
    version->major = buffer[5];  // Major Revision

    printf("SFDP: Version %d.%d (JESD216", version->major, version->minor);

    // 根据版本号输出对应的 JEDEC 标准
    if (version->major == 1) {
        if (version->minor == 0) {
            printf(" original)\r\n");
        } else if (version->minor == 6) {
            printf("A)\r\n");
        } else if (version->minor == 7) {
            printf("B)\r\n");
        } else if (version->minor >= 8) {
            printf("B.01+)\r\n");
        }
    } else {
        printf(")\r\n");
    }
}
```

### 3.3 参数头数量

SFDP 头部的第 6 个字节（偏移量 0x06）表示参数头的数量。该值为 N 时，实际参数头数量为 N+1。

```c
// 参数头数量解析
#define SFDP_NUM_HEADERS_OFFSET  0x06

/**
 * 解析参数头数量
 * @param buffer 指向 SFDP 头部数据的缓冲区
 * @return 参数头数量
 */
uint8_t sfdp_parse_num_headers(const uint8_t *buffer)
{
    uint8_t num_headers;

    num_headers = buffer[SFDP_NUM_HEADERS_OFFSET];
    printf("SFDP: Number of parameter headers: %d\r\n", num_headers + 1);

    return num_headers;
}
```

大多数 Flash 芯片的 SFDP 至少包含两个参数头：

- 参数头 0：基础 Flash 参数表（BFPT），参数 ID 为 0x0001
- 参数头 1：扇区映射表，参数 ID 为 0x0002（可选）

某些芯片还可能包含 4 字节地址参数表（ID 0x0003）和厂商特定参数表（ID 0xFFxx）。

---

## 4. 参数表遍历机制

### 4.1 参数头解析

参数头是访问各参数表的入口，每个参数头占用 8 个字节，包含参数 ID、版本、长度和指针信息。

```c
// 参数头结构定义
typedef struct {
    uint16_t id;                 // 参数 ID
    uint8_t  major_rev;         // 参数表主版本号
    uint8_t  minor_rev;         // 参数表次版本号
    uint16_t length;            // 参数表长度（字节）
    uint32_t pointer;           // 参数表地址偏移
} sfdp_parameter_header_t;

// 参数头偏移量定义
#define SFDP_PARAM_ID_LSB       0x00
#define SFDP_PARAM_ID_MSB       0x01
#define SFDP_PARAM_MAJOR_REV    0x02
#define SFDP_PARAM_MINOR_REV    0x03
#define SFDP_PARAM_LENGTH_LSB   0x04
#define SFDP_PARAM_LENGTH_MSB  0x05
#define SFDP_PARAM_POINTER_LSB  0x06
#define SFDP_PARAM_POINTER_MSB  0x07
#define SFDP_HEADER_SIZE        8

// 标准参数表 ID 定义
#define SFDP_PARAM_ID_BFPT       0x0001
#define SFDP_PARAM_ID_SECTOR_MAP 0x0002
#define SFDP_PARAM_ID_4BYTE_ADDR 0x0003

/**
 * 解析单个参数头
 * @param buffer 指向参数头数据的缓冲区
 * @param header 输出参数头结构
 */
void sfdp_parse_parameter_header(const uint8_t *buffer, sfdp_parameter_header_t *header)
{
    header->id = buffer[SFDP_PARAM_ID_LSB] | (buffer[SFDP_PARAM_ID_MSB] << 8);
    header->major_rev = buffer[SFDP_PARAM_MAJOR_REV];
    header->minor_rev = buffer[SFDP_PARAM_MINOR_REV];
    header->length = buffer[SFDP_PARAM_LENGTH_LSB] |
                     (buffer[SFDP_PARAM_LENGTH_MSB] << 8);
    header->pointer = buffer[SFDP_PARAM_POINTER_LSB] |
                     (buffer[SFDP_PARAM_POINTER_MSB] << 8);

    printf("SFDP: Parameter Header - ID: 0x%04X, Ver: %d.%d, Length: %d, Pointer: 0x%04X\r\n",
           header->id, header->major_rev, header->minor_rev,
           header->length, header->pointer);
}
```

### 4.2 参数表指针计算

参数表指针表示该参数数据相对于 SFDP 起始地址的偏移量。计算参数表的实际物理地址需要将指针值加上 SFDP 基地址。

```c
/**
 * 计算参数表的实际读取地址
 * @param sfdp_base SFDP 起始地址（通常为 0）
 * @param pointer 参数头中的指针值
 * @return 参数表的实际读取地址
 */
uint32_t sfdp_calculate_table_address(uint32_t sfdp_base, uint16_t pointer)
{
    return sfdp_base + pointer;
}
```

需要注意的是，某些老旧芯片的参数表指针可能为 0，这意味着参数表紧跟在参数头之后。驱动代码应当正确处理这种情况。

### 4.3 遍历算法

完整的参数表遍历算法负责解析所有参数头，并根据参数 ID 调用相应的解析器处理各参数表。

```c
// SFDP 解析结果结构
typedef struct {
    sfdp_version_t    version;          // SFDP 版本
    uint8_t           num_headers;     // 参数头数量

    // BFPT 解析结果
    bfpt_info_t       bfpt;

    // 扇区映射信息
    sector_map_t      sector_map;

    // 解析状态标志
    uint8_t           bfpt_found:1;
    uint8_t           sector_map_found:1;
    uint8_t           has_vendor_params:1;
} sfdp_info_t;

/**
 * 遍历并解析所有参数表
 * @param sfdp_buffer 完整的 SFDP 数据缓冲区
 * @param info 输出解析结果
 * @return 0 表示成功，负值表示错误
 */
int sfdp_parse_all_tables(const uint8_t *sfdp_buffer, sfdp_info_t *info)
{
    int ret;
    uint8_t i;
    uint8_t num_headers;
    const uint8_t *header_ptr;
    sfdp_parameter_header_t header;

    // 初始化解析结果
    memset(info, 0, sizeof(sfdp_info_t));

    // 解析 SFDP 头部
    ret = sfdp_verify_signature(sfdp_buffer);
    if (ret != 0) return ret;

    sfdp_parse_version(sfdp_buffer, &info->version);
    num_headers = sfdp_parse_num_headers(sfdp_buffer);
    info->num_headers = num_headers;

    // 参数头从偏移 0x08 开始（SFDP 头部之后）
    header_ptr = sfdp_buffer + 8;

    // 遍历所有参数头
    for (i = 0; i <= num_headers; i++) {
        sfdp_parse_parameter_header(header_ptr, &header);

        // 根据参数 ID 选择相应的解析器
        switch (header.id) {
            case SFDP_PARAM_ID_BFPT:
                printf("SFDP: Found BFPT at offset 0x%04X\r\n", header.pointer);
                ret = sfdp_parse_bfpt(sfdp_buffer + header.pointer,
                                      header.length, &info->bfpt);
                if (ret == 0) {
                    info->bfpt_found = 1;
                }
                break;

            case SFDP_PARAM_ID_SECTOR_MAP:
                printf("SFDP: Found Sector Map at offset 0x%04X\r\n", header.pointer);
                ret = sfdp_parse_sector_map(sfdp_buffer + header.pointer,
                                            header.length, &info->sector_map);
                if (ret == 0) {
                    info->sector_map_found = 1;
                }
                break;

            case SFDP_PARAM_ID_4BYTE_ADDR:
                printf("SFDP: Found 4-Byte Address Parameters\r\n");
                // 处理 4 字节地址参数
                break;

            default:
                // 厂商特定参数或其他未知参数
                if ((header.id & 0xFF00) == 0xFF00) {
                    uint8_t vendor_code = header.id & 0x00FF;
                    printf("SFDP: Found vendor parameter (ID: 0x%04X, Vendor: 0x%02X)\r\n",
                           header.id, vendor_code);
                    ret = sfdp_parse_vendor_params(vendor_code,
                                                    sfdp_buffer + header.pointer,
                                                    header.length, info);
                    if (ret == 0) {
                        info->has_vendor_params = 1;
                    }
                } else {
                    printf("SFDP: Unknown parameter ID: 0x%04X\r\n", header.id);
                }
                break;
        }

        // 移动到下一个参数头
        header_ptr += SFDP_HEADER_SIZE;
    }

    return 0;
}
```

---

## 5. JEDEC 基础参数表（BFPT）解析

### 5.1 容量解析

BFPT 中的芯片容量编码采用密度位方式，容量值等于实际容量位宽减 1。计算实际容量需要解析字节 4-6 中的密度位字段。

```c
// BFPT 容量解析
#define BFPT_DENSITY_OFFSET      0x04

/**
 * 从 BFPT 中解析芯片容量
 * @param bfpt_data 指向 BFPT 数据的缓冲区
 * @return 芯片容量（单位：位）
 */
uint32_t bfpt_parse_density(const uint8_t *bfpt_data)
{
    uint32_t density;
    uint32_t density_bits;

    // BFPT 字节 4-5: 密度位 [23:8]
    // BFPT 字节 6:   密度位 [31:24]（JESD216B 及以后版本）

    density = bfpt_data[BFPT_DENSITY_OFFSET] |
              (bfpt_data[BFPT_DENSITY_OFFSET + 1] << 8) |
              (bfpt_data[BFPT_DENSITY_OFFSET + 2] << 16);

    // 密度编码规则：密度 = 密度位 + 1
    density_bits = density + 1;

    return density_bits;
}

/**
 * 格式化输出芯片容量
 * @param density_bits 密度位
 * @return 指向格式化字符串的指针
 */
const char* bfpt_format_capacity(uint32_t density_bits)
{
    static char buffer[32];

    if (density_bits >= (1024 * 1024 * 1024)) {
        snprintf(buffer, sizeof(buffer), "%uGbit", density_bits / (1024 * 1024 * 1024));
    } else if (density_bits >= 1024 * 1024) {
        snprintf(buffer, sizeof(buffer), "%uMbit", density_bits / (1024 * 1024));
    } else if (density_bits >= 1024) {
        snprintf(buffer, sizeof(buffer), "%uKbit", density_bits / 1024);
    } else {
        snprintf(buffer, sizeof(buffer), "%ubit", density_bits);
    }

    return buffer;
}
```

### 5.2 擦除块大小解析

BFPT 字节 0-1 定义了芯片支持的擦除类型和擦除粒度。

```c
// BFPT 擦除参数定义
#define BFPT_ERASE_CONFIG_OFFSET    0x00
#define BFPT_SECTOR_TYPE_OFFSET      0x01

typedef struct {
    uint8_t  supports_4k_erase;     // 是否支持 4KB 擦除
    uint8_t  erase_cmd_4k;          // 4KB 擦除命令
    uint8_t  sector_type;           // 扇区架构类型
} bfpt_erase_info_t;

/**
 * 解析 BFPT 擦除参数
 * @param bfpt_data 指向 BFPT 数据的缓冲区
 * @param erase_info 输出擦除信息结构
 */
void bfpt_parse_erase_info(const uint8_t *bfpt_data, bfpt_erase_info_t *erase_info)
{
    // 解析字节 0: 4KB 擦除支持
    uint8_t erase_config = bfpt_data[BFPT_ERASE_CONFIG_OFFSET];

    switch (erase_config & 0x0F) {
        case 0x00:
            erase_info->supports_4k_erase = 0;
            erase_info->erase_cmd_4k = 0;
            printf("BFPT: 4KB erase not supported\r\n");
            break;
        case 0x01:
            erase_info->supports_4k_erase = 1;
            erase_info->erase_cmd_4k = 0xDA;  // 4KB 扇区擦除命令
            printf("BFPT: 4KB erase supported (command 0xDA)\r\n");
            break;
        default:
            erase_info->supports_4k_erase = 0;
            erase_info->erase_cmd_4k = 0;
            printf("BFPT: Unknown erase configuration\r\n");
            break;
    }

    // 解析字节 1: 扇区架构类型
    erase_info->sector_type = bfpt_data[BFPT_SECTOR_TYPE_OFFSET];

    printf("BFPT: Sector type: %s\r\n",
           (erase_info->sector_type == 0x00) ? "Block erase only" :
           (erase_info->sector_type == 0x01) ? "4KB sector preferred" : "Unknown");
}
```

### 5.3 时序参数提取

BFPT 包含丰富的时序参数，用于配置 SPI 控制器的时钟频率和时序。

```c
// BFPT 时序参数定义
#define BFPT_MAX_CLOCK_SINGLE   0x0C
#define BFPT_MAX_CLOCK_QUAD     0x0D
#define BFPT_TIMING_PARAM1      0x0E
#define BFPT_TIMING_PARAM2      0x0F

// 时序代码到频率的映射表
static const uint32_t clock_freq_table[] = {
    0,    20,   25,   33,   40,   50,   66,   80,
    100,  104,  110,  120,  133,  166,  200,  0
};

typedef struct {
    uint32_t max_freq_single;   // Single SPI 最大频率（MHz）
    uint32_t max_freq_dual;     // Dual SPI 最大频率（MHz）
    uint32_t max_freq_quad;     // Quad SPI 最大频率（MHz）
    uint8_t  tCLH;              // 时钟高电平最小时间（代码值）
    uint8_t  tCLL;              // 时钟低电平最小时间（代码值）
    uint8_t  tDS;               // 数据建立时间（代码值）
    uint8_t  tDH;               // 数据保持时间（代码值）
} bfpt_timing_info_t;

/**
 * 解析 BFPT 时序参数
 * @param bfpt_data 指向 BFPT 数据的缓冲区
 * @param timing_info 输出时序信息结构
 */
void bfpt_parse_timing_info(const uint8_t *bfpt_data, bfpt_timing_info_t *timing_info)
{
    uint8_t clock_code;

    // 解析最大 Single SPI 频率
    clock_code = bfpt_data[BFPT_MAX_CLOCK_SINGLE] & 0x1F;
    if (clock_code < 15) {
        timing_info->max_freq_single = clock_freq_table[clock_code];
    } else {
        timing_info->max_freq_single = 0;
    }

    // 解析最大 Quad SPI 频率
    clock_code = bfpt_data[BFPT_MAX_CLOCK_QUAD] & 0x1F;
    if (clock_code < 15) {
        timing_info->max_freq_quad = clock_freq_table[clock_code];
    } else {
        timing_info->max_freq_quad = 0;
    }

    // 解析时序参数
    uint8_t timing1 = bfpt_data[BFPT_TIMING_PARAM1];
    uint8_t timing2 = bfpt_data[BFPT_TIMING_PARAM2];

    timing_info->tCLH = (timing1 >> 4) & 0x0F;
    timing_info->tCLL = timing1 & 0x0F;
    timing_info->tDS  = (timing2 >> 4) & 0x0F;
    timing_info->tDH  = timing2 & 0x0F;

    printf("BFPT: Max frequency - Single: %uMHz, Quad: %uMHz\r\n",
           timing_info->max_freq_single, timing_info->max_freq_quad);
    printf("BFPT: Timing - tCLH:%d, tCLL:%d, tDS:%d, tDH:%d\r\n",
           timing_info->tCLH, timing_info->tCLL,
           timing_info->tDS, timing_info->tDH);
}
```

### 5.4 传输模式支持

BFPT 描述了芯片支持的各种数据传输模式，包括 Single SPI、Dual SPI、Quad SPI 和 Octal SPI。

```c
// BFPT 传输模式定义
#define BFPT_ARCH_OFFSET        0x02
#define BFPT_IO_WIDTH_MASK       0x0F
#define BFPT_DDR_MASK            0xF0

typedef struct {
    uint8_t  addr_mode;         // 地址模式：3 字节 / 4 字节
    uint8_t  io_width;          // 支持的 IO 宽度
    uint8_t  page_program_bits; // 页面编程位数（8 或 16）
    uint8_t  continous_read;    // 连续读取支持
    uint8_t  ddr_support;       // DDR 支持
} bfpt_transfer_mode_t;

/**
 * 解析 BFPT 传输模式
 * @param bfpt_data 指向 BFPT 数据的缓冲区
 * @param mode 输出传输模式结构
 */
void bfpt_parse_transfer_mode(const uint8_t *bfpt_data, bfpt_transfer_mode_t *mode)
{
    uint8_t arch = bfpt_data[BFPT_ARCH_OFFSET];
    uint8_t dtr = bfpt_data[BFPT_ARCH_OFFSET + 1];

    // 解析地址模式
    mode->addr_mode = arch & 0x03;
    printf("BFPT: Address mode - %s\r\n",
           (mode->addr_mode == 0) ? "3-byte only" :
           (mode->addr_mode == 1) ? "4-byte only" :
           (mode->addr_mode >= 2) ? "3/4-byte switchable" : "Unknown");

    // 解析 IO 宽度
    mode->io_width = dtr & BFPT_IO_WIDTH_MASK;
    printf("BFPT: IO width support - ");
    if (mode->io_width == 0x00) {
        printf("x1/x2/x4\r\n");
    } else if (mode->io_width == 0x01) {
        printf("x1/x2/x4/x8 (Octal)\r\n");
    } else {
        printf("Reserved\r\n");
    }

    // 解析 DDR 支持
    mode->ddr_support = (dtr >> 4) & 0x0F;
    printf("BFPT: DDR support - %s\r\n",
           (mode->ddr_support == 0x01) ? "Yes" : "No");

    // 解析页面编程位数
    mode->page_program_bits = (arch >> 4) & 0x01;
    printf("BFPT: Page program - %u-bit\r\n",
           (mode->page_program_bits == 0) ? 8 : 16);

    // 解析连续读取模式
    mode->continous_read = (arch >> 5) & 0x01;
    printf("BFPT: Continuous read - %s\r\n",
           mode->continous_read ? "Supported" : "Not supported");
}
```

### 5.5 BFPT 完整解析函数

以下是 BFPT 完整解析函数的实现：

```c
// BFPT 解析结果结构
typedef struct {
    uint32_t            density_bits;        // 芯片密度（位）
    bfpt_erase_info_t   erase_info;          // 擦除信息
    bfpt_timing_info_t  timing_info;         // 时序信息
    bfpt_transfer_mode_t transfer_mode;      // 传输模式
    uint8_t             write_granularity;  // 写入粒度（字节）
    uint8_t             page_size;            // 页面大小（字节）
    uint8_t             quad_enable_info;    // Quad Enable 信息
    uint8_t             version;             // BFPT 版本
} bfpt_info_t;

/**
 * 解析完整的 BFPT
 * @param bfpt_data 指向 BFPT 数据的缓冲区
 * @param length BFPT 数据长度
 * @param info 输出解析结果
 * @return 0 表示成功，负值表示错误
 */
int sfdp_parse_bfpt(const uint8_t *bfpt_data, uint16_t length, bfpt_info_t *info)
{
    if (bfpt_data == NULL || info == NULL) {
        return -SFDP_ERR_NULL_POINTER;
    }

    if (length < 16) {
        printf("BFPT: Invalid length %d (minimum 16 bytes)\r\n", length);
        return -SFDP_ERR_INVALID_LENGTH;
    }

    // 解析各部分参数
    info->density_bits = bfpt_parse_density(bfpt_data);
    printf("BFPT: Density = %s (%u bits)\r\n",
           bfpt_format_capacity(info->density_bits), info->density_bits);

    bfpt_parse_erase_info(bfpt_data, &info->erase_info);
    bfpt_parse_timing_info(bfpt_data, &info->timing_info);
    bfpt_parse_transfer_mode(bfpt_data, &info->transfer_mode);

    // 解析写入粒度（字节 8）
    if (length > 8) {
        info->write_granularity = bfpt_data[8];
        if (info->write_granularity == 0) {
            info->write_granularity = 1;  // 默认 1 字节粒度
        }
        printf("BFPT: Write granularity = %u bytes\r\n", info->write_granularity);
    }

    // 解析页面大小（字节 0x43，JESD216B 及以后）
    if (length > 0x43) {
        info->page_size = bfpt_data[0x43];
        if (info->page_size == 0) {
            info->page_size = 256;  // 默认 256 字节
        }
        printf("BFPT: Page size = %u bytes\r\n", info->page_size);
    }

    // 解析 Quad Enable 信息（字节 0x42）
    if (length > 0x42) {
        info->quad_enable_info = bfpt_data[0x42];
        printf("BFPT: Quad Enable info = 0x%02X\r\n", info->quad_enable_info);
    }

    info->version = bfpt_data[2];  // 主版本号

    return 0;
}
```

---

## 6. 扇区映射表解析

### 6.1 4KB 擦除参数表

扇区映射表（Sector Map Table）从 JESD216B 版本开始引入，用于描述 Flash 芯片的详细扇区结构。该表以 4KB 擦除参数开头，后面跟随多个区域描述符。

```c
// 扇区映射表定义
#define SECTOR_MAP_CONFIG_OFFSET    0x00
#define SECTOR_MAP_DESC_LEN_OFFSET  0x01
#define SECTOR_MAP_NUM_REGIONS_OFFSET 0x02
#define SECTOR_MAP_REGION_DESC_START  0x03

typedef struct {
    uint8_t  config;              // 配置标识
    uint8_t  descriptor_length;  // 区域描述符长度
    uint8_t  num_regions;         // 区域数量
} sector_map_header_t;

/**
 * 解析扇区映射表头部
 * @param map_data 指向扇区映射表数据的缓冲区
 * @param header 输出头部信息
 */
void sector_map_parse_header(const uint8_t *map_data, sector_map_header_t *header)
{
    header->config = map_data[SECTOR_MAP_CONFIG_OFFSET];
    header->descriptor_length = map_data[SECTOR_MAP_DESC_LEN_OFFSET];
    header->num_regions = map_data[SECTOR_MAP_NUM_REGIONS_OFFSET];

    printf("Sector Map: Config = 0x%02X\r\n", header->config);
    printf("Sector Map: Descriptor length = %u bytes\r\n", header->descriptor_length);
    printf("Sector Map: Number of regions = %u\r\n", header->num_regions);

    // 配置标识含义
    switch (header->config) {
        case 0x00:
            printf("Sector Map: Block erase only\r\n");
            break;
        case 0x01:
            printf("Sector Map: 4KB sector preferred\r\n");
            break;
        case 0x02:
            printf("Sector Map: 4KB sector in parameter region only\r\n");
            break;
        default:
            printf("Sector Map: Unknown configuration\r\n");
            break;
    }
}
```

### 6.2 扇区类型解析

每个区域描述符包含扇区类型和区域大小信息。JEDEC 标准定义了三种主要的扇区类型。

```c
// 扇区类型定义
#define SECTOR_TYPE_4KB     0x00
#define SECTOR_TYPE_32KB    0x01
#define SECTOR_TYPE_64KB    0x02

// 扇区信息结构
typedef struct {
    uint8_t  type;           // 扇区类型
    uint32_t size;           // 单个扇区大小（字节）
    uint32_t count;          // 扇区数量
    uint32_t total_size;    // 区域总大小（字节）
    uint32_t start_addr;     // 区域起始地址
} sector_info_t;

/**
 * 计算扇区大小
 * @param type 扇区类型代码
 * @return 扇区大小（字节）
 */
uint32_t sector_get_size_by_type(uint8_t type)
{
    switch (type) {
        case SECTOR_TYPE_4KB:
            return 4 * 1024;     // 4KB
        case SECTOR_TYPE_32KB:
            return 32 * 1024;    // 32KB
        case SECTOR_TYPE_64KB:
            return 64 * 1024;   // 64KB
        default:
            return 0;
    }
}

/**
 * 获取扇区类型名称
 * @param type 扇区类型代码
 * @return 扇区类型名称
 */
const char* sector_get_type_name(uint8_t type)
{
    switch (type) {
        case SECTOR_TYPE_4KB:
            return "4KB Sector";
        case SECTOR_TYPE_32KB:
            return "32KB Block";
        case SECTOR_TYPE_64KB:
            return "64KB Block";
        default:
            return "Unknown";
    }
}
```

### 6.3 地址布局构建

完整的扇区映射需要遍历所有区域描述符，计算每个区域的起始地址和大小。

```c
// 扇区映射解析结果
#define MAX_SECTOR_REGIONS  16

typedef struct {
    sector_map_header_t header;
    sector_info_t       regions[MAX_SECTOR_REGIONS];
    uint32_t            total_size;    // Flash 总大小
} sector_map_t;

/**
 * 解析扇区映射表
 * @param map_data 指向扇区映射表数据的缓冲区
 * @param length 扇区映射表长度
 * @param map 输出扇区映射结果
 * @return 0 表示成功，负值表示错误
 */
int sfdp_parse_sector_map(const uint8_t *map_data, uint16_t length, sector_map_t *map)
{
    uint8_t i;
    const uint8_t *region_ptr;
    uint32_t current_addr = 0;

    if (map_data == NULL || map == NULL) {
        return -SFDP_ERR_NULL_POINTER;
    }

    // 解析头部
    sector_map_parse_header(map_data, &map->header);

    // 检查描述符长度是否有效
    if (map->header.descriptor_length < 4) {
        printf("Sector Map: Invalid descriptor length\r\n");
        return -SFDP_ERR_INVALID_DATA;
    }

    // 解析每个区域
    region_ptr = map_data + SECTOR_MAP_REGION_DESC_START;

    for (i = 0; i < map->header.num_regions && i < MAX_SECTOR_REGIONS; i++) {
        sector_info_t *region = &map->regions[i];

        // 解析区域描述符（4 字节格式）
        region->type = region_ptr[0];
        region->size = sector_get_size_by_type(region->type);

        // 区域大小为 3 字节，little-endian
        region->count = region_ptr[1] |
                        (region_ptr[2] << 8) |
                        (region_ptr[3] << 16);

        region->total_size = region->size * region->count;
        region->start_addr = current_addr;

        printf("Sector Map: Region %u - Type: %s, Count: %u, "
               "Size: %uKB, Addr: 0x%06X - 0x%06X\r\n",
               i, sector_get_type_name(region->type),
               region->count, region->size / 1024,
               region->start_addr, region->start_addr + region->total_size - 1);

        // 更新当前地址
        current_addr += region->total_size;

        // 移动到下一个描述符
        region_ptr += map->header.descriptor_length;
    }

    map->total_size = current_addr;

    printf("Sector Map: Total Flash size = %u bytes (%u MB)\r\n",
           map->total_size, map->total_size / (1024 * 1024));

    return 0;
}
```

---

## 7. 厂商特定参数处理

### 7.1 厂商扩展参数识别

厂商特定参数的参数 ID 以 0xFF 开头，其中低字节表示厂商代码。驱动需要识别这些参数并调用相应的解析器。

```c
// 厂商代码定义
#define VENDOR_CODE_WINBOND   0x01
#define VENDOR_CODE_MICRON    0x02
#define VENDOR_CODE_MACRONIX  0x04
#define VENDOR_CODE_GIGADEVICE 0x08
#define VENDOR_CODE_ISSI     0x0D
#define VENDOR_CODE_EON       0x1C

/**
 * 获取厂商名称
 * @param vendor_code 厂商代码
 * @return 厂商名称
 */
const char* sfdp_get_vendor_name(uint8_t vendor_code)
{
    switch (vendor_code) {
        case VENDOR_CODE_WINBOND:
            return "Winbond";
        case VENDOR_CODE_MICRON:
            return "Micron";
        case VENDOR_CODE_MACRONIX:
            return "Macronix";
        case VENDOR_CODE_GIGADEVICE:
            return "GigaDevice";
        case VENDOR_CODE_ISSI:
            return "ISSI";
        case VENDOR_CODE_EON:
            return "EON";
        default:
            return "Unknown";
    }
}

/**
 * 解析厂商特定参数
 * @param vendor_code 厂商代码
 * @param vendor_data 指向厂商参数数据的缓冲区
 * @param length 参数长度
 * @param sfdp_info 指向 SFDP 解析结果
 * @return 0 表示成功，负值表示错误
 */
int sfdp_parse_vendor_params(uint8_t vendor_code, const uint8_t *vendor_data,
                              uint16_t length, sfdp_info_t *sfdp_info)
{
    printf("SFDP: Parsing vendor parameters for %s\r\n",
           sfdp_get_vendor_name(vendor_code));

    switch (vendor_code) {
        case VENDOR_CODE_WINBOND:
            return winbond_parse_params(vendor_data, length, sfdp_info);

        case VENDOR_CODE_MICRON:
            return micron_parse_params(vendor_data, length, sfdp_info);

        case VENDOR_CODE_MACRONIX:
            return macronix_parse_params(vendor_data, length, sfdp_info);

        case VENDOR_CODE_GIGADEVICE:
            return gigadevice_parse_params(vendor_data, length, sfdp_info);

        default:
            printf("SFDP: Unsupported vendor (code: 0x%02X)\r\n", vendor_code);
            return -SFDP_ERR_UNSUPPORTED_VENDOR;
    }
}
```

### 7.2 Winbond 参数解析

Winbond 是最常见的 SPI Flash 厂商之一，其 W25QxxJV 系列芯片提供了丰富的扩展参数。

```c
// Winbond 特定参数偏移量
#define WINBOND_VOLATILE_SR_ENABLE  0x00
#define WINBOND_PAGE_SIZE_SELECT    0x01
#define WINBOND_ERASE_SUSPEND       0x02
#define WINBOND_ERASE_RESUME        0x03
#define WINBOND_POWER_SAVE_MODE      0x05
#define WINBOND_LOCK_BITS           0x07
#define WINBOND_JEDEC_ID            0x08

typedef struct {
    uint8_t volatile_sr_enable;   // 易失性状态寄存器支持
    uint8_t page_size_select;     // 页面大小选择
    uint8_t erase_suspend;        // 擦除挂起支持
    uint8_t erase_resume;         // 擦除恢复支持
    uint8_t power_save_mode;      // 掉电模式支持
    uint8_t jedec_id;             // JEDEC ID
} winbond_params_t;

/**
 * 解析 Winbond 特定参数
 * @param vendor_data 指向 Winbond 参数数据的缓冲区
 * @param length 参数长度
 * @param sfdp_info 指向 SFDP 解析结果
 * @return 0 表示成功，负值表示错误
 */
int winbond_parse_params(const uint8_t *vendor_data, uint16_t length,
                         sfdp_info_t *sfdp_info)
{
    winbond_params_t *params;

    if (vendor_data == NULL || sfdp_info == NULL) {
        return -SFDP_ERR_NULL_POINTER;
    }

    if (length < 16) {
        printf("Winbond: Invalid parameter length\r\n");
        return -SFDP_ERR_INVALID_LENGTH;
    }

    params = (winbond_params_t*)vendor_data;

    printf("Winbond Parameters:\r\n");

    // 解析易失性状态寄存器支持
    params->volatile_sr_enable = vendor_data[WINBOND_VOLATILE_SR_ENABLE];
    printf("  - Volatile Status Register: %s\r\n",
           (params->volatile_sr_enable == 0x01) ? "Supported" : "Not supported");

    // 解析页面大小选择
    params->page_size_select = vendor_data[WINBOND_PAGE_SIZE_SELECT];
    printf("  - Page Size: %s\r\n",
           (params->page_size_select == 0x01) ? "512 bytes" : "256 bytes");

    // 解析擦除挂起/恢复支持
    params->erase_suspend = vendor_data[WINBOND_ERASE_SUSPEND];
    params->erase_resume = vendor_data[WINBOND_ERASE_RESUME];
    printf("  - Erase Suspend/Resume: %s/%s\r\n",
           (params->erase_suspend) ? "Supported" : "Not supported",
           (params->erase_resume) ? "Supported" : "Not supported");

    // 解析掉电模式
    params->power_save_mode = vendor_data[WINBOND_POWER_SAVE_MODE];
    printf("  - Power Save Mode: %s\r\n",
           (params->power_save_mode == 0x01) ? "Supported" : "Not supported");

    // 解析 JEDEC ID
    params->jedec_id = vendor_data[WINBOND_JEDEC_ID];
    printf("  - JEDEC ID: 0x%02X\r\n", params->jedec_id);

    return 0;
}
```

### 7.3 Micron 参数解析

Micron 的 Flash 芯片通常提供更丰富的功能，包括 OTP 区域、密码保护等安全特性。

```c
// Micron 特定参数偏移量
#define MICRON_OTP_LOCK_BYTE       0x00
#define MICRON_OTP_LENGTH          0x01
#define MICRON_OTP_PAGE_SIZE       0x02
#define MICRON_DIE_COUNT           0x03
#define MICRON_DIE_TYPE            0x04
#define MICRON_ERASE_CONFIG        0x06
#define MICRON_OTP_CONFIG          0x07

typedef struct {
    uint16_t otp_lock_offset;    // OTP 锁定字节偏移
    uint16_t otp_length;          // OTP 区域长度
    uint8_t  otp_page_size;      // OTP 页面大小
    uint8_t  die_count;           // Die 数量
    uint8_t  die_type;            // Die 类型
    uint8_t  erase_config;        // 擦除配置
    uint8_t  otp_config;          // OTP 配置
} micron_params_t;

/**
 * 解析 Micron 特定参数
 * @param vendor_data 指向 Micron 参数数据的缓冲区
 * @param length 参数长度
 * @param sfdp_info 指向 SFDP 解析结果
 * @return 0 表示成功，负值表示错误
 */
int micron_parse_params(const uint8_t *vendor_data, uint16_t length,
                        sfdp_info_t *sfdp_info)
{
    micron_params_t *params;

    if (vendor_data == NULL || sfdp_info == NULL) {
        return -SFDP_ERR_NULL_POINTER;
    }

    if (length < 16) {
        printf("Micron: Invalid parameter length\r\n");
        return -SFDP_ERR_INVALID_LENGTH;
    }

    params = (micron_params_t*)vendor_data;

    printf("Micron Parameters:\r\n");

    // 解析 OTP 区域信息
    params->otp_lock_offset = vendor_data[MICRON_OTP_LOCK_BYTE] |
                              (vendor_data[MICRON_OTP_LOCK_BYTE + 1] << 8);
    params->otp_length = vendor_data[MICRON_OTP_LENGTH] |
                         (vendor_data[MICRON_OTP_LENGTH + 1] << 8);
    params->otp_page_size = vendor_data[MICRON_OTP_PAGE_SIZE];

    printf("  - OTP Region:\r\n");
    printf("    Lock Byte Offset: 0x%04X\r\n", params->otp_lock_offset);
    printf("    Length: %u bytes\r\n", params->otp_length);
    printf("    Page Size: %u bytes\r\n",
           (params->otp_page_size == 0) ? 256 : (256 * params->otp_page_size));

    // 解析 Die 信息
    params->die_count = vendor_data[MICRON_DIE_COUNT];
    params->die_type = vendor_data[MICRON_DIE_TYPE];
    printf("  - Die Information:\r\n");
    printf("    Die Count: %u\r\n", params->die_count);
    printf("    Die Type: %u\r\n", params->die_type);

    // 解析擦除配置
    params->erase_config = vendor_data[MICRON_ERASE_CONFIG];
    printf("  - Erase Configuration: 0x%02X\r\n", params->erase_config);

    // 解析 OTP 配置
    params->otp_config = vendor_data[MICRON_OTP_CONFIG];
    printf("  - OTP Configuration:\r\n");
    printf("    Password Protection: %s\r\n",
           (params->otp_config & 0x01) ? "Supported" : "Not supported");
    printf("    Persistent Lock: %s\r\n",
           (params->otp_config & 0x02) ? "Supported" : "Not supported");
    printf("    One-Time Lock: %s\r\n",
           (params->otp_config & 0x04) ? "Supported" : "Not supported");

    return 0;
}
```

---

## 8. 兼容性与扩展

### 8.1 版本兼容性处理

SFDP 解析器需要处理不同版本的 SFDP 标准，确保向后兼容性。

```c
// SFDP 版本兼容性检查
typedef struct {
    uint8_t  min_major;
    uint8_t  min_minor;
    const char *description;
} sfdp_version_requirement_t;

static const sfdp_version_requirement_t version_requirements[] = {
    {1, 0, "JESD216 - Basic SFDP support"},
    {1, 6, "JESD216A - 4-byte address support"},
    {1, 7, "JESD216B - Sector map and 32-byte BFPT"},
    {1, 8, "JESD216B.01 - Updated sector map"},
    {1, 10, "JESD216D - Octal SPI and extended params"},
};

/**
 * 检查 SFDP 版本是否满足最低要求
 * @param version 指向版本号的指针
 * @param required_major 要求的主版本号
 * @param required_minor 要求的次版本号
 * @return 0 表示满足要求，负值表示不满足
 */
int sfdp_check_version_compatibility(const sfdp_version_t *version,
                                     uint8_t required_major,
                                     uint8_t required_minor)
{
    if (version->major < required_major) {
        printf("SFDP: Version %d.%d does not meet minimum requirement %d.%d\r\n",
               version->major, version->minor,
               required_major, required_minor);
        return -SFDP_ERR_INCOMPATIBLE_VERSION;
    }

    if (version->major == required_major && version->minor < required_minor) {
        printf("SFDP: Version %d.%d does not meet minimum requirement %d.%d\r\n",
               version->major, version->minor,
               required_major, required_minor);
        return -SFDP_ERR_INCOMPATIBLE_VERSION;
    }

    return 0;
}

/**
 * 根据版本选择合适的解析策略
 * @param version SFDP 版本
 * @return 推荐的 BFPT 解析长度
 */
uint16_t sfdp_recommend_bfpt_length(const sfdp_version_t *version)
{
    // JESD216 原始版本 BFPT 长度为 16 字节
    // JESD216B 及以后版本 BFPT 长度为 32 字节
    if (version->major >= 1 && version->minor >= 7) {
        return 32;  // 支持完整的 32 字节 BFPT
    }

    return 16;  // 仅支持 16 字节 BFPT
}
```

### 8.2 未知参数跳过

对于无法识别的参数表，解析器应当跳过而不是报错，以保证对未知芯片的兼容性。

```c
/**
 * 跳过未知参数表
 * @param table_data 指向参数表数据的缓冲区
 * @param length 参数表长度
 * @param table_id 参数表 ID
 */
void sfdp_skip_unknown_table(const uint8_t *table_data, uint16_t length,
                             uint16_t table_id)
{
    uint8_t vendor_code = 0;

    if ((table_id & 0xFF00) == 0xFF00) {
        vendor_code = table_id & 0x00FF;
        printf("SFDP: Skipping vendor parameter table (Vendor: 0x%02X, Length: %u)\r\n",
               vendor_code, length);
    } else {
        printf("SFDP: Skipping unknown parameter table (ID: 0x%04X, Length: %u)\r\n",
               table_id, length);
    }

    // 可以选择将未知参数数据保存到缓冲区以供后续分析
    // 这对于调试和新芯片支持很有帮助
}
```

### 8.3 驱动适配层设计

为了支持多种硬件平台，SFDP 解析器应当通过抽象接口与底层 SPI 通信层解耦。

```c
// SPI 抽象接口定义
typedef struct spi_bus_ops {
    int (*read)(void *bus, uint32_t address, uint8_t *buffer, uint16_t length);
    int (*write)(void *bus, uint32_t address, const uint8_t *buffer, uint16_t length);
    int (*set_frequency)(void *bus, uint32_t freq);
    int (*set_mode)(void *bus, uint8_t mode);
} spi_bus_ops_t;

// SFDP 驱动配置
typedef struct {
    void                 *spi_bus;         // SPI 总线句柄
    const spi_bus_ops_t *ops;             // SPI 操作接口
    uint32_t             sfdp_base;        // SFDP 基地址（通常为 0）
    uint32_t             max_freq;         // 最大读取频率
} sfdp_driver_config_t;

// SFDP 驱动句柄
typedef struct {
    sfdp_driver_config_t config;
    sfdp_info_t          info;
    uint8_t              initialized;
} sfdp_driver_t;

/**
 * SFDP 驱动初始化
 * @param driver 驱动句柄
 * @param config 驱动配置
 * @return 0 表示成功，负值表示错误
 */
int sfdp_driver_init(sfdp_driver_t *driver, const sfdp_driver_config_t *config)
{
    int ret;

    if (driver == NULL || config == NULL) {
        return -SFDP_ERR_NULL_POINTER;
    }

    memcpy(&driver->config, config, sizeof(sfdp_driver_config_t));

    // 设置 SPI 频率（SFDP 读取通常使用较低频率）
    ret = driver->config.ops->set_frequency(driver->config.spi_bus,
                                             driver->config.max_freq);
    if (ret != 0) {
        return ret;
    }

    // 设置 SPI 模式
    ret = driver->config.ops->set_mode(driver->config.spi_bus, 0);
    if (ret != 0) {
        return ret;
    }

    driver->initialized = 1;

    return 0;
}

/**
 * 检测并解析 Flash 参数
 * @param driver 驱动句柄
 * @param flash_info 输出 Flash 信息
 * @return 0 表示成功，负值表示错误
 */
int sfdp_driver_detect(sfdp_driver_t *driver, flash_info_t *flash_info)
{
    int ret;
    uint8_t sfdp_buffer[256];

    if (!driver->initialized) {
        return -SFDP_ERR_NOT_INITIALIZED;
    }

    // 读取完整的 SFDP 数据
    ret = driver->config.ops->read(driver->config.spi_bus,
                                    driver->config.sfdp_base,
                                    sfdp_buffer, sizeof(sfdp_buffer));
    if (ret != 0) {
        printf("SFDP: Failed to read SFDP data\r\n");
        return ret;
    }

    // 解析 SFDP 数据
    ret = sfdp_parse_all_tables(sfdp_buffer, &driver->info);
    if (ret != 0) {
        printf("SFDP: Failed to parse SFDP data\r\n");
        return ret;
    }

    // 将解析结果转换为 Flash 信息
    flash_info->density = driver->info.bfpt.density_bits / 8;  // 转换为字节
    flash_info->page_size = driver->info.bfpt.page_size;
    flash_info->supports_4k_erase = driver->info.bfpt.erase_info.supports_4k_erase;
    flash_info->max_freq = driver->info.bfpt.timing_info.max_freq_single;
    flash_info->addr_mode = driver->info.bfpt.transfer_mode.addr_mode;

    return 0;
}
```

---

## 9. 驱动代码示例

### 9.1 SFDP 读取函数

以下是完整的 SFDP 读取函数实现：

```c
// SFDP 错误代码定义
#define SFDP_ERR_OK               0
#define SFDP_ERR_NULL_POINTER    -1
#define SFDP_ERR_INVALID_SIGNATURE -2
#define SFDP_ERR_INVALID_LENGTH  -3
#define SFDP_ERR_INVALID_DATA     -4
#define SFDP_ERR_INCOMPATIBLE_VERSION -5
#define SFDP_ERR_UNSUPPORTED_VENDOR  -6
#define SFDP_ERR_NOT_INITIALIZED    -7
#define SFDP_ERR_READ_FAILED        -8

/**
 * 读取 SFDP 参数数据
 * @param bus SPI 总线句柄
 * @param address 读取起始地址
 * @param buffer 输出缓冲区
 * @param length 读取长度
 * @return 0 表示成功，负值表示错误
 */
int sfdp_read(spi_bus_t *bus, uint32_t address, uint8_t *buffer, uint16_t length)
{
    uint8_t cmd[5];
    int ret;

    if (bus == NULL || buffer == NULL) {
        return SFDP_ERR_NULL_POINTER;
    }

    // 构建 SFDP 读取命令
    // 命令格式: 0x5A + 3 字节地址 + 1 字节 dummy
    cmd[0] = 0x5A;  // SFDP READ 命令
    cmd[1] = (uint8_t)(address >> 16);  // 地址字节 2
    cmd[2] = (uint8_t)(address >> 8);  // 地址字节 1
    cmd[3] = (uint8_t)(address);        // 地址字节 0
    cmd[4] = 0x00;  // Dummy cycles (部分芯片需要)

    // 使用 SPI 全双工模式传输
    // 注意：实际实现需要根据具体硬件抽象层调整
    ret = spi_transfer_full(bus, cmd, buffer, length + 4);

    // 跳过命令和地址字节，从 dummy 周期后开始获取数据
    if (ret == 0) {
        // 将数据移动到缓冲区开头（跳过命令和地址）
        memmove(buffer, buffer + 4, length);
    }

    return ret;
}

/**
 * 读取 SFDP 头部信息
 * @param bus SPI 总线句柄
 * @param header 输出头部信息
 * @return 0 表示成功，负值表示错误
 */
int sfdp_read_header(spi_bus_t *bus, uint8_t *header)
{
    return sfdp_read(bus, 0, header, 8);
}

/**
 * 读取完整 SFDP 数据
 * @param bus SPI 总线句柄
 * @param buffer 输出缓冲区（至少 256 字节）
 * @param actual_length 实际读取的长度
 * @return 0 表示成功，负值表示错误
 */
int sfdp_read_full(spi_bus_t *bus, uint8_t *buffer, uint16_t *actual_length)
{
    int ret;
    uint8_t header[8];
    uint8_t num_headers;
    uint16_t max_length = 256;
    uint16_t table_start = 8;  // 参数头从偏移 8 开始
    uint8_t i;

    // 读取 SFDP 头部
    ret = sfdp_read_header(bus, header);
    if (ret != 0) {
        return ret;
    }

    // 验证签名
    ret = sfdp_verify_signature(header);
    if (ret != 0) {
        return ret;
    }

    // 复制头部数据到缓冲区
    memcpy(buffer, header, 8);

    // 获取参数头数量
    num_headers = header[6];

    // 计算需要的缓冲区大小
    // 每个参数头 8 字节，加上参数表数据
    // 简单起见，直接读取 256 字节
    if (max_length > 256) {
        max_length = 256;
    }

    // 读取完整的 SFDP 数据
    ret = sfdp_read(bus, 0, buffer, max_length);
    if (ret != 0) {
        return ret;
    }

    *actual_length = max_length;

    return 0;
}
```

### 9.2 BFPT 解析函数

以下是 BFPT 解析函数的完整实现：

```c
/**
 * 完整 BFPT 解析函数
 * @param bfpt_data 指向 BFPT 数据的缓冲区
 * @param length BFPT 数据长度
 * @param info 输出解析结果
 * @return 0 表示成功，负值表示错误
 */
int bfpt_parse(const uint8_t *bfpt_data, uint16_t length, bfpt_info_t *info)
{
    uint32_t density;

    // 参数验证
    if (bfpt_data == NULL || info == NULL) {
        return SFDP_ERR_NULL_POINTER;
    }

    // 检查最小长度
    if (length < 16) {
        printf("BFPT: Error - Length too short (%u bytes)\r\n", length);
        return SFDP_ERR_INVALID_LENGTH;
    }

    // 清空输出结构
    memset(info, 0, sizeof(bfpt_info_t));

    printf("BFPT: Parsing %u bytes of BFPT data\r\n", length);

    // ========== 解析字节 0-1: 擦除配置 ==========
    info->erase_info.supports_4k_erase = (bfpt_data[0] & 0x0F);
    info->erase_info.erase_cmd_4k = (info->erase_info.supports_4k_erase == 1) ? 0xDA : 0;
    info->erase_info.sector_type = bfpt_data[1];

    printf("  Erase: 4KB erase %s, Sector type: 0x%02X\r\n",
           info->erase_info.supports_4k_erase ? "supported" : "not supported",
           info->erase_info.sector_type);

    // ========== 解析字节 2-3: 架构信息 ==========
    info->transfer_mode.addr_mode = bfpt_data[2] & 0x03;
    info->transfer_mode.continous_read = (bfpt_data[2] >> 5) & 0x01;
    info->transfer_mode.page_program_bits = (bfpt_data[2] >> 4) & 0x01;
    info->transfer_mode.io_width = bfpt_data[3] & 0x0F;
    info->transfer_mode.ddr_support = (bfpt_data[3] >> 4) & 0x0F;

    printf("  Architecture: Addr mode %s, %s\r\n",
           info->transfer_mode.addr_mode == 0 ? "3-byte" :
           info->transfer_mode.addr_mode == 1 ? "4-byte" : "3/4-byte",
           info->transfer_mode.ddr_support ? "DDR supported" : "DDR not supported");

    // ========== 解析字节 4-6: 芯片容量 ==========
    density = bfpt_data[4] | (bfpt_data[5] << 8) | (bfpt_data[6] << 16);
    info->density_bits = density + 1;  // 密度 = 密度位 + 1

    printf("  Density: %s (%u bits)\r\n",
           bfpt_format_capacity(info->density_bits), info->density_bits);

    // ========== 解析字节 8: 写入粒度 ==========
    info->write_granularity = bfpt_data[8];
    if (info->write_granularity == 0) {
        info->write_granularity = 1;
    }
    printf("  Write granularity: %u bytes\r\n", info->write_granularity);

    // ========== 解析字节 10-11: 擦除时间 ==========
    info->erase_time_4k = bfpt_data[10];
    info->erase_time_block = bfpt_data[11];
    printf("  Erase time: 4KB=%ums, Block=%ums\r\n",
           info->erase_time_4k, info->erase_time_block);

    // ========== 解析字节 12-13: 最大时钟频率 ==========
    uint8_t clock_code = bfpt_data[12] & 0x1F;
    info->timing_info.max_freq_single = (clock_code < 15) ?
        clock_freq_table[clock_code] : 0;

    clock_code = bfpt_data[13] & 0x1F;
    info->timing_info.max_freq_quad = (clock_code < 15) ?
        clock_freq_table[clock_code] : 0;

    printf("  Max frequency: Single=%uMHz, Quad=%uMHz\r\n",
           info->timing_info.max_freq_single,
           info->timing_info.max_freq_quad);

    // ========== 解析字节 14-15: 时序参数 ==========
    uint8_t timing = bfpt_data[14];
    info->timing_info.tCLH = (timing >> 4) & 0x0F;
    info->timing_info.tCLL = timing & 0x0F;

    timing = bfpt_data[15];
    info->timing_info.tDS = (timing >> 4) & 0x0F;
    info->timing_info.tDH = timing & 0x0F;

    // ========== 解析扩展参数 (如果 BFPT 长度为 32 字节) ==========
    if (length >= 32) {
        // 字节 16-17: 新型擦除时间参数
        // 字节 18-19: 4 字节地址参数（如果存在）
        // 字节 20-21: 保留

        // 字节 22-23: Quad Enable 信息
        info->quad_enable_info = bfpt_data[0x42];

        // 字节 24-25 (偏移 0x43): 页面大小
        info->page_size = bfpt_data[0x43];
        if (info->page_size == 0) {
            info->page_size = 256;
        }

        printf("  Page size: %u bytes\r\n", info->page_size);
    } else {
        info->page_size = 256;  // 默认值
    }

    info->version = bfpt_data[2];  // BFPT 主版本号

    printf("BFPT: Parsing completed successfully\r\n");

    return SFDP_ERR_OK;
}
```

### 9.3 扇区表构建函数

以下是扇区表构建函数的完整实现：

```c
/**
 * 构建扇区查找表
 * 根据扇区映射表构建用于快速查找的扇区信息表
 * @param sector_map 解析后的扇区映射信息
 * @param table 输出扇区查找表
 * @param table_size 查找表大小
 * @return 0 表示成功，负值表示错误
 */
int sector_build_lookup_table(const sector_map_t *sector_map,
                               sector_lookup_entry_t *table,
                               uint32_t table_size)
{
    uint8_t i;
    uint32_t current_addr = 0;
    uint32_t entry_idx = 0;

    if (sector_map == NULL || table == NULL) {
        return SFDP_ERR_NULL_POINTER;
    }

    printf("Building sector lookup table for %u regions\r\n",
           sector_map->header.num_regions);

    // 遍历每个区域
    for (i = 0; i < sector_map->header.num_regions; i++) {
        const sector_info_t *region = &sector_map->regions[i];
        uint32_t sector_addr;

        printf("  Region %u: %s x %u\r\n",
               i, sector_get_type_name(region->type),
               region->count);

        // 为区域中的每个扇区创建查找表项
        for (sector_addr = 0; sector_addr < region->count; sector_addr++) {
            uint32_t sector_start = current_addr + (sector_addr * region->size);

            // 检查是否超出查找表大小
            if (entry_idx >= table_size) {
                printf("Sector table overflow!\r\n");
                break;
            }

            // 创建查找表项
            table[entry_idx].start_addr = sector_start;
            table[entry_idx].size = region->size;
            table[entry_idx].type = region->type;
            table[entry_idx].region_id = i;

            entry_idx++;
        }

        current_addr += region->total_size;
    }

    printf("Sector lookup table built: %u entries\r\n", entry_idx);

    return SFDP_ERR_OK;
}

/**
 * 查找地址所在的扇区信息
 * @param table 扇区查找表
 * @param table_size 查找表大小
 * @param address 要查询的地址
 * @param info 输出扇区信息
 * @return 0 表示成功，负值表示未找到
 */
int sector_lookup(const sector_lookup_entry_t *table,
                  uint32_t table_size,
                  uint32_t address,
                  sector_lookup_entry_t *info)
{
    uint32_t i;
    uint32_t prev_start = 0;
    uint32_t prev_size = 0;

    if (table == NULL || info == NULL) {
        return SFDP_ERR_NULL_POINTER;
    }

    // 简单的线性查找
    // 对于大规模扇区表，可以考虑使用二分查找优化
    for (i = 0; i < table_size; i++) {
        if (address < table[i].start_addr) {
            // 找到前一个扇区
            if (i > 0) {
                *info = table[i - 1];
                return 0;
            }
            break;
        }

        // 记录最后一个条目的信息作为后备
        prev_start = table[i].start_addr;
        prev_size = table[i].size;
    }

    // 检查地址是否在最后一个扇区内
    if (address >= prev_start && address < prev_start + prev_size) {
        *info = table[table_size - 1];
        return 0;
    }

    printf("Sector lookup failed for address 0x%08X\r\n", address);

    return SFDP_ERR_INVALID_ADDRESS;
}

/**
 * 获取地址对应的擦除命令和大小
 * @param table 扇区查找表
 * @param table_size 查找表大小
 * @param address 要操作的地址
 * @param erase_cmd 输出擦除命令
 * @param erase_size 输出擦除大小
 * @return 0 表示成功，负值表示错误
 */
int sector_get_erase_params(const sector_lookup_entry_t *table,
                             uint32_t table_size,
                             uint32_t address,
                             uint8_t *erase_cmd,
                             uint32_t *erase_size)
{
    int ret;
    sector_lookup_entry_t info;

    ret = sector_lookup(table, table_size, address, &info);
    if (ret != 0) {
        return ret;
    }

    // 根据扇区类型确定擦除命令和大小
    switch (info.type) {
        case SECTOR_TYPE_4KB:
            *erase_cmd = 0x20;  // 4KB 扇区擦除命令
            *erase_size = 4 * 1024;
            break;

        case SECTOR_TYPE_32KB:
            *erase_cmd = 0x52;  // 32KB 块擦除命令
            *erase_size = 32 * 1024;
            break;

        case SECTOR_TYPE_64KB:
            *erase_cmd = 0xD8;  // 64KB 块擦除命令
            *erase_size = 64 * 1024;
            break;

        default:
            printf("Unknown sector type: %u\r\n", info.type);
            return SFDP_ERR_INVALID_DATA;
    }

    return 0;
}
```

---

## 本章小结

本章详细介绍了 SFDP 参数解析驱动的设计与实现，涵盖了从底层读取到高级解析的完整技术栈。

在驱动架构部分，我们分析了 SFDP 解析器的设计目标和模块划分，包括 SFDP 读取模块、头部解析模块、参数表遍历模块、BFPT 解析模块、扇区映射解析模块和厂商参数处理模块。这种模块化设计确保了驱动的可维护性和可扩展性。

在 SFDP 读取时序部分，我们详细讲解了 0x5A 命令的完整序列、3 字节和 4 字节地址模式的处理，以及读取流程的实现细节。正确的时序控制是可靠参数读取的基础。

在头部解析与验证部分，我们实现了签名字节验证（0x50444653）、版本号解析和参数头数量提取。签名验证是确保数据有效性的第一道防线。

在参数表遍历机制部分，我们设计了完整的参数头解析和遍历算法，支持根据参数 ID 自动调度相应的解析器处理不同类型的参数表。

在 BFPT 解析部分，我们详细实现了芯片容量解析、擦除块大小解析、时序参数提取和传输模式支持等核心功能。这些参数是配置 Flash 驱动的基础。

在扇区映射表解析部分，我们实现了 4KB 扇区参数解析、扇区类型解析和完整地址布局的构建。这对于具有复杂层次结构的 Flash 芯片尤为重要。

在厂商特定参数处理部分，我们针对 Winbond 和 Micron 等主要厂商的扩展参数实现了专门的解析函数，提供了对非标准特性的支持。

在兼容性与扩展部分，我们讨论了版本兼容性处理、未知参数跳过和驱动适配层设计等重要话题，确保驱动能够在各种硬件平台上稳定运行。

最后，我们提供了完整的驱动代码示例，包括 SFDP 读取函数、BFPT 解析函数和扇区表构建函数，可直接应用于实际项目开发。

通过本章的学习，读者应当能够独立实现功能完善的 SFDP 参数解析驱动，为构建通用的 Nor Flash 驱动奠定坚实基础。

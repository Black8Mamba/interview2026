# 启动加载器集成

启动加载器（Bootloader）是嵌入式系统中的关键软件组件，负责在系统上电或复位后初始化硬件、加载应用程序并转跳执行。对于基于 Nor Flash 的嵌入式系统，启动加载器需要与 Flash 驱动紧密配合，实现从外部 Flash 直接执行代码（XIP）或将代码复制到内存后执行两种模式。本章将详细介绍 XIP 实现原理、启动镜像布局设计、镜像校验与回退机制、启动流程以及集成代码示例，帮助开发者构建可靠的启动系统。

---

## 1. XIP（Execute-In-Place）实现原理

XIP（Execute-In-Place，原地执行）是一种允许代码直接在外部 Flash 存储器中运行而无需复制到 RAM 的技术。这种技术广泛应用于嵌入式系统中，特别是在成本敏感或 RAM 资源受限的场景下。XIP 技术能够显著减少启动时间并节省宝贵的 RAM 空间，但同时也需要硬件和软件的协同支持。

### 1.1 XIP 技术概述

在传统的启动流程中，Bootloader 会将存储在 Flash 中的应用程序代码复制到 RAM 中，然后从 RAM 中执行代码。这种方式的优势在于 RAM 的访问速度远高于 Flash，能够提供更快的代码执行速度。然而，其缺点是需要占用大量的 RAM 空间来存储代码副本，对于 RAM 资源有限的嵌入式系统来说是一个负担。

XIP 技术解决了这一问题。在 XIP 模式下，CPU 直接从外部 Flash 读取指令并执行，无需将代码复制到 RAM。要实现 XIP，需要满足以下硬件条件：第一，外部 Flash 必须支持 XIP 模式，通常通过特定的接口（如 SPI XIP、OPI XIP 或并行接口）连接到 CPU；第二，CPU 的内存映射需要将外部 Flash 映射到可执行代码的地址空间；第三，Flash 的读取速度需要满足 CPU 的执行需求，或者通过缓存机制来弥补速度差异。

XIP 技术的优势包括：无需等待代码加载到 RAM，缩短启动时间；不占用 RAM 存储代码，节省宝贵的 RAM 资源；支持固件在线升级，可以在不移动代码的情况下更新 Flash 中的内容；系统复位后应用程序立即可用，无需重新加载。然而，XIP 也有其局限性：Flash 的读取速度通常低于 RAM，可能影响代码执行性能；某些需要动态修改的代码段（如自修改代码）无法在 XIP 模式下使用；长时间运行可能增加 Flash 的磨损。

### 1.2 XIP 硬件接口

XIP 的实现依赖于特定的硬件接口。不同的 Flash 类型和 CPU 架构使用不同的 XIP 接口，了解这些接口对于正确实现 XIP 至关重要。

**SPI XIP 接口**是最常见的 XIP 模式之一，适用于使用 SPI 接口连接的外部 Flash。在 SPI XIP 模式下，Flash 芯片提供专门的 XIP 命令，允许 CPU 通过内存映射访问方式直接读取 Flash 数据。典型的 SPI XIP 配置使用四线制（QSPI）或八线制（OPI）接口，以提供足够的带宽。

```c
/**
 * SPI XIP 模式配置结构
 * 用于配置 Flash 芯片进入 XIP 模式
 */
typedef struct {
    uint8_t  mode;              // XIP 模式 (0: Single, 1: Dual, 2: Quad, 3: Octal)
    uint8_t  addr_bytes;         // 地址字节数 (3 或 4)
    uint8_t  dummy_cycles;      // 虚拟周期数
    uint32_t read_cmd;           // XIP 读取命令
    uint32_t enable_cmd;         // XIP 使能命令
    uint32_t disable_cmd;        // XIP 禁能命令
} nor_xip_config_t;

/**
 * 常见 SPI Flash XIP 配置
 */
static const nor_xip_config_t xip_configs[] = {
    // Single SPI XIP
    {
        .mode = 0,
        .addr_bytes = 3,
        .dummy_cycles = 8,
        .read_cmd = 0x03,        // READ command
        .enable_cmd = 0x00,
        .disable_cmd = 0x00
    },
    // Dual SPI XIP
    {
        .mode = 1,
        .addr_bytes = 3,
        .dummy_cycles = 4,
        .read_cmd = 0xBB,        // DREAD command
        .enable_cmd = 0xB7,      // Enter XIP
        .disable_cmd = 0xE9      // Exit XIP
    },
    // Quad SPI XIP
    {
        .mode = 2,
        .addr_bytes = 3,
        .dummy_cycles = 6,
        .read_cmd = 0xEB,        // QREAD command
        .enable_cmd = 0x38,      // Enter QPI XIP
        .disable_cmd = 0xFF      // Exit QPI XIP
    }
};
```

**并行 XIP 接口**使用传统的并行 NOR Flash 接口（如 NOR FLASH 接口），直接将 Flash 映射到 CPU 的内存空间中。在这种模式下，Flash 像内存一样被访问，CPU 可以直接执行存储在 Flash 中的代码。并行 XIP 通常提供更高的带宽和更低的访问延迟，但需要更多的引脚和更复杂的硬件设计。

```c
/**
 * 并行 XIP 配置
 */
typedef struct {
    uint32_t base_addr;         // Flash 映射基地址
    uint32_t size;              // Flash 大小
    uint8_t  bus_width;         // 总线宽度 (8/16/32)
    uint8_t  wait_states;       // 等待状态数
    bool     burst_enable;       // 突发模式使能
    bool     cache_enable;       // 缓存使能
} parallel_xip_config_t;
```

### 1.3 XIP 软件实现

XIP 的软件实现涉及多个层面，包括 Flash 驱动配置、内存映射设置、缓存管理和应用程序链接脚本。

**Flash 驱动 XIP 配置**是实现 XIP 的第一步。驱动需要支持 XIP 模式切换，使 Flash 能够在普通访问模式和 XIP 模式之间切换。

```c
/**
 * XIP 模式上下文
 */
typedef struct {
    nor_device_t *device;        // Flash 设备
    nor_xip_config_t config;     // XIP 配置
    bool is_enabled;             // XIP 是否已使能
    uint32_t xip_base;           // XIP 映射基地址
} nor_xip_context_t;

/**
 * 使能 XIP 模式
 *
 * @param ctx XIP 上下文
 * @return 0 成功
 */
int nor_xip_enable(nor_xip_context_t *ctx)
{
    int ret;
    uint8_t sr1, sr2;
    nor_device_t *dev = ctx->device;

    if (!ctx || ctx->is_enabled) {
        return 0;  // 已使能
    }

    /* 读取当前状态寄存器 */
    ret = nor_read_status_reg(dev, &sr1);
    if (ret < 0) return ret;

    ret = nor_read_status_reg2(dev, &sr2);
    if (ret < 0) return ret;

    /* 根据 XIP 模式配置 */
    switch (ctx->config.mode) {
        case 0:  // Single SPI XIP
            /* Single 模式通常不需要特殊配置 */
            break;

        case 1:  // Dual SPI XIP
            /* 使能 Dual 模式 */
            sr2 |= 0x01;  // DUAL bit
            ret = nor_write_both_status_regs(dev, sr1, sr2);
            if (ret < 0) return ret;

            /* 发送 XIP 使能命令 */
            if (ctx->config.enable_cmd != 0) {
                nor_write_enable(dev);
                nor_xip_send_cmd(dev, ctx->config.enable_cmd);
            }
            break;

        case 2:  // Quad SPI XIP
            /* 使能 Quad 模式 */
            sr2 |= 0x02;  // QUAD bit
            ret = nor_write_both_status_regs(dev, sr1, sr2);
            if (ret < 0) return ret;

            /* 发送 XIP 使能命令 */
            if (ctx->config.enable_cmd != 0) {
                nor_write_enable(dev);
                nor_xip_send_cmd(dev, ctx->config.enable_cmd);
            }
            break;

        default:
            return -NOR_ERR_INVALID_PARAM;
    }

    /* 配置内存映射 */
    ret = nor_xip_setup_mapping(ctx);
    if (ret < 0) return ret;

    ctx->is_enabled = true;
    return 0;
}

/**
 * 禁能 XIP 模式
 *
 * @param ctx XIP 上下文
 * @return 0 成功
 */
int nor_xip_disable(nor_xip_context_t *ctx)
{
    int ret;
    nor_device_t *dev = ctx->device;

    if (!ctx || !ctx->is_enabled) {
        return 0;  // 未使能
    }

    /* 发送 XIP 禁能命令 */
    if (ctx->config.disable_cmd != 0) {
        nor_xip_send_cmd(dev, ctx->config.disable_cmd);
    }

    /* 禁能 Quad 模式 */
    uint8_t sr1, sr2;
    ret = nor_read_status_reg(dev, &sr1);
    if (ret < 0) return ret;

    ret = nor_read_status_reg2(dev, &sr2);
    if (ret < 0) return ret;

    sr2 &= ~0x03;  // 清除 DUAL/QUAD 位
    ret = nor_write_both_status_regs(dev, sr1, sr2);

    /* 禁用内存映射 */
    nor_xip_remove_mapping(ctx);

    ctx->is_enabled = false;
    return ret;
}

/**
 * 从 XIP 地址读取数据
 * 用于 XIP 模式下的数据读取
 */
int nor_xip_read(nor_xip_context_t *ctx, uint32_t offset,
                 uint8_t *buf, uint32_t len)
{
    uint32_t xip_addr = ctx->xip_base + offset;

    /* 直接从内存映射地址读取 */
    memcpy(buf, (void *)xip_addr, len);

    return (int)len;
}
```

### 1.4 XIP 缓存管理

由于 Flash 的访问速度通常低于 CPU 的执行速度，系统通常会使用缓存（Cache）来提高 XIP 性能。缓存管理是 XIP 实现中的重要环节，需要平衡性能和一致性。

```c
/**
 * XIP 缓存配置
 */
typedef struct {
    bool     icache_enable;      // 指令缓存使能
    bool     dcache_enable;      // 数据缓存使能
    uint32_t cache_size;         // 缓存大小 (KB)
    uint8_t  cache_policy;       // 缓存策略
} nor_xip_cache_config_t;

/* 缓存策略定义 */
#define NOR_XIP_CACHE_WRITE_THROUGH  0  // 写穿透
#define NOR_XIP_CACHE_WRITE_BACK     1  // 写回

/**
 * 配置 XIP 缓存
 */
int nor_xip_cache_configure(nor_xip_cache_config_t *config)
{
    /* 使能指令缓存 */
    if (config->icache_enable) {
        __enable_icache();
    } else {
        __disable_icache();
    }

    /* 使能数据缓存 */
    if (config->dcache_enable) {
        __enable_dcache();
    } else {
        __disable_dcache();
    }

    /* 配置缓存策略 */
    if (config->dcache_enable) {
        SCB->CCR |= (config->cache_policy == NOR_XIP_CACHE_WRITE_BACK)
                    ? SCBCCR_DC : 0;
    }

    return 0;
}

/**
 * 刷新 XIP 缓存
 * 在执行代码前刷新缓存，确保读取最新数据
 */
void nor_xip_cache_invalidate(uint32_t addr, uint32_t len)
{
    uint32_t start = addr & ~0x1F;  // 对齐到 32 字节
    uint32_t end = (addr + len + 0x1F) & ~0x1F;

    for (uint32_t line = start; line < end; line += 32) {
        /* 逐行失效缓存 */
        __builtin_arm_dccmvau(line);
    }

    /* 数据同步指令 */
    __dsb(15);
    __isb(15);
}
```

---

## 2. 启动镜像布局设计

启动镜像（Boot Image）是存储在 Flash 中的完整启动数据包，包含 Bootloader、应用程序和必要的元数据。合理的镜像布局设计是保证系统可靠启动和升级的基础。本节将详细介绍启动镜像的布局结构、头部信息设计以及各分区的规划。

### 2.1 镜像布局概述

启动镜像的布局设计需要考虑多个因素，包括 Flash 的物理特性、启动流程的要求、固件升级的便利性以及系统安全性。一个典型的启动镜像布局通常包含多个区域，每个区域都有其特定的用途和访问模式。

基本的镜像布局包括引导程序区、应用程序区、配置数据区和备份区。引导程序区存放第一级启动代码（BL1），负责最基本的硬件初始化和第二级引导程序的加载；应用程序区存放主应用程序代码，可能包含多个版本；配置数据区存放启动参数、校准数据和其他配置信息；备份区用于存放旧版本固件，以便在升级失败时回退。

```c
/**
 * 启动镜像布局结构
 *
 * 典型的 Flash 分区布局:
 * +------------------+ 0x00000000
 * |   Boot Header    |  (镜像头部信息)
 * +------------------+ 0x00000100
 * |   Bootloader     |  (BL1 - 第一级引导)
 * |   (64KB)         |
 * +------------------+ 0x00010100
 * |   App Header     |  (应用程序头部)
 * +------------------+ 0x00010200
 * |   Application    |  (主应用程序)
 * |   (最大可用)     |
 * +------------------+ 0x000F0200
 * |   Config Data    |  (配置数据区)
 * |   (16KB)         |
 * +------------------+ 0x000F4200
 * |   Backup/Failover|  (备份区)
 * |   (64KB)         |
 * +------------------+ 0x00100000
 */
typedef struct {
    uint32_t boot_offset;        // Bootloader 偏移
    uint32_t boot_size;         // Bootloader 大小
    uint32_t app_offset;        // 应用程序偏移
    uint32_t app_size;          // 应用程序大小
    uint32_t config_offset;     // 配置数据偏移
    uint32_t config_size;       // 配置数据大小
    uint32_t backup_offset;     // 备份区偏移
    uint32_t backup_size;       // 备份区大小
    uint32_t total_size;        // 总镜像大小
} boot_image_layout_t;
```

### 2.2 镜像头部设计

镜像头部（Boot Header）是启动镜像的关键组成部分，包含了验证镜像完整性、确定加载位置、选择启动模式等重要信息。头部设计需要考虑扩展性和兼容性，以便支持未来的功能扩展。

```c
/**
 * 启动镜像头部结构
 * 存储在 Flash 的起始位置
 *
 * 字节序: 小端序 (Little Endian)
 */
typedef struct __attribute__((packed)) {
    /* 镜像标识 */
    uint32_t magic;              // 魔数: 0x424F4F54 ("BOOT")
    uint16_t version;            // 镜像格式版本
    uint16_t header_size;        // 头部大小

    /* 镜像信息 */
    uint32_t image_size;         // 整个镜像大小
    uint32_t load_addr;          // 加载地址
    uint32_t entry_point;        // 入口点地址

    /* 校验信息 */
    uint32_t crc32;              // CRC32 校验值
    uint16_t crc_type;           // 校验类型
    uint16_t signature_size;     // 签名大小

    /* 启动参数 */
    uint32_t boot_flags;         // 启动标志
    uint32_t timeout;            // 启动超时(ms)
    uint8_t  primary_sel;       // 主启动分区
    uint8_t  fallback_sel;       // 回退分区
    uint16_t reserved;           // 保留

    /* Flash 参数 */
    uint32_t flash_offset;       // Flash 偏移
    uint32_t flash_size;         // Flash 大小

    /* 扩展信息 */
    uint32_t ext_magic;          // 扩展魔数
    uint32_t build_time;         // 构建时间戳
    uint32_t git_commit;         // Git 提交哈希
} boot_header_t;

/* 镜像魔数 */
#define BOOT_MAGIC              0x424F4F54  // "BOOT"
#define BOOT_MAGIC_V1           0x424F4F31  // "BOO1"
#define BOOT_MAGIC_V2           0x424F4F32  // "BOO2"

/* 校验类型 */
#define BOOT_CRC_NONE           0           // 无校验
#define BOOT_CRC32              1           // CRC32
#define BOOT_CRC16             2           // CRC16
#define BOOT_SHA256            3           // SHA-256

/* 启动标志 */
#define BOOT_FLAG_XIP           (1 << 0)    // XIP 模式启动
#define BOOT_FLAG_COMPRESSED    (1 << 1)    // 压缩镜像
#define BOOT_FLAG_ENCRYPTED     (1 << 2)    // 加密镜像
#define BOOT_FLAG_SIGNED        (1 << 3)    // 已签名镜像
#define BOOT_FLAG_FORCE_UPDATE  (1 << 4)    // 强制更新
#define BOOT_FLAG_SAFE_MODE    (1 << 5)    // 安全模式
#define BOOT_FLAG_TEST         (1 << 6)    // 测试模式
```

### 2.3 分区规划策略

分区规划需要根据具体的应用场景和硬件资源进行优化。不同的系统可能需要不同的分区策略，但通常需要平衡可靠性、升级便利性和存储效率。

```c
/**
 * 分区表条目
 */
typedef struct {
    char     name[16];          // 分区名称
    uint32_t offset;             // 偏移地址
    uint32_t size;               // 分区大小
    uint8_t  type;               // 分区类型
    uint8_t  flags;             // 分区标志
    uint16_t reserved;           // 保留
} partition_entry_t;

/* 分区类型 */
#define PART_TYPE_BOOTLOADER    0x01        // 引导程序
#define PART_TYPE_APPLICATION  0x02        // 应用程序
#define PART_TYPE_CONFIG        0x03        // 配置数据
#define PART_TYPE_DATA          0x04        // 用户数据
#define PART_TYPE_BACKUP        0x05        // 备份分区
#define PART_TYPE_FACTORY       0x06        // 出厂固件
#define PART_TYPE_LOG           0x10        // 日志分区

/* 分区标志 */
#define PART_FLAG_READONLY      (1 << 0)    // 只读
#define PART_FLAG_WRITEABLE     (1 << 1)    // 可写
#define PART_FLAG_BOOTABLE      (1 << 2)    // 可启动
#define PART_FLAG_ENCRYPTED    (1 << 3)    // 加密
#define PART_FLAG_COMPRESSED   (1 << 4)    // 压缩

/**
 * 默认分区表配置
 */
static const partition_entry_t default_partitions[] = {
    {
        .name = "bootloader",
        .offset = 0x00000000,
        .size = 0x00010000,      // 64KB
        .type = PART_TYPE_BOOTLOADER,
        .flags = PART_FLAG_READONLY | PART_FLAG_BOOTABLE
    },
    {
        .name = "app_primary",
        .offset = 0x00010000,
        .size = 0x000E0000,      // 896KB
        .type = PART_TYPE_APPLICATION,
        .flags = PART_FLAG_BOOTABLE
    },
    {
        .name = "app_backup",
        .offset = 0x000F0000,
        .size = 0x00060000,      // 384KB
        .type = PART_TYPE_APPLICATION,
        .flags = 0
    },
    {
        .name = "config",
        .offset = 0x00150000,
        .size = 0x00004000,      // 16KB
        .type = PART_TYPE_CONFIG,
        .flags = PART_FLAG_WRITEABLE
    },
    {
        .name = "factory",
        .offset = 0x00154000,
        .size = 0x00010000,      // 64KB
        .type = PART_TYPE_FACTORY,
        .flags = PART_FLAG_READONLY
    },
    {
        .name = "data",
        .offset = 0x00164000,
        .size = 0x00020000,      // 128KB
        .type = PART_TYPE_DATA,
        .flags = PART_FLAG_WRITEABLE
    }
};
```

### 2.4 链接脚本配置

链接脚本定义了代码和数据在内存（对于 XIP 模式则是 Flash）中的布局。正确的链接脚本配置是确保 XIP 正常工作的关键。

```ld
/* XIP 模式链接脚本示例 */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00010000, LENGTH = 896K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* 代码段 - 放在 Flash 中 (XIP) */
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))    /* 中断向量表 */
        *(.text)
        *(.text.*)
        *(.rodata)              /* 只读数据 */
        *(.rodata.*)
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* 数据段 - 复制到 RAM */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    /* 未初始化数据段 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM

    /* XIP 相关段 */
    .xip_text :
    {
        . = ALIGN(4);
        *(.xip_text)
        *(.xip_text.*)
    } >FLASH

    .xip_rodata :
    {
        . = ALIGN(4);
        *(.xip_rodata)
        *(.xip_rodata.*)
    } >FLASH

    /* 启动堆栈 */
    .heap :
    {
        . = ALIGN(8);
        PROVIDE(__heap_start = .);
        . = . + 0x2000;          // 8KB 堆
        . = ALIGN(8);
        PROVIDE(__heap_end = .);
    } >RAM

    .stack :
    {
        . = ALIGN(8);
        PROVIDE(__stack_top = .);
        . = . + 0x1000;          // 4KB 栈
    } >RAM
}
```

---

## 3. 镜像校验与回退机制

镜像校验是确保启动固件完整性和可信性的关键机制。回退机制则提供了在固件升级失败时恢复到正常工作状态的能力。这两种机制共同构成了嵌入式系统可靠启动的保障体系。

### 3.1 校验算法实现

镜像校验通常使用 CRC 或哈希算法来检测数据损坏。CRC（循环冗余校验）算法计算速度快、实现简单，适用于检测随机错误；哈希算法（如 SHA-256）安全性更高，还能检测恶意篡改。

```c
/**
 * CRC32 校验实现
 * 使用标准 CRC-32/MPEG-2 多项式
 */
#define CRC32_POLYNOMIAL    0x04C11DB7
#define CRC32_INIT          0xFFFFFFFF

/**
 * CRC32 查表法实现
 * 预计算表以提高性能
 */
static uint32_t crc32_table[256];
static bool crc32_table_init = false;

/**
 * 初始化 CRC32 查表
 */
void crc32_init_table(void)
{
    if (crc32_table_init) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i << 24;
        for (uint32_t j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ CRC32_POLYNOMIAL;
            } else {
                crc = crc << 1;
            }
        }
        crc32_table[i] = crc;
    }

    crc32_table_init = true;
}

/**
 * 计算数据 CRC32
 *
 * @param data 输入数据
 * @param len 数据长度
 * @return CRC32 校验值
 */
uint32_t crc32_calculate(const uint8_t *data, uint32_t len)
{
    uint32_t crc = CRC32_INIT;

    if (!crc32_table_init) {
        crc32_init_table();
    }

    for (uint32_t i = 0; i < len; i++) {
        uint8_t index = (crc >> 24) ^ data[i];
        crc = (crc << 8) ^ crc32_table[index];
    }

    return crc ^ CRC32_INIT;
}

/**
 * 计算 Flash 区域 CRC32
 *
 * @param dev Flash 设备
 * @param offset 起始偏移
 * @param len 计算长度
 * @return CRC32 校验值
 */
uint32_t nor_crc32_region(nor_device_t *dev, uint32_t offset, uint32_t len)
{
    uint8_t buffer[256];
    uint32_t crc = CRC32_INIT;
    uint32_t processed = 0;

    crc32_init_table();

    while (processed < len) {
        uint32_t chunk = (len - processed > sizeof(buffer))
                         ? sizeof(buffer) : (len - processed);

        nor_read(dev, offset + processed, buffer, chunk);

        for (uint32_t i = 0; i < chunk; i++) {
            uint8_t index = (crc >> 24) ^ buffer[i];
            crc = (crc << 8) ^ crc32_table[index];
        }

        processed += chunk;
    }

    return crc ^ CRC32_INIT;
}

/**
 * SHA-256 校验实现
 * 用于更安全的镜像验证
 */
typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t  buffer[64];
} sha256_context_t;

#define SHA256_BLOCK_SIZE  64
#define SHA256_DIGEST_SIZE 32

/**
 * SHA-256 初始化
 */
void sha256_init(sha256_context_t *ctx)
{
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
}

/**
 * SHA-256 数据处理
 */
void sha256_update(sha256_context_t *ctx, const uint8_t *data, uint32_t len);

/**
 * SHA-256 完成
 */
void sha256_final(sha256_context_t *ctx, uint8_t *digest);

/**
 * 验证镜像完整性
 *
 * @param header 镜像头部
 * @param dev Flash 设备
 * @return 0 验证成功，非0 验证失败
 */
int boot_verify_image(const boot_header_t *header, nor_device_t *dev)
{
    uint32_t calculated_crc;
    uint32_t stored_crc;

    /* 检查魔数 */
    if (header->magic != BOOT_MAGIC) {
        boot_error("Invalid magic: 0x%08X", header->magic);
        return -BOOT_ERR_INVALID_IMAGE;
    }

    /* 检查版本兼容性 */
    if (header->version > BOOT_FORMAT_VERSION) {
        boot_error("Unsupported version: %d", header->version);
        return -BOOT_ERR_UNSUPPORTED_VERSION;
    }

    /* 根据校验类型计算校验值 */
    switch (header->crc_type) {
        case BOOT_CRC32:
            stored_crc = header->crc32;
            /* 头部中 CRC 字段置零后再计算 */
            calculated_crc = nor_crc32_region(dev,
                header->flash_offset,
                header->image_size);
            break;

        case BOOT_SHA256:
            {
                sha256_context_t sha;
                uint8_t digest[SHA256_DIGEST_SIZE];
                sha256_init(&sha);
                /* 读取并计算 */
                sha256_final(&sha, digest);
                /* 比较签名 */
                return memcmp(digest,
                    ((uint8_t *)header + header->header_size),
                    SHA256_DIGEST_SIZE);
            }

        default:
            boot_error("Unknown CRC type: %d", header->crc_type);
            return -BOOT_ERR_INVALID_CRC;
    }

    /* 比较 CRC */
    if (calculated_crc != stored_crc) {
        boot_error("CRC mismatch: calculated=0x%08X, stored=0x%08X",
                   calculated_crc, stored_crc);
        return -BOOT_ERR_CRC_MISMATCH;
    }

    return 0;
}
```

### 3.2 双镜像策略

双镜像策略是实现可靠固件升级的常用方法。该策略在 Flash 中维护两个固件分区（新固件和旧固件），新固件先下载到备份分区，验证通过后再切换启动分区。如果新固件启动失败，系统可以回退到旧固件。

```c
/**
 * 双镜像状态管理
 */
typedef struct {
    uint8_t  active_slot;        // 当前活动分区 (0 或 1)
    uint8_t  update_slot;        // 升级分区 (0 或 1)
    uint8_t  boot_slot;          // 启动分区
    uint8_t  retry_count;       // 启动重试计数
    uint32_t active_version;    // 当前活动版本
    uint32_t update_version;    // 升级版本
    uint32_t boot_attempts;     // 启动尝试次数
} dual_image_context_t;

/* 分区索引 */
#define SLOT_A    0
#define SLOT_B    1
#define MAX_SLOTS 2

/**
 * 镜像槽位信息
 */
typedef struct {
    uint32_t offset;             // Flash 偏移
    uint32_t size;               // 分区大小
    bool     valid;              // 是否有有效镜像
    uint32_t version;            // 固件版本
    uint32_t build_time;         // 构建时间
    uint32_t crc;               // CRC 校验值
} image_slot_info_t;

/**
 * 获取指定槽位信息
 */
int boot_get_slot_info(uint8_t slot, image_slot_info_t *info)
{
    nor_device_t *dev = nor_get_device();
    uint32_t slot_offset;
    boot_header_t header;

    if (slot >= MAX_SLOTS) {
        return -BOOT_ERR_INVALID_PARAM;
    }

    /* 计算槽位偏移 */
    slot_offset = (slot == SLOT_A) ?
                  config.app_a_offset : config.app_b_offset;

    /* 读取头部 */
    nor_read(dev, slot_offset, &header, sizeof(header));

    /* 填充槽位信息 */
    info->offset = slot_offset;
    info->size = (slot == SLOT_A) ?
                 config.app_a_size : config.app_b_size;

    /* 检查有效性 */
    if (header.magic == BOOT_MAGIC) {
        info->valid = true;
        info->version = header.version;
        info->build_time = header.build_time;
        info->crc = header.crc32;
    } else {
        info->valid = false;
        info->version = 0;
        info->build_time = 0;
        info->crc = 0;
    }

    return 0;
}

/**
 * 选择最佳启动分区
 *
 * @return 分区索引 (SLOT_A 或 SLOT_B)
 */
int boot_select_boot_slot(void)
{
    image_slot_info_t slot_a, slot_b;
    int ret;

    /* 获取两个槽位信息 */
    ret = boot_get_slot_info(SLOT_A, &slot_a);
    if (ret < 0) return ret;

    ret = boot_get_slot_info(SLOT_B, &slot_b);
    if (ret < 0) return ret;

    /* 根据槽位状态选择 */
    if (!slot_a.valid && !slot_b.valid) {
        boot_error("No valid boot image found!");
        return -BOOT_ERR_NO_VALID_IMAGE;
    }

    if (slot_a.valid && !slot_b.valid) {
        return SLOT_A;
    }

    if (!slot_a.valid && slot_b.valid) {
        return SLOT_B;
    }

    /* 两者都有效时，优先选择版本号更高的 */
    if (slot_a.version >= slot_b.version) {
        return SLOT_A;
    } else {
        return SLOT_B;
    }
}
```

### 3.3 回退机制实现

回退机制是双镜像策略的关键组成部分。当新固件启动失败时，系统需要自动或手动回退到旧固件，以确保系统的可用性。

```c
/**
 * 回退策略配置
 */
typedef struct {
    uint8_t  enable_auto_fallback;   // 使能自动回退
    uint8_t  max_retry;               // 最大重试次数
    uint8_t  fallback_slot;           // 回退目标槽位
    uint8_t  reserved;
    uint32_t fallback_timeout;        // 回退超时(启动失败后等待时间)
} fallback_config_t;

/**
 * 启动失败记录
 */
typedef struct {
    uint8_t  failed_slot;         // 失败的槽位
    uint8_t  fail_count;          // 失败次数
    uint32_t last_fail_time;      // 最后失败时间
    uint32_t fail_reason;         // 失败原因
} boot_fail_record_t;

/**
 * 执行回退操作
 *
 * @param ctx 双镜像上下文
 * @param target_slot 目标槽位
 * @return 0 成功
 */
int boot_fallback(dual_image_context_t *ctx, uint8_t target_slot)
{
    int ret;
    image_slot_info_t target_info;

    boot_info("Attempting fallback to slot %d", target_slot);

    /* 检查目标槽位是否有效 */
    ret = boot_get_slot_info(target_slot, &target_info);
    if (ret < 0) {
        boot_error("Failed to get target slot info");
        return ret;
    }

    if (!target_info.valid) {
        boot_error("Target slot %d is not valid!", target_slot);
        return -BOOT_ERR_NO_VALID_IMAGE;
    }

    /* 清除当前活动槽位标记 */
    ret = boot_clear_active_flag(ctx->active_slot);
    if (ret < 0) {
        boot_error("Failed to clear active flag");
    }

    /* 设置新的活动槽位 */
    ctx->active_slot = target_slot;
    ret = boot_set_active_flag(target_slot);
    if (ret < 0) {
        boot_error("Failed to set active flag");
        return ret;
    }

    /* 记录回退信息 */
    boot_record_fallback(ctx, target_slot);

    boot_info("Fallback to slot %d completed", target_slot);

    return 0;
}

/**
 * 尝试启动并处理失败
 * 如果启动失败且满足回退条件，执行回退
 *
 * @param ctx 双镜像上下文
 * @param slot 要启动的槽位
 * @return 启动结果
 */
int boot_try_start(dual_image_context_t *ctx, uint8_t slot)
{
    int ret;
    uint32_t start_time;
    uint32_t elapsed;

    boot_info("Attempting to boot from slot %d", slot);

    start_time = get_tick_ms();

    /* 尝试启动 */
    ret = boot_jump_to_image(ctx, slot);

    /* 记录启动时间 */
    elapsed = get_tick_ms() - start_time;

    if (ret == 0) {
        /* 启动成功 */
        boot_info("Boot successful in %lu ms", elapsed);
        ctx->retry_count = 0;
        ctx->boot_attempts = 0;
        return 0;
    }

    /* 启动失败 */
    boot_error("Boot failed with error %d", ret);

    /* 记录失败信息 */
    boot_record_failure(ctx, slot, ret);

    /* 检查是否需要回退 */
    ctx->retry_count++;
    ctx->boot_attempts++;

    if (ctx->retry_count >= config.max_retry) {
        boot_error("Max retry count reached, attempting fallback");

        /* 查找可用的回退槽位 */
        uint8_t fallback_target = (slot == SLOT_A) ? SLOT_B : SLOT_A;
        image_slot_info_t fallback_info;

        if (boot_get_slot_info(fallback_target, &fallback_info) == 0 &&
            fallback_info.valid) {
            ret = boot_fallback(ctx, fallback_target);
            if (ret == 0) {
                /* 回退成功后重新启动 */
                ctx->retry_count = 0;
                return boot_try_start(ctx, fallback_target);
            }
        }

        /* 无法回退 */
        boot_error("Fallback failed, no valid image available");
        return -BOOT_ERR_BOOT_FAILED;
    }

    /* 等待后重试 */
    boot_info("Retrying in %d ms...", config.retry_delay_ms);
    delay_ms(config.retry_delay_ms);

    return ret;
}

/**
 * 验证并标记新升级的镜像 在固
 *件升级完成后调用，验证新固件可以正常启动
 *
 * @param slot 升级的槽位
 * @param mark_as_valid 是否标记为有效
 * @return 0 成功
 */
int boot_validate_update(uint8_t slot, bool mark_as_valid)
{
    image_slot_info_t slot_info;
    int ret;

    ret = boot_get_slot_info(slot, &slot_info);
    if (ret < 0) return ret;

    if (!slot_info.valid) {
        boot_error("Slot %d has no valid image", slot);
        return -BOOT_ERR_INVALID_IMAGE;
    }

    /* 尝试启动验证 */
    ret = boot_verify_image_at(slot);
    if (ret < 0) {
        boot_error("Image verification failed for slot %d", slot);

        if (mark_as_valid) {
            /* 标记为临时有效（可回退） */
            boot_mark_temp_valid(slot);
        }
        return ret;
    }

    /* 验证成功，标记为正式有效 */
    if (mark_as_valid) {
        boot_mark_permanent_valid(slot);
        boot_info("Image in slot %d validated and marked as permanent", slot);
    }

    return 0;
}
```

### 3.4 安全启动验证

安全启动（Secure Boot）通过数字签名验证固件的完整性和来源，确保只有受信任的固件才能在系统上运行。

```c
/**
 * 安全启动配置
 */
typedef struct {
    bool     enable_secure_boot;     // 使能安全启动
    bool     enable_verify_image;     // 验证镜像签名
    bool     enable_encryption;       // 使能解密
    uint32_t public_key_offset;       // 公钥存储偏移
    uint32_t public_key_size;        // 公钥大小
    uint32_t signature_offset;        // 签名存储偏移
    uint32_t signature_size;          // 签名大小
} secure_boot_config_t;

/**
 * 安全启动状态
 */
typedef struct {
    bool     verified;               // 验证通过
    bool     trusted;                // 来源可信
    uint32_t image_hash[8];          // 镜像哈希
    uint32_t verify_time;            // 验证时间
} secure_boot_status_t;

/**
 * 验证镜像签名
 *
 * @param dev Flash 设备
 * @param header 镜像头部
 * @return 0 验证成功
 */
int boot_verify_signature(nor_device_t *dev, const boot_header_t *header)
{
    int ret;
    uint8_t image_hash[SHA256_DIGEST_SIZE];
    uint8_t signature[256];
    uint32_t signature_offset;
    sha256_context_t sha;

    /* 检查是否需要签名验证 */
    if (!(header->boot_flags & BOOT_FLAG_SIGNED)) {
        boot_info("Image is not signed, skipping signature verification");
        return 0;
    }

    /* 计算镜像哈希 */
    sha256_init(&sha);

    uint8_t buffer[256];
    uint32_t data_offset = header->flash_offset + header->header_size;
    uint32_t data_size = header->image_size - header->header_size;

    while (data_size > 0) {
        uint32_t chunk = (data_size > sizeof(buffer)) ?
                         sizeof(buffer) : data_size;

        nor_read(dev, data_offset, buffer, chunk);
        sha256_update(&sha, buffer, chunk);

        data_offset += chunk;
        data_size -= chunk;
    }

    sha256_final(&sha, image_hash);

    /* 读取镜像签名 */
    signature_offset = header->flash_offset +
                       header->header_size +
                       header->image_size;
    nor_read(dev, signature_offset, signature, header->signature_size);

    /* 获取公钥 */
    uint8_t public_key[256];
    nor_read(dev, config.public_key_offset, public_key,
             config.public_key_size);

    /* 验证签名 */
    ret = rsa_verify(public_key, image_hash, signature);
    if (ret < 0) {
        boot_error("Signature verification failed!");
        return -BOOT_ERR_SIGNATURE_INVALID;
    }

    boot_info("Signature verification passed");
    return 0;
}
```

---

## 4. 启动流程详解

启动流程是系统从上电到应用程序运行的完整过程。一个健壮的启动流程需要完成硬件初始化、Flash 检测、镜像验证、内存配置和程序转跳等关键步骤。本节将详细分析每个阶段的工作内容和实现方法。

### 4.1 启动阶段划分

典型的嵌入式系统启动流程可以分为多个阶段，每个阶段都有特定的任务和目标。

第一阶段是硬件复位后的初始阶段（BL1），这个阶段主要完成最基本的硬件初始化，包括设置堆栈指针、配置系统时钟、初始化内存控制器等。由于此时系统环境还不稳定，此阶段的代码通常非常精简，直接在 Flash 中运行（如果是 XIP 模式）或从 Flash 复制到 SRAM 中运行。

第二阶段是更高级的引导程序阶段（BL2），这个阶段完成更复杂的硬件初始化，包括外设初始化、Flash 驱动初始化、内存测试等。然后进行启动镜像的检测和加载。

第三阶段是镜像加载阶段，负责读取启动镜像头部、验证镜像完整性、配置内存映射和复制必要的数据段到 RAM。

最后是应用程序启动阶段，将控制权转交给应用程序的入口点。

```c
/**
 * 启动阶段定义
 */
typedef enum {
    BOOT_STAGE_RESET = 0,        // 复位后初始阶段
    BOOT_STAGE_BL1,              // 第一级引导
    BOOT_STAGE_BL2,              // 第二级引导
    BOOT_STAGE_LOAD,             // 镜像加载
    BOOT_STAGE_VERIFY,           // 镜像验证
    BOOT_STAGE_JUMP,             // 转跳应用程序
    BOOT_STAGE_COMPLETE          // 启动完成
} boot_stage_t;

/**
 * 启动上下文
 */
typedef struct {
    boot_stage_t stage;          // 当前启动阶段
    boot_stage_t failed_stage;   // 失败阶段
    uint32_t error_code;         // 错误码
    uint32_t start_time;         // 启动开始时间
    uint32_t stage_time;         // 当前阶段耗时
    uint32_t total_time;         // 总启动时间

    /* 镜像信息 */
    uint32_t boot_addr;          // 启动地址
    uint32_t load_addr;         // 加载地址
    uint32_t entry_addr;        // 入口地址
    uint32_t image_size;        // 镜像大小

    /* Flash 信息 */
    nor_device_t *flash_dev;     // Flash 设备
    uint32_t flash_id;           // Flash ID

    /* 启动参数 */
    boot_header_t header;        // 启动头部
    dual_image_context_t dual;   // 双镜像上下文
} boot_context_t;

/**
 * 启动主流程
 *
 * @return 0 启动成功
 */
int boot_main(void)
{
    int ret;
    boot_context_t ctx;

    /* 初始化启动上下文 */
    memset(&ctx, 0, sizeof(ctx));
    ctx.start_time = get_tick_ms();

    boot_info("=== Bootloader Starting ===");

    /* 阶段 1: 硬件初始化 */
    ret = boot_stage1_hardware_init(&ctx);
    if (ret < 0) {
        goto boot_failed;
    }

    /* 阶段 2: Flash 初始化 */
    ret = boot_stage2_flash_init(&ctx);
    if (ret < 0) {
        goto boot_failed;
    }

    /* 阶段 3: 启动镜像检测 */
    ret = boot_stage3_image_detect(&ctx);
    if (ret < 0) {
        goto boot_failed;
    }

    /* 阶段 4: 镜像验证 */
    ret = boot_stage4_image_verify(&ctx);
    if (ret < 0) {
        /* 尝试回退 */
        ret = boot_try_fallback(&ctx);
        if (ret < 0) {
            goto boot_failed;
        }
    }

    /* 阶段 5: 内存配置 */
    ret = boot_stage5_memory_setup(&ctx);
    if (ret < 0) {
        goto boot_failed;
    }

    /* 阶段 6: 数据复制 */
    ret = boot_stage6_data_copy(&ctx);
    if (ret < 0) {
        goto boot_failed;
    }

    /* 阶段 7: 转跳应用程序 */
    ret = boot_stage7_jump_to_app(&ctx);
    if (ret < 0) {
        goto boot_failed;
    }

    /* 不应到达这里 */
    return -BOOT_ERR_UNREACHABLE;

boot_failed:
    ctx.error_code = ret;
    ctx.total_time = get_tick_ms() - ctx.start_time;
    boot_error("Boot failed at stage %d, error: %d, time: %lu ms",
               ctx.stage, ret, ctx.total_time);

    /* 进入恢复模式 */
    boot_enter_recovery_mode(&ctx);

    return ret;
}
```

### 4.2 硬件初始化阶段

硬件初始化是启动过程的第一步，需要在最短时间内完成最基本的外设配置，为后续的启动流程创造条件。

```c
/**
 * 阶段 1: 硬件初始化
 */
int boot_stage1_hardware_init(boot_context_t *ctx)
{
    ctx->stage = BOOT_STAGE_RESET;
    boot_info("Stage 1: Hardware initialization");

    /* 禁用中断 */
    __disable_irq();

    /* 配置系统时钟 */
    system_clock_config();

    /* 初始化系统定时器 */
    systick_config();

    /* 初始化调试串口 (可选) */
#ifdef DEBUG_UART_ENABLED
    debug_uart_init();
#endif

    /* 初始化看门狗 */
    iwdg_init();

    /* 简单内存检测 */
    if (!boot_check_memory()) {
        boot_error("Memory check failed!");
        return -BOOT_ERR_MEMORY;
    }

    /* 初始化必要的外设 GPIO */
    gpio_early_init();

    ctx->stage = BOOT_STAGE_BL1;
    boot_info("Stage 1 completed");

    return 0;
}

/**
 * 配置系统时钟
 */
void system_clock_config(void)
{
    /* 等待时钟稳定 */
    while (!rcc_clock_ready()) {
        /* 超时检测 */
    }

    /* 配置 PLL */
    rcc_pll_config(RCC_PLL_SOURCE_HSE, 8, 336, 2, 7);

    /* 切换到 PLL 时钟 */
    rcc_system_clock_switch(RCC_SYSCLK_PLL);

    /* 配置 APB 时钟 */
    rcc_apb1_clock_enable(RCC_APB1_DIV_4);
    rcc_apb2_clock_enable(RCC_APB2_DIV_2);
}

/**
 * 简单内存检测
 */
bool boot_check_memory(void)
{
    volatile uint32_t *test_addr = (volatile uint32_t *)0x20000000;
    uint32_t test_pattern = 0xDEADBEEF;
    uint32_t backup;

    /* 备份原值 */
    backup = *test_addr;

    /* 写入测试值 */
    *test_addr = test_pattern;

    /* 验证 */
    if (*test_addr != test_pattern) {
        return false;
    }

    /* 写反值测试 */
    *test_addr = ~test_pattern;
    if (*test_addr != ~test_pattern) {
        *test_addr = backup;
        return false;
    }

    /* 恢复原值 */
    *test_addr = backup;

    return true;
}
```

### 4.3 Flash 初始化与检测

Flash 初始化是启动过程中的关键步骤，需要正确配置 Flash 控制器并识别 Flash 芯片。

```c
/**
 * 阶段 2: Flash 初始化与检测
 */
int boot_stage2_flash_init(boot_context_t *ctx)
{
    int ret;
    uint8_t mfg_id;
    uint16_t dev_id;

    ctx->stage = BOOT_STAGE_BL2;
    boot_info("Stage 2: Flash initialization");

    /* 初始化 Flash 传输层 */
    ret = nor_transport_init();
    if (ret < 0) {
        boot_error("Flash transport init failed: %d", ret);
        return -BOOT_ERR_FLASH_INIT;
    }

    /* 初始化 Flash 设备 */
    ctx->flash_dev = nor_device_get_default();
    if (!ctx->flash_dev) {
        boot_error("Failed to get flash device");
        return -BOOT_ERR_NO_DEVICE;
    }

    /* 读取 Flash ID */
    ret = nor_read_jedec_id(ctx->flash_dev, &mfg_id, &dev_id);
    if (ret < 0) {
        boot_error("Failed to read flash ID: %d", ret);
        return -BOOT_ERR_FLASH_READ;
    }

    ctx->flash_id = ((uint32_t)mfg_id << 16) | dev_id;
    boot_info("Flash detected: MFG=0x%02X, DEV=0x%04X", mfg_id, dev_id);

    /* 验证 Flash ID 是否支持 */
    if (!boot_is_flash_supported(ctx->flash_id)) {
        boot_error("Flash not supported: 0x%08X", ctx->flash_id);
        return -BOOT_ERR_UNSUPPORTED_FLASH;
    }

    /* 初始化 Flash 驱动 */
    ret = nor_init(ctx->flash_dev);
    if (ret < 0) {
        boot_error("Flash init failed: %d", ret);
        return -BOOT_ERR_FLASH_INIT;
    }

    /* 解除写保护 */
    ret = nor_unlock_all(ctx->flash_dev);
    if (ret < 0) {
        boot_warn("Failed to unlock flash protection");
    }

    /* 测试 Flash 读写 */
    ret = boot_test_flash(ctx->flash_dev);
    if (ret < 0) {
        boot_error("Flash test failed: %d", ret);
        return -BOOT_ERR_FLASH_TEST;
    }

    boot_info("Stage 2 completed");

    return 0;
}

/**
 * 测试 Flash 基本功能
 */
int boot_test_flash(nor_device_t *dev)
{
    int ret;
    uint8_t test_data[16] = {0};
    uint8_t read_data[16];
    uint32_t test_addr = BOOT_TEST_ADDR;  // 专用测试地址

    /* 读取原始数据 */
    ret = nor_read(dev, test_addr, read_data, sizeof(read_data));
    if (ret != sizeof(read_data)) {
        return -BOOT_ERR_FLASH_READ;
    }

    /* 准备测试数据 */
    for (int i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t)(i + 0xAA);
    }

    /* 擦除扇区 */
    ret = nor_erase_sector(dev, test_addr);
    if (ret < 0) {
        return -BOOT_ERR_FLASH_ERASE;
    }

    /* 写入测试数据 */
    ret = nor_write(dev, test_addr, test_data, sizeof(test_data));
    if (ret != sizeof(test_data)) {
        return -BOOT_ERR_FLASH_WRITE;
    }

    /* 读取验证 */
    ret = nor_read(dev, test_addr, read_data, sizeof(read_data));
    if (ret != sizeof(read_data)) {
        return -BOOT_ERR_FLASH_READ;
    }

    /* 比较数据 */
    if (memcmp(test_data, read_data, sizeof(test_data)) != 0) {
        return -BOOT_ERR_FLASH_VERIFY;
    }

    /* 恢复原始数据 */
    nor_write(dev, test_addr, read_data, sizeof(read_data));

    return 0;
}
```

### 4.4 镜像加载与验证

镜像加载阶段负责读取启动镜像头部，验证镜像完整性，并根据配置准备启动环境。

```c
/**
 * 阶段 3 & 4: 镜像检测与验证
 */
int boot_stage3_image_detect(boot_context_t *ctx)
{
    int ret;
    nor_device_t *dev = ctx->flash_dev;

    ctx->stage = BOOT_STAGE_LOAD;
    boot_info("Stage 3: Image detection");

    /* 查找有效的启动分区 */
    int boot_slot = boot_select_boot_slot();
    if (boot_slot < 0) {
        boot_error("No valid boot image found");
        return -BOOT_ERR_NO_VALID_IMAGE;
    }

    /* 获取分区信息 */
    image_slot_info_t slot_info;
    ret = boot_get_slot_info(boot_slot, &slot_info);
    if (ret < 0) {
        return ret;
    }

    /* 计算头部偏移 */
    uint32_t header_offset = slot_info.offset;

    /* 读取启动头部 */
    ret = nor_read(dev, header_offset, &ctx->header, sizeof(boot_header_t));
    if (ret != sizeof(boot_header_t)) {
        boot_error("Failed to read boot header");
        return -BOOT_ERR_HEADER_READ;
    }

    /* 解析头部信息 */
    ctx->boot_addr = slot_info.offset;
    ctx->image_size = ctx->header.image_size;
    ctx->load_addr = ctx->header.load_addr;
    ctx->entry_addr = ctx->header.entry_point;

    boot_info("Boot image found: version=%d, size=%lu, load=0x%08X, entry=0x%08X",
              ctx->header.version, ctx->image_size,
              ctx->load_addr, ctx->entry_addr);

    /* 阶段 4: 镜像验证 */
    boot_info("Stage 4: Image verification");

    ret = boot_verify_image(&ctx->header, dev);
    if (ret < 0) {
        boot_error("Image verification failed: %d", ret);
        return ret;
    }

    /* 安全启动验证 (如果使能) */
    if (config.enable_secure_boot) {
        ret = boot_verify_signature(dev, &ctx->header);
        if (ret < 0) {
            boot_error("Signature verification failed: %d", ret);
            return -BOOT_ERR_SIGNATURE_INVALID;
        }
    }

    boot_info("Image verification passed");

    return 0;
}

/**
 * 阶段 5 & 6: 内存配置与数据复制
 */
int boot_stage5_memory_setup(boot_context_t *ctx)
{
    boot_info("Stage 5: Memory setup");

    /* 配置内存映射 */
    if (ctx->header.boot_flags & BOOT_FLAG_XIP) {
        /* XIP 模式: 配置内存映射 */
        boot_setup_xip_mapping(ctx);
    } else {
        /* 非 XIP 模式: 准备 RAM */
        boot_setup_ram_mapping(ctx);
    }

    boot_info("Stage 5 completed");

    /* 阶段 6: 数据复制 */
    boot_info("Stage 6: Data copy");

    if (!(ctx->header.boot_flags & BOOT_FLAG_XIP)) {
        /* 将代码段复制到 RAM */
        boot_copy_code_to_ram(ctx);
    }

    /* 复制数据段 */
    boot_copy_data_section(ctx);

    /* 初始化 BSS 段 */
    boot_init_bss_section(ctx);

    boot_info("Stage 6 completed");

    return 0;
}

/**
 * 配置 XIP 内存映射
 */
void boot_setup_xip_mapping(boot_context_t *ctx)
{
    nor_xip_config_t xip_config = {
        .mode = NOR_XIP_MODE_QUAD,
        .addr_bytes = 4,
        .dummy_cycles = 8,
        .read_cmd = 0xEB
    };

    nor_xip_context_t xip_ctx = {
        .device = ctx->flash_dev,
        .config = xip_config,
        .xip_base = config.xip_base_addr
    };

    int ret = nor_xip_enable(&xip_ctx);
    if (ret < 0) {
        boot_warn("XIP enable failed, falling back to RAM boot");
        ctx->header.boot_flags &= ~BOOT_FLAG_XIP;
    } else {
        boot_info("XIP mode enabled at 0x%08X", config.xip_base_addr);
    }
}
```

### 4.5 应用程序转跳

启动的最后一步是将控制权转交给应用程序。这需要正确设置堆栈指针和寄存器，然后执行函数调用转跳。

```c
/**
 * 阶段 7: 转跳到应用程序
 */
int boot_stage7_jump_to_app(boot_context_t *ctx)
{
    boot_info("Stage 7: Jumping to application");

    /* 记录启动总时间 */
    ctx->total_time = get_tick_ms() - ctx->start_time;
    boot_info("Boot completed in %lu ms", ctx->total_time);

    /* 打印启动信息 */
    boot_print_banner(ctx);

    /* 获取应用程序入口点 */
    uint32_t entry_point = ctx->entry_addr;
    uint32_t stack_pointer = *((volatile uint32_t *)ctx->load_addr);

    /* 验证堆栈指针有效性 */
    if (!boot_is_valid_stack_pointer(stack_pointer)) {
        boot_error("Invalid stack pointer: 0x%08X", stack_pointer);
        return -BOOT_ERR_INVALID_STACK;
    }

    /* 验证入口点有效性 */
    if (!boot_is_valid_code_address(entry_point)) {
        boot_error("Invalid entry point: 0x%08X", entry_point);
        return -BOOT_ERR_INVALID_ENTRY;
    }

    /* 禁能中断 */
    __disable_irq();

    /* 清除所有挂起的中断 */
    NVIC_ClearPendingIRQ();

    /* 刷新缓存 */
    SCB_InvalidateICache();
    SCB_InvalidateDCache();

    /* 禁用所有外设中断 (可选) */
    boot_disable_peripherals();

    /* 禁用看门狗 (在应用程序中重新使能) */
    iwdg_stop();

    /* 刷新所有数据缓存 */
    SCB->CCR |= SCB_CCR_DC_Msk;
    __DSB();
    __ISB();

    boot_info("Jumping to app at 0x%08X, SP=0x%08X", entry_point, stack_pointer);

    /* 执行转跳 */
    typedef void (*app_entry_t)(void);
    app_entry_t app_entry = (app_entry_t)entry_point;

    /* 设置堆栈指针 */
    __set_MSP(stack_pointer);

    /* 转跳到应用程序 */
    app_entry();

    /* 不应到达这里 */
    boot_error("App returned unexpectedly!");

    return -BOOT_ERR_UNREACHABLE;
}

/**
 * 验证堆栈指针
 */
bool boot_is_valid_stack_pointer(uint32_t sp)
{
    /* 检查是否在 RAM 范围内 */
    return (sp >= SRAM_BASE) && (sp < (SRAM_BASE + SRAM_SIZE));
}

/**
 * 验证代码地址
 */
bool boot_is_valid_code_address(uint32_t addr)
{
    /* 检查是否在 Flash 或 RAM 范围内 */
    return ((addr >= FLASH_BASE) && (addr < (FLASH_BASE + FLASH_SIZE))) ||
           ((addr >= SRAM_BASE) && (addr < (SRAM_BASE + SRAM_SIZE)));
}
```

---

## 5. 集成代码示例

本节提供完整的 Bootloader 集成代码示例，包括初始化代码、中断向量表配置、链接脚本以及应用程序的启动代码，帮助开发者快速构建可用的启动系统。

### 5.1 Bootloader 主程序

以下是完整的 Bootloader 主程序框架，展示了各个阶段的集成。

```c
/**
 * Nor Flash Bootloader 主程序
 *
 * 文件: bootloader/main.c
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bootloader.h"
#include "nor_flash.h"
#include "image_header.h"

/* 启动配置 */
static boot_config_t config = {
    .max_retry = 3,
    .retry_delay_ms = 1000,
    .enable_secure_boot = false,
    .enable_auto_fallback = true,
    .boot_timeout_ms = 5000,
    .xip_base_addr = 0x90000000,
    .app_a_offset = 0x10000,
    .app_a_size = 0xE0000,
    .app_b_offset = 0xF0000,
    .app_b_size = 0x60000,
};

/* 全局启动上下文 */
static boot_context_t g_boot_ctx;

/**
 * Bootloader 入口点
 */
void bootloader_entry(void)
{
    int ret;

    /* 初始化启动上下文 */
    memset(&g_boot_ctx, 0, sizeof(g_boot_ctx));
    g_boot_ctx.start_time = get_tick_ms();

    /* 初始化串口调试输出 */
    debug_uart_init();
    boot_print_header();

    /* 执行启动流程 */
    ret = boot_main();

    if (ret < 0) {
        boot_error("Boot failed: %d", ret);
        /* 进入恢复模式 */
        boot_recovery_mode();
    }

    /* 不应到达这里 */
    while (1) {
        /* 等待看门狗复位 */
    }
}

/**
 * 主启动流程
 */
int boot_main(void)
{
    int ret;

    boot_info("========================================");
    boot_info("  Nor Flash Bootloader v%d.%d.%d",
              BOOTLOADER_VERSION_MAJOR,
              BOOTLOADER_VERSION_MINOR,
              BOOTLOADER_VERSION_PATCH);
    boot_info("========================================");

    /* 阶段 1: 硬件初始化 */
    ret = boot_init_hardware();
    if (ret < 0) {
        return ret;
    }

    /* 阶段 2: Flash 初始化 */
    ret = boot_init_flash();
    if (ret < 0) {
        return ret;
    }

    /* 阶段 3: 读取启动配置 */
    ret = boot_load_config();
    if (ret < 0) {
        /* 使用默认配置 */
        boot_warn("Using default config");
    }

    /* 阶段 4: 镜像选择与验证 */
    ret = boot_select_and_verify_image();
    if (ret < 0) {
        /* 尝试回退 */
        ret = boot_handle_fallback();
        if (ret < 0) {
            return ret;
        }
    }

    /* 阶段 5: 设置启动环境 */
    ret = boot_setup_environment();
    if (ret < 0) {
        return ret;
    }

    /* 阶段 6: 转跳到应用程序 */
    ret = boot_launch_application();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

/**
 * 硬件初始化
 */
int boot_init_hardware(void)
{
    boot_info("\n[1/6] Hardware initialization...");

    /* 系统时钟配置 */
    system_clock_init();

    /* GPIO 初始化 */
    gpio_init();

    /* 系统定时器初始化 */
    systick_init();

    /* 随机数生成器初始化 */
    rng_init();

    /* 初始化看门狗 (短期超时，用于检测启动挂起) */
    iwdg_init_with_timeout(1000);  // 1秒超时

    boot_info("  Hardware init: OK");
    return 0;
}

/**
 * Flash 初始化
 */
int boot_init_flash(void)
{
    boot_info("\n[2/6] Flash initialization...");

    /* 初始化 SPI 接口 */
    int ret = spi_flash_init();
    if (ret < 0) {
        boot_error("  SPI init failed: %d", ret);
        return -BOOT_ERR_SPI_INIT;
    }

    /* 检测 Flash 芯片 */
    uint8_t mfg_id;
    uint16_t dev_id;
    ret = spi_flash_read_id(&mfg_id, &dev_id);
    if (ret < 0) {
        boot_error("  Flash ID read failed: %d", ret);
        return -BOOT_ERR_FLASH_ID;
    }

    boot_info("  Flash: MFG=0x%02X, DEV=0x%04X", mfg_id, dev_id);

    /* 获取 Flash 参数 */
    flash_params_t params;
    ret = spi_flash_get_params(&params);
    if (ret < 0) {
        boot_error("  Get params failed: %d", ret);
        return -BOOT_ERR_FLASH_PARAMS;
    }

    boot_info("  Flash size: %lu MB", params.total_size / (1024*1024));
    boot_info("  Page size: %u B", params.page_size);
    boot_info("  Sector size: %lu KB", params.sector_size / 1024);

    /* 解除 Flash 写保护 */
    spi_flash_unlock();

    /* 测试 Flash 读写 */
    ret = flash_test();
    if (ret < 0) {
        boot_error("  Flash test failed: %d", ret);
        return -BOOT_ERR_FLASH_TEST;
    }

    boot_info("  Flash init: OK");
    return 0;
}

/**
 * 加载启动配置
 */
int boot_load_config(void)
{
    boot_info("\n[3/6] Loading configuration...");

    /* 从配置分区读取配置 */
    uint8_t config_data[sizeof(boot_config_t)];
    int ret = nor_read(CONFIG_OFFSET, config_data, sizeof(config_data));

    if (ret < 0) {
        return ret;
    }

    /* 验证配置 */
    if (config_data[0] == 0xFF || config_data[0] == 0x00) {
        return -BOOT_ERR_INVALID_CONFIG;
    }

    /* 复制配置 */
    memcpy(&config, config_data, sizeof(config));

    boot_info("  Config loaded: OK");
    return 0;
}

/**
 * 选择并验证镜像
 */
int boot_select_and_verify_image(void)
{
    boot_info("\n[4/6] Selecting and verifying image...");

    /* 尝试从 A 分区启动 */
    int slot = SLOT_A;
    int ret = boot_verify_slot(slot);

    if (ret < 0) {
        boot_warn("  Slot A invalid, trying slot B");

        /* 尝试从 B 分区启动 */
        slot = SLOT_B;
        ret = boot_verify_slot(slot);

        if (ret < 0) {
            boot_error("  No valid boot image found!");
            return -BOOT_ERR_NO_VALID_IMAGE;
        }
    }

    /* 读取镜像头部 */
    uint32_t image_offset = (slot == SLOT_A) ? config.app_a_offset
                                              : config.app_b_offset;
    nor_read(image_offset, &g_boot_ctx.header, sizeof(boot_header_t));

    g_boot_ctx.active_slot = slot;

    boot_info("  Selected slot: %c", (slot == SLOT_A) ? 'A' : 'B');
    boot_info("  Image version: %d", g_boot_ctx.header.version);
    boot_info("  Image size: %lu", g_boot_ctx.header.image_size);
    boot_info("  Entry point: 0x%08X", g_boot_ctx.header.entry_point);

    /* 记录启动槽位 */
    boot_save_boot_info(slot, &g_boot_ctx.header);

    boot_info("  Image verify: OK");
    return 0;
}

/**
 * 验证指定槽位
 */
int boot_verify_slot(uint8_t slot)
{
    uint32_t offset = (slot == SLOT_A) ? config.app_a_offset
                                        : config.app_b_offset;

    /* 读取头部 */
    boot_header_t header;
    int ret = nor_read(offset, &header, sizeof(header));
    if (ret != sizeof(header)) {
        return -BOOT_ERR_HEADER_READ;
    }

    /* 检查魔数 */
    if (header.magic != BOOT_MAGIC) {
        return -BOOT_ERR_INVALID_MAGIC;
    }

    /* 验证 CRC */
    uint32_t stored_crc = header.crc32;
    uint32_t calc_crc = nor_crc32(offset, header.image_size);

    if (stored_crc != calc_crc) {
        boot_error("CRC mismatch: stored=0x%08X, calc=0x%08X",
                   stored_crc, calc_crc);
        return -BOOT_ERR_CRC_MISMATCH;
    }

    return 0;
}

/**
 * 处理回退
 */
int boot_handle_fallback(void)
{
    if (!config.enable_auto_fallback) {
        boot_error("  Auto fallback disabled");
        return -BOOT_ERR_FALLBACK_DISABLED;
    }

    boot_info("  Attempting fallback...");

    /* 切换到另一个槽位 */
    uint8_t fallback_slot = (g_boot_ctx.active_slot == SLOT_A) ? SLOT_B : SLOT_A;
    int ret = boot_verify_slot(fallback_slot);

    if (ret < 0) {
        boot_error("  Fallback slot also invalid!");
        return -BOOT_ERR_NO_FALLBACK;
    }

    /* 读取回退镜像头部 */
    uint32_t offset = (fallback_slot == SLOT_A) ? config.app_a_offset
                                                  : config.app_b_offset;
    nor_read(offset, &g_boot_ctx.header, sizeof(boot_header_t));

    g_boot_ctx.active_slot = fallback_slot;
    g_boot_ctx.fallback_used = true;

    boot_info("  Fallback to slot %c successful",
              (fallback_slot == SLOT_A) ? 'A' : 'B');

    return 0;
}

/**
 * 设置启动环境
 */
int boot_setup_environment(void)
{
    boot_info("\n[5/6] Setting up environment...");

    /* 检查是否使用 XIP */
    if (g_boot_ctx.header.boot_flags & BOOT_FLAG_XIP) {
        boot_info("  XIP mode enabled");

        /* 配置 XIP */
        ret = xip_setup(g_boot_ctx.active_slot);
        if (ret < 0) {
            boot_warn("  XIP setup failed, using RAM boot");
            g_boot_ctx.header.boot_flags &= ~BOOT_FLAG_XIP;
        }
    }

    if (!(g_boot_ctx.header.boot_flags & BOOT_FLAG_XIP)) {
        boot_info("  RAM boot mode");

        /* 复制代码到 RAM */
        ret = copy_to_ram(g_boot_ctx.active_slot);
        if (ret < 0) {
            boot_error("  Copy to RAM failed: %d", ret);
            return ret;
        }
    }

    /* 初始化应用程序堆栈 */
    uint32_t stack_ptr = *((volatile uint32_t *)g_boot_ctx.header.load_addr);

    /* 验证堆栈指针 */
    if (!IS_VALID_STACK_PTR(stack_ptr)) {
        boot_error("  Invalid stack pointer!");
        return -BOOT_ERR_INVALID_STACK;
    }

    boot_info("  Environment setup: OK");
    return 0;
}

/**
 * 启动应用程序
 */
int boot_launch_application(void)
{
    boot_info("\n[6/6] Launching application...");

    /* 打印启动信息 */
    uint32_t total_time = get_tick_ms() - g_boot_ctx.start_time;
    boot_info("  Boot time: %lu ms", total_time);
    boot_info("  Entry: 0x%08X", g_boot_ctx.header.entry_point);

    /* 禁能中断 */
    __disable_irq();
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICER[1] = 0xFFFFFFFF;

    /* 刷新缓存 */
    SCB_InvalidateICache();
    SCB_InvalidateDCache();

    /* 停止看门狗 */
    iwdg_stop();

    /* 获取入口点 */
    void (*app_entry)(void) = (void (*)(void))g_boot_ctx.header.entry_point;

    boot_info("\n========================================");
    boot_info("  Jumping to application...");
    boot_info("========================================\n");

    /* 延迟一下让日志输出完成 */
    delay_ms(10);

    /* 设置堆栈并转跳 */
    __set_MSP(*((volatile uint32_t *)g_boot_ctx.header.load_addr));
    app_entry();

    /* 不应到达 */
    return -BOOT_ERR_UNREACHABLE;
}
```

### 5.2 应用程序链接脚本

应用程序需要正确配置链接脚本以支持 XIP 或 RAM 启动模式。

```ld
/**
 * 应用程序链接脚本 - XIP 模式
 *
 * 文件: linker/app_xip.ld
 */

/* 目标 Flash 配置 */
FLASH_ORIGIN = 0x00010000;
FLASH_SIZE = 0x000E0000;

/* RAM 配置 */
RAM_ORIGIN = 0x20000000;
RAM_SIZE = 0x00020000;

/* 堆栈和堆配置 */
Stack_Size = 0x1000;         /* 4KB 栈 */
Heap_Size = 0x2000;          /* 8KB 堆 */

/* 入口点 */
ENTRY(Reset_Handler)

/* 内存定义 */
MEMORY
{
    FLASH (rx) : ORIGIN = FLASH_ORIGIN, LENGTH = FLASH_SIZE
    RAM (rwx)  : ORIGIN = RAM_ORIGIN, LENGTH = RAM_SIZE
}

/* 段定义 */
SECTIONS
{
    /* 中断向量表 */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* 代码段 */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* 只读数据 */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } >FLASH

    /* ARM 异常表 */
    .ARM.extab :
    {
        *(.ARM.extab* * .gnu.linkonce.armextab.*)
    } >FLASH

    .ARM :
    {
        *(.ARM.exidx*)
    } >FLASH

    /* 初始化数据段 (需要复制到 RAM) */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT>FLASH

    /* _sidata 是 Flash 中初始化数据的起始位置 */
    _sidata = LOADADDR(.data);

    /* 未初始化数据段 */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* 堆 */
    .heap :
    {
        . = ALIGN(8);
        __heap_start = .;
        . = . + Heap_Size;
        . = ALIGN(8);
        __heap_end = .;
    } >RAM

    /* 栈 */
    .stack :
    {
        . = ALIGN(8);
        . = . + Stack_Size;
        . = ALIGN(8);
        __StackTop = .;
    } >RAM

    /* XIP 只读数据 */
    .xip_ro :
    {
        *(.xip_rodata)
        *(.xip_rodata.*)
    } >FLASH
}

/* 导出符号供启动代码使用 */
_start = ADDR(.isr_vector) + 0x04;
PROVIDE(end = .);
PROVIDE(_end = .);
```

### 5.3 启动代码

应用程序的启动代码负责在 main() 函数之前完成必要的初始化工作。

```c
/**
 * 启动代码 - 初始化文件
 *
 * 文件: startup/startup.c
 */

#include <stdint.h>

/* 外部声明 */
extern uint32_t _sidata;    /* 初始化数据在 Flash 中的起始位置 */
extern uint32_t _sdata;     /* 数据段起始位置 (RAM) */
extern uint32_t _edata;     /* 数据段结束位置 (RAM) */
extern uint32_t _sbss;      /* BSS 段起始位置 */
extern uint32_t _ebss;      /* BSS 段结束位置 */
extern uint32_t _estack;    /* 堆栈顶 */
extern int main(void);      /* 主函数 */

/* 系统时钟配置 (由系统提供) */
extern void SystemInit(void);
extern void SystemClock_Config(void);

/* 看门狗处理 */
extern void IwdgInit(void);

/**
 * 重置处理函数
 */
void Reset_Handler(void)
{
    /* 初始化数据段 */
    uint32_t *src = &_sidata;
    uint32_t *dest = &_sdata;

    while (dest < &_edata) {
        *dest++ = *src++;
    }

    /* 初始化 BSS 段 */
    dest = &_sbss;
    while (dest < &_ebss) {
        *dest++ = 0;
    }

    /* 配置系统时钟 */
    SystemInit();

    /* 初始化系统时钟 (可选，由 SystemInit 自动配置) */
    // SystemClock_Config();

    /* 初始化看门狗 */
    IwdgInit();

    /* 调用主函数 */
    main();

    /* 如果 main 返回，则进入死循环 */
    while (1) {
        __WFI();
    }
}

/**
 * 空闲处理 - 内存堆分配失败时调用
 */
void _sbrk(void)
{
    while (1) {
        __WFI();
    }
}

/**
 * 终止函数
 */
void _abort(void)
{
    while (1) {
        __WFI();
    }
}

/**
 * 弱定义 NMI 处理
 */
__attribute__((weak))
void NMI_Handler(void)
{
    while (1) {
        __WFI();
    }
}

/**
 * 弱定义硬件错误处理
 */
__attribute__((weak))
void HardFault_Handler(void)
{
    while (1) {
        __WFI();
    }
}

/**
 * 弱定义内存管理错误处理
 */
__attribute__((weak))
void MemManage_Handler(void)
{
    while (1) {
        __WFI();
    }
}

/**
 * 弱定义总线错误处理
 */
__attribute__((weak))
void BusFault_Handler(void)
{
    while (1) {
        __WFI();
    }
}

/**
 * 弱定义使用错误处理
 */
__attribute__((weak))
void UsageFault_Handler(void)
{
    while (1) {
        __WFI();
    }
}
```

### 5.4 中断向量表配置

中断向量表的正确配置对于系统的正常启动和运行至关重要。

```c
/**
 * 中断向量表定义
 *
 * 文件: startup/system_vectors.c
 */

#include <stdint.h>

/* 向前声明 */
extern uint32_t _estack;
extern void Reset_Handler(void);

/* 弱定义的中断处理函数 */
void NMI_Handler(void) __attribute__((weak));
void HardFault_Handler(void) __attribute__((weak));
void MemManage_Handler(void) __attribute__((weak));
void BusFault_Handler(void) __attribute__((weak));
void UsageFault_Handler(void) __attribute__((weak));
void SVC_Handler(void) __attribute__((weak));
void DebugMon_Handler(void) __attribute__((weak));
void PendSV_Handler(void) __attribute__((weak));
void SysTick_Handler(void) __attribute__((weak));

/* 外部中断处理 (根据实际芯片配置) */
void WWDG_IRQHandler(void) __attribute__((weak));
void PVD_IRQHandler(void) __attribute__((weak));
void TAMPER_IRQHandler(void) __attribute__((weak));
void RTC_IRQHandler(void) __attribute__((weak));
void FLASH_IRQHandler(void) __attribute__((weak));
void RCC_IRQHandler(void) __attribute__((weak));
void EXTI0_IRQHandler(void) __attribute__((weak));
void EXTI1_IRQHandler(void) __attribute__((weak));
/* ... 更多中断 ... */

/* 默认中断处理 - 死循环 */
__attribute__((naked, noreturn))
void Default_Handler(void)
{
    while (1) {
        __WFI();
    }
}

/* 中断向量表 */
__attribute__((section(".isr_vector"), used))
void (* const g_pfnVectors[])(void) = {
    /* 堆栈顶 */
    (void (*)(void))((uint32_t)&_estack),

    /* 复位 */
    Reset_Handler,

    /* 异常处理 */
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,  /* 保留 */
    SVC_Handler,
    DebugMon_Handler,
    0,          /* 保留 */
    PendSV_Handler,
    SysTick_Handler,

    /* 外部中断 */
    WWDG_IRQHandler,
    PVD_IRQHandler,
    TAMPER_IRQHandler,
    RTC_IRQHandler,
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_IRQHandler,
    EXTI1_IRQHandler,
    /* ... */
};
```

### 5.5 应用程序主函数示例

应用程序的主函数示例，展示了如何正确初始化和使用 Flash 驱动。

```c
/**
 * 应用程序主函数示例
 *
 * 文件: application/main.c
 */

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "nor_flash.h"

/* 系统句柄 */
nor_device_t *g_flash_dev = NULL;

/**
 * 主函数
 */
int main(void)
{
    int ret;

    /* 硬件初始化 */
    hardware_init();

    /* 初始化串口 */
    console_init();
    console_printf("\n\r");
    console_printf("========================================\n\r");
    console_printf("  Application Started\n\r");
    console_printf("  Version: %s\n\r", APP_VERSION);
    console_printf("  Build: %s %s\n\r", __DATE__, __TIME__);
    console_printf("========================================\n\r");

    /* 初始化 Flash */
    ret = flash_init();
    if (ret < 0) {
        console_printf("ERROR: Flash init failed: %d\n\r", ret);
        error_handler();
    }

    /* 测试 Flash 读写 */
    ret = flash_test();
    if (ret < 0) {
        console_printf("ERROR: Flash test failed: %d\n\r", ret);
        error_handler();
    }

    console_printf("Flash: OK\n\r");

    /* 获取 Flash 信息 */
    nor_device_info_t info;
    nor_get_info(g_flash_dev, &info);
    console_printf("Flash: %s %s (%.2f MB)\n\r",
                   info.manufacturer_name,
                   info.device_name,
                   info.capacity / (1024.0 * 1024.0));

    /* 初始化看门狗 */
    iwdg_init();  // 正常超时

    /* 主循环 */
    while (1) {
        /* 处理任务 */
        process_tasks();

        /* 喂狗 */
        iwdg_feed();

        /* 进入低功耗模式 */
        enter_sleep_mode();
    }
}

/**
 * Flash 初始化
 */
int flash_init(void)
{
    int ret;

    /* 获取默认 Flash 设备 */
    g_flash_dev = nor_device_get_default();
    if (!g_flash_dev) {
        return -1;
    }

    /* 初始化设备 */
    ret = nor_init(g_flash_dev);
    if (ret < 0) {
        return ret;
    }

    /* 读取设备 ID */
    uint8_t mfg_id;
    uint16_t dev_id;
    ret = nor_read_jedec_id(g_flash_dev, &mfg_id, &dev_id);
    if (ret < 0) {
        return ret;
    }

    g_flash_dev->manufacturer_id = mfg_id;
    g_flash_dev->device_id = dev_id;

    return 0;
}

/**
 * Flash 测试
 */
int flash_test(void)
{
    uint8_t write_buf[256];
    uint8_t read_buf[256];
    uint32_t test_addr = 0x1000;  // 测试地址

    /* 准备测试数据 */
    for (int i = 0; i < sizeof(write_buf); i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    /* 擦除扇区 */
    int ret = nor_erase_sector(g_flash_dev, test_addr);
    if (ret < 0) {
        return ret;
    }

    /* 写入数据 */
    ret = nor_write(g_flash_dev, test_addr, write_buf, sizeof(write_buf));
    if (ret < 0) {
        return ret;
    }

    /* 读取数据 */
    ret = nor_read(g_flash_dev, test_addr, read_buf, sizeof(read_buf));
    if (ret < 0) {
        return ret;
    }

    /* 验证 */
    if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
        return -2;
    }

    return 0;
}

/**
 * 硬件初始化
 */
void hardware_init(void)
{
    /* 配置系统时钟 */
    SystemClock_Config();

    /* 配置 GPIO */
    MX_GPIO_Init();

    /* 配置 DMA */
    MX_DMA_Init();

    /* 配置 SPI */
    MX_SPI_Init();
}

/**
 * 错误处理
 */
void error_handler(void)
{
    while (1) {
        /* 闪烁 LED 指示错误 */
        LED_On();
        delay_ms(100);
        LED_Off();
        delay_ms(100);
    }
}
```

---

## 本章小结

本章详细介绍了 Nor Flash 启动加载器的集成技术，内容涵盖 XIP 实现原理、启动镜像布局设计、镜像校验与回退机制、启动流程以及集成代码示例。

**在 XIP 实现原理方面**，本章介绍了 XIP 技术的硬件和软件实现。XIP 允许代码直接在外部 Flash 中执行，无需复制到 RAM，能够显著缩短启动时间并节省 RAM 资源。实现 XIP 需要配置 Flash 的 XIP 模式、设置内存映射以及正确管理缓存。

**在启动镜像布局设计方面**，本章详细说明了启动镜像的结构设计，包括镜像头部、分区规划和链接脚本配置。合理的镜像布局是保证系统可靠启动和升级的基础，需要综合考虑 Flash 特性、启动流程和安全性要求。

**在镜像校验与回退机制方面**，本章介绍了 CRC32 和 SHA-256 等校验算法，以及双镜像策略和自动回退机制。这些机制能够有效检测固件损坏并在升级失败时自动恢复到正常工作状态，是保证系统可靠性的关键设计。

**在启动流程详解方面**，本章逐步分析了从硬件初始化到应用程序转跳的完整启动流程，包括硬件初始化、Flash 检测、镜像验证、内存配置和数据复制等关键阶段。每个阶段都有明确的职责和实现要点。

**在集成代码示例方面**，本章提供了完整的 Bootloader 主程序、应用程序链接脚本、启动代码和中断向量表配置示例。这些代码可以直接作为开发参考，帮助开发者快速构建可靠的启动系统。

通过本章的学习，开发者应该能够深入理解 Nor Flash 启动加载器的工作原理，掌握 XIP 技术和镜像管理的实现方法，并能够根据实际需求设计和实现符合项目要求的启动系统。

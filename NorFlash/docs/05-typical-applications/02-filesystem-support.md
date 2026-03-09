# 文件系统支持

Nor Flash 可以直接作为文件系统的存储介质，为嵌入式系统提供文件级别的数据管理能力。相比裸数据存储，文件系统提供了更灵活的数据组织、更方便的数据访问和更好的可维护性。本章将详细介绍在 Nor Flash 上集成文件系统的方案，包括 LittleFS、FATFS 的配置使用以及专用日志文件系统的设计。

---

## LittleFS 集成

LittleFS 是一个专为嵌入式系统设计的闪存文件系统，具有掉电安全、磨损平衡和低内存占用等特点，非常适合在 Nor Flash 上使用。

### LittleFS 特点分析

**优势特性**

1. **掉电安全**：LittleFS 采用日志结构设计，任何写操作都会先记录到日志中，系统突然断电不会导致文件系统损坏
2. **磨损平衡**：内置磨损均衡算法，将擦写操作分散到整个存储区域，延长 Flash 寿命
3. **低内存占用**：内存需求极低，适合资源受限的嵌入式系统
4. **目录支持**：支持多级目录结构
5. **小文件优化**：对小文件和频繁更新的场景进行了优化

**局限性**

1. **Flash 容量限制**：建议在 1MB 以下的 Flash 上使用效果最佳
2. **块大小依赖**：需要根据实际 Flash 的块大小进行配置
3. **删除性能**：大量删除操作后可能产生碎片

### LittleFS 在 Nor Flash 上的移植

LittleFS 的移植主要涉及以下几个关键步骤：

**第一步：定义块设备接口**

LittleFS 需要与底层存储设备交互，需要实现块设备接口函数：

```c
#include "lfs.h"

// Nor Flash 块设备配置
typedef struct {
    nor_flash_ops_t *ops;        // Nor Flash 操作接口
    uint32_t block_size;        // 块大小
    uint32_t block_count;       // 块数量
    uint32_t read_size;         // 读取单元大小
    uint32_t prog_size;         // 编程单元大小
    uint32_t cache_size;        // 缓存大小
    uint32_t lookahead_size;    // 预分配大小
} nor_flash_lfs_t;

// 块读取函数
static int block_read(const struct lfs_config *cfg, lfs_block_t block,
                      lfs_off_t off, void *buffer, lfs_size_t size)
{
    nor_flash_lfs_t *dev = (nor_flash_lfs_t *)cfg->context;
    uint32_t addr = block * cfg->block_size + off;

    return dev->ops->read(addr, buffer, size) ? 0 : LFS_ERR_IO;
}

// 块写入函数
static int block_prog(const struct lfs_config *cfg, lfs_block_t block,
                      lfs_off_t off, const void *buffer, lfs_size_t size)
{
    nor_flash_lfs_t *dev = (nor_flash_lfs_t *)cfg->context;
    uint32_t addr = block * cfg->block_size + off;

    return dev->ops->write(addr, buffer, size) ? 0 : LFS_ERR_IO;
}

// 块擦除函数
static int block_erase(const struct lfs_config *cfg, lfs_block_t block)
{
    nor_flash_lfs_t *dev = (nor_flash_lfs_t *)cfg->context;
    uint32_t addr = block * cfg->block_size;

    return dev->ops->erase(addr) ? 0 : LFS_ERR_IO;
}

// 同步函数
static int block_sync(const struct lfs_config *cfg)
{
    (void)cfg;
    // 对于 Nor Flash，通常不需要额外同步
    return 0;
}

// LittleFS 配置结构
static const struct lfs_config lfs_cfg = {
    .context = &nor_lfs_dev,
    .read = block_read,
    .prog = block_prog,
    .erase = block_erase,
    .sync = block_sync,

    // 根据 Nor Flash 参数配置
    .read_size = 4,              // 最小读取单元
    .prog_size = 4,              // 最小编程单元
    .block_size = 4096,          // 块大小 (4KB)
    .block_count = 128,          // 块数量 (512KB Flash)
    .cache_size = 256,           // 缓存大小
    .lookahead_size = 16,        // 预分配表大小

    // 磨损均衡配置
    .block_cycles = 500,         // 块擦写周期
};
```

**第二步：初始化 LittleFS**

```c
int lfs_nor_init(lfs_t *lfs)
{
    int err;

    // 初始化 Nor Flash 硬件
    nor_flash_init();

    // 挂载文件系统
    err = lfs_mount(lfs, &lfs_cfg);
    if (err != LFS_ERR_OK) {
        // 格式化文件系统
        err = lfs_format(lfs, &lfs_cfg);
        if (err != LFS_ERR_OK) {
            return err;
        }

        // 重新挂载
        err = lfs_mount(lfs, &lfs_cfg);
        if (err != LFS_ERR_OK) {
            return err;
        }
    }

    return 0;
}
```

**第三步：文件操作示例**

```c
#include "lfs.h"

// 写入配置文件
int save_config(lfs_t *lfs, const char *filename, const void *data, size_t size)
{
    int err;
    lfs_file_t file;

    // 打开文件（不存在则创建）
    err = lfs_file_open(lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err != LFS_ERR_OK) {
        return err;
    }

    // 写入数据
    err = lfs_file_write(lfs, &file, data, size);
    if (err < 0) {
        lfs_file_close(lfs, &file);
        return err;
    }

    // 关闭文件
    err = lfs_file_close(lfs, &file);
    if (err != LFS_ERR_OK) {
        return err;
    }

    // 同步文件系统
    return lfs_sync(lfs);
}

// 读取配置文件
int load_config(lfs_t *lfs, const char *filename, void *data, size_t size)
{
    int err;
    lfs_file_t file;

    // 以只读模式打开文件
    err = lfs_file_open(lfs, &file, filename, LFS_O_RDONLY);
    if (err != LFS_ERR_OK) {
        return err;
    }

    // 读取数据
    err = lfs_file_read(lfs, &file, data, size);
    if (err < 0) {
        lfs_file_close(lfs, &file);
        return err;
    }

    // 关闭文件
    return lfs_file_close(lfs, &file);
}

// 目录操作示例
int create_app_directory(lfs_t *lfs, const char *dir_name)
{
    int err;

    // 创建目录
    err = lfs_mkdir(lfs, dir_name);
    if (err != LFS_ERR_OK && err != LFS_ERR_EXIST) {
        return err;
    }

    // 在目录中创建文件
    char path[64];
    snprintf(path, sizeof(path), "%s/config.txt", dir_name);

    return save_config(lfs, path, "default", 7);
}
```

### LittleFS 配置优化

根据 Nor Flash 的特性进行适当配置，可以提升文件系统性能：

**读取性能优化**

```c
// 配置更大的读取缓存，提升连续读取性能
static const struct lfs_config lfs_cfg_optimized = {
    .read_size = 256,            // 匹配 DMA 缓冲区大小
    .prog_size = 256,
    .cache_size = 512,           // 更大的缓存
    // ...
};
```

**写入性能优化**

```c
// 配置写缓存，减少编程操作次数
static const struct lfs_config lfs_cfg_write_optimized = {
    .prog_size = 256,            // 256 字节编程单元
    .cache_size = 1024,          // 1KB 缓存
    .block_cycles = 1000,        // 减少擦除频率
    // ...
};
```

---

## FATFS 配置

FATFS 是通用的 FAT 文件系统实现，广泛应用于 SD 卡、USB 闪存等存储设备。在 Nor Flash 上使用 FATFS，可以与主流嵌入式操作系统兼容，方便数据交换。

### FATFS 特点分析

**优势特性**

1. **跨平台兼容**：FAT 文件系统被所有主流操作系统支持
2. **简单易用**：API 接口清晰，文档完善
3. **广泛支持**：各类 MCU 和 RTOS 都有成熟的驱动支持
4. **大文件支持**：FAT32 支持最大 32GB 分区和 4GB 单文件

**局限性**

1. **非掉电安全**：直接写操作可能被意外断电打断，导致文件系统损坏
2. **无磨损均衡**：需要上层应用实现磨损平衡
3. **碎片问题**：频繁的删除和写入会产生文件碎片

### FATFS 在 Nor Flash 上的配置

**第一步：配置 FATFS 选项**

```c
// ffconf.h - FATFS 配置文件

// 1. 功能配置
#define FF_FS_READONLY         0       // 0: 读写模式
#define FF_FS_MINIMIZE         0       // 0: 完整功能
#define FF_USE_STRFUNC         1       // 启用字符串函数
#define FF_USE_FIND            1       // 启用文件查找
#define FF_USE_MKFS            1       // 启用格式化功能
#define FF_USE_FASTSEEK        1       // 启用快速查找

// 2. 系统配置
#define FF_FS_NORTC            0       // 使用 RTC 时间戳
#define FF_FS_NORTC_SEC        0       // 默认时间
#define FF_FS_NORTC_MIN        0
#define FF_FS_NORTC_HOUR       0
#define FF_FS_NORTC_DAY        1
#define FF_FS_NORTC_MON        1
#define FF_FS_NORTC_YEAR       2020

// 3. 内存配置 - 根据 MCU 资源调整
#define FF_MAX_SS              4096    // 最大扇区大小
#define FF_USE_LFN             3       // 3: 堆栈长文件名
#define FF_MAX_LFN             255     // 最大长文件名长度
#define FF_LFN_UNICODE         0       // 0: ANSI/OEM 编码
#define FF_FS_RPATH            0       // 相对路径功能

// 4. 锁功能配置
#define FF_FS_LOCK             4       // 并行打开文件数
```

**第二步：实现磁盘接口**

```c
#include "ff.h"
#include "diskio.h"

// Nor Flash 磁盘控制块
typedef struct {
    nor_flash_ops_t *ops;
    DWORD           sector_size;    // 扇区大小
    DWORD           sector_count;   // 扇区数量
    uint8_t        write_buff[4096]; // 写缓冲
    BOOL            write_dirty;   // 缓冲是否脏
    DWORD           write_lba;     // 缓冲对应的扇区
} nor_disk_t;

// 初始化磁盘
DSTATUS disk_initialize(BYTE pdrv)
{
    (void)pdrv;

    // 初始化 Nor Flash
    nor_flash_init();

    return 0;  // 成功
}

// 获取磁盘状态
DSTATUS disk_status(BYTE pdrv)
{
    (void)pdrv;
    return 0;  // 磁盘就绪
}

// 读取扇区
DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    (void)pdrv;

    nor_flash_read(sector * SECTOR_SIZE, buff, count * SECTOR_SIZE);

    return RES_OK;
}

// 写入扇区
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    (void)pdrv;

    // 对于 Nor Flash，直接写入可能不是最高效的
    // 但 FATFS 的设计假设扇区可以独立写入
    for (UINT i = 0; i < count; i++) {
        uint32_t addr = (sector + i) * SECTOR_SIZE;

        // 写入前需要先擦除（如果未擦除）
        nor_flash_erase(addr, SECTOR_SIZE);
        nor_flash_write(addr, buff + i * SECTOR_SIZE, SECTOR_SIZE);
    }

    return RES_OK;
}

// IO 控制
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)pdrv;

    switch (cmd) {
        case GET_SECTOR_SIZE:
            *(DWORD *)buff = SECTOR_SIZE;
            break;

        case GET_SECTOR_COUNT:
            *(DWORD *)buff = FLASH_SIZE / SECTOR_SIZE;
            break;

        case GET_BLOCK_SIZE:
            // 返回最小擦除单元（单位：扇区）
            *(DWORD *)buff = FLASH_ERASE_BLOCK / SECTOR_SIZE;
            break;

        case CTRL_SYNC:
            // 确保所有挂起的写操作完成
            nor_flash_wait_busy();
            break;

        case CTRL_TRIM:
            // 标记扇区为未使用（用于磨损均衡）
            // Nor Flash 不直接支持 Trim，但可以记录
            break;
    }

    return RES_OK;
}
```

**第三步：使用 FATFS 进行文件操作**

```c
#include "ff.h"

FATFS fs;
FIL fil;
FRESULT fres;

// 初始化文件系统
int fatfs_init(void)
{
    // 尝试挂载文件系统
    fres = f_mount(&fs, "", 1);
    if (fres == FR_NO_FILESYSTEM) {
        // 创建文件系统
        BYTE work[FF_MAX_SS];
        fres = f_mkfs("", FM_FAT32, 0, work, sizeof(work));
        if (fres != FR_OK) {
            return -1;
        }

        // 重新挂载
        fres = f_mount(&fs, "", 1);
    }

    return (fres == FR_OK) ? 0 : -1;
}

// 写入传感器数据到文件
int save_sensor_log(const char *filename, const char *data)
{
    // 打开文件（追加模式）
    fres = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (fres != FR_OK) {
        return -1;
    }

    // 写入数据
    UINT bw;
    fres = f_write(&fil, data, strlen(data), &bw);
    if (fres != FR_OK) {
        f_close(&fil);
        return -1;
    }

    // 关闭文件
    fres = f_close(&fil);
    return (fres == FR_OK) ? 0 : -1;
}

// 读取配置文件
int read_config_file(const char *filename, char *buffer, size_t size)
{
    // 打开文件
    fres = f_open(&fil, filename, FA_READ);
    if (fres != FR_OK) {
        return -1;
    }

    // 读取数据
    UINT br;
    fres = f_read(&fil, buffer, size - 1, &br);
    if (fres != FR_OK) {
        f_close(&fil);
        return -1;
    }

    buffer[br] = '\0';

    // 关闭文件
    fres = f_close(&fil);
    return (fres == FR_OK) ? 0 : -1;
}

// 目录操作示例
int list_files(const char *path)
{
    DIR dir;
    FILINFO fno;

    // 打开目录
    fres = f_opendir(&dir, path);
    if (fres != FR_OK) {
        return -1;
    }

    // 遍历目录
    while (1) {
        fres = f_readdir(&dir, &fno);
        if (fres != FR_OK || fno.fname[0] == 0) {
            break;
        }

        if (fno.fattrib & AM_DIR) {
            printf("[DIR]  %s\r\n", fno.fname);
        } else {
            printf("[FILE] %s (%lu bytes)\r\n", fno.fname, fno.fsize);
        }
    }

    // 关闭目录
    f_closedir(&dir);
    return 0;
}
```

### FATFS 掉电保护机制

由于 FATFS 本身不具备掉电保护能力，需要在应用层实现保护机制：

```c
// 带有事务保护的写操作
int safe_write_file(const char *filename, const void *data, size_t size)
{
    FRESULT fres;
    FIL fil;
    BYTE work[FF_MAX_SS];

    // 1. 创建临时文件
    char temp_name[64];
    snprintf(temp_name, sizeof(temp_name), "%s.tmp", filename);

    fres = f_open(&fil, temp_name, FA_CREATE_ALWAYS | FA_WRITE);
    if (fres != FR_OK) {
        return -1;
    }

    // 2. 写入数据
    UINT bw;
    fres = f_write(&fil, data, size, &bw);
    f_close(&fil);

    if (fres != FR_OK || bw != size) {
        f_unlink(temp_name);
        return -1;
    }

    // 3. 确保数据写入存储
    disk_ioctl(0, CTRL_SYNC, NULL);

    // 4. 删除原文件（如存在）
    f_unlink(filename);

    // 5. 重命名临时文件为正式文件名
    fres = f_rename(temp_name, filename);
    if (fres != FR_OK) {
        // 重命名失败，尝试恢复
        return -1;
    }

    // 6. 再次同步
    disk_ioctl(0, CTRL_SYNC, NULL);

    return 0;
}
```

---

## 日志文件系统

对于需要频繁记录日志的嵌入式应用，设计专用的日志文件系统可以获得更好的性能和可靠性。本节介绍针对 Nor Flash 优化的日志文件系统设计。

### 日志文件系统设计目标

1. **顺序写入优化**：最大化顺序写入性能，减少随机访问
2. **掉电安全**：确保已写入的日志不会因断电丢失
3. **空间管理**：支持日志循环覆盖，自动清理旧日志
4. **快速查询**：支持按时间和日志级别查询

### 日志文件系统数据结构

```c
#include <stdint.h>
#include <stdbool.h>

// 日志头结构
typedef struct {
    uint32_t magic;              // 日志区标识 (0x4C4F474C 'LOG')
    uint16_t version;            // 版本号
    uint16_t block_size;         // 块大小
    uint32_t total_blocks;       // 总块数
    uint32_t head_block;         // 头指针（最新日志）
    uint32_t tail_block;         // 尾指针（最旧日志）
    uint32_t current_size;       // 当前日志大小
    uint32_t max_size;           // 最大日志大小
    uint32_t entry_count;        // 日志条目数
    uint16_t reserved;
    uint16_t checksum;           // 头部校验
} __attribute__((packed)) log_header_t;

// 日志条目头
typedef struct {
    uint32_t timestamp;          // 时间戳（秒）
    uint16_t level;             // 日志级别
    uint16_t length;            // 数据长度
    uint32_t sequence;          // 序列号
    uint32_t prev_offset;       // 前一条日志偏移
    uint32_t next_offset;       // 后一条日志偏移
} __attribute__((packed)) log_entry_header_t;

// 日志级别定义
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
} log_level_t;
```

### 日志文件系统实现

```c
// 日志文件系统上下文
typedef struct {
    nor_flash_ops_t *ops;        // Flash 操作接口
    log_header_t   header;       // 日志头
    flash_addr_t    base_addr;   // 日志区基地址
    uint8_t        *buffer;      // 写缓冲
    size_t         buffer_pos;   // 缓冲位置
    bool           initialized;  // 初始化标志
} logfs_t;

// 初始化日志文件系统
int logfs_init(logfs_t *logfs, nor_flash_ops_t *ops,
               flash_addr_t base_addr, size_t size)
{
    // 参数检查
    if (logfs == NULL || ops == NULL) {
        return -1;
    }

    // 初始化上下文
    logfs->ops = ops;
    logfs->base_addr = base_addr;
    logfs->buffer = malloc(LOG_BUFFER_SIZE);
    if (logfs->buffer == NULL) {
        return -2;
    }
    logfs->buffer_pos = 0;
    logfs->initialized = false;

    // 读取日志头
    ops->read(base_addr, &logfs->header, sizeof(log_header_t));

    // 检查是否需要格式化
    if (logfs->header.magic != 0x4C4F474C) {
        // 格式化日志区
        memset(&logfs->header, 0, sizeof(log_header_t));
        logfs->header.magic = 0x4C4F474C;
        logfs->header.version = 1;
        logfs->header.block_size = FLASH_BLOCK_SIZE;
        logfs->header.total_blocks = size / FLASH_BLOCK_SIZE;
        logfs->header.head_block = 0;
        logfs->header.tail_block = 0;
        logfs->header.current_size = 0;
        logfs->header.max_size = size;
        logfs->header.entry_count = 0;

        // 写入头部
        ops->write(base_addr, &logfs->header, sizeof(log_header_t));
    }

    logfs->initialized = true;
    return 0;
}

// 写入日志条目
int logfs_write(logfs_t *logfs, log_level_t level,
                const void *data, size_t length)
{
    if (!logfs->initialized) {
        return -1;
    }

    // 检查空间，必要时进行循环覆盖
    if (logfs->header.current_size + length + sizeof(log_entry_header_t)
            > logfs->header.max_size) {
        logfs_rotate(logfs);
    }

    // 准备日志条目
    uint8_t entry[256];  // 简化实现，使用固定缓冲区
    log_entry_header_t *hdr = (log_entry_header_t *)entry;

    hdr->timestamp = get_timestamp();
    hdr->level = level;
    hdr->length = length;
    hdr->sequence = logfs->header.entry_count + 1;
    hdr->prev_offset = logfs->header.head_block * FLASH_BLOCK_SIZE;

    // 复制数据
    memcpy(entry + sizeof(log_entry_header_t), data, length);

    // 计算校验
    uint16_t checksum = crc16_compute(entry, sizeof(log_entry_header_t) + length);
    hdr->checksum = checksum;

    // 写入 Flash（使用追加模式）
    flash_addr_t write_addr = logfs->base_addr + sizeof(log_header_t)
                              + (logfs->header.head_block * FLASH_BLOCK_SIZE);

    // 擦除块（如需要）
    ops->erase(write_addr, FLASH_BLOCK_SIZE);

    // 写入数据
    ops->write(write_addr, entry, sizeof(log_entry_header_t) + length);

    // 更新头部
    logfs->header.head_block = (logfs->header.head_block + 1) % logfs->header.total_blocks;
    logfs->header.current_size += sizeof(log_entry_header_t) + length;
    logfs->header.entry_count++;

    // 写入头部
    ops->write(logfs->base_addr, &logfs->header, sizeof(log_header_t));

    return 0;
}

// 日志轮转（循环覆盖旧日志）
void logfs_rotate(logfs_t *logfs)
{
    // 计算需要释放的空间
    size_t free_needed = logfs->header.max_size / 4;  // 保留 25% 空间
    size_t freed = 0;

    while (freed < free_needed && logfs->header.entry_count > 0) {
        // 读取最旧的日志条目
        flash_addr_t tail_addr = logfs->base_addr + sizeof(log_header_t)
                                  + (logfs->header.tail_block * FLASH_BLOCK_SIZE);

        log_entry_header_t hdr;
        logfs->ops->read(tail_addr, &hdr, sizeof(log_entry_header_t));

        freed += sizeof(log_entry_header_t) + hdr.length;
        logfs->header.tail_block = (logfs->header.tail_block + 1) % logfs->header.total_blocks;
        logfs->header.entry_count--;
    }

    logfs->header.current_size -= freed;
}

// 读取日志（倒序遍历）
int logfs_read_reverse(logfs_t *logfs, uint32_t count,
                       void (*callback)(const log_entry_header_t *, const void *))
{
    if (!logfs->initialized) {
        return -1;
    }

    uint32_t block = logfs->header.head_block;
    uint32_t read_count = 0;
    uint8_t entry[256];

    while (read_count < count && logfs->header.entry_count > 0) {
        // 回退一个块
        if (block == 0) {
            block = logfs->header.total_blocks - 1;
        } else {
            block--;
        }

        flash_addr_t addr = logfs->base_addr + sizeof(log_header_t)
                            + (block * FLASH_BLOCK_SIZE);

        log_entry_header_t *hdr = (log_entry_header_t *)entry;
        logfs->ops->read(addr, hdr, sizeof(log_entry_header_t));

        // 读取数据
        void *data = entry + sizeof(log_entry_header_t);
        logfs->ops->read(addr + sizeof(log_entry_header_t), data, hdr->length);

        // 调用回调
        callback(hdr, data);

        read_count++;
    }

    return read_count;
}
```

### 日志文件系统使用示例

```c
// 便捷的日志写入宏
#define LOGD(fmt, ...) logfs_write(&logfs, LOG_LEVEL_DEBUG, \
                                    str_log, sprintf(str_log, fmt, ##__VA_ARGS__))
#define LOGI(fmt, ...) logfs_write(&logfs, LOG_LEVEL_INFO,  \
                                    str_log, sprintf(str_log, fmt, ##__VA_ARGS__))
#define LOGW(fmt, ...) logfs_write(&logfs, LOG_LEVEL_WARN,  \
                                    str_log, sprintf(str_log, fmt, ##__VA_ARGS__))
#define LOGE(fmt, ...) logfs_write(&logfs, LOG_LEVEL_ERROR, \
                                    str_log, sprintf(str_log, fmt, ##__VA_ARGS__))

// 应用中使用日志
void sensor_task(void)
{
    char str_log[128];

    // 记录传感器数据
    LOGI("Sensor data: temp=%.2f, hum=%.2f, press=%.4f",
         sensor.temp, sensor.hum, sensor.press);

    // 记录告警
    if (sensor.temp > TEMP_THRESHOLD) {
        LOGW("Temperature threshold exceeded: %.2f", sensor.temp);
    }

    // 记录错误
    if (err != SENSOR_OK) {
        LOGE("Sensor error: code=%d", err);
    }
}

// 读取并显示最近日志
void dump_recent_logs(int count)
{
    printf("=== Recent %d Log Entries ===\r\n", count);

    logfs_read_reverse(&logfs, count, [](const log_entry_header_t *hdr, const void *data) {
        const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        const char *level = (hdr->level <= LOG_LEVEL_FATAL) ? level_str[hdr->level] : "UNK";

        printf("[%s] seq=%lu, len=%u: %s\r\n",
               level, hdr->sequence, hdr->length, (const char *)data);
    });
}
```

---

## 本章小结

本章详细介绍了在 Nor Flash 上集成文件系统的方案：

1. **LittleFS 集成**：介绍了 LittleFS 的掉电安全和磨损平衡特性，提供了完整的移植步骤和配置示例。LittleFS 特别适合对可靠性要求高、存储容量较小的应用场景。

2. **FATFS 配置**：介绍了 FATFS 的通用性和跨平台兼容性，提供了磁盘接口实现和掉电保护机制。适用于需要与 PC 或其他系统交换数据的应用。

3. **日志文件系统**：设计了专用的日志文件系统，优化了顺序写入性能和空间管理，支持日志循环覆盖和快速查询，适合需要频繁记录日志的嵌入式应用。

选择合适的文件系统需要综合考虑应用场景的可靠性要求、存储容量、性能需求和跨平台兼容性等因素。

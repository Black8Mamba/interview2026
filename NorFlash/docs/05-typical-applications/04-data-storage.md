# 数据存储方案

Nor Flash 在嵌入式系统中不仅用于代码存储，还广泛应用于各类数据的持久化存储，包括配置参数、传感器数据、系统日志等。本章将详细介绍基于 Nor Flash 的数据存储方案设计，涵盖配置参数存储、传感器数据存储、日志存储和混合存储策略。

---

## 配置参数存储

配置参数是系统运行过程中需要持久化保存的设置信息，如设备名称、通信参数、校准数据等。配置参数存储需要满足快速读写和可靠保存的要求。

### 参数存储格式设计

```c
#include <stdint.h>
#include <stdbool.h>

// 配置参数区头部
typedef struct {
    uint32_t magic;              // 标识 (0x434F4E46 'CONF')
    uint16_t version;            // 配置版本
    uint16_t count;              // 参数数量
    uint32_t data_size;          // 数据区大小
    uint32_t crc32;              // 数据区 CRC32
    uint32_t backup_crc32;       // 备份区 CRC32
    uint32_t timestamp;          // 最后修改时间
    uint8_t  reserved[8];
} __attribute__((packed)) config_header_t;

// 参数条目
typedef struct {
    uint16_t id;                 // 参数 ID
    uint16_t type;               // 参数类型
    uint16_t offset;             // 数据区偏移
    uint16_t length;             // 参数长度
    uint32_t default_value;      // 默认值
} config_item_t;

// 参数类型定义
typedef enum {
    PARAM_TYPE_INT8    = 0,
    PARAM_TYPE_UINT8   = 1,
    PARAM_TYPE_INT16   = 2,
    PARAM_TYPE_UINT16  = 3,
    PARAM_TYPE_INT32   = 4,
    PARAM_TYPE_UINT32  = 5,
    PARAM_TYPE_FLOAT   = 6,
    PARAM_TYPE_STRING  = 7,
    PARAM_TYPE_BLOB    = 8,
} param_type_t;

// 参数定义表
typedef struct {
    config_item_t item;
    const char   *name;
    void         *default_ptr;
} param_definition_t;

// 参数定义示例
static param_definition_t device_params[] = {
    {{0x0001, PARAM_TYPE_STRING, 0,  32, 0}, "device_name",   "ESP32-Dev"},
    {{0x0002, PARAM_TYPE_UINT32, 32, 4,  0}, "device_id",    0},
    {{0x0003, PARAM_TYPE_UINT8,  36, 1,  115200}, "baud_rate",  NULL},
    {{0x0004, PARAM_TYPE_UINT8,  37, 1,  8},   "data_bits",   NULL},
    {{0x0005, PARAM_TYPE_UINT8,  38, 1,  1},   "stop_bits",   NULL},
    {{0x0006, PARAM_TYPE_UINT8,  39, 1,  0},   "parity",      NULL},
    {{0x0010, PARAM_TYPE_UINT32, 40, 4,  0},   "wifi_ssid",   NULL},
    {{0x0011, PARAM_TYPE_STRING, 44, 64, 0},   "wifi_pass",   NULL},
    {{0x0020, PARAM_TYPE_UINT32, 108, 4,  8080}, "http_port",  NULL},
    {{0x0021, PARAM_TYPE_UINT32, 112, 4,  300},  "update_interval", NULL},
};
```

### 参数存储实现

```c
#include <string.h>

// 配置存储上下文
typedef struct {
    nor_flash_ops_t *ops;
    flash_addr_t     primary_addr;    // 主配置区
    flash_addr_t     backup_addr;     // 备份配置区
    config_header_t  header;
    uint8_t         *data_buffer;
    bool             dirty;          // 是否有未保存的修改
} config_store_t;

// 初始化配置存储
int config_init(config_store_t *cfg, nor_flash_ops_t *ops,
                flash_addr_t primary, flash_addr_t backup)
{
    cfg->ops = ops;
    cfg->primary_addr = primary;
    cfg->backup_addr = backup;
    cfg->dirty = false;

    // 分配数据缓冲区
    cfg->data_buffer = malloc(MAX_CONFIG_SIZE);
    if (cfg->data_buffer == NULL) {
        return -1;
    }

    // 读取配置头
    ops->read(primary, &cfg->header, sizeof(config_header_t));

    // 检查配置是否有效
    if (cfg->header.magic != 0x434F4E46) {
        // 未初始化，使用默认值
        config_reset_to_default(cfg);
        return config_save(cfg);
    }

    // 加载配置数据
    ops->read(primary + sizeof(config_header_t),
              cfg->data_buffer, cfg->header.data_size);

    // 验证 CRC
    uint32_t crc = crc32_compute(cfg->data_buffer, cfg->header.data_size);
    if (crc != cfg->header.crc32) {
        // 主区损坏，尝试从备份恢复
        if (config_load_from_backup(cfg) != 0) {
            config_reset_to_default(cfg);
        }
    }

    return 0;
}

// 保存配置（双备份机制）
int config_save(config_store_t *cfg)
{
    // 计算 CRC
    cfg->header.crc32 = crc32_compute(cfg->data_buffer, cfg->header.data_size);
    cfg->header.timestamp = get_timestamp();

    // 写入主配置区
    nor_flash_erase(cfg->primary_addr, CONFIG_SECTOR_SIZE);
    nor_flash_write(cfg->primary_addr, &cfg->header, sizeof(config_header_t));
    nor_flash_write(cfg->primary_addr + sizeof(config_header_t),
                    cfg->data_buffer, cfg->header.data_size);

    // 复制到备份区
    cfg->header.backup_crc32 = cfg->header.crc32;
    nor_flash_erase(cfg->backup_addr, CONFIG_SECTOR_SIZE);
    nor_flash_write(cfg->backup_addr, &cfg->header, sizeof(config_header_t));
    nor_flash_write(cfg->backup_addr + sizeof(config_header_t),
                    cfg->data_buffer, cfg->header.data_size);

    cfg->dirty = false;

    return 0;
}

// 从备份恢复配置
int config_load_from_backup(config_store_t *cfg)
{
    config_header_t backup_header;
    uint8_t *backup_data = malloc(MAX_CONFIG_SIZE);

    if (backup_data == NULL) {
        return -1;
    }

    // 读取备份区
    cfg->ops->read(cfg->backup_addr, &backup_header, sizeof(backup_header));
    cfg->ops->read(cfg->backup_addr + sizeof(backup_header),
                   backup_data, backup_header.data_size);

    // 验证备份 CRC
    uint32_t crc = crc32_compute(backup_data, backup_header.data_size);
    if (crc == backup_header.crc32) {
        // 备份有效，恢复配置
        memcpy(&cfg->header, &backup_header, sizeof(backup_header));
        memcpy(cfg->data_buffer, backup_data, backup_header.data_size);

        // 恢复到主区
        config_save(cfg);

        free(backup_data);
        return 0;
    }

    free(backup_data);
    return -1;
}

// 参数读取接口
int config_get(config_store_t *cfg, uint16_t param_id, void *value)
{
    // 查找参数定义
    config_item_t *item = find_param_item(param_id);
    if (item == NULL) {
        return -1;
    }

    // 检查边界
    if (item->offset + item->length > cfg->header.data_size) {
        return -2;
    }

    // 复制数据
    memcpy(value, cfg->data_buffer + item->offset, item->length);

    return 0;
}

// 参数写入接口
int config_set(config_store_t *cfg, uint16_t param_id, const void *value)
{
    // 查找参数定义
    config_item_t *item = find_param_item(param_id);
    if (item == NULL) {
        return -1;
    }

    // 检查边界
    if (item->offset + item->length > cfg->header.data_size) {
        return -2;
    }

    // 复制数据
    memcpy(cfg->data_buffer + item->offset, value, item->length);
    cfg->dirty = true;

    return 0;
}
```

### 参数存储使用示例

```c
// 定义设备配置
typedef struct {
    char     device_name[32];
    uint32_t device_id;
    uint32_t baud_rate;
    uint8_t  data_bits;
    uint8_t  stop_bits;
    uint8_t  parity;
} device_config_t;

// 读取设备配置
int get_device_config(device_config_t *config)
{
    config_store_t cfg;
    if (config_init(&cfg, &nor_ops, CONFIG_PRIMARY_ADDR, CONFIG_BACKUP_ADDR) != 0) {
        return -1;
    }

    config_get(&cfg, 0x0001, config->device_name);
    config_get(&cfg, 0x0002, &config->device_id);
    config_get(&cfg, 0x0003, &config->baud_rate);
    config_get(&cfg, 0x0004, &config->data_bits);
    config_get(&cfg, 0x0005, &config->stop_bits);
    config_get(&cfg, 0x0006, &config->parity);

    return 0;
}

// 保存设备配置
int save_device_config(const device_config_t *config)
{
    config_store_t cfg;
    if (config_init(&cfg, &nor_ops, CONFIG_PRIMARY_ADDR, CONFIG_BACKUP_ADDR) != 0) {
        return -1;
    }

    config_set(&cfg, 0x0001, config->device_name);
    config_set(&cfg, 0x0002, &config->device_id);
    config_set(&cfg, 0x0003, &config->baud_rate);
    config_set(&cfg, 0x0004, &config->data_bits);
    config_set(&cfg, 0x0005, &config->stop_bits);
    config_set(&cfg, 0x0006, &config->parity);

    return config_save(&cfg);
}
```

---

## 传感器数据存储

传感器数据存储需要支持高频写入和大容量存储，同时保证数据的完整性和访问效率。

### 环形缓冲区存储

对于连续采样的传感器数据，环形缓冲区是一种高效的存储方式：

```c
#include <stdint.h>
#include <stdbool.h>

// 传感器数据条目
typedef struct {
    uint32_t timestamp;          // 时间戳（毫秒）
    int16_t  value;              // 传感器值
    uint8_t  status;             // 数据状态
    uint8_t  reserved;
} __attribute__((packed)) sensor_data_entry_t;

// 传感器环形缓冲区
typedef struct {
    nor_flash_ops_t *ops;
    flash_addr_t     base_addr;     // 缓冲区基地址
    uint32_t         capacity;       // 缓冲区容量（条目数）
    uint32_t         entry_size;    // 单条目大小
    uint32_t         head;           // 头指针（写入位置）
    uint32_t         tail;          // 尾指针（最旧数据）
    uint32_t         count;         // 当前条目数
    uint32_t         wrap_count;    // 环绕计数
} sensor_ringbuf_t;

// 初始化传感器环形缓冲区
int sensor_ringbuf_init(sensor_ringbuf_t *rb, nor_flash_ops_t *ops,
                        flash_addr_t addr, uint32_t capacity)
{
    rb->ops = ops;
    rb->base_addr = addr;
    rb->capacity = capacity;
    rb->entry_size = sizeof(sensor_data_entry_t);
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->wrap_count = 0;

    // 读取头部信息（如已存在）
    ringbuf_header_t header;
    ops->read(addr, &header, sizeof(header));

    if (header.magic == 0x53454E53) {  // 'SENS'
        rb->head = header.head;
        rb->tail = header.tail;
        rb->count = header.count;
        rb->wrap_count = header.wrap_count;
    } else {
        // 初始化新缓冲区
        header.magic = 0x53454E53;
        header.head = 0;
        header.tail = 0;
        header.count = 0;
        header.wrap_count = 0;
        ops->write(addr, &header, sizeof(header));
    }

    return 0;
}

// 写入传感器数据
int sensor_write(sensor_ringbuf_t *rb, int16_t value, uint8_t status)
{
    sensor_data_entry_t entry = {
        .timestamp = get_tick_ms(),
        .value = value,
        .status = status,
        .reserved = 0
    };

    // 计算写入地址
    flash_addr_t write_addr = rb->base_addr + sizeof(ringbuf_header_t)
                               + (rb->head * rb->entry_size);

    // 写入数据（整块擦除由磨损均衡处理，此处简化）
    uint32_t block_addr = write_addr & ~(FLASH_BLOCK_SIZE - 1);
    if (write_addr - block_addr + rb->entry_size > FLASH_BLOCK_SIZE) {
        // 跨块，需要先擦除新块
        rb->ops->erase(block_addr + FLASH_BLOCK_SIZE, FLASH_BLOCK_SIZE);
    }

    // 检查是否需要擦除
    static uint32_t last_erased_block = 0;
    if (block_addr != last_erased_block) {
        rb->ops->erase(block_addr, FLASH_BLOCK_SIZE);
        last_erased_block = block_addr;
    }

    rb->ops->write(write_addr, &entry, rb->entry_size);

    // 更新指针
    rb->head = (rb->head + 1) % rb->capacity;

    if (rb->head == 0) {
        rb->wrap_count++;
    }

    if (rb->count < rb->capacity) {
        rb->count++;
    } else {
        // 缓冲区已满，移动尾指针
        rb->tail = (rb->tail + 1) % rb->capacity;
    }

    // 保存头部信息
    ringbuf_header_t header = {
        .magic = 0x53454E53,
        .head = rb->head,
        .tail = rb->tail,
        .count = rb->count,
        .wrap_count = rb->wrap_count
    };
    rb->ops->write(rb->base_addr, &header, sizeof(header));

    return 0;
}

// 读取指定范围的数据
int sensor_read_range(sensor_ringbuf_t *rb, uint32_t start_index,
                     sensor_data_entry_t *entries, uint32_t count)
{
    if (start_index >= rb->count) {
        return 0;
    }

    uint32_t read_count = (start_index + count > rb->count)
                          ? (rb->count - start_index) : count;

    uint32_t idx = (rb->tail + start_index) % rb->capacity;

    for (uint32_t i = 0; i < read_count; i++) {
        flash_addr_t read_addr = rb->base_addr + sizeof(ringbuf_header_t)
                                 + (idx * rb->entry_size);
        rb->ops->read(read_addr, &entries[i], rb->entry_size);

        idx = (idx + 1) % rb->capacity;
    }

    return read_count;
}

// 读取最近 N 条数据
int sensor_read_recent(sensor_ringbuf_t *rb, uint_count,
                       sensor_data_entry_t *32_t recententries)
{
    if (recent_count > rb->count) {
        recent_count = rb->count;
    }

    uint32_t start_index = rb->count - recent_count;
    return sensor_read_range(rb, start_index, entries, recent_count);
}
```

### 结构化数据存储

对于需要组织和查询的传感器数据，可以使用结构化存储方式：

```c
// 传感器数据记录结构
typedef struct {
    uint32_t record_id;          // 记录 ID
    uint32_t timestamp;           // 时间戳
    float    temperature;         // 温度
    float    humidity;           // 湿度
    float    pressure;           // 气压
    uint16_t voltage;            // 供电电压
    uint8_t  flags;              // 标志位
    uint8_t  reserved;
    uint32_t crc32;              // CRC32
} __attribute__((packed)) sensor_record_t;

// 数据记录管理器
typedef struct {
    nor_flash_ops_t *ops;
    flash_addr_t     base_addr;
    uint32_t         record_size;
    uint32_t         max_records;
    uint32_t         current_count;
    uint32_t         next_record_id;
} data_record_mgr_t;

// 添加数据记录
int data_record_add(data_record_mgr_t *mgr, const sensor_record_t *record)
{
    // 检查存储空间
    if (mgr->current_count >= mgr->max_records) {
        // 空间已满，删除最旧的记录
        data_record_delete_oldest(mgr);
    }

    // 分配新记录
    flash_addr_t write_addr = mgr->base_addr
                               + (mgr->current_count * mgr->record_size);

    // 擦除块（如需要）
    uint32_t block_size = FLASH_BLOCK_SIZE;
    flash_addr_t block_start = write_addr & ~(block_size - 1);
    flash_addr_t block_end = (write_addr + mgr->record_size) & ~(block_size - 1);

    if (block_start == block_end) {
        if ((write_addr % block_size) == 0) {
            mgr->ops->erase(write_addr, block_size);
        }
    } else {
        mgr->ops->erase(block_start, block_size);
        if ((write_addr + mgr->record_size) % block_size == 0) {
            mgr->ops->erase(block_end, block_size);
        }
    }

    // 写入记录
    sensor_record_t rec = *record;
    rec.record_id = mgr->next_record_id++;
    rec.crc32 = crc32_compute(&rec, offsetof(sensor_record_t, crc32));

    mgr->ops->write(write_addr, &rec, sizeof(rec));
    mgr->current_count++;

    return rec.record_id;
}

// 查询时间范围内的记录
int data_record_query(data_record_mgr_t *mgr, uint32_t start_time,
                      uint32_t end_time, sensor_record_t *records,
                      uint32_t max_count)
{
    uint32_t found = 0;
    sensor_record_t rec;

    for (uint32_t i = 0; i < mgr->current_count && found < max_count; i++) {
        flash_addr_t read_addr = mgr->base_addr + (i * mgr->record_size);
        mgr->ops->read(read_addr, &rec, sizeof(rec));

        // 验证 CRC
        uint32_t crc = crc32_compute(&rec, offsetof(sensor_record_t, crc32));
        if (crc != rec.crc32) {
            continue;  // 跳过损坏的记录
        }

        // 检查时间范围
        if (rec.timestamp >= start_time && rec.timestamp <= end_time) {
            records[found++] = rec;
        }
    }

    return found;
}
```

---

## 日志存储

系统日志对于故障诊断和运行分析至关重要。本节介绍针对 Nor Flash 优化的日志存储方案。

### 分级日志存储

```c
#include <stdio.h>
#include <string.h>

// 日志级别
typedef enum {
    LOG_DBG = 0,
    LOG_INF = 1,
    LOG_WRN = 2,
    LOG_ERR = 3,
    LOG_CRT = 4
} log_level_t;

// 日志条目
typedef struct {
    uint32_t timestamp;          // 时间戳（秒）
    uint8_t  level;             // 日志级别
    uint8_t  module;            // 模块 ID
    uint16_t length;            // 消息长度
    uint32_t sequence;          // 序列号
} __attribute__((packed)) log_entry_meta_t;

// 日志存储配置
typedef struct {
    flash_addr_t base_addr;      // 日志区基地址
    flash_addr_t index_addr;    // 索引区地址
    uint32_t    sector_size;     // 扇区大小
    uint32_t    total_size;      // 日志区总大小
    uint32_t    current_offset; // 当前写入偏移
    uint32_t    entry_count;    // 总条目数
    uint32_t    sequence;        // 序列号
    uint8_t     level_mask;      // 日志级别掩码
} log_storage_t;

// 写日志
int log_write(log_storage_t *log, log_level_t level, uint8_t module,
              const char *message)
{
    // 检查日志级别过滤
    if (!(log->level_mask & (1 << level))) {
        return 0;
    }

    size_t msg_len = strlen(message);
    size_t total_len = sizeof(log_entry_meta_t) + msg_len;

    // 检查空间
    if (log->current_offset + total_len > log->total_size) {
        // 空间不足，执行日志轮转
        log_rotate(log);
    }

    // 准备元数据
    log_entry_meta_t meta = {
        .timestamp = get_timestamp(),
        .level = level,
        .module = module,
        .length = msg_len,
        .sequence = log->sequence++
    };

    // 写入 Flash
    flash_addr_t write_addr = log->base_addr + log->current_offset;
    nor_flash_write(write_addr, &meta, sizeof(meta));
    nor_flash_write(write_addr + sizeof(meta), message, msg_len);

    // 更新偏移
    log->current_offset += total_len;

    // 更新索引（如使用索引）
    update_log_index(log, log->entry_count, write_addr, meta.timestamp);
    log->entry_count++;

    return 0;
}

// 日志轮转（保留最近的日志）
void log_rotate(log_storage_t *log)
{
    // 计算需要保留的起始位置
    uint32_t keep_size = log->total_size / 2;  // 保留一半空间
    uint32_t new_offset = log->current_offset - keep_size;

    // 读取旧数据到内存
    uint8_t *temp_buf = malloc(keep_size);
    if (temp_buf == NULL) {
        // 内存不足，直接清空
        nor_flash_erase(log->base_addr, log->total_size);
        log->current_offset = 0;
        log->entry_count = 0;
        return;
    }

    nor_flash_read(log->base_addr + new_offset, temp_buf, keep_size);

    // 擦除整个日志区
    nor_flash_erase(log->base_addr, log->total_size);

    // 写回保留的日志
    nor_flash_write(log->base_addr, temp_buf, keep_size);

    // 重建索引
    rebuild_log_index(log, temp_buf, keep_size);

    log->current_offset = keep_size;
    free(temp_buf);
}
```

---

## 混合存储策略

在复杂的嵌入式系统中，单一的存储方案往往难以满足所有需求。混合存储策略通过组合不同的存储方案，实现性能和可靠性的最优平衡。

### 混合存储架构

```
+---------------------------+
|     应用层 API            |
+---------------------------+
            |
+-----------+-----------+-----------+
|           |           |           |
+-----------+---+   +----+----+   +-+------+
| 配置存储服务 |   | 传感器数据  |   | 日志服务 |
|   (参数区)   |   |   服务      |   |         |
+------+------+   +----+----+----+   +-+------+
       |                |                |
+------+------+   +----+----+----+   +-+------+
|  小数据频繁    |   |  大数据顺序   |   |  循环覆盖  |
|  双备份策略   |   |  读写分离     |   |  空间管理  |
+------+------+   +----+----+----+   +-+------+
       |                |                |
+------+------+   +----+----+----+   +-+------+
| Nor Flash  |   | Nor Flash  |   | Nor Flash |
| 参数区域    |   | 数据区域    |   | 日志区域   |
+------+------+   +----+----+----+   +-+------+
```

### 存储区域划分

```c
// Nor Flash 存储区域划分
typedef struct {
    flash_addr_t bootloader_addr;     // 启动加载器
    uint32_t    bootloader_size;

    flash_addr_t app_a_addr;          // 应用程序 A
    uint32_t    app_a_size;

    flash_addr_t app_b_addr;          // 应用程序 B
    uint32_t    app_b_size;

    flash_addr_t config_addr;         // 配置参数区
    uint32_t    config_size;

    flash_addr_t sensor_data_addr;   // 传感器数据区
    uint32_t    sensor_data_size;

    flash_addr_t log_addr;            // 系统日志区
    uint32_t    log_size;

    flash_addr_t upgrade_addr;        // 升级包缓存区
    uint32_t    upgrade_size;

    flash_addr_t reserve_addr;        // 预留区域
    uint32_t    reserve_size;
} flash_partition_t;

// 典型 8MB Flash 分区划分
static const flash_partition_t flash_partitions = {
    .bootloader_addr = 0x00000000,
    .bootloader_size = 0x00010000,    // 64 KB

    .app_a_addr      = 0x00010000,
    .app_a_size     = 0x00100000,    // 1 MB

    .app_b_addr      = 0x00110000,
    .app_b_size     = 0x00100000,    // 1 MB

    .config_addr     = 0x00210000,
    .config_size    = 0x00020000,    // 128 KB

    .sensor_data_addr = 0x00230000,
    .sensor_data_size = 0x00200000,  // 2 MB

    .log_addr       = 0x00430000,
    .log_size      = 0x00100000,    // 1 MB

    .upgrade_addr   = 0x00530000,
    .upgrade_size  = 0x00200000,    // 2 MB

    .reserve_addr   = 0x00730000,
    .reserve_size  = 0x000D0000,    // 832 KB
};
```

### 存储管理器设计

```c
// 统一的存储管理器
typedef struct {
    nor_flash_ops_t *ops;
    flash_partition_t partitions;
    config_store_t   config;
    sensor_ringbuf_t sensor;
    log_storage_t    log;
    bool             initialized;
} storage_manager_t;

// 初始化存储管理器
int storage_init(storage_manager_t *mgr, nor_flash_ops_t *ops)
{
    mgr->ops = ops;

    // 初始化各存储模块
    int ret;

    // 配置存储
    ret = config_init(&mgr->config, ops,
                      flash_partitions.config_addr,
                      flash_partitions.config_addr + flash_partitions.config_size / 2);
    if (ret != 0) {
        return ret;
    }

    // 传感器数据存储
    ret = sensor_ringbuf_init(&mgr->sensor, ops,
                              flash_partitions.sensor_data_addr,
                              flash_partitions.sensor_data_size / sizeof(sensor_data_entry_t));
    if (ret != 0) {
        return ret;
    }

    // 日志存储
    ret = log_storage_init(&mgr->log, ops,
                           flash_partitions.log_addr,
                           flash_partitions.log_size);
    if (ret != 0) {
        return ret;
    }

    mgr->initialized = true;
    return 0;
}

// 统一的存储 API
int storage_write_config(uint16_t id, const void *value)
{
    storage_manager_t *mgr = get_storage_manager();
    if (!mgr->initialized) return -1;

    return config_set(&mgr->config, id, value);
}

int storage_read_config(uint16_t id, void *value)
{
    storage_manager_t *mgr = get_storage_manager();
    if (!mgr->initialized) return -1;

    return config_get(&mgr->config, id, value);
}

int storage_save_config(void)
{
    storage_manager_t *mgr = get_storage_manager();
    if (!mgr->initialized) return -1;

    return config_save(&mgr->config);
}

int storage_write_sensor_data(int16_t value, uint8_t status)
{
    storage_manager_t *mgr = get_storage_manager();
    if (!mgr->initialized) return -1;

    return sensor_write(&mgr->sensor, value, status);
}

int storage_read_recent_sensor_data(uint32_t count, sensor_data_entry_t *entries)
{
    storage_manager_t *mgr = get_storage_manager();
    if (!mgr->initialized) return -1;

    return sensor_read_recent(&mgr->sensor, count, entries);
}

int storage_write_log(log_level_t level, uint8_t module, const char *message)
{
    storage_manager_t *mgr = get_storage_manager();
    if (!mgr->initialized) return -1;

    return log_write(&mgr->log, level, module, message);
}
```

### 磨损均衡策略

Nor Flash 的擦写寿命有限，需要实现磨损均衡策略：

```c
// 动态磨损均衡
typedef struct {
    uint32_t erase_count[256];       // 各块的擦除计数
    uint32_t total_erases;           // 总擦除次数
    uint32_t avg_erases;             // 平均擦除次数
} wear_leveling_t;

// 初始化磨损均衡
void wear_leveling_init(wear_leveling_t *wl, nor_flash_ops_t *ops,
                        flash_addr_t base, uint32_t size)
{
    // 读取保存的擦除计数
    flash_addr_t wear_addr = base + size - FLASH_BLOCK_SIZE;
    ops->read(wear_addr, wl->erase_count, sizeof(wl->erase_count));

    // 计算总擦除次数和平均值
    wl->total_erases = 0;
    uint32_t block_count = size / FLASH_BLOCK_SIZE;
    for (uint32_t i = 0; i < block_count; i++) {
        wl->total_erases += wl->erase_count[i];
    }
    wl->avg_erases = wl->total_erases / block_count;
}

// 选择最佳写入块（擦除次数最少）
uint32_t wear_leveling_select_block(wear_leveling_t *wl, flash_addr_t base,
                                     uint32_t size)
{
    uint32_t block_size = FLASH_BLOCK_SIZE;
    uint32_t block_count = size / block_size;

    uint32_t min_count = wl->erase_count[0];
    uint32_t best_block = 0;

    // 查找擦除次数最少的块
    for (uint32_t i = 1; i < block_count; i++) {
        if (wl->erase_count[i] < min_count) {
            min_count = wl->erase_count[i];
            best_block = i;
        }
    }

    // 如果最少的块比平均值少 20% 以上，使用该块
    // 否则使用当前块（已有一定磨损）
    if (min_count < wl->avg_erases * 8 / 10) {
        return best_block;
    }

    // 否则使用轮询方式
    uint32_t current_block = (wl->total_erases) % block_count;
    return current_block;
}

// 记录块擦除
void wear_leveling_record(wear_leveling_t *wl, uint32_t block_index)
{
    wl->erase_count[block_index]++;
    wl->total_erases++;
    wl->avg_erases = wl->total_erases / (sizeof(wl->erase_count) / sizeof(uint32_t));
}

// 保存磨损均衡数据
void wear_leveling_save(wear_leveling_t *wl, nor_flash_ops_t *ops,
                        flash_addr_t addr)
{
    // 擦除并写入计数数据
    ops->erase(addr, FLASH_BLOCK_SIZE);
    ops->write(addr, wl->erase_count, sizeof(wl->erase_count));
}
```

---

## 本章小结

本章详细介绍了基于 Nor Flash 的数据存储方案设计：

1. **配置参数存储**：设计了结构化的参数存储格式，采用双备份机制确保配置可靠性，提供了完整的读写接口和默认值管理。

2. **传感器数据存储**：针对高频写入场景优化，实现了环形缓冲区存储和结构化数据存储，支持高效的数据追加和范围查询。

3. **日志存储**：设计了分级日志存储方案，支持日志级别过滤和自动轮转，确保长时间运行下的日志记录能力。

4. **混合存储策略**：通过统一的存储区域划分和存储管理器，实现了不同数据类型的最优存储方案组合，并设计了针对 Nor Flash 特性的磨损均衡策略。

合理的数据存储方案设计，可以充分发挥 Nor Flash 的性能优势，同时保证数据的可靠性和系统的长期稳定运行。

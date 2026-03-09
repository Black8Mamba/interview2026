# Nor Flash 掉电保护策略

掉电保护（Power-Fail Protection）是嵌入式系统中保障数据可靠性的关键机制。在 Flash 编程过程中突然断电可能导致数据损坏甚至文件系统破坏。本文档将介绍编程/擦除操作的原子性保证、事务日志机制以及数据恢复策略。

---

## 1. 编程/擦除原子性

### 1.1 原子性概念与挑战

Flash 编程和擦除操作通常需要较长时间（毫秒级），在此期间发生掉电会导致数据处于不一致状态：

- **部分编程**：数据只有部分字节写入成功
- **部分擦除**：块处于半擦除状态，无法正常使用
- **元数据损坏**：文件系统元数据（如 FAT 表、目录项）损坏

**掉电场景分析**：

```
正常写入:  数据A [完整] -> Flash [数据A]
                      |
掉电发生:  数据A [不完整] -> Flash [部分数据/垃圾数据]
```

### 1.2 编程原子性保证

#### 1.2.1 写缓冲机制

使用 RAM 缓冲区确保写入的完整性：

```c
// 原子写入上下文
typedef struct {
    uint32_t target_addr;       // 目标地址
    uint32_t data_size;         // 数据大小
    uint8_t *buffer;            // RAM 缓冲区
    uint8_t  in_progress;       // 操作进行中
    uint8_t  verified;          // 已验证标志
} atomic_write_ctx_t;

// 原子写入实现（两阶段写入）
int atomic_write_with_buffer(uint32_t addr, const uint8_t *data,
                             uint32_t len)
{
    static atomic_write_ctx_t ctx;
    int ret;

    // 阶段1: 写入到临时位置（如果是重要数据）
    uint32_t temp_addr = get_temp_sector_addr();

    // 确保临时区域已擦除
    ret = flash_erase(temp_addr);
    if (ret != 0) {
        return ret;
    }

    // 写入数据到临时区域
    ret = flash_write(temp_addr, data, len);
    if (ret != 0) {
        return ret;
    }

    // 验证临时区域数据
    ret = verify_data(temp_addr, data, len);
    if (ret != 0) {
        return ret;
    }

    // 阶段2: 复制到目标位置
    // 如果目标区域需要先擦除
    if (need_erase_before_write(addr, len)) {
        ret = flash_erase(get_sector_addr(addr));
        if (ret != 0) {
            return ret;
        }
    }

    ret = flash_write(addr, data, len);
    if (ret != 0) {
        // 写入失败，但临时区域仍有备份
        return ret;
    }

    // 阶段3: 验证目标位置
    ret = verify_data(addr, data, len);
    if (ret != 0) {
        // 尝试从临时区域恢复
        return recovery_from_backup(addr, temp_addr, len);
    }

    // 阶段4: 清除临时区域（可选）
    flash_erase(temp_addr);

    return 0;
}
```

#### 1.2.2 镜像写入

使用镜像块实现安全的块更新：

```c
// 镜像块写入
typedef struct {
    uint32_t main_block;        // 主数据块地址
    uint32_t mirror_block;      // 镜像块地址
    uint32_t state_block;       // 状态块地址
    uint8_t  active;            // 当前活动块
} mirror_write_ctx_t;

// 初始化镜像写入上下文
int mirror_init(mirror_write_ctx_t *ctx, uint32_t main_block,
                uint32_t mirror_block, uint32_t state_block)
{
    ctx->main_block = main_block;
    ctx->mirror_block = mirror_block;
    ctx->state_block = state_block;

    // 读取状态确定当前活动块
    uint8_t state;
    flash_read(state_block, &state, 1);

    ctx->active = (state & 0x01);  // 0: 主块, 1: 镜像块

    return 0;
}

// 镜像写入（原子操作）
int mirror_write(mirror_write_ctx_t *ctx, const uint8_t *data,
                uint32_t len)
{
    uint32_t target_block = ctx->active ? ctx->main_block : ctx->mirror_block;
    uint32_t backup_block = ctx->active ? ctx->mirror_block : ctx->main_block;
    int ret;

    // 1. 擦除备份块
    ret = flash_erase(backup_block);
    if (ret != 0) {
        return ret;
    }

    // 2. 写入新数据到备份块
    ret = flash_write(backup_block, data, len);
    if (ret != 0) {
        return ret;
    }

    // 3. 验证备份块数据
    uint8_t *verify_buf = malloc(len);
    if (!verify_buf) {
        return -ENOMEM;
    }

    flash_read(backup_block, verify_buf, len);
    if (memcmp(data, verify_buf, len) != 0) {
        free(verify_buf);
        return -EIO;
    }
    free(verify_buf);

    // 4. 切换状态（原子操作 - 只需写入一个字节）
    uint8_t new_state = ctx->active ? 0xFE : 0xFF;  // 切换活动块
    ret = flash_write(ctx->state_block, &new_state, 1);
    if (ret != 0) {
        // 状态写入失败，但数据已在备份块中
        return ret;
    }

    // 5. 更新活动块指针
    ctx->active = !ctx->active;

    // 6. 擦除旧主块（现在变为备份块）
    flash_erase(target_block);

    return 0;
}

// 读取当前数据
int mirror_read(mirror_write_ctx_t *ctx, uint8_t *data, uint32_t len)
{
    uint32_t active_block = ctx->active ? ctx->main_block : ctx->mirror_block;
    return flash_read(active_block, data, len);
}
```

### 1.3 擦除原子性保证

擦除操作的原子性比编程更难保证，因为擦除过程更复杂：

```c
// 擦除保护机制
typedef struct {
    uint32_t block_addr;         // 块地址
    uint8_t  erase_state;       // 擦除状态
    uint8_t  old_data[256];     // 备份旧数据（最大块大小）
    uint8_t  backup_valid;      // 备份有效标志
} erase_protection_ctx_t;

// 受保护的擦除操作
int protected_erase(uint32_t block_addr)
{
    erase_protection_ctx_t ctx;
    int ret;

    ctx.block_addr = block_addr;

    // 1. 备份块数据
    ctx.backup_valid = 0;
    uint32_t block_size = get_block_size(block_addr);

    if (block_size <= sizeof(ctx.old_data)) {
        ret = flash_read(block_addr, ctx.old_data, block_size);
        if (ret == 0) {
            ctx.backup_valid = 1;
        }
    }

    // 2. 执行擦除
    ret = flash_erase(block_addr);
    if (ret != 0) {
        // 擦除失败，尝试恢复数据
        if (ctx.backup_valid) {
            recovery_from_backup(block_addr, ctx.old_data, block_size);
        }
        return ret;
    }

    // 3. 验证擦除
    if (!verify_erased(block_addr)) {
        // 擦除不完全，恢复数据
        if (ctx.backup_valid) {
            recovery_from_backup(block_addr, ctx.old_data, block_size);
        }
        return -EIO;
    }

    return 0;
}

// 验证块已擦除
int verify_erased(uint32_t addr)
{
    uint32_t block_size = get_block_size(addr);
    uint8_t *buffer = malloc(block_size);

    if (!buffer) {
        return 0;
    }

    flash_read(addr, buffer, block_size);

    // 检查是否全为 0xFF
    for (uint32_t i = 0; i < block_size; i++) {
        if (buffer[i] != 0xFF) {
            free(buffer);
            return 0;
        }
    }

    free(buffer);
    return 1;
}
```

---

## 2. 事务日志机制

### 2.1 日志概述

事务日志（Transaction Log）通过记录操作历史来实现数据恢复：

- **预写日志（WAL - Write Ahead Log）**：在修改数据前先记录日志
- **回滚日志**：记录旧值，支持撤销操作
- **检查点（Checkpoint）**：定期保存系统状态

### 2.2 预写日志实现

```c
// 日志条目结构
typedef struct {
    uint32_t magic;              // 魔数，标识有效日志
    uint32_t sequence;           // 日志序列号
    uint32_t timestamp;          // 时间戳
    uint32_t target_addr;       // 目标地址
    uint32_t data_length;       // 数据长度
    uint8_t  operation;         // 操作类型
    uint8_t  checksum;          // 条目校验和
    uint8_t  data[];            // 数据（可变长度）
} __attribute__((packed)) log_entry_t;

// 日志操作类型
#define LOG_OP_WRITE   0x01     // 写入
#define LOG_OP_ERASE   0x02     // 擦除
#define LOG_OP_UPDATE  0x03     // 更新

// 日志缓冲区
typedef struct {
    uint32_t log_base_addr;      // 日志区域基址
    uint32_t log_size;          // 日志区域大小
    uint32_t current_seq;       // 当前序列号
    uint32_t entry_count;       // 当前条目数
    log_entry_t *buffer;        // 内存缓冲区
    uint32_t buffer_size;       // 缓冲区大小
    uint32_t buffer_used;       // 已使用大小
} transaction_log_t;

// 初始化事务日志
int transaction_log_init(transaction_log_t *log, uint32_t base,
                         uint32_t size)
{
    log->log_base_addr = base;
    log->log_size = size;
    log->current_seq = 0;
    log->entry_count = 0;
    log->buffer = NULL;
    log->buffer_used = 0;

    // 扫描现有日志
    log->current_seq = scan_existing_logs(base, &log->entry_count);

    return 0;
}

// 预写日志 - 在写入数据前记录日志
int log_write_pre(transaction_log_t *log, uint32_t addr,
                  const uint8_t *data, uint32_t len)
{
    // 1. 读取旧数据
    uint8_t *old_data = malloc(len);
    if (!old_data) {
        return -ENOMEM;
    }

    flash_read(addr, old_data, len);

    // 2. 创建日志条目
    uint32_t entry_size = sizeof(log_entry_t) + len;
    log_entry_t *entry = malloc(entry_size);

    if (!entry) {
        free(old_data);
        return -ENOMEM;
    }

    entry->magic = LOG_MAGIC;
    entry->sequence = ++log->current_seq;
    entry->timestamp = get_tick();
    entry->target_addr = addr;
    entry->data_length = len;
    entry->operation = LOG_OP_WRITE;
    memcpy(entry->data, old_data, len);  // 保存旧数据用于回滚
    entry->checksum = calculate_checksum(entry, entry_size);

    free(old_data);

    // 3. 将日志写入内存缓冲区
    if (log->buffer_used + entry_size > log->buffer_size) {
        // 缓冲区满，先刷新到 Flash
        flush_log_buffer(log);
    }

    memcpy(log->buffer + log->buffer_used, entry, entry_size);
    log->buffer_used += entry_size;
    log->entry_count++;

    free(entry);

    return 0;
}

// 刷新日志缓冲区到 Flash
int flush_log_buffer(transaction_log_t *log)
{
    if (log->buffer_used == 0) {
        return 0;
    }

    // 找到日志区域的空闲位置
    uint32_t write_addr = find_free_log_space(log);

    // 写入日志
    int ret = flash_write(write_addr, log->buffer, log->buffer_used);

    if (ret != 0) {
        // 写入失败，数据仍在缓冲区中
        return ret;
    }

    // 写入成功，清空缓冲区
    log->buffer_used = 0;

    return 0;
}

// 带事务的写入
int transactional_write(transaction_log_t *log, uint32_t addr,
                       const uint8_t *data, uint32_t len)
{
    int ret;

    // 1. 记录预写日志
    ret = log_write_pre(log, addr, data, len);
    if (ret != 0) {
        return ret;
    }

    // 2. 执行实际写入
    ret = flash_write(addr, data, len);
    if (ret != 0) {
        // 写入失败，日志可用于恢复
        return ret;
    }

    // 3. 刷新日志到 Flash（确保日志先写入）
    ret = flush_log_buffer(log);
    if (ret != 0) {
        // 日志写入失败，但数据已写入
        // 这种情况需要特别注意
        log_error("Log flush failed after write\n");
    }

    return 0;
}
```

### 2.3 日志恢复

```c
// 从日志恢复数据
int log_recover(transaction_log_t *log)
{
    uint32_t scan_addr = log->log_base_addr;
    log_entry_t entry;
    int recovered = 0;

    while (scan_addr < log->log_base_addr + log->log_size) {
        // 读取日志头
        int ret = flash_read(scan_addr, &entry, sizeof(log_entry_t));
        if (ret != 0 || entry.magic != LOG_MAGIC) {
            break;
        }

        // 读取完整条目
        uint32_t entry_size = sizeof(log_entry_t) + entry.data_length;
        log_entry_t *full_entry = malloc(entry_size);
        if (!full_entry) {
            break;
        }

        flash_read(scan_addr, full_entry, entry_size);

        // 验证校验和
        if (full_entry->checksum != calculate_checksum(full_name, entry_size)) {
            free(full_entry);
            break;
        }

        // 根据操作类型恢复
        if (full_entry->operation == LOG_OP_WRITE) {
            // 回滚到旧数据
            flash_write(full_entry->target_addr, full_entry->data,
                       full_entry->data_length);
            recovered++;
        }

        // 标记该日志条目为已处理
        mark_entry_processed(scan_addr);

        scan_addr += entry_size;
        free(full_entry);
    }

    log_info("Recovered %d entries from log\n", recovered);

    return recovered;
}

// 检查并清理已完成的事务
int log_cleanup(transaction_log_t *log)
{
    uint32_t scan_addr = log->log_base_addr;

    while (scan_addr < log->log_base_addr + log->log_size) {
        log_entry_t entry;
        int ret = flash_read(scan_addr, &entry, sizeof(log_entry_t));

        if (ret != 0 || entry.magic != LOG_MAGIC) {
            break;
        }

        // 检查数据是否已提交（通过检查目标地址的数据是否是最新的）
        // 如果是，可以删除这个日志条目

        scan_addr += sizeof(log_entry_t) + entry.data_length;
    }

    return 0;
}
```

---

## 3. 恢复策略

### 3.1 分级恢复机制

```c
// 恢复策略级别
typedef enum {
    RECOVERY_LEVEL_NONE,      // 无需恢复
    RECOVERY_LEVEL_LOG,       // 使用日志恢复
    RECOVERY_LEVEL_BACKUP,     // 使用备份恢复
    RECOVERY_LEVEL_DEFAULT    // 恢复到默认状态
} recovery_level_t;

// 恢复上下文
typedef struct {
    recovery_level_t level;   // 恢复级别
    uint32_t checksum;        // 数据校验和
    uint32_t timestamp;       // 检查点时间戳
} recovery_context_t;

// 启动时检查和恢复
int recovery_check_and_restore(void)
{
    recovery_context_t ctx;
    int ret;

    // 1. 检查文件系统完整性
    ret = fs_check_integrity();
    if (ret == 0) {
        return 0;  // 无需恢复
    }

    log_warning("File system integrity check failed, starting recovery\n");

    // 2. 尝试日志恢复
    ret = log_recover(&g_transaction_log);
    if (ret > 0) {
        log_info("Log recovery: restored %d entries\n", ret);
        ret = fs_check_integrity();
        if (ret == 0) {
            return 0;  // 恢复成功
        }
    }

    // 3. 尝试备份恢复
    ret = backup_restore();
    if (ret == 0) {
        log_info("Backup recovery successful\n");
        return 0;
    }

    // 4. 恢复到默认状态
    log_error("All recovery methods failed, restoring factory defaults\n");
    return factory_reset();
}
```

### 3.2 检查点机制

```c
// 检查点结构
typedef struct {
    uint32_t magic;            // 魔数
    uint32_t version;         // 版本号
    uint32_t sequence;        // 检查点序列号
    uint32_t timestamp;       // 时间戳
    uint32_t fs_state_crc;    // 文件系统状态校验和
    uint32_t data_crc;        // 应用数据校验和
    uint8_t  valid;           // 有效性标志
} checkpoint_t;

#define CHECKPOINT_ADDR   0x1000  // 检查点存储地址
#define CHECKPOINT_COUNT  3       // 保存多个检查点

// 保存检查点
int save_checkpoint(uint32_t fs_crc, uint32_t data_crc)
{
    static uint32_t current_seq = 0;
    checkpoint_t cp;

    cp.magic = CHECKPOINT_MAGIC;
    cp.version = FW_VERSION;
    cp.sequence = ++current_seq;
    cp.timestamp = get_tick();
    cp.fs_state_crc = fs_crc;
    cp.data_crc = data_crc;
    cp.valid = 1;

    // 轮流写入检查点（使用环形缓冲区）
    uint32_t cp_addr = CHECKPOINT_ADDR +
                       (current_seq % CHECKPOINT_COUNT) * sizeof(checkpoint_t);

    return flash_write(cp_addr, &cp, sizeof(cp));
}

// 加载检查点
int load_checkpoint(checkpoint_t *cp)
{
    checkpoint_t temp;
    uint32_t latest_seq = 0;
    uint32_t latest_addr = 0;

    // 查找最新的有效检查点
    for (uint32_t i = 0; i < CHECKPOINT_COUNT; i++) {
        uint32_t addr = CHECKPOINT_ADDR + i * sizeof(checkpoint_t);
        flash_read(addr, &temp, sizeof(temp));

        if (temp.magic == CHECKPOINT_MAGIC && temp.valid) {
            if (temp.sequence > latest_seq) {
                latest_seq = temp.sequence;
                latest_addr = addr;
            }
        }
    }

    if (latest_addr == 0) {
        return -ENOENT;  // 无有效检查点
    }

    flash_read(latest_addr, cp, sizeof(cp));

    return 0;
}

// 验证检查点
int verify_checkpoint(checkpoint_t *cp)
{
    if (cp->magic != CHECKPOINT_MAGIC || !cp->valid) {
        return 0;
    }

    // 验证版本兼容性
    if (cp->version != FW_VERSION) {
        log_warning("Checkpoint version mismatch\n");
        return 0;
    }

    return 1;
}
```

### 3.3 完整恢复流程

```c
// 完整系统恢复流程
int full_system_recovery(void)
{
    int ret;
    checkpoint_t cp;

    log_info("Starting full system recovery\n");

    // 步骤1: 读取检查点
    ret = load_checkpoint(&cp);
    if (ret != 0 || !verify_checkpoint(&cp)) {
        log_warning("No valid checkpoint, using factory defaults\n");
        return factory_reset();
    }

    // 步骤2: 验证当前数据状态
    uint32_t current_fs_crc = calculate_fs_crc();
    uint32_t current_data_crc = calculate_data_crc();

    if (current_fs_crc == cp.fs_state_crc &&
        current_data_crc == cp.data_crc) {
        log_info("Data is consistent, no recovery needed\n");
        return 0;
    }

    // 步骤3: 尝试日志恢复
    log_info("Attempting log-based recovery\n");
    ret = log_recover(&g_transaction_log);

    if (ret >= 0) {
        // 验证恢复后的状态
        current_fs_crc = calculate_fs_crc();
        current_data_crc = calculate_data_crc();

        if (current_fs_crc == cp.fs_state_crc &&
            current_data_crc == cp.data_crc) {
            log_info("Log recovery successful\n");
            return 0;
        }
    }

    // 步骤4: 从备份恢复
    log_info("Attempting backup recovery\n");
    ret = backup_restore();

    if (ret == 0) {
        log_info("Backup recovery successful\n");
        return 0;
    }

    // 步骤5: 最后的恢复尝试 - 尝试恢复部分数据
    log_info("Attempting partial data recovery\n");
    ret = partial_data_recovery();

    if (ret == 0) {
        log_info("Partial recovery successful\n");
        return 0;
    }

    // 步骤6: 工厂复位
    log_error("All recovery methods exhausted\n");
    return factory_reset();
}
```

### 3.4 掉电检测与应对

```c
// 掉电检测中断处理
volatile uint8_t g_power_fail_flag = 0;

void power_fail_interrupt(void)
{
    g_power_fail_flag = 1;

    // 禁用所有中断
    __disable_irq();

    // 关键数据紧急保存
    emergency_save_critical_data();
}

// 紧急保存关键数据
void emergency_save_critical_data(void)
{
    // 1. 刷新日志缓冲区
    if (g_transaction_log.buffer_used > 0) {
        // 尝试直接写入（日志优先）
        uint32_t addr = find_free_log_space(&g_transaction_log);
        flash_write(addr, g_transaction_log.buffer,
                   g_transaction_log.buffer_used);
    }

    // 2. 保存检查点
    save_checkpoint(calculate_fs_crc(), calculate_data_crc());

    // 3. 关闭文件系统
    fs_shutdown();

    // 4. 等待一小段时间确保数据写入
    for (volatile int i = 0; i < 10000; i++);
}
```

---

## 4. 可靠性设计最佳实践

### 4.1 设计原则

1. **数据冗余**：关键数据多份存储
2. **日志优先**：重要操作先写日志再执行
3. **原子操作**：使用镜像块或两阶段写入
4. **定期检查点**：定期保存系统状态
5. **掉电检测**：及时响应掉电事件

### 4.2 存储布局建议

```
┌─────────────────────────────────────────────────────────────┐
│                    Flash 存储布局建议                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  0x0000 - 0x0FFF │ 引导加载器 (4KB)                         │
│                  │                                          │
│  0x1000 - 0x1FFF │ 检查点区域 (3 x 4KB, 环形)              │
│                  │                                          │
│  0x2000 - 0x3FFF │ 事务日志区域 (8KB)                       │
│                  │                                          │
│  0x4000 - 0x4FFF │ 镜像块 A (4KB)                          │
│                  │                                          │
│  0x5000 - 0x5FFF │ 镜像块 B (4KB)                          │
│                  │                                          │
│  0x6000 - ...    │ 主数据区域                               │
│                  │                                          │
└─────────────────────────────────────────────────────────────┘
```

### 4.3 性能与可靠性的平衡

| 策略 | 可靠性 | 性能影响 | 存储开销 |
|------|--------|----------|----------|
| 每次写入刷新日志 | 高 | 高 | 中 |
| 批量日志刷新 | 中 | 低 | 中 |
| 镜像块写入 | 高 | 中 | 高 |
| 检查点保存 | 中 | 低 | 低 |
| 组合策略 | 很高 | 中 | 高 |

---

## 5. 小结

掉电保护是嵌入式 Flash 应用中的核心挑战。通过编程/擦除原子性保证、事务日志机制和分级恢复策略的组合，可以有效保障数据的可靠性和系统的可恢复性。在实际设计中，需要根据应用场景选择合适的策略，并进行充分的测试验证。

---

**相关文档**

- [Nor Flash 驱动框架设计](../02-driver-development/01-driver-framework.md)
- [Nor Flash 磨损均衡机制](./01-wear-leveling.md)
- [Nor Flash 位操作与位编程](./03-bit-operation.md)
- [Nor Flash 安全特性](./05-security-features.md)

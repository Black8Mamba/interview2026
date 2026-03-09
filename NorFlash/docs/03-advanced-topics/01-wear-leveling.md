# Nor Flash 磨损均衡机制

磨损均衡（Wear Leveling）是延长 Nor Flash 使用寿命的核心技术。由于 Flash 存储单元的擦写次数有限（通常为 10 万至 100 万次），如果不加控制地频繁擦写某些特定块，将导致这些块提前失效，进而影响整个存储设备的可靠性。本文档将详细介绍块寿命与擦写次数限制、静态与动态磨损均衡算法，以及具体的实现方案。

---

## 1. 块寿命与擦写次数限制

### 1.1 Flash 单元磨损机制

Nor Flash 采用浮栅晶体管存储电荷来表示数据。每个存储单元在经历多次擦除（Erase）和编程（Program）操作后，浮栅中的氧化层会逐渐老化，导致电荷保持能力下降，最终无法可靠地现象存储数据。这种称为**单元磨损（Cell Wear）**。

影响 Flash 单元寿命的因素包括：

- **擦写循环次数（P/E Cycle）**：每次完整的擦除+编程过程算作一次 P/E 循环
- **数据保持时间（Data Retention）**：随着单元老化，数据保持时间会缩短
- **温度影响**：高温会加速氧化层老化，高温工作会显著降低寿命
- **编程策略**：使用过高的编程电压或过长的编程时间会加速磨损

### 1.2 擦写次数规格

不同类型的 Nor Flash 芯片具有不同的擦写次数限制：

| Flash 类型 | 典型擦写次数 | 数据保持时间 | 应用场景 |
|------------|--------------|--------------|----------|
| 标准 NOR | 10,000 - 100,000 次 | 10 - 20 年 | 代码存储、固件升级 |
| Serial NOR (SPI) | 100,000 次 | 10 年以上 | 嵌入式系统、IoT 设备 |
| 并行 NOR | 10,000 - 100,000 次 | 10 - 20 年 | 高性能应用 |
| MLC NOR | 1,000 - 10,000 次 | 5 - 10 年 | 大容量存储（已较少使用） |

### 1.3 块层级寿命管理

Nor Flash 通常按块（Block/Sector）进行组织，每个块包含多个页（Page）。磨损均衡的基本单元是块，原因是：

- **擦除操作的最小单位是块**：无法对单个存储单元进行单独擦除
- **块是坏块管理的基本单位**：坏块标记以块为单位
- **块大小影响磨损分布**：大块可能导致小块区域过度使用

典型 Nor Flash 的块大小从 4KB 到 256KB 不等。SPI Nor Flash 通常使用统一的 4KB 或 64KB 扇区结构。

---

## 2. 静态磨损均衡与动态磨损均衡

### 2.1 动态磨损均衡

**动态磨损均衡（Dynamic Wear Leveling）** 只关注正在被写入的数据块，将新数据分配到使用次数较少的块中。这种方法实现简单，但只能处理活跃数据区域。

**工作原理**：

```
写入请求 -> 查找使用次数最少的块 -> 将数据写入该块 -> 更新映射表
```

**特点**：

- **优点**：实现简单，开销小，适用于只读或很少更新的应用
- **缺点**：无法处理静态数据（长期不更新但占用好块的情况）

**典型应用场景**：

- 文件系统的临时缓存区
- 日志存储区域
- 频繁更新的配置参数存储

### 2.2 静态磨损均衡

**静态磨损均衡（Static Wear Leveling）** 不仅处理活跃数据，还定期将长期未更新的静态数据移动到高使用次数的块中，从而回收"冷数据"块供新数据使用。这是更全面的磨损均衡策略。

**工作原理**：

```
周期性 识别长期扫描 ->未访问的块 -> 将冷数据移动到高磨损块 -> 擦除原块
```

**特点**：

- **优点**：均衡效果更好，能充分利用所有块的寿命
- **缺点**：实现复杂，需要额外的后台任务和存储开销

### 2.3 两种方案的对比

| 特性 | 动态磨损均衡 | 静态磨损均衡 |
|------|--------------|--------------|
| 复杂度 | 低 | 高 |
| 开销 | 小 | 中等（需要后台扫描） |
| 均衡效果 | 一般 | 优秀 |
| 静态数据处理 | 无 | 有 |
| 功耗影响 | 无 | 定期扫描增加功耗 |
| 适用场景 | 简单应用、只读存储 | 复杂文件系统、频繁写入 |

### 2.4 混合策略

实际应用中，通常采用混合策略：

1. **写入时动态均衡**：新数据写入选择低磨损块
2. **后台静态均衡**：定期扫描并移动冷数据块
3. **阈值触发**：当块磨损差异超过阈值时触发均衡

---

## 3. 磨损均衡算法实现

### 3.1 块状态管理结构

实现磨损均衡首先需要维护块的状态信息：

```c
// 块状态结构
typedef struct {
    uint32_t block_id;           // 块编号
    uint16_t erase_count;        // 擦除次数
    uint8_t  status;              // 块状态（可用/坏块/静态数据）
    uint32_t last_access_time;    // 最后访问时间戳
    uint8_t  data_type;           // 数据类型（静态/动态）
} block_info_t;

// 磨损均衡上下文
typedef struct {
    block_info_t *blocks;         // 块信息数组
    uint32_t total_blocks;        // 总块数
    uint32_t bad_blocks;          // 坏块数
    uint16_t max_erase_count;     // 最大擦除次数
    uint16_t min_erase_count;     // 最小擦除次数
    uint32_t wear_threshold;      // 磨损阈值（触发均衡）
    uint32_t static_check_interval; // 静态检查间隔
} wear_leveling_ctx_t;
```

### 3.2 动态磨损均衡实现

动态磨损均衡在每次写入时选择擦除次数最少的块：

```c
// 查找最佳写入块（擦除次数最少）
static int find_best_block(wear_leveling_ctx_t *ctx)
{
    uint32_t best_block = INVALID_BLOCK;
    uint16_t min_erase = UINT16_MAX;

    for (uint32_t i = 0; i < ctx->total_blocks; i++) {
        if (ctx->blocks[i].status == BLOCK_STATUS_AVAILABLE &&
            ctx->blocks[i].erase_count < min_erase) {
            min_erase = ctx->blocks[i].erase_count;
            best_block = i;
        }
    }

    return best_block;
}

// 带磨损均衡的写入操作
int wl_write(wear_leveling_ctx_t *ctx, uint32_t lba,
             const uint8_t *data, uint32_t len)
{
    // 1. 查找最佳块
    int block = find_best_block(ctx);
    if (block < 0) {
        return -ENOSPC;  // 无可用块
    }

    // 2. 检查是否需要块替换（当前块已写满）
    // ... 块管理和数据迁移逻辑 ...

    // 3. 执行写入
    int ret = flash_write(ctx->blocks[block].physical_addr, data, len);
    if (ret != 0) {
        // 标记为坏块
        ctx->blocks[block].status = BLOCK_STATUS_BAD;
        ctx->bad_blocks++;
        // 尝试其他块
        return wl_write(ctx, lba, data, len);
    }

    // 4. 更新块信息
    ctx->blocks[block].last_access_time = get_tick();

    return 0;
}
```

### 3.3 静态磨损均衡实现

静态磨损均衡需要后台任务定期执行：

```c
// 静态块判定阈值（毫秒）
#define STATIC_BLOCK_THRESHOLD   (60 * 60 * 1000)  // 1小时无访问

// 静态磨损均衡后台任务
void static_wear_leveling_task(wear_leveling_ctx_t *ctx)
{
    uint32_t current_time = get_tick();

    // 1. 查找静态数据块（长期未访问且擦除次数较低）
    for (uint32_t i = 0; i < ctx->total_blocks; i++) {
        if (ctx->blocks[i].status != BLOCK_STATUS_AVAILABLE) {
            continue;
        }

        // 检查是否为静态数据块
        if (current_time - ctx->blocks[i].last_access_time >
            STATIC_BLOCK_THRESHOLD) {

            // 2. 查找需要回收的高磨损块
            int target_block = find_high_wear_block(ctx);
            if (target_block < 0) {
                continue;
            }

            // 3. 将静态数据移动到高磨损块
            move_block_data(i, target_block, ctx);

            // 4. 擦除原块并更新计数
            flash_erase(ctx->blocks[i].physical_addr);
            ctx->blocks[i].erase_count++;
            ctx->blocks[i].last_access_time = current_time;
        }
    }

    // 5. 更新全局统计
    update_wear_statistics(ctx);
}

// 查找高磨损块（擦除次数最多的可用块）
static int find_high_wear_block(wear_leveling_ctx_t *ctx)
{
    uint32_t target_block = INVALID_BLOCK;
    uint16_t max_erase = 0;

    for (uint32_t i = 0; i < ctx->total_blocks; i++) {
        if (ctx->blocks[i].status == BLOCK_STATUS_AVAILABLE &&
            ctx->blocks[i].erase_count > max_erase &&
            ctx->blocks[i].erase_count < ctx->max_erase_count - 100) {
            max_erase = ctx->blocks[i].erase_count;
            target_block = i;
        }
    }

    return target_block;
}
```

### 3.4 块映射表管理

为了实现高效的磨损均衡，需要维护逻辑块地址（LBA）到物理块地址（PBA）的映射：

```c
// 映射表条目
typedef struct {
    uint32_t lba;                 // 逻辑块地址
    uint32_t pba;                // 物理块地址
    uint16_t version;            // 版本号（用于数据一致性）
} mapping_entry_t;

// 映射表结构
typedef struct {
    mapping_entry_t *entries;
    uint32_t total_entries;
    uint32_t flash_addr;         // 映射表存储位置
} mapping_table_t;

// 加载映射表
int mapping_table_load(mapping_table_t *table, nor_flash_t *flash)
{
    // 从 Flash 特定位置读取映射表
    return flash_read(table->flash_addr,
                      (uint8_t *)table->entries,
                      sizeof(mapping_entry_t) * table->total_entries);
}

// 保存映射表
int mapping_table_save(mapping_table_t *table, nor_flash_t *flash)
{
    // 将映射表写回 Flash
    return flash_write(table->flash_addr,
                       (const uint8_t *)table->entries,
                       sizeof(mapping_entry_t) * table->total_entries);
}
```

### 3.5 磨损均衡策略配置

根据应用场景选择合适的磨损均衡策略：

```c
// 磨损均衡配置
typedef struct {
    uint8_t  enable;                  // 使能开关
    uint8_t  mode;                    // 模式：0-禁用，1-动态，2-静态
    uint16_t wear_threshold;          // 触发阈值（擦除次数差）
    uint32_t static_check_period;     // 静态检查周期（毫秒）
    uint8_t  bad_block_reserve;       // 保留块比例（百分比）
} wear_leveling_config_t;

// 推荐配置示例
const wear_leveling_config_t default_config = {
    .enable = 1,
    .mode = WEAR_LEVELING_STATIC,     // 启用静态磨损均衡
    .wear_threshold = 1000,           // 擦除次数差超过1000时触发
    .static_check_period = 60000,     // 每分钟检查一次
    .bad_block_reserve = 2,           // 保留2%的块作为备用
};
```

### 3.6 坏块管理集成

磨损均衡需要与坏块管理紧密结合：

```c
// 坏块管理集成
int wl_init(wear_leveling_ctx_t *ctx, nor_flash_t *flash)
{
    // 1. 扫描并标记所有坏块
    ctx->bad_blocks = 0;
    for (uint32_t i = 0; i < ctx->total_blocks; i++) {
        if (is_bad_block(flash, i)) {
            ctx->blocks[i].status = BLOCK_STATUS_BAD;
            ctx->bad_blocks++;
        } else {
            ctx->blocks[i].status = BLOCK_STATUS_AVAILABLE;
            // 读取块的擦除计数（如果有）
            ctx->blocks[i].erase_count = read_erase_count(flash, i);
        }
    }

    // 2. 确保有足够的备用块
    uint32_t available = ctx->total_blocks - ctx->bad_blocks;
    uint32_t reserved = (ctx->total_blocks * ctx->bad_block_reserve) / 100;

    if (available <= reserved) {
        return -ENOSPC;  // 备用块不足
    }

    return 0;
}
```

---

## 4. 实际应用注意事项

### 4.1 性能与寿命的平衡

- **写入放大（Write Amplification）**：磨损均衡会增加实际写入量，需要权衡
- **后台任务调度**：静态均衡应在系统空闲时执行，避免影响实时性能
- **存储开销**：映射表和块信息需要额外的存储空间

### 4.2 可靠性考虑

- **掉电保护**：磨损均衡状态需要在掉电时保存
- **数据一致性**：块数据迁移过程中发生断电可能导致数据丢失
- **擦除计数存储**：定期保存擦除计数到 EEPROM 或专用块

### 4.3 监控与诊断

```c
// 磨损均衡状态查询
typedef struct {
    uint16_t avg_erase_count;     // 平均擦除次数
    uint16_t max_erase_count;     // 最大擦除次数
    uint16_t min_erase_count;     // 最小擦除次数
    uint16_t erase_count_diff;    // 擦除次数差异
    uint32_t bad_block_count;     // 坏块数量
    uint8_t  wear_level;          // 磨损等级（0-100%）
} wl_status_t;

void wl_get_status(wear_leveling_ctx_t *ctx, wl_status_t *status)
{
    uint32_t total = 0;
    status->max_erase_count = 0;
    status->min_erase_count = UINT16_MAX;

    for (uint32_t i = 0; i < ctx->total_blocks; i++) {
        if (ctx->blocks[i].status == BLOCK_STATUS_AVAILABLE) {
            total += ctx->blocks[i].erase_count;
            status->max_erase_count = MAX(status->max_erase_count,
                                          ctx->blocks[i].erase_count);
            status->min_erase_count = MIN(status->min_erase_count,
                                          ctx->blocks[i].erase_count);
        }
    }

    status->bad_block_count = ctx->bad_blocks;
    status->erase_count_diff = status->max_erase_count - status->min_erase_count;
    status->avg_erase_count = total / (ctx->total_blocks - ctx->bad_blocks);
    status->wear_level = (status->max_erase_count * 100) / ctx->max_erase_count;
}
```

---

## 5. 小结

磨损均衡是延长 Nor Flash 使用寿命的关键技术。通过合理选择动态或静态磨损均衡策略，并结合完善的块状态管理和坏块处理机制，可以显著提高 Flash 存储系统的可靠性和使用寿命。在实际应用中，需要根据具体的应用场景和性能要求进行权衡配置，并实现必要的监控和诊断功能。

---

**相关文档**

- [Nor Flash 驱动框架设计](../02-driver-development/01-driver-framework.md)
- [Flash 擦写算法](./06-flash-algorithm.md)

# Nor Flash 位操作与位编程

位操作（Bit Operation）和位编程（Bit Programming）是 Nor Flash 编程中的高级技术。与传统的整字节或整块写入不同，位级操作允许对 Flash 中的单个位进行修改，这在某些特定应用场景（如配置位更新、标志位设置）中非常有用。本文档将介绍位编程的原理、优化写入策略以及原子位操作的实现。

---

## 1. 位编程原理

### 1.1 Nor Flash 编程基础

Nor Flash 的编程（Program）操作具有以下特性：

- **只能从 1 写为 0**：Flash 单元的初始状态为 1（擦除后），编程操作可以将某些位从 1 变为 0
- **0 无法直接变回 1**：必须通过擦除操作（Erase）将块恢复到全 1 状态
- **编程以字（Word）为单位**：通常以 16 位或 32 位为单位进行编程
- **编程前无需擦除**：这是 Nor Flash 相对于 Nand Flash 的重要优势

### 1.2 位编程机制

位编程是 Nor Flash 的特有功能，允许对已编程数据的特定位进行修改：

**工作原理**：

```
原始数据: 0xFF (11111111)
写入数据: 0x0F (00001111)
─────────────────────────
结果数据: 0x0F (00001111)  // 高位被清除为0

原始数据: 0x0F (00001111)
写入数据: 0xFF (11111111)
─────────────────────────
结果数据: 0x0F (00001111)  // 无法将0恢复为1，无变化
```

**关键特性**：

- 位编程只能将 1 变为 0，不能将 0 变为 1
- 写入 0 的位置会导致相应位被清除
- 写入 1 的位置保持不变
- 写入的数据是原始数据和写入值的"与"运算结果

### 1.3 位编程命令

SPI Nor Flash 通常使用以下命令进行位编程：

| 命令 | 代码 | 说明 |
|------|------|------|
| READ | 0x03 / 0x0B | 读取数据 |
| PAGE PROGRAM | 0x02 | 页编程（标准编程） |
| BIT PROGRAM | 0x20 | 位编程（部分 Flash 支持） |
| WRITE ENABLE | 0x06 | 写使能 |
| READ STATUS | 0x05 | 读取状态寄存器 |
| CHIP ERASE | 0xC7 / 0x60 | 整片擦除 |
| SECTOR ERASE | 0xD8 | 扇区擦除 |

### 1.4 位编程时序

```
┌─────────────────────────────────────────────────────────────┐
│                    位编程操作时序                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  WREN ─┐                                                    │
│        │                                                    │
│  CS ───┘    ┌─────────────────────────────────────────┐    │
│        ─────┤                                         ├    │
│        ┌    │    发送位编程命令 (0x20)                 │    │
│  SI    │    │    发送地址 (24位)                      │    │
│        │    │    发送数据 (1-4字节)                    │    │
│        ▼    └─────────────────────────────────────────┘    │
│        │                                                    │
│  CS ──────────────────────────────────────────             │
│        │                                                    │
│  等待 ─────────────────────────────────────────────────    │
│        │                                                    │
│  状态 ─ OK ──────────────────────────────────────────      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 优化写入策略

### 2.1 编程类型选择

根据数据特性选择合适的编程方式：

```c
// 编程类型枚举
typedef enum {
    PROGRAM_TYPE_FULL,      // 整字节/整字编程
    PROGRAM_TYPE_BIT,       // 位编程
    PROGRAM_TYPE_INCREMENTAL, // 增量编程（多次写入同一地址）
    PROGRAM_TYPE_BUFFER     // 缓冲编程（批量写入）
} program_type_t;

// 根据数据特征选择编程类型
program_type_t select_program_type(const uint8_t *data, uint32_t len)
{
    uint32_t zero_bits = 0;
    uint32_t one_bits = 0;

    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == 0xFF) {
            one_bits++;  // 全部是1，无需写入
        } else if (data[i] == 0x00) {
            zero_bits++; // 全部是0，使用位编程
        }
    }

    // 决策逻辑
    if (zero_bits > len / 2) {
        return PROGRAM_TYPE_BIT;  // 大部分位需要清除
    } else if (len > 16) {
        return PROGRAM_TYPE_BUFFER;  // 大数据使用缓冲
    } else {
        return PROGRAM_TYPE_FULL;  // 标准编程
    }
}
```

### 2.2 写入优化策略

#### 2.2.1 增量写入策略

对于需要多次更新同一地址的场景（如日志追加、计数器），使用增量写入：

```c
// 增量写入缓冲区
typedef struct {
    uint32_t address;           // 目标地址
    uint32_t current_value;     // 当前值（从 Flash 读取）
    uint8_t  dirty;             // 是否有未写入的更改
    uint32_t write_count;       // 写入次数
} incremental_write_ctx_t;

// 增量写入 - 只清除需要变为0的位
int incremental_write(incremental_write_ctx_t *ctx, uint32_t addr,
                      uint32_t new_value, uint32_t mask)
{
    // 1. 读取当前值（如果需要）
    if (ctx->address != addr || !ctx->dirty) {
        if (ctx->dirty) {
            // 先写入之前缓存的数据
            flush_incremental_write(ctx);
        }
        nor_flash_read(addr, (uint8_t *)&ctx->current_value, 4);
        ctx->address = addr;
    }

    // 2. 计算增量值（只清除需要变为0的位）
    uint32_t increment = (~ctx->current_value) & new_value & mask;

    // 3. 使用位编程写入
    if (increment != 0) {
        int ret = bit_program(addr, increment);
        if (ret != 0) {
            return ret;
        }
        ctx->current_value |= increment;
        ctx->write_count++;
    }

    ctx->dirty = 1;
    return 0;
}

// 刷新缓冲区
int flush_incremental_write(incremental_write_ctx_t *ctx)
{
    if (!ctx->dirty) {
        return 0;
    }

    ctx->dirty = 0;
    return 0;
}
```

#### 2.2.2 批量位操作

对多个位进行批量操作：

```c
// 批量位操作
typedef struct {
    uint32_t addr;              // 起始地址
    uint32_t count;            // 操作数量
    uint8_t  bits_to_clear[16]; // 需要清除的位掩码
} bit_batch_op_t;

// 批量位清除（按地址排序，优化操作）
int bit_batch_clear(bit_batch_op_t *ops, uint32_t op_count)
{
    // 1. 按地址排序操作
    qsort(ops, op_count, sizeof(bit_batch_op_t), addr_compare);

    // 2. 合并相邻或重叠的操作
    bit_batch_op_t merged[16];
    uint32_t merged_count = merge_bit_ops(ops, op_count, merged);

    // 3. 执行合并后的操作
    for (uint32_t i = 0; i < merged_count; i++) {
        // 使用位编程命令
        int ret = bit_program_range(merged[i].addr,
                                    merged[i].bits_to_clear,
                                    merged[i].count);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}
```

### 2.3 写入性能优化

#### 2.3.1 缓存写入

使用 RAM 缓存减少 Flash 写入次数：

```c
// 写缓存管理
#define WRITE_CACHE_SIZE 256

typedef struct {
    uint8_t  data[WRITE_CACHE_SIZE];
    uint32_t base_addr;          // 缓存对应的起始地址
    uint32_t dirty_start;        // 脏数据起始偏移
    uint32_t dirty_end;          // 脏数据结束偏移
    uint8_t  valid;              // 缓存是否有效
} write_cache_t;

write_cache_t g_write_cache;

// 缓存写入
int cached_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    // 1. 检查是否需要刷新缓存
    if (g_write_cache.valid &&
        (addr < g_write_cache.base_addr ||
         addr >= g_write_cache.base_addr + WRITE_CACHE_SIZE)) {
        flush_cache(&g_write_cache);
    }

    // 2. 初始化新缓存
    if (!g_write_cache.valid) {
        g_write_cache.base_addr = addr & ~(WRITE_CACHE_SIZE - 1);
        g_write_cache.valid = 1;
        memset(g_write_cache.data, 0xFF, WRITE_CACHE_SIZE);
    }

    // 3. 复制数据到缓存
    uint32_t offset = addr - g_write_cache.base_addr;
    memcpy(g_write_cache.data + offset, data, len);

    // 4. 标记脏数据区域
    g_write_cache.dirty_start = MIN(g_write_cache.dirty_start, offset);
    g_write_cache.dirty_end = MAX(g_write_cache.dirty_end, offset + len);

    return 0;
}

// 刷新缓存到 Flash
int flush_cache(write_cache_t *cache)
{
    if (!cache->valid || cache->dirty_start >= cache->dirty_end) {
        return 0;
    }

    uint32_t dirty_len = cache->dirty_end - cache->dirty_start;
    uint32_t dirty_addr = cache->base_addr + cache->dirty_start;

    // 使用位编程写入脏数据区域
    int ret = bit_program_range(dirty_addr,
                               cache->data + cache->dirty_start,
                               dirty_len);

    if (ret == 0) {
        cache->dirty_start = WRITE_CACHE_SIZE;
        cache->dirty_end = 0;
    }

    return ret;
}
```

#### 2.3.2 并行写入

对于支持双通道或双 Die 的 Flash，可以并行写入：

```c
// 双通道写入（适用于双 Die Flash）
int dual_die_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t die0_addr, die1_addr;
    uint32_t die0_len, die1_len;

    // 分离地址到两个 Die
    split_to_die(addr, len, &die0_addr, &die0_len, &die1_addr, &die1_len);

    // 并行写入
    int ret0 = 0, ret1 = 0;

    if (die0_len > 0) {
        ret0 = nor_flash_write(die0_addr, data, die0_len);
    }

    if (die1_len > 0) {
        ret1 = nor_flash_write(die1_addr, data + die0_len, die1_len);
    }

    return (ret0 != 0) ? ret0 : ret1;
}
```

---

## 3. 原子位操作

### 3.1 原子性概念

原子操作（Atomic Operation）是指不可中断的操作，要么完全执行，要么完全不执行，不会出现中间状态。在 Flash 编程中，保证原子性对于防止数据损坏至关重要。

**Flash 操作的原子性问题**：

- **编程超时**：长时间编程可能被中断
- **掉电丢失**：编程过程中突然断电
- **状态错误**：操作失败但部分数据已写入

### 3.2 位级原子操作实现

```c
// 位操作标志
#define BIT_OP_SET    (1 << 0)   // 设置位（置1）
#define BIT_OP_CLEAR  (1 << 1)   // 清除位（置0）
#define BIT_OP_TOGGLE (1 << 2)   // 翻转位

// 原子位操作
typedef struct {
    uint32_t addr;              // 操作地址
    uint32_t mask;             // 位掩码
    uint8_t  operation;        // 操作类型
    uint8_t  completed;        // 操作完成标志
} atomic_bit_op_t;

// 原子设置位（将指定位置1）
int atomic_bit_set(uint32_t addr, uint32_t mask)
{
    uint8_t status;

    // 读取当前值
    uint32_t current;
    nor_flash_read(addr, (uint8_t *)&current, sizeof(current));

    // 计算新值（将目标位置1）
    uint32_t new_value = current | mask;

    // 如果新值与当前值相同，无需操作
    if (new_value == current) {
        return 0;
    }

    // 使用位编程写入
    int ret = bit_program(addr, new_value & mask);

    // 验证写入结果
    if (ret == 0) {
        nor_flash_read(addr, (uint8_t *)&current, sizeof(current));
        if ((current & mask) != (new_value & mask)) {
            ret = -EIO;
        }
    }

    return ret;
}

// 原子清除位（将指定位置0）
int atomic_bit_clear(uint32_t addr, uint32_t mask)
{
    // 读取当前值
    uint32_t current;
    nor_flash_read(addr, (uint8_t *)&current, sizeof(current));

    // 计算新值（将目标位清0）
    uint32_t new_value = current & ~mask;

    // 如果新值与当前值相同，无需操作
    if (new_value == current) {
        return 0;
    }

    // 使用位编程写入（只写入需要清0的位）
    int ret = bit_program(addr, new_value);

    // 验证写入结果
    if (ret == 0) {
        nor_flash_read(addr, (uint8_t *)&current, sizeof(current));
        if ((current & mask) != 0) {
            ret = -EIO;
        }
    }

    return ret;
}

// 原子翻转位
int atomic_bit_toggle(uint32_t addr, uint32_t mask)
{
    // 读取当前值
    uint32_t current;
    nor_flash_read(addr, (uint8_t *)&current, sizeof(current));

    // 计算新值（翻转目标位）
    uint32_t new_value = current ^ mask;

    // 如果新值与当前值相同，无需操作
    if (new_value == current) {
        return 0;
    }

    // 位编程只能清0，不能直接翻转
    // 需要先清0，再置1（如果是翻转1到0的位）
    uint32_t clear_bits = current & mask;  // 需要从1变0的位
    uint32_t set_bits = new_value & mask; // 需要从0变1的位

    if (clear_bits) {
        int ret = bit_program(addr, current & ~clear_bits);
        if (ret != 0) {
            return ret;
        }
    }

    if (set_bits) {
        int ret = bit_program(addr, set_bits);
        if (ret != 0) {
            return ret;
        }
    }

    // 验证
    nor_flash_read(addr, (uint8_t *)&current, sizeof(current));
    if ((current & mask) != (new_value & mask)) {
        return -EIO;
    }

    return 0;
}
```

### 3.3 多字节原子操作

对于多字节数据，需要保证写入的原子性：

```c
// 原子写入多字节数据
int atomic_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    // 1. 读取原始数据
    uint8_t *old_data = kmalloc(len);
    if (!old_data) {
        return -ENOMEM;
    }

    nor_flash_read(addr, old_data, len);

    // 2. 计算需要变更的位
    uint8_t *change_mask = kmalloc(len);
    if (!change_mask) {
        free(old_data);
        return -ENOMEM;
    }

    for (uint32_t i = 0; i < len; i++) {
        change_mask[i] = data[i] ^ old_data[i];
    }

    // 3. 只写入实际需要变更的字节
    int ret = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (change_mask[i] != 0) {
            // 读取当前值，应用变更
            uint8_t current;
            nor_flash_read(addr + i, &current, 1);
            uint8_t new_value = current & ~change_mask[i] |
                               (data[i] & change_mask[i]);

            ret = bit_program(addr + i, new_value);
            if (ret != 0) {
                break;
            }
        }
    }

    // 4. 验证写入
    if (ret == 0) {
        uint8_t *verify_data = kmalloc(len);
        nor_flash_read(addr, verify_data, len);

        if (memcmp(data, verify_data, len) != 0) {
            ret = -EIO;
        }

        free(verify_data);
    }

    free(old_data);
    free(change_mask);

    return ret;
}
```

### 3.4 比较并交换（CAS）操作

CAS（Compare-And-Swap）是实现无锁算法的关键操作：

```c
// 原子 CAS 操作
// 返回值：0 表示成功（值已更新），-EAGAIN 表示值已改变（需要重试）
int atomic_compare_and_swap(uint32_t addr, uint32_t old_value,
                            uint32_t new_value)
{
    uint32_t current;

    // 1. 读取当前值
    nor_flash_read(addr, (uint8_t *)&current, sizeof(current));

    // 2. 比较
    if (current != old_value) {
        return -EAGAIN;  // 值已改变
    }

    // 3. 尝试写入
    int ret = atomic_write(addr, (uint8_t *)&new_value, sizeof(new_value));

    if (ret != 0) {
        return ret;
    }

    // 4. 再次读取验证
    nor_flash_read(addr, (uint8_t *)&current, sizeof(current));
    if (current != new_value) {
        return -EAGAIN;  // 写入失败，需要重试
    }

    return 0;
}

// 带重试的 CAS 循环
int atomic_cas_with_retry(uint32_t addr, uint32_t old_value,
                          uint32_t new_value, int max_retries)
{
    int retries = 0;

    while (retries < max_retries) {
        int ret = atomic_compare_and_swap(addr, old_value, new_value);

        if (ret == 0) {
            return 0;  // 成功
        }

        if (ret != -EAGAIN) {
            return ret;  // 其他错误
        }

        // 重试前读取最新值
        nor_flash_read(addr, (uint8_t *)&old_value, sizeof(old_value));
        retries++;
    }

    return -ETIMEDOUT;  // 重试次数用尽
}
```

### 3.5 标志位管理

利用位操作实现高效的标志位管理：

```c
// Flash 标志位管理
typedef struct {
    uint32_t base_addr;          // 标志位区域基址
    uint32_t size;              // 区域大小（字节）
    uint32_t next_free_bit;     // 下一个可用位
} flag_manager_t;

// 初始化标志位管理器
int flag_manager_init(flag_manager_t *mgr, uint32_t base, uint32_t size)
{
    mgr->base_addr = base;
    mgr->size = size;
    mgr->next_free_bit = 0;

    // 扫描已使用的位
    for (uint32_t i = 0; i < size; i++) {
        uint8_t data;
        nor_flash_read(base + i, &data, 1);
        if (data != 0xFF) {
            // 找到已使用的位，更新 next_free_bit
            for (int bit = 0; bit < 8; bit++) {
                if (!(data & (1 << bit))) {
                    uint32_t bit_pos = i * 8 + bit;
                    if (bit_pos >= mgr->next_free_bit) {
                        mgr->next_free_bit = bit_pos + 1;
                    }
                }
            }
        }
    }

    return 0;
}

// 分配一个标志位
int flag_alloc(flag_manager_t *mgr, uint32_t *bit_pos)
{
    if (mgr->next_free_bit >= mgr->size * 8) {
        return -ENOSPC;  // 没有可用位
    }

    *bit_pos = mgr->next_free_bit;

    // 更新下一个可用位
    mgr->next_free_bit++;

    return 0;
}

// 设置标志位
int flag_set(flag_manager_t *mgr, uint32_t bit_pos)
{
    uint32_t addr = mgr->base_addr + (bit_pos / 8);
    uint8_t mask = 1 << (bit_pos % 8);

    return atomic_bit_clear(addr, mask);  // 清除位表示标志置位
}

// 清除标志位
int flag_clear(flag_manager_t *mgr, uint32_t bit_pos)
{
    uint32_t addr = mgr->base_addr + (bit_pos / 8);
    uint8_t mask = 1 << (bit_pos % 8);

    return atomic_bit_set(addr, mask);  // 设置位表示标志清除
}

// 检查标志位
int flag_test(flag_manager_t *mgr, uint32_t bit_pos)
{
    uint8_t data;
    uint32_t addr = mgr->base_addr + (bit_pos / 8);

    nor_flash_read(addr, &data, 1);

    return !(data & (1 << (bit_pos % 8)));  // 0 表示置位，1 表示清除
}
```

---

## 4. 实际应用示例

### 4.1 配置位管理

使用位操作管理系统配置：

```c
// 配置位定义
#define CONFIG_FLAG_BOOT_COMPLETE   (1 << 0)
#define CONFIG_FLAG_FACTORY_RESET   (1 << 1)
#define CONFIG_FLAG_OTA_PENDING     (1 << 2)
#define CONFIG_FLAG_DEBUG_MODE      (1 << 3)
#define CONFIG_FLAG_ENTROPY_INIT    (1 << 4)

// 配置区域地址
#define CONFIG_FLAGS_ADDR   0x0000
#define CONFIG_DATA_ADDR    0x0010

// 初始化系统配置
int config_init(void)
{
    // 首次启动时初始化配置区域
    uint8_t flag;
    nor_flash_read(CONFIG_FLAGS_ADDR, &flag, 1);

    if (flag == 0xFF) {
        // 首次启动，设置默认值
        nor_flash_write(CONFIG_FLAGS_ADDR, (uint8_t[]){0xFF}, 1);
        nor_flash_write(CONFIG_DATA_ADDR,
                        (uint8_t[]){0x00, 0x00, 0x00, 0x00}, 4);
    }

    return 0;
}

// 设置启动完成标志
int set_boot_complete(void)
{
    return atomic_bit_clear(CONFIG_FLAGS_ADDR, CONFIG_FLAG_BOOT_COMPLETE);
}

// 检查是否需要恢复出厂设置
int check_factory_reset(void)
{
    uint8_t flag;
    nor_flash_read(CONFIG_FLAGS_ADDR, &flag, 1);
    return !(flag & CONFIG_FLAG_FACTORY_RESET);
}

// 请求 OTA 升级
int request_ota_update(void)
{
    return atomic_bit_clear(CONFIG_FLAGS_ADDR, CONFIG_FLAG_OTA_PENDING);
}
```

### 4.2 事件日志记录

使用位操作记录事件：

```c
// 事件日志（使用位图）
#define MAX_EVENTS 256
#define EVENT_LOG_ADDR  0x1000

typedef struct {
    uint32_t addr;
    uint8_t  events[(MAX_EVENTS + 7) / 8];
} event_log_t;

// 记录事件
int event_log(uint32_t event_id)
{
    if (event_id >= MAX_EVENTS) {
        return -EINVAL;
    }

    uint32_t addr = EVENT_LOG_ADDR + (event_id / 8);
    uint8_t mask = 1 << (event_id % 8);

    return atomic_bit_clear(addr, mask);
}

// 检查事件是否已记录
int event_check(uint32_t event_id)
{
    if (event_id >= MAX_EVENTS) {
        return 0;
    }

    uint8_t data;
    uint32_t addr = EVENT_LOG_ADDR + (event_id / 8);

    nor_flash_read(addr, &data, 1);

    return !(data & (1 << (event_id % 8)));
}

// 清除事件记录
int event_clear(uint32_t event_id)
{
    if (event_id >= MAX_EVENTS) {
        return -EINVAL;
    }

    uint32_t addr = EVENT_LOG_ADDR + (event_id / 8);
    uint8_t mask = 1 << (event_id % 8);

    return atomic_bit_set(addr, mask);
}
```

---

## 5. 小结

位操作和位编程是 Nor Flash 编程中的重要技术。通过合理利用位编程特性，可以实现高效的数据更新、原子操作和状态管理。在实际应用中，需要注意 Flash 的物理限制（只能从 1 写为 0），并结合缓存、批量操作等优化策略来提升性能和可靠性。

---

**相关文档**

- [Nor Flash 驱动框架设计](../02-driver-development/01-driver-framework.md)
- [SPI Flash 驱动实现](../02-driver-development/02-spi-driver-impl.md)
- [Nor Flash 掉电保护策略](./04-power-fail-protection.md)

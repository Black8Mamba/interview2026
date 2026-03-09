# Nor Flash ECC 纠错机制

Error Correction Code（ECC，纠错码）是保证 Nor Flash 数据可靠性的重要机制。随着 Flash 工艺制程的缩小和存储密度的增加，数据位翻转（Bit Flip）等问题越来越常见。本文档将介绍 ECC 的基本原理、硬件与软件实现方案，以及在 Nor Flash 中的应用。

---

## 1. ECC 原理简介

### 1.1 存储错误类型

Nor Flash 中可能发生的错误主要包括：

- **位翻转（Bit Flip）**：存储单元的电荷状态发生意外改变，导致 0 变为 1 或 1 变为 0
- **位粘连（Bit Stuck）**：某个位始终保持在固定状态（通常为 1）
- **读写干扰（Read/Write Disturb）**：相邻单元的操作影响目标单元
- **数据保持错误（Data Retention Error）**：随着时间推移，电荷泄漏导致数据错误

### 1.2 ECC 基本概念

ECC 通过在数据中添加冗余校验位来实现错误检测和纠正。主要参数包括：

| 参数 | 含义 | 典型值 |
|------|------|--------|
| 数据位宽 | 原始数据的位数 | 256、512、1024 字节 |
| 校验位 | ECC 校验码的位数 | 3-24 位 |
| 纠错能力 | 能纠正的错误位数 | 1-8 位 |
| 检测能力 | 能检测的错误位数 | 2-8 位 |

### 1.3 常见 ECC 算法

#### 1.3.1 汉明码（Hamming Code）

汉明码是最常用的 ECC 算法，能够纠正单比特错误（SEC，Single Error Correction）：

```c
// 汉明码计算（简化版本，针对8位数据）
// 生成 4 位 ECC 校验码
uint8_t hamming_encode_8(uint8_t data)
{
    uint8_t ecc = 0;

    // P1: 检查位 1,3,5,7,9,11
    ecc |= ((((data >> 1) & 0x55) ^ ((data >> 3) & 0x55) ^
             ((data >> 5) & 0x55) ^ ((data >> 7) & 0x55)) & 0x01) << 0;

    // P2: 检查位 2,3,6,7,10,11
    ecc |= ((((data >> 2) & 0x33) ^ ((data >> 3) & 0x33) ^
             ((data >> 6) & 0x33) ^ ((data >> 7) & 0x33)) & 0x01) << 1;

    // P4: 检查位 4-7
    ecc |= ((((data >> 4) & 0x0F) ^ ((data >> 5) & 0x0F) ^
             ((data >> 6) & 0x0F) ^ ((data >> 7) & 0x0F)) & 0x01) << 2;

    // P8: 检查位 8-15
    ecc |= ((((data >> 8) & 0x0F) ^ ((data >> 9) & 0x0F) ^
             ((data >> 10) & 0x0F) ^ ((data >> 11) & 0x0F)) & 0x01) << 3;

    return ecc;
}

// 汉明码纠错
int hamming_correct_8(uint8_t *data, uint8_t ecc_read, uint8_t ecc_calc)
{
    uint8_t syndrome = ecc_read ^ ecc_calc;

    if (syndrome == 0) {
        return 0;  // 无错误
    }

    // 检查是否可纠正（单比特错误）
    // 汉明码能检测双比特错误但无法纠正
    int bit_pos = -1;
    for (int i = 0; i < 8; i++) {
        if (syndrome == (1 << i)) {
            bit_pos = i;
            break;
        }
    }

    if (bit_pos >= 0) {
        // 纠正 ECC 本身的错误
        *data ^= (1 << bit_pos);
        return 1;  // 已纠正
    }

    return -EBADMSG;  // 不可纠正的错误
}
```

#### 1.3.2 BCH 码

BCH（Bose-Chaudhuri-Hocquenghem）码是一种更强大的纠错码，能够纠正多个比特错误：

| BCH 类型 | 数据位 | 校验位 | 纠错能力 |
|----------|--------|--------|----------|
| BCH(15,7) | 7 | 8 | 2 位 |
| BCH(31,21) | 21 | 10 | 2 位 |
| BCH(63,51) | 51 | 12 | 2 位 |
| BCH(255,231) | 231 | 24 | 4 位 |

#### 1.3.3 RS 码（Reed-Solomon）

RS 码特别适合处理字节级的错误，在 Flash 存储中广泛应用：

- **符号单位**：以字节（8位）为单位
- **纠错能力**：每 255 个符号可纠正 16 个符号错误
- **应用场景**：常与 BCH 码结合使用

### 1.4 ECC 在 Flash 中的存储

ECC 校验码需要存储在数据附近，通常有几种方式：

1. **专用 ECC 区域**：在每个扇区/块中预留专门的 ECC 存储空间
2. **带外存储（OOB）**：在 Flash 的 Out-of-Band 区域存储 ECC
3. **独立 ECC 块**：使用单独的 Flash 块存储 ECC 数据

```
┌────────────────────┬────────────────────┐
│    主数据区         │     ECC 区域        │
│    (Main Data)     │   (ECC Bits)       │
│    N bytes         │   M bytes          │
└────────────────────┴────────────────────┘
```

---

## 2. 硬件 ECC 与软件 ECC

### 2.1 硬件 ECC

硬件 ECC 由专用的硬件模块（集成在 Flash 控制器或独立芯片中）执行，具有以下特点：

**优点**：

- **速度快**：硬件并行计算，性能优于软件实现
- **CPU 零负担**：不占用 CPU 资源
- **可靠性高**：硬件实现经过严格验证

**缺点**：

- **成本增加**：需要额外的硬件模块
- **灵活性差**：算法固定，难以自定义
- **资源独占**：通常只能被特定控制器使用

**典型硬件 ECC 控制器**：

```c
// 硬件 ECC 控制器配置
typedef struct {
    uint8_t  ecc_mode;            // ECC 模式：硬件/软件
    uint8_t  ecc_type;           // ECC 类型：汉明码/BCH/RS
    uint32_t data_width;         // 数据位宽
    uint32_t ecc_bits;           // ECC 位数
    uint8_t  error_threshold;    // 错误阈值
} hw_ecc_config_t;

// 硬件 ECC 初始化
int hw_ecc_init(hw_ecc_config_t *config)
{
    // 配置 ECC 控制器
    ECC_CR |= config->ecc_mode;
    ECC_MODER |= config->ecc_type;
    ECC_PAGESIZE = config->data_width;

    // 启用 ECC
    ECC_CR |= ECC_CR_EN;

    return 0;
}

// 使用硬件 ECC 读取数据
int hw_ecc_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    // 1. 启用硬件 ECC 引擎
    ECC_CR |= ECC_CR_EN;

    // 2. 读取数据（硬件自动计算 ECC）
    nor_flash_read(addr, data, len);

    // 3. 读取计算出的 ECC 值
    uint32_t ecc_calc = ECC_DR;

    // 4. 从存储区域读取原始 ECC
    uint32_t ecc_stored = read_ecc_from_oob(addr);

    // 5. 比较并纠错
    uint32_t syndrome = ecc_calc ^ ecc_stored;

    if (syndrome != 0) {
        // 根据硬件手册处理错误
        return handle_ecc_error(syndrome, data);
    }

    return 0;
}
```

### 2.2 软件 ECC

软件 ECC 完全由 CPU 执行，不需要专用硬件：

**优点**：

- **成本低**：无需额外硬件
- **灵活可变**：可实现自定义算法
- **易于升级**：软件更新方便

**缺点**：

- **性能开销**：消耗 CPU 资源
- **实现复杂**：需要仔细实现以确保正确性

**软件 ECC 实现示例**：

```c
// 256 字节数据的软件 ECC（使用 BCH 算法简化版）
#define ECC_TABLE_SIZE 256

// BCH 编码表（简化示例）
static const uint8_t bch_enc_table[ECC_TABLE_SIZE] = {
    // 预计算的 BCH 编码值
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    // ... 完整表格需要 256 项
};

// 计算 256 字节数据的 ECC（简化 BCH）
void software_ecc_calc(const uint8_t *data, uint8_t *ecc)
{
    uint8_t ecc0 = 0, ecc1 = 0, ecc2 = 0;

    for (uint32_t i = 0; i < 256; i++) {
        uint8_t index = data[i] ^ ecc2;
        ecc2 = ecc1;
        ecc1 = ecc0;
        ecc0 = bch_enc_table[index];
    }

    ecc[0] = ecc0;
    ecc[1] = ecc1;
    ecc[2] = ecc2;
}

// 软件 ECC 纠错
int software_ecc_correct(uint8_t *data, const uint8_t *ecc_read,
                         const uint8_t *ecc_calc)
{
    uint8_t syndrome[3];
    syndrome[0] = ecc_read[0] ^ ecc_calc[0];
    syndrome[1] = ecc_read[1] ^ ecc_calc[1];
    syndrome[2] = ecc_read[2] ^ ecc_calc[2];

    // 检查是否有错误
    if (syndrome[0] == 0 && syndrome[1] == 0 && syndrome[2] == 0) {
        return 0;  // 无错误
    }

    // 简化的纠错逻辑（实际需要完整的查表过程）
    // ... 根据 syndrome 查找错误位置并纠正 ...

    return 1;  // 已纠正错误
}
```

### 2.3 硬件与软件 ECC 对比

| 特性 | 硬件 ECC | 软件 ECC |
|------|----------|----------|
| 性能 | 高 | 中等 |
| CPU 占用 | 无 | 有 |
| 成本 | 高（需要硬件） | 低 |
| 灵活性 | 低（算法固定） | 高 |
| 纠错能力 | 可很强 | 可自定义 |
| 功耗 | 较低 | 较高 |
| 调试难度 | 较难 | 较易 |

### 2.4 混合方案

许多系统采用硬件和软件 ECC 混合方案：

1. **读取时使用硬件 ECC**：提高读取性能
2. **写入时使用软件 ECC**：计算一次，多次使用
3. **错误处理使用软件 ECC**：复杂错误场景使用软件处理

```c
// 混合 ECC 方案
typedef struct {
    uint8_t  use_hw;              // 优先使用硬件 ECC
    uint8_t  enable_software_fallback; // 硬件失败时使用软件
} ecc_strategy_t;

// 读取时的 ECC 处理
int ecc_read_with_strategy(uint32_t addr, uint8_t *data, uint32_t len,
                           ecc_strategy_t *strategy)
{
    int ret;

    if (strategy->use_hw) {
        // 尝试硬件 ECC
        ret = hw_ecc_read(addr, data, len);
        if (ret == 0 || !strategy->enable_software_fallback) {
            return ret;
        }
        // 硬件 ECC 失败，回退到软件
    }

    // 软件 ECC 处理
    return software_ecc_read(addr, data, len);
}
```

---

## 3. ECC 实现方案

### 3.1 ECC 模块架构设计

```c
// ECC 操作结果
typedef enum {
    ECC_OK = 0,
    ECC_ERROR_CORRECTED,      // 错误已纠正
    ECC_ERROR_UNCORRECTABLE,  // 不可纠正错误
    ECC_ERROR_HARDWARE,       // 硬件错误
    ECC_ERROR_TIMEOUT         // 操作超时
} ecc_result_t;

// ECC 配置
typedef struct {
    uint8_t  algo;                 // 算法：汉明码/BCH/RS
    uint32_t data_size;           // 数据块大小
    uint32_t ecc_size;            // ECC 校验码大小
    uint8_t  correction_capability; // 纠错能力（位数）
    uint8_t  use_hw;              // 使用硬件 ECC
    uint32_t hw_ecc_base;         // 硬件 ECC 寄存器基址
} ecc_config_t;

// ECC 上下文
typedef struct {
    ecc_config_t config;
    void *hw_context;             // 硬件上下文
    uint32_t total_errors;        // 总错误计数
    uint32_t corrected_errors;    // 已纠正错误计数
    uint32_t uncorrectable_errors; // 不可纠正错误计数
} ecc_context_t;
```

### 3.2 统一 ECC 接口

```c
// ECC 抽象接口
typedef struct {
    int (*init)(ecc_context_t *ctx, const ecc_config_t *config);
    int (*calc)(ecc_context_t *ctx, const uint8_t *data,
                uint32_t len, uint8_t *ecc);
    int (*verify)(ecc_context_t *ctx, const uint8_t *data,
                  uint32_t len, const uint8_t *ecc_read,
                  const uint8_t *ecc_calc);
    int (*correct)(ecc_context_t *ctx, uint8_t *data,
                   uint32_t len, const uint8_t *ecc_read,
                   const uint8_t *ecc_calc);
    int (*deinit)(ecc_context_t *ctx);
} ecc_ops_t;

// ECC 驱动注册
int ecc_register_driver(ecc_ops_t *ops)
{
    static ecc_ops_t *current_ops = NULL;
    current_ops = ops;
    return 0;
}
```

### 3.3 读取流程中的 ECC 处理

```c
// 带 ECC 验证的 Flash 读取
int flash_read_with_ecc(ecc_context_t *ctx, uint32_t addr,
                        uint8_t *data, uint32_t len)
{
    // 1. 读取主数据区
    int ret = nor_flash_read(addr, data, len);
    if (ret != 0) {
        return ret;
    }

    // 2. 读取存储的 ECC
    uint8_t ecc_stored[32];  // 根据 ECC 大小调整
    read_ecc_data(addr, ecc_stored, ctx->config.ecc_size);

    // 3. 计算当前数据的 ECC
    uint8_t ecc_calc[32];
    ctx->config.ops->calc(ctx, data, len, ecc_calc);

    // 4. 验证并纠正
    ret = ctx->config.ops->verify(ctx, data, len, ecc_stored, ecc_calc);

    switch (ret) {
        case ECC_OK:
            return 0;

        case ECC_ERROR_CORRECTED:
            ctx->corrected_errors++;
            log_warning("ECC: Corrected %d bit error at 0x%08X\n",
                       correction_bits, addr);
            return 0;

        case ECC_ERROR_UNCORRECTABLE:
            ctx->uncorrectable_errors++;
            log_error("ECC: Uncorrectable error at 0x%08X\n", addr);
            return -EBADMSG;

        default:
            return -EIO;
    }
}
```

### 3.4 写入流程中的 ECC 处理

```c
// 带 ECC 的 Flash 写入
int flash_write_with_ecc(ecc_context_t *ctx, uint32_t addr,
                        const uint8_t *data, uint32_t len)
{
    // 1. 计算数据的 ECC
    uint8_t ecc[32];
    ctx->config.ops->calc(ctx, data, len, ecc);

    // 2. 写入主数据区
    int ret = nor_flash_write(addr, data, len);
    if (ret != 0) {
        return ret;
    }

    // 3. 写入 ECC 数据
    ret = write_ecc_data(addr, ecc, ctx->config.ecc_size);
    if (ret != 0) {
        log_error("Failed to write ECC data\n");
        return ret;
    }

    return 0;
}
```

### 3.5 错误统计与监控

```c
// ECC 统计信息
typedef struct {
    uint32_t total_reads;          // 总读取次数
    uint32_t total_writes;         // 总写入次数
    uint32_t errors_corrected;     // 已纠正错误数
    uint32_t errors_uncorrectable; // 不可纠正错误数
    uint32_t error_rate;           // 错误率（百分比）
    uint32_t max_consecutive_errors; // 最大连续错误数
} ecc_statistics_t;

// 获取 ECC 统计
void ecc_get_statistics(ecc_context_t *ctx, ecc_statistics_t *stats)
{
    stats->total_reads = ctx->total_reads;
    stats->total_writes = ctx->total_writes;
    stats->errors_corrected = ctx->corrected_errors;
    stats->errors_uncorrectable = ctx->uncorrectable_errors;

    if (stats->total_reads > 0) {
        stats->error_rate = (ctx->corrected_errors + ctx->uncorrectable_errors)
                            * 100 / stats->total_reads;
    } else {
        stats->error_rate = 0;
    }
}

// ECC 错误告警
void ecc_check_health(ecc_context_t *ctx)
{
    ecc_statistics_t stats;
    ecc_get_statistics(ctx, &stats);

    if (stats.error_rate > 10) {
        log_critical("Flash ECC error rate critically high: %u%%\n",
                    stats.error_rate);
        // 触发告警或数据迁移
    } else if (stats.error_rate > 5) {
        log_warning("Flash ECC error rate elevated: %u%%\n",
                   stats.error_rate);
    }
}
```

### 3.6 高级 ECC 策略

#### 3.6.1 动态 ECC 强度

根据错误率动态调整 ECC 强度：

```c
// 动态 ECC 策略
typedef struct {
    uint8_t  min_strength;         // 最小 ECC 强度
    uint8_t  max_strength;         // 最大 ECC 强度
    uint32_t error_threshold_low;  // 降级阈值
    uint32_t error_threshold_high; // 升级阈值
    uint32_t check_interval;       // 检查间隔
} dynamic_ecc_policy_t;

void dynamic_ecc_adjust(ecc_context_t *ctx, dynamic_ecc_policy_t *policy)
{
    uint32_t recent_errors = get_recent_error_count(ctx);

    if (recent_errors > policy->error_threshold_high &&
        ctx->current_strength < policy->max_strength) {
        // 错误率升高，增加 ECC 强度
        increase_ecc_strength(ctx);
        log_info("Increased ECC strength to %d\n", ctx->current_strength);
    } else if (recent_errors < policy->error_threshold_low &&
               ctx->current_strength > policy->min_strength) {
        // 错误率降低，可以降低 ECC 强度以提高性能
        decrease_ecc_strength(ctx);
        log_info("Decreased ECC strength to %d\n", ctx->current_strength);
    }
}
```

#### 3.6.2 冗余存储策略

对于关键数据，可以采用多副本冗余存储：

```c
// 冗余存储 ECC 方案
#define REDUNDANT_COPIES 3

int redundant_read_with_ecc(ecc_context_t *ctx, uint32_t addr,
                            uint8_t *data, uint32_t len)
{
    uint8_t buffer[REDUNDANT_COPIES][len];
    int valid_copies = 0;

    // 读取所有副本
    for (int i = 0; i < REDUNDANT_COPIES; i++) {
        int ret = flash_read_with_ecc(ctx, addr + i * len, buffer[i], len);
        if (ret == 0) {
            valid_copies++;
        }
    }

    if (valid_copies == 0) {
        return -EBADMSG;  // 所有副本都失败
    }

    // 多数投票（如果多个副本成功）
    if (valid_copies >= 2) {
        majority_vote(buffer, valid_copies, data);
    } else {
        // 只有一个副本成功
        memcpy(data, buffer[0], len);
    }

    return 0;
}
```

---

## 4. 小结

ECC 纠错机制是保障 Nor Flash 数据可靠性的关键手段。选择硬件 ECC 还是软件 ECC 需要根据具体的应用场景、成本预算和性能要求进行权衡。在实际实现中，建议采用模块化的 ECC 接口设计，支持多种算法和动态策略，并根据错误率监控来调整 ECC 强度，以实现性能和可靠性的最佳平衡。

---

**相关文档**

- [Nor Flash 驱动框架设计](../02-driver-development/01-driver-framework.md)
- [Nor Flash 硬件基础](../01-hardware-basics/01-nor-flash-intro.md)
- [SPI Flash 驱动实现](../02-driver-development/02-spi-driver-impl.md)

# Nor Flash 性能调优

Nor Flash 的性能优化是提升系统整体效率的关键环节。本章详细介绍批量操作优化、DMA 和双缓冲应用、缓存策略以及吞吐量测试等性能调优技术，帮助开发者充分发挥 Nor Flash 的性能潜力。

---

## 1. 批量操作优化

批量操作是指一次性处理大量数据的读写操作，通过优化批量操作的实现方式，可以显著提升数据传输效率。

### 1.1 连续读写优化

Nor Flash 的页编程和读取操作都有特定的边界限制，连续读写需要在这些限制之间进行合理的数据分块。

**页边界处理优化**

Nor Flash 的页编程操作有严格的页边界限制，每次编程操作不能超过单页大小，且不能跨页边界。在实现连续写入时，需要正确计算每页的剩余空间：

```c
/**
 * 优化后的连续写入函数
 * 减少函数调用开销和状态检查次数
 */
int nor_write_optimized(nor_device_t *dev, uint32_t addr,
                        const uint8_t *buf, uint32_t len)
{
    uint32_t page_size = dev->page_size;
    uint32_t wrote = 0;

    /* 预先等待设备就绪 */
    int ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    while (wrote < len) {
        /* 计算当前页的起始地址和剩余空间 */
        uint32_t page_start = addr & ~(page_size - 1);  /* 页起始地址 */
        uint32_t page_offset = addr % page_size;         /* 页内偏移 */
        uint32_t page_remaining = page_size - page_offset; /* 页剩余空间 */

        /* 本次写入长度：取剩余数据量和页剩余空间的最小值 */
        uint32_t chunk = (len - wrote) < page_remaining ? (len - wrote) : page_remaining;

        /* 发送写使能（每页操作前） */
        ret = nor_write_enable(dev);
        if (ret < 0) {
            return ret;
        }

        /* 发送页编程命令 */
        nor_transport_cmd(dev->transport, dev->cmd.page_program);

        /* 发送地址 */
        nor_send_address(dev, addr);

        /* 发送数据 */
        nor_transport_send(dev->transport, buf + wrote, chunk);

        /* 等待本页写入完成 */
        ret = nor_wait_ready(dev, NOR_TIMEOUT_PAGE_PROGRAM);
        if (ret < 0) {
            return ret;
        }

        addr += chunk;
        wrote += chunk;
    }

    return (int)wrote;
}

/**
 * 批量写入优化版本
 * 预先计算所有页边界，减少运行时计算
 */
int nor_write_batch(nor_device_t *dev, uint32_t addr,
                   const uint8_t *buf, uint32_t len)
{
    if (len == 0) {
        return 0;
    }

    /* 计算页对齐信息 */
    uint32_t page_size = dev->page_size;
    uint32_t first_page_end = page_size - (addr % page_size);
    uint32_t num_full_pages = (len - first_page_end) / page_size;
    uint32_t last_page_size = (len - first_page_end) % page_size;

    uint32_t pos = 0;
    int ret;

    /* 处理第一页（可能不完整） */
    if (first_page_end > 0) {
        uint32_t chunk = (len < first_page_end) ? len : first_page_end;
        ret = nor_page_program(dev, addr, buf + pos, chunk);
        if (ret < 0) {
            return ret;
        }
        addr += chunk;
        pos += chunk;
    }

    /* 处理完整的页 */
    for (uint32_t i = 0; i < num_full_pages; i++) {
        ret = nor_page_program(dev, addr, buf + pos, page_size);
        if (ret < 0) {
            return ret;
        }
        addr += page_size;
        pos += page_size;
    }

    /* 处理最后一页（如果存在且不完整） */
    if (last_page_size > 0 && pos < len) {
        ret = nor_page_program(dev, addr, buf + pos, last_page_size);
        if (ret < 0) {
            return ret;
        }
    }

    return (int)len;
}
```

### 1.2 擦除批量优化

擦除操作是 Nor Flash 最耗时的操作，批量擦除优化需要合理选择擦除粒度和调度策略。

**动态擦除粒度选择**

根据写入数据的大小动态选择擦除粒度，可以平衡擦除时间和存储空间利用率：

```c
/**
 * 动态擦除策略
 * 根据区域大小自动选择最优擦除粒度
 */
int nor_erase_optimized(nor_device_t *dev, uint32_t addr, uint32_t len)
{
    uint32_t sector_size = dev->sector_size;
    uint32_t block_size_32k = 32 * 1024;
    uint32_t block_size_64k = dev->block_size;
    uint32_t erased = 0;
    int ret;

    /* 计算起始偏移对齐 */
    uint32_t start_offset = addr % sector_size;
    if (start_offset > 0) {
        /* 起始地址不对齐，先处理头部 */
        uint32_t head_len = sector_size - start_offset;
        if (head_len > len) {
            head_len = len;
        }
        ret = nor_erase_sector(dev, addr - start_offset);
        if (ret < 0) {
            return ret;
        }
        addr += head_len;
        len -= head_len;
        erased += head_len;
    }

    /* 使用最大块擦除 */
    while (len >= block_size_64k) {
        ret = nor_erase_block(dev, addr, block_size_64k);
        if (ret < 0) {
            return ret;
        }
        addr += block_size_64k;
        len -= block_size_64k;
        erased += block_size_64k;

        /* 报告进度 */
        if (dev->callbacks.progress) {
            dev->callbacks.progress(NOR_EVENT_ERASE_PROGRESS,
                                   (uint8_t)((erased * 100) / (erased + len)));
        }
    }

    /* 处理剩余部分 */
    while (len >= block_size_32k) {
        ret = nor_erase_block(dev, addr, block_size_32k);
        if (ret < 0) {
            return ret;
        }
        addr += block_size_32k;
        len -= block_size_32k;
        erased += block_size_32k;
    }

    /* 处理尾部 */
    if (len > 0) {
        uint32_t sector_addr = addr - (addr % sector_size);
        ret = nor_erase_sector(dev, sector_addr);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}
```

### 1.3 命令队列与流水线

对于支持流水线操作的 Flash 芯片，可以利用命令队列和流水线技术提高吞吐量。

**流水线操作**

某些高性能 Flash 支持命令流水线，可以在上一批数据写入期间准备下一批命令：

```c
/**
 * 流水线写入模式
 * 利用 Flash 内部缓冲减少等待时间
 */
typedef struct {
    nor_device_t *dev;
    uint8_t *buffer;
    uint32_t buffer_size;
    uint32_t write_pos;
    uint8_t pending;  /* 有待处理的数据 */
} nor_pipeline_ctx_t;

int nor_pipeline_init(nor_pipeline_ctx_t *ctx, nor_device_t *dev,
                      uint32_t buffer_size)
{
    ctx->dev = dev;
    ctx->buffer = (uint8_t *)malloc(buffer_size);
    if (!ctx->buffer) {
        return -NOR_ERR_NO_MEMORY;
    }
    ctx->buffer_size = buffer_size;
    ctx->write_pos = 0;
    ctx->pending = 0;
    return 0;
}

int nor_pipeline_write(nor_pipeline_ctx_t *ctx, uint32_t addr,
                       const uint8_t *data, uint32_t len)
{
    nor_device_t *dev = ctx->dev;
    uint32_t page_size = dev->page_size;
    uint32_t written = 0;

    while (written < len) {
        /* 计算当前页的剩余空间 */
        uint32_t page_offset = addr % page_size;
        uint32_t page_remaining = page_size - page_offset;
        uint32_t chunk = page_remaining;
        if (chunk > (len - written)) {
            chunk = len - written;
        }

        /* 发送写使能 */
        int ret = nor_write_enable(dev);
        if (ret < 0) {
            return ret;
        }

        /* 发送页编程命令和地址 */
        nor_transport_cmd(dev->transport, dev->cmd.page_program);
        nor_send_address(dev, addr);

        /* 发送数据 */
        nor_transport_send(dev->transport, data + written, chunk);

        /* 不等待完成，立即返回准备下一次写入 */
        /* 注意：需要在后续操作前等待完成 */

        addr += chunk;
        written += chunk;
    }

    return (int)written;
}

/**
 * 刷新流水线，等待所有待处理操作完成
 */
int nor_pipeline_flush(nor_pipeline_ctx_t *ctx)
{
    return nor_wait_ready(ctx->dev, NOR_TIMEOUT_CHIP_ERASE);
}
```

---

## 2. DMA 与双缓冲应用

DMA（直接内存访问）和双缓冲技术可以显著提升大数据传输的效率，减少 CPU 干预。

### 2.1 DMA 传输配置

DMA 允许外设与内存直接交换数据，无需 CPU 参与，适合大批量数据传输场景。

**SPI DMA 配置**

配置 SPI DMA 传输需要正确设置 DMA 控制器和 SPI 外设：

```c
/**
 * SPI DMA 传输配置
 */
typedef struct {
    SPI_HandleTypeDef *hspi;        /* SPI 句柄 */
    DMA_HandleTypeDef *hdma_tx;     /* DMA 发送句柄 */
    DMA_HandleTypeDef *hdma_rx;     /* DMA 接收句柄 */
    uint8_t transfer_in_progress;   /* 传输进行中标志 */
} nor_dma_context_t;

/**
 * 初始化 SPI DMA
 */
int nor_dma_init(nor_dma_context_t *ctx, SPI_HandleTypeDef *hspi,
                 DMA_HandleTypeDef *hdma_tx, DMA_HandleTypeDef *hdma_rx)
{
    ctx->hspi = hspi;
    ctx->hdma_tx = hdma_tx;
    ctx->hdma_rx = hdma_rx;
    ctx->transfer_in_progress = 0;

    /* 启用 DMA 请求 */
    __HAL_SPI_ENABLE_DMA(hspi, SPI_DMA_REQ_TX);
    __HAL_SPI_ENABLE_DMA(hspi, SPI_DMA_REQ_RX);

    /* 设置 DMA 中断优先级 */
    HAL_NVIC_SetPriority(DMA_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA_IRQn);

    return 0;
}

/**
 * DMA 读取数据
 * 使用 DMA 从 Flash 读取大量数据
 */
int nor_dma_read(nor_device_t *dev, uint32_t addr,
                 uint8_t *buf, uint32_t len)
{
    nor_dma_context_t *dma_ctx = (nor_dma_context_t *)dev->priv;

    if (len == 0) {
        return 0;
    }

    /* 等待之前的操作完成 */
    int ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送读命令和地址 */
    nor_transport_cmd(dev->transport, dev->cmd.fast_read);
    nor_send_address(dev, addr);

    /* 启动 DMA 接收 */
    dma_ctx->transfer_in_progress = 1;
    HAL_StatusTypeDef status = HAL_SPI_Receive_DMA(dma_ctx->hspi, buf, len);

    if (status != HAL_OK) {
        dma_ctx->transfer_in_progress = 0;
        return -NOR_ERR_TRANSPORT;
    }

    return (int)len;
}

/**
 * DMA 写入数据
 * 使用 DMA 向 Flash 写入大量数据
 */
int nor_dma_write(nor_device_t *dev, uint32_t addr,
                  const uint8_t *buf, uint32_t len)
{
    nor_dma_context_t *dma_ctx = (nor_dma_context_t *)dev->priv;

    if (len == 0) {
        return 0;
    }

    /* 等待之前的操作完成 */
    int ret = nor_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret < 0) {
        return ret;
    }

    /* 发送页编程命令和地址 */
    nor_transport_cmd(dev->transport, dev->cmd.page_program);
    nor_send_address(dev, addr);

    /* 启动 DMA 发送 */
    dma_ctx->transfer_in_progress = 1;
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(dma_ctx->hspi, buf, len);

    if (status != HAL_OK) {
        dma_ctx->transfer_in_progress = 0;
        return -NOR_ERR_TRANSPORT;
    }

    return (int)len;
}

/**
 * DMA 传输完成回调
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    nor_dma_context_t *ctx = get_dma_context(hspi);
    ctx->transfer_in_progress = 0;

    /* 等待 Flash 写入完成 */
    nor_wait_ready(ctx->dev, NOR_TIMEOUT_PAGE_PROGRAM);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    nor_dma_context_t *ctx = get_dma_context(hspi);
    ctx->transfer_in_progress = 0;
}
```

### 2.2 双缓冲技术

双缓冲技术使用两个缓冲区交替工作，实现数据传输与处理的并行执行。

**读写双缓冲实现**

```c
/**
 * 双缓冲上下文
 */
typedef struct {
    nor_device_t *dev;

    /* 双缓冲区 */
    uint8_t *buffer_a;
    uint8_t *buffer_b;
    uint8_t *current_read_buffer;
    uint8_t *current_write_buffer;
    uint32_t buffer_size;

    /* 状态标志 */
    uint8_t read_buffer_busy;
    uint8_t write_buffer_busy;

    /* DMA 句柄 */
    nor_dma_context_t *dma_ctx;
} nor_double_buffer_t;

/**
 * 初始化双缓冲
 */
int nor_double_buffer_init(nor_double_buffer_t *ctx, nor_device_t *dev,
                           uint32_t buffer_size)
{
    ctx->dev = dev;
    ctx->buffer_size = buffer_size;

    /* 分配两个缓冲区 */
    ctx->buffer_a = (uint8_t *)malloc(buffer_size);
    ctx->buffer_b = (uint8_t *)malloc(buffer_size);
    if (!ctx->buffer_a || !ctx->buffer_b) {
        return -NOR_ERR_NO_MEMORY;
    }

    ctx->current_read_buffer = ctx->buffer_a;
    ctx->current_write_buffer = ctx->buffer_b;
    ctx->read_buffer_busy = 0;
    ctx->write_buffer_busy = 0;

    return 0;
}

/**
 * 双缓冲读取（异步）
 * 启动 DMA 读取到当前读缓冲区
 */
int nor_double_buffer_read_start(nor_double_buffer_t *ctx,
                                  uint32_t addr, uint32_t len)
{
    if (ctx->read_buffer_busy) {
        return -NOR_ERR_BUSY;  /* 上一次读取尚未完成 */
    }

    /* 切换到下一个读缓冲区 */
    if (ctx->current_read_buffer == ctx->buffer_a) {
        ctx->current_read_buffer = ctx->buffer_b;
    } else {
        ctx->current_read_buffer = ctx->buffer_a;
    }

    /* 启动 DMA 读取 */
    int ret = nor_dma_read(ctx->dev, addr, ctx->current_read_buffer, len);
    if (ret > 0) {
        ctx->read_buffer_busy = 1;
    }

    return ret;
}

/**
 * 双缓冲读取完成检测
 */
int nor_double_buffer_read_complete(nor_double_buffer_t *ctx,
                                     uint8_t **buf, uint32_t *len)
{
    if (!ctx->read_buffer_busy) {
        /* 读取已完成，返回当前读缓冲区 */
        *buf = ctx->current_read_buffer;
        /* 注意：需要根据实际传输长度设置 */
        *len = ctx->buffer_size;
        return 0;
    }

    /* 检查 DMA 是否完成 */
    /* 可以通过查询状态或等待中断 */
    return -NOR_ERR_BUSY;
}

/**
 * 双缓冲写入（异步）
 * 将数据写入当前写缓冲区，然后触发 DMA 传输
 */
int nor_double_buffer_write(nor_double_buffer_t *ctx,
                             uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (ctx->write_buffer_busy) {
        return -NOR_ERR_BUSY;
    }

    /* 复制数据到当前写缓冲区 */
    memcpy(ctx->current_write_buffer, data, len);

    /* 启动 DMA 写入 */
    int ret = nor_dma_write(ctx->dev, addr, ctx->current_write_buffer, len);
    if (ret > 0) {
        ctx->write_buffer_busy = 1;

        /* 切换到下一个写缓冲区 */
        if (ctx->current_write_buffer == ctx->buffer_a) {
            ctx->current_write_buffer = ctx->buffer_b;
        } else {
            ctx->current_write_buffer = ctx->buffer_a;
        }
    }

    return ret;
}
```

### 2.3 环形缓冲区应用

环形缓冲区（Ring Buffer）是一种高效的队列数据结构，适合处理连续数据流。

**DMA 与环形缓冲区结合**

```c
/**
 * DMA 环形缓冲区
 * 适用于连续数据采集和传输场景
 */
typedef struct {
    uint8_t *buffer;           /* 缓冲区基地址 */
    uint32_t buffer_size;       /* 缓冲区大小（必须为 2 的幂次） */
    uint32_t write_index;       /* 写索引 */
    uint32_t read_index;        /* 读索引 */

    /* DMA 半传输和传输完成回调 */
    void (*half_callback)(void *arg);
    void (*full_callback)(void *arg);
    void *callback_arg;
} nor_ring_buffer_t;

/**
 * 初始化环形缓冲区
 */
int nor_ring_buffer_init(nor_ring_buffer_t *ctx, uint32_t size)
{
    /* 大小必须为 2 的幂次 */
    if ((size & (size - 1)) != 0) {
        return -NOR_ERR_INVALID_PARAM;
    }

    ctx->buffer = (uint8_t *)malloc(size);
    if (!ctx->buffer) {
        return -NOR_ERR_NO_MEMORY;
    }

    ctx->buffer_size = size;
    ctx->write_index = 0;
    ctx->read_index = 0;

    return 0;
}

/**
 * 获取可读数据长度
 */
static inline uint32_t nor_ring_buffer_available(nor_ring_buffer_t *ctx)
{
    return (ctx->write_index - ctx->read_index) & (ctx->buffer_size - 1);
}

/**
 * 获取可写空间大小
 */
static inline uint32_t nor_ring_buffer_free_space(nor_ring_buffer_t *ctx)
{
    return ctx->buffer_size - nor_ring_buffer_available(ctx);
}

/**
 * DMA 半传输中断处理
 * 当 DMA 传输到达缓冲区一半时调用
 */
void nor_ring_buffer_half_callback(nor_ring_buffer_t *ctx)
{
    uint32_t avail = nor_ring_buffer_available(ctx);

    /* 通知应用处理前半部分数据 */
    if (ctx->half_callback && avail > ctx->buffer_size / 2) {
        ctx->half_callback(ctx->callback_arg);
    }
}

/**
 * DMA 传输完成中断处理
 * 当 DMA 传输完成时调用
 */
void nor_ring_buffer_full_callback(nor_ring_buffer_t *ctx)
{
    uint32_t avail = nor_ring_buffer_available(ctx);

    /* 通知应用处理后半部分数据 */
    if (ctx->full_callback && avail > 0) {
        ctx->full_callback(ctx->callback_arg);
    }
}
```

---

## 3. 缓存策略

合理的缓存策略可以显著减少 Flash 访问次数，降低功耗，提高系统响应速度。

### 3.1 读缓存实现

读缓存将频繁访问的数据保存在内存中，减少对 Flash 的读取次数。

**LRU 读缓存**

使用 LRU（最近最少使用）策略管理缓存空间：

```c
/**
 * LRU 缓存条目
 */
typedef struct {
    uint32_t flash_addr;        /* Flash 地址 */
    uint32_t size;              /* 数据大小 */
    uint8_t *data;              /* 缓存数据 */
    uint32_t access_count;      /* 访问计数 */
    uint32_t last_access_time;  /* 最后访问时间 */
    uint8_t valid;              /* 有效标志 */
} nor_cache_entry_t;

/**
 * LRU 读缓存
 */
typedef struct {
    nor_cache_entry_t *entries; /* 缓存条目数组 */
    uint32_t num_entries;       /* 缓存条目数量 */
    uint32_t total_cache_size;   /* 总缓存大小 */
    uint32_t hit_count;          /* 命中计数 */
    uint32_t miss_count;         /* 未命中计数 */
} nor_read_cache_t;

/**
 * 初始化读缓存
 */
int nor_read_cache_init(nor_read_cache_t *cache, uint32_t num_entries,
                        uint32_t total_size)
{
    cache->entries = (nor_cache_entry_t *)calloc(num_entries,
                                                  sizeof(nor_cache_entry_t));
    if (!cache->entries) {
        return -NOR_ERR_NO_MEMORY;
    }

    cache->num_entries = num_entries;
    cache->total_cache_size = total_size;
    cache->hit_count = 0;
    cache->miss_count = 0;

    return 0;
}

/**
 * 查找缓存条目
 */
nor_cache_entry_t* nor_cache_find(nor_read_cache_t *cache,
                                   uint32_t flash_addr, uint32_t size)
{
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        nor_cache_entry_t *entry = &cache->entries[i];
        if (entry->valid &&
            entry->flash_addr == flash_addr &&
            entry->size >= size) {
            return entry;
        }
    }
    return NULL;
}

/**
 * 查找最少使用的缓存条目
 */
nor_cache_entry_t* nor_cache_find_lru(nor_read_cache_t *cache)
{
    nor_cache_entry_t *lru_entry = NULL;
    uint32_t min_access_time = UINT32_MAX;

    for (uint32_t i = 0; i < cache->num_entries; i++) {
        nor_cache_entry_t *entry = &cache->entries[i];
        if (!entry->valid || entry->last_access_time < min_access_time) {
            min_access_time = entry->last_access_time;
            lru_entry = entry;
        }
    }

    return lru_entry;
}

/**
 * 带缓存的读取函数
 */
int nor_read_cached(nor_device_t *dev, nor_read_cache_t *cache,
                    uint32_t addr, uint8_t *buf, uint32_t len)
{
    nor_cache_entry_t *entry;
    int ret;

    /* 查找缓存 */
    entry = nor_cache_find(cache, addr, len);

    if (entry) {
        /* 缓存命中 */
        cache->hit_count++;
        entry->access_count++;
        entry->last_access_time = nor_get_tick();

        /* 复制缓存数据 */
        memcpy(buf, entry->data, len);
        return (int)len;
    }

    /* 缓存未命中 */
    cache->miss_count++;

    /* 从 Flash 读取数据 */
    ret = nor_read(dev, addr, buf, len);
    if (ret < 0) {
        return ret;
    }

    /* 将数据存入缓存 */
    entry = nor_cache_find_lru(cache);
    if (entry->valid) {
        free(entry->data);
    }

    entry->flash_addr = addr;
    entry->size = len;
    entry->data = (uint8_t *)malloc(len);
    if (entry->data) {
        memcpy(entry->data, buf, len);
        entry->valid = 1;
    }
    entry->access_count = 1;
    entry->last_access_time = nor_get_tick();

    return ret;
}

/**
 * 获取缓存命中率
 */
float nor_cache_get_hit_rate(nor_read_cache_t *cache)
{
    uint32_t total = cache->hit_count + cache->miss_count;
    if (total == 0) {
        return 0.0f;
    }
    return (float)cache->hit_count / total;
}
```

### 3.2 写缓存与写合并

写缓存可以合并多次小规模写入，减少 Flash 编程次数，延长 Flash 寿命。

**延迟写合并**

```c
/**
 * 写缓存上下文
 */
typedef struct {
    nor_device_t *dev;

    /* 写缓冲条目 */
    typedef struct {
        uint32_t flash_addr;
        uint32_t size;
        uint8_t *data;
        uint8_t dirty;        /* 数据已修改标志 */
        uint8_t valid;        /* 条目有效标志 */
    } nor_write_entry_t;

    nor_write_entry_t *entries;
    uint32_t num_entries;

    /* 合并相关 */
    uint32_t merge_threshold;  /* 触发自动合并的阈值 */
} nor_write_cache_t;

/**
 * 延迟写操作
 * 将数据写入缓存，不立即写入 Flash
 */
int nor_write_delayed(nor_write_cache_t *cache, uint32_t addr,
                      const uint8_t *data, uint32_t len)
{
    /* 查找是否有重叠的缓存条目 */
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        nor_write_entry_t *entry = &cache->entries[i];

        if (entry->valid) {
            /* 检查地址范围是否重叠 */
            uint32_t entry_end = entry->flash_addr + entry->size;
            uint32_t new_end = addr + len;

            if (!(entry_end <= addr || entry->flash_addr >= new_end)) {
                /* 存在重叠，合并数据 */
                /* 扩展缓冲区以容纳新数据 */
                uint32_t min_addr = MIN(entry->flash_addr, addr);
                uint32_t max_addr = MAX(entry_end, new_end);
                uint32_t new_size = max_addr - min_addr;

                uint8_t *new_buf = (uint8_t *)realloc(entry->data, new_size);
                if (!new_buf) {
                    return -NOR_ERR_NO_MEMORY;
                }

                /* 复制旧数据到新缓冲区 */
                memcpy(new_buf, entry->data, entry->size);

                /* 复制新数据到正确位置 */
                memcpy(new_buf + (addr - min_addr), data, len);

                entry->data = new_buf;
                entry->flash_addr = min_addr;
                entry->size = new_size;
                entry->dirty = 1;

                return 0;
            }
        }
    }

    /* 没有重叠，创建新条目 */
    nor_write_entry_t *entry = NULL;
    for (uint32_t i = 0; i < cache->num_entries; i++) {
        if (!cache->entries[i].valid) {
            entry = &cache->entries[i];
            break;
        }
    }

    if (!entry) {
        /* 没有空闲条目，尝试驱逐最旧的条目 */
        entry = nor_find_oldest_entry(cache);
        if (entry) {
            /* 先将脏数据写入 Flash */
            if (entry->dirty) {
                nor_write(entry->dev, entry->flash_addr,
                          entry->data, entry->size);
            }
            free(entry->data);
        } else {
            return -NOR_ERR_NO_MEMORY;
        }
    }

    /* 创建新条目 */
    entry->data = (uint8_t *)malloc(len);
    if (!entry->data) {
        return -NOR_ERR_NO_MEMORY;
    }

    memcpy(entry->data, data, len);
    entry->flash_addr = addr;
    entry->size = len;
    entry->dirty = 1;
    entry->valid = 1;

    return 0;
}

/**
 * 刷新写缓存
 * 将所有缓存数据写入 Flash
 */
int nor_write_cache_flush(nor_write_cache_t *cache)
{
    int ret;

    for (uint32_t i = 0; i < cache->num_entries; i++) {
        nor_write_entry_t *entry = &cache->entries[i];

        if (entry->valid && entry->dirty) {
            /* 写入 Flash */
            ret = nor_write(cache->dev, entry->flash_addr,
                            entry->data, entry->size);
            if (ret < 0) {
                return ret;
            }

            entry->dirty = 0;
        }
    }

    return 0;
}
```

### 3.3 缓存失效策略

缓存需要在适当的时候失效，以确保数据一致性。

**缓存失效管理**

```c
/**
 * 缓存失效类型
 */
typedef enum {
    NOR_CACHE_INVALIDATE_ALL,      /* 失效所有缓存 */
    NOR_CACHE_INVALIDATE_RANGE,    /* 失效指定范围 */
    NOR_CACHE_INVALIDATE_ENTRY,    /* 失效指定条目 */
} nor_cache_invalidate_type_t;

/**
 * 失效指定范围的缓存
 */
int nor_cache_invalidate_range(nor_read_cache_t *cache,
                                 uint32_t addr, uint32_t len)
{
    uint32_t end_addr = addr + len;

    for (uint32_t i = 0; i < cache->num_entries; i++) {
        nor_cache_entry_t *entry = &cache->entries[i];
        if (!entry->valid) {
            continue;
        }

        uint32_t entry_end = entry->flash_addr + entry->size;

        /* 检查是否与失效范围重叠 */
        if (!(entry_end <= addr || entry->flash_addr >= end_addr)) {
            /* 失效此条目 */
            entry->valid = 0;
            free(entry->data);
            entry->data = NULL;
        }
    }

    return 0;
}

/**
 * 擦除前失效相关缓存
 */
int nor_cache_invalidate_before_erase(nor_read_cache_t *cache,
                                        uint32_t addr, uint32_t len)
{
    /* 擦除操作会使整个区域数据变为 0xFF，必须失效所有相关缓存 */
    return nor_cache_invalidate_range(cache, addr, len);
}

/**
 * 写入前失效相关缓存
 */
int nor_cache_invalidate_before_write(nor_read_cache_t *cache,
                                        uint32_t addr, uint32_t len)
{
    /* 写入操作会修改数据，必须失效所有相关缓存 */
    return nor_cache_invalidate_range(cache, addr, len);
}
```

---

## 4. 吞吐量测试

性能优化需要量化评估，吞吐量测试是衡量优化效果的重要手段。

### 4.1 测试方法与指标

吞吐量测试需要设计合理的测试方法，选择合适的性能指标。

**关键性能指标**

Nor Flash 性能测试通常关注以下指标：顺序读取吞吐量（Sequential Read Throughput），指连续读取大量数据时的传输速率；顺序写入吞吐量（Sequential Write Throughput），指连续写入大量数据时的传输速率；随机访问延迟（Random Access Latency），指单次随机读写操作的响应时间；擦除吞吐量，指擦除操作的数据处理速率。

```c
/**
 * 性能测试结果
 */
typedef struct {
    /* 读取性能 */
    uint32_t read_throughput_bps;     /* 读取吞吐量（字节/秒） */
    uint32_t read_latency_us;         /* 读取延迟（微秒） */
    uint32_t read_latency_max_us;     /* 最大读取延迟 */

    /* 写入性能 */
    uint32_t write_throughput_bps;    /* 写入吞吐量 */
    uint32_t write_latency_us;        /* 写入延迟 */
    uint32_t write_latency_max_us;    /* 最大写入延迟 */

    /* 擦除性能 */
    uint32_t erase_throughput_bps;    /* 擦除吞吐量 */

    /* 测试参数 */
    uint32_t test_size;               /* 测试数据大小 */
    uint32_t test_duration_ms;        /* 测试持续时间 */
} nor_perf_result_t;
```

### 4.2 吞吐量测试实现

实现完整的吞吐量测试功能：

```c
/**
 * 顺序读取吞吐量测试
 */
int nor_test_read_throughput(nor_device_t *dev, uint32_t test_size,
                              nor_perf_result_t *result)
{
    uint8_t *buffer;
    uint32_t iterations = 10;
    uint64_t total_bytes = 0;
    uint64_t total_time_us = 0;
    int ret;

    /* 分配测试缓冲区 */
    buffer = (uint8_t *)malloc(test_size);
    if (!buffer) {
        return -NOR_ERR_NO_MEMORY;
    }

    /* 预热：先读取一次 */
    ret = nor_read(dev, 0, buffer, test_size);
    if (ret < 0) {
        free(buffer);
        return ret;
    }

    /* 多次测试取平均值 */
    for (uint32_t i = 0; i < iterations; i++) {
        uint32_t start_time = nor_get_tick_us();

        ret = nor_read(dev, 0, buffer, test_size);
        if (ret < 0) {
            free(buffer);
            return ret;
        }

        uint32_t elapsed_us = nor_get_tick_us() - start_time;

        total_bytes += test_size;
        total_time_us += elapsed_us;
    }

    /* 计算平均吞吐量 */
    uint64_t avg_time_us = total_time_us / iterations;
    result->read_throughput_bps = (uint32_t)((uint64_t)test_size * 1000000 / avg_time_us);
    result->read_latency_us = (uint32_t)(avg_time_us);
    result->test_size = test_size;

    free(buffer);
    return 0;
}

/**
 * 顺序写入吞吐量测试
 */
int nor_test_write_throughput(nor_device_t *dev, uint32_t test_size,
                               nor_perf_result_t *result)
{
    uint8_t *buffer;
    uint8_t *verify_buffer;
    uint32_t iterations = 5;
    uint64_t total_bytes = 0;
    uint64_t total_time_us = 0;
    int ret;

    /* 分配测试缓冲区 */
    buffer = (uint8_t *)malloc(test_size);
    verify_buffer = (uint8_t *)malloc(test_size);
    if (!buffer || !verify_buffer) {
        free(buffer);
        free(verify_buffer);
        return -NOR_ERR_NO_MEMORY;
    }

    /* 填充测试数据 */
    for (uint32_t i = 0; i < test_size; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }

    /* 测试多次 */
    for (uint32_t i = 0; i < iterations; i++) {
        uint32_t addr = i * test_size;

        /* 擦除目标区域 */
        ret = nor_erase(dev, addr, test_size);
        if (ret < 0) {
            free(buffer);
            free(verify_buffer);
            return ret;
        }

        /* 写入测试 */
        uint32_t start_time = nor_get_tick_us();

        ret = nor_write(dev, addr, buffer, test_size);
        if (ret < 0) {
            free(buffer);
            free(verify_buffer);
            return ret;
        }

        uint32_t elapsed_us = nor_get_tick_us() - start_time;

        total_bytes += test_size;
        total_time_us += elapsed_us;
    }

    /* 计算平均吞吐量 */
    uint64_t avg_time_us = total_time_us / iterations;
    result->write_throughput_bps = (uint32_t)((uint64_t)test_size * 1000000 / avg_time_us);
    result->write_latency_us = (uint32_t)(avg_time_us);
    result->test_size = test_size;

    free(buffer);
    free(verify_buffer);
    return 0;
}

/**
 * 擦除性能测试
 */
int nor_test_erase_throughput(nor_device_t *dev, uint32_t test_size,
                               nor_perf_result_t *result)
{
    uint32_t iterations = 5;
    uint64_t total_bytes = 0;
    uint64_t total_time_us = 0;
    int ret;

    /* 测试多次 */
    for (uint32_t i = 0; i < iterations; i++) {
        uint32_t addr = i * test_size;

        /* 擦除测试 */
        uint32_t start_time = nor_get_tick_us();

        ret = nor_erase(dev, addr, test_size);
        if (ret < 0) {
            return ret;
        }

        uint32_t elapsed_us = nor_get_tick_us() - start_time;

        total_bytes += test_size;
        total_time_us += elapsed_us;
    }

    /* 计算平均吞吐量 */
    uint64_t avg_time_us = total_time_us / iterations;
    result->erase_throughput_bps = (uint32_t)((uint64_t)test_size * 1000000 / avg_time_us);
    result->test_size = test_size;

    return 0;
}

/**
 * 综合性能测试
 */
int nor_test_performance(nor_device_t *dev, nor_perf_result_t *result)
{
    int ret;

    printf("=== Nor Flash Performance Test ===\n\n");

    /* 读取吞吐量测试 */
    printf("Testing read throughput...\n");
    ret = nor_test_read_throughput(dev, 4096, result);
    if (ret == 0) {
        printf("  4KB read: %lu KB/s\n",
               result->read_throughput_bps / 1024);
    }

    ret = nor_test_read_throughput(dev, 65536, result);
    if (ret == 0) {
        printf("  64KB read: %lu KB/s\n",
               result->read_throughput_bps / 1024);
    }

    /* 写入吞吐量测试 */
    printf("\nTesting write throughput...\n");
    ret = nor_test_write_throughput(dev, 4096, result);
    if (ret == 0) {
        printf("  4KB write: %lu KB/s\n",
               result->write_throughput_bps / 1024);
    }

    /* 擦除吞吐量测试 */
    printf("\nTesting erase throughput...\n");
    ret = nor_test_erase_throughput(dev, 65536, result);
    if (ret == 0) {
        printf("  64KB erase: %lu KB/s\n",
               result->erase_throughput_bps / 1024);
    }

    return 0;
}
```

### 4.3 性能优化效果评估

对比优化前后的性能数据，评估优化效果：

```c
/**
 * 性能对比报告
 */
typedef struct {
    nor_perf_result_t before;   /* 优化前 */
    nor_perf_result_t after;    /* 优化后 */

    /* 性能提升 */
    float read_improvement;      /* 读取提升比例 */
    float write_improvement;     /* 写入提升比例 */
    float erase_improvement;     /* 擦除提升比例 */
} nor_perf_comparison_t;

/**
 * 生成性能对比报告
 */
void nor_perf_compare(nor_perf_comparison_t *comp)
{
    /* 计算性能提升 */
    if (comp->before.read_throughput_bps > 0) {
        comp->read_improvement =
            ((float)comp->after.read_throughput_bps /
             comp->before.read_throughput_bps - 1.0f) * 100.0f;
    }

    if (comp->before.write_throughput_bps > 0) {
        comp->write_improvement =
            ((float)comp->after.write_throughput_bps /
             comp->before.write_throughput_bps - 1.0f) * 100.0f;
    }

    if (comp->before.erase_throughput_bps > 0) {
        comp->erase_improvement =
            ((float)comp->after.erase_throughput_bps /
             comp->before.erase_throughput_bps - 1.0f) * 100.0f;
    }

    /* 打印报告 */
    printf("=== Performance Comparison ===\n");
    printf("Read:   %.1f%% improvement (%lu -> %lu KB/s)\n",
           comp->read_improvement,
           comp->before.read_throughput_bps / 1024,
           comp->after.read_throughput_bps / 1024);
    printf("Write:  %.1f%% improvement (%lu -> %lu KB/s)\n",
           comp->write_improvement,
           comp->before.write_throughput_bps / 1024,
           comp->after.write_throughput_bps / 1024);
    printf("Erase:  %.1f%% improvement (%lu -> %lu KB/s)\n",
           comp->erase_improvement,
           comp->before.erase_throughput_bps / 1024,
           comp->after.erase_throughput_bps / 1024);
}
```

---

## 本章小结

本章详细介绍了 Nor Flash 性能调优的各项技术，主要内容包括：

1. **批量操作优化**
   - 连续读写优化：页边界处理、批量写入策略
   - 擦除批量优化：动态擦除粒度选择
   - 命令队列与流水线：提高命令处理效率

2. **DMA 与双缓冲应用**
   - DMA 传输配置：SPI DMA 配置与使用
   - 双缓冲技术：读写双缓冲实现
   - 环形缓冲区：DMA 与环形缓冲区结合

3. **缓存策略**
   - 读缓存实现：LRU 缓存管理
   - 写缓存与写合并：延迟写合并策略
   - 缓存失效策略：确保数据一致性

4. **吞吐量测试**
   - 测试方法与指标：关键性能指标定义
   - 吞吐量测试实现：读写擦除性能测试
   - 效果评估：性能对比报告生成

通过合理的性能调优，可以显著提升 Nor Flash 的数据吞吐能力，降低 CPU 占用，延长 Flash 使用寿命，从而提升整个系统的性能和可靠性。

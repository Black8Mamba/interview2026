# OTA 升级实现

OTA（Over-The-Air）升级是嵌入式设备通过无线或有线网络接收并安装固件更新的技术。对于部署在现场的嵌入式设备，OTA 升级是实现功能迭代、漏洞修复和安全更新的关键手段。本章将详细介绍基于 Nor Flash 的 OTA 升级系统设计，涵盖升级流程设计、镜像校验、断点续传和版本回退等核心技术。

---

## 升级流程设计

### OTA 升级系统架构

典型的 OTA 升级系统由以下组件构成：

**服务端组件**

1. **固件仓库**：存储各版本的固件文件
2. **版本服务器**：提供版本查询、固件分发接口
3. **升级管理平台**：管理设备分组、升级策略、升级进度

**设备端组件**

1. **升级客户端**：负责与服务器通信，下载固件
2. **固件管理器**：管理 Flash 中的固件镜像
3. **启动加载器**：负责固件切换和回退

### 升级流程状态机

OTA 升级流程可以分解为以下状态：

```c
typedef enum {
    OTA_STATE_IDLE,              // 空闲状态
    OTA_STATE_CHECK_VERSION,    // 检查版本
    OTA_STATE_DOWNLOADING,      // 下载中
    OTA_STATE_DOWNLOADED,       // 下载完成
    OTA_STATE_VERIFYING,        // 验证中
    OTA_STATE_UPGRADING,        // 升级中
    OTA_STATE_REBOOTING,        // 重启中
    OTA_STATE_COMPLETE,         // 升级完成
    OTA_STATE_FAILED,           // 升级失败
    OTA_STATE_ROLLBACK          // 回退中
} ota_state_t;

// OTA 上下文结构
typedef struct {
    ota_state_t    state;             // 当前状态
    uint32_t      server_version;    // 服务器版本号
    uint32_t      local_version;     // 本地版本号
    uint32_t      download_size;     // 已下载大小
    uint32_t      total_size;        // 总大小
    uint32_t      download_offset;   // 下载偏移（断点续传）
    flash_addr_t   target_addr;       // 目标地址
    uint8_t       retry_count;       // 重试次数
    int32_t       last_error;        // 最后错误码
    char           version_info[64]; // 版本信息字符串
} ota_context_t;
```

### 完整升级流程

**第一步：版本检查**

设备定期或按需向服务器查询最新固件版本：

```c
// 版本检查请求
typedef struct {
    char    device_id[32];       // 设备标识
    char    hardware_version[16]; // 硬件版本
    char    firmware_version[16]; // 当前固件版本
    char    chip_id[16];         // 芯片标识
} version_check_request_t;

// 版本检查响应
typedef struct {
    uint8_t  has_update;         // 是否有新版本
    uint8_t  force_update;       // 是否强制升级
    uint32_t latest_version;    // 最新版本号
    uint32_t file_size;         // 固件文件大小
    uint32_t file_crc32;        // 固件 CRC32
    char     download_url[256];  // 下载 URL
    char     release_note[512];  // 发布说明
    char     signature[256];     // 固件签名
} version_check_response_t;

int ota_check_version(ota_context_t *ctx, version_check_response_t *resp)
{
    // 构建版本检查请求
    version_check_request_t req = {0};
    get_device_id(req.device_id);
    get_hardware_version(req.hardware_version);
    get_firmware_version(req.firmware_version);
    get_chip_id(req.chip_id);

    // 发送 HTTP/HTTPS 请求
    int ret = http_post("/api/version/check", &req, sizeof(req), resp);
    if (ret != 0) {
        return ret;
    }

    // 更新上下文
    ctx->server_version = resp->latest_version;
    ctx->total_size = resp->file_size;

    // 判断是否需要升级
    if (resp->has_update && resp->latest_version > ctx->local_version) {
        ctx->state = OTA_STATE_DOWNLOADING;
        return 0;  // 需要升级
    }

    return 1;  // 无需升级
}
```

**第二步：固件下载**

设备从服务器下载固件到 Flash：

```c
// 分段下载固件
int ota_download_firmware(ota_context_t *ctx, const char *url)
{
    int ret;
    uint8_t buffer[4096];
    flash_addr_t write_addr = UPGRADE_BUFFER_ADDR;

    // 支持断点续传：从上次位置继续下载
    uint32_t offset = ctx->download_offset;

    while (offset < ctx->total_size) {
        // 计算本次下载大小
        uint32_t chunk_size = MIN(sizeof(buffer), ctx->total_size - offset);

        // 下载数据块
        ret = http_range_get(url, offset, chunk_size, buffer);
        if (ret != 0) {
            // 下载失败，记录断点
            ctx->download_offset = offset;
            ctx->last_error = ret;
            return ret;
        }

        // 写入 Flash
        nor_flash_erase(write_addr, chunk_size);
        nor_flash_write(write_addr, buffer, chunk_size);

        write_addr += chunk_size;
        offset += chunk_size;
        ctx->download_offset = offset;

        // 更新下载进度
        if (ctx->progress_callback) {
            ctx->progress_callback(offset, ctx->total_size);
        }

        // 检查暂停/取消标志
        if (ctx->abort_flag) {
            return -ECANCELED;
        }
    }

    ctx->state = OTA_STATE_DOWNLOADED;
    return 0;
}
```

**第三步：固件验证**

下载完成后，验证固件的完整性和合法性：

```c
int ota_verify_firmware(ota_context_t *ctx, flash_addr_t addr, uint32_t size)
{
    // 1. CRC32 校验
    uint32_t crc = crc32_compute_at(addr, size);
    if (crc != ctx->expected_crc32) {
        return -E_CRC_MISMATCH;
    }

    // 2. 固件头部验证
    firmware_header_t header;
    nor_flash_read(addr, &header, sizeof(header));

    if (header.magic != FIRMWARE_MAGIC) {
        return -E_INVALID_HEADER;
    }

    // 3. 版本号验证
    if (header.version <= ctx->local_version) {
        return -E_VERSION_TOO_OLD;
    }

    // 4. 硬件兼容性验证
    if (header.hardware_version != ctx->hardware_version) {
        return -E_HW_INCOMPATIBLE;
    }

    // 5. 签名验证（如有）
    if (ctx->signature_required) {
        int ret = rsa_verify_firmware(addr, size, &header.signature);
        if (ret != 0) {
            return -E_SIGNATURE_INVALID;
        }
    }

    ctx->state = OTA_STATE_VERIFYING;
    return 0;
}
```

**第四步：固件安装**

验证通过后，执行固件安装：

```c
int ota_install_firmware(ota_context_t *ctx, flash_addr_t src_addr)
{
    flash_addr_t dst_addr = IMAGE_B_ADDR;  // 写入备份分区

    // 1. 擦除目标分区
    uint32_t size = ctx->total_size;
    nor_flash_erase(dst_addr, size);

    // 2. 复制固件到目标分区
    uint8_t buffer[4096];
    uint32_t offset = 0;

    while (offset < size) {
        uint32_t chunk = MIN(sizeof(buffer), size - offset);

        nor_flash_read(src_addr + offset, buffer, chunk);
        nor_flash_write(dst_addr + offset, buffer, chunk);

        offset += chunk;
    }

    // 3. 再次校验
    uint32_t crc = crc32_compute_at(dst_addr, size);
    if (crc != ctx->expected_crc32) {
        return -E_VERIFY_FAIL;
    }

    // 4. 更新固件信息
    update_image_info(IMAGE_B_INDEX, dst_addr, size, ctx->server_version);

    // 5. 设置升级成功标志
    set_boot_flag(BOOT_FLAG_UPGRADE_SUCCESS);

    ctx->state = OTA_STATE_UPGRADING;
    return 0;
}
```

**第五步：系统重启**

安装完成后，系统重启并切换到新固件：

```c
void ota_reboot_to_new_firmware(ota_context_t *ctx)
{
    // 保存升级上下文到持久存储
    save_ota_context(ctx);

    // 同步文件系统
    sync_all();

    // 设置重启标志
    set_reboot_flag(REBOOT_TO_NEW_FIRMWARE);

    // 执行软重启
    system_reset();
}
```

---

## 镜像校验

固件镜像校验是 OTA 升级安全性的关键保障，需要验证固件的完整性、合法性和兼容性。

### 多层校验机制

**第一层：完整性校验**

使用 CRC32 或 SHA-256 验证固件数据在传输和存储过程中没有损坏：

```c
// CRC32 校验实现
uint32_t crc32_compute(const void *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)data;

    for (size_t i = 0; i < length; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

// SHA-256 校验实现（适用于高安全场景）
void sha256_compute(const void *data, size_t length, uint8_t *hash)
{
    sha256_context_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, length);
    sha256_final(&ctx, hash);
}
```

**第二层：身份校验**

验证固件来源的合法性，防止恶意固件刷入：

```c
// 固件签名验证（RSA 算法）
int rsa_verify_firmware(flash_addr_t addr, uint32_t size,
                        const firmware_signature_t *sig)
{
    // 1. 计算固件摘要
    uint8_t digest[32];
    sha256_compute_at(addr, size, digest);

    // 2. 获取公钥（预置在设备中）
    rsa_public_key_t pubkey;
    load_factory_public_key(&pubkey);

    // 3. 验证签名
    return rsa_verify(&pubkey, digest, sig->rsa_signature);
}

// AES 加密固件解密
int decrypt_firmware(flash_addr_t src, flash_addr_t dst, uint32_t size)
{
    aes_context_t ctx;
    uint8_t iv[16];

    // 读取加密参数
    nor_flash_read(src + sizeof(firmware_header_t), iv, sizeof(iv));

    // 初始化 AES 解密
    aes_setkey_dec(&ctx, session_key, 256);
    aes_crypt_cbc(&ctx, AES_DECRYPT, size, iv,
                  (uint8_t *)(src + sizeof(firmware_header_t) + 16),
                  (uint8_t *)dst);

    return 0;
}
```

**第三层：兼容性校验**

确保固件与设备硬件和软件环境兼容：

```c
typedef struct {
    uint16_t hardware_id;        // 硬件型号 ID
    uint16_t hardware_version;   // 硬件版本
    uint32_t min_bootloader_version; // 最小启动加载器版本
    uint32_t max_flash_size;     // 最大 Flash 需求
    uint32_t min_ram_size;       // 最小 RAM 需求
    char     supported_chips[64]; // 支持的芯片列表
} firmware_compatibility_t;

int check_hardware_compatibility(const firmware_header_t *header)
{
    // 检查硬件型号
    if (header->hardware_id != get_hardware_id()) {
        return -E_HW_INCOMPATIBLE;
    }

    // 检查硬件版本
    uint16_t hw_ver = get_hardware_version();
    if (hw_ver < header->min_hardware_version) {
        return -E_HW_VERSION_TOO_OLD;
    }

    // 检查启动加载器版本
    uint32_t bl_ver = get_bootloader_version();
    if (bl_ver < header->min_bl_version) {
        return -E_BL_VERSION_TOO_OLD;
    }

    // 检查 Flash 大小需求
    if (get_flash_size() < header->min_flash_size) {
        return -E_FLASH_TOO_SMALL;
    }

    // 检查 RAM 大小需求
    if (get_ram_size() < header->min_ram_size) {
        return -E_RAM_TOO_SMALL;
    }

    return 0;
}
```

### 固件头部设计

固件头部包含元数据信息，用于校验和识别固件：

```c
typedef struct {
    uint32_t magic;                   // 固件标识 (0x46524D00 'FRM\0')
    uint16_t header_version;          // 头部版本
    uint16_t flags;                   // 标志位
    uint32_t version;                 // 固件版本号
    uint32_t size;                    // 固件大小（不含头部）
    uint32_t crc32;                  // 固件 CRC32
    uint32_t timestamp;               // 构建时间
    uint16_t hardware_id;             // 目标硬件 ID
    uint16_t hardware_version;        // 目标硬件版本
    uint32_t min_bl_version;         // 最小启动加载器版本
    uint32_t min_flash_size;         // 最小 Flash 需求
    uint32_t min_ram_size;           // 最小 RAM 需求
    uint8_t  signature_algorithm;    // 签名算法
    uint8_t  reserved1;
    uint16_t header_crc16;           // 头部 CRC16
    uint32_t signature[32];           // RSA 签名（512 字节）
    char     build_info[64];         // 构建信息
    uint8_t  reserved2[128];
} __attribute__((packed)) firmware_header_t;

#define FIRMWARE_MAGIC        0x46524D00

// 头部标志位
#define FLAG_ENCRYPTED        0x01    // 固件已加密
#define FLAG_COMPRESSED      0x02    // 固件已压缩
#define FLAG_FORCE_UPDATE    0x04    // 强制升级
#define FLAG_DELTA_UPDATE    0x08    // 增量升级包
```

---

## 断点续传

网络不稳定或升级过程中断时，断点续传功能可以避免重新下载整个固件，显著提升升级效率和成功率。

### 断点续传原理

HTTP 协议支持 Range 请求头，允许客户端请求文件的指定字节范围。服务器响应 206 Partial Content，并返回请求的数据范围。

### 断点续传实现

```c
// HTTP Range 请求
typedef struct {
    uint32_t offset;       // 请求起始偏移
    uint32_t length;       // 请求长度
    uint32_t total_size;   // 文件总大小
    bool    has_range;     // 是否支持 Range
} http_range_t;

// 带断点续传的下载函数
int http_download_with_resume(const char *url, flash_addr_t flash_addr,
                              http_range_t *range, uint8_t *buffer, size_t buf_size)
{
    char header[256];
    int ret;

    // 构建 Range 请求头
    if (range->offset > 0) {
        snprintf(header, sizeof(header),
                 "Range: bytes=%lu-%lu\r\n",
                 range->offset, range->offset + range->length - 1);
    }

    // 发送请求
    ret = http_get_with_header(url, header, buffer, buf_size,
                               &range->total_size);

    if (ret == HTTP_STATUS_PARTIAL_CONTENT) {
        // 服务器支持断点续传
        range->has_range = true;
    } else if (ret == HTTP_STATUS_OK) {
        // 服务器不支持 Range，从头开始下载
        range->offset = 0;
        range->has_range = false;
    } else {
        return ret;
    }

    return 0;
}

// 完整的断点续传下载
int ota_download_with_resume(ota_context_t *ctx)
{
    int ret;
    uint8_t buffer[4096];
    flash_addr_t write_addr = UPGRADE_BUFFER_ADDR + ctx->download_offset;
    http_range_t range = {
        .offset = ctx->download_offset,
        .length = sizeof(buffer),
        .total_size = ctx->total_size,
        .has_range = false
    };

    // 如果之前有下载记录，先验证已下载部分
    if (ctx->download_offset > 0) {
        ret = verify_partial_download(ctx);
        if (ret != 0) {
            // 校验失败，需要重新下载
            ctx->download_offset = 0;
            write_addr = UPGRADE_BUFFER_ADDR;
            range.offset = 0;
        }
    }

    // 继续下载
    while (ctx->download_offset < ctx->total_size) {
        // 调整请求大小
        range.length = MIN(sizeof(buffer), ctx->total_size - ctx->download_offset);

        ret = http_download_with_resume(ctx->download_url, write_addr,
                                        &range, buffer, sizeof(buffer));

        if (ret != 0 && ret != HTTP_STATUS_PARTIAL_CONTENT) {
            // 网络错误，保存断点并退出
            save_download_checkpoint(ctx);
            return ret;
        }

        // 写入 Flash
        if (ret == HTTP_STATUS_PARTIAL_CONTENT) {
            nor_flash_write(write_addr, buffer, range.length);
        }

        write_addr += range.length;
        ctx->download_offset += range.length;
        range.offset += range.length;

        // 定期保存断点
        if (ctx->download_offset % CHECKPOINT_INTERVAL == 0) {
            save_download_checkpoint(ctx);
        }
    }

    return 0;
}

// 保存下载断点
void save_download_checkpoint(const ota_context_t *ctx)
{
    checkpoint_t cp = {
        .download_offset = ctx->download_offset,
        .total_size = ctx->total_size,
        .crc32_partial = crc32_compute_at(UPGRADE_BUFFER_ADDR, ctx->download_offset),
        .timestamp = get_timestamp()
    };

    // 保存到 Flash 专用区域
    nor_flash_erase(CHECKPOINT_ADDR, sizeof(checkpoint_t));
    nor_flash_write(CHECKPOINT_ADDR, &cp, sizeof(checkpoint_t));
}

// 恢复下载
int restore_download_checkpoint(ota_context_t *ctx)
{
    checkpoint_t cp;
    nor_flash_read(CHECKPOINT_ADDR, &cp, sizeof(checkpoint_t));

    // 验证断点有效性
    if (cp.download_offset > 0 && cp.download_offset < ctx->total_size) {
        uint32_t crc = crc32_compute_at(UPGRADE_BUFFER_ADDR, cp.download_offset);
        if (crc == cp.crc32_partial) {
            // 断点有效，恢复下载
            ctx->download_offset = cp.download_offset;
            return 0;
        }
    }

    return -1;  // 无有效断点
}
```

---

## 版本回退

版本回退机制确保当新固件存在严重问题时，系统能够自动或手动回退到之前的稳定版本。

### 回退触发条件

**自动回退**

1. **启动失败计数**：新固件连续启动失败达到阈值
2. **看门狗超时**：新固件运行异常导致看门狗复位
3. **心跳超时**：应用层心跳停止

**手动回退**

1. **用户触发**：通过物理按钮或通信接口触发
2. **远程触发**：服务器下发回退命令

### 回退实现机制

```c
// 回退标志定义
#define BOOT_FLAG_NORMAL         0x00    // 正常启动
#define BOOT_FLAG_UPGRADE       0x01    // 升级后首次启动
#define BOOT_FLAG_ROLLBACK      0x02    // 需要回退

// 升级上下文保存
typedef struct {
    uint32_t from_version;       // 升级前版本
    uint32_t to_version;         // 升级后版本
    uint32_t upgrade_time;       // 升级时间
    uint32_t test_period;        // 测试周期（秒）
    uint32_t boot_count;         // 启动计数
    uint32_t failed_count;       // 失败计数
    uint8_t  status;             // 状态
} upgrade_record_t;

// 启动时检查是否需要回退
int check_rollback_needed(void)
{
    upgrade_record_t record;
    load_upgrade_record(&record);

    // 检查是否在测试期内
    uint32_t elapsed = get_timestamp() - record.upgrade_time;
    if (elapsed > record.test_period) {
        // 测试期已过，确认升级成功
        confirm_upgrade_success();
        return 0;  // 不需要回退
    }

    // 检查失败计数
    if (record.failed_count >= MAX_BOOT_FAILURES) {
        return -E_ROLLBACK_REQUIRED;
    }

    // 检查看门狗复位标志
    if (check_rst_flag(RST_FLAG_WDOG)) {
        record.failed_count++;
        save_upgrade_record(&record);

        if (record.failed_count >= MAX_BOOT_FAILURES) {
            return -E_ROLLBACK_REQUIRED;
        }
    }

    return 0;
}

// 执行回退操作
int perform_rollback(void)
{
    upgrade_record_t record;
    load_upgrade_record(&record);

    // 确定回退目标
    flash_addr_t rollback_addr = (record.from_version == get_current_version())
                                  ? IMAGE_B_ADDR : IMAGE_A_ADDR;

    // 读取固件信息
    firmware_header_t header;
    nor_flash_read(rollback_addr, &header, sizeof(header));

    // 验证固件完整性
    uint32_t crc = crc32_compute_at(rollback_addr + sizeof(header), header.size);
    if (crc != header.crc32) {
        // 备份固件也已损坏
        return -E_NO_VALID_FIRMWARE;
    }

    // 设置回退标志
    set_boot_flag(BOOT_FLAG_ROLLBACK);

    // 设置回退目标
    set_boot_target(rollback_addr);

    // 重启系统
    system_reset();

    return 0;
}

// 回退后的处理
void after_rollback(void)
{
    upgrade_record_t record;
    load_upgrade_record(&record);

    // 记录回退信息
    record.status = STATUS_ROLLBACK_COMPLETE;
    record.failed_count = 0;
    save_upgrade_record(&record);

    // 上报回退事件到服务器
    report_rollback_event(record.from_version, record.to_version);

    // 清除升级标志
    clear_upgrade_flag();
}
```

### 安全回退策略

为防止回退到存在安全漏洞的旧版本，可以实现版本策略控制：

```c
// 版本策略检查
int check_version_policy(uint32_t target_version, uint32_t current_version)
{
    // 安全策略：禁止回退超过安全版本数
    uint32_t min_allowed_version = get_security_baseline_version();
    if (target_version < min_allowed_version) {
        return -E_SECURITY_POLICY_VIOLATION;
    }

    // 检查版本兼容性矩阵
    if (!is_version_compatible(target_version, current_version)) {
        return -E_VERSION_INCOMPATIBLE;
    }

    return 0;
}

// 固件版本兼容性矩阵
typedef struct {
    uint32_t version;
    uint32_t can_upgrade_to[8];  // 可升级到的版本列表
    uint32_t can_rollback_to[8]; // 可回退到的版本列表
} version_compatibility_t;
```

---

## 本章小结

本章详细介绍了基于 Nor Flash 的 OTA 升级系统设计要点：

1. **升级流程设计**：从版本检查、固件下载、镜像验证到固件安装的完整流程，介绍了各阶段的关键实现和状态管理。

2. **镜像校验**：采用多层校验机制，包括完整性校验（CRC32、SHA-256）、身份校验（RSA 签名）和兼容性校验，确保固件的完整性、合法性和适配性。

3. **断点续传**：利用 HTTP Range 机制实现断点续传功能，通过定期保存下载断点和校验数据，确保网络不稳定时的升级成功率。

4. **版本回退**：设计了自动和手动两种回退触发机制，配合升级记录和测试周期管理，确保新固件出现问题时能够安全回退到稳定版本。

完整的 OTA 升级系统需要综合考虑安全性、可靠性和用户体验，实现固件的平滑升级和安全回退。

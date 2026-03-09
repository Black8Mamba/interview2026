# Nor Flash 安全特性

Nor Flash 在嵌入式系统中常用于存储敏感数据（如固件、密钥、配置信息等），因此需要提供多层次的安全保护机制。本文档将介绍 Nor Flash 的写保护、密码保护与加密功能，以及安全区域划分策略。

---

## 1. 写保护

### 1.1 写保护类型

Nor Flash 提供多种写保护机制：

- **硬件写保护**：通过引脚（/WP）控制
- **软件写保护**：通过状态寄存器配置
- **块/扇区保护**：保护特定的存储区域

### 1.2 硬件写保护

硬件写保护是最基础的保护方式，通过 /WP（Write Protect）引脚控制：

```
┌─────────────────────────────────────────────────────────────┐
│                   硬件写保护电路                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│    /WP ──┬──── Flash 芯片 /WP 引脚                        │
│         │                                                  │
│    ┌────┴────┐                                            │
│    │  接地   │  ──── 低电平: 写保护启用                    │
│    └─────────┘                                            │
│                                                             │
│    ┌────────┐                                             │
│    │ VCC    │  ──── 高电平: 写保护禁用                    │
│    └────────┘                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**特性**：

- /WP 引脚为低电平时，无法执行编程和擦除操作
- 优先级最高，无法被软件覆盖
- 适用于保护引导区域或关键配置

### 1.3 软件写保护

通过 Flash 内部的状态寄存器配置写保护：

```c
// 写保护类型
typedef enum {
    WP_NONE = 0,           // 无保护
    WP_HARDWARE,           // 硬件写保护
    WP_SOFTWARE,           // 软件写保护
    WP_PERMANENT          // 永久保护（不可逆）
} wp_type_t;

// 写保护配置
typedef struct {
    wp_type_t type;        // 保护类型
    uint32_t start_addr;   // 保护起始地址
    uint32_t end_addr;    // 保护结束地址
    uint8_t  block_level;  // 块级保护（1=启用）
} wp_config_t;

// SPI Nor Flash 状态寄存器1
typedef struct {
    uint8_t BUSY   : 1;    // 忙碌标志
    uint8_t WEL    : 1;    // 写使能锁存
    uint8_t BP0    : 1;    // 块保护位0
    uint8_t BP1    : 1;    // 块保护位1
    uint8_t BP2    : 1;    // 块保护位2
    uint8_t SEC    : 1;    // 扇区保护
    uint8_t TB     : 1;    // 顶部/底部块保护
    uint8_t SRP0   : 1;    // 状态寄存器保护位0
} status_reg1_t;

// 写保护操作
int flash_set_write_protect(uint32_t start_addr, uint32_t end_addr,
                           uint8_t enable)
{
    uint8_t status;
    int ret;

    // 1. 读取当前状态
    ret = flash_read_status_reg(1, &status);
    if (ret != 0) {
        return ret;
    }

    // 2. 根据地址范围确定保护级别
    uint32_t flash_size = get_flash_size();
    uint32_t block_size = get_sector_size();

    uint8_t bp_bits = calculate_bp_bits(start_addr, end_addr,
                                         flash_size, block_size);

    // 3. 修改状态寄存器
    status_reg1_t sr;
    sr.BYTE = status;

    if (enable) {
        // 启用保护
        sr.BP2 = (bp_bits >> 2) & 0x01;
        sr.BP1 = (bp_bits >> 1) & 0x01;
        sr.BP0 = bp_bits & 0x01;
    } else {
        // 禁用保护
        sr.BP2 = 0;
        sr.BP1 = 0;
        sr.BP0 = 0;
    }

    // 4. 写使能
    ret = flash_write_enable();
    if (ret != 0) {
        return ret;
    }

    // 5. 写入状态寄存器
    ret = flash_write_status_reg(1, sr.BYTE);

    return ret;
}

// 计算块保护位
uint8_t calculate_bp_bits(uint32_t start, uint32_t end,
                          uint32_t flash_size, uint32_t block_size)
{
    uint32_t protected_size = 0;

    if (end >= flash_size - block_size * 4) {
        protected_size = 4;  // 顶部 4 块
    } else if (end >= flash_size - block_size * 8) {
        protected_size = 8;  // 顶部 8 块
    } else if (end >= flash_size - block_size * 16) {
        protected_size = 16; // 顶部 16 块
    } else if (end >= flash_size / 2) {
        protected_size = 32; // 顶部一半
    }

    // 转换为 BP 位
    if (protected_size >= 32) return 0x07;  // BP2:1:0 = 111
    if (protected_size >= 16) return 0x06;  // BP2:1:0 = 110
    if (protected_size >= 8)  return 0x04;  // BP2:1:0 = 100
    if (protected_size >= 4)  return 0x02;  // BP2:1:0 = 010
    if (protected_size >= 2)  return 0x01;  // BP2:1:0 = 001

    return 0x00;  // 无保护
}
```

### 1.4 扇区保护

针对特定扇区的保护：

```c
// 扇区保护操作
int flash_sector_protect(uint32_t sector_addr, uint8_t protect)
{
    int ret;

    // 发送扇区保护命令（具体命令因芯片而异）
    uint8_t cmd = protect ? 0x36 : 0x39;  // 0x36=保护, 0x39=解除保护

    // 写使能
    ret = flash_write_enable();
    if (ret != 0) {
        return ret;
    }

    // 发送保护命令
    ret = flash_send_command(cmd, sector_addr);

    // 等待操作完成
    flash_wait_busy();

    return ret;
}

// 批量扇区保护
int flash_protect_range(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t sector_size = get_sector_size();
    uint32_t addr = start_addr;

    while (addr < end_addr) {
        int ret = flash_sector_protect(addr, 1);
        if (ret != 0) {
            return ret;
        }
        addr += sector_size;
    }

    return 0;
}
```

### 1.5 永久保护

某些 Flash 支持永久保护，一旦设置无法撤销：

```c
// 永久保护（锁定 OTP 区域）
int flash_permanent_lock(uint32_t addr)
{
    uint8_t status;

    // 某些芯片支持一次性编程锁定
    // 谨慎使用，无法撤销

    // 读取状态
    flash_read_status_reg(1, &status);

    // 某些芯片有特殊的永久锁定位
    // 例如: 设置 SRP1 位使 SRP0 不可更改

    return 0;
}
```

---

## 2. 密码保护与加密

### 2.1 密码保护机制

部分高端 Nor Flash 提供内置密码保护功能：

```c
// 密码保护配置
typedef struct {
    uint8_t  enabled;           // 是否启用密码保护
    uint8_t  password[16];      // 密码（通常为 32-128 位）
    uint32_t protected_start;  // 受保护起始地址
    uint32_t protected_end;    // 受保护结束地址
    uint8_t  unlock_attempts;   // 解锁尝试次数
    uint8_t  max_attempts;     // 最大尝试次数
} password_protection_t;

// 密码保护命令（因芯片而异）
#define CMD_PASSWORD_UNLOCK  0xE1  // 示例命令
#define CMD_PASSWORD_LOCK    0xE3  // 示例命令
#define CMD_PASSWORD_READ    0xE5  // 示例命令

// 解锁受保护的 Flash 区域
int flash_password_unlock(const uint8_t *password)
{
    int ret;

    // 发送解锁命令和密码
    ret = flash_send_command(CMD_PASSWORD_UNLOCK, 0);
    if (ret != 0) {
        return ret;
    }

    // 发送密码
    ret = flash_spi_transfer(password, 16);

    // 验证是否成功
    uint8_t status;
    flash_read_status_reg(1, &status);

    if ((status & 0x80) == 0) {  // 假设 Bit 7 表示锁定状态
        return 0;  // 解锁成功
    }

    return -EACCES;  // 密码错误
}

// 锁定 Flash 区域
int flash_password_lock(void)
{
    return flash_send_command(CMD_PASSWORD_LOCK, 0);
}
```

### 2.2 软件加密方案

对于没有硬件加密的 Flash，可使用软件加密：

#### 2.2.1 AES 加密

```c
// AES 加密上下文
typedef struct {
    uint8_t  key[32];           // AES-256 密钥
    uint8_t  iv[16];            // 初始向量
    uint32_t key_size;         // 密钥大小
} aes_context_t;

// 加密 Flash 数据
int flash_encrypt_write(aes_context_t *ctx, uint32_t addr,
                       const uint8_t *data, uint32_t len)
{
    uint8_t *encrypted = malloc(len);
    if (!encrypted) {
        return -ENOMEM;
    }

    // AES 加密
    aes_encrypt(ctx, data, encrypted, len);

    // 写入加密数据
    int ret = flash_write(addr, encrypted, len);

    free(encrypted);

    return ret;
}

// 解密 Flash 数据
int flash_decrypt_read(aes_context_t *ctx, uint32_t addr,
                      uint8_t *data, uint32_t len)
{
    uint8_t *encrypted = malloc(len);
    if (!encrypted) {
        return -ENOMEM;
    }

    // 读取加密数据
    int ret = flash_read(addr, encrypted, len);
    if (ret != 0) {
        free(encrypted);
        return ret;
    }

    // AES 解密
    aes_decrypt(ctx, encrypted, data, len);

    free(encrypted);

    return 0;
}
```

#### 2.2.2 XTS 模式（适用于块设备）

XTS 模式专为块存储设备设计，适合 Flash 加密：

```c
// XTS-AES 加密（适用于 Flash）
typedef struct {
    uint8_t  key[32];           // 128 位密钥（两份 64 位）
    uint32_t sector_size;      // 扇区大小
} xts_context_t;

// XTS 加密写入
int flash_xts_write(xts_context_t *ctx, uint32_t sector_num,
                   uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint8_t tweak[16];
    uint8_t *encrypted = malloc(len);

    // 生成 tweak 值
    generate_tweak(ctx, sector_num, tweak);

    // XTS 加密
    xts_encrypt(ctx->key, tweak, offset, data, encrypted, len);

    // 写入
    int ret = flash_write(sector_num * ctx->sector_size + offset,
                         encrypted, len);

    free(encrypted);

    return ret;
}

// XTS 解密读取
int flash_xts_read(xts_context_t *ctx, uint32_t sector_num,
                  uint32_t offset, uint8_t *data, uint32_t len)
{
    uint8_t tweak[16];
    uint8_t *encrypted = malloc(len);

    // 读取加密数据
    int ret = flash_read(sector_num * ctx->sector_size + offset,
                        encrypted, len);
    if (ret != 0) {
        free(encrypted);
        return ret;
    }

    // 生成 tweak 值
    generate_tweak(ctx, sector_num, tweak);

    // XTS 解密
    xts_decrypt(ctx->key, tweak, offset, encrypted, data, len);

    free(encrypted);

    return 0;
}
```

### 2.3 密钥存储

安全的密钥存储是加密方案的关键：

```c
// 安全密钥存储
typedef struct {
    uint8_t  key_valid;         // 密钥有效标志
    uint8_t  key_encrypted;     // 密钥是否已加密
    uint32_t key_addr;         // 密钥存储地址
    uint32_t key_version;       // 密钥版本
} key_storage_t;

// 密钥安全存储区域
#define KEY_STORAGE_ADDR   0x1000
#define KEY_STORAGE_SIZE   256

// 从安全区域加载密钥
int load_encryption_key(key_storage_t *storage, uint8_t *key)
{
    // 安全区域应受到写保护
    flash_set_write_protect(KEY_STORAGE_ADDR,
                           KEY_STORAGE_ADDR + KEY_STORAGE_SIZE, 1);

    // 读取密钥
    return flash_read(KEY_STORAGE_ADDR, key, 32);
}

// 首次启动时生成并存储密钥
int generate_and_store_key(void)
{
    uint8_t key[32];

    // 生成随机密钥
    generate_random_key(key, sizeof(key));

    // 加密密钥（使用根密钥或设备唯一密钥）
    uint8_t encrypted_key[32];
    encrypt_with_device_key(key, encrypted_key);

    // 写入受保护的存储区域
    int ret = flash_write(KEY_STORAGE_ADDR, encrypted_key,
                         sizeof(encrypted_key));

    // 启用写保护
    if 0) {
 (ret ==        flash_set_write_protect(KEY_STORAGE_ADDR,
                              KEY_STORAGE_ADDR + KEY_STORAGE_SIZE, 1);
    }

    return ret;
}
```

---

## 3. 安全区域划分

### 3.1 分区策略

合理划分 Flash 区域以实现安全隔离：

```
┌─────────────────────────────────────────────────────────────┐
│                  Flash 安全区域划分                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  引导区 (Boot)     0x00000 - 0x03FFF   │ 受硬件写保护       │
│  (4KB)                               │ 永久不可更改       │
│                    ─────────────────  │                    │
│  安全参数区        0x04000 - 0x04FFF   │ 受密码保护         │
│  (Secure Param)   │ 受软件写保护      │                    │
│                    │                    │                    │
│  密钥存储区        0x05000 - 0x05FFF   │ 最高安全级别       │
│  (Key Storage)    │ 加密存储          │ 物理隔离           │
│                    │                    │                    │
│  ─────────────────│────────────────────│                    │
│                    │                    │                    │
│  应用程序区        0x06000 - 0xFFFFF   │ 标准保护           │
│  (Application)    │ 可远程升级         │                    │
│                    │                    │                    │
│  用户数据区        0x100000 - ...      │ 无保护或可选保护   │
│  (User Data)      │ 灵活访问           │                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 安全区域管理

```c
// 安全区域定义
typedef enum {
    ZONE_BOOT,           // 引导区
    ZONE_SECURE_PARAM,  // 安全参数区
    ZONE_KEY_STORAGE,   // 密钥存储区
    ZONE_APPLICATION,   // 应用程序区
    ZONE_USER_DATA      // 用户数据区
} flash_zone_t;

// 区域配置
typedef struct {
    flash_zone_t zone;
    uint32_t start_addr;
    uint32_t end_addr;
    uint8_t  protection_level;  // 0=无, 1=软件, 2=硬件, 3=永久
    uint8_t  encrypted;         // 是否加密
    uint8_t  authenticated;     // 是否需要认证
} flash_zone_config_t;

// Flash 区域配置表
static const flash_zone_config_t zone_configs[] = {
    { ZONE_BOOT,          0x00000, 0x03FFF, 3, 0, 1 },
    { ZONE_SECURE_PARAM, 0x04000, 0x04FFF, 2, 1, 1 },
    { ZONE_KEY_STORAGE,   0x05000, 0x05FFF, 3, 1, 1 },
    { ZONE_APPLICATION,   0x06000, 0xFFFFF, 1, 0, 0 },
    { ZONE_USER_DATA,    0x100000, 0x1FFFFF, 0, 0, 0 },
};

// 获取区域配置
const flash_zone_config_t* get_zone_config(flash_zone_t zone)
{
    for (int i = 0; i < sizeof(zone_configs) / sizeof(zone_configs[0]); i++) {
        if (zone_configs[i].zone == zone) {
            return &zone_configs[i];
        }
    }
    return NULL;
}

// 检查访问权限
int check_zone_access(flash_zone_t zone, uint8_t operation)
{
    const flash_zone_config_t *config = get_zone_config(zone);
    if (!config) {
        return -EINVAL;
    }

    // 读取操作
    if (operation == OP_READ) {
        return 0;  // 通常允许读取
    }

    // 写入/擦除操作需要检查保护
    if (config->protection_level >= 2) {
        // 需要验证是否已解锁
        if (!is_zone_unlocked(zone)) {
            return -EACCES;
        }
    }

    return 0;
}

// 区域写入保护
int zone_write_protect(flash_zone_t zone)
{
    const flash_zone_config_t *config = get_zone_config(zone);
    if (!config) {
        return -EINVAL;
    }

    uint32_t start = config->start_addr;
    uint32_t end = config->end_addr;

    switch (config->protection_level) {
        case 1:  // 软件保护
            return flash_set_write_protect(start, end, 1);

        case 2:  // 硬件保护
            // 设置硬件写保护引脚
            return hardware_wp_enable();

        case 3:  // 永久保护
            return flash_permanent_lock(start);

        default:
            return 0;
    }
}
```

### 3.3 安全引导

安全引导验证固件完整性和来源：

```c
// 安全引导配置
typedef struct {
    uint32_t flash_addr;        // 固件地址
    uint32_t size;             // 固件大小
    uint8_t  signature[256];   // 数字签名
    uint8_t  hash[32];         // 哈希值
    uint32_t version;          // 固件版本
    uint32_t timestamp;        // 编译时间戳
} secure_boot_data_t;

// 固件签名验证
int verify_firmware_signature(const uint8_t *firmware, uint32_t size,
                             const secure_boot_data_t *sb_data,
                             const uint8_t *public_key)
{
    // 1. 验证哈希
    uint8_t hash[32];
    sha256(firmware, size, hash);

    if (memcmp(hash, sb_data->hash, 32) != 0) {
        log_error("Firmware hash mismatch\n");
        return -EACCES;
    }

    // 2. 验证签名
    int ret = rsa_verify(sb_data->hash, 32, sb_data->signature,
                        256, public_key);

    if (ret != 0) {
        log_error("Firmware signature verification failed\n");
        return -EACCES;
    }

    // 3. 验证版本（防回滚）
    if (sb_data->version < get_min_allowed_version()) {
        log_error("Firmware version too old\n");
        return -EACCES;
    }

    return 0;
}

// 安全引导流程
int secure_boot(void)
{
    secure_boot_data_t sb_data;
    uint8_t public_key[256];

    // 1. 从安全区域读取引导数据
    flash_read(SECURE_BOOT_DATA_ADDR, &sb_data, sizeof(sb_data));

    // 2. 加载公钥
    load_public_key(public_key);

    // 3. 读取固件
    uint8_t *firmware = malloc(sb_data.size);
    flash_read(sb_data.flash_addr, firmware, sb_data.size);

    // 4. 验证固件
    int ret = verify_firmware_signature(firmware, sb_data.size,
                                       &sb_data, public_key);

    free(firmware);

    if (ret != 0) {
        // 验证失败，进入恢复模式
        log_error("Secure boot failed, entering recovery mode\n");
        enter_recovery_mode();
        return ret;
    }

    // 5. 验证通过，跳转执行
    jump_to_application(sb_data.flash_addr);

    return 0;
}
```

### 3.4 防回滚机制

防止使用旧版本固件覆盖新版本：

```c
// 版本信息存储
typedef struct {
    uint32_t version;           // 当前版本
    uint32_t min_version;       // 允许的最低版本
    uint32_t counter;           // 升级计数器
    uint8_t  signature[64];      // 版本信息签名
} version_info_t;

#define VERSION_INFO_ADDR  0x04000

// 检查版本是否允许升级
int check_version_allowed(uint32_t new_version)
{
    version_info_t info;

    flash_read(VERSION_INFO_ADDR, &info, sizeof(info));

    // 版本必须递增
    if (new_version <= info.version) {
        log_error("Version must be greater than current: %u\n",
                 info.version);
        return -EACCES;
    }

    // 版本不能低于最低版本
    if (new_version < info.min_version) {
        log_error("Version %u is below minimum %u\n",
                 new_version, info.min_version);
        return -EACCES;
    }

    return 0;
}

// 更新版本信息
int update_version_info(uint32_t new_version)
{
    version_info_t info;
    uint8_t signature[64];

    // 读取当前版本信息
    flash_read(VERSION_INFO_ADDR, &info, sizeof(info));

    // 更新版本
    info.version = new_version;
    info.counter++;

    // 对新版本信息签名
    sign_version_info(&info, signature);
    memcpy(info.signature, signature, sizeof(info.signature));

    // 写入新版本信息
    return flash_write(VERSION_INFO_ADDR, &info, sizeof(info));
}
```

---

## 4. 综合安全方案

### 4.1 安全框架

```c
// Flash 安全框架
typedef struct {
    aes_context_t aes_ctx;              // AES 加密上下文
    xts_context_t xts_ctx;              // XTS 加密上下文
    flash_zone_config_t zones[5];       // 区域配置
    uint8_t  secure_boot_enabled;        // 安全引导使能
    uint8_t  encryption_enabled;        // 加密使能
    uint8_t  initialized;               // 初始化完成标志
} flash_security_ctx_t;

// 初始化安全框架
int flash_security_init(flash_security_ctx_t *ctx)
{
    int ret;

    // 初始化区域保护
    for (int i = 0; i < 5; i++) {
        memcpy(&ctx->zones[i], &zone_configs[i],
              sizeof(flash_zone_config_t));

        // 启用写保护
        if (ctx->zones[i].protection_level >= 2) {
            ret = zone_write_protect(ctx->zones[i].zone);
            if (ret != 0) {
                log_warning("Failed to protect zone %d\n", i);
            }
        }
    }

    // 加载加密密钥
    ret = load_encryption_key(ctx, ctx->aes_ctx.key);
    if (ret != 0) {
        // 首次启动，生成新密钥
        ret = generate_and_store_key();
    }

    ctx->encryption_enabled = 1;
    ctx->initialized = 1;

    return 0;
}

// 安全读取
int secure_flash_read(flash_security_ctx_t *ctx, uint32_t addr,
                      uint8_t *data, uint32_t len)
{
    flash_zone_t zone = get_zone_by_addr(addr);

    // 检查访问权限
    int ret = check_zone_access(zone, OP_READ);
    if (ret != 0) {
        return ret;
    }

    // 获取区域配置
    const flash_zone_config_t *config = get_zone_config(zone);

    // 执行读取
    ret = flash_read(addr, data, len);
    if (ret != 0) {
        return ret;
    }

    // 如果需要解密
    if (config->encrypted && ctx->encryption_enabled) {
        uint32_t sector = addr / ctx->xts_ctx.sector_size;
        uint32_t offset = addr % ctx->xts_ctx.sector_size;

        return flash_xts_read(&ctx->xts_ctx, sector, offset, data, len);
    }

    return 0;
}

// 安全写入
int secure_flash_write(flash_security_ctx_t *ctx, uint32_t addr,
                      const uint8_t *data, uint32_t len)
{
    flash_zone_t zone = get_zone_by_addr(addr);

    // 检查访问权限
    int ret = check_zone_access(zone, OP_WRITE);
    if (ret != 0) {
        return ret;
    }

    // 获取区域配置
    const flash_zone_config_t *config = get_zone_config(zone);

    uint8_t *data_to_write = (uint8_t *)data;

    // 如果需要加密
    if (config->encrypted && ctx->encryption_enabled) {
        uint8_t *encrypted = malloc(len);
        if (!encrypted) {
            return -ENOMEM;
        }

        uint32_t sector = addr / ctx->xts_ctx.sector_size;
        uint32_t offset = addr % ctx->xts_ctx.sector_size;

        ret = flash_xts_write(&ctx->xts_ctx, sector, offset,
                             data, encrypted);

        data_to_write = encrypted;
    }

    // 执行写入
    ret = flash_write(addr, data_to_write, len);

    if (data_to_write != data) {
        free(data_to_write);
    }

    // 记录到事务日志
    if (ret == 0) {
        log_write_pre(&g_transaction_log, addr, data, len);
    }

    return ret;
}
```

---

## 5. 小结

Nor Flash 的安全特性包括多层次的写保护、密码保护、加密存储以及安全区域划分。在实际应用中，需要根据安全需求和成本预算选择合适的安全方案。对于高安全要求的应用，建议采用安全引导、加密存储和防回滚机制的组合。

---

**相关文档**

- [Nor Flash 驱动框架设计](../02-driver-development/01-driver-framework.md)
- [Nor Flash 掉电保护策略](./04-power-fail-protection.md)
- [SPI Flash 驱动实现](../02-driver-development/02-spi-driver-impl.md)
- [Nor Flash 硬件基础](../01-hardware-basics/01-nor-flash-intro.md)

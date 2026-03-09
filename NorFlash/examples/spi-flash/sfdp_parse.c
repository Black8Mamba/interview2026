/**
 * @file sfdp_parse.c
 * @brief SFDP (Serial Flash Discoverable Parameters) 参数解析示例
 *
 * 本示例演示 Serial Flash Discoverable Parameters 的解析：
 * - SFDP 头信息读取
 * - JEDEC Flash 参数表解析
 * - 扇区大小、页大小提取
 * - 电压范围、时序参数获取
 *
 * 用于实现通用 SPI Flash 驱动
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// SFDP 命令
#define CMD_READ_SFDP            0x5A
#define CMD_READ_ID              0x9F

// SFDP 访问地址
#define SFDP_HEADER_ADDR         0x00
#define SFDP_JEDEC_PARAM_ADDR   0x10

// SFDP 签名
#define SFDP_SIGNATURE           0x50444653  // "SFDP" 小端序

// JEDEC 参数表关键偏移
#define JEDEC_PARAM_SIZE         0x00
#define JEDEC_PARAM_MAJOR_MINOR  0x01
#define JEDEC_PARAM_NUM_PARAMS   0x02
#define JEDEC_ERASE_TYPES        0x0F
#define JEDEC_ERASE_TYPE_1_INFO  0x10
#define JEDEC_ERASE_TYPE_2_INFO  0x13
#define JEDEC_ERASE_TYPE_3_INFO  0x16
#define JEDEC_ERASE_TYPE_4_INFO  0x19
#define JEDEC_PAGE_SIZE          0x1A
#define JEDEC_PAGE_SIZE_SHIFT    0x1B

// Flash 参数结构
typedef struct {
    // 芯片基本信息
    uint32_t signature;
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t num_parameters;
    uint32_t flash_size;          // 字节

    // 扇区/块配置
    uint32_t sector_size;         // 最小擦除单元
    uint8_t sector_erase_type;    // 擦除类型 (1-4)
    uint32_t block_size;          // 块大小
    uint8_t block_erase_type;     // 块擦除类型

    // 页配置
    uint16_t page_size;           // 页大小

    // 时序参数 (以纳秒为单位)
    uint32_t t_pp;                // 页编程时间
    uint32_t t_se;                // 扇区擦除时间
    uint32_t t_be;                // 块擦除时间
    uint32_t t_ce;                // 全片擦除时间

    // 时钟频率
    uint32_t max_freq;             // 最大时钟频率 (Hz)
    uint32_t max_read_freq;       // 最大读取频率

    // 特性标志
    uint8_t has_dual_quad;         // 支持双线/四线
    uint8_t has_3byte_addr;        // 3字节地址模式
    uint8_t has_4byte_addr;        // 4字节地址模式
    uint8_t has_volatile_qe;       // 临时 QE 位
} flash_params_t;

// 全局参数结构
static flash_params_t g_flash_params;

// 函数原型
void sfdp_read_header(uint8_t *buffer);
void sfdp_parse_jedec_params(const uint8_t *buffer);
void sfdp_parse_erase_info(flash_params_t *params, const uint8_t *buffer);
void sfdp_display_params(const flash_params_t *params);
int sfdp_detect_flash(uint8_t *manufacturer, uint16_t *device_id);
int sfdp_parse(void);

/**
 * @brief 读取 SFDP 头信息
 * @param buffer 接收缓冲区 (至少 16 字节)
 */
void sfdp_read_header(uint8_t *buffer)
{
    uint8_t cmd[5] = {
        CMD_READ_SFDP,
        0x00,  // 地址高字节
        0x00,  // 地址中字节
        0x00,  // 地址低字节
        0x00   // 虚拟周期
    };

    // 发送 SFDP 读取命令
    // TODO: SPI 传输实现

    // 解析头信息
    g_flash_params.signature = buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
    g_flash_params.minor_version = buffer[4];
    g_flash_params.major_version = buffer[5];
    g_flash_params.num_parameters = buffer[6];

    printf("SFDP Header:\n");
    printf("  Signature: 0x%08X (expected: 0x%08X)\n", g_flash_params.signature, SFDP_SIGNATURE);
    printf("  Version: %d.%d\n", g_flash_params.major_version, g_flash_params.minor_version);
    printf("  Parameters: %d\n", g_flash_params.num_parameters);
}

/**
 * @brief 解析 JEDEC 参数表
 * @param buffer 参数表数据 (至少 256 字节)
 */
void sfdp_parse_jedec_params(const uint8_t *buffer)
{
    uint32_t temp;
    uint8_t mem_density;

    // 1. 解析 Flash 容量 (bits 31:0, DWORDS 1-2)
    // 实际容量 = 2^density bits (除非 density >= 31)
    mem_density = buffer[7] & 0x1F;

    if (mem_density < 31) {
        g_flash_params.flash_size = (1UL << mem_density) / 8;  // 转换为字节
    } else {
        // 密度 >= 31 时，使用额外的密度信息
        g_flash_params.flash_size = (1UL << (mem_density - 31)) * 0x8000000;  // 256M bits
    }

    printf("\nFlash Size: %lu MB (%lu bytes)\n",
           g_flash_params.flash_size / (1024 * 1024),
           g_flash_params.flash_size);

    // 2. 解析页大小
    temp = buffer[42] & 0x0F;  // bits 3:0
    g_flash_params.page_size = 1 << temp;
    printf("Page Size: %d bytes\n", g_flash_params.page_size);

    // 3. 解析擦除信息
    sfdp_parse_erase_info(&g_flash_params, buffer);

    // 4. 解析时序参数
    // 4-byte 序列号指令时序
    // 页编程时间 (bits 31:24)
    temp = buffer[41] >> 4;  // 转换为微秒乘数
    g_flash_params.t_pp = temp * 10;  // 估算值

    // 5. 解析支持的特性
    // 检查第8个 DWORD (bits 127:96) 中的特性
    // bit 4: 4-byte 地址模式支持
    if (buffer[35] & (1 << 2)) {
        g_flash_params.has_4byte_addr = 1;
        printf("Supports 4-byte address mode\n");
    } else {
        g_flash_params.has_3byte_addr = 1;
    }

    // bit 3: 双口操作
    // bit 2: 四口操作
    if (buffer[35] & 0x04) {
        g_flash_params.has_dual_quad = 1;
        printf("Supports Quad SPI\n");
    }

    // 解析最大时钟频率
    // TODO: 根据参数表解析实际时序
    g_flash_params.max_freq = 50 * 1000 * 1000;  // 默认 50MHz
}

/**
 * @brief 解析擦除类型信息
 */
void sfdp_parse_erase_info(flash_params_t *params, const uint8_t *buffer)
{
    uint8_t erase_types;
    uint8_t type_1_size, type_2_size, type_3_size, type_4_size;

    // 解析支持的擦除类型
    erase_types = buffer[15];

    printf("\nSupported Erase Types:\n");

    // 擦除类型 1
    type_1_size = buffer[16] & 0x3F;
    if (erase_types & 0x01) {
        params->sector_erase_type = 1;
        params->sector_size = 1UL << type_1_size;
        printf("  Type 1: %lu bytes\n", params->sector_size);
    }

    // 擦除类型 2
    type_2_size = buffer[19] & 0x3F;
    if (erase_types & 0x02) {
        printf("  Type 2: %lu bytes\n", 1UL << type_2_size);
    }

    // 擦除类型 3
    type_3_size = buffer[22] & 0x3F;
    if (erase_types & 0x04) {
        printf("  Type 3: %lu bytes\n", 1UL << type_3_size);
    }

    // 擦除类型 4
    type_4_size = buffer[25] & 0x3F;
    if (erase_types & 0x08) {
        printf("  Type 4: %lu bytes\n", 1UL << type_4_size);
    }
}

/**
 * @brief 显示解析后的 Flash 参数
 */
void sfdp_display_params(const flash_params_t *params)
{
    printf("\n=== SFDP Parsed Parameters ===\n");
    printf("Flash Size: %lu bytes (%.2f MB)\n",
           params->flash_size,
           (float)params->flash_size / (1024 * 1024));
    printf("Page Size: %d bytes\n", params->page_size);
    printf("Sector Size: %lu bytes\n", params->sector_size);
    printf("Max Clock Frequency: %lu MHz\n", params->max_freq / 1000000);
    printf("3-byte Address: %s\n", params->has_3byte_addr ? "Yes" : "No");
    printf("4-byte Address: %s\n", params->has_4byte_addr ? "Yes" : "No");
    printf("Quad SPI: %s\n", params->has_dual_quad ? "Yes" : "No");
    printf("================================\n");
}

/**
 * @brief 检测 Flash 芯片
 */
int sfdp_detect_flash(uint8_t *manufacturer, uint16_t *device_id)
{
    uint8_t id_buf[3];

    // 读取 JEDEC ID
    // TODO: 实现 SPI 读取

    *manufacturer = id_buf[0];
    *device_id = (id_buf[1] << 8) | id_buf[2];

    printf("Flash Detected: Manufacturer=0x%02X, DeviceID=0x%04X\n",
           *manufacturer, *device_id);

    return 0;
}

/**
 * @brief 主函数 - SFDP 解析示例
 */
int main(void)
{
    uint8_t manufacturer;
    uint16_t device_id;
    uint8_t sfdp_buffer[256];

    printf("=== SFDP Parameter Parsing Demo ===\n\n");

    // 步骤 1: 检测 Flash
    if (sfdp_detect_flash(&manufacturer, &device_id) != 0) {
        printf("Error: Flash detection failed\n");
        return -1;
    }

    // 步骤 2: 读取 SFDP 头信息
    printf("\n--- Reading SFDP Header ---\n");
    sfdp_read_header(sfdp_buffer);

    // 步骤 3: 读取 JEDEC 参数表
    printf("\n--- Reading JEDEC Parameter Table ---\n");
    // TODO: 读取参数表到 sfdp_buffer

    // 步骤 4: 解析参数
    printf("\n--- Parsing Parameters ---\n");
    sfdp_parse_jedec_params(sfdp_buffer);

    // 步骤 5: 显示解析结果
    sfdp_display_params(&g_flash_params);

    // 使用解析结果配置驱动
    // TODO: 根据 g_flash_params 配置驱动参数

    printf("\n=== Demo Complete ===\n");
    return 0;
}

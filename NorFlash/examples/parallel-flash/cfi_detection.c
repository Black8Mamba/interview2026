/**
 * @file cfi_detection.c
 * @brief CFI 芯片检测示例
 *
 * 本示例实现 Common Flash Interface 芯片识别：
 * - CFI 查询模式进入
 * - 制造商 ID 和设备 ID 读取
 * - CFI 信息结构解析
 * - 芯片兼容性判断
 */

#include <stdio.h>
#include <stdint.h>

// CFI 命令定义
#define CFI_QUERY_CMD1         0x98    // 进入 CFI 查询模式
#define CFI_QUERY_CMD2         0xF0    // 退出 CFI 查询模式

// CFI 查询地址
#define CFI_MANUFACTURER_ADDR  0x00    // 制造商 ID 地址
#define CFI_DEVICE_ID_ADDR     0x01    // 设备 ID 地址
#define CFI_QUERY_SIGNATURE    0x10    // CFI 查询签名 "QRY"
#define CFI_QUERY_STRING       0x11    // CFI 查询字符串

// Flash 地址 (根据 FSMC 配置)
#define FLASH_BASE             0x60000000

// 芯片 ID 结构
typedef struct {
    uint16_t manufacturer;
    uint16_t device_id;
    char *manufacturer_name;
    char *device_name;
} chip_id_t;

// CFI 信息结构
typedef struct {
    char query_string[4];        // "QRY"
    uint16_t primary_cmd_set;    // 主命令集
    uint16_t algo_id;            // 算法 ID
    uint8_t vcc_min;            // 最小工作电压
    uint8_t vcc_max;            // 最大工作电压
    uint8_t vpp_min;            // 最小编程电压
    uint8_t vpp_max;            // 最大编程电压
    uint8_t word_write_timeout;  // 单字写超时 (2^n us)
    uint8_t buf_write_timeout;  // 缓冲写超时 (2^n us)
    uint8_t block_erase_timeout;// 块擦除超时 (2^n ms)
    uint8_t chip_erase_timeout; // 芯片擦除超时 (2^n ms)
    uint8_t dev_size;           // 设备密度 (2^n bytes)
    uint8_t interface_code;     // 接口类型
    uint8_t max_buf_write_size; // 最大缓冲写入大小
} cfi_info_t;

// 函数原型
void cfi_enter_query_mode(void);
void cfi_exit_query_mode(void);
void cfi_read_id(chip_id_t *id);
int cfi_detect(cfi_info_t *info);
void cfi_display_info(const cfi_info_t *info, const chip_id_t *id);
int cfi_verify(void);

/**
 * @brief 进入 CFI 查询模式
 */
void cfi_enter_query_mode(void)
{
    volatile uint16_t *flash;

    flash = (volatile uint16_t *)FLASH_BASE;

    // 发送 CFI 查询命令
    flash[CFI_QUERY_CMD1] = 0x98;

    // 延时等待
    for (volatile int i = 0; i < 100; i++);
}

/**
 * @brief 退出 CFI 查询模式
 */
void cfi_exit_query_mode(void)
{
    volatile uint16_t *flash;

    flash = (volatile uint16_t *)FLASH_BASE;

    // 退出 CFI 查询模式 (复位)
    flash[0] = 0xF0;

    // 延时
    for (volatile int i = 0; i < 100; i++);
}

/**
 * @brief 读取芯片 ID (自动退出 CFI 模式)
 */
void cfi_read_id(chip_id_t *id)
{
    volatile uint16_t *flash;
    uint16_t manufacturer, device_id;

    flash = (volatile uint16_t *)FLASH_BASE;

    // 读取制造商 ID 和设备 ID
    manufacturer = flash[0x00];
    device_id = flash[0x01];

    // 退出 CFI 模式
    flash[0] = 0xF0;

    id->manufacturer = manufacturer;
    id->device_id = device_id;

    // 识别制造商
    switch (manufacturer) {
        case 0x0001: id->manufacturer_name = "AMD/Spansion"; break;
        case 0x0019: id->manufacturer_name = "Macronix"; break;
        case 0x0020: id->manufacturer_name = "Micron"; break;
        case 0x0037: id->manufacturer_name = "Micron"; break;
        case 0x0040: id->manufacturer_name = "Eon"; break;
        case 0x0089: id->manufacturer_name = "Intel"; break;
        case 0x00DA: id->manufacturer_name = "Winbond"; break;
        case 0x00BF: id->manufacturer_name = "SST"; break;
        default:     id->manufacturer_name = "Unknown"; break;
    }

    // 识别设备
    printf("Chip ID: Manufacturer=0x%04X, Device=0x%04X\n",
           manufacturer, device_id);
}

/**
 * @brief 检测并读取 CFI 信息
 */
int cfi_detect(cfi_info_t *info)
{
    volatile uint16_t *flash;
    uint8_t *info_bytes;
    uint32_t i;

    // 进入 CFI 查询模式
    cfi_enter_query_mode();

    flash = (volatile uint16_t *)FLASH_BASE;
    info_bytes = (uint8_t *)flash;

    // 读取 CFI 查询签名
    for (i = 0; i < 3; i++) {
        info->query_string[i] = info_bytes[CFI_QUERY_STRING + i * 2];
    }
    info->query_string[3] = '\0';

    // 验证签名
    if (strncmp(info->query_string, "QRY", 3) != 0) {
        printf("Error: Invalid CFI signature\n");
        cfi_exit_query_mode();
        return -1;
    }

    printf("CFI Signature: %s\n", info->query_string);

    // 读取 CFI 参数 (大端序)
    info->primary_cmd_set = flash[0x13];
    info->algo_id = flash[0x14];

    // 电压信息
    info->vcc_min = flash[0x1B] & 0x0F;
    info->vcc_max = (flash[0x1B] >> 4) & 0x0F;
    info->vpp_min = flash[0x1C] & 0x0F;
    info->vpp_max = (flash[0x1C] >> 4) & 0x0F;

    // 超时参数
    info->word_write_timeout = flash[0x1F];
    info->buf_write_timeout = flash[0x20];
    info->block_erase_timeout = flash[0x21];
    info->chip_erase_timeout = flash[0x22];

    // 设备密度
    info->dev_size = flash[0x27];

    // 接口类型
    info->interface_code = flash[0x28];

    // 最大缓冲写入大小
    info->max_buf_write_size = flash[0x2A];

    // 退出 CFI 查询模式
    cfi_exit_query_mode();

    return 0;
}

/**
 * @brief 显示 CFI 信息
 */
void cfi_display_info(const cfi_info_t *info, const chip_id_t *id)
{
    uint32_t flash_size;

    printf("\n=== CFI Detection Results ===\n");
    printf("Query Signature: %s\n", info->query_string);
    printf("Primary Command Set: 0x%04X\n", info->primary_cmd_set);
    printf("Algorithm ID: 0x%04X\n", info->algo_id);

    printf("\nVoltage:\n");
    printf("  Vcc: %d.%dV - %d.%dV\n",
           info->vcc_min / 10, info->vcc_min % 10,
           info->vcc_max / 10, info->vcc_max % 10);

    printf("\nTimeouts:\n");
    printf("  Word Write: 2^%d us\n", info->word_write_timeout);
    printf("  Buffer Write: 2^%d us\n", info->buf_write_timeout);
    printf("  Block Erase: 2^%d ms\n", info->block_erase_timeout);
    printf("  Chip Erase: 2^%d ms\n", info->chip_erase_timeout);

    // 计算实际密度
    flash_size = 1UL << info->dev_size;
    printf("\nDevice Size: %lu bytes (%.2f MB)\n",
           flash_size, (float)flash_size / (1024 * 1024));

    printf("Interface Code: 0x%02X\n", info->interface_code);
    printf("Max Buffer Write: %d bytes\n", 1 << info->max_buf_write_size);

    printf("\nChip Identification:\n");
    printf("  Manufacturer: %s (0x%04X)\n",
           id->manufacturer_name, id->manufacturer);
    printf("  Device ID: 0x%04X\n", id->device_id);

    printf("==============================\n");
}

/**
 * @brief 验证 CFI 兼容性
 */
int cfi_verify(void)
{
    cfi_info_t info;
    chip_id_t id;

    // 检测芯片
    if (cfi_detect(&info) != 0) {
        printf("Flash does not support CFI or detection failed\n");
        return -1;
    }

    // 读取 ID
    cfi_read_id(&id);

    // 显示信息
    cfi_display_info(&info, &id);

    return 0;
}

/**
 * @brief 主函数 - CFI 检测示例
 */
int main(void)
{
    printf("=== CFI Detection Demo ===\n\n");

    if (cfi_verify() == 0) {
        printf("\nCFI Detection SUCCESS\n");
    } else {
        printf("\nCFI Detection FAILED\n");
        return -1;
    }

    printf("\n=== Demo Complete ===\n");
    return 0;
}

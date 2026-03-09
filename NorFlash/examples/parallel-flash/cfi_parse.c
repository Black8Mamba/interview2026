/**
 * @file cfi_parse.c
 * @brief CFI 参数解析示例
 *
 * 本示例解析 CFI 提供的芯片参数：
 * - 扇区布局信息提取
 * - 电气时序参数获取
 * - 芯片容量和特性解析
 * - 保护区域信息读取
 */

#include <stdio.h>
#include <stdint.h>

// CFI 查询地址
#define FLASH_BASE              0x60000000
#define CFI_QUERY_ADDR          0x55

// CFI 偏移地址
#define CFI_QUERY_SIGNATURE     0x10
#define CFI_PRIMARY_CMD_SET     0x13
#define CFI_ALGO_ID             0x14
#define CFI_VCC_MIN             0x1B
#define CFI_VCC_MAX             0x1C
#define CFI_VPP_MIN             0x1D
#define CFI_VPP_MAX             0x1E
#define CFI_WORD_WRITE_TOUT     0x1F
#define CFI_BUF_WRITE_TOUT      0x20
#define CFI_BLOCK_ERASE_TOUT    0x21
#define CFI_CHIP_ERASE_TOUT     0x22
#define CFI_DEV_SIZE            0x27
#define CFI_INTERFACE_CODE       0x28
#define CFI_MAX_BUF_WRITE_SIZE  0x2A
#define CFI_NUM_ERASE_REGIONS   0x2C
#define CFI_ERASE_REGION_INFO   0x2D

// 扇区信息结构
typedef struct {
    uint32_t region_count;
    uint32_t sector_size[8];
    uint32_t sector_count[8];
    uint32_t total_sectors;
} sector_layout_t;

// 解析后的参数结构
typedef struct {
    // 基本信息
    char query_string[4];
    uint16_t cmd_set;
    uint32_t flash_size;

    // 时序参数
    uint32_t word_write_timeout_us;
    uint32_t buf_write_timeout_us;
    uint32_t block_erase_timeout_ms;
    uint32_t chip_erase_timeout_ms;

    // 接口信息
    uint8_t interface_code;
    uint32_t max_buffer_write;

    // 特性
    uint8_t vcc_min;
    uint8_t vcc_max;
    uint8_t vpp_min;
    uint8_t vpp_max;

    // 扇区布局
    sector_layout_t sector_layout;
} cfi_params_t;

// 函数原型
void cfi_enter_query(void);
void cfi_exit_query(void);
void cfi_parse_basic_params(cfi_params_t *params);
void cfi_parse_sector_layout(cfi_params_t *params);
void cfi_display_params(const cfi_params_t *params);

/**
 * @brief 进入 CFI 查询模式
 */
void cfi_enter_query(void)
{
    volatile uint16_t *flash = (volatile uint16_t *)FLASH_BASE;
    flash[CFI_QUERY_ADDR] = 0x98;
    for (volatile int i = 0; i < 100; i++);
}

/**
 * @brief 退出 CFI 查询模式
 */
void cfi_exit_query(void)
{
    volatile uint16_t *flash = (volatile uint16_t *)FLASH_BASE;
    flash[0] = 0xF0;
    for (volatile int i = 0; i < 100; i++);
}

/**
 * @brief 解析基本参数
 */
void cfi_parse_basic_params(cfi_params_t *params)
{
    volatile uint16_t *flash;
    uint16_t *param_ptr;
    uint8_t tout;

    cfi_enter_query();
    flash = (volatile uint16_t *)FLASH_BASE;
    param_ptr = (uint16_t *)flash;

    // 读取查询字符串
    params->query_string[0] = (uint8_t)(flash[CFI_QUERY_SIGNATURE] & 0xFF);
    params->query_string[1] = (uint8_t)(flash[CFI_QUERY_SIGNATURE + 2] & 0xFF);
    params->query_string[2] = (uint8_t)(flash[CFI_QUERY_SIGNATURE + 4] & 0xFF);
    params->query_string[3] = '\0';

    // 读取命令集
    params->cmd_set = flash[CFI_PRIMARY_CMD_SET];

    // 读取电压范围
    params->vcc_min = (uint8_t)(flash[CFI_VCC_MIN] & 0x0F);
    params->vcc_max = (uint8_t)((flash[CFI_VCC_MIN] >> 4) & 0x0F);
    params->vpp_min = (uint8_t)(flash[CFI_VPP_MIN] & 0x0F);
    params->vpp_max = (uint8_t)((flash[CFI_VPP_MIN] >> 4) & 0x0F);

    // 读取超时参数
    tout = (uint8_t)flash[CFI_WORD_WRITE_TOUT];
    params->word_write_timeout_us = (tout > 31) ? (1UL << 31) : (1UL << tout);

    tout = (uint8_t)flash[CFI_BUF_WRITE_TOUT];
    params->buf_write_timeout_us = (tout > 31) ? (1UL << 31) : (1UL << tout);

    tout = (uint8_t)flash[CFI_BLOCK_ERASE_TOUT];
    params->block_erase_timeout_ms = (tout > 31) ? (1UL << 31) : (1UL << tout);

    tout = (uint8_t)flash[CFI_CHIP_ERASE_TOUT];
    params->chip_erase_timeout_ms = (tout > 31) ? (1UL << 31) : (1UL << tout);

    // 读取 Flash 容量
    params->flash_size = 1UL << flash[CFI_DEV_SIZE];

    // 读取接口类型
    params->interface_code = (uint8_t)flash[CFI_INTERFACE_CODE];

    // 读取最大缓冲写入
    params->max_buffer_write = 1UL << flash[CFI_MAX_BUF_WRITE_SIZE];

    cfi_exit_query();
}

/**
 * @brief 解析扇区布局
 */
void cfi_parse_sector_layout(cfi_params_t *params)
{
    volatile uint16_t *flash;
    uint8_t num_regions;
    uint32_t region_size;
    uint8_t region_num_sectors;
    uint32_t i;
    uint32_t total_sectors = 0;

    cfi_enter_query();
    flash = (volatile uint16_t *)FLASH_BASE;

    // 读取擦除区域数量
    num_regions = (uint8_t)flash[CFI_NUM_ERASE_REGIONS];
    params->sector_layout.region_count = num_regions;

    printf("Erase Regions: %d\n", num_regions);

    // 解析每个区域的信息
    for (i = 0; i < num_regions && i < 8; i++) {
        uint32_t offset = CFI_ERASE_REGION_INFO + (i * 4);

        // 读取区域信息 (大端序)
        region_num_sectors = (uint8_t)flash[offset] + 1;
        region_size = (flash[offset + 1] | (flash[offset + 2] << 16));
        region_size = (region_size + 1) * 256;  // 单位是 256 字节

        params->sector_layout.sector_count[i] = region_num_sectors;
        params->sector_layout.sector_size[i] = region_size;
        total_sectors += region_num_sectors;

        printf("  Region %d: %d sectors x %lu bytes = %lu KB\n",
               i, region_num_sectors,
               region_size,
               region_size * region_num_sectors / 1024);
    }

    params->sector_layout.total_sectors = total_sectors;

    cfi_exit_query();
}

/**
 * @brief 显示解析后的参数
 */
void cfi_display_params(const cfi_params_t *params)
{
    uint32_t i;

    printf("\n=== CFI Parameters ===\n");
    printf("Query String: %s\n", params->query_string);
    printf("Command Set: 0x%04X\n", params->cmd_set);
    printf("Flash Size: %lu bytes (%.2f MB)\n",
           params->flash_size, (float)params->flash_size / (1024 * 1024));

    printf("\nTiming Parameters:\n");
    printf("  Word Write Timeout: %lu us\n", params->word_write_timeout_us);
    printf("  Buffer Write Timeout: %lu us\n", params->buf_write_timeout_us);
    printf("  Block Erase Timeout: %lu ms\n", params->block_erase_timeout_ms);
    printf("  Chip Erase Timeout: %lu ms\n", params->chip_erase_timeout_ms);

    printf("\nVoltage:\n");
    printf("  Vcc: %d.%dV - %d.%dV\n",
           params->vcc_min / 10, params->vcc_min % 10,
           params->vcc_max / 10, params->vcc_max % 10);
    printf("  Vpp: %d.%dV - %d.%dV\n",
           params->vpp_min / 10, params->vpp_min % 10,
           params->vpp_max / 10, params->vpp_max % 10);

    printf("\nInterface: ");
    switch (params->interface_code) {
        case 0x00: printf("x8 Async\n"); break;
        case 0x01: printf("x16 Async\n"); break;
        case 0x02: printf("x8/x16 I/O\n"); break;
        case 0x03: printf("x32 Async\n"); break;
        default:   printf("Unknown\n"); break;
    }

    printf("Max Buffer Write: %lu bytes\n", params->max_buffer_write);

    printf("\nSector Layout:\n");
    printf("  Total Sectors: %lu\n", params->sector_layout.total_sectors);
    for (i = 0; i < params->sector_layout.region_count; i++) {
        printf("  Region %lu: %lu x %lu bytes\n",
               i,
               params->sector_layout.sector_count[i],
               params->sector_layout.sector_size[i]);
    }

    printf("======================\n");
}

/**
 * @brief 主函数 - CFI 解析示例
 */
int main(void)
{
    cfi_params_t params;

    printf("=== CFI Parameter Parsing Demo ===\n\n");

    // 解析基本参数
    printf("[1] Parsing basic parameters...\n");
    cfi_parse_basic_params(&params);

    // 解析扇区布局
    printf("\n[2] Parsing sector layout...\n");
    cfi_parse_sector_layout(&params);

    // 显示结果
    printf("\n[3] Displaying parameters...\n");
    cfi_display_params(&params);

    printf("\n=== Demo Complete ===\n");
    return 0;
}

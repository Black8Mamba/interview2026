/**
 * @file xilinx_interface.c
 * @brief Xilinx FPGA Flash 接口示例
 *
 * 本示例展示 FPGA 配置 Flash 的接口方式：
 * - Xilinx Platform Flash 识别
 * - FPGA 配置数据读取
 * - 边界扫描接口使用
 * - 多 Flash 级联配置
 *
 * 适用于 FPGA 开发板的 Flash 编程
 */

#include <stdio.h>
#include <stdint.h>

// Xilinx 配置 Flash 命令
#define XILINX_UNLOCK1          0xAA
#define XILINX_UNLOCK2          0x55
#define XILINX_READ_ID          0x90
#define XILINX_READ_STATUS      0x70
#define XILINX_CLEAR_STATUS    0x50
#define XILINX_ERASE           0x80
#define XILINX_PROGRAM         0xA0
#define XILINX_READ            0xF0

// Xilinx FPGA 配置接口
#define XILINX_CONFIG_BASE      0x60000000
#define XILINX_SPI_BASE         0x64000000

// Platform Flash ID
#define PLATFORM_FLASH_XCF1     0x004D
#define PLATFORM_FLASH_XCF2     0x004E
#define PLATFORM_FLASH_XCF4     0x0041
#define PLATFORM_FLASH_XCF8     0x0043

// Flash 信息结构
typedef struct {
    uint16_t manufacturer_id;
    uint16_t device_id;
    uint32_t capacity;
    uint32_t sector_size;
    uint32_t num_sectors;
    char name[32];
} xilinx_flash_info_t;

// 全局 Flash 信息
static xilinx_flash_info_t g_flash_info;

/**
 * @brief 读取 Platform Flash ID
 */
void xilinx_read_flash_id(xilinx_flash_info_t *info)
{
    volatile uint16_t *flash;
    uint16_t manufacturer, device;

    flash = (volatile uint16_t *)XILINX_CONFIG_BASE;

    // 发送读 ID 命令
    flash[0x5555] = 0xAA;
    flash[0x2AAA] = 0x55;
    flash[0x5555] = 0x90;

    // 读取 ID
    manufacturer = flash[0x0000];
    device = flash[0x0001];

    // 退出读取模式
    flash[0] = 0xF0;

    info->manufacturer_id = manufacturer;
    info->device_id = device;

    // 识别芯片
    switch (device) {
        case PLATFORM_FLASH_XCF1:
            strcpy(info->name, "Xilinx Platform Flash XCF1");
            info->capacity = 1 * 1024 * 1024;    // 1Mb
            info->sector_size = 64 * 1024;        // 64KB
            info->num_sectors = 16;
            break;
        case PLATFORM_FLASH_XCF2:
            strcpy(info->name, "Xilinx Platform Flash XCF2");
            info->capacity = 2 * 1024 * 1024;    // 2Mb
            info->sector_size = 64 * 1024;
            info->num_sectors = 32;
            break;
        case PLATFORM_FLASH_XCF4:
            strcpy(info->name, "Xilinx Platform Flash XCF4");
            info->capacity = 4 * 1024 * 1024;    // 4Mb
            info->sector_size = 128 * 1024;      // 128KB
            info->num_sectors = 32;
            break;
        case PLATFORM_FLASH_XCF8:
            strcpy(info->name, "Xilinx Platform Flash XCF8");
            info->capacity = 8 * 1024 * 1024;    // 8Mb
            info->sector_size = 128 * 1024;
            info->num_sectors = 64;
            break;
        default:
            strcpy(info->name, "Unknown Platform Flash");
            info->capacity = 0;
            info->sector_size = 0;
            info->num_sectors = 0;
            printf("Warning: Unknown device ID 0x%04X\n", device);
            break;
    }

    printf("Flash ID: Manufacturer=0x%04X, Device=0x%04X\n",
           manufacturer, device);
    printf("Flash Name: %s\n", info->name);
}

/**
 * @brief 读取 Flash 状态寄存器
 */
uint16_t xilinx_read_status(void)
{
    volatile uint16_t *flash;
    uint16_t status;

    flash = (volatile uint16_t *)XILINX_CONFIG_BASE;

    // 发送读状态命令
    flash[0x5555] = 0xAA;
    flash[0x2AAA] = 0x55;
    flash[0x5555] = 0x70;

    // 读取状态
    status = flash[0];

    // 退出
    flash[0] = 0xF0;

    return status;
}

/**
 * @brief 等待 Flash 忙碌结束
 */
void xilinx_wait_ready(void)
{
    uint16_t status;
    uint32_t timeout = 100000;

    do {
        status = xilinx_read_status();
        if (timeout-- == 0) {
            printf("Error: Timeout waiting for flash ready\n");
            return;
        }
    } while (status & 0x80);  // 检查 busy 位
}

/**
 * @brief 解锁 Flash
 */
void xilinx_unlock(void)
{
    volatile uint16_t *flash;

    flash = (volatile uint16_t *)XILINX_CONFIG_BASE;

    // 发送解锁序列
    flash[0x5555] = 0xAA;
    flash[0x2AAA] = 0x55;
    flash[0x5555] = 0x80;
    flash[0x5555] = 0xAA;
    flash[0x2AAA] = 0x55;
}

/**
 * @brief 扇区擦除
 */
void xilinx_sector_erase(uint32_t sector_addr)
{
    volatile uint16_t *flash;

    flash = (volatile uint16_t *)XILINX_CONFIG_BASE;

    // 解锁
    xilinx_unlock();

    // 发送擦除命令
    flash[0x5555] = 0xAA;
    flash[0x2AAA] = 0x55;
    flash[0x5555] = 0x30;

    // 地址
    flash[sector_addr / 2] = 0x30;

    // 等待完成
    xilinx_wait_ready();

    printf("Sector at 0x%08X erased.\n", sector_addr);
}

/**
 * @brief 编程 Flash (字编程)
 */
void xilinx_program_word(uint32_t addr, uint16_t data)
{
    volatile uint16_t *flash;

    flash = (volatile uint16_t *)XILINX_CONFIG_BASE;

    // 解锁
    xilinx_unlock();

    // 发送编程命令
    flash[0x5555] = 0xAA;
    flash[0x2AAA] = 0x55;
    flash[0x5555] = 0xA0;

    // 写入数据
    flash[addr / 2] = data;

    // 等待完成
    xilinx_wait_ready();
}

/**
 * @brief 读取 Flash 数据
 */
uint16_t xilinx_read_word(uint32_t addr)
{
    volatile uint16_t *flash;
    uint16_t data;

    flash = (volatile uint16_t *)XILINX_CONFIG_BASE;

    data = flash[addr / 2];

    return data;
}

/**
 * @brief 读取 FPGA 配置数据
 */
void xilinx_read_config_data(uint32_t offset, uint8_t *buffer, uint32_t len)
{
    uint32_t i;
    uint16_t word;

    // FPGA 配置数据通常存储在 Flash 的特定位置
    // 例如从偏移 0x100000 开始
    uint32_t config_offset = 0x100000 + offset;

    for (i = 0; i < len; i += 2) {
        word = xilinx_read_word(config_offset + i);
        buffer[i] = (uint8_t)(word & 0xFF);
        if (i + 1 < len) {
            buffer[i + 1] = (uint8_t)((word >> 8) & 0xFF);
        }
    }
}

/**
 * @brief 编程 FPGA 配置文件
 */
int xilinx_program_bitstream(uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t addr;
    uint16_t word;

    printf("Programming FPGA bitstream...\n");

    // 擦除所有使用的扇区
    // 假设 bitstream 从偏移 0x100000 开始
    for (i = 0; i < len; i += g_flash_info.sector_size) {
        xilinx_sector_erase(0x100000 + i);
    }

    // 编程数据
    addr = 0x100000 + offset;
    for (i = 0; i < len; i += 2) {
        word = data[i];
        if (i + 1 < len) {
            word |= (data[i + 1] << 8);
        }
        xilinx_program_word(addr + i, word);

        if ((i / 2) % 1000 == 0) {
            printf("Progress: %lu/%lu bytes\r", i, len);
        }
    }

    printf("Progress: %lu/%lu bytes\n", len, len);
    printf("Programming complete.\n");

    return 0;
}

/**
 * @brief 边界扫描接口操作 (JTAG)
 */
void jtag_init(void)
{
    // TODO: 初始化 JTAG 接口
    printf("JTAG interface initialized\n");
}

/**
 * @brief 通过 JTAG 读取 Flash ID
 */
uint32_t jtag_read_flash_id(void)
{
    // TODO: 通过 JTAG TAP 状态机读取 Flash ID
    // 1. 进入 SHIFT-DR 状态
    // 2. 发送读 ID 命令
    // 3. 读取 32 位 ID
    printf("JTAG Flash ID read: 0x");

    // 示例返回值
    return 0x01444043;
}

/**
 * @brief 级联 Flash 检测
 */
int detect_cascade_flash(void)
{
    int flash_count = 0;
    uint32_t i;

    printf("\n=== Cascade Flash Detection ===\n");

    // 最多支持 4 个级联 Flash
    for (i = 0; i < 4; i++) {
        // 切换到对应的 Flash (通过片选)
        // TODO: 配置片选信号

        printf("Checking Flash #%d...\n", i + 1);

        // 尝试读取 ID
        xilinx_read_flash_id(&g_flash_info);

        if (g_flash_info.capacity > 0) {
            flash_count++;
            printf("  Found: %s\n", g_flash_info.name);
        }
    }

    printf("Total Cascade Flashes: %d\n", flash_count);
    return flash_count;
}

/**
 * @brief 主函数 - Xilinx Flash 接口示例
 */
int main(void)
{
    uint8_t test_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                             0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    uint8_t read_buffer[16];
    uint32_t i;

    printf("=== Xilinx FPGA Flash Interface Demo ===\n\n");

    // 步骤 1: 初始化 JTAG
    printf("[Step 1] Initialize JTAG interface...\n");
    jtag_init();

    // 步骤 2: 读取 Flash ID
    printf("\n[Step 2] Read Flash ID...\n");
    xilinx_read_flash_id(&g_flash_info);

    // 显示 Flash 信息
    printf("\nFlash Information:\n");
    printf("  Name: %s\n", g_flash_info.name);
    printf("  Capacity: %lu bytes (%.2f MB)\n",
           g_flash_info.capacity,
           (float)g_flash_info.capacity / (1024 * 1024));
    printf("  Sector Size: %lu bytes\n", g_flash_info.sector_size);
    printf("  Number of Sectors: %lu\n", g_flash_info.num_sectors);

    // 步骤 3: 擦除测试
    printf("\n[Step 3] Erase test...\n");
    xilinx_sector_erase(0);

    // 步骤 4: 编程测试
    printf("\n[Step 4] Program test...\n");
    for (i = 0; i < 16; i += 2) {
        xilinx_program_word(i, test_data[i] | (test_data[i + 1] << 8));
    }

    // 步骤 5: 读取验证
    printf("\n[Step 5] Read verification...\n");
    for (i = 0; i < 16; i += 2) {
        uint16_t word = xilinx_read_word(i);
        read_buffer[i] = (uint8_t)(word & 0xFF);
        read_buffer[i + 1] = (uint8_t)((word >> 8) & 0xFF);
    }

    // 验证数据
    if (memcmp(test_data, read_buffer, 16) == 0) {
        printf("Program/Read test PASSED\n");
    } else {
        printf("Program/Read test FAILED\n");
    }

    // 步骤 6: 级联检测
    printf("\n[Step 6] Cascade detection...\n");
    detect_cascade_flash();

    printf("\n=== Demo Complete ===\n");
    return 0;
}

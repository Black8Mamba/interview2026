/**
 * @file sector_erase.c
 * @brief SPI Flash 扇区擦除操作示例
 *
 * 本示例演示扇区擦除功能的使用方法：
 * - 扇区擦除命令发送
 * - 等待擦除完成（轮询状态寄存器）
 * - 全片擦除操作
 * - 擦除验证方法
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// SPI Flash 命令定义
#define CMD_WRITE_ENABLE        0x06
#define CMD_WRITE_DISABLE       0x04
#define CMD_READ_STATUS_REG1    0x05
#define CMD_READ_STATUS_REG2    0x35
#define CMD_WRITE_STATUS_REG    0x01
#define CMD_PAGE_PROGRAM        0x02
#define CMD_QUAD_PAGE_PROGRAM   0x32
#define CMD_BLOCK_ERASE_64K     0xD8
#define CMD_BLOCK_ERASE_32K     0x52
#define CMD_SECTOR_ERASE_4K     0x20
#define CMD_CHIP_ERASE          0xC7    // 或 0x60
#define CMD_READ_DATA           0x03
#define CMD_FAST_READ           0x0B
#define CMD_READ_ID              0x9F
#define CMD_READ_SFDP           0x5A
#define CMD_ENABLE_RESET        0x66
#define CMD_RESET               0x99

// 状态寄存器位定义
#define STATUS_SR0_BUSY         0x01    // 正在写入/擦除
#define STATUS_SR0_WEL          0x02    // 写使能锁存
#define STATUS_SR0_BP0          0x04    // 块保护位0
#define STATUS_SR0_BP1          0x08    // 块保护位1
#define STATUS_SR0_BP2          0x10    // 块保护位2
#define STATUS_SR0_TB           0x20    // 顶部/底部保护
#define STATUS_SR0_SEC          0x40    // 扇区/块保护
#define STATUS_SR0_CMP          0x80    // 互补保护

// Flash 配置（需根据实际芯片修改）
#define FLASH_SECTOR_SIZE       4096    // 4KB 扇区
#define FLASH_BLOCK_SIZE        65536   // 64KB 块
#define FLASH_PAGE_SIZE         256     // 256B 页

// 函数原型
void spi_init(void);
void spi_transfer(uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);
uint8_t spi_read_status(void);
void spi_write_enable(void);
void spi_wait_busy(uint32_t timeout_ms);
void spi_sector_erase(uint32_t addr);
void spi_block_erase_32k(uint32_t addr);
void spi_block_erase_64k(uint32_t addr);
void spi_chip_erase(void);
int verify_erased(uint32_t addr, uint32_t len);

/**
 * @brief SPI 初始化
 */
void spi_init(void)
{
    // TODO: 初始化 SPI 外设
}

/**
 * @brief SPI 数据传输
 */
void spi_transfer(uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len)
{
    // TODO: 实现 SPI 传输
}

/**
 * @brief 读取状态寄存器
 */
uint8_t spi_read_status(void)
{
    uint8_t tx_buf[2] = {CMD_READ_STATUS_REG1, 0xFF};
    uint8_t rx_buf[2];

    spi_transfer(tx_buf, rx_buf, 2);
    return rx_buf[1];
}

/**
 * @brief 发送写使能命令
 */
void spi_write_enable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    spi_transfer(&cmd, NULL, 1);
}

/**
 * @brief 等待 Flash 空闲
 * @param timeout_ms 超时时间（毫秒）
 */
void spi_wait_busy(uint32_t timeout_ms)
{
    uint32_t start = 0; // TODO: 获取系统时间

    while (spi_read_status() & STATUS_SR0_BUSY) {
        // TODO: 检查超时
    }
}

/**
 * @brief 4KB 扇区擦除
 * @param addr 扇区起始地址（必须对齐到扇区边界）
 */
void spi_sector_erase(uint32_t addr)
{
    uint8_t cmd[4];

    // 发送写使能
    spi_write_enable();

    // 发送扇区擦除命令
    cmd[0] = CMD_SECTOR_ERASE_4K;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;

    spi_transfer(cmd, NULL, 4);

    // 等待擦除完成
    spi_wait_busy(500); // 扇区擦除通常需要 30-400ms
}

/**
 * @brief 32KB 块擦除
 * @param addr 块起始地址
 */
void spi_block_erase_32k(uint32_t addr)
{
    uint8_t cmd[4];

    spi_write_enable();

    cmd[0] = CMD_BLOCK_ERASE_32K;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;

    spi_transfer(cmd, NULL, 4);

    spi_wait_busy(1000); // 块擦除通常需要 200-1000ms
}

/**
 * @brief 64KB 块擦除
 * @param addr 块起始地址
 */
void spi_block_erase_64k(uint32_t addr)
{
    uint8_t cmd[4];

    spi_write_enable();

    cmd[0] = CMD_BLOCK_ERASE_64K;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;

    spi_transfer(cmd, NULL, 4);

    spi_wait_busy(1000); // 64KB 块擦除通常需要 200-1000ms
}

/**
 * @brief 全片擦除
 * @note 此操作会擦除整个 Flash 芯片，时间较长
 */
void spi_chip_erase(void)
{
    uint8_t cmd = CMD_CHIP_ERASE;

    spi_write_enable();
    spi_transfer(&cmd, NULL, 1);

    // 全片擦除可能需要 10-100 秒
    spi_wait_busy(120000);
}

/**
 * @brief 验证区域是否已擦除
 * @param addr 起始地址
 * @param len 验证长度
 * @return 0: 已擦除 非0: 未擦除
 */
int verify_erased(uint32_t addr, uint32_t len)
{
    uint8_t *verify_buffer;
    uint32_t i;

    verify_buffer = (uint8_t *)malloc(len);
    if (!verify_buffer) {
        return -1;
    }

    // TODO: 实现读取功能
    // spi_read_data(addr, verify_buffer, len);

    // 检查是否全部为 0xFF
    for (i = 0; i < len; i++) {
        if (verify_buffer[i] != 0xFF) {
            free(verify_buffer);
            return -2;
        }
    }

    free(verify_buffer);
    return 0;
}

/**
 * @brief 擦除指定地址范围的示例
 */
void erase_address_range(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t addr;

    // 地址对齐到扇区边界
    start_addr = (start_addr / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;

    for (addr = start_addr; addr < end_addr; addr += FLASH_SECTOR_SIZE) {
        printf("Erasing sector at 0x%08X...\n", addr);
        spi_sector_erase(addr);

        // 验证擦除结果
        if (verify_erased(addr, FLASH_SECTOR_SIZE) != 0) {
            printf("Warning: Sector at 0x%08X erase verification failed!\n", addr);
        }
    }
}

/**
 * @brief 主函数 - 扇区擦除示例
 */
int main(void)
{
    printf("=== SPI Flash Sector Erase Demo ===\n");

    // 初始化 SPI
    spi_init();

    // 示例1: 擦除单个扇区
    printf("\nErasing sector at 0x000000...\n");
    spi_sector_erase(0);
    printf("Sector erase completed.\n");

    // 验证扇区已擦除
    if (verify_erased(0, FLASH_SECTOR_SIZE) == 0) {
        printf("Verify: Sector is erased.\n");
    }

    // 示例2: 擦除多个连续扇区
    printf("\nErasing sectors from 0x1000 to 0x2000...\n");
    erase_address_range(0x1000, 0x2000);
    printf("Range erase completed.\n");

    // 示例3: 使用块擦除（更高效）
    printf("\nErasing 64KB block at 0x10000...\n");
    spi_block_erase_64k(0x10000);
    printf("Block erase completed.\n");

    printf("\n=== Demo Complete ===\n");
    return 0;
}

/**
 * @file basic_read_write.c
 * @brief SPI Flash 基本读写操作示例
 *
 * 本示例展示 SPI Flash 的基础读写功能，包括：
 * - 芯片初始化和配置
 * - 字节/页/批量数据读取
 * - 字节/页/批量数据写入
 * - 读取状态寄存器判断操作完成
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// SPI Flash 命令定义
#define CMD_READ_ID         0x9F    // 读取芯片ID
#define CMD_READ_STATUS     0x05    // 读取状态寄存器
#define CMD_WRITE_ENABLE    0x06    // 写使能
#define CMD_PAGE_PROGRAM    0x02    // 页编程
#define CMD_READ_DATA       0x03    // 读取数据
#define CMD_CHIP_ERASE      0xC7    // 全片擦除

// 状态寄存器位定义
#define STATUS_BUSY         (1 << 0)   // 忙碌标志
#define STATUS_WEL          (1 << 1)   // 写使能锁存

// SPI Flash 基础配置（需根据实际硬件修改）
#define SPI_FLASH_CS_PORT   0
#define SPI_FLASH_CS_PIN    0
#define SPI_FLASH_PAGE_SIZE 256     // 页大小
#define SPI_FLASH_SECTOR_SIZE 4096  // 扇区大小

// 函数原型声明
void spi_flash_init(void);
void spi_flash_cs_select(uint8_t select);
uint8_t spi_flash_send_byte(uint8_t data);
void spi_flash_read_id(uint8_t *manufacturer, uint16_t *device_id);
uint8_t spi_flash_read_status(void);
void spi_flash_write_enable(uint8_t enable);
void spi_flash_page_program(uint32_t addr, const uint8_t *data, uint16_t len);
void spi_flash_read_data(uint32_t addr, uint8_t *data, uint32_t len);
void spi_flash_sector_erase(uint32_t addr);
void spi_flash_chip_erase(void);
void spi_flash_wait_busy(void);

/**
 * @brief SPI Flash 初始化
 */
void spi_flash_init(void)
{
    // TODO: 实现 SPI 外设初始化
    // 1. 配置 GPIO 引脚
    // 2. 配置 SPI 模式（时钟极性、相位等）
    // 3. 设置时钟频率（通常不超过芯片支持的最大频率）
}

/**
 * @brief 片选信号控制
 * @param select 1: 选中 0: 取消选中
 */
void spi_flash_cs_select(uint8_t select)
{
    // TODO: 实现片选控制
}

/**
 * @brief SPI 字节发送/接收
 * @param data 要发送的数据
 * @return 接收到的数据
 */
uint8_t spi_flash_send_byte(uint8_t data)
{
    // TODO: 实现 SPI 数据传输
    return 0;
}

/**
 * @brief 读取 Flash 芯片 ID
 * @param manufacturer 存储制造商ID
 * @param device_id 存储设备ID
 */
void spi_flash_read_id(uint8_t *manufacturer, uint16_t *device_id)
{
    spi_flash_cs_select(1);
    spi_flash_send_byte(CMD_READ_ID);
    *manufacturer = spi_flash_send_byte(0xFF);
    *device_id = (spi_flash_send_byte(0xFF) << 8) | spi_flash_send_byte(0xFF);
    spi_flash_cs_select(0);
}

/**
 * @brief 读取状态寄存器
 * @return 状态寄存器值
 */
uint8_t spi_flash_read_status(void)
{
    uint8_t status;
    spi_flash_cs_select(1);
    spi_flash_send_byte(CMD_READ_STATUS);
    status = spi_flash_send_byte(0xFF);
    spi_flash_cs_select(0);
    return status;
}

/**
 * @brief 等待 Flash 忙碌标志清除
 */
void spi_flash_wait_busy(void)
{
    while (spi_flash_read_status() & STATUS_BUSY) {
        // 等待中...
    }
}

/**
 * @brief 写使能/禁止
 * @param enable 1: 使能 0: 禁止
 */
void spi_flash_write_enable(uint8_t enable)
{
    spi_flash_cs_select(1);
    spi_flash_send_byte(CMD_WRITE_ENABLE);
    spi_flash_cs_select(0);
}

/**
 * @brief 页编程（写入最多一页数据）
 * @param addr 起始地址
 * @param data 数据缓冲区
 * @param len 数据长度（不超过页大小）
 */
void spi_flash_page_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    spi_flash_write_enable(1);
    spi_flash_cs_select(1);
    spi_flash_send_byte(CMD_PAGE_PROGRAM);
    spi_flash_send_byte((addr >> 16) & 0xFF);
    spi_flash_send_byte((addr >> 8) & 0xFF);
    spi_flash_send_byte(addr & 0xFF);

    for (uint16_t i = 0; i < len; i++) {
        spi_flash_send_byte(data[i]);
    }

    spi_flash_cs_select(0);
    spi_flash_wait_busy();
}

/**
 * @brief 读取数据
 * @param addr 起始地址
 * @param data 数据缓冲区
 * @param len 读取长度
 */
void spi_flash_read_data(uint32_t addr, uint8_t *data, uint32_t len)
{
    spi_flash_cs_select(1);
    spi_flash_send_byte(CMD_READ_DATA);
    spi_flash_send_byte((addr >> 16) & 0xFF);
    spi_flash_send_byte((addr >> 8) & 0xFF);
    spi_flash_send_byte(addr & 0xFF);

    for (uint32_t i = 0; i < len; i++) {
        data[i] = spi_flash_send_byte(0xFF);
    }

    spi_flash_cs_select(0);
}

/**
 * @brief 扇区擦除（4KB扇区）
 * @param addr 扇区地址
 */
void spi_flash_sector_erase(uint32_t addr)
{
    // TODO: 实现扇区擦除命令
    // CMD_SECTOR_ERASE = 0x20
}

/**
 * @brief 全片擦除
 */
void spi_flash_chip_erase(void)
{
    // TODO: 实现全片擦除命令
}

/**
 * @brief 主函数 - 示例演示
 */
int main(void)
{
    uint8_t manufacturer;
    uint16_t device_id;
    uint8_t write_buffer[256];
    uint8_t read_buffer[256];

    // 初始化 SPI Flash
    spi_flash_init();

    // 读取芯片ID
    spi_flash_read_id(&manufacturer, &device_id);
    printf("Flash ID: 0x%02X, 0x%04X\n", manufacturer, device_id);

    // 准备测试数据
    memset(write_buffer, 0xAA, sizeof(write_buffer));

    // 扇区擦除
    spi_flash_sector_erase(0);

    // 页编程写入
    spi_flash_page_program(0, write_buffer, 256);

    // 读取数据
    spi_flash_read_data(0, read_buffer, 256);

    // 验证数据
    if (memcmp(write_buffer, read_buffer, 256) == 0) {
        printf("Read/Write test PASSED\n");
    } else {
        printf("Read/Write test FAILED\n");
    }

    return 0;
}

/**
 * @file quad_mode.c
 * @brief SPI Flash 四线模式（QUAD SPI）示例
 *
 * 本示例展示如何配置和使用四线 SPI 模式：
 * - 进入四线模式（QE 位配置）
 * - 四线快速读取命令
 * - 四线页写入
 * - 性能对比测试
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// SPI Flash 命令定义
#define CMD_READ_ID              0x9F
#define CMD_READ_STATUS_REG1     0x05
#define CMD_READ_STATUS_REG2     0x35
#define CMD_WRITE_STATUS_REG     0x01
#define CMD_WRITE_ENABLE         0x06
#define CMD_QUAD_PAGE_PROGRAM    0x32    // 四线页编程
#define CMD_QUAD_IO_FAST_READ    0xEB    // 四线快速读取
#define CMD_QUAD_IO_READ         0xE8    // 四线 IO 读取
#define CMD_READ_DATA            0x03    // 单线读取
#define CMD_FAST_READ            0x0B    // 单线快速读取
#define CMD_PAGE_PROGRAM         0x02    // 单线页编程

// 状态寄存器位定义
#define STATUS_SR0_BUSY          0x01
#define STATUS_SR0_WEL           0x02
#define STATUS_SR1_QE            0x01    // Quad Enable 位

// Flash 配置
#define FLASH_SECTOR_SIZE        4096
#define FLASH_PAGE_SIZE          256

// QUAD 模式配置结构
typedef struct {
    uint8_t enabled;
    uint8_t dummy_cycles;      // 虚拟周期数
    uint32_t read_cmd;         // 读取命令
    uint32_t program_cmd;       // 编程命令
} quad_config_t;

// 函数原型
void spi_init(void);
void spi_set_quad_mode(uint8_t enable);
uint8_t spi_read_status2(void);
void spi_write_status(uint8_t sr1, uint8_t sr2);
void spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);
void spi_wait_busy(void);
void quad_page_program(uint32_t addr, const uint8_t *data, uint16_t len);
void quad_fast_read(uint32_t addr, uint8_t *data, uint32_t len);
void single_page_program(uint32_t addr, const uint8_t *data, uint16_t len);
void single_fast_read(uint32_t addr, uint8_t *data, uint32_t len);
uint32_t measure_read_speed(uint32_t addr, uint32_t len);
uint32_t measure_program_speed(uint32_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief SPI 初始化
 */
void spi_init(void)
{
    // TODO: 初始化 SPI 外设
    // 建议配置:
    // - 模式: Mode 0 或 Mode 3
    // - 时钟: 根据芯片支持的最大频率
    // - 位宽: 初始为单线模式
}

/**
 * @brief 读取状态寄存器2
 */
uint8_t spi_read_status2(void)
{
    uint8_t tx_buf[2] = {CMD_READ_STATUS_REG2, 0xFF};
    uint8_t rx_buf[2];

    spi_transfer(tx_buf, rx_buf, 2);
    return rx_buf[1];
}

/**
 * @brief 写入状态寄存器（使能 Quad 模式）
 * @param sr1 状态寄存器1值
 * @param sr2 状态寄存器2值
 */
void spi_write_status(uint8_t sr1, uint8_t sr2)
{
    uint8_t cmd[3] = {CMD_WRITE_STATUS_REG, sr1, sr2};

    // 发送写使能
    uint8_t write_en = CMD_WRITE_ENABLE;
    spi_transfer(&write_en, NULL, 1);

    // 写入状态寄存器
    spi_transfer(cmd, NULL, 3);

    // 等待写入完成
    spi_wait_busy();
}

/**
 * @brief 启用/禁用 Quad 模式
 * @param enable 1: 启用 0: 禁用
 */
void spi_set_quad_mode(uint8_t enable)
{
    uint8_t sr1, sr2;

    sr1 = spi_read_status();   // 读取 SR1
    sr2 = spi_read_status2();  // 读取 SR2

    if (enable) {
        sr2 |= STATUS_SR1_QE;  // 设置 QE 位
    } else {
        sr2 &= ~STATUS_SR1_QE; // 清除 QE 位
    }

    spi_write_status(sr1, sr2);

    // 验证设置
    sr2 = spi_read_status2();
    if (enable && (sr2 & STATUS_SR1_QE)) {
        printf("Quad mode enabled successfully.\n");
    } else if (!enable && !(sr2 & STATUS_SR1_QE)) {
        printf("Quad mode disabled successfully.\n");
    } else {
        printf("Warning: Quad mode configuration may failed!\n");
    }
}

/**
 * @brief SPI 数据传输（支持4线模式）
 */
void spi_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len)
{
    // TODO: 实现 SPI 传输
    // Quad 模式下需要配置 MOSI 为双向或使用 4 条数据线
}

/**
 * @brief 等待 Flash 空闲
 */
void spi_wait_busy(void)
{
    while (spi_read_status() & STATUS_SR0_BUSY) {
        // 等待
    }
}

/**
 * @brief 读取状态寄存器1
 */
uint8_t spi_read_status(void)
{
    uint8_t tx_buf[2] = {CMD_READ_STATUS_REG1, 0xFF};
    uint8_t rx_buf[2];

    spi_transfer(tx_buf, rx_buf, 2);
    return rx_buf[1];
}

/**
 * @brief Quad 模式页编程
 * @param addr 起始地址
 * @param data 数据缓冲区
 * @param len 数据长度
 */
void quad_page_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    uint8_t cmd[4];
    uint32_t i;

    // 发送写使能
    uint8_t write_en = CMD_WRITE_ENABLE;
    spi_transfer(&write_en, NULL, 1);

    // 发送 Quad 页编程命令
    cmd[0] = CMD_QUAD_PAGE_PROGRAM;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;

    spi_transfer(cmd, NULL, 4);

    // 发送数据（4位并行）
    for (i = 0; i < len; i++) {
        spi_transfer(&data[i], NULL, 1);
    }

    // 等待编程完成
    spi_wait_busy();
}

/**
 * @brief Quad 模式快速读取
 * @param addr 起始地址
 * @param data 数据缓冲区
 * @param len 读取长度
 */
void quad_fast_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    uint8_t cmd[6];  // 命令 + 地址 + 虚拟字节
    uint32_t i;

    // 发送 Quad 快速读取命令
    cmd[0] = CMD_QUAD_IO_FAST_READ;
    cmd[1] = (addr >> 16) & 0xFF;
    cmd[2] = (addr >> 8) & 0xFF;
    cmd[3] = addr & 0xFF;
    cmd[4] = 0x00;  // 虚拟周期 (根据芯片手册设置)
    cmd[5] = 0x00;  // 虚拟周期

    spi_transfer(cmd, NULL, 6);

    // 接收数据（4位并行）
    for (i = 0; i < len; i++) {
        spi_transfer(NULL, &data[i], 1);
    }
}

/**
 * @brief 单线模式页编程（对比用）
 */
void single_page_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    // TODO: 实现单线页编程
}

/**
 * @brief 单线模式快速读取（对比用）
 */
void single_fast_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    // TODO: 实现单线快速读取
}

/**
 * @brief 测试读取速度
 * @param addr 测试地址
 * @param len 读取数据长度
 * @return 速度 (bytes/ms)
 */
uint32_t measure_read_speed(uint32_t addr, uint32_t len)
{
    // TODO: 实现速度测量
    return 0;
}

/**
 * @brief 测试编程速度
 * @param addr 测试地址
 * @param data 数据
 * @param len 数据长度
 * @return 速度 (bytes/ms)
 */
uint32_t measure_program_speed(uint32_t addr, const uint8_t *data, uint16_t len)
{
    // TODO: 实现速度测量
    return 0;
}

/**
 * @brief 主函数 - Quad 模式示例
 */
int main(void)
{
    uint8_t test_data[FLASH_PAGE_SIZE];
    uint8_t read_buffer[FLASH_PAGE_SIZE];
    uint32_t i;

    printf("=== SPI Flash Quad Mode Demo ===\n");

    // 初始化 SPI
    spi_init();

    // 填充测试数据
    for (i = 0; i < FLASH_PAGE_SIZE; i++) {
        test_data[i] = (uint8_t)i;
    }

    // ============ 步骤1: 启用 Quad 模式 ============
    printf("\n[Step 1] Enable Quad Mode...\n");
    spi_set_quad_mode(1);

    // ============ 步骤2: Quad 模式读取 ID ============
    printf("\n[Step 2] Read Flash ID in Quad Mode...\n");
    // TODO: 实现 Quad 模式读取 ID

    // ============ 步骤3: Quad 模式页编程 ============
    printf("\n[Step 3] Quad Page Program...\n");
    quad_page_program(0, test_data, FLASH_PAGE_SIZE);
    printf("Quad page program completed.\n");

    // ============ 步骤4: Quad 模式读取 ============
    printf("\n[Step 4] Quad Fast Read...\n");
    quad_fast_read(0, read_buffer, FLASH_PAGE_SIZE);

    // 验证数据
    if (memcmp(test_data, read_buffer, FLASH_PAGE_SIZE) == 0) {
        printf("Quad mode read/write verification PASSED.\n");
    } else {
        printf("Quad mode read/write verification FAILED.\n");
    }

    // ============ 步骤5: 性能对比 ============
    printf("\n[Step 5] Performance Comparison...\n");

    // 单线模式速度测试
    // TODO: 禁用 Quad 模式进行对比测试

    // Quad 模式速度测试
    // TODO: 测量 Quad 模式速度

    printf("\n=== Demo Complete ===\n");
    return 0;
}

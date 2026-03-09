/**
 * @file fsmc_basic.c
 * @brief FSMC 控制器基本操作示例
 *
 * 本示例展示并口 Flash 与 FSMC 控制器的连接配置：
 * - FSMC 初始化配置
 * * 异步/同步读取模式
 * - 读写时序配置
 * - 内存映射访问方式
 *
 * 适用于 STM32 等具备 FSMC 接口的 MCU
 */

#include <stdio.h>
#include <stdint.h>

// FSMC 区域定义
#define FSMC_BANK1_ADDR       0x60000000  // Bank1 起始地址
#define FSMC_BANK1_SIZE       0x04000000  // 64MB 地址空间
#define FLASH_BASE_ADDR       FSMC_BANK1_ADDR

// FSMC 寄存器结构 (根据具体 MCU 修改)
typedef struct {
    volatile uint32_t BC1;     // Bank1 控制寄存器
    volatile uint32_t BK1R1;   // Bank1 时序寄存器1
    volatile uint32_t BK1R2;   // Bank1 时序寄存器2
    volatile uint32_t BK1R3;   // Bank1 时序寄存器3
} FSMC_Bank1_TypeDef;

#define FSMC_BASE             0xA0000000
#define FSMC_BANK1            ((FSMC_Bank1_TypeDef *)(FSMC_BASE + 0x0000))

// FSMC 控制位定义
#define FSMC_BUS_WIDTH_16BIT  (1 << 4)
#define FSMC_WRITE_ENABLE     (1 << 12)
#define FSMC_NWAIT_ENABLE     (1 << 6)

// 时序参数 (单位: HCLK 时钟周期)
typedef struct {
    uint8_t addr_setup;       // 地址建立时间
    uint8_t addr_hold;         // 地址保持时间
    uint8_t data_setup;       // 数据建立时间
    uint8_t data_hold;        // 数据保持时间
    uint8_t bus_turnaround;   // 总线转换时间
} fsmc_timing_t;

// 函数原型
void fsmc_gpio_init(void);
void fsmc_controller_init(void);
void fsmc_timing_config(const fsmc_timing_t *timing);
void fsmc_memory_write(uint32_t addr, const uint8_t *data, uint32_t len);
void fsmc_memory_read(uint32_t addr, uint8_t *data, uint32_t len);
void fsmc_sector_erase(uint32_t addr);
void fsmc_wait_busy(void);

/**
 * @brief FSMC GPIO 初始化
 */
void fsmc_gpio_init(void)
{
    // TODO: 配置 FSMC 使用的 GPIO
    // STM32 示例:
    // - D0-D15: 数据总线
    // - A0-A25: 地址总线
    // - NBL0, NBL1: 字节掩码
    // - NE1: 片选信号
    // - NOE: 输出使能
    // - NWE: 写使能
    printf("FSMC GPIO initialized\n");
}

/**
 * @brief FSMC 控制器初始化
 */
void fsmc_controller_init(void)
{
    // TODO: 初始化 FSMC 控制器
    // 1. 使能 FSMC 时钟
    // 2. 配置 Bank1 为 16 位NOR/PSRAM 模式
    // 3. 配置时序

    // 基础配置
    FSMC_BANK1->BC1 = 0x00003011;  // 异步模式, 16位总线

    // 使能 Flash Bank
    FSMC_BANK1->BC1 |= (1 << 6);   // MBKEN - 存储块使能

    printf("FSMC controller initialized\n");
}

/**
 * @brief 配置 FSMC 时序
 * @param timing 时序参数结构体
 */
void fsmc_timing_config(const fsmc_timing_t *timing)
{
    uint32_t temp;

    // 配置时序寄存器1 (读时序)
    temp = 0;
    temp |= (timing->addr_setup - 1) << 8;   // ADDSET
    temp |= (timing->data_setup - 1) << 0;   // DATAST
    FSMC_BANK1->BK1R1 = temp;

    // 配置时序寄存器2 (写时序)
    temp = 0;
    temp |= (timing->addr_setup - 1) << 8;
    temp |= (timing->data_setup - 1) << 0;
    FSMC_BANK1->BK1R2 = temp;

    // 配置时序寄存器3
    temp = 0;
    temp |= (timing->bus_turnaround - 1) << 0;  // BUSTURN
    FSMC_BANK1->BK1R3 = temp;

    printf("FSMC timing configured:\n");
    printf("  Address Setup: %d cycles\n", timing->addr_setup);
    printf("  Data Setup: %d cycles\n", timing->data_setup);
    printf("  Bus Turnaround: %d cycles\n", timing->bus_turnaround);
}

/**
 * @brief 内存映射读取
 */
void fsmc_memory_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    volatile uint16_t *flash_ptr;
    uint32_t i;

    // 转换为 16 位指针
    flash_ptr = (volatile uint16_t *)(FLASH_BASE_ADDR + addr);

    for (i = 0; i < len / 2; i++) {
        data[i * 2] = (uint8_t)(flash_ptr[i] & 0xFF);
        data[i * 2 + 1] = (uint8_t)((flash_ptr[i] >> 8) & 0xFF);
    }

    // 处理奇数字节
    if (len % 2) {
        data[len - 1] = (uint8_t)(flash_ptr[len / 2] & 0xFF);
    }
}

/**
 * @brief 内存映射写入
 */
void fsmc_memory_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    volatile uint16_t *flash_ptr;
    uint32_t i;

    flash_ptr = (volatile uint16_t *)(FLASH_BASE_ADDR + addr);

    for (i = 0; i < len / 2; i++) {
        flash_ptr[i] = (data[i * 2 + 1] << 8) | data[i * 2];
    }

    // 处理奇数字节
    if (len % 2) {
        flash_ptr[len / 2] = data[len - 1];
    }
}

/**
 * @brief 扇区擦除 (通过命令序列)
 */
void fsmc_sector_erase(uint32_t sector_addr)
{
    volatile uint16_t *flash_ptr;
    uint16_t *addr_ptr;

    flash_ptr = (volatile uint16_t *)FLASH_BASE_ADDR;
    addr_ptr = (uint16_t *)(FLASH_BASE_ADDR + sector_addr);

    // 发送解锁命令序列
    flash_ptr[0x555] = 0xAA;
    flash_ptr[0x2AA] = 0x55;

    // 发送擦除命令
    flash_ptr[0x555] = 0x80;
    flash_ptr[0x555] = 0xAA;
    flash_ptr[0x2AA] = 0x55;

    // 确认扇区擦除
    *addr_ptr = 0x30;

    // 等待擦除完成
    fsmc_wait_busy();
}

/**
 * @brief 等待 Flash 空闲
 */
void fsmc_wait_busy(void)
{
    volatile uint16_t *flash_ptr;
    uint16_t status;

    flash_ptr = (volatile uint16_t *)FLASH_BASE_ADDR;

    do {
        // 读取状态寄存器 (CFI 模式下)
        *addr_ptr = 0x70;
        status = flash_ptr[0];
    } while (status & 0x80);  // 检查 busy 位
}

/**
 * @brief 主函数 - FSMC 基本操作示例
 */
int main(void)
{
    fsmc_timing_t timing = {
        .addr_setup = 5,
        .addr_hold = 2,
        .data_setup = 8,
        .data_hold = 2,
        .bus_turnaround = 2
    };

    uint8_t write_buffer[256];
    uint8_t read_buffer[256];
    uint32_t i;

    printf("=== FSMC Basic Operations Demo ===\n\n");

    // 初始化 FSMC
    fsmc_gpio_init();
    fsmc_controller_init();

    // 配置时序
    fsmc_timing_config(&timing);

    // 准备测试数据
    for (i = 0; i < 256; i++) {
        write_buffer[i] = (uint8_t)i;
    }

    // 扇区擦除
    printf("\nErasing sector at 0x000000...\n");
    fsmc_sector_erase(0);

    // 写入数据
    printf("Writing data...\n");
    fsmc_memory_write(0, write_buffer, 256);

    // 读取数据
    printf("Reading data...\n");
    fsmc_memory_read(0, read_buffer, 256);

    // 验证数据
    if (memcmp(write_buffer, read_buffer, 256) == 0) {
        printf("\nRead/Write test PASSED\n");
    } else {
        printf("\nRead/Write test FAILED\n");
    }

    printf("\n=== Demo Complete ===\n");
    return 0;
}

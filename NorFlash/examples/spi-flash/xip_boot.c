/**
 * @file xip_boot.c
 * @brief XIP (Execute-In-Place) 启动示例
 *
 * 本示例实现代码原地执行功能：
 * - 将 Flash 映射到内存地址空间
 * - 配置 XIP 模式
 * - 从 Flash 直接启动程序
 * - XIP 性能优化技巧
 *
 * 适用于嵌入式系统的无盘启动方案
 */

#include <stdio.h>
#include <stdint.h>

// XIP 模式配置
#define XIP_BASE_ADDRESS         0x90000000  // Flash 映射起始地址（根据硬件配置）
#define FLASH_TOTAL_SIZE         (8 * 1024 * 1024)  // 8MB Flash

// Flash 命令定义
#define CMD_READ_ID              0x9F
#define CMD_READ_STATUS          0x05
#define CMD_WRITE_ENABLE         0x06
#define CMD_ENTER_XIP            0xFF    // 进入 XIP 模式
#define CMD_EXIT_XIP             0x00    // 退出 XIP 模式
#define CMD_FAST_READ            0x0B
#define CMD_DUAL_IO_FAST_READ    0xBB    // 双线模式
#define CMD_QUAD_IO_FAST_READ    0xEB    // 四线模式

// XIP 配置结构
typedef struct {
    uint32_t base_address;      // XIP 基地址
    uint8_t mode;                // XIP 模式 (0:单线, 1:双线, 2:四线)
    uint8_t dummy_cycles;       // 虚拟周期
    uint8_t enabled;             // XIP 使能状态
} xip_config_t;

// XIP 当前配置
static xip_config_t g_xip_config = {
    .base_address = XIP_BASE_ADDRESS,
    .mode = 2,       // 默认使用四线模式
    .dummy_cycles = 6,
    .enabled = 0
};

// 函数原型
void xip_init(void);
void xip_enable(void);
void xip_disable(void);
void xip_configure(uint8_t mode, uint8_t dummy_cycles);
void xip_set_base_address(uint32_t addr);
void xip_read(uint32_t addr, void *buffer, uint32_t len);
void xip_verify_mode(void);
void xip_memory_test(void);
void xip_performance_test(uint32_t addr, uint32_t size);

/**
 * @brief XIP 系统初始化
 */
void xip_init(void)
{
    // TODO: 初始化硬件相关配置
    // 1. 配置 Quad SPI 控制器
    // 2. 设置 Flash 映射地址
    // 3. 配置时序参数

    printf("XIP System Initialized\n");
    printf("  Base Address: 0x%08X\n", g_xip_config.base_address);
    printf("  Flash Size: %d MB\n", FLASH_TOTAL_SIZE / (1024 * 1024));
}

/**
 * @brief 启用 XIP 模式
 */
void xip_enable(void)
{
    // TODO: 配置 FMC/FSMC 或 QuadSPI 控制器进入 XIP 模式

    g_xip_config.enabled = 1;

    // 设置控制寄存器进入 XIP 模式
    // TODO: 根据具体硬件配置

    printf("XIP Mode Enabled\n");
}

/**
 * @brief 禁用 XIP 模式
 */
void xip_disable(void)
{
    // TODO: 退出 XIP 模式

    g_xip_config.enabled = 0;

    printf("XIP Mode Disabled\n");
}

/**
 * @brief 配置 XIP 参数
 * @param mode XIP 模式 (0:单线, 1:双线, 2:四线)
 * @param dummy_cycles 虚拟周期数
 */
void xip_configure(uint8_t mode, uint8_t dummy_cycles)
{
    if (mode > 2) {
        printf("Error: Invalid XIP mode\n");
        return;
    }

    g_xip_config.mode = mode;
    g_xip_config.dummy_cycles = dummy_cycles;

    // 重新配置 XIP 参数
    // TODO: 更新寄存器配置

    printf("XIP Configured: Mode=%d, Dummy Cycles=%d\n", mode, dummy_cycles);
}

/**
 * @brief 设置 XIP 基地址
 * @param addr 新的基地址
 */
void xip_set_base_address(uint32_t addr)
{
    // 地址必须对齐到 Flash 大小边界
    if (addr >= FLASH_TOTAL_SIZE) {
        printf("Error: Address exceeds Flash size\n");
        return;
    }

    g_xip_config.base_address = 0x90000000 | addr;

    // TODO: 更新硬件映射

    printf("XIP Base Address: 0x%08X\n", g_xip_config.base_address);
}

/**
 * @brief XIP 模式读取数据（通过内存映射）
 * @param addr Flash 地址
 * @param buffer 读取缓冲区
 * @param len 读取长度
 */
void xip_read(uint32_t addr, void *buffer, uint32_t len)
{
    uint32_t *src;
    uint32_t *dst;
    uint32_t i;

    if (!g_xip_config.enabled) {
        printf("Warning: XIP not enabled, using slow read\n");
        // TODO: 使用普通 SPI 读取
        return;
    }

    // 直接通过内存映射读取
    src = (uint32_t *)(g_xip_config.base_address + addr);
    dst = (uint32_t *)buffer;

    // 使用适当的数据宽度读取
    for (i = 0; i < len / 4; i++) {
        dst[i] = src[i];
    }

    // 处理剩余字节
    if (len % 4) {
        uint8_t *src_byte = (uint8_t *)src;
        uint8_t *dst_byte = (uint8_t *)dst;
        for (i = 0; i < len % 4; i++) {
            dst_byte[len - 4 + i] = src_byte[i];
        }
    }
}

/**
 * @brief 验证 XIP 模式配置
 */
void xip_verify_mode(void)
{
    printf("\n=== XIP Configuration ===\n");
    printf("Enabled: %s\n", g_xip_config.enabled ? "Yes" : "No");
    printf("Base Address: 0x%08X\n", g_xip_config.base_address);

    switch (g_xip_config.mode) {
        case 0: printf("Mode: Single SPI\n"); break;
        case 1: printf("Mode: Dual SPI\n"); break;
        case 2: printf("Mode: Quad SPI\n"); break;
        default: printf("Mode: Unknown\n"); break;
    }

    printf("Dummy Cycles: %d\n", g_xip_config.dummy_cycles);
    printf("========================\n\n");
}

/**
 * @brief XIP 内存测试
 */
void xip_memory_test(void)
{
    uint32_t test_addr = 0;
    uint32_t test_pattern = 0xDEADBEEF;
    uint32_t read_value;

    printf("Running XIP Memory Test...\n");

    // 注意: Flash 必须是已写入的才能读取
    // 这里假设某个地址已有有效数据

    // 读取测试
    xip_read(test_addr, &read_value, sizeof(read_value));
    printf("Read at 0x%08X: 0x%08X\n", test_addr, read_value);

    // 内存映射直接读取测试
    {
        volatile uint32_t *xip_ptr = (volatile uint32_t *)(g_xip_config.base_address + test_addr);
        read_value = *xip_ptr;
        printf("Direct XIP Read at 0x%08X: 0x%08X\n", test_addr, read_value);
    }

    printf("Memory Test Complete\n");
}

/**
 * @brief XIP 性能测试
 * @param addr 测试地址
 * @param size 测试数据大小
 */
void xip_performance_test(uint32_t addr, uint32_t size)
{
    uint8_t buffer[4096];
    uint32_t start_time, end_time;
    uint32_t i;
    volatile uint8_t *xip_ptr;

    printf("\n=== XIP Performance Test ===\n");
    printf("Test Address: 0x%08X\n", addr);
    printf("Test Size: %d bytes\n", size);

    // 预取使能测试
    // TODO: 使能 CPU 指令预取

    // 测试 1: 通过函数调用读取
    start_time = 0; // TODO: 获取系统时间
    for (i = 0; i < 1000; i++) {
        xip_read(addr, buffer, size);
    }
    end_time = 0; // TODO: 获取系统时间
    printf("Function Call Read: %d iterations in %d ms\n", 1000, end_time - start_time);

    // 测试 2: 直接内存映射读取
    xip_ptr = (volatile uint8_t *)(g_xip_config.base_address + addr);
    start_time = 0; // TODO: 获取系统时间
    for (i = 0; i < 1000; i++) {
        volatile uint8_t val = xip_ptr[0];
        (void)val;
    }
    end_time = 0; // TODO: 获取系统时间
    printf("Direct Memory Read: %d iterations in %d ms\n", 1000, end_time - start_time);

    // 测试 3: 批量连续读取
    start_time = 0; // TODO: 获取系统时间
    for (i = 0; i < size; i++) {
        volatile uint8_t val = xip_ptr[i];
        (void)val;
    }
    end_time = 0; // TODO: 获取系统时间
    printf("Sequential Read: %d bytes in %d ms\n", size, end_time - start_time);

    printf("=============================\n\n");
}

/**
 * @brief 代码跳转函数（跳转到 Flash 中的其他程序）
 * @param addr 目标地址
 */
void xip_jump_to(uint32_t addr)
{
    typedef void (*function_ptr_t)(void);
    function_ptr_t jump_func;

    // 确保地址对齐
    if (addr % 4 != 0) {
        printf("Error: Jump address must be 4-byte aligned\n");
        return;
    }

    // 获取函数指针
    jump_func = (function_ptr_t)(g_xip_config.base_address + addr);

    // 禁用中断
    // TODO: __disable_irq();

    // 刷新 CPU 缓存
    // TODO: SCB_InvalidateICache(); SCB_InvalidateDCache();

    // 跳转到目标地址
    printf("Jumping to XIP address: 0x%08X\n", addr);
    jump_func();

    // 如果返回到这里，说明跳转失败
    printf("Error: Jump failed, returned to caller\n");
}

/**
 * @brief 主函数 - XIP 启动示例
 */
int main(void)
{
    printf("=== XIP (Execute-In-Place) Boot Demo ===\n\n");

    // 初始化 XIP 系统
    xip_init();

    // 配置 XIP 为四线模式
    xip_configure(2, 6);

    // 验证配置
    xip_verify_mode();

    // 启用 XIP 模式
    xip_enable();

    // 运行内存测试
    xip_memory_test();

    // 运行性能测试
    xip_performance_test(0, 4096);

    // 示例: 跳转到 Flash 中的应用程序
    // xip_jump_to(0x1000);  // 跳转到偏移 0x1000 处的程序

    printf("=== Demo Complete ===\n");
    return 0;
}

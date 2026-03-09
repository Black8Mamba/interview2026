/**
 * @file sfdp_timing.c
 * @brief SFDP 时序配置示例
 *
 * 本示例基于 SFDP 参数配置硬件时序：
 * - 从 SFDP 提取时序参数
 * - 配置时钟分频
 * - 设置片选信号时序
 * - 动态调整时序优化性能
 */

#include <stdio.h>
#include <stdint.h>

// SFDP 时序参数偏移
#define SFDP_TIMING_PARAM_ADDR    0x30   // 第一个参数表地址

// 时序单位
#define NS_PER_US                 1000
#define NS_PER_MS                 1000000

// SPI 控制器配置结构
typedef struct {
    uint32_t clock_freq;           // 时钟频率 (Hz)
    uint8_t clock_div;            // 分频系数
    uint8_t cpha;                  // 时钟相位
    uint8_t cpol;                  // 时钟极性
    uint8_t cs_setup_ns;          // CS 建立时间
    uint8_t cs_hold_ns;           // CS 保持时间
    uint8_t cs_deselect_ns;       // CS 取消选择时间
} spi_timing_config_t;

// 读取时序参数
typedef struct {
    uint32_t t_rc;     // 读取周期时间
    uint32_t t_rr;     // 读取恢复时间
    uint32_t t_clch;   // 时钟低到数据有效
    uint32_t t_chcl;   // 时钟高到数据有效
    uint32_t t_dlch;   // 数据有效到时钟
    uint32_t t_chdx;   // 时钟无效到数据无效
} read_timing_t;

// 编程时序参数
typedef struct {
    uint32_t t_pp;    // 页编程时间
    uint32_t t_se;    // 扇区擦除时间
    uint32_t t_be;    // 块擦除时间
    uint32_t t_ce;    // 全片擦除时间
} program_timing_t;

// 全局时序配置
static spi_timing_config_t g_spi_timing;
static read_timing_t g_read_timing;
static program_timing_t g_program_timing;

/**
 * @brief 从 SFDP 数据中提取时序参数
 * @param sfdp_data SFDP 数据缓冲区
 */
void sfdp_extract_timing_params(const uint8_t *sfdp_data)
{
    uint32_t timing_dword;
    uint8_t timing_encoding;

    // 读取时序参数 (SFDP 参数表中的 DWORD 5-8)
    timing_dword = sfdp_data[20] | (sfdp_data[21] << 8) | (sfdp_data[22] << 16) | (sfdp_data[23] << 24);

    // 解析读取时序
    // bit 31:28 - 快速读取时钟周期编码
    // bit 27:24 - 快速读取数据延迟
    // bit 23:20 - 快速读取 DQS 延迟
    // bit 19:16 - 快速读取时钟延迟

    timing_encoding = (timing_dword >> 28) & 0x0F;

    printf("Timing Encoding: 0x%02X\n", timing_encoding);

    // 根据编码计算实际时序值
    // 这些值需要根据具体芯片手册转换

    // 编程时序参数 (从参数表的其他位置获取)
    // 这里使用典型值
    g_program_timing.t_pp = 500;      // 500us (典型页编程时间)
    g_program_timing.t_se = 400000;   // 400ms (典型扇区擦除时间)
    g_program_timing.t_be = 1000000; // 1s (典型块擦除时间)
    g_program_timing.t_ce = 50000000; // 50s (典型全片擦除时间)

    printf("\nProgram/Erase Timing:\n");
    printf("  Page Program: %lu us\n", g_program_timing.t_pp);
    printf("  Sector Erase: %lu ms\n", g_program_timing.t_se / 1000);
    printf("  Block Erase: %lu ms\n", g_program_timing.t_be / 1000);
    printf("  Chip Erase: %lu ms\n", g_program_timing.t_ce / 1000);
}

/**
 * @brief 计算最优时钟分频
 * @param target_freq 目标频率
 * @param apb_freq APB 时钟频率
 * @return 最优分频系数
 */
uint8_t calculate_clock_div(uint32_t target_freq, uint32_t apb_freq)
{
    uint8_t div;

    // 分频系数 = APB 时钟 / 目标时钟
    // 必须是 2 的幂次 (2, 4, 8, 16, ...)

    if (target_freq >= apb_freq) {
        return 1;  // 不分频
    }

    div = 2;
    while ((apb_freq / div) > target_freq && div < 256) {
        div *= 2;
    }

    return div;
}

/**
 * @brief 基于时序参数配置 SPI 时钟
 * @param max_freq 芯片支持的最大频率
 */
void configure_spi_clock(uint32_t max_freq)
{
    uint32_t apb_clock = 84000000;  // 假设 APB 时钟 84MHz
    uint8_t div;

    // 计算分频
    div = calculate_clock_div(max_freq, apb_clock);

    g_spi_timing.clock_div = div;
    g_spi_timing.clock_freq = apb_clock / div;

    printf("\nSPI Clock Configuration:\n");
    printf("  APB Clock: %lu Hz\n", apb_clock);
    printf("  Target Clock: %lu Hz\n", max_freq);
    printf("  Division: %d\n", div);
    printf("  Actual Clock: %lu Hz\n", g_spi_timing.clock_freq);
}

/**
 * @brief 配置片选信号时序
 * @param setup_ns CS 建立时间 (纳秒)
 * @param hold_ns CS 保持时间 (纳秒)
 * @param deselect_ns CS 取消选择时间 (纳秒)
 */
void configure_cs_timing(uint32_t setup_ns, uint32_t hold_ns, uint32_t deselect_ns)
{
    g_spi_timing.cs_setup_ns = (uint8_t)setup_ns;
    g_spi_timing.cs_hold_ns = (uint8_t)hold_ns;
    g_spi_timing.cs_deselect_ns = (uint8_t)deselect_ns;

    // TODO: 根据时序配置 GPIO 或 SPI 控制器寄存器

    printf("\nCS Timing Configuration:\n");
    printf("  Setup: %d ns\n", g_spi_timing.cs_setup_ns);
    printf("  Hold: %d ns\n", g_spi_timing.cs_hold_ns);
    printf("  Deselect: %d ns\n", g_spi_timing.cs_deselect_ns);
}

/**
 * @brief 根据时序参数验证配置是否合理
 * @return 0: 有效 非0: 需要调整
 */
int validate_timing_config(void)
{
    uint32_t clock_period_ns;
    int warnings = 0;

    // 计算时钟周期
    clock_period_ns = 1000000000 / g_spi_timing.clock_freq;

    printf("\n=== Timing Validation ===\n");
    printf("Clock Period: %lu ns\n", clock_period_ns);

    // 检查 CS 建立时间
    if (g_spi_timing.cs_setup_ns > 0 &&
        g_spi_timing.cs_setup_ns < clock_period_ns) {
        printf("Warning: CS setup time < 1 clock cycle\n");
        warnings++;
    }

    // 检查 CS 保持时间
    if (g_spi_timing.cs_hold_ns > 0 &&
        g_spi_timing.cs_hold_ns < clock_period_ns) {
        printf("Warning: CS hold time < 1 clock cycle\n");
        warnings++;
    }

    printf("Validation Result: %s\n", warnings == 0 ? "PASS" : "WARNINGS");

    return warnings;
}

/**
 * @brief 应用时序配置到硬件
 */
void apply_timing_config(void)
{
    printf("\n=== Applying Timing Configuration ===\n");

    // TODO: 配置 SPI 控制器
    // 1. 设置时钟分频
    // 2. 配置 CPOL/CPHA
    // 3. 配置 CS 时序
    // 4. 使能 FIFO/DMA (可选)

    printf("Timing configuration applied to hardware.\n");
}

/**
 * @brief 性能测试 - 测试不同频率下的读写
 */
void performance_test(uint32_t base_addr, uint32_t size)
{
    uint32_t test_freqs[] = {10000000, 25000000, 50000000, 80000000, 100000000};
    uint32_t freq;
    uint32_t i;

    printf("\n=== Performance Test ===\n");

    for (i = 0; i < sizeof(test_freqs) / sizeof(test_freqs[0]); i++) {
        freq = test_freqs[i];

        // 临时调整频率
        g_spi_timing.clock_freq = freq;
        g_spi_timing.clock_div = calculate_clock_div(freq, 84000000);

        // TODO: 执行读写测试并记录时间

        printf("Frequency: %lu MHz, ", freq / 1000000);
        printf("Read Speed: ", 0); // TODO: 实际测试值
        printf("MB/s\n");
    }
}

/**
 * @brief 动态时序优化
 */
void dynamic_timing_optimization(void)
{
    printf("\n=== Dynamic Timing Optimization ===\n");

    // 1. 从最高频率开始测试
    uint32_t current_freq = 100000000;  // 100MHz

    // 2. 执行读写测试
    // TODO: 实际测试

    // 3. 如果出现错误，降低频率
    // while (test_failed && current_freq > min_freq) {
    //     current_freq -= 10000000;
    //     configure_spi_clock(current_freq);
    //     if (test_passed) break;
    // }

    printf("Optimal frequency found: %lu MHz\n", current_freq / 1000000);
}

/**
 * @brief 主函数 - SFDP 时序配置示例
 */
int main(void)
{
    uint8_t sfdp_data[256];

    printf("=== SFDP Timing Configuration Demo ===\n\n");

    // 步骤 1: 读取 SFDP 数据
    printf("[Step 1] Reading SFDP data...\n");
    // TODO: 读取 SFDP 参数 步骤 2表

    //: 提取时序参数
    printf("\n[Step 2] Extracting timing parameters...\n");
    sfdp_extract_timing_params(sfdp_data);

    // 步骤 3: 配置 SPI 时钟
    printf("\n[Step 3] Configuring SPI clock...\n");
    configure_spi_clock(50000000);  // 50MHz

    // 步骤 4: 配置 CS 时序
    printf("\n[Step 4] Configuring CS timing...\n");
    configure_cs_timing(10, 10, 100);  // 10ns 建立, 10ns 保持, 100ns 取消选择

    // 步骤 5: 验证配置
    printf("\n[Step 5] Validating configuration...\n");
    validate_timing_config();

    // 步骤 6: 应用配置
    printf("\n[Step 6] Applying configuration...\n");
    apply_timing_config();

    // 步骤 7: 性能测试
    performance_test(0, 4096);

    // 步骤 8: 动态优化
    dynamic_timing_optimization();

    printf("\n=== Demo Complete ===\n");
    return 0;
}

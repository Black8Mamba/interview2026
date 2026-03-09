/**
 * @file cfi_timing.c
 * @brief CFI 时序配置示例
 *
 * 本示例基于 CFI 参数配置硬件时序：
 * - 从 CFI 提取时序参数
 * - 配置 FSMC 时序寄存器
 * - 设置建立/保持时间
 * - 时序优化和验证
 */

#include <stdio.h>
#include <stdint.h>

// FSMC 寄存器 (根据具体 MCU)
#define FSMC_BASE              0xA0000000
typedef struct {
    volatile uint32_t BC[4];
    volatile uint32_t BKxR[4][3];
} FSMC_TypeDef;
#define FSMC                   ((FSMC_TypeDef *)FSMC_BASE)

// Flash 配置
#define FLASH_BASE             0x60000000

// 时序参数 (HCLK 时钟周期, 假设 120MHz)
#define HCLK_NS                8.33   // 1 HCLK = 8.33ns @ 120MHz
#define DEFAULT_HCLK           120000000

// CFI 时序参数结构
typedef struct {
    uint32_t addr_setup;      // 地址建立时间 (ns)
    uint32_t addr_hold;       // 地址保持时间 (ns)
    uint32_t data_setup;      // 数据建立时间 (ns)
    uint32_t data_hold;       // 数据保持时间 (ns)
    uint32_t wait_pulse;      // WAIT 引脚脉冲宽度 (ns)
    uint32_t t_acc;           // 访问时间 (ns)
    uint32_t t_oe;            // 输出使能延迟 (ns)
} flash_timing_t;

// FSMC 配置
typedef struct {
    uint8_t add_set;          // ADDSET [0-15]
    uint8_t data_st;          // DATAST [0-255]
    uint8_t bus_turn;         // BUSTURN [0-15]
    uint8_t clk_div;          // 时钟分频
} fsmc_timing_config_t;

// 全局配置
static fsmc_timing_config_t g_fsmc_config;
static flash_timing_t g_flash_timing;

/**
 * @brief 从 CFI 读取时序参数
 */
void cfi_read_timing_params(flash_timing_t *timing)
{
    volatile uint16_t *flash;

    // 进入 CFI 模式
    flash = (volatile uint16_t *)FLASH_BASE;
    flash[0x55] = 0x98;

    // 读取时序参数
    // TODO: 从 CFI 参数表读取实际值
    // 这里使用典型值作为示例

    // 典型异步 NOR Flash 时序
    timing->t_acc = 150;      // 访问时间 150ns
    timing->t_oe = 30;        // 输出使能延迟 30ns
    timing->addr_setup = 10; // 地址建立时间
    timing->addr_hold = 20;  // 地址保持时间
    timing->data_setup = 50; // 数据建立时间
    timing->data_hold = 10;  // 数据保持时间
    timing->wait_pulse = 100; // WAIT 脉冲宽度

    // 退出 CFI 模式
    flash[0] = 0xF0;

    printf("Flash Timing Parameters (from CFI):\n");
    printf("  Access Time: %lu ns\n", timing->t_acc);
    printf("  OE Delay: %lu ns\n", timing->t_oe);
    printf("  Address Setup: %lu ns\n", timing->addr_setup);
    printf("  Address Hold: %lu ns\n", timing->addr_hold);
    printf("  Data Setup: %lu ns\n", timing->data_setup);
    printf("  Data Hold: %lu ns\n", timing->data_hold);
    printf("  Wait Pulse: %lu ns\n", timing->wait_pulse);
}

/**
 * @brief 计算 FSMC 时序寄存器值
 * @param timing Flash 时序参数
 * @param hclk HCLK 时钟频率 (Hz)
 * @param config FSMC 配置输出
 */
void calculate_fsmc_timing(const flash_timing_t *timing,
                           uint32_t hclk,
                           fsmc_timing_config_t *config)
{
    uint32_t hclk_ns = 1000000000 / hclk;

    printf("\nCalculating FSMC timing (HCLK=%lu MHz, %lu ns/cycle)...\n",
           hclk / 1000000, hclk_ns);

    // 计算 ADDSET (地址建立时间)
    // ADDSET = ceil(addr_setup / HCLK) - 1
    config->add_set = (timing->addr_setup + hclk_ns - 1) / hclk_ns;
    if (config->add_set > 15) config->add_set = 15;
    if (config->add_set < 1) config->add_set = 1;

    // 计算 DATAST (数据建立时间)
    // DATAST = ceil(data_setup / HCLK) - 1
    // 需要额外增加 1 周期
    config->data_st = (timing->data_setup + hclk_ns - 1) / hclk_ns;
    if (config->data_st > 255) config->data_st = 255;
    if (config->data_st < 2) config->data_st = 2;

    // 计算 BUSTURN (总线转换周期)
    config->bus_turn = (timing->addr_hold + hclk_ns - 1) / hclk_ns;
    if (config->bus_turn > 15) config->bus_turn = 15;

    printf("FSMC Timing Configuration:\n");
    printf("  ADDSET (Address Setup): %d cycles\n", config->add_set);
    printf("  DATAST (Data Setup): %d cycles\n", config->data_st);
    printf("  BUSTURN (Bus Turnaround): %d cycles\n", config->bus_turn);
}

/**
 * @brief 配置 FSMC 时序寄存器
 */
void fsmc_configure_timing(const fsmc_timing_config_t *config)
{
    uint32_t temp;

    // Bank1 控制寄存器
    // TODO: 根据具体 MCU 配置
    // 示例: 16 位异步 NOR Flash, 启用写使能

    // 配置时序寄存器 1 (读时序)
    temp = 0;
    temp |= (config->add_set - 1) << 8;   // ADDSET
    temp |= (config->data_st - 1) << 0;   // DATAST

    // 写入 FSMC_BANK1_R1
    // FSMC->BK1R[0] = temp;

    // 配置时序寄存器 2 (写时序)
    temp = 0;
    temp |= (config->add_set - 1) << 8;
    temp |= (config->data_st - 1) << 0;

    // 写入 FSMC_BANK1_R2
    // FSMC->BK1R[1] = temp;

    // 配置时序寄存器 3 (额外等待)
    temp = config->bus_turn;
    // 写入 FSMC_BANK1_R3
    // FSMC->BK1R[2] = temp;

    printf("FSMC timing registers configured.\n");
}

/**
 * @brief 验证时序配置
 */
int verify_timing_config(const flash_timing_t *flash_timing,
                         const fsmc_timing_config_t *fsmc_config,
                         uint32_t hclk)
{
    uint32_t hclk_ns = 1000000000 / hclk;
    uint32_t actual_access, expected_access;
    int warnings = 0;

    printf("\n=== Timing Verification ===\n");

    // 验证访问时间
    actual_access = (fsmc_config->add_set + fsmc_config->data_st) * hclk_ns;
    expected_access = flash_timing->t_acc;

    printf("Access Time:\n");
    printf("  Expected: %lu ns\n", expected_access);
    printf("  Actual: %lu ns\n", actual_access);

    if (actual_access < expected_access) {
        printf("  WARNING: Actual < Expected!\n");
        warnings++;
    } else if (actual_access > expected_access * 1.5) {
        printf("  WARNING: Too slow, may affect performance.\n");
        warnings++;
    } else {
        printf("  OK\n");
    }

    // 验证数据建立时间
    uint32_t actual_setup = fsmc_config->data_st * hclk_ns;
    printf("\nData Setup:\n");
    printf("  Expected: %lu ns\n", flash_timing->data_setup);
    printf("  Actual: %lu ns\n", actual_setup);

    if (actual_setup < flash_timing->data_setup) {
        printf("  WARNING: Setup time too short!\n");
        warnings++;
    } else {
        printf("  OK\n");
    }

    printf("\nVerification Result: %s\n", warnings == 0 ? "PASS" : "WARNINGS");

    return warnings;
}

/**
 * @brief 时序优化 - 尝试提高性能
 */
void optimize_timing(uint32_t hclk)
{
    uint32_t hclk_ns = 1000000000 / hclk;
    fsmc_timing_config_t opt_config;

    printf("\n=== Timing Optimization ===\n");

    // 尝试最小化时序参数，同时保证可靠性
    // 保守设置

    // 最小 ADDSET = 1 周期
    opt_config.add_set = 1;

    // 最小 DATAST = 2 周期
    opt_config.data_st = 2;

    // 最小 BUSTURN = 1 周期
    opt_config.bus_turn = 1;

    printf("Optimized Timing:\n");
    printf("  ADDSET: %d cycles (%lu ns)\n",
           opt_config.add_set, opt_config.add_set * hclk_ns);
    printf("  DATAST: %d cycles (%lu ns)\n",
           opt_config.data_st, opt_config.data_st * hclk_ns);
    printf("  BUSTURN: %d cycles (%lu ns)\n",
           opt_config.bus_turn, opt_config.bus_turn * hclk_ns);

    // TODO: 应用优化配置
    // fsmc_configure_timing(&opt_config);

    // 注意: 优化后应进行稳定性测试
    printf("\nNote: Run stability test after optimization.\n");
}

/**
 * @brief 主函数 - CFI 时序配置示例
 */
int main(void)
{
    uint32_t hclk = DEFAULT_HCLK;

    printf("=== CFI Timing Configuration Demo ===\n\n");

    // 步骤 1: 从 CFI 读取时序参数
    printf("[Step 1] Reading timing parameters from CFI...\n");
    cfi_read_timing_params(&g_flash_timing);

    // 步骤 2: 计算 FSMC 配置
    printf("\n[Step 2] Calculating FSMC timing...\n");
    calculate_fsmc_timing(&g_flash_timing, hclk, &g_fsmc_config);

    // 步骤 3: 配置 FSMC
    printf("\n[Step 3] Configuring FSMC registers...\n");
    fsmc_configure_timing(&g_fsmc_config);

    // 步骤 4: 验证配置
    printf("\n[Step 4] Verifying configuration...\n");
    verify_timing_config(&g_flash_timing, &g_fsmc_config, hclk);

    // 步骤 5: 性能优化
    printf("\n[Step 5] Optimization...\n");
    optimize_timing(hclk);

    printf("\n=== Demo Complete ===\n");
    return 0;
}

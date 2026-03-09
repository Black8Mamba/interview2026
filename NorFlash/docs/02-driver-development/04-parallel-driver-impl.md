# 并行Nor Flash驱动实现详解

并行Nor Flash驱动是在驱动框架基础上，针对并行接口Nor Flash芯片的具体实现。与SPI接口不同，并行接口通过地址总线和数据总线直接与MCU连接，能够实现更高的数据传输带宽和更低的访问延迟。本章详细介绍并行驱动概述、FSMC/FMC配置方法、异步读/写时序实现、突发模式支持以及完整的驱动代码示例。

---

## 1. 并行Nor Flash驱动概述

### 1.1 驱动设计目标

并行Nor Flash驱动的设计目标与SPI驱动类似，但需要特别关注并行接口的特殊需求：

**高性能**：并行接口的主要优势在于高带宽。驱动设计应充分利用这一特性，支持高速读取和批量数据传输。理想情况下，并行Nor Flash的读取速度可达200-400MB/s，远超SPI Flash的性能。

**内存映射访问**：并行Nor Flash通常被映射到处理器的地址空间，应用程序可以直接通过指针访问Flash内容，如同访问普通内存一样。这种XIP（Execute-In-Place）能力是并行Flash的重要特性，驱动需要确保内存映射的正确配置。

**时序精确性**：并行接口的时序比SPI更为复杂，涉及地址建立、数据建立、输出使能等多个时间参数的协调。驱动需要根据具体芯片的时序要求进行精确配置。

**CFI兼容性**：大多数现代并行Nor Flash支持CFI（Common Flash Interface）标准，驱动可以通过CFI查询获取芯片的详细参数，实现自动识别和配置。

### 1.2 支持的芯片型号

当前驱动支持的主流并行Nor Flash芯片如下表所示：

| 厂商 | 芯片型号 | 容量 | 数据宽度 | 访问时间 | 特点 |
|------|----------|------|----------|----------|------|
| Micron | MT28EW | 256Mbit-1Gbit | x8/x16 | 90-120ns | CFI支持，高可靠性 |
| Macronix | MX29GL | 64Mbit-512Mbit | x8/x16 | 90-110ns | CFI支持，低功耗 |
| Winbond | W29GL | 64Mbit-256Mbit | x8/x16 | 90-120ns | CFI支持，宽电压范围 |
| ISSI | IS66/67WVH | 32Mbit-256Mbit | x8/x16 | 90-120ns | CFI支持，车规级 |

这些芯片都支持CFI标准，驱动可以通过查询接口自动识别芯片型号并获取相应参数。

### 1.3 驱动架构

并行Nor Flash驱动在整体架构上与SPI驱动保持一致，采用分层设计。差异主要体现在传输层，SPI驱动的SPI传输接口被替换为FSMC/FMC并行传输接口。

```
┌─────────────────────────────────────────────────────────────┐
│                        应用层                                │
│              业务逻辑、文件系统、固件升级等                    │
└─────────────────────────────┬───────────────────────────────┘
                              │ 标准设备API
┌─────────────────────────────▼───────────────────────────────┐
│                      驱动层                                 │
│         设备初始化、读写操作、擦除、状态管理、错误处理           │
└─────────────────────────────┬───────────────────────────────┘
                              │ 传输接口抽象
┌─────────────────────────────▼───────────────────────────────┐
│                     传输层 (FSMC/FMC)                       │
│              FSMC寄存器配置、内存映射访问、DMA传输            │
└─────────────────────────────┬───────────────────────────────┘
                              │ GPIO/FSMC寄存器操作
┌─────────────────────────────▼───────────────────────────────┐
│                      硬件层                                 │
│              FSMC控制器、GPIO配置、Flash芯片                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. FSMC/FMC配置详解

FSMC（Flexible Static Memory Controller）和FMC（Flexible Memory Controller）是STMicroelectronics STM32系列MCU的外部存储器控制器。FSMC用于STM32F1/F2/F4系列，FMC用于STM32F7/H7系列。两者在基本概念上相似，但FMC提供了更高的性能和更多配置选项。

### 2.1 FSMC硬件连接

STM32与并行Nor Flash的典型硬件连接如下所示：

```
                    STM32与并行Nor Flash连接图
    ┌─────────────────┐           ┌─────────────────┐
    │                 │           │                 │
    │    STM32 MCU    │           │   并行 Nor Flash │
    │                 │           │                 │
    │  FSMC_A[0:25] ──┼───────────┼─ A[0:25]       │
    │                 │  地址总线  │                 │
    │  FSMC_D[0:15] ──┼───────────┼─ DQ[0:15]      │
    │                 │  数据总线  │                 │
    │  FSMC_NOE   ────┼───────────┼─ OE#           │
    │  FSMC_NWE   ────┼───────────┼─ WE#           │
    │  FSMC_NE[1:4] ──┼───────────┼─ CE#           │
    │                 │           │                 │
    │  FSMC_NBL[0] ──┼───────────┼─ LB# (x16模式) │
    │  FSMC_NBL[1] ──┼───────────┼─ UB# (x16模式) │
    │                 │           │                 │
    └─────────────────┘           └─────────────────┘
```

**关键信号说明**：

| STM32信号 | Flash信号 | 说明 |
|-----------|-----------|------|
| FSMC_A[0:25] | A[0:25] | 地址总线 |
| FSMC_D[0:15] | DQ[0:15] | 数据总线 |
| FSMC_NOE | OE# | 输出使能，低有效 |
| FSMC_NWE | WE# | 写使能，低有效 |
| FSMC_NE1-NE4 | CE# | 片选信号，根据BANK选择 |
| FSMC_NBL[0] | LB# | 低字节使能（x16模式） |
| FSMC_NBL[1] | UB# | 高字节使能（x16模式） |

### 2.2 FSMC初始化配置

以下是STM32 HAL库环境下FSMC的完整初始化代码：

```c
/**
 * FSMC配置结构体
 */
typedef struct {
    uint32_t Bank;                // 存储区域选择
    uint32_t DataAddressMux;      // 地址/数据复用
    uint32_t MemoryType;          // 存储器类型
    uint32_t MemoryDataWidth;     // 数据宽度
    uint32_t BurstAccessMode;     // 突发访问模式
    uint32_t WaitSignalPolarity;  // 等待信号极性
    uint32_t WaitSignalActive;    // 等待信号有效时机
    uint32_t WriteOperation;      // 写操作使能
    uint32_t WaitSignal;          // 等待信号使能
    uint32_t ExtendedMode;        // 扩展模式
    uint32_t AsynchronousWait;    // 异步等待使能
} FSMC_NORSRAM_InitTypeDef;

/**
 * FSMC时序配置结构体
 */
typedef struct {
    uint32_t AddressSetupTime;      // 地址建立时间
    uint32_t AddressHoldTime;        // 地址保持时间
    uint32_t DataSetupTime;          // 数据建立时间
    uint32_t BusTurnAroundDuration;  // 总线转向时间
    uint32_t CLKDivision;            // 时钟分频（同步模式）
    uint32_t DataLatency;            // 数据延迟（同步模式）
    uint32_t AccessMode;              // 访问模式
} FSMC_NORSRAM_TimingTypeDef;

/**
 * 并行Nor Flash硬件配置
 */
typedef struct {
    GPIO_TypeDef *bank1_port;     // NE1对应GPIO端口
    uint16_t bank1_pin;          // NE1对应GPIO引脚

    GPIO_TypeDef *oe_port;       // OE#对应GPIO端口
    uint16_t oe_pin;

    GPIO_TypeDef *we_port;       // WE#对应GPIO端口
    uint16_t we_pin;

    uint8_t data_width;          // 数据宽度：8或16
    uint8_t use_memory_mapping;   // 是否使用内存映射
} parallel_flash_hw_config_t;

/**
 * FSMC GPIO引脚配置
 */
static void fsmc_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能FSMC时钟 */
    __HAL_RCC_FSMC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* 配置地址总线引脚 (A0-A25) */
    /* 以PD0-7, PE0-15, PF0-12, PG0-9为例，实际引脚根据原理图配置 */

    /* 数据总线引脚 (D0-D15) */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;

    /* PD0-D4: D0-D4 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                          GPIO_PIN_3 | GPIO_PIN_4;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* PD8-D15: D8-D15 */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                          GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
                          GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* PE0-D15: D0-D15 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                          GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* PF0-PF12: A0-A12 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                          GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* PG0-PG12: A13-A25 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                          GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                          GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* 控制信号引脚: NOE, NWE, NE1 */
    /* NOE (Output Enable) */
    GPIO_InitStruct.Pin = GPIO_PIN_4;  // PD4
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* NWE (Write Enable) */
    GPIO_InitStruct.Pin = GPIO_PIN_5;  // PD5
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* NE1 (Chip Enable) */
    GPIO_InitStruct.Pin = GPIO_PIN_7;  // PG9
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

/**
 * FSMC Nor Flash初始化
 */
int fsmc_nor_flash_init(FSMC_NORSRAM_TimingTypeDef *read_timing,
                        FSMC_NORSRAM_TimingTypeDef *write_timing)
{
    FSMC_NORSRAM_InitTypeDef *FSMC_NORSRAMInit;

    /* 初始化GPIO */
    fsmc_gpio_init();

    /* 配置FSMC_NORSRAMInit结构体 */
    FSMC_NORSRAMInit->NSBank = FSMC_NORSRAM_BANK1;       // 使用Bank1
    FSMC_NORSRAMInit->DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE; // 非复用
    FSMC_NORSRAMInit->MemoryType = FSMC_MEMORY_TYPE_NOR;  // NOR Flash
    FSMC_NORSRAMInit->MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16; // 16位数据宽度
    FSMC_NORSRAMInit->BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE; // 异步模式
    FSMC_NORSRAMInit->WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    FSMC_NORSRAMInit->WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    FSMC_NORSRAMInit->WriteOperation = FSMC_WRITE_OPERATION_ENABLE; // 写使能
    FSMC_NORSRAMInit->WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    FSMC_NORSRAMInit->ExtendedMode = FSMC_EXTENDED_MODE_ENABLE; // 扩展模式
    FSMC_NORSRAMInit->AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;

    /* 配置读取时序 */
    FSMC_NORSRAMInit->ReadWriteTimingStruct = read_timing;

    /* 配置写入时序 */
    FSMC_NORSRAMInit->WriteTimingStruct = write_timing;

    /* 初始化FSMC */
    HAL_NOR_Init(&hnor, FSMC_NORSRAMInit);

    return 0;
}
```

### 2.3 时序参数配置

时序参数的正确配置是保证Nor Flash正常工作的关键。以下是针对不同速度等级的时序配置示例：

```c
/**
 * 时序参数计算
 * @param hclk_freq HCLK频率（Hz）
 * @param flash_access_time Flash访问时间（ns）
 * @return 计算后的时序参数
 */
FSMC_NORSRAM_TimingTypeDef fsmc_calculate_timing(uint32_t hclk_freq,
                                                    uint32_t flash_access_time)
{
    FSMC_NORSRAM_TimingTypeDef timing;
    uint32_t hclk_period_ns = 1000000000 / hclk_freq;  // HCLK周期（ns）

    /* 地址建立时间：至少1个HCLK周期 */
    timing.AddressSetupTime = 1;

    /* 地址保持时间：至少1个HCLK周期 */
    timing.AddressHoldTime = 1;

    /* 数据建立时间：根据Flash的访问时间计算 */
    uint32_t data_setup_cycles = (flash_access_time + hclk_period_ns - 1) / hclk_period_ns;
    timing.DataSetupTime = data_setup_cycles;
    if (timing.DataSetupTime < 2) {
        timing.DataSetupTime = 2;  // 最小值
    }

    /* 总线转向时间：读切换到写或反之的等待时间 */
    timing.BusTurnAroundDuration = 1;

    /* 同步模式参数（异步模式不使用） */
    timing.CLKDivision = 0;
    timing.DataLatency = 0;

    /* 异步模式A */
    timing.AccessMode = FSMC_ACCESS_MODE_A;

    return timing;
}

/**
 * 高速时序配置（HCLK=100MHz，Flash访问时间=90ns）
 */
FSMC_NORSRAM_TimingTypeDef fsmc_high_speed_read_timing(void)
{
    FSMC_NORSRAM_TimingTypeDef timing;

    /* HCLK=10ns, Flash tACC=90ns */
    timing.AddressSetupTime = 1;      // 10ns
    timing.AddressHoldTime = 1;        // 10ns
    timing.DataSetupTime = 9;          // 90ns (满足90ns要求)
    timing.BusTurnAroundDuration = 1;  // 10ns
    timing.CLKDivision = 0;
    timing.DataLatency = 0;
    timing.AccessMode = FSMC_ACCESS_MODE_A;

    return timing;
}

/**
 * 写入时序配置（写入通常比读取快）
 */
FSMC_NORSRAM_TimingTypeDef fsmc_write_timing(void)
{
    FSMC_NORSRAM_TimingTypeDef timing;

    /* 写入时序可以更短 */
    timing.AddressSetupTime = 1;      // 10ns
    timing.AddressHoldTime = 1;        // 10ns
    timing.DataSetupTime = 2;          // 20ns (写入通常更快)
    timing.BusTurnAroundDuration = 1;
    timing.CLKDivision = 0;
    timing.DataLatency = 0;
    timing.AccessMode = FSMC_ACCESS_MODE_A;

    return timing;
}

/**
 * 中速时序配置（HCLK=72MHz，Flash访问时间=100ns）
 */
FSMC_NORSRAM_TimingTypeDef fsmc_medium_speed_timing(void)
{
    FSMC_NORSRAM_TimingTypeDef timing;

    /* HCLK=13.9ns, Flash tACC=100ns */
    timing.AddressSetupTime = 1;      // ~14ns
    timing.AddressHoldTime = 1;        // ~14ns
    timing.DataSetupTime = 8;          // ~111ns (满足100ns要求)
    timing.BusTurnAroundDuration = 1;
    timing.CLKDivision = 0;
    timing.DataLatency = 0;
    timing.AccessMode = FSMC_ACCESS_MODE_A;

    return timing;
}
```

### 2.4 FMC配置（STM32F7/H7）

对于STM32F7和STM32H7系列，需要使用FMC（Flexible Memory Controller）替代FSMC。FMC在基本概念上与FSMC相似，但提供了更高的性能和更多配置选项。

```c
/**
 * FMC Nor Flash初始化（STM32F7/H7）
 */
int fmc_nor_flash_init(void)
{
    FMC_NORSRAM_InitTypeDef *FMC_NORSRAMInit;
    FMC_NORSRAM_TimingTypeDef *read_timing;
    FMC_NORSRAM_TimingTypeDef *write_timing;

    /* 使能FMC时钟 */
    __HAL_RCC_FMC_CLK_ENABLE();

    /* FMC_NORSRAMInit配置 */
    FMC_NORSRAMInit->NSBank = FMC_NORSRAM_BANK1;
    FMC_NORSRAMInit->DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;
    FMC_NORSRAMInit->MemoryType = FMC_MEMORY_TYPE_NOR;
    FMC_NORSRAMInit->MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;
    FMC_NORSRAMInit->BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
    FMC_NORSRAMInit->WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
    FMC_NORSRAMInit->WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
    FMC_NORSRAMInit->WriteOperation = FMC_WRITE_OPERATION_ENABLE;
    FMC_NORSRAMInit->WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
    FMC_NORSRAMInit->ExtendedMode = FMC_EXTENDED_MODE_ENABLE;
    FMC_NORSRAMInit->AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;

    /* 读取时序（FMC支持更高频率） */
    read_timing->AddressSetupTime = 1;
    read_timing->AddressHoldTime = 1;
    read_timing->DataSetupTime = 5;   // 更短的建立时间
    read_timing->BusTurnAroundDuration = 1;
    read_timing->AccessMode = FMC_ACCESS_MODE_A;

    /* 写入时序 */
    write_timing->AddressSetupTime = 1;
    write_timing->AddressHoldTime = 1;
    write_timing->DataSetupTime = 2;
    write_timing->BusTurnAroundDuration = 1;
    write_timing->AccessMode = FMC_ACCESS_MODE_A;

    /* 初始化FMC */
    HAL_NOR_Init(&hnor, FMC_NORSRAMInit, read_timing, write_timing);

    return 0;
}
```

---

## 3. 异步读/写时序实现

并行Nor Flash采用异步接口方式，没有独立的时钟信号，数据的传输完全由控制信号的时序关系决定。本节详细介绍读操作和写操作的具体实现方法。

### 3.1 内存映射读取

内存映射访问是并行Nor Flash最重要的特性之一。通过FSMC配置，Nor Flash被映射到处理器的地址空间中，应用程序可以直接通过指针访问Flash内容，如同访问普通内存一样。

```c
/**
 * Nor Flash内存映射基地址
 * Bank1的起始地址为0x60000000，根据NE引脚选择偏移不同
 */
#define NOR_FLASH_BASE_ADDR     0x60000000  // NE1
#define NOR_FLASH_BANK1_ADDR    0x60000000  // NE1
#define NOR_FLASH_BANK2_ADDR    0x64000000  // NE2
#define NOR_FLASH_BANK3_ADDR    0x68000000  // NE3
#define NOR_FLASH_BANK4_ADDR    0x6C000000  // NE4

/**
 * 内存映射读取（直接指针访问）
 * 这种方式利用FSMC的内存映射功能，读取速度最快
 */
typedef struct {
    volatile uint16_t data;  // 16位数据
} nor_flash_mem_t;

#define NOR_FLASH_PTR   ((nor_flash_mem_t *)NOR_FLASH_BASE_ADDR)

/**
 * 从Nor Flash读取16位数据（内存映射方式）
 */
uint16_t nor_flash_read_halfword(uint32_t addr)
{
    /* 地址必须字对齐 */
    return NOR_FLASH_PTR[addr / 2];
}

/**
 * 从Nor Flash读取32位数据
 */
uint32_t nor_flash_read_word(uint32_t addr)
{
    /* 地址必须双字对齐 */
    uint32_t val1 = NOR_FLASH_PTR[addr / 2];
    uint32_t val2 = NOR_FLASH_PTR[(addr + 2) / 2];
    return val1 | (val2 << 16);
}

/**
 * 从Nor Flash读取多个字
 */
void nor_flash_read_buffer(uint32_t addr, uint16_t *buf, uint32_t words)
{
    uint32_t i;
    for (i = 0; i < words; i++) {
        buf[i] = NOR_FLASH_PTR[(addr / 2) + i];
    }
}

/**
 * 读取一个字节（x16模式）
 */
uint8_t nor_flash_read_byte(uint32_t addr)
{
    uint16_t halfword = NOR_FLASH_PTR[addr / 2];
    if (addr % 2 == 0) {
        return halfword & 0xFF;  // 偶地址：低字节
    } else {
        return (halfword >> 8) & 0xFF;  // 奇地址：高字节
    }
}
```

### 3.2 命令基访问模式

除了内存映射方式，驱动还可以通过命令基访问模式与Nor Flash进行交互。这种方式允许发送特定的命令序列（如读取ID、擦除、编程等），这是实现Flash擦除和编程操作的必要方式。

```c
/**
 * 并行Nor Flash命令定义
 */
#define NOR_CMD_READ_ARRAY        0xFF    // 读取阵列（正常读取）
#define NOR_CMD_READ_ID           0x90    // 读取ID
#define NOR_CMD_READ_STATUS       0x70    // 读取状态寄存器
#define NOR_CMD_WRITE             0x40   // 编程（写入）
#define NOR_CMD_ERASE             0x20   // 擦除
#define NOR_CMD_ERASE_CONFIRM     0xD0   // 擦除确认
#define NOR_CMD_CFI_QUERY         0x98   // CFI查询
#define NOR_CMD_CLEAR_STATUS      0x50   // 清除状态寄存器
#define NOR_CMD_PROGRAM           0x10   // 编程（字编程）
#define NOR_CMD_BUFFER_PROGRAM    0x25   // 缓冲编程
#define NOR_CMD_BUFFER_CONFIRM    0x29   // 缓冲编程确认

/**
 * 并行Nor Flash命令接口结构体
 */
typedef struct {
    volatile uint16_t cmd;    // 命令寄存器
    volatile uint16_t data;    // 数据寄存器（读写）
    volatile uint16_t status;  // 状态寄存器
} nor_flash_cmd_t;

#define NOR_FLASH_CMD_PTR  ((nor_flash_cmd_t *)NOR_FLASH_BASE_ADDR)

/**
 * 读取Nor Flash ID（命令基模式）
 */
int nor_flash_read_id(uint16_t *manufacturer_id, uint16_t *device_id)
{
    uint32_t addr;

    /* 发送读ID命令 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ID;

    /* 读取厂商ID */
    addr = NOR_FLASH_BASE_ADDR;
    *manufacturer_id = NOR_FLASH_CMD_PTR->data;

    /* 读取设备ID */
    addr = NOR_FLASH_BASE_ADDR + 2;
    *device_id = NOR_FLASH_CMD_PTR->data;

    /* 返回读取阵列模式 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return 0;
}

/**
 * 读取状态寄存器
 */
uint8_t nor_flash_read_status(void)
{
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_STATUS;
    return (uint8_t)NOR_FLASH_CMD_PTR->status;
}

/**
 * 等待操作完成（轮询状态位）
 */
int nor_flash_wait_ready(uint32_t timeout_ms)
{
    uint8_t status;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        status = nor_flash_read_status();

        /* 检查SR.7 (Write In Progress位) */
        if (status & 0x80) {
            /* 检查SR.5 (Erase/Program Error位) */
            if (status & 0x20) {
                return -NOR_ERR_ERASE;  // 擦除错误
            }
            return 0;  // 操作完成
        }
    }

    return -NOR_ERR_TIMEOUT;  // 超时
}
```

### 3.3 写入操作实现

并行Nor Flash的写入操作与SPI Flash有所不同，通常使用字编程（Word Program）或缓冲编程（Buffer Program）命令。

```c
/**
 * 字编程（Word Program）
 * 将数据写入指定地址
 */
int nor_flash_program_word(uint32_t addr, uint16_t data)
{
    int ret;

    /* 发送写使能（某些芯片需要） */
    NOR_FLASH_CMD_PTR->cmd = 0x40;  // 编程命令

    /* 发送地址和数据 */
    volatile uint16_t *ptr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + addr);
    *ptr = data;

    /* 等待编程完成 */
    ret = nor_flash_wait_ready(NOR_TIMEOUT_WORD_PROGRAM);
    if (ret < 0) {
        /* 清除状态寄存器 */
        NOR_FLASH_CMD_PTR->cmd = NOR_CMD_CLEAR_STATUS;
        return ret;
    }

    /* 返回读取阵列模式 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return 0;
}

/**
 * 缓冲编程（Buffer Program）
 * 适用于大块数据写入，效率更高
 */
int nor_flash_buffer_program(uint32_t addr, const uint16_t *data, uint32_t words)
{
    uint32_t i;
    int ret;

    if (words == 0 || words > 32) {  // 缓冲区大小通常为32字
        return -NOR_ERR_INVALID_PARAM;
    }

    /* 发送缓冲编程命令 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_BUFFER_PROGRAM;

    /* 发送起始地址 */
    volatile uint16_t *ptr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + addr);

    /* 写入数据到缓冲区 */
    for (i = 0; i < words; i++) {
        ptr[i] = data[i];
    }

    /* 发送缓冲编程确认 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_BUFFER_CONFIRM;

    /* 等待编程完成 */
    ret = nor_flash_wait_ready(NOR_TIMEOUT_BUFFER_PROGRAM);
    if (ret < 0) {
        NOR_FLASH_CMD_PTR->cmd = NOR_CMD_CLEAR_STATUS;
        return ret;
    }

    /* 返回读取阵列模式 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return 0;
}

/**
 * 通用写入函数（自动处理）
 */
int nor_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t written = 0;
    uint16_t halfword;

    /* 处理奇数字节 */
    if (addr % 2 && len > 0) {
        halfword = nor_flash_read_halfword(addr - 1);
        halfword = (halfword & 0xFF00) | buf[0];
        nor_flash_program_word(addr - 1, halfword);
        addr++;
        written++;
        len--;
    }

    /* 处理字对齐的数据 */
    while (len >= 2) {
        halfword = buf[written + 1] << 8 | buf[written];
        nor_flash_program_word(addr, halfword);
        addr += 2;
        written += 2;
        len -= 2;
    }

    /* 处理最后一个字节 */
    if (len > 0) {
        halfword = nor_flash_read_halfword(addr);
        halfword = (halfword & 0x00FF) | (buf[written] << 8);
        nor_flash_program_word(addr, halfword);
        written++;
    }

    return (int)written;
}
```

### 3.4 擦除操作实现

并行Nor Flash的擦除操作通常以扇区（Sector）或块（Block）为单位进行。

```c
/**
 * 扇区擦除（Sector Erase）
 * @param addr 扇区地址（必须对齐到扇区边界）
 */
int nor_flash_erase_sector(uint32_t addr)
{
    int ret;

    /* 发送擦除命令 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE;

    /* 发送扇区地址 */
    volatile uint16_t *ptr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + addr);
    *ptr = 0x0000;  // 任意数据触发擦除

    /* 发送擦除确认 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE_CONFIRM;

    /* 等待擦除完成 */
    ret = nor_flash_wait_ready(NOR_TIMEOUT_SECTOR_ERASE);
    if (ret < 0) {
        NOR_FLASH_CMD_PTR->cmd = NOR_CMD_CLEAR_STATUS;
        return ret;
    }

    /* 返回读取阵列模式 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return 0;
}

/**
 * 块擦除（Block Erase）
 * 块大小通常为64KB
 */
int nor_flash_erase_block(uint32_t addr)
{
    int ret;
    uint32_t block_size = 64 * 1024;  // 64KB

    /* 对齐检查 */
    if (addr % block_size != 0) {
        return -NOR_ERR_ALIGNMENT;
    }

    /* 块擦除命令序列 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE;

    volatile uint16_t *ptr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + addr);
    *ptr = 0x0000;

    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE_CONFIRM;

    ret = nor_flash_wait_ready(NOR_TIMEOUT_BLOCK_ERASE);

    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return ret;
}

/**
 * 整片擦除
 */
int nor_flash_erase_chip(void)
{
    int ret;

    /* 发送整片擦除命令 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE;

    /* 发送解锁序列 */
    NOR_FLASH_CMD_PTR->cmd = 0xAA;  // 某些芯片需要
    volatile uint16_t *addr_ptr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + 0x555);
    *addr_ptr = 0x555;

    NOR_FLASH_CMD_PTR->cmd = 0x55;
    *addr_ptr = 0x2AA;

    /* 发送擦除命令 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE;
    *addr_ptr = 0x555;

    /* 确认擦除 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_ERASE_CONFIRM;

    ret = nor_flash_wait_ready(NOR_TIMEOUT_CHIP_ERASE);

    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return ret;
}

/**
 * 通用擦除函数（自动选择擦除粒度）
 */
int nor_flash_erase(uint32_t addr, uint32_t len)
{
    uint32_t sector_size = 64 * 1024;  // 默认扇区大小
    uint32_t erased = 0;

    /* 擦除每个扇区 */
    while (erased < len) {
        uint32_t current_addr = addr + erased;

        /* 对齐到扇区边界 */
        uint32_t sector_addr = (current_addr / sector_size) * sector_size;

        int ret = nor_flash_erase_block(sector_addr);
        if (ret < 0) {
            return ret;
        }

        erased += sector_size;
    }

    return 0;
}
```

---

## 4. 突发模式支持

突发（Burst）模式是某些高性能并行Nor Flash支持的特性，允许在初始化一次地址后连续读取多个数据字，而无需每次都发送地址。这种模式可以显著提高顺序读取的性能，特别适用于代码执行（XIP）和大规模数据读取场景。

### 4.1 突发模式原理

在普通异步读取模式下，每次读取都需要完整的地址建立和数据访问周期。而在突发模式下，Flash芯片在第一个访问周期后会自动递增内部地址，从而减少地址建立时间，提高连续读取的吞吐量。

```
普通读取模式：
    ┌──────┬──────┬──────┬──────┬──────┐
    │ Addr │ Addr │ Addr │ Addr │ Addr │  ← 每次都需要地址
    │  0   │  2   │  4   │  6   │  8   │
    └──────┴──────┴──────┴──────┴──────┘
    ┌──────┬──────┬──────┬──────┬──────┐
    │ Data │ Data │ Data │ Data │ Data │  ← 每个访问周期后需要等待
    └──────┴──────┴──────┴──────┴──────┘

突发模式（4字突发）：
    ┌────────────────────┬────────────────────┐
    │       Addr 0      │    Auto Incr      │  ← 只需一次地址
    └────────────────────┴────────────────────┘
    ┌──────┬──────┬──────┬──────┐
    │ Data │ Data │ Data │ Data │      ← 连续数据流
    └──────┴──────┴──────┴──────┘
```

### 4.2 突发模式配置

以下是突发模式的配置和实现代码：

```c
/**
 * 突发模式配置定义
 */
typedef enum {
    NOR_BURST_NONE = 0,     // 无突发
    NOR_BURST_4 = 1,       // 4字突发
    NOR_BURST_8 = 2,       // 8字突发
    NOR_BURST_16 = 3,      // 16字突发
    NOR_BURST_32 = 4,      // 32字突发
    NOR_BURST_CONTINUOUS = 5  // 连续突发
} nor_burst_length_t;

/**
 * 突发模式使能配置
 */
typedef struct {
    uint8_t enabled;            // 突发模式使能
    nor_burst_length_t length; // 突发长度
    uint8_t wait_enabled;      // 等待使能
} nor_burst_config_t;

/**
 * 设置突发模式
 */
int nor_flash_set_burst_mode(uint8_t enable, nor_burst_length_t length)
{
    /* 某些芯片需要通过配置寄存器设置突发模式 */
    /* 这里以配置命令为例 */

    if (enable) {
        /* 发送突发模式设置命令 */
        NOR_FLASH_CMD_PTR->cmd = 0x60;  // 读取模式配置命令

        /* 写入突发长度配置 */
        volatile uint16_t *config_addr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + 0x100);
        *config_addr = (length << 8) | 0x01;  // 使能突发 + 长度
    } else {
        /* 禁用突发模式 */
        NOR_FLASH_CMD_PTR->cmd = 0x60;
        volatile uint16_t *config_addr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + 0x100);
        *config_addr = 0x00;
    }

    /* 返回正常读取模式 */
    NOR_FLASH_CMD_PTR->cmd = NOR_CMD_READ_ARRAY;

    return 0;
}

/**
 * 突发读取（用于大块顺序数据读取）
 * 注意：实际支持情况取决于具体芯片
 */
int nor_flash_burst_read(uint32_t addr, uint16_t *buf, uint32_t words)
{
    uint32_t i;

    /* 使能突发模式 */
    nor_flash_set_burst_mode(1, NOR_BURST_8);

    /* 起始地址（触发第一次访问） */
    volatile uint16_t *ptr = (volatile uint16_t *)(NOR_FLASH_BASE_ADDR + addr);

    /* 突发读取 */
    for (i = 0; i < words; i++) {
        buf[i] = ptr[i];
    }

    /* 禁用突发模式 */
    nor_flash_set_burst_mode(0, NOR_BURST_NONE);

    return (int)words;
}
```

### 4.3 FSMC同步突发模式

对于支持同步突发访问的Nor Flash，FSMC也提供了相应的同步突发模式配置。

```c
/**
 * FSMC同步突发模式配置
 */
int fsmc_sync_burst_init(void)
{
    FSMC_NORSRAM_InitTypeDef FSMC_NORSRAMInit;
    FSMC_NORSRAM_TimingTypeDef timing;

    /* 基础配置 */
    FSMC_NORSRAMInit.NSBank = FSMC_NORSRAM_BANK1;
    FSMC_NORSRAMInit.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    FSMC_NORSRAMInit.MemoryType = FSMC_MEMORY_TYPE_NOR;
    FSMC_NORSRAMInit.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;

    /* 使能突发模式 */
    FSMC_NORSRAMInit.BurstAccessMode = FSMC_BURST_ACCESS_MODE_ENABLE;
    FSMC_NORSRAMInit.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    FSMC_NORSRAMInit.WaitSignalActive = FSMC_WAIT_TIMING_DURING_WS;
    FSMC_NORSRAMInit.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    FSMC_NORSRAMInit.WaitSignal = FSMC_WAIT_SIGNAL_ENABLE;
    FSMC_NORSRAMInit.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;

    /* 同步时序配置 */
    timing.AddressSetupTime = 2;
    timing.AddressHoldTime = 1;
    timing.DataSetupTime = 2;
    timing.BusTurnAroundDuration = 1;

    /* 同步模式参数 */
    timing.CLKDivision = 2;           // 时钟分频：HCLK/2
    timing.DataLatency = 2;           // 数据延迟：2个时钟周期

    timing.AccessMode = FSMC_ACCESS_MODE_B;  // 模式B

    HAL_NOR_Init(&hnor, &FSMC_NORSRAMInit, &timing, &timing);

    return 0;
}
```

---

## 5. 驱动代码示例

本节提供完整的并行Nor Flash驱动代码示例，包括初始化、读取、写入和擦除等核心功能。

### 5.1 驱动头文件定义

```c
/**
 * @file nor_parallel_driver.h
 * @brief 并行Nor Flash驱动头文件
 */

#ifndef __NOR_PARALLEL_DRIVER_H
#define __NOR_PARALLEL_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * 常量定义
 *============================================================================*/

/* 超时时间（毫秒） */
#define NOR_TIMEOUT_DEFAULT           100
#define NOR_TIMEOUT_WORD_PROGRAM     300
#define NOR_TIMEOUT_BUFFER_PROGRAM   500
#define NOR_TIMEOUT_SECTOR_ERASE     500
#define NOR_TIMEOUT_BLOCK_ERASE      1000
#define NOR_TIMEOUT_CHIP_ERASE       200000

/* 错误码 */
#define NOR_OK                       0
#define NOR_ERR                      -1
#define NOR_ERR_INVALID_PARAM         -2
#define NOR_ERR_NO_DEVICE            -3
#define NOR_ERR_OUT_OF_RANGE         -4
#define NOR_ERR_ALIGNMENT            -5
#define NOR_ERR_TIMEOUT              -6
#define NOR_ERR_ERASE                -7

/*============================================================================
 * 类型定义
 *============================================================================*/

/* 设备状态 */
typedef enum {
    NOR_STATE_IDLE = 0,
    NOR_STATE_READING,
    NOR_STATE_WRITING,
    NOR_STATE_ERASING,
    NOR_STATE_ERROR,
} nor_state_t;

/* 突发长度 */
typedef enum {
    NOR_BURST_DISABLE = 0,
    NOR_BURST_4_WORDS,
    NOR_BURST_8_WORDS,
    NOR_BURST_CONTINUOUS,
} nor_burst_length_t;

/* 并行Flash配置 */
typedef struct {
    uint32_t base_addr;           // 基地址（FSMC Bank地址）
    uint8_t data_width;           // 数据宽度：8或16
    uint8_t burst_enable;         // 突发模式使能
    uint32_t sector_size;         // 扇区大小
    uint32_t block_size;          // 块大小
    uint32_t total_size;          // 总容量
} nor_parallel_config_t;

/* 并行Flash设备结构体 */
typedef struct {
    nor_parallel_config_t config;
    uint16_t manufacturer_id;
    uint16_t device_id;
    nor_state_t state;
    bool burst_mode;
    void *priv;
} nor_parallel_device_t;

/*============================================================================
 * API函数声明
 *============================================================================*/

/* 初始化与配置 */
int nor_parallel_init(nor_parallel_device_t *dev, const nor_parallel_config_t *config);
int nor_parallel_deinit(nor_parallel_device_t *dev);

/* 读取操作 */
int nor_parallel_read(nor_parallel_device_t *dev, uint32_t addr,
                      uint8_t *buf, uint32_t len);
int nor_parallel_read_id(nor_parallel_device_t *dev,
                         uint16_t *mfg_id, uint16_t *dev_id);

/* 写入操作 */
int nor_parallel_write(nor_parallel_device_t *dev, uint32_t addr,
                       const uint8_t *buf, uint32_t len);
int nor_parallel_erase_sector(nor_parallel_device_t *dev, uint32_t addr);
int nor_parallel_erase_block(nor_parallel_device_t *dev, uint32_t addr);
int nor_parallel_erase_chip(nor_parallel_device_t *dev);

/* 状态查询 */
int nor_parallel_wait_ready(nor_parallel_device_t *dev, uint32_t timeout_ms);
nor_state_t nor_parallel_get_state(nor_parallel_device_t *dev);

/* 突发模式 */
int nor_parallel_set_burst(nor_parallel_device_t *dev,
                           nor_burst_length_t length);

#ifdef __cplusplus
}
#endif

#endif /* __NOR_PARALLEL_DRIVER_H */
```

### 5.2 驱动实现文件

```c
/**
 * @file nor_parallel_driver.c
 * @brief 并行Nor Flash驱动实现
 */

#include "nor_parallel_driver.h"
#include <string.h>

/* 默认配置 */
static const nor_parallel_config_t default_config = {
    .base_addr = 0x60000000,
    .data_width = 16,
    .burst_enable = 0,
    .sector_size = 64 * 1024,
    .block_size = 64 * 1024,
    .total_size = 8 * 1024 * 1024,
};

/**
 * 初始化并行Nor Flash
 */
int nor_parallel_init(nor_parallel_device_t *dev, const nor_parallel_config_t *config)
{
    if (!dev || !config) {
        return NOR_ERR_INVALID_PARAM;
    }

    /* 复制配置 */
    memcpy(&dev->config, config, sizeof(nor_parallel_config_t));

    /* 如果未提供配置，使用默认值 */
    if (config->sector_size == 0) {
        dev->config.sector_size = default_config.sector_size;
    }
    if (config->block_size == 0) {
        dev->config.block_size = default_config.block_size;
    }
    if (config->total_size == 0) {
        dev->config.total_size = default_config.total_size;
    }

    /* 读取芯片ID进行验证 */
    uint16_t mfg_id, dev_id;
    int ret = nor_parallel_read_id(dev, &mfg_id, &dev_id);
    if (ret < 0) {
        return NOR_ERR_NO_DEVICE;
    }

    dev->manufacturer_id = mfg_id;
    dev->device_id = dev_id;
    dev->state = NOR_STATE_IDLE;
    dev->burst_mode = false;

    return NOR_OK;
}

/**
 * 读取芯片ID
 */
int nor_parallel_read_id(nor_parallel_device_t *dev,
                         uint16_t *mfg_id, uint16_t *dev_id)
{
    if (!dev || !mfg_id || !dev_id) {
        return NOR_ERR_INVALID_PARAM;
    }

    /* 命令基读取ID */
    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;

    /* 发送读ID命令 */
    *cmd_ptr = 0x90;

    /* 读取厂商ID */
    volatile uint16_t *addr_ptr = (volatile uint16_t *)dev->config.base_addr;
    *mfg_id = *addr_ptr;

    /* 读取设备ID */
    *dev_id = *(addr_ptr + 1);

    /* 返回读取阵列模式 */
    *cmd_ptr = 0xFF;

    return NOR_OK;
}

/**
 * 等待Flash就绪
 */
int nor_parallel_wait_ready(nor_parallel_device_t *dev, uint32_t timeout_ms)
{
    if (!dev) {
        return NOR_ERR_INVALID_PARAM;
    }

    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        /* 读取状态寄存器 */
        *cmd_ptr = 0x70;
        uint8_t status = (uint8_t)(*(cmd_ptr + 1) & 0xFF);

        /* 检查SR.7 (WIP - Write In Progress) */
        if (status & 0x80) {
            /* 检查错误位 */
            if (status & 0x20) {
                *cmd_ptr = 0x50;  // 清除状态寄存器
                *cmd_ptr = 0xFF;  // 返回读取阵列
                return NOR_ERR_ERASE;
            }

            *cmd_ptr = 0xFF;
            return NOR_OK;
        }
    }

    *cmd_ptr = 0xFF;
    return NOR_ERR_TIMEOUT;
}

/**
 * 读取数据
 */
int nor_parallel_read(nor_parallel_device_t *dev, uint32_t addr,
                       uint8_t *buf, uint32_t len)
{
    if (!dev || !buf || len == 0) {
        return NOR_ERR_INVALID_PARAM;
    }

    /* 地址边界检查 */
    if (addr + len > dev->config.total_size) {
        return NOR_ERR_OUT_OF_RANGE;
    }

    /* 等待之前的操作完成 */
    int ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        return ret;
    }

    dev->state = NOR_STATE_READING;

    /* 根据数据宽度选择读取方式 */
    if (dev->config.data_width == 16) {
        /* 16位模式 */
        uint32_t word_count = len / 2;
        volatile uint16_t *flash_ptr = (volatile uint16_t *)(dev->config.base_addr + addr);

        uint32_t i;
        for (i = 0; i < word_count; i++) {
            uint16_t data = flash_ptr[i];
            buf[i * 2] = data & 0xFF;
            buf[i * 2 + 1] = (data >> 8) & 0xFF;
        }

        /* 处理奇数字节 */
        if (len % 2) {
            uint16_t data = flash_ptr[word_count];
            buf[len - 1] = data & 0xFF;
        }
    } else {
        /* 8位模式 */
        volatile uint8_t *flash_ptr = (volatile uint8_t *)(dev->config.base_addr + addr);
        memcpy(buf, (uint8_t *)flash_ptr, len);
    }

    dev->state = NOR_STATE_IDLE;

    return (int)len;
}

/**
 * 写入数据
 */
int nor_parallel_write(nor_parallel_device_t *dev, uint32_t addr,
                       const uint8_t *buf, uint32_t len)
{
    if (!dev || !buf || len == 0) {
        return NOR_ERR_INVALID_PARAM;
    }

    if (addr + len > dev->config.total_size) {
        return NOR_ERR_OUT_OF_RANGE;
    }

    dev->state = NOR_STATE_WRITING;

    uint32_t written = 0;

    /* 16位模式写入 */
    if (dev->config.data_width == 16) {
        /* 处理对齐 */
        if (addr % 2 && written < len) {
            /* 字节对齐处理 */
            written++;
        }

        /* 字写入 */
        while (written + 1 < len) {
            uint16_t data = buf[written] | (buf[written + 1] << 8);

            volatile uint16_t *flash_ptr = (volatile uint16_t *)(dev->config.base_addr + addr + written);
            *flash_ptr = 0x40;  // 编程命令
            *flash_ptr = data;

            int ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_WORD_PROGRAM);
            if (ret < 0) {
                dev->state = NOR_STATE_IDLE;
                return ret;
            }

            written += 2;
        }
    } else {
        /* 8位模式 */
        while (written < len) {
            volatile uint8_t *flash_ptr = (volatile uint8_t *)(dev->config.base_addr + addr + written);
            *flash_ptr = 0x40;
            *flash_ptr = buf[written];

            int ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_WORD_PROGRAM);
            if (ret < 0) {
                dev->state = NOR_STATE_IDLE;
                return ret;
            }

            written++;
        }
    }

    /* 返回读取阵列模式 */
    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;
    *cmd_ptr = 0xFF;

    dev->state = NOR_STATE_IDLE;

    return (int)written;
}

/**
 * 扇区擦除
 */
int nor_parallel_erase_sector(nor_parallel_device_t *dev, uint32_t addr)
{
    if (!dev) {
        return NOR_ERR_INVALID_PARAM;
    }

    /* 对齐检查 */
    if (addr % dev->config.sector_size != 0) {
        return NOR_ERR_ALIGNMENT;
    }

    if (addr >= dev->config.total_size) {
        return NOR_ERR_OUT_OF_RANGE;
    }

    dev->state = NOR_STATE_ERASING;

    /* 等待之前的操作完成 */
    int ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        dev->state = NOR_STATE_IDLE;
        return ret;
    }

    /* 擦除序列 */
    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;
    volatile uint16_t *addr_ptr = (volatile uint16_t *)(dev->config.base_addr + addr);

    *cmd_ptr = 0x20;      // 擦除命令
    *addr_ptr = 0xD0;     // 确认

    /* 等待擦除完成 */
    ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_SECTOR_ERASE);

    /* 返回读取阵列 */
    *cmd_ptr = 0xFF;

    dev->state = (ret < 0) ? NOR_STATE_ERROR : NOR_STATE_IDLE;

    return ret;
}

/**
 * 块擦除
 */
int nor_parallel_erase_block(nor_parallel_device_t *dev, uint32_t addr)
{
    if (!dev) {
        return NOR_ERR_INVALID_PARAM;
    }

    if (addr % dev->config.block_size != 0) {
        return NOR_ERR_ALIGNMENT;
    }

    dev->state = NOR_STATE_ERASING;

    int ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        dev->state = NOR_STATE_IDLE;
        return ret;
    }

    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;
    volatile uint16_t *addr_ptr = (volatile uint16_t *)(dev->config.base_addr + addr);

    *cmd_ptr = 0x20;
    *addr_ptr = 0xD0;

    ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_BLOCK_ERASE);
    *cmd_ptr = 0xFF;

    dev->state = (ret < 0) ? NOR_STATE_ERROR : NOR_STATE_IDLE;

    return ret;
}

/**
 * 整片擦除
 */
int nor_parallel_erase_chip(nor_parallel_device_t *dev)
{
    if (!dev) {
        return NOR_ERR_INVALID_PARAM;
    }

    dev->state = NOR_STATE_ERASING;

    int ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_DEFAULT);
    if (ret < 0) {
        dev->state = NOR_STATE_IDLE;
        return ret;
    }

    /* 整片擦除序列 */
    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;
    volatile uint16_t *addr_ptr;

    *cmd_ptr = 0x20;
    addr_ptr = (volatile uint16_t *)(dev->config.base_addr + 0x555);
    *addr_ptr = 0xAA;

    *cmd_ptr = 0x55;
    addr_ptr = (volatile uint16_t *)(dev->config.base_addr + 0x2AA);
    *addr_ptr = 0x55;

    *cmd_ptr = 0x20;
    addr_ptr = (volatile uint16_t *)(dev->config.base_addr + 0x555);
    *addr_ptr = 0xAA;

    *cmd_ptr = 0xD0;
    *addr_ptr = 0xD0;

    ret = nor_parallel_wait_ready(dev, NOR_TIMEOUT_CHIP_ERASE);
    *cmd_ptr = 0xFF;

    dev->state = (ret < 0) ? NOR_STATE_ERROR : NOR_STATE_IDLE;

    return ret;
}

/**
 * 获取设备状态
 */
nor_state_t nor_parallel_get_state(nor_parallel_device_t *dev)
{
    if (!dev) {
        return NOR_STATE_ERROR;
    }
    return dev->state;
}

/**
 * 设置突发模式
 */
int nor_parallel_set_burst(nor_parallel_device_t *dev, nor_burst_length_t length)
{
    if (!dev) {
        return NOR_ERR_INVALID_PARAM;
    }

    volatile uint16_t *cmd_ptr = (volatile uint16_t *)dev->config.base_addr;

    if (length == NOR_BURST_DISABLE) {
        *cmd_ptr = 0xFF;  // 返回普通模式
        dev->burst_mode = false;
    } else {
        /* 某些芯片支持突发模式配置 */
        *cmd_ptr = 0x60;

        volatile uint16_t *config_ptr = (volatile uint16_t *)(dev->config.base_addr + 0x100);

        switch (length) {
            case NOR_BURST_4_WORDS:
                *config_ptr = 0x01;  // 4字突发
                break;
            case NOR_BURST_8_WORDS:
                *config_ptr = 0x02;  // 8字突发
                break;
            case NOR_BURST_CONTINUOUS:
                *config_ptr = 0x0F;  // 连续突发
                break;
            default:
                return NOR_ERR_INVALID_PARAM;
        }

        dev->burst_mode = true;
    }

    return NOR_OK;
}
```

### 5.3 使用示例

```c
/**
 * 并行Nor Flash使用示例
 */
void nor_parallel_example(void)
{
    nor_parallel_device_t flash_dev;
    nor_parallel_config_t config;

    /* 配置Flash参数 */
    config.base_addr = 0x60000000;   // FSMC Bank1基地址
    config.data_width = 16;           // 16位数据宽度
    config.burst_enable = 0;         // 禁用突发模式
    config.sector_size = 64 * 1024;  // 64KB扇区
    config.block_size = 64 * 1024;
    config.total_size = 8 * 1024 * 1024;  // 8MB容量

    /* 初始化 */
    int ret = nor_parallel_init(&flash_dev, &config);
    if (ret != NOR_OK) {
        printf("Flash init failed: %d\n", ret);
        return;
    }

    printf("Flash initialized: MFG=%04X, DEV=%04X\n",
           flash_dev.manufacturer_id, flash_dev.device_id);

    /* 读取数据 */
    uint8_t read_buf[256];
    ret = nor_parallel_read(&flash_dev, 0x1000, read_buf, sizeof(read_buf));
    if (ret > 0) {
        printf("Read %d bytes\n", ret);
    }

    /* 写入数据 */
    uint8_t write_buf[] = "Hello, Parallel Flash!";
    ret = nor_parallel_write(&flash_dev, 0x1000, write_buf, sizeof(write_buf));
    if (ret > 0) {
        printf("Wrote %d bytes\n", ret);
    }

    /* 验证写入 */
    memset(read_buf, 0, sizeof(read_buf));
    ret = nor_parallel_read(&flash_dev, 0x1000, read_buf, sizeof(write_buf));
    if (ret > 0 && memcmp(read_buf, write_buf, sizeof(write_buf)) == 0) {
        printf("Verify OK\n");
    } else {
        printf("Verify failed\n");
    }

    /* 擦除扇区 */
    ret = nor_parallel_erase_sector(&flash_dev, 0x1000);
    if (ret == NOR_OK) {
        printf("Sector erased\n");
    }

    /* 获取状态 */
    nor_state_t state = nor_parallel_get_state(&flash_dev);
    printf("Flash state: %d\n", state);
}
```

---

## 本章小结

本章详细介绍了并行Nor Flash驱动的完整实现，主要内容包括：

**1. 并行Nor Flash驱动概述**

- 驱动设计目标：高性能、内存映射访问、时序精确性、CFI兼容性
- 支持的主流芯片型号及其特性
- 驱动架构与层次结构

**2. FSMC/FMC配置详解**

- STM32与并行Nor Flash的硬件连接方式
- FSMC初始化配置代码
- 时序参数计算方法和不同速度等级的配置文件
- FMC在STM32F7/H7系列中的应用

**3. 异步读/写时序实现**

- 内存映射读取：通过指针直接访问Flash
- 命令基访问模式：发送特定命令序列
- 写入操作：字编程和缓冲编程
- 擦除操作：扇区擦除、块擦除、整片擦除

**4. 突发模式支持**

- 突发模式原理和优势
- 突发模式配置和实现
- FSMC同步突发模式配置

**5. 驱动代码示例**

- 完整的驱动头文件定义
- 驱动实现文件和核心API
- 实际使用示例

通过本章的学习，开发者应能够掌握并行Nor Flash驱动的核心技术要点，实现稳定可靠的并行Flash驱动代码。与SPI Flash相比，并行Flash具有更高的数据传输带宽和更低的访问延迟，特别适用于需要高速代码执行（XIP）或大规模数据读取的应用场景。

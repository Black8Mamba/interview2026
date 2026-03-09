# 启动加载器设计

启动加载器（Bootloader）是嵌入式系统中负责初始化硬件、加载并跳转至应用程序的核心软件模块。在 Nor Flash 应用场景中，由于 Nor Flash 支持 XIP（eXecute In Place）特性，启动加载器的设计既可以采用传统的两级引导模式（Flash器 +  加载应用程序），也可以利用 Nor Flash 的直接执行特性实现单级启动。本章将详细介绍基于 Nor Flash 的启动加载器设计方法，涵盖启动流程概述、双镜像备份、增量升级和恢复模式等关键技术。

---

## 启动流程概述

### 系统启动流程

基于 Nor Flash 的嵌入式系统启动流程通常包括以下几个阶段：

**第一阶段：上电复位与硬件初始化**

系统上电后，微控制器从预设的启动地址开始执行代码。对于支持 Nor Flash XIP 的系统，CPU 可以直接从 Nor Flash 中读取指令运行，无需先将代码复制到 RAM 中。启动加载器首先执行以下初始化操作：

1. 配置系统时钟（PLL、Dividers）
2. 初始化外部存储器接口（FSMC、EMC 或 SPI 控制器）
3. 配置中断向量表基地址
4. 初始化堆栈指针
5. 配置看门狗定时器（防止加载失败导致系统死锁）

**第二阶段：Nor Flash 初始化**

启动加载器需要正确初始化 Nor Flash 控制器，建立与 Flash 芯片的通信。这一阶段的主要任务包括：

1. 配置 SPI 或并行接口时序参数
2. 读取 SFDP（Serial Flash Discoverable Parameter）信息或 CFI 查询
3. 识别 Flash 芯片型号和容量
4. 配置读参数（如 DDR 模式、时钟分频等）

**第三阶段：启动条件判断**

启动加载器需要根据预设的条件判断应该启动哪个应用程序镜像。常见的判断逻辑包括：

1. **正常启动**：加载主应用程序镜像
2. **升级模式**：检测到升级标志，加载新镜像或进入升级流程
3. **恢复模式**：检测到特定引脚组合或超时条件，进入恢复模式
4. **回退启动**：新镜像启动失败，回退到备份镜像

**第四阶段：应用程序加载与跳转**

完成应用程序镜像的加载和验证后，启动加载器将控制权移交给应用程序：

1. 配置应用程序的内存映射（将 Flash 地址映射到代码执行区域）
2. 初始化应用程序的中断向量表
3. 跳转到应用程序入口函数（Reset_Handler）

### 两种启动模式对比

**模式一：XIP 直接启动**

在 XIP 模式下，应用程序代码直接在 Nor Flash 中执行，无需复制到 RAM。这种模式的优点包括：

- 启动速度快，无需等待代码复制
- RAM 资源需求低，适合资源受限的系统
- 适用于对启动时间敏感的应用

缺点是：

- 应用程序运行速度受限于 Flash 读取速度
- 无法对代码段进行加密保护
- 部分微控制器不支持 Flash XIP

**模式二：加载复制启动**

启动加载器将应用程序代码从 Nor Flash 复制到 RAM 中执行。这种模式的优点包括：

- 应用程序运行速度快（RAM 访问速度远高于 Flash）
- 可以对代码进行加密传输和动态解密
- 适合需要频繁更新或大型应用程序的场景

缺点是：

- 需要较大的 RAM 空间
- 启动时间较长（需要复制代码）
- 设计复杂度较高

---

## 双镜像备份

双镜像备份是提高系统可靠性的关键技术，通过在 Flash 中保存两个独立的应用程序镜像，确保在主镜像损坏时能够回退到备份镜像继续运行。

### 镜像分区设计

典型的双镜像分区布局如下：

| 分区 | 起始地址 | 大小 | 用途 |
|------|----------|------|------|
| Bootloader | 0x00000000 | 64 KB | 启动加载器 |
| 镜像 A | 0x00010000 | 512 KB | 主应用程序镜像 |
| 镜像 B | 0x00090000 | 512 KB | 备份应用程序镜像 |
| 升级包 | 0x00110000 | 256 KB | 接收新版本固件 |
| 参数区 | 0x00150000 | 64 KB | 配置参数和状态信息 |
| 数据区 | 0x00160000 | 剩余空间 | 日志存储、用户数据 |

### 镜像状态管理

启动加载器需要维护每个镜像的状态信息，包括：

```c
typedef struct {
    uint32_t version;           // 镜像版本号
    uint32_t size;              // 镜像大小
    uint32_t crc32;             // CRC32 校验值
    uint32_t timestamp;         // 镜像创建时间
    uint8_t  status;            // 状态：有效、无效、测试中
    uint8_t  boot_count;        // 启动次数
    uint8_t  fail_count;        // 连续失败次数
    uint8_t  reserved;
} image_header_t;

#define IMAGE_STATUS_INVALID    0x00
#define IMAGE_STATUS_VALID       0x01
#define IMAGE_STATUS_TESTING    0x02
#define IMAGE_STATUS_UPGRADING  0x03
```

### 启动选择策略

启动加载器根据以下优先级选择启动镜像：

**策略一：主镜像优先**

1. 检查镜像 A 状态是否为 VALID
2. 验证镜像 A 的 CRC32 校验
3. 如果校验通过，启动镜像 A
4. 如果校验失败，检查镜像 B 状态
5. 如果镜像 B 有效且校验通过，启动镜像 B
6. 否则进入恢复模式

**策略二：版本号优先**

1. 比较镜像 A 和镜像 B 的版本号
2. 选择版本号较高的镜像作为主启动镜像
3. 验证选中镜像的完整性
4. 如果主镜像损坏，尝试启动备份镜像

**策略三：尝试升级镜像**

1. 检查是否存在正在测试的升级镜像
2. 如果存在，优先启动升级镜像
3. 升级镜像启动成功后，将其标记为主镜像
4. 如果升级镜像启动失败，回退到原镜像

### 镜像更新流程

双镜像机制的核心价值在于支持安全的固件升级。典型的更新流程如下：

1. **接收新固件**：将新版本固件写入升级包区域
2. **校验新固件**：验证新固件的完整性和合法性
3. **更新备份镜像**：将新固件复制到备份镜像区域
4. **标记升级状态**：设置升级状态标志，标记新镜像为测试状态
5. **重启系统**：系统重启后，启动加载器检测到升级标志
6. **启动新镜像**：加载并运行新镜像
7. **确认或回退**：
   - 如果新镜像运行正常，设置其为主镜像状态
   - 如果新镜像启动失败或运行异常，回退到原镜像

---

## 增量升级

增量升级（Delta Upgrade）通过仅传输和应用固件差异来实现升级，可以显著减少升级包大小和传输时间。本节介绍基于 Nor Flash 的增量升级方案设计。

### 增量升级原理

增量升级的核心思想是计算旧固件与新固件之间的差异（Delta），生成差分包。升级时，将差分包应用到旧固件上，生成新固件。常用的差分算法包括：

- **Bsdiff**：高效的二进制差分算法，适合固件升级
- **Xdelta**：通用差分算法，支持压缩
- **Roffed**：专为嵌入式系统设计的轻量级差分算法

### 差分包格式设计

```c
typedef struct {
    uint32_t magic;             // 差分包标识 (0x44495441 'DITA')
    uint16_t version;           // 差分包格式版本
    uint16_t flags;             // 标志位
    uint32_t src_version;       // 源固件版本
    uint32_t dst_version;       // 目标固件版本
    uint32_t src_size;          // 源固件大小
    uint32_t dst_size;          // 目标固件大小
    uint32_t diff_size;         // 差分包大小
    uint32_t diff_crc;          // 差分包 CRC32
    uint32_t reserved[4];
} delta_header_t;
```

### 增量升级流程

**服务端：生成差分包**

1. 获取旧版本固件和新版本固件
2. 使用差分算法计算差异
3. 添加差分包头部信息
4. 对差分包进行压缩和签名
5. 将差分包分发到设备端

**设备端：应用差分包**

1. 接收并验证差分包
2. 从 Flash 中读取当前固件
3. 应用差分变化，生成新固件
4. 将新固件写入备份镜像区域
5. 校验新固件完整性
6. 重启并切换到新固件

### 差分算法实现

以下是使用 Bsdiff 算法进行增量升级的简化示例：

```c
#include <stdint.h>
#include <string.h>

// Bsdiff 差分应用函数
int32_t bspatch_apply(const uint8_t *old_data, size_t old_size,
                      const uint8_t *patch_data, size_t patch_size,
                      uint8_t *new_data, size_t new_size)
{
    // 解析差分包头部
    const bspatch_header_t *header = (const bspatch_header_t *)patch_data;

    if (memcmp(header->magic, "BSPATCH", 7) != 0) {
        return -1;  // 无效的差分包
    }

    // 差分应用实现
    // 1. 复制未修改的部分
    // 2. 应用追加数据
    // 3. 应用二分查找差异

    // 此处省略详细的 Bspatch 实现...

    return 0;
}

// 增量升级函数
int32_t incremental_upgrade(const uint8_t *delta_pkg, size_t delta_size,
                            flash_addr_t src_addr, flash_addr_t dst_addr)
{
    // 1. 读取源固件
    uint8_t *old_firmware = malloc(SOURCE_FW_SIZE);
    if (old_firmware == NULL) {
        return -1;
    }

    nor_flash_read(src_addr, old_firmware, SOURCE_FW_SIZE);

    // 2. 应用差分包
    uint8_t *new_firmware = malloc(NEW_FW_SIZE);
    if (new_firmware == NULL) {
        free(old_firmware);
        return -1;
    }

    int32_t ret = bspatch_apply(old_firmware, SOURCE_FW_SIZE,
                                delta_pkg, delta_size,
                                new_firmware, NEW_FW_SIZE);

    if (ret != 0) {
        free(old_firmware);
        free(new_firmware);
        return ret;
    }

    // 3. 写入目标位置
    nor_flash_erase(dst_addr, NEW_FW_SIZE);
    nor_flash_write(dst_addr, new_firmware, NEW_FW_SIZE);

    // 4. 验证新固件
    uint32_t crc = crc32_compute(new_firmware, NEW_FW_SIZE);
    if (crc != get_image_crc(dst_addr)) {
        free(old_firmware);
        free(new_firmware);
        return -2;  // CRC 校验失败
    }

    free(old_firmware);
    free(new_firmware);

    return 0;
}
```

### 增量升级的注意事项

1. **Flash 空间要求**：增量升级需要同时存储源固件和目标固件，确保 Flash 空间充足
2. **内存需求**：差分应用过程中需要较大的临时缓冲区，考虑嵌入式系统的内存限制
3. **可靠性保障**：差分应用失败可能导致固件损坏，需要实现完整的回退机制
4. **兼容性处理**：不同芯片平台可能需要不同的差分算法实现

---

## 恢复模式

恢复模式（Recovery Mode）是系统在正常启动失败时进入的特殊运行模式，用于修复系统固件或恢复出厂设置。本节详细介绍恢复模式的设计和实现。

### 进入恢复模式的条件

系统可以通过以下方式进入恢复模式：

**硬件触发方式**

1. **专用引脚**：设计专用恢复引脚，上电时检测引脚状态
2. **按键组合**：检测特定按键组合（如 BOOT + RESET）
3. ** JTAG 接口**：通过调试接口触发恢复模式

**软件触发方式**

1. **启动失败计数**：连续多次启动失败后自动进入
2. **看门狗复位**：看门狗超时复位后进入
3. **升级失败标志**：固件升级过程中断，设置恢复标志

### 恢复模式功能设计

恢复模式应提供以下核心功能：

**基本功能**

1. **串口通信**：通过 UART 与上位机通信
2. **固件下载**：接收新的固件文件
3. **固件烧录**：将接收的固件写入 Flash
4. **固件校验**：验证烧录固件的完整性

**高级功能**

1. **Flash 擦除**：支持完全擦除或选择性擦除
2. **参数恢复**：恢复出厂默认配置
3. **诊断信息**：读取和显示系统状态
4. **镜像管理**：查看和管理 Flash 中的多个镜像

### 恢复模式通信协议

恢复模式通常通过串口实现固件下载，常用的协议包括：

**YMODEM 协议**

- 支持文件传输和校验
- 断点续传功能
- 广泛支持，兼容性好

**XMODEM 协议**

- 简单易实现
- 128 字节数据包
- 适合资源受限的系统

**自定义协议**

- 针对特定应用优化
- 可定制校验机制
- 需要配套上位机软件

### 恢复模式实现示例

```c
#include <stdint.h>
#include <stdbool.h>

// 恢复模式状态机
typedef enum {
    RECOVERY_IDLE,              // 空闲状态
    RECOVERY_WAITING,          // 等待主机连接
    RECOVERY_RECEIVING,        // 接收固件数据
    RECOVERY_VERIFYING,        // 验证固件
    RECOVERY_PROGRAMMING,      // 编程 Flash
    RECOVERY_COMPLETE,         // 完成
    RECOVERY_ERROR             // 错误状态
} recovery_state_t;

// 恢复模式上下文
typedef struct {
    recovery_state_t state;
    uint32_t         total_size;
    uint32_t         received_size;
    uint32_t         packet_count;
    uint32_t         crc32;
    flash_addr_t     write_addr;
    uint8_t          retry_count;
} recovery_context_t;

// 恢复模式主函数
void enter_recovery_mode(void)
{
    // 1. 初始化串口
    uart_init(115200);

    // 2. 发送欢迎信息
    uart_send_string("\r\n=== Nor Flash Recovery Mode ===\r\n");
    uart_send_string("Version: 1.0.0\r\n");
    uart_send_string("Flash Size: ");
    uart_send_string(flash_size_str);
    uart_send_string("\r\n\r\n");

    // 3. 初始化恢复上下文
    recovery_context_t ctx = {0};
    ctx.state = RECOVERY_WAITING;
    ctx.write_addr = IMAGE_A_START;

    // 4. 进入状态机循环
    while (1) {
        switch (ctx.state) {
            case RECOVERY_WAITING:
                handle_waiting_state(&ctx);
                break;

            case RECOVERY_RECEIVING:
                handle_receiving_state(&ctx);
                break;

            case RECOVERY_VERIFYING:
                handle_verifying_state(&ctx);
                break;

            case RECOVERY_PROGRAMMING:
                handle_programming_state(&ctx);
                break;

            case RECOVERY_COMPLETE:
                handle_complete_state(&ctx);
                break;

            case RECOVERY_ERROR:
                handle_error_state(&ctx);
                break;
        }
    }
}

// 等待状态处理
void handle_waiting_state(recovery_context_t *ctx)
{
    uart_send_string("Waiting for firmware...\r\n");
    uart_send_string("Send firmware using YMODEM protocol\r\n");

    // 等待 YMODEM 启动
    int32_t timeout = 30000;  // 30秒超时
    uint32_t start = get_tick_ms();

    while (get_tick_ms() - start < timeout) {
        if (uart_available() > 0) {
            uint8_t c = uart_read();
            if (c == YMODEM_SOH || c == YMODEM_STX) {
                // 开始接收
                ctx->state = RECOVERY_RECEIVING;
                ctx->packet_count = 0;
                ctx->received_size = 0;
                ctx->crc32 = 0;
                return;
            }
        }
        delay_ms(10);
    }

    // 超时，进入错误状态
    uart_send_string("Timeout, waiting for host\r\n");
    ctx->state = RECOVERY_ERROR;
    ctx->retry_count++;
}
```

### 恢复模式的保护机制

为防止恢复模式被恶意利用或误操作，需要实现以下保护机制：

1. **进入条件限制**：设置特定的硬件条件或超时条件才能进入
2. **通信加密**：对传输的固件进行加密，防止被截获分析
3. **签名验证**：要求固件包含有效的数字签名
4. **次数限制**：限制连续进入恢复模式的次数
5. **自动退出**：长时间无操作自动退出恢复模式

---

## 本章小结

本章详细介绍了基于 Nor Flash 的启动加载器设计要点：

1. **启动流程概述**：从上电复位到应用程序跳转的完整流程，介绍了 XIP 直接启动和加载复制启动两种模式的特点和适用场景。

2. **双镜像备份**：通过在 Flash 中保存主备两个镜像，结合版本管理和状态机机制，显著提高了系统的可靠性和升级安全性。

3. **增量升级**：通过传输和应用固件差异，减小升级包大小，适用于网络带宽受限或升级成本敏感的场景。

4. **恢复模式**：设计了多种进入条件和丰富的恢复功能，配合完善的保护机制，确保系统在固件损坏时能够恢复运行。

掌握这些技术，可以设计出高可靠、易维护的 Nor Flash 启动加载器系统。

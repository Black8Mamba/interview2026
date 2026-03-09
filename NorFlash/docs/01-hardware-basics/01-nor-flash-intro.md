# Nor Flash 简介与分类

## Nor Flash 与 Nand Flash 对比

### 存储原理对比

**Nor Flash（Not OR Flash）**

Nor Flash 采用 NOR 门结构，其存储单元以并联方式排列。每个存储单元的一端连接到字线（Word Line），另一端连接到位线（Bit Line）。这种架构允许随机访问任意存储单元，类似于 DRAM 的访问方式。

- **单元结构**：每个存储单元由一个浮栅晶体管（Floating Gate Transistor）构成
- **读取方式**：通过字线选中目标单元，位线直接读取浮栅中的电荷状态
- **寻址方式**：支持字节级随机寻址，可直接执行代码（XIP, eXecute In Place）

**Nand Flash（Not AND Flash）**

Nand Flash 采用 NAND 门结构，存储单元以串联方式排列。多个存储单元组成一个 NAND 串（NAND String），多个 NAND 串再并联形成存储阵列。

- **单元结构**：同样使用浮栅晶体管（Floating Gate Transistor），但以 NAND 串形式组织
- **读取方式**：需要按页（Page）顺序读取，无法随机访问单个字节
- **寻址方式**：以页为单位进行读写，以块（Block）为单位进行擦除

### 性能对比

| 特性 | Nor Flash | Nand Flash |
|------|-----------|------------|
| **读取速度** | 50-300 MB/s（并行接口）<br>SPI接口：50-133 MB/s | 200-400 MB/s（并行接口）<br>ONFI: 400-800 MB/s |
| **写入速度** | 0.5-2 MB/s | 10-50 MB/s |
| **擦除速度** | 0.5-2 MB/s | 10-50 MB/s（块擦除） |
| **随机访问** | 支持字节级随机访问 | 仅支持页级顺序访问 |
| **可直接执行代码** | 支持（XIP） | 不支持 |

**解读**：

- Nor Flash 的读取速度虽然略慢于 Nand Flash，但其随机访问特性使其可以直接在芯片内执行代码
- Nand Flash 在写入和擦除速度上具有明显优势，适合大容量数据存储场景
- SPI 接口的 Nor Flash 读取速度受限于时钟频率，但新一代高速 SPI（Octal SPI）已接近传统并行接口性能

### 可靠性对比

| 特性 | Nor Flash | Nand Flash |
|------|-----------|------------|
| **使用寿命** | 10,000-100,000 次擦写 | 1,000-10,000 次擦写 |
| **位翻转（Bit Flip）概率** | 较低 | 较高 |
| **坏块管理** | 无需坏块管理 | 需要坏块管理机制 |
| **位宽** | x8/x16/x32 | x8/x16 |
| **数据保持时间** | 10-20 年 | 10 年 |

**解读**：

- Nor Flash 的擦写寿命显著高于 Nand Flash，在需要频繁更新的应用场景中更具优势
- Nand Flash 存在位翻转问题，需要采用 ECC（Error Correcting Code）纠错机制
- Nand Flash 在使用过程中会产生坏块，需要在固件层面实现坏块管理（BBM, Bad Block Management）

### 成本对比

| 特性 | Nor Flash | Nand Flash |
|------|-----------|------------|
| **单位成本** | 较高（约 2-5 倍） | 较低 |
| **最小容量** | 512 Kbit - 2 Gbit | 1 Gbit - 512 Gbit |
| **芯片面积** | 较大（NOR 单元面积约为 NAND 的 2-3 倍） | 较小 |
| **制造成本** | 较高 | 较低 |

**解读**：

- Nor Flash 由于其复杂的单元结构和较大的单元面积，单位存储成本显著高于 Nand Flash
- Nor Flash 最小容量通常在 512 Kbit 到 2 Gbit 之间，而 Nand Flash 可以轻松达到数 Gbit 到数百 Gbit
- 对于需要大容量存储的应用，Nand Flash 是更经济的选择

### 适用场景分析

**Nor Flash 适用场景**：

1. **代码存储与直接执行（XIP）**
   - 嵌入式系统的启动代码存储
   - FPGA 配置文件存储
   - 微控制器内置存储扩展

2. **低容量高可靠性存储**
   - 汽车电子（ADAS、车载娱乐系统）
   - 工业控制（PLC、变频器）
   - 医疗设备

3. **需要随机访问的场景**
   - 参数配置存储
   - 密钥和安全凭证存储
   - 固件升级存储

**Nand Flash 适用场景**：

1. **大容量数据存储**
   - USB 闪存盘
   - SD 卡、eMMC
   - 固态硬盘（SSD）

2. **需要高速写入的场景**
   - 多媒体数据存储
   - 日志存储
   - 数据采集

---

## SPI Nor Flash 分类

SPI（Serial Peripheral Interface）Nor Flash 通过串行接口进行数据传输，相比并行接口具有引脚数量少、成本低、易于布局等优点。

### 标准 SPI（Single SPI）

标准 SPI 是最初级的 SPI Nor Flash 模式，使用单根数据线进行全双工通信。

**技术特点**：

- **数据线**：1-bit 数据传输（DI/DO 分离或共用）
- **时钟频率**：常见 50MHz、100MHz，最高可达 133MHz
- **传输模式**：全双工通信
- **命令序列**：1-1-1 模式（1 命令线、1 地址线、1 数据线）

**工作原理**：

1. 主机发送 8 位命令码
2. 主机发送 24 位地址（3 字节）
3. 从机通过 DO 线返回数据
4. 每字节数据传输需要 8 个时钟周期

**典型性能**：

- 读取速度：50-133 MB/s（@ 50-133MHz）
- 写入速度：0.5-2 MB/s

**代表芯片**：

- Winbond W25Q80DV（8Mbit, 108MHz）
- Macronix MX25L8006E（8Mbit, 100MHz）
- GigaDevice GD25Q80C（8Mbit, 120MHz）

### Dual SPI（双线模式）

Dual SPI 模式通过同时使用两根数据线（DI 和 DO）来提升数据传输速率。

**技术特点**：

- **数据线**：2-bit 数据传输
- **时钟频率**：与标准 SPI 相同（50-133MHz）
- **传输模式**：半双工通信
- **命令序列**：1-2-2 模式

**工作原理**：

1. 主机发送命令码（通过 DI 线）
2. 主机发送地址（通过 DI/DO 双线）
3. 数据通过 DI/DO 双线同时传输（每时钟周期传输 2 位）

**性能提升**：

- 读取速度：提升约 2 倍（100-266 MB/s）
- 适用于连续数据读取场景

**代表芯片**：

- Winbond W25Q128JV（128Mbit, Dual/Quad SPI）
- Micron MT25QL128ABA（128Mbit）

### Quad SPI（四线模式）

Quad SPI 模式使用四根数据线进行数据传输，进一步提升性能。

**技术特点**：

- **数据线**：4-bit 数据传输
- **时钟频率**：可达 133MHz 或更高
- **传输模式**：半双工通信
- **命令序列**：1-4-4 模式
- **新增引脚**：IO0、IO1、IO2（/WP）、IO3（/HOLD）

**工作原理**：

1. 主机发送命令码（通过 IO0）
2. 主机发送地址（通过 IO0-IO3 四线）
3. 数据通过 IO0-IO3 四线同时传输（每时钟周期传输 4 位）

**性能提升**：

- 读取速度：提升约 4 倍（200-532 MB/s）
- 写入速度：提升约 4 倍

**代表芯片**：

- Winbond W25Q256JV（256Mbit, Quad SPI）
- Macronix MX25L25645G（256Mbit, 133MHz）
- GigaDevice GD25Q256C（256Mbit）

### Octal SPI（八线模式）

Octal SPI 是最新的 SPI Nor Flash 标准，使用八根数据线进行数据传输，性能接近并行接口。

**技术特点**：

- **数据线**：8-bit 数据传输
- **时钟频率**：可达 200MHz
- **传输模式**：全双工/半双工
- **命令序列**：1-8-8 模式
- **新增引脚**：IO0-IO7（8 根数据线）

**工作原理**：

1. 主机发送命令码（通过 IO0）
2. 主机发送地址（通过 IO0-IO7 八线）
3. 数据通过 IO0-IO7 八线同时传输（每时钟周期传输 8 位）

**性能提升**：

- 读取速度：提升约 8 倍（最高可达 400-800 MB/s）
- 接近传统并行 Nor Flash 性能
- 支持 DDR（Double Data Rate）模式

**代表芯片**：

- Winbond W25Q512JVQ（512Mbit, Octal SPI, 200MHz）
- Macronix MX25UM51245G（512Mbit, Octal SPI）
- Micron MT35XU512ABA（512Mbit, Octal SPI）

### 各模式传输速率对比

| SPI 模式 | 数据线宽度 | 时钟频率 | 理论带宽 | 读取速度示例 |
|----------|------------|----------|----------|--------------|
| Single SPI | 1-bit | 133MHz | 16.6 MB/s | 16.6 MB/s |
| Dual SPI | 2-bit | 133MHz | 33.3 MB/s | 33.3 MB/s |
| Quad SPI | 4-bit | 133MHz | 66.6 MB/s | 66.6 MB/s |
| Octal SPI | 8-bit | 200MHz | 200 MB/s | 200 MB/s |
| Octal SPI DDR | 8-bit | 200MHz | 400 MB/s | 400 MB/s |

**注**：实际可用带宽会因协议开销、命令序列等因素低于理论值。

### SPI Nor Flash 选型要点

1. **容量需求**：根据代码/数据大小选择合适容量，考虑预留空间
2. **速度要求**：高速读取场景优先选择 Quad SPI 或 Octal SPI
3. **接口兼容性**：确认主控芯片支持的 SPI 模式
4. **电压等级**：3.3V、1.8V、1.2V 等，根据系统电源设计选择
5. **封装形式**：SOP8、USON8、WSON8 等，根据 PCB 布局选择
6. **可靠性要求**：汽车级、工业级、商业级

---

## 并行 Nor Flash 分类

并行 Nor Flash 使用多根数据线同时传输数据，相比 SPI 接口具有更高的带宽，但引脚数量较多。

### CFI 接口（Common Flash Interface）

CFI 是 JEDEC 制定的标准化 Flash 接口规范，定义了 Flash 芯片的查询和识别机制。

**技术特点**：

- **标准化查询**：通过特定地址读取芯片识别信息（厂商 ID、器件 ID、容量等）
- **统一命令集**：支持统一的擦除、写入命令序列
- **兼容性**：不同厂商的 CFI 兼容芯片可以互换
- **数据线宽度**：x8、x16 可选

**CFI 查询流程**：

1. 写入 CFI 查询命令（0x98）
2. 读取地址 0x10-0x2F 获取芯片信息
3. 包括：厂商标识、器件标识、容量、块大小、电压等信息

**CFI 模式优势**：

- 简化软件驱动开发
- 支持自动识别芯片类型
- 便于产品升级替换

**典型 CFI 芯片**：

- Spansion S29GL512S（512Mbit, CFI）
- Micron MT28EW（256Mbit-1Gbit, CFI）
- Macronix MX68GL1G（1Gbit, CFI）

### FPGA 专用接口

FPGA 专用 Nor Flash 通常具有针对 FPGA 配置优化的接口时序。

**技术特点**：

- **高速配置**：针对 FPGA 配置时序优化
- **菊花链支持**：支持多片级联配置
- **可选 x8/x16 模式**：灵活选择数据位宽
- **专用控制信号**：/OE、/CE、/WE 等精细控制

**FPGA 配置模式**：

- **被动串行（PS）**：串行配置，速度较低但引脚少
- **被动并行（PP）**：并行配置，速度高
- **主动串行（AS）**：Xilinx 7 系列专用
- **被动快速（Fast PASS）**：高速配置模式

**典型 FPGA 配置芯片**：

- Xilinx Platform Flash（XCFxxP 系列）
- Intel EPCQ（串行配置）
- Micron N25Q（SPI/并行兼容）

### 异步并行接口特点

**接口信号**：

- **数据总线**：D[15:0]（x16）或 D[7:0]（x8）
- **地址总线**：A[23:0]（最大 16MB @ x16）
- **控制信号**：
  - /CE（Chip Enable）
  - /OE（Output Enable）
  - /WE（Write Enable）
  - /RESET（复位）
  - /RYBY（就绪/忙碌）

**时序特性**：

- **读取周期**：约 70-120ns
- **写入周期**：约 10-20us
- **块擦除时间**：约 0.5-2 秒

**优势**：

- 高带宽：可达 400MB/s（x16, 100MHz）
- 低延迟：随机访问延迟低
- 直接内存映射：可像 SRAM 一样直接寻址

**劣势**：

- 引脚数量多（30-56 引脚）
- PCB 布局要求高
- 成本较高

**典型并行 Nor Flash**：

- Winbond W29GLxxxC 系列（128Mbit-1Gbit）
- Macronix MX29GL 系列（128Mbit-1Gbit）
- Micron MT28APxxx 系列（256Mbit-1Gbit）

---

## 主流芯片厂商与产品线

### Winbond（华邦电子）

**公司概况**：

- 总部：台湾新竹
- 成立时间：1987 年
- 主要产品：Nor Flash、Nand Flash、DRAM

**SPI Nor Flash 产品线**：

| 系列 | 容量范围 | 接口 | 电压 | 特点 |
|------|----------|------|------|------|
| W25QxxJV | 1Mbit-512Mbit | Single/Dual/Quad/Octal SPI | 3.3V/1.8V | 主流产品，高可靠性 |
| W25QxxFV | 1Mbit-128Mbit | Quad SPI | 3.3V | 增强型 |
| W25NxxG | 1Gbit-2Gbit | Toggle DDR | 3.3V | SLC Nand Flash |
| W25Mxx | 512Mbit-2Gbit | Quad SPI | 3.3V | 串行 Nand Flash |

**代表产品**：

- **W25Q256JV**：256Mbit, Quad SPI, 133MHz, 3.3V
- **W25Q512JVQ**：512Mbit, Octal SPI, 200MHz, 1.8V
- **W25Q80DV**：8Mbit, Dual SPI, 108MHz, 3.3V

**优势**：

- 产品线齐全，覆盖全容量段
- Octal SPI 产品领先
- 稳定供货，产能充足

### Micron（镁光科技）

**公司概况**：

- 总部：美国爱达荷州博伊斯
- 成立时间：1978 年
- 主要产品：DRAM、Nand Flash、Nor Flash、传感器

**Nor Flash 产品线**：

| 系列 | 容量范围 | 接口 | 电压 | 特点 |
|------|----------|------|------|------|
| MT25QL | 128Mbit-512Mbit | Quad SPI | 3.3V/1.8V | 高性能 |
| MT35X | 256Mbit-2Gbit | Octal SPI | 1.8V/1.2V | 面向高性能应用 |
| MT28EW | 256Mbit-1Gbit | 并行 CFI | 3.3V | 大容量并行 |
| MT28AP | 256Mbit-1Gbit | 并行 CFI | 1.8V | 低功耗并行 |

**代表产品**：

- **MT25QL128ABA**：128Mbit, Quad SPI, 166MHz
- **MT35XU512ABA**：512Mbit, Octal SPI, 200MHz
- **MT28EW512ABA**：512Mbit, 并行 CFI, x16

**优势**：

- 先进制程，技术领先
- 高性能产品突出
- 汽车级产品线完整

### Macronix（旺宏电子）

**公司概况**：

- 总部：台湾新竹
- 成立时间：1989 年
- 主要产品：Nor Flash、Nand Flash、ROM

**Nor Flash 产品线**：

| 系列 | 容量范围 | 接口 | 电压 | 特点 |
|------|----------|------|------|------|
| MX25L | 256Kbit-256Mbit | Single/Dual/Quad SPI | 3.3V/1.8V | 主流产品 |
| MX25UM | 256Mbit-1Gbit | Octal SPI | 1.8V/1.2V | 高性能 |
| MX29GL | 128Mbit-1Gbit | 并行 CFI | 3.3V | 大容量并行 |
| MX66L | 512Mbit-2Gbit | Octal SPI | 1.8V | 超大容量 |

**代表产品**：

- **MX25L25645G**：256Mbit, Quad SPI, 133MHz
- **MX25UM51245G**：512Mbit, Octal SPI, 200MHz
- **MX29GL512GH**：512Mbit, 并行 CFI, x16

**优势**：

- Nor Flash 产能全球第一
- 48nm 及更先进制程
- 高可靠性汽车级产品

### ISSI（芯成半导体）

**公司概况**：

- 总部：美国加州圣何塞
- 成立时间：1988 年
- 主要产品：SRAM、DRAM、Nor Flash

**Nor Flash 产品线**：

| 系列 | 容量范围 | 接口 | 电压 | 特点 |
|------|----------|------|------|------|
| IS25L | 256Kbit-256Mbit | Single/Dual/Quad SPI | 3.3V/1.8V | 高性价比 |
| IS25LP | 128Mbit-512Mbit | Quad SPI | 1.8V | 低功耗 |
| IS25WP | 128Mbit-256Mbit | Quad SPI | 1.8V | 宽压 |
| IS66/IS67 | 256Mbit-1Gbit | 并行 CFI | 3.3V | 大容量 |

**代表产品**：

- **IS25WP256D**：256Mbit, Quad SPI, 133MHz, 1.8V
- **IS66WVH512M8**：512Mbit, Octal SPI, 200MHz
- **IS67WVH512M8**：512Mbit, Octal SPI, 1.2V

**优势**：

- 汽车级产品认证完整（IATF16949）
- 专注高可靠性市场
- 本地化技术支持

### GigaDevice（兆易创新）

**公司概况**：

- 总部：中国北京
- 成立时间：2005 年
- 主要产品：Nor Flash、MCU、传感器

**Nor Flash 产品线**：

| 系列 | 容量范围 | 接口 | 电压 | 特点 |
|------|----------|------|------|------|
| GD25Q | 1Mbit-512Mbit | Single/Dual/Quad SPI | 3.3V/1.8V | 主流产品 |
| GD25LT | 16Mbit-256Mbit | Quad SPI | 1.8V | 低功耗 |
| GD25LD | 1Mbit-64Mbit | Dual SPI | 3.3V | 高性价比 |
| GD25B | 1Mbit-128Mbit | Single SPI | 3.3V | 基本型 |

**代表产品**：

- **GD25Q256C**：256Mbit, Quad SPI, 120MHz
- **GD25Q512MC**：512Mbit, Octal SPI, 200MHz
- **GD25F512M**：512Mbit, Octal SPI, 1.8V

**优势**：

- 中国本土最大 Nor Flash 供应商
- 供货稳定，价格竞争力强
- 完善的技术支持团队
- 55nm 先进制程

### 厂商产品线对比总结

| 厂商 | 容量范围 | SPI产品 | 并行产品 | 特色 |
|------|----------|---------|----------|------|
| Winbond | 1Mbit-512Mbit | 丰富 | 丰富 | Octal SPI 领先 |
| Macronix | 256Kbit-2Gbit | 丰富 | 丰富 | 产能第一 |
| Micron | 128Mbit-2Gbit | 丰富 | 丰富 | 高性能 |
| ISSI | 256Kbit-1Gbit | 较全 | 较全 | 汽车级 |
| GigaDevice | 1Mbit-512Mbit | 丰富 | 较少 | 本土化 |

### 选型建议

**选择策略**：

1. **成本优先**：GigaDevice、ISSI
2. **高性能**：Micron、Macronix
3. **供货稳定**：Winbond、Macronix
4. **汽车级**：Micron、Macronix、ISSI
5. **本土支持**：GigaDevice

**注意事项**：

- 确认主控芯片的 Flash 控制器兼容性
- 关注供应链稳定性
- 考虑长期供货保证
- 验证样品和量产交期

---

## 本章小结

本章详细介绍了 Nor Flash 的基础知识和分类：

1. **Nor Flash vs Nand Flash**：Nor Flash 以其随机访问能力和可直接执行代码的特性，适用于代码存储和低容量高可靠性场景；Nand Flash 则在大容量数据存储领域具有成本优势。

2. **SPI Nor Flash 分类**：从 Single SPI 到 Octal SPI，数据宽度从 1-bit 发展到 8-bit，读取性能从 16.6 MB/s 提升到 400 MB/s，满足不同应用场景的需求。

3. **并行 Nor Flash**：CFI 接口提供了标准化兼容方案，FPGA 专用接口针对配置场景优化，异步并行接口则提供了最高带宽。

4. **主流厂商**：Winbond、Macronix、Micron、ISSI、GigaDevice 等厂商提供了丰富的产品线，开发者可根据容量、性能、成本、供货等因素进行选型。

掌握这些基础知识，将有助于后续章节对 Nor Flash 驱动开发的学习。

# 术语表

## 1. 专业术语解释

### A

**Asynchronous（异步）**
指数据传输不依赖时钟信号，Nor Flash并行接口采用异步方式，地址/数据直接有效即可操作，无需时钟同步。

**Array（存储阵列）**
Nor Flash内部的存储单元矩阵，是实际存储数据的核心区域，包含数百万个浮栅晶体管。

### B

**Bit Flip（位翻转）**
存储单元中的数据位从0变为1或从1变为0的错误现象，通常由辐射、电压波动或单元老化引起，需要ECC纠错。

**Block（块）**
Nor Flash的中间擦除单元，通常为32KB或64KB。介于扇区(Sector)和芯片(Chip)之间的大小。

**Bootloader（引导加载程序）**
存储在Nor Flash起始位置的程序，负责初始化硬件、加载应用程序，是嵌入式系统启动的关键组件。

**Burst Read（突发读取）**
连续读取多个字节的高效方式，只需发送一次起始地址，后续地址自动递增，减少命令开销。

### C

**CFI（Common Flash Interface）**
并行Nor Flash的标准化查询接口，通过特定命令序列可获取芯片的供应商、容量、擦除块大小等参数信息。

**Chip Erase（整片擦除）**
一次性擦除整个芯片所有内容，是最耗时的擦除操作，通常需要数十秒到数分钟。

**Clock（时钟）**
SPI接口的同步信号，决定数据传输速率。时钟频率越高，传输速度越快，但信号完整性要求越高。

**CS# / CS（Chip Select）**
片选信号，低电平有效。用于选中指定的Flash芯片，允许在总线上连接多个设备。

### D

**Data Retention（数据保持时间）**
Nor Flash在断电后能保持存储数据的时间，通常为10-20年。高温环境会显著缩短此时间。

**Die（芯片内核）**
晶圆上切割下来的单个Flash内核，是芯片的核心物理部分，一个封装可能包含多个die。

**DQS（Data Strobe）**
数据选通信号，用于高速数据传输中的数据同步，常见于DDR和Octal SPI模式。

**DTR（Double Transfer Rate）**
双倍数据传输率，在时钟的上升沿和下降沿都传输数据，理论带宽翻倍。

### E

**ECC（Error Correction Code）**
错误校正码，用于检测和纠正存储数据中的位错误。Nor Flash通常在系统层面实现。

**EEPROM（Electrically Erasable Programmable ROM）**
电可擦除可编程只读存储，NOR Flash的前身，支持单字节擦写但容量较小。

**Embedded Algorithm（嵌入式算法）**
在Flash芯片内部实现的擦除/编程算法，简化外部控制器设计，提高操作可靠性。

### F

**Flash Memory（闪存）**
一种非易失性存储器，通过浮栅晶体管存储电荷，断电后数据不丢失。可分为Nor Flash和Nand Flash。

**Flash ID（Flash标识符）**
Flash芯片的唯一识别码，SPI Flash通过0x9F命令读取JEDEC ID，包含厂商代码、芯片类型和容量代码。

### G

**GPIO（General Purpose Input/Output）**
通用输入输出接口，在嵌入式系统中常用于模拟SPI接口（软件模拟SPI）。

### H

**HyperBus（超高速总线）**
Micron推出的高速存储接口标准，支持高达400MHz时钟，用于高性能Nor Flash连接。

### I

**I/O（Input/Output）**
输入输出接口，在Flash中指数据端口的位宽，如x1/x2/x4/x8模式。

**ID（Identification）**
识别码，用于识别Flash芯片类型。JEDEC ID包含3字节：厂商ID(1字节)+类型ID(1字节)+容量ID(1字节)。

### J

**JEDEC（Joint Electron Device Engineering Council）**
电子工业协会，制定Nor Flash等半导体器件的标准，包括命令集、时序参数等。

### L

**Latency（延迟）**
命令发送后到数据返回的等待时间，通常以时钟周期数表示。高频率下实际延迟时间可能更短。

### M

**Memory Map（内存映射）**
将Flash存储空间映射到处理器地址空间，应用程序可以直接通过指针访问Flash地址。

**MTD（Memory Technology Device）**
Linux系统中用于管理闪存设备的抽象层，提供统一的块设备接口。

### N

**Nand Flash**
采用NAND结构的闪存，存储单元串联排列。容量大、成本低，但仅支持页读写不支持随机字节访问。

**Nor Flash**
采用NOR结构的闪存，存储单元并联排列。支持随机字节访问和XIP，但容量和成本较高。

### O

**Octal SPI（八线SPI）**
使用8根数据线的SPI模式，数据宽度为8位，时钟可达200MHz以上，带宽接近并行接口。

**OTP（One Time Programmable）**
一次性可编程区域，编程后不可擦除，用于存储序列号、密钥等防篡改数据。

### P

**Page（页）**
Nor Flash的最小编程单元，通常为256字节或512字节。页编程不能跨页边界。

**Page Program（页编程）**
向Flash写入数据的操作，一次最多写入一页数据。需先执行写使能命令。

**Power-on Reset（上电复位）**
芯片上电后自动进入的初始状态，寄存器复位到默认值，退出任何低功耗模式。

**Program（编程）**
将Flash存储单元从1改为0的操作，需要先擦除（使单元变为1）才能编程。

### Q

**QE（Quad Enable）**
四线使能位，位于状态寄存器2中。置1后启用QSPI模式，数据传输使用4根数据线。

**QSPI（Quad SPI）**
四线SPI接口，数据宽度为4位。相比标准SPI，理论带宽提升4倍。

### R

**Random Access（随机访问）**
Nor Flash的核心优势，可以直接访问任意字节地址，无需先读取整页或整块数据。

**Read While Write（边读边写）**
部分高端Flash支持的功能，允许在同时进行读取和写入操作，提高系统并行性。

**Reset（复位）**
将Flash芯片恢复到初始状态的操作，可通过硬件复位引脚或软件复位命令执行。

### S

**Sector（扇区）**
Nor Flash的最小擦除单元，通常为4KB。部分芯片支持4KB或64KB等多种扇区大小。

**Sector Erase（扇区擦除）**
擦除最小存储单元的操作，将扇区内所有位恢复为1，是最常用的擦除操作。

**SFDP（Serial Flash Discoverable Parameter）**
串行Flash可发现参数标准，通过标准化的数据结构存储芯片参数，方便驱动程序自动识别。

**SOP（Small Outline Package）**
小外形封装，常见的Flash芯片封装形式，如SOP8(8引脚)、SOP16(16引脚)。

**SPI（Serial Peripheral Interface）**
串行外设接口，Flash芯片最常用的通信协议，采用主从架构和全双工通信。

**SRAM（Static Random Access Memory）**
静态随机存取存储器，Nor Flash编程时通常需要SRAM缓冲区存储待写入数据。

### T

**Throughput（吞吐量）**
数据传输速率，通常以MB/s表示。计算公式：时钟频率 × 数据线数 / 8。

### V

**VCC（电路电源）**
Flash芯片的正极电源引脚，通常为2.7V-3.6V（标准电压）或1.7V-2.0V（低电压）。

### W

**Wear Leveling（磨损均衡）**
通过动态分配存储位置使各扇区擦写次数均衡的技术，延长Flash使用寿命。

**WEL（Write Enable Latch）**
写使能锁存器，状态寄存器中的位。必须置1才能执行写入和擦除操作。

**WP#（Write Protect）**
写保护引脚，低电平有效。硬件保护Flash指定区域不被意外修改。

### X

**XIP（eXecute In Place）**
原地执行，Nor Flash支持直接从Flash芯片中读取指令并执行，无需先加载到RAM。

## 2. 缩写对照表

### 通信接口

| 缩写 | 全称 | 中文 |
|------|------|------|
| SPI | Serial Peripheral Interface | 串行外设接口 |
| DSPI | Dual SPI | 双线串行接口 |
| QSPI | Quad SPI | 四线串行接口 |
| Octal SPI | Octal SPI | 八线串行接口 |
| DTR | Double Transfer Rate | 双倍传输率 |
| DDR | Double Data Rate | 双倍数据率 |
| I/O | Input/Output | 输入/输出 |
| GPIO | General Purpose Input/Output | 通用输入/输出 |

### 存储相关

| 缩写 | 全称 | 中文 |
|------|------|------|
| NOR | Not OR | 非或（闪存类型） |
| NAND | Not AND | 与非（闪存类型） |
| EEPROM | Electrically Erasable Programmable ROM | 电可擦除可编程只读存储器 |
| SRAM | Static Random Access Memory | 静态随机存取存储器 |
| DRAM | Dynamic Random Access Memory | 动态随机存取存储器 |

### 操作命令

| 缩写 | 全称 | 中文 |
|------|------|------|
| READ | Read Data | 读取数据 |
| WRITE | Write Data | 写入数据 |
| PROG | Program | 编程 |
| ERASE | Erase | 擦除 |
| SE | Sector Erase | 扇区擦除 |
| BE | Block Erase | 块擦除 |
| CE | Chip Erase | 整片擦除 |
| PP | Page Program | 页编程 |
| WE | Write Enable | 写使能 |
| WD | Write Disable | 写禁止 |

### 参数与状态

| 缩写 | 全称 | 中文 |
|------|------|------|
| ID | Identification | 标识符 |
| JEDEC ID | JEDEC Identification | JEDEC标准标识 |
| SFDP | Serial Flash Discoverable Parameter | 串行闪存可发现参数 |
| CFI | Common Flash Interface | 通用闪存接口 |
| SR | Status Register | 状态寄存器 |
| SR1 | Status Register 1 | 状态寄存器1 |
| SR2 | Status Register 2 | 状态寄存器2 |
| SR3 | Status Register 3 | 状态寄存器3 |
| WEL | Write Enable Latch | 写使能锁存 |
| BUSY | Busy Flag | 忙碌标志 |
| QE | Quad Enable | 四线使能 |

### 硬件规格

| 缩写 | 全称 | 中文 |
|------|------|------|
| VCC | Voltage Collector | 电源电压 |
| GND | Ground | 地 |
| WP# | Write Protect | 写保护 |
| HOLD# | Hold | 暂停 |
| CS# | Chip Select | 片选 |
| CLK | Clock | 时钟 |
| D0-D7 | Data 0-7 | 数据0-7 |

### 性能参数

| 缩写 | 全称 | 中文 |
|------|------|------|
| MHz | Megahertz | 兆赫兹 |
| MB/s | Megabytes per Second | 兆字节每秒 |
| ns | Nanosecond | 纳秒 |
| us | Microsecond | 微秒 |
| ms | Millisecond | 毫秒 |
| s | Second | 秒 |
| tACC | Access Time | 访问时间 |
| tRC | Read Cycle Time | 读周期时间 |

### 可靠性

| 缩写 | 全称 | 中文 |
|------|------|------|
| ECC | Error Correction Code | 错误校正码 |
| P/E | Program/Erase Cycle | 编程/擦除周期 |
| P/E Cycles | Program/Erase Cycles | 编程/擦除次数 |
| TID | Total Ionizing Dose | 总电离剂量 |
| EB | Endurance Budget | 耐力预算 |

### 标准与组织

| 缩写 | 全称 | 中文 |
|------|------|------|
| JEDEC | Joint Electron Device Engineering Council | 电子器件工程联合委员会 |
| ONFI | Open Nand Flash Interface | 开放NAND闪存接口 |
| AEC | Automotive Electronics Council | 汽车电子委员会 |
| AEC-Q100 | Automotive Electronics Council-Q100 | 汽车电子委员会Q100标准 |

### 软件与驱动

| 缩写 | 全称 | 中文 |
|------|------|------|
| HAL | Hardware Abstraction Layer | 硬件抽象层 |
| API | Application Programming Interface | 应用程序接口 |
| DMA | Direct Memory Access | 直接存储器访问 |
| IRQ | Interrupt Request | 中断请求 |
| ROM | Read Only Memory | 只读存储器 |
| RAM | Random Access Memory | 随机存取存储器 |
| MTD | Memory Technology Device | 内存技术设备 |

## 3. 计量单位换算

### 容量单位

| 单位 | 字节数 | 位数 |
|------|--------|------|
| 1 Bit (b) | 1/8 Byte | 1 |
| 1 Byte (B) | 1 | 8 |
| 1 KB | 1,024 | 8,192 |
| 1 MB | 1,048,576 | 8,388,608 |
| 1 GB | 1,073,741,824 | 8,589,934,592 |

### 常见容量标识

| 标识 | 实际容量 | 用途 |
|------|----------|------|
| 8Mbit | 1MB | 小型固件 |
| 16Mbit | 2MB | 标准固件 |
| 32Mbit | 4MB | 中型应用 |
| 64Mbit | 8MB | 大型应用 |
| 128Mbit | 16MB | XIP+数据存储 |
| 256Mbit | 32MB | 大型系统 |
| 512Mbit | 64MB | 高端应用 |

### 时间单位换算

| 单位 | 毫秒 | 微秒 | 纳秒 |
|------|------|------|------|
| 1 ms | 1 | 1,000 | 1,000,000 |
| 1 us | 0.001 | 1 | 1,000 |
| 1 ns | 0.000001 | 0.001 | 1 |

---

**相关章节**
- [Nor Flash简介与分类](../01-hardware-basics/01-nor-flash-intro.md)
- [SPI接口详解](../01-hardware-basics/03-spi-interface.md)
- [SFDP标准详解](../01-hardware-basics/04-sfdp-standard.md)

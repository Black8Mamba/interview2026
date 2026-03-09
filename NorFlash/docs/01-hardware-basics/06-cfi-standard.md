# CFI并行接口标准详解

CFI（Common Flash Interface，通用Flash接口）是并行Nor Flash领域最重要的标准化接口规范之一。它为Flash芯片与主控系统之间提供了统一的参数查询机制，使得软件驱动能够自动识别和适配不同厂商、不同型号的Flash芯片，极大地简化了嵌入式系统的开发工作。本章将全面介绍CFI标准的技术细节、数据结构、参数解析方法以及实际应用案例，帮助开发者深入理解这一关键标准并在项目中有效运用。

---

## CFI标准概述

CFI标准是由JEDEC（固态技术协会）制定的通用Flash接口规范，旨在解决不同厂商Flash芯片之间的兼容性问题。在CFI标准出现之前，软件驱动通常需要针对每一种具体的Flash芯片型号进行硬编码，这导致了驱动维护困难、系统升级受限等问题。CFI的引入使得Flash驱动可以从芯片内部读取详细的参数信息，实现了真正的即插即用支持。

### CFI的定义与背景

CFI最初于1998年由Intel公司提出，随后被JEDEC接纳为标准规范（JESD68）。该标准定义了一套标准的查询命令和数据结构，使得主控系统能够获取Flash芯片的核心参数，包括容量、扇区结构、时序要求、电压范围等关键信息。这些信息被存储在Flash芯片内部的特定区域，可以通过标准的查询命令进行读取。

在嵌入式系统开发中，CFI标准的价值体现在多个方面。首先，它简化了驱动开发工作，开发者无需为每一款芯片单独编写驱动代码。其次，它提高了系统的可移植性，同一套软件代码可以在使用不同Flash芯片的硬件平台上运行。此外，CFI还为系统提供了动态识别和适配能力，使得即插即用和热插拔成为可能。

### JEDEC标准体系

JEDEC是全球领先的半导体行业标准组织之一，负责制定和维护各种存储器相关的技术标准。CFI标准作为JEDEC标准体系的重要组成部分，与其他相关标准共同构成了完整的Flash存储器技术规范。

在JEDEC标准体系中，与Flash存储器相关的主要标准包括：

JESD68系列标准定义了CFI的基本规范，其中JESD68.01是CFI的核心标准文档。该标准详细规定了查询命令、数据结构、参数格式等内容。此外，JESD71定义了统一的Flash存储器术语和命名规范，JED21系列标准则涵盖了不同类型Flash的具体技术规范。这些标准相互配合，共同为Flash存储器的设计和应用提供了完整的技术参考。

值得注意的是，CFI标准主要针对并行接口的NOR型Flash存储器。对于串行接口的Flash存储器，JEDEC制定了SFDP（Serial Flash Discoverable Parameters）标准作为对应的参数查询机制，这两种标准在接口特性和数据结构上有所不同，但核心设计理念是一致的。

### CFI芯片与非CFI芯片的对比

在实际的Flash存储器产品中，并不是所有的芯片都支持CFI标准。支持CFI标准的芯片被称为CFI芯片，而不支持的则被称为非CFI芯片或传统芯片。两者的主要区别体现在以下几个方面。

在参数获取方式上，CFI芯片可以通过标准的查询命令（0x98）读取存储在芯片内部的参数信息，包括厂商识别码、设备识别码、容量信息、扇区结构、时序参数等。非CFI芯片则不具备这种能力，驱动软件必须通过其他方式（如预设的芯片ID表）来获取这些信息，或者根本不支持动态识别。

在兼容性方面，CFI芯片具有显著的优势。由于所有的CFI芯片都遵循相同的数据结构标准，软件驱动可以统一处理来自不同厂商的芯片，无需针对特定型号进行定制。非CFI芯片则需要为每种芯片型号维护单独的识别和配置代码，这增加了软件维护的复杂度，也限制了系统的可扩展性。

在系统灵活性方面，使用CFI芯片的系统可以更容易地实现硬件平台的更换。当需要更换Flash芯片供应商或升级到更大容量的芯片时，只要新芯片支持CFI标准，现有的软件驱动通常可以直接正常工作。而使用非CFI芯片的系统则可能需要进行软件修改甚至重新开发驱动。

从成本角度来看，非CFI芯片通常价格更低，因为它们不需要存储CFI数据结构，也不需要实现查询功能。对于成本敏感且不需要动态识别功能的应用，非CFI芯片仍然是可行的选择。然而，随着CFI芯片的普及和成本差距的缩小，越来越多的设计倾向于选择支持CFI标准的产品。

### CFI标准的主要优势

CFI标准为嵌入式系统开发带来了多方面的优势，这些优势使其成为现代Flash存储器应用的首选方案。

**标准化带来的互操作性**是CFI最核心的价值。通过统一的数据结构和查询机制，CFI使得来自不同厂商的Flash芯片可以在同一系统中互换使用。这种标准化不仅简化了硬件设计，也为供应链管理提供了更大的灵活性。系统设计者可以选择多个合格的芯片供应商，降低单一供应商依赖的风险。

**简化软件开发**是CFI另一个重要优势。传统的Flash驱动开发需要对每一款目标芯片进行详细的参数配置，包括容量、扇区大小、时序参数等。使用CFI标准后，驱动软件可以通过查询自动获取这些信息，大大减少了手动配置的工作量。同时，自动化的参数获取也减少了人为错误的可能性，提高了软件的可靠性。

**支持高级功能**是CFI标准的扩展优势。除了基本的识别参数外，CFI还定义了许多高级功能的数据结构，包括擦除块区域描述、时序参数详细信息、电源管理功能等。软件驱动可以利用这些信息实现更精细的芯片控制，如优化的擦除算法、自适应的时序调整等。

**未来可扩展性**也是CFI设计的重要考量。CFI数据结构预留了扩展区域，厂商可以在不破坏标准兼容性的前提下添加自定义的扩展信息。这种设计确保了标准的稳定性，也为未来的功能增强留出了空间。

---

## CFI查询接口

CFI查询接口是访问Flash芯片内部参数信息的唯一标准途径。通过发送特定的查询命令并按照规定的时序读取查询数据，主控系统可以获取芯片的各种配置信息。本节详细介绍CFI查询接口的工作原理和使用方法。

### CFI查询命令（0x98）

CFI查询命令是进入CFI查询模式的唯一标准命令，其操作码为0x98。这个命令在Flash芯片处于读阵列模式时发送，用于切换芯片的工作状态到查询模式。

在发送CFI查询命令之前，Flash芯片必须处于正常的读阵列模式。如果芯片当前正处于擦除或编程操作中，需要先等待这些操作完成，或者通过发送复位命令（0xF0或0x70）将芯片复位到读阵列模式。

查询命令0x98的发送过程与普通读取操作类似。主控系统将地址设置为0x55（对于x8模式）或0xAA（对于x16模式），将数据设置为0x98，然后激活WE#信号完成写入操作。这个命令将Flash芯片从读阵列模式切换到查询模式。

需要特别注意的是，CFI查询命令的地址和数据组合可能有特定的要求。标准的CFI查询使用地址0x55（或0xAA）和数据0x98，但某些芯片可能有不同的实现。在实际开发中，应参考具体芯片的数据手册以确保使用正确的查询序列。

### 查询进入与退出方法

正确地进入和退出CFI查询模式是获取准确参数信息的基础。以下是详细的操作步骤和注意事项。

**进入CFI查询模式的步骤**：

第一步，确保Flash芯片处于就绪状态。检查RY/BY#信号是否为高电平，或者查询状态寄存器的相应位。如果芯片正在执行擦除或编程操作，需要等待操作完成。

第二步，发送CFI查询命令。将地址设置为0x55（x8模式）或0xAA（x16模式），将数据线设置为0x98，然后执行写操作。这个写操作会将芯片切换到查询模式。

第三步，切换到查询地址。查询数据存储在特定的地址空间，通常从地址0x00开始（某些芯片可能从其他地址开始）。查询模式下的读操作将从这些特定地址返回预定义的参数数据。

**退出CFI查询模式的步骤**：

完成参数查询后，需要将Flash芯片恢复到正常的读阵列模式。最简单的方法是发送复位命令，通常是向任意地址写入0xF0或0x70。复位命令会终止查询模式并将芯片返回到读阵列状态。

另一种退出查询模式的方法是执行一次完整的读阵列命令序列，这同样可以将芯片恢复到正常工作状态。无论使用哪种方法，关键是在进行正常的读取、擦除或编程操作之前，确保芯片已经成功退出了查询模式。

**状态转换注意事项**：

在查询模式和正常模式之间切换时，需要注意一些潜在的问题。首先，某些芯片在退出查询模式后可能需要短暂的恢复时间才能响应正常的操作请求。其次，如果在查询过程中发生了电源波动或复位信号，芯片可能仍处于查询模式，这时需要再次发送复位命令以确保芯片处于正确的状态。

### 查询数据读取时序

CFI查询数据的读取时序与普通Flash读取操作类似，但由于查询数据是预先存储在芯片内部的，因此时序要求可能有所不同。以下是查询数据读取的时序要求和操作要点。

查询模式下，读取操作使用标准的Flash读时序。主控系统将目标地址驱动到地址总线上，然后激活CE#和OE#信号。Flash芯片在接收到有效的地址和控制信号后，会从内部存储的查询表中返回相应的数据。

查询数据的读取时序关键参数包括地址建立时间、数据有效延迟和输出保持时间。这些参数的典型值与普通读取操作相似，但具体的时序要求可能因芯片型号而异。在进行查询操作时，应参考芯片数据手册中的详细时序规格。

对于时序配置，建议从相对宽松的参数开始，逐步优化以提高访问速度。大多数CFI查询操作对时序的要求不如高速数据读取严格，因此使用保守的时序参数通常不会影响整体性能。

**查询操作的典型时序配置示例**：

```
/* CFI查询时序配置示例 */
FSMC_NORSRAM_TimingTypeDef CFI_Timing;

/* 使用相对宽松的时序参数确保兼容性 */
CFI_Timing.AddressSetupTime = 2;      /* 地址建立时间 */
CFI_Timing.AddressHoldTime = 1;       /* 地址保持时间 */
CFI_Timing.DataSetupTime = 5;         /* 数据建立时间 */
CFI_Timing.BusTurnAroundDuration = 1; /* 总线转向时间 */
CFI_Timing.AccessMode = FSMC_ACCESS_MODE_A;
```

---

## CFI数据结构

CFI数据结构是CFI标准的核心内容，它定义了Flash芯片内部存储参数信息的数据格式和组织方式。理解这些数据结构对于正确解析CFI参数至关重要。本节详细介绍CFI数据结构的各个组成部分及其含义。

### Query数据格式

CFI查询数据以特定的数据结构存储在Flash芯片的查询区域中。这个数据结构按照预定义的格式组织，包含从基本识别信息到详细技术参数的完整内容。查询数据通常存储在从地址0x00开始的连续地址空间中，每个地址对应一个字节的数据。

查询数据采用小端格式（Little Endian）存储多字节数据，即低位字节存储在低地址，高位字节存储在高地址。在解析多字节参数时需要注意这一格式约定。

查询数据结构主要分为三个部分：基本查询信息（QRY）、主要参数表（Primary Algorithm Information）和扩展参数表（Extended Query Information）。基本查询信息包含识别签名和版本号，主要参数表提供芯片的核心技术参数，扩展参数表则包含厂商特定的扩展信息。

### QRY签名识别

QRY签名是确认Flash芯片支持CFI标准的关键标识。当主控系统向芯片发送CFI查询命令后，如果在地址0x00处读取到字符Q（0x51）、R（0x52）、Y（0x59）的ASCII码组合，则表明该芯片支持CFI标准。

QRY签名的具体位置和格式如下：地址0x00存储字符Q的ASCII码（0x51），地址0x01存储字符R的ASCII码（0x52），地址0x02存储字符Y的ASCII码（0x59）。这三个连续的字节构成了完整的QRY签名。

在进行CFI查询时，首先应该验证QRY签名的存在。如果在预期的位置没有找到正确的QRY签名，有几种可能的原因：芯片可能不支持CFI标准、查询命令可能没有正确执行、或者芯片可能处于错误的状态。建议在这种情况下检查硬件连接和时序配置，并参考芯片数据手册进行故障排除。

QRY签名验证的代码实现示例：

```c
/**
 * 验证Flash芯片的CFI签名
 * @param base_addr Flash芯片基址
 * @return 1表示支持CFI，0表示不支持
 */
int cfi_verify_signature(uint32_t base_addr)
{
    uint8_t q, r, y;

    /* 读取QRY签名 */
    q = read8(base_addr + 0x00);
    r = read8(base_addr + 0x01);
    y = read8(base_addr + 0x02);

    /* 验证签名 */
    return (q == 0x51 && r == 0x52 && y == 0x59);
}
```

### 厂商ID和设备ID

厂商ID和设备ID是识别Flash芯片制造商和具体型号的关键信息。这些ID在CFI查询数据中位于特定的位置，用于软件驱动的芯片识别和适配。

厂商ID存储在查询数据的特定地址中，通常是地址0x00或0x03（取决于芯片的具体实现）。这是一个单字节或双字节的值，用于标识Flash芯片的制造商。JEDEC为每个主要的Flash制造商分配了唯一的厂商识别码，例如：0x00（未识别）、0x01（AMD/Spansion）、0x1C（eMemory）、0x1F（Atmel）、0x20（Micron）、0x37（ Macronix）、0x40（Winbond）、0x68（Boya）等。

设备ID紧随厂商ID之后，用于标识同一厂商的不同产品型号。设备ID通常占用一到两个字（2到4字节），具体的格式和长度因厂商而异。设备ID的编码方式包括简单的序列号、包含容量信息的编码、与特定功能相关的位域等。

通过读取和分析厂商ID和设备ID，软件驱动可以确定具体的芯片型号，然后根据芯片特性进行相应的配置。以下是读取和解析芯片ID的示例代码：

```c
/* CFI芯片ID信息结构 */
typedef struct {
    uint16_t vendor_id;      /* 厂商ID */
    uint32_t device_id;      /* 设备ID */
    char *vendor_name;       /* 厂商名称字符串 */
    char *device_name;       /* 设备名称字符串 */
} cfi_chip_id_t;

/* 已知厂商ID映射表 */
static const char* vendor_names[] = {
    [0x01] = "AMD/Spansion",
    [0x1C] = "eMemory",
    [0x1F] = "Atmel",
    [0x20] = "Micron",
    [0x37] = "Macronix",
    [0x40] = "Winbond",
    [0x68] = "Boya",
    /* 更多厂商可添加 */
};

/**
 * 读取CFI芯片ID信息
 * @param base_addr Flash基址
 * @return 芯片ID结构
 */
cfi_chip_id_t cfi_read_chip_id(uint32_t base_addr)
{
    cfi_chip_id_t id;

    /* 读取厂商ID */
    id.vendor_id = read8(base_addr + 0x00);

    /* 读取设备ID（2字节） */
    id.device_id = read8(base_addr + 0x01);
    id.device_id |= (uint16_t)read8(base_addr + 0x02) << 8;

    /* 获取厂商名称 */
    if (id.vendor_id < sizeof(vendor_names)/sizeof(vendor_names[0])) {
        id.vendor_name = (char*)vendor_names[id.vendor_id];
    } else {
        id.vendor_name = "Unknown";
    }

    return id;
}
```

### 核心参数表结构

CFI核心参数表（Primary Algorithm Information）包含了Flash芯片的主要技术参数，这些参数对于正确配置和使用Flash芯片至关重要。参数表按照JEDEC标准规定的格式组织，每个参数占用特定的地址偏移量。

核心参数表从QRY签名之后开始，通常从地址0x10或0x13开始（取决于芯片实现）。下表详细列出了主要参数的位置和含义：

| 地址偏移 | 参数名称 | 长度 | 描述 |
|---------|---------|------|------|
| 0x00-0x02 | QRY | 3 | QRY签名 |
| 0x03-0x04 | P_ID | 2 | 主设备算法识别码 |
| 0x05-0x06 | P_ID | 2 | 次设备算法识别码 |
| 0x07 | API_Version | 1 | 主算法接口版本号 |
| 0x08 | API_Version | 1 | 次算法接口版本号 |
| 0x09 | Boot_Configuration | 1 | 引导配置标志 |
| 0x0A-0x0B | Nbr_erase_regions | 2 | 擦除块区域数量 |
| 0x0C-0x0D | Blk_erase_typ1 | 2 | 第1类擦除块大小 |
| 0x0E-0x0F | Blk_erase_typ1 | 2 | 第1类擦除块数量 |
| 0x10-0x11 | Blk_erase_typ2 | 2 | 第2类擦除块大小 |
| 0x12-0x13 | Blk_erase_typ2 | 2 | 第2类擦除块数量 |
| ... | ... | ... | ... |
| 0x1B-0x1C | Flash_Interface | 2 | Flash接口类型 |
| 0x1D-0x1E | Max_buf_write_size | 2 | 最大缓冲写入大小 |
| 0x1F-0x20 | Erase_suspend | 2 | 擦除挂起能力 |
| 0x21-0x22 | Blk_protect_schemes | 2 | 块保护方案 |
| 0x23 | Temp_Circuit | 1 | 温度电路标志 |
| 0x24-0x25 | Vol_Range | 2 | 电压范围 |
| 0x26-0x27 | Vcc_Wr_Min | 2 | 最小Vcc编程电压 |
| 0x28-0x29 | Vcc_Wr_Max | 2 | 最大Vcc编程电压 |
| 0x2A-0x2B | Vpp_Wr_Min | 2 | 最小Vpp编程电压 |
| 0x2C-0x2D | Vpp_Wr_Max | 2 | 最大Vpp编程电压 |
| 0x2E-0x2F | Typ_page_or_buf_wr_t | 2 | 典型页/缓冲写入时间 |
| 0x30-0x31 | Typ_buf_wr_t | 2 | 典型缓冲写入时间 |
| 0x32-0x33 | Typ_block_erase_t | 2 | 典型块擦除时间 |
| 0x34-0x35 | Typ_chip_erase_t | 2 | 典型整片擦除时间 |
| 0x36 | Max_word_wr_t | 1 | 最大字编程时间 |
| 0x37-0x38 | Typ_word_wr_t | 2 | 典型字编程时间 |
| 0x39-0x3A | Size | 2 | 芯片容量编码 |

上表中的地址偏移量是相对于查询数据起始地址的，实际计算时需要加上查询区域的基地址。此外，某些参数的具体含义和格式可能因芯片型号而异，详细的参数说明应参考具体的芯片数据手册。

---

## 核心参数详解

CFI核心参数表包含了Flash芯片的各项关键技术参数，这些参数的正确理解和应用对于实现可靠的Flash操作至关重要。本节详细解释各个核心参数的含义、编码方式和应用方法。

### 芯片容量编码

芯片容量是Flash最基本的参数之一，在CFI中以特定的编码方式存储。容量信息位于查询数据结构的后部，通常在地址0x27或0x29处（相对于查询基址）。

CFI中的容量编码采用2的幂次方编码方式，具体的计算公式为：实际容量 = 2^N字节，其中N是存储在查询数据中的编码值。例如，如果编码值为26，则芯片容量为2^26 = 64MB。这种编码方式可以用较小的存储空间表示大范围的容量值。

以下是从CFI读取并计算芯片容量的示例代码：

```c
/**
 * 从CFI数据计算Flash芯片容量
 * @param cfi_data CFI查询数据基址
 * @return 芯片容量（字节数）
 */
uint32_t cfi_get_capacity(uint32_t cfi_data)
{
    uint16_t size_encoding;
    uint32_t capacity;

    /* 读取容量编码 */
    size_encoding = read16(cfi_data + 0x27);

    /* 计算实际容量: 2^N 字节 */
    capacity = 1UL << size_encoding;

    return capacity;
}

/**
 * 将容量转换为可读格式
 * @param capacity 容量（字节）
 * @param buffer 输出字符串缓冲区
 */
void cfi_format_capacity(uint32_t capacity, char *buffer)
{
    if (capacity >= (1UL << 30)) {
        sprintf(buffer, "%u GB", capacity >> 30);
    } else if (capacity >= (1UL << 20)) {
        sprintf(buffer, "%u MB", capacity >> 20);
    } else if (capacity >= (1UL << 10)) {
        sprintf(buffer, "%u KB", capacity >> 10);
    } else {
        sprintf(buffer, "%u B", capacity);
    }
}
```

**常用芯片容量编码参考**：

| 编码值 | 容量 | 说明 |
|--------|------|------|
| 20 | 1 MB | 1Mbit |
| 21 | 2 MB | 2Mbit |
| 22 | 4 MB | 4Mbit |
| 23 | 8 MB | 8Mbit |
| 24 | 16 MB | 16Mbit |
| 25 | 32 MB | 32Mbit |
| 26 | 64 MB | 64Mbit |
| 27 | 128 MB | 128Mbit |
| 28 | 256 MB | 256Mbit |
| 29 | 512 MB | 512Mbit |
| 30 | 1 GB | 1Gbit |

### 扇区与块结构布局

Flash芯片的存储空间被划分为多个擦除单元，不同类型的芯片可能采用不同的划分方式。CFI标准定义了擦除块区域（Erase Block Region）结构来描述这种划分。

**擦除块区域描述**：

擦除块区域描述信息包括每个区域中擦除块的数量和大小。不同容量的Flash芯片可能有一个或多个擦除块区域，每个区域内的所有块具有相同的大小。芯片的总容量等于各个区域容量之和。

擦除块区域的数量存储在查询数据的特定位置（地址偏移0x0A-0x0B），这是一个16位的值。对于大多数芯片，该值为1到4之间的整数。区域信息从查询数据的固定位置开始，每个区域占用4个字节（2字节块大小 + 2字节块数量）。

擦除块大小采用与容量类似的编码方式，即用幂次方编码。实际的块大小 = 2^N字节。块数量则使用实际的数值编码。

以下是从CFI读取和分析擦除块结构的示例：

```c
/* 擦除块区域结构 */
typedef struct {
    uint32_t block_size;     /* 块大小（字节） */
    uint32_t block_count;    /* 块数量 */
    uint32_t region_size;    /* 区域总大小（字节） */
} erase_region_t;

/* CFI参数结构 */
typedef struct {
    uint32_t total_capacity; /* 总容量（字节） */
    uint8_t num_regions;     /* 擦除块区域数量 */
    erase_region_t regions[4]; /* 最多4个区域 */
} cfi_geometry_t;

/**
 * 解析CFI擦除块区域信息
 * @param cfi_data CFI查询数据基址
 * @return 几何信息结构
 */
cfi_geometry_t cfi_parse_geometry(uint32_t cfi_data)
{
    cfi_geometry_t geo;
    uint16_t num_regions;
    uint32_t offset;
    uint16_t block_size_code;
    uint16_t block_count;

    /* 读取擦除块区域数量 */
    num_regions = read16(cfi_data + 0x0A);
    geo.num_regions = num_regions;

    /* 读取各区域信息 */
    offset = 0x0C;  /* 第一个区域信息起始地址 */
    geo.total_capacity = 0;

    for (int i = 0; i < num_regions && i < 4; i++) {
        /* 读取块大小编码 */
        block_size_code = read16(cfi_data + offset);
        geo.regions[i].block_size = 1UL << block_size_code;

        /* 读取块数量 */
        block_count = read16(cfi_data + offset + 2);
        geo.regions[i].block_count = block_count;

        /* 计算区域大小 */
        geo.regions[i].region_size = (uint32_t)block_count * geo.regions[i].block_size;

        /* 累加到总容量 */
        geo.total_capacity += geo.regions[i].region_size;

        /* 移动到下一个区域 */
        offset += 4;
    }

    return geo;
}

/**
 * 打印Flash几何信息
 * @param geo 几何信息结构
 */
void cfi_dump_geometry(const cfi_geometry_t *geo)
{
    printf("Flash Geometry:\n");
    printf("  Total Capacity: %u bytes\n", geo->total_capacity);
    printf("  Number of Regions: %u\n", geo->num_regions);

    for (int i = 0; i < geo->num_regions; i++) {
        printf("  Region %d:\n", i + 1);
        printf("    Block Size: %u bytes (0x%X)\n",
               geo->regions[i].block_size,
               geo->regions[i].block_size);
        printf("    Block Count: %u\n", geo->regions[i].block_count);
        printf("    Region Size: %u bytes\n", geo->regions[i].region_size);
    }
}
```

### 接口类型识别

Flash芯片的接口类型通过特定的CFI参数进行标识，这个参数决定了芯片与主控系统之间的通信方式。接口类型信息存储在查询数据的特定位置。

CFI定义的接口类型编码如下表所示：

| 编码值 | 接口类型 | 说明 |
|--------|----------|------|
| 0x0000 | x8 | 仅8位数据接口 |
| 0x0001 | x16 | 仅16位数据接口 |
| 0x0002 | x8/x16 | 8位/16位可选接口 |
| 0x0003 | x32 | 32位数据接口 |
| 0x0004 | x16 (复用) | 16位复用接口 |
| 0x0013 | x32 (复用) | 32位复用接口 |

接口类型的识别对于正确配置硬件接口和软件驱动至关重要。不同的接口类型需要使用不同的数据总线宽度和地址映射方式。以下是读取接口类型的示例代码：

```c
/* 接口类型名称映射表 */
static const char* interface_type_names[] = {
    "x8", "x16", "x8/x16", "x32",
    "x16 Multiplexed", "Unknown", "Unknown", "Unknown",
    "Unknown", "Unknown", "Unknown", "Unknown",
    "Unknown", "x32 Multiplexed"
};

/**
 * 读取Flash接口类型
 * @param cfi_data CFI查询数据基址
 * @return 接口类型编码
 */
uint16_t cfi_get_interface_type(uint32_t cfi_data)
{
    return read16(cfi_data + 0x13);
}

/**
 * 获取接口类型名称
 * @param type 接口类型编码
 * @return 接口类型名称字符串
 */
const char* cfi_get_interface_name(uint16_t type)
{
    if (type < sizeof(interface_type_names)/sizeof(interface_type_names[0])) {
        return interface_type_names[type];
    }
    return "Unknown";
}
```

### 时序参数

时序参数定义了Flash芯片对访问时间的具体要求，包括读取、编程和擦除等操作的时间特性。这些参数对于配置主控的时序控制单元至关重要。

**主要时序参数**：

读取操作时序参数相对简单，主要涉及从地址有效到数据有效的时间（访问时间）。这个参数直接影响Flash读取操作的最高频率。

编程操作时序参数包括典型编程时间和最大编程时间。典型编程时间用于正常的操作估计，最大编程时间用于超时检测和错误处理。

擦除操作时序参数同样包括典型擦除时间和最大擦除时间。整片擦除时间通常远大于块擦除时间，因为需要遍历所有块进行擦除。

时序参数的存储采用特定的编码方式，某些参数可能以毫秒或微秒为单位直接存储，也可能采用幂次方编码。以下是解析和应用时序参数的示例：

```c
/* CFI时序参数结构 */
typedef struct {
    uint16_t typ_word_prog_time;     /* 典型字编程时间 */
    uint16_t max_word_prog_time;     /* 最大字编程时间 */
    uint16_t typ_buf_prog_time;      /* 典型缓冲编程时间 */
    uint16_t typ_block_erase_time;   /* 典型块擦除时间 */
    uint16_t max_block_erase_time;   /* 最大块擦除时间 */
    uint16_t typ_chip_erase_time;    /* 典型整片擦除时间 */
    uint16_t max_chip_erase_time;    /* 最大整片擦除时间 */
} cfi_timing_t;

/**
 * 解析CFI时序参数
 * @param cfi_data CFI查询数据基址
 * @return 时序参数结构
 */
cfi_timing_t cfi_parse_timing(uint32_t cfi_data)
{
    cfi_timing_t timing;

    /* 读取典型编程时间（编码方式：2^N微秒） */
    timing.typ_word_prog_time = read16(cfi_data + 0x31);

    /* 读取最大编程时间 */
    timing.max_word_prog_time = read8(cfi_data + 0x30);

    /* 读取典型缓冲编程时间 */
    timing.typ_buf_prog_time = read16(cfi_data + 0x2F);

    /* 读取典型块擦除时间（编码方式：2^N毫秒） */
    timing.typ_block_erase_time = read16(cfi_data + 0x2D);

    /* 读取最大块擦除时间 */
    timing.max_block_erase_time = read16(cfi_data + 0x2C);

    /* 读取典型整片擦除时间 */
    timing.typ_chip_erase_time = read16(cfi_data + 0x2B);

    /* 读取最大整片擦除时间 */
    timing.max_chip_erase_time = read16(cfi_data + 0x2A);

    return timing;
}

/**
 * 将编码的时序值转换为微秒
 * @param encoded 编码值
 * @return 时间值（微秒）
 */
uint32_t cfi_decode_time_us(uint16_t encoded)
{
    /* 时序编码公式: Time = 2^N 微秒/毫秒 */
    return 1UL << encoded;
}

/**
 * 将编码的时序值转换为毫秒
 * @param encoded 编码值
 * @return 时间值（毫秒）
 */
uint32_t cfi_decode_time_ms(uint16_t encoded)
{
    return 1UL << encoded;
}
```

### 供电电压范围

供电电压范围参数定义了Flash芯片正常工作所需的电压条件。这些参数对于确保芯片可靠运行和选择合适的电源设计非常重要。

CFI中的电压参数包括：

**工作电压范围**（Vol_Range）：定义了芯片正常工作时的电压范围。通常用最小值和最大值表示。

**编程电压**（Vcc_Wr）：定义了进行编程操作时需要的电压范围。这个电压通常高于读取时的电压，以保证编程操作的可靠性。

**Vpp编程电压**（Vpp_Wr）：某些Flash芯片支持使用额外的Vpp引脚进行编程，Vpp参数定义了Vpp电压的工作范围。对于只使用Vcc进行编程的芯片，这些参数可能不适用。

电压参数的存储格式可能因芯片而异，有些使用直接电压值编码，有些使用与参考电压的比例编码。以下是电压参数的解析方法：

```c
/* 电压参数结构 */
typedef struct {
    uint16_t vcc_wr_min;   /* 最小编程电压（Vcc, 0.1V单位） */
    uint16_t vcc_wr_max;   /* 最大编程电压（Vcc, 0.1V单位） */
    uint16_t vpp_wr_min;   /* 最小Vpp电压（0.1V单位） */
    uint16_t vpp_wr_max;   /* 最大Vpp电压（0.1V单位） */
    uint8_t  vol_range;    /* 电压范围标志 */
} cfi_voltage_t;

/**
 * 解析CFI电压参数
 * @param cfi_data CFI查询数据基址
 * @return 电压参数结构
 */
cfi_voltage_t cfi_parse_voltage(uint32_t cfi_data)
{
    cfi_voltage_t volt;

    /* 读取电压参数（单位：0.1V） */
    volt.vcc_wr_min = read16(cfi_data + 0x1B);
    volt.vcc_wr_max = read16(cfi_data + 0x1D);
    volt.vpp_wr_min = read16(cfi_data + 0x1F);
    volt.vpp_wr_max = read16(cfi_data + 0x21);
    volt.vol_range = read8(cfi_data + 0x18);

    return volt;
}

/**
 * 打印电压参数
 * @param volt 电压参数结构
 */
void cfi_dump_voltage(const cfi_voltage_t *volt)
{
    printf("Voltage Parameters:\n");
    printf("  Vcc Write Range: %u.%uV - %u.%uV\n",
           volt->vcc_wr_min / 10, volt->vcc_wr_min % 10,
           volt->vcc_wr_max / 10, volt->vcc_wr_max % 10);

    if (volt->vpp_wr_min != 0 || volt->vpp_wr_max != 0xFFFF) {
        printf("  Vpp Write Range: %u.%uV - %u.%uV\n",
               volt->vpp_wr_min / 10, volt->vpp_wr_min % 10,
               volt->vpp_wr_max / 10, volt->vpp_wr_max % 10);
    }
}
```

---

## 扩展CFI查询

除了基本的CFI查询数据结构外，许多Flash芯片还支持扩展查询功能，提供更详细的芯片特定信息和高级功能描述。扩展CFI查询为不同厂商提供了添加自定义信息的机制，同时也为未来的标准扩展预留了空间。

### 扩展查询命令

扩展查询使用与基本查询不同的命令序列来进入。通常，扩展查询通过发送特定的扩展查询命令（通常是0x00或0x90）到特定地址来实现。具体的扩展查询进入方法因芯片厂商而异，需要参考具体芯片的数据手册。

扩展查询数据存储在查询区域的另一个地址空间中，通常从高于基本查询数据的地址开始。扩展查询数据同样以QRY签名开头，后面跟着厂商特定的参数结构。

**常见的扩展查询区域**：

1. **系统接口查询**（System Interface Information）：提供与系统接口相关的详细信息，包括最佳读取时序、页面大小等。

2. **设备参数查询**（Device Parameters）：提供更详细的设备特定参数，如特定的编程/擦除算法要求。

3. **供应商特定查询**（Vendor-Specific）：由各厂商自行定义的扩展信息，可能包括特定的优化参数、特殊功能说明等。

### 厂商特定扩展

不同的Flash芯片厂商在扩展CFI区域中提供各自的特定信息。这些扩展信息通常包括：

** Macronix（旺宏电子）**的扩展查询包含特定的功能标识，如安全区域配置、额外保护功能等。

**Micron（镁光科技）**的扩展查询提供详细的速度等级信息、温度范围扩展和特定的操作模式参数。

**Winbond（华邦电子）**的扩展查询包含与其特定功能相关的配置信息。

解析这些厂商特定扩展需要参考各厂商的数据手册。以下是一个通用的扩展查询结构框架：

```c
/* 扩展查询数据结构 */
typedef struct {
    uint8_t qry[3];              /* "QRY"签名 */
    uint16_t version;            /* 扩展查询版本 */
    uint8_t vendor[224];         /* 厂商特定数据区域 */
} extended_query_t;

/**
 * 读取扩展CFI查询数据
 * @param base_addr Flash基址
 * @return 扩展查询数据（需转换为实际结构）
 */
uint32_t cfi_read_extended_query(uint32_t base_addr)
{
    /* 扩展查询区域的起始地址因厂商而异 */
    /* 常见起始地址: 0x100 或 0x80 */
    return base_addr + 0x100;
}

/**
 * 检查扩展查询是否存在
 * @param ext_addr 扩展查询基址
 * @return 1表示存在，0表示不存在
 */
int cfi_extended_query_exists(uint32_t ext_addr)
{
    uint8_t q, r, y;

    q = read8(ext_addr + 0x00);
    r = read8(ext_addr + 0x01);
    y = read8(ext_addr + 0x02);

    return (q == 0x51 && r == 0x52 && y == 0x59);
}
```

### 额外功能标识

扩展CFI查询中的重要内容之一是芯片额外功能标识。这些标识通过位掩码的形式表示芯片支持的各种高级功能。

**常见功能标识**：

**擦除挂起功能**（Erase Suspend）：表示芯片支持在擦除过程中暂停，以便进行读取操作。这对于需要中断擦除进行紧急数据读取的应用非常有用。

**编程挂起功能**（Program Suspend）：表示芯片支持在编程过程中暂停，允许更高优先级的读取操作优先执行。

**块保护功能**（Block Protection）：表示芯片支持对特定存储块进行保护，防止意外修改或擦除。

**安全区域功能**（Security Region）：表示芯片具有额外的安全存储区域，用于存储密钥、序列号等敏感信息。

**同步/异步接口**：标识Flash接口的类型，对于支持同步接口的芯片，这会影响到时序配置方法。

以下是功能标识的解析示例：

```c
/* CFI功能标识结构 */
typedef struct {
    uint8_t erase_suspend;     /* 擦除挂起能力 */
    uint8_t program_suspend;   /* 编程挂起能力 */
    uint8_t block_protect;     /* 块保护方案 */
    uint8_t temp_range;        /* 温度范围 */
    uint8_t protect_scheme;    /* 保护方案类型 */
} cfi_features_t;

/**
 * 解析CFI功能标识
 * @param cfi_data CFI查询数据基址
 * @return 功能标识结构
 */
cfi_features_t cfi_parse_features(uint32_t cfi_data)
{
    cfi_features_t features;

    /* 读取擦除/编程挂起能力 */
    features.erase_suspend = read8(cfi_data + 0x1A);
    features.program_suspend = (read8(cfi_data + 0x1A) >> 1) & 0x01;

    /* 读取块保护方案 */
    features.block_protect = read8(cfi_data + 0x1D);

    /* 读取温度范围 */
    features.temp_range = read8(cfi_data + 0x1E);

    /* 读取保护方案类型 */
    features.protect_scheme = read8(cfi_data + 0x1E);

    return features;
}

/**
 * 检查特定功能是否支持
 * @param features 功能标识结构
 * @param feature 功能名称
 * @return 1表示支持，0表示不支持
 */
int cfi_feature_supported(const cfi_features_t *features, const char *feature)
{
    if (strcmp(feature, "ERASE_SUSPEND") == 0) {
        return features->erase_suspend & 0x01;
    } else if (strcmp(feature, "PROGRAM_SUSPEND") == 0) {
        return features->program_suspend;
    } else if (strcmp(feature, "BLOCK_PROTECT") == 0) {
        return features->block_protect > 0;
    }
    return 0;
}
```

---

## 实际案例分析

本节通过分析实际Flash芯片的CFI数据，演示CFI标准的具体应用。这些案例来自主流Flash芯片厂商的产品，包括镁光（Micron）、ISSI等公司的并行Nor Flash产品。

### 镁光MT28EW系列CFI解析

Micron的MT28EW系列是面向高性能应用的大容量并行Nor Flash产品，支持完整的CFI标准接口。以下是该系列典型芯片的CFI数据解析示例。

**MT28EW128ABA**是一款128Mbit的并行Nor Flash，支持CFI标准接口，x8/x16可选数据宽度。以下是从该芯片读取的CFI数据及其解析结果：

**基本查询数据**（地址0x00起）：

```
Offset  Value   Description
0x00    0x51    'Q'
0x01    0x52    'R'
0x02    0x59    'Y'
0x03    0x00    Primary Algorithm (JEDEC)
0x04    0x00    (continuation)
0x10    0x02    Number of erase regions
```

**几何参数**：

```
0x0C    Block Region 1: Size 64KB (0x10 = 2^16)
0x0D
0x0E    Block Region 1: Count 256 blocks
0x0F
0x10    Block Region 2: Size 32KB (0x0F = 2^15)
0x11
0x12    Block Region 2: Count 0 blocks
0x13
```

**容量与接口参数**：

```
0x27    Size: 27 = 128Mbit (2^27 = 128Mbit)
0x13    Interface: x8/x16 (0x0002)
```

**时序参数**：

```
0x20    Max block erase time code: 14 (16384 ms = 16s)
0x21
0x22    Typ block erase time code: 13 (8192 ms = 8s)
0x23
```

**镁光MT28EW系列CFI解析代码示例**：

```c
/* MT28EW系列CFI参数定义 */
#define MT28EW_VENDOR_ID     0x20    /* Micron vendor ID */
#define MT28EW128_DEVICE_ID  0x227E  /* MT28EW128ABA */

/* 镁光MT28EW CFI解析结构 */
typedef struct {
    uint32_t capacity;         /* 容量（字节） */
    uint8_t  interface_type;   /* 接口类型 */
    uint32_t block_size_main;  /* 主块大小 */
    uint32_t block_count_main; /* 主块数量 */
    uint32_t block_size_param; /* 参数块大小 */
    uint32_t block_count_param;/* 参数块数量 */
    uint16_t max_erase_time;   /* 最大擦除时间(ms) */
    uint16_t typ_erase_time;   /* 典型擦除时间(ms) */
    uint8_t  features;        /* 功能标识 */
} mt28ew_cfi_t;

/**
 * 解析MT28EW系列CFI参数
 * @param cfi_base CFI查询基址
 * @return 解析后的参数结构
 */
mt28ew_cfi_t mt28ew_parse_cfi(uint32_t cfi_base)
{
    mt28ew_cfi_t cfi;
    uint16_t block_size_code;

    /* 解析容量 */
    cfi.capacity = 1UL << read8(cfi_base + 0x27);

    /* 解析接口类型 */
    cfi.interface_type = read16(cfi_base + 0x13);

    /* 解析块结构 - Region 1 */
    block_size_code = read16(cfi_base + 0x0C);
    cfi.block_size_main = 1UL << block_size_code;
    cfi.block_count_main = read16(cfi_base + 0x0E);

    /* 解析块结构 - Region 2 */
    block_size_code = read16(cfi_base + 0x10);
    cfi.block_size_param = 1UL << block_size_code;
    cfi.block_count_param = read16(cfi_base + 0x12);

    /* 解析时序参数 */
    cfi.max_erase_time = 1UL << read16(cfi_base + 0x20);
    cfi.typ_erase_time = 1UL << read16(cfi_base + 0x22);

    /* 解析功能标识 */
    cfi.features = read8(cfi_base + 0x1A);

    return cfi;
}

/**
 * 打印MT28EW CFI参数
 * @param cfi 解析后的参数
 */
void mt28ew_dump_cfi(const mt28ew_cfi_t *cfi)
{
    printf("Micron MT28EW Flash Parameters:\n");
    printf("  Capacity: %u Mbits (%u bytes)\n",
           (cfi->capacity >> 20) * 8, cfi->capacity);
    printf("  Interface: %s\n",
           cfi->interface_type == 0x0002 ? "x8/x16" : "Unknown");
    printf("  Main Block Size: %u KB\n", cfi->block_size_main >> 10);
    printf("  Main Block Count: %u\n", cfi->block_count_main);
    printf("  Max Erase Time: %u ms\n", cfi->max_erase_time);
    printf("  Typ Erase Time: %u ms\n", cfi->typ_erase_time);
    printf("  Erase Suspend: %s\n",
           (cfi->features & 0x01) ? "Supported" : "Not Supported");
}
```

### ISSI IS66WVH系列CFI参数

ISSI（芯成半导体）的IS66WVH系列是高性价比的并行Nor Flash产品，同样支持完整的CFI标准接口。以下是该系列芯片的CFI参数分析。

**IS66WVH128M8**是一款128Mbit的并行Nor Flash，支持CFI接口，具有稳定的供货能力。以下是其CFI参数的关键信息：

**基本识别信息**：

```
Vendor ID: 0x1D (ESMT/Eon/ISSI)
Device ID: 0x7E (for 128Mbit)
QRY Signature: Valid
```

**芯片容量**：

```
Size Code: 0x1B (128Mbit = 2^27 = 128Mbit)
Actual Capacity: 16,777,216 bytes (16 MB)
```

**擦除块结构**：

ISSI的许多并行Nor Flash采用统一的块结构设计，通常只有一个擦除区域，块大小为64KB：

```
Erase Regions: 1
Block Size: 64 KB (0x10000)
Block Count: 256
Total: 256 × 64KB = 16 MB
```

**时序参数**：

IS66WVH系列的典型时序参数如下：

```
Typical Page/Buffer Write Time: 256 us
Typical Block Erase Time: 500 ms
Max Block Erase Time: 2000 ms
Typical Word Program Time: 40 us
Max Word Program Time: 150 us
```

**ISSI系列CFI解析示例**：

```c
/* ISSI IS66WVH系列CFI解析器 */
typedef struct {
    uint32_t capacity;           /* 容量 */
    uint8_t  num_regions;        /* 区域数量 */
    uint32_t erase_block_size;   /* 擦除块大小 */
    uint16_t erase_block_count;  /* 擦除块数量 */
    uint16_t erase_time_typ;     /* 典型擦除时间 */
    uint16_t erase_time_max;     /* 最大擦除时间 */
    uint16_t program_time_typ;   /* 典型编程时间 */
    uint8_t  interface;          /* 接口类型 */
} issi_cfi_t;

/**
 * 解析ISSI系列CFI参数
 * @param cfi_base CFI查询基址
 * @return 解析后的参数结构
 */
issi_cfi_t issi_parse_cfi(uint32_t cfi_base)
{
    issi_cfi_t cfi;
    uint16_t block_size_code;

    /* 容量 */
    cfi.capacity = 1UL << read8(cfi_base + 0x27);

    /* 区域数量 */
    cfi.num_regions = read8(cfi_base + 0x0C);

    /* 擦除块参数 */
    block_size_code = read16(cfi_base + 0x0D);
    cfi.erase_block_size = 1UL << block_size_code;
    cfi.erase_block_count = read16(cfi_base + 0x0F);

    /* 时序参数 */
    cfi.erase_time_typ = read16(cfi_base + 0x2D);
    cfi.erase_time_max = read16(cfi_base + 0x2B);
    cfi.program_time_typ = read16(cfi_base + 0x31);

    /* 接口类型 */
    cfi.interface = read16(cfi_base + 0x13);

    return cfi;
}
```

### CFI解析代码示例

以下是一个完整的CFI解析代码示例，展示了如何系统地读取和解析Flash芯片的CFI参数：

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Flash读取函数原型（需根据平台实现） */
extern uint8_t  read8(uint32_t addr);
extern uint16_t read16(uint32_t addr);

/* CFI查询命令 */
#define CFI_CMD_QUERY        0x98
#define CFI_CMD_RESET        0xF0

/* CFI数据偏移量 */
#define CFI_OFFSET_QRY       0x00
#define CFI_OFFSET_PID       0x03
#define CFI_OFFSET_AID       0x05
#define CFI_OFFSET_VER       0x07
#define CFI_OFFSET_NUM_REG   0x0C
#define CFI_OFFSET_REG1_SIZE 0x0D
#define CFI_OFFSET_REG1_CNT  0x0F
#define CFI_OFFSET_INTERFACE 0x13
#define CFI_OFFSET_SIZE      0x27

/* CFI数据结构 */
typedef struct {
    /* 基本信息 */
    uint8_t  qry[3];             /* QRY签名 */
    uint16_t pri_id;            /* 主算法ID */
    uint16_t alt_id;            /* 备用算法ID */
    uint8_t  ver_minor;         /* 版本号(次) */
    uint8_t  ver_major;         /* 版本号(主) */

    /* 几何信息 */
    uint8_t  num_regions;       /* 区域数量 */
    struct {
        uint32_t size;          /* 块大小(字节) */
        uint32_t count;         /* 块数量 */
    } region[4];

    /* 接口与容量 */
    uint16_t interface;         /* 接口类型 */
    uint32_t capacity;          /* 容量(字节) */

    /* 功能标识 */
    uint8_t  erase_suspend;     /* 擦除挂起 */
    uint8_t  program_suspend;   /* 编程挂起 */
    uint8_t  block_protect;     /* 块保护 */

    /* 时序参数 */
    uint8_t  max_word_prog;     /* 最大字编程时间系数 */
    uint16_t typ_word_prog;     /* 典型字编程时间系数 */
    uint16_t typ_block_erase;   /* 典型块擦除时间系数 */
    uint16_t max_block_erase;   /* 最大块擦除时间系数 */

    /* 电压参数 */
    uint8_t  vcc_min;            /* 最小Vcc(0.1V) */
    uint8_t  vcc_max;            /* 最大Vcc(0.1V) */

    /* 解析状态 */
    int valid;                  /* CFI是否有效 */
} cfi_info_t;

/**
 * 进入CFI查询模式
 * @param flash_base Flash基址
 * @param is_x16 是否为x16模式
 */
void cfi_enter_query_mode(uint32_t flash_base, int is_x16)
{
    uint32_t query_addr;

    /* 计算查询命令地址 */
    query_addr = is_x16 ? flash_base + 0xAA : flash_base + 0x55;

    /* 发送查询命令 */
    write8(query_addr, CFI_CMD_QUERY);
}

/**
 * 退出CFI查询模式
 * @param flash_base Flash基址
 */
void cfi_exit_query_mode(uint32_t flash_base)
{
    write8(flash_base, CFI_CMD_RESET);
}

/**
 * 完整的CFI解析函数
 * @param flash_base Flash基址
 * @param is_x16 是否为x16模式
 * @return 解析后的CFI信息
 */
cfi_info_t cfi_parse(uint32_t flash_base, int is_x16)
{
    cfi_info_t cfi;
    uint32_t cfi_base;
    uint8_t region_count;
    uint32_t offset;
    uint16_t size_code;

    memset(&cfi, 0, sizeof(cfi));

    /* 进入查询模式 */
    cfi_enter_query_mode(flash_base, is_x16);

    /* 等待查询数据就绪 */
    /* CFI查询数据的基址取决于芯片实现 */
    /* 通常在地址0x10开始或者与查询命令地址相关 */
    cfi_base = flash_base + 0x10;  /* 典型偏移 */

    /* 读取QRY签名 */
    cfi.qry[0] = read8(cfi_base + CFI_OFFSET_QRY + 0);
    cfi.qry[1] = read8(cfi_base + CFI_OFFSET_QRY + 1);
    cfi.qry[2] = read8(cfi_base + CFI_OFFSET_QRY + 2);

    /* 验证签名 */
    if (cfi.qry[0] != 'Q' || cfi.qry[1] != 'R' || cfi.qry[2] != 'Y') {
        cfi.valid = 0;
        cfi_exit_query_mode(flash_base);
        return cfi;
    }

    /* 读取基本ID */
    cfi.pri_id = read16(cfi_base + CFI_OFFSET_PID);
    cfi.alt_id = read16(cfi_base + CFI_OFFSET_AID);
    cfi.ver_minor = read8(cfi_base + CFI_OFFSET_VER);
    cfi.ver_major = read8(cfi_base + CFI_OFFSET_VER + 1);

    /* 读取区域数量 */
    region_count = read8(cfi_base + CFI_OFFSET_NUM_REG);
    cfi.num_regions = region_count > 4 ? 4 : region_count;

    /* 读取各区域信息 */
    offset = CFI_OFFSET_NUM_REG + 1;
    for (int i = 0; i < cfi.num_regions; i++) {
        size_code = read8(cfi_base + offset);
        cfi.region[i].size = 1UL << size_code;
        cfi.region[i].count = read16(cfi_base + offset + 1);
        offset += 4;
    }

    /* 读取接口类型 */
    cfi.interface = read16(cfi_base + CFI_OFFSET_INTERFACE);

    /* 读取容量 */
    size_code = read8(cfi_base + CFI_OFFSET_SIZE);
    cfi.capacity = 1UL << size_code;

    /* 读取功能标识 */
    cfi.erase_suspend = read8(cfi_base + 0x1A);
    cfi.program_suspend = read8(cfi_base + 0x1B);
    cfi.block_protect = read8(cfi_base + 0x1D);

    /* 读取时序参数 */
    cfi.max_word_prog = read8(cfi_base + 0x30);
    cfi.typ_word_prog = read16(cfi_base + 0x31);
    cfi.typ_block_erase = read16(cfi_base + 0x2D);
    cfi.max_block_erase = read16(cfi_base + 0x2B);

    /* 读取电压参数 */
    cfi.vcc_min = read8(cfi_base + 0x1B);
    cfi.vcc_max = read8(cfi_base + 0x1D);

    /* 标记为有效 */
    cfi.valid = 1;

    /* 退出查询模式 */
    cfi_exit_query_mode(flash_base);

    return cfi;
}

/**
 * 打印CFI信息
 * @param cfi CFI信息结构
 */
void cfi_dump(const cfi_info_t *cfi)
{
    if (!cfi || !cfi->valid) {
        printf("CFI: Invalid or not present\n");
        return;
    }

    printf("=== CFI Information ===\n");
    printf("QRY Signature: %c%c%c\n",
           cfi->qry[0], cfi->qry[1], cfi->qry[2]);
    printf("Primary ID: 0x%04X\n", cfi->pri_id);
    printf("Alternate ID: 0x%04X\n", cfi->alt_id);
    printf("Version: %d.%d\n", cfi.ver_major, cfi.ver_minor);

    printf("\n=== Geometry ===\n");
    printf("Capacity: %u MB (%u Mbit)\n",
           cfi->capacity >> 20, (cfi->capacity >> 20) * 8);
    printf("Interface: 0x%04X\n", cfi->interface);
    printf("Erase Regions: %u\n", cfi->num_regions);

    for (int i = 0; i < cfi->num_regions; i++) {
        printf("  Region %d: %u KB x %u = %u KB\n",
               i + 1,
               cfi->region[i].size >> 10,
               cfi->region[i].count,
               (cfi->region[i].size * cfi->region[i].count) >> 10);
    }

    printf("\n=== Features ===\n");
    printf("Erase Suspend: %s\n",
           (cfi->erase_suspend & 0x01) ? "Yes" : "No");
    printf("Program Suspend: %s\n",
           (cfi->erase_suspend & 0x02) ? "Yes" : "No");
    printf("Block Protect: 0x%02X\n", cfi->block_protect);

    printf("\n=== Timing (encoded) ===\n");
    printf("Max Word Program: 2^%u us\n", cfi->max_word_prog);
    printf("Typ Word Program: 2^%u us\n", cfi->typ_word_prog);
    printf("Typ Block Erase: 2^%u ms\n", cfi->typ_block_erase);
    printf("Max Block Erase: 2^%u ms\n", cfi->max_block_erase);

    printf("\n=== Voltage ===\n");
    printf("Vcc Range: %u.%uV - %u.%uV\n",
           cfi->vcc_min / 10, cfi->vcc_min % 10,
           cfi->vcc_max / 10, cfi->vcc_max % 10);
}
```

---

## CFI与非CFI芯片兼容处理

在实际的嵌入式系统设计中，可能会遇到同时使用CFI芯片和非CFI芯片的情况，或者需要在已有的系统中替换不同类型的Flash芯片。因此，软件驱动需要具备处理这两种类型芯片的能力。本节介绍CFI与非CFI芯片的兼容处理策略。

### 检测芯片是否支持CFI

检测Flash芯片是否支持CFI标准是兼容处理的第一步。以下是几种常用的检测方法：

**方法一：QRY签名检测**。这是最可靠的CFI检测方法。通过发送CFI查询命令并读取查询数据，验证地址0x00-0x02处是否为字符"QRY"（0x51, 0x52, 0x59）。如果签名正确，则芯片支持CFI。

```c
/**
 * 检测Flash芯片是否支持CFI
 * @param flash_base Flash基址
 * @param is_x16 是否为x16模式
 * @return 1表示支持CFI，0表示不支持
 */
int cfi_detect(uint32_t flash_base, int is_x16)
{
    uint8_t q, r, y;
    uint32_t query_addr;

    /* 发送CFI查询命令 */
    query_addr = is_x16 ? flash_base + 0xAA : flash_base + 0x55;
    write8(query_addr, 0x98);

    /* 读取查询数据起始位置 */
    /* 注意: 实际地址因芯片而异，这里使用典型值 */
    q = read8(flash_base + 0x10);
    r = read8(flash_base + 0x11);
    y = read8(flash_base + 0x12);

    /* 恢复Flash到正常模式 */
    write8(flash_base, 0xF0);

    /* 验证QRY签名 */
    return (q == 'Q' && r == 'R' && y == 'Y');
}
```

**方法二：设备ID回读检测**。某些非CFI芯片在特定模式下会返回设备ID，这可以作为辅助判断依据。但这种方法不够可靠，因为不同芯片的ID回读行为差异很大。

**方法三：尝试读取已知CFI参数位置**。如果CFI查询命令没有产生预期的结果，可以尝试读取一些CFI参数的典型位置（如容量编码、厂商ID等）。如果读回的数据是0xFF或0x00，可能表示芯片不支持CFI。

### 降级策略设计

当系统检测到Flash芯片不支持CFI标准时，需要采用降级策略来处理。以下是几种常用的降级处理方案：

**方案一：使用默认参数表**。对于已知的非CFI芯片型号，可以在驱动中维护一个默认参数表。当无法通过CFI获取参数时，从默认表中查找对应的参数。这种方法适用于系统使用固定型号芯片的场景。

```c
/* 非CFI芯片参数表 */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    char *name;
    uint32_t capacity;
    uint32_t sector_size;
    uint32_t sector_count;
    uint16_t erase_time;
} fallback_chip_params_t;

/* 默认芯片参数表 */
static const fallback_chip_params_t fallback_chips[] = {
    /* 示例条目 - 需根据实际芯片填充 */
    {0x01, 0x227E, "AMD Am29LV128M", 16*1024*1024, 64*1024, 256, 1000},
    {0x40, 0x227E, "Winbond W29GL128C", 16*1024*1024, 64*1024, 256, 1000},
    /* 结束标记 */
    {0xFFFF, 0xFFFF, NULL, 0, 0, 0, 0}
};

/**
 * 从默认表查找芯片参数
 * @param vendor_id 厂商ID
 * @param device_id 设备ID
 * @return 参数指针，未找到返回NULL
 */
const fallback_chip_params_t* cfi_find_fallback(uint16_t vendor_id, uint16_t device_id)
{
    const fallback_chip_params_t *p = fallback_chips;

    while (p->name != NULL) {
        if (p->vendor_id == vendor_id && p->device_id == device_id) {
            return p;
        }
        p++;
    }

    return NULL;
}
```

**方案二：基于硬件配置的参数推断**。如果系统设计允许，可以通过硬件配置信息来推断Flash参数。例如，通过检测地址总线宽度（x8/x16）、片选信号数量等硬件特性来缩小参数范围。

**方案三：提示用户配置**。对于通用性较强的系统，可以提供用户配置接口，允许用户手动指定芯片参数。这种方法增加了系统配置的灵活性，但也增加了用户负担。

### 驱动适配方案

为了实现CFI和非CFI芯片的统一驱动支持，可以采用以下适配方案：

**统一抽象层设计**。定义统一的Flash操作接口抽象层，底层实现根据芯片类型（CFI或非CFI）选择相应的参数获取方式。

```c
/* Flash操作抽象接口 */
typedef struct flash_ops {
    int (*init)(void);
    int (*read)(uint32_t addr, uint8_t *buf, uint32_t len);
    int (*write)(uint32_t addr, const uint8_t *buf, uint32_t len);
    int (*erase)(uint32_t addr, uint32_t len);
    int (*chip_id)(uint16_t *vendor, uint32_t *device);
    uint32_t (*capacity)(void);
    uint32_t (*sector_size)(void);
    uint32_t (*sector_count)(void);
} flash_ops_t;

/* 外部函数声明 */
extern flash_ops_t cfi_flash_ops;
extern flash_ops_t legacy_flash_ops;

/**
 * Flash驱动初始化 - 自动检测芯片类型
 * @return 成功返回操作接口，失败返回NULL
 */
const flash_ops_t* flash_driver_init(void)
{
    /* 尝试CFI检测 */
    if (cfi_detect(FLASH_BASE, 0)) {
        /* CFI芯片，使用CFI驱动 */
        cfi_flash_ops.init();
        return &cfi_flash_ops;
    }

    /* 尝试非CFI芯片识别 */
    uint16_t vendor, device;
    if (legacy_flash_detect(&vendor, &device)) {
        /* 找到匹配的非CFI芯片参数 */
        if (cfi_find_fallback(vendor, device) != NULL) {
            legacy_flash_ops.init();
            return &legacy_flash_ops;
        }
    }

    /* 无法识别，使用保守的默认参数 */
    return NULL;
}
```

**参数缓存机制**。在系统启动时进行一次完整的CFI参数解析，并将解析结果缓存到内存中供后续操作使用。这样可以避免频繁访问查询区域，提高运行效率。

```c
/* CFI参数缓存 */
static cfi_info_t cached_cfi_info;
static int cfi_cache_valid = 0;

/**
 * 获取CFI信息（带缓存）
 * @param force_refresh 强制重新读取
 * @return CFI信息指针
 */
const cfi_info_t* cfi_get_info(int force_refresh)
{
    if (!cfi_cache_valid || force_refresh) {
        cached_cfi_info = cfi_parse(FLASH_BASE, 0);
        cfi_cache_valid = cached_cfi_info.valid;
    }

    return &cached_cfi_info;
}
```

**错误恢复机制**。在参数获取过程中可能出现各种错误，如读取超时、数据校验失败等。健壮的驱动应该具备完善的错误检测和恢复能力。

```c
/**
 * CFI读取错误恢复
 * @param flash_base Flash基址
 * @return 恢复结果
 */
int cfi_recovery(uint32_t flash_base)
{
    int retry;

    for (retry = 0; retry < 3; retry++) {
        /* 发送复位命令 */
        write8(flash_base, 0xF0);

        /* 等待Flash就绪 */
        delay_ms(10);

        /* 重新尝试CFI检测 */
        if (cfi_detect(flash_base, 0)) {
            return 0;  /* 恢复成功 */
        }
    }

    return -1;  /* 恢复失败 */
}
```

---

## 本章小结

本章全面介绍了CFI（Common Flash Interface）并行接口标准的核心技术内容，包括标准概述、查询接口、数据结构、参数详解、扩展查询、实际案例以及兼容处理策略等多个方面。

**CFI标准概述**部分阐明了CFI的定义和背景，以及JEDEC标准体系在其中的作用。CFI作为Flash芯片与主控系统之间的标准化接口，解决了不同厂商芯片之间的兼容性问题，为嵌入式系统开发带来了标准化、简化和可扩展性的优势。

**CFI查询接口**部分详细介绍了CFI查询命令0x98的使用方法，以及查询进入和退出的操作流程。正确掌握这些基础操作是获取芯片参数的前提。

**CFI数据结构**部分深入解析了Query数据格式、QRY签名识别、厂商ID和设备ID以及核心参数表的组织方式。这些数据结构是实现CFI参数自动获取的基础。

**核心参数详解**部分逐一说明了芯片容量编码、扇区与块结构、接口类型、时序参数和供电电压范围等关键技术参数。这些参数的正确理解和应用对于实现可靠的Flash操作至关重要。

**扩展CFI查询**部分介绍了扩展查询命令和厂商特定扩展的内容，帮助开发者获取更详细的芯片信息。

**实际案例分析**部分通过镁光MT28EW系列和ISSI IS66WVH系列的具体CFI解析示例，演示了CFI标准在实际芯片上的应用方法。

**兼容处理策略**部分提供了CFI与非CFI芯片的检测方法和降级策略，确保软件驱动能够在不同类型的Flash芯片上正常工作。

掌握CFI标准对于嵌入式系统开发具有重要意义，它不仅简化了Flash驱动的开发工作，也为系统的灵活性和可维护性提供了有力保障。在实际的Nor Flash开发实践中，建议始终优先选择支持CFI标准的产品，以获得更好的软件兼容性和开发效率。

# NOR Flash 技术文档

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)](./docs/00-table-of-contents.md)

NOR Flash技术文档是一套系统性的学习资源，涵盖NOR Flash硬件基础、驱动开发、高级特性、调试优化及典型应用场景。本文档面向嵌入式开发工程师、固件工程师及硬件工程师，帮助读者从入门到精通全面掌握NOR Flash技术。

---

## 项目简介

NOR Flash是一种非易失性存储技术，具有随机访问能力强、读取速度快、支持XIP(eXecute In Place)等特点，广泛应用于嵌入式系统的启动代码存储、固件升级、数据存储等场景。

本项目提供：

- **完整的知识体系**：从硬件原理到驱动开发，从基础应用到高级优化
- **实用的代码示例**：包含可直接使用的驱动框架和参考实现
- **丰富的工程案例**：Bootloader、文件系统、OTA升级等典型应用
- **详细的调试指南**：常见问题分析、性能调优、测试验证方法

---

## 文档结构说明

```
nor_flash/
├── docs/
│   ├── 00-table-of-contents.md          # 目录索引
│   │
│   ├── 01-hardware-basics/               # 硬件基础篇 (6章)
│   │   ├── 01-nor-flash-intro.md
│   │   ├── 02-memory-structure.md
│   │   ├── 03-spi-interface.md
│   │   ├── 04-sfdp-standard.md
│   │   ├── 05-parallel-interface.md
│   │   └── 06-cfi-standard.md
│   │
│   ├── 02-driver-development/            # 驱动开发篇 (7章)
│   │   ├── 01-driver-framework.md
│   │   ├── 02-spi-driver-impl.md
│   │   ├── 03-sfdp-parser.md
│   │   ├── 04-parallel-driver-impl.md
│   │   ├── 05-cfi-impl.md
│   │   ├── 06-flash-algorithm.md
│   │   └── 07-bootloader-integration.md
│   │
│   ├── 03-advanced-topics/              # 高级话题篇 (5章)
│   │   ├── 01-wear-leveling.md
│   │   ├── 02-ecc-error-correction.md
│   │   ├── 03-bit-operation.md
│   │   ├── 04-power-fail-protection.md
│   │   └── 05-security-features.md
│   │
│   ├── 04-debug-optimization/           # 调试优化篇 (4章)
│   │   ├── 01-common-issues.md
│   │   ├── 02-performance-tuning.md
│   │   ├── 03-testing-verification.md
│   │   └── 04-debug-tools.md
│   │
│   ├── 05-typical-applications/          # 典型应用篇 (4章)
│   │   ├── 01-bootloader-design.md
│   │   ├── 02-filesystem-support.md
│   │   ├── 03-ota-upgrade.md
│   │   └── 04-data-storage.md
│   │
│   └── 06-appendix/                     # 附录篇 (4章)
│       ├── 01-chip-selection-guide.md
│       ├── 02-command-reference.md
│       ├── 03-timming-parameters.md
│       └── 04-glossary.md
│
├── README.md
└── LICENSE
```

### 篇章概览

| 篇目 | 章节数 | 主要内容 |
|:---|:---:|:---|
| 硬件基础篇 | 6章 | NOR Flash原理、存储结构、SPI/并行接口、SFDP/CFI标准 |
| 驱动开发篇 | 7章 | 驱动框架、SPI/并行驱动实现、SFDP解析、CFI、Flash算法、Bootloader集成 |
| 高级话题篇 | 5章 | 磨损均衡、ECC纠错、位操作、掉电保护、安全特性 |
| 调试优化篇 | 4章 | 常见问题、性能调优、测试验证、调试工具 |
| 典型应用篇 | 4章 | Bootloader设计、文件系统、OTA升级、数据存储 |
| 附录篇 | 4章 | 芯片选型、命令参考、时序参数、术语表 |

---

## 快速入门指引

### 初次接触NOR Flash

建议按照文档顺序依次阅读，重点掌握以下内容：

1. **硬件基础篇** - 了解NOR Flash的基本原理和接口类型
2. **驱动开发篇第1章** - 理解驱动框架和抽象层设计

推荐阅读路径：

```
01-nor-flash-intro.md → 02-memory-structure.md → 03-spi-interface.md
    → 01-driver-framework.md → 02-spi-driver-impl.md
```

### 需要开发实际驱动

建议重点阅读以下章节：

1. **SPI接口** 和 **SFDP标准** - 理解SPI Flash的通信协议
2. **驱动开发篇** 全部章节 - 掌握驱动实现要点
3. **Flash算法** - 理解读写擦除的时序要求

推荐阅读路径：

```
03-spi-interface.md → 04-sfdp-standard.md → 01-driver-framework.md
    → 02-spi-driver-impl.md → 03-sfdp-parser.md → 06-flash-algorithm.md
```

### 需要解决特定问题

可直接查阅相关章节：

- 遇到问题 → [调试优化篇](./docs/04-debug-optimization/01-common-issues.md)
- 需要优化性能 → [性能调优](./docs/04-debug-optimization/02-performance-tuning.md)
- 需要实现OTA → [OTA升级](./docs/05-typical-applications/03-ota-upgrade.md)
- 需要实现文件系统 → [文件系统支持](./docs/05-typical-applications/02-filesystem-support.md)

---

## 进阶学习路径

### 路径一：驱动开发专家

目标：深入理解NOR Flash驱动机制，能够开发高质量的驱动程序

```
硬件基础篇(全读)
    ↓
驱动开发篇
    ├─ 01-driver-framework.md (深入理解架构)
    ├─ 02-spi-driver-impl.md (重点掌握)
    ├─ 03-sfdp-parser.md (重点掌握)
    ├─ 05-cfi-impl.md
    └─ 06-flash-algorithm.md (深入理解)
    ↓
高级话题篇
    ├─ 01-wear-leveling.md (重要)
    ├─ 02-ecc-error-correction.md (重要)
    └─ 04-power-fail-protection.md
    ↓
调试优化篇
    ├─ 02-performance-tuning.md (重点掌握)
    └─ 03-testing-verification.md
```

### 路径二：系统应用专家

目标：掌握NOR Flash在实际系统中的应用，包括Bootloader、文件系统等

```
硬件基础篇(选读)
    ├─ 01-nor-flash-intro.md
    ├─ 02-memory-structure.md
    └─ 03-spi-interface.md
    ↓
驱动开发篇(选读)
    ├─ 01-driver-framework.md
    ├─ 02-spi-driver-impl.md
    └─ 07-bootloader-integration.md
    ↓
典型应用篇(全读)
    ├─ 01-bootloader-design.md (重点掌握)
    ├─ 02-filesystem-support.md (重点掌握)
    ├─ 03-ota-upgrade.md (重点掌握)
    └─ 04-data-storage.md
    ↓
高级话题篇
    └─ 04-power-fail-protection.md
```

### 路径三：高级优化专家

目标：深入理解NOR Flash的高级特性，进行性能优化和可靠性设计

```
硬件基础篇(全读)
    ↓
驱动开发篇(全读)
    ↓
高级话题篇(全读 - 重点)
    ├─ 01-wear-leveling.md (深入理解)
    ├─ 02-ecc-error-correction.md (深入理解)
    ├─ 03-bit-operation.md
    ├─ 04-power-fail-protection.md (深入理解)
    └─ 05-security-features.md
    ↓
调试优化篇
    ├─ 02-performance-tuning.md (深入掌握)
    └─ 03-testing-verification.md (深入掌握)
    ↓
附录篇
    └─ 02-command-reference.md
    └─ 03-timming-parameters.md
```

---

## 相关资源

### 芯片厂商文档

- [Micron NOR Flash](https://www.micron.com/products/nor-flash)
- [Winbond NOR Flash](https://www.winbond.com/hq/)
- [Macronix NOR Flash](https://www.macronix.com/)
- [Cypress/Spansion NOR Flash](https://www.cypress.com/)

### 技术标准

- [JEDEC SFDP标准 (JESD216)](https://www.jedec.org/)
- [JEDEC CFI标准 (JESD68.01)](https://www.jedec.org/)

### 开发工具

- [OpenOCD](https://openocd.org/) - 开源调试工具
- [Segger J-Link](https://www.segger.com/) - 商业调试器
- [ST-LINK](https://www.st.com/) - ST调试器

---

## 参与贡献

欢迎提交Issue和Pull Request来完善文档。

- 报告问题：[Issue Tracker](https://github.com/example/nor-flash-docs/issues)
- 贡献代码：提交Pull Request
- 文档纠错：直接提交修改

---

## 许可证

本项目采用 [MIT](./LICENSE) 许可证开源。

---

## 联系方式

- 邮箱：contact@example.com
- 网站：https://example.com/nor-flash-docs

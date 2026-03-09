# NOR Flash 技术文档 - 目录索引

本文档系统性地介绍NOR Flash的硬件基础、驱动开发、高级话题、调试优化及典型应用。

---

## 硬件基础篇

本篇介绍NOR Flash的基本原理和硬件接口，是后续开发的基础。

| 章节 | 标题 | 描述 |
|:---:|:---|:---|
| 01 | [NOR Flash 简介](./01-hardware-basics/01-nor-flash-intro.md) | NOR Flash的基本概念、特点与应用场景 |
| 02 | [存储结构](./01-hardware-basics/02-memory-structure.md) | NOR Flash的存储组织、扇区结构、块划分 |
| 03 | [SPI接口](./01-hardware-basics/03-spi-interface.md) | SPI接口协议、时序要求、通信模式 |
| 04 | [SFDP标准](./01-hardware-basics/04-sfdp-standard.md) | Serial Flash Discoverable Parameter规范 |
| 05 | [并行接口](./01-hardware-basics/05-parallel-interface.md) | 并行接口类型、信号定义、时序要求 |
| 06 | [CFI标准](./01-hardware-basics/06-cfi-standard.md) | Common Flash Interface识别与查询 |

---

## 驱动开发篇

本篇深入讲解NOR Flash驱动程序的实现方法。

| 章节 | 标题 | 描述 |
|:---:|:---|:---|
| 01 | [驱动框架](./02-driver-development/01-driver-framework.md) | 驱动架构设计、抽象层定义、接口规范 |
| 02 | [SPI驱动实现](./02-driver-development/02-spi-driver-impl.md) | SPI Flash驱动开发、命令序列、数据传输 |
| 03 | [SFDP解析器](./02-driver-development/03-sfdp-parser.md) | SFDP参数读取、解析与自动配置 |
| 04 | [并行驱动实现](./02-driver-development/04-parallel-driver-impl.md) | 并行Flash驱动开发、内存映射访问 |
| 05 | [CFI实现](./02-driver-development/05-cfi-impl.md) | CFI查询实现、芯片识别与参数获取 |
| 06 | [Flash算法](./02-driver-development/06-flash-algorithm.md) | 读写算法、擦除时序、状态检查 |
| 07 | [Bootloader集成](./02-driver-development/07-bootloader-integration.md) | 在Bootloader中集成Flash驱动 |

---

## 高级话题篇

本篇涵盖NOR Flash的高级特性与优化技术。

| 章节 | 标题 | 描述 |
|:---:|:---|:---|
| 01 | [Wear Leveling](./03-advanced-topics/01-wear-leveling.md) | 磨损均衡算法、均衡策略、寿命延长 |
| 02 | [ECC纠错](./03-advanced-topics/02-ecc-error-correction.md) | 错误检测与纠正、数据完整性保障 |
| 03 | [位操作](./03-advanced-topics/03-bit-operation.md) | 位操作指令、位翻转、原子写入 |
| 04 | [掉电保护](./03-advanced-topics/04-power-fail-protection.md) | 掉电安全设计、事务机制、数据恢复 |
| 05 | [安全特性](./03-advanced-topics/05-security-features.md) | 写保护、加密、OTP区域、安全存储 |

---

## 调试优化篇

本篇介绍NOR Flash的调试方法与性能优化技巧。

| 章节 | 标题 | 描述 |
|:---:|:---|:---|
| 01 | [常见问题](./04-debug-optimization/01-common-issues.md) | 典型问题分析、故障排查、解决方案 |
| 02 | [性能调优](./04-debug-optimization/02-performance-tuning.md) | 访问速度优化、缓存策略、并发操作 |
| 03 | [测试验证](./04-debug-optimization/03-testing-verification.md) | 测试用例设计、边界条件、可靠性验证 |
| 04 | [调试工具](./04-debug-optimization/04-debug-tools.md) | 调试工具使用、日志分析、协议分析 |

---

## 典型应用篇

本篇展示NOR Flash在实际项目中的应用案例。

| 章节 | 标题 | 描述 |
|:---:|:---|:---|
| 01 | [Bootloader设计](./05-typical-applications/01-bootloader-design.md) | 启动引导程序设计、固件升级机制 |
| 02 | [文件系统支持](./05-typical-applications/02-filesystem-support.md) | Flash文件系统实现、FTL层设计 |
| 03 | [OTA升级](./05-typical-applications/03-ota-upgrade.md) | 空中升级方案、双镜像备份 |
| 04 | [数据存储](./05-typical-applications/04-data-storage.md) | 结构化数据存储、日志系统、配置管理 |

---

## 附录篇

本篇提供参考资料与工具文档。

| 章节 | 标题 | 描述 |
|:---:|:---|:---|
| 01 | [芯片选型指南](./06-appendix/01-chip-selection-guide.md) | 主流芯片对比、选型建议、供应商 |
| 02 | [命令参考](./06-appendix/02-command-reference.md) | Flash命令集、详细指令说明 |
| 03 | [时序参数](./06-appendix/03-timming-parameters.md) | 时序规格、时序图、参数配置 |
| 04 | [术语表](./06-appendix/04-glossary.md) | 专业术语解释、缩写对照 |

---

## 相关资源

- [项目GitHub仓库](https://github.com/example/nor-flash-docs)
- [问题反馈](https://github.com/example/nor-flash-docs/issues)
- [贡献指南](./CONTRIBUTING.md)

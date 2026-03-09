# Nor Flash全面学习文档暨开发指南 - 实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 创建一套完整的Nor Flash开发指南文档（30章节，63-80页），涵盖硬件基础、驱动开发、高级话题、调试优化、典型应用和附录。

**Architecture:** 采用模块化结构，按功能模块组织内容。硬件基础篇奠定理论基础，驱动开发篇提供实战代码，高级话题深入技术细节，调试优化篇解决实际问题，典型应用篇展示典型场景，附录篇提供快速参考。

**Tech Stack:** Markdown文档格式、C语言示例代码

---

## 实施概览

| 模块 | 章节数 | 预估工时 |
|------|-------|---------|
| 硬件基础篇 | 6章 | 8-10小时 |
| 驱动开发篇 | 7章 | 12-15小时 |
| 高级话题篇 | 5章 | 6-8小时 |
| 调试优化篇 | 4章 | 5-6小时 |
| 典型应用篇 | 4章 | 5-6小时 |
| 附录篇 | 4章 | 3-4小时 |
| **总计** | **30章** | **39-49小时** |

---

## Phase 1: 硬件基础篇

### Task 1: Nor Flash简介与分类

**Files:**
- Create: `docs/01-hardware-basics/01-nor-flash-intro.md`

**Step 1: 研究与资料收集**

- 收集Nor Flash vs Nand Flash对比资料
- 整理SPI/Dual/Quad/Octal SPI分类标准
- 调研主流芯片厂商产品线（Winbond、镁光、旺宏、ISSI等）
- 参考JEDEC标准文档

**Step 2: 编写文档结构**

```markdown
# Nor Flash简介与分类

## 1.1 Nor Flash与Nand Flash对比
[对比表格和说明]

## 1.2 SPI Nor Flash分类
- 标准SPI
- Dual SPI
- Quad SPI
- Octal SPI

## 1.3 并行Nor Flash分类
- CFI接口
- FPGA专用接口

## 1.4 主流芯片厂商与产品线
[厂商和产品列表]
```

**Step 3: Review与修改**

- 检查技术准确性
- 补充遗漏的分类标准
- 优化图表和表格

**Step 4: 提交**
```

### Task 2: 存储结构与寻址

**Files:**
- Create: `docs/01-hardware-basics/02-memory-structure.md`

### Task 3: SPI接口详解

**Files:**
- Create: `docs/01-hardware-basics/03-spi-interface.md`

### Task 4: SFDP串行接口标准

**Files:**
- Create: `docs/01-hardware-basics/04-sfdp-standard.md`

**Step 1: 研究SFDP标准**

- 收集JESD216、JESD216A、JESD216B、JESD216D标准文档
- 分析SFDP头部结构（Signature、版本号、参数数量）
- 研究BFPT（B Flash Parameter Table）参数表
- 整理常见厂商SFDP差异

**Step 2: 编写SFDP详解**

```markdown
# SFDP串行接口标准

## 2.1 标准概述
- SFDP (Serial Flash Discoverable Parameters)
- JEDEC标准体系

## 2.2 SFDP数据结构
### 2.2.1 头部格式
| 偏移 | 长度 | 名称 | 描述 |
|------|------|------|------|
| 0x00 | 4    | SFDP Signature | 0x50444653 ("SFDP") |
...

## 2.3 参数表详解
### 2.3.1 JEDEC基础参数表(BFPT)
[参数详细说明]

## 2.4 实际案例分析
### 2.4.1 Winbond W25QxxJV
[SFDP参数解析示例]
```

**Step 3: 补充示例代码**

- 创建SFDP读取时序示例
- 编写BFPT参数解析代码

### Task 5: 并行接口详解

**Files:**
- Create: `docs/01-hardware-basics/05-parallel-interface.md`

### Task 6: CFI并行接口标准

**Files:**
- Create: `docs/01-hardware-basics/06-cfi-standard.md`

**Step 1: 研究CFI标准**

- 收集JESD CFI标准文档
- 分析CFI查询命令序列（0x98）
- 研究核心参数表结构
- 整理CFI查询数据格式

**Step 2: 编写CFI详解**

```markdown
# CFI并行接口标准

## 3.1 标准概述
- CFI (Common Flash Interface)
- JEDEC标准体系

## 3.2 CFI查询接口
### 3.2.1 查询命令序列
[命令时序图]

### 3.2.2 查询数据格式
| 偏移 | 描述 |
|------|------|
| 0x00 | Query Unique ASCII "QRY" |
...

## 3.3 核心参数解析
### 3.3.1 芯片容量编码
### 3.3.2 扇区/块结构
### 3.3.3 时序参数

## 3.4 实际案例
### 3.4.1 镁光MT28EW系列
[完整CFI解析示例]
```

---

## Phase 2: 驱动开发篇

### Task 7: 驱动框架设计

**Files:**
- Create: `docs/02-driver-development/01-driver-framework.md`

### Task 8: SPI驱动实现

**Files:**
- Create: `docs/02-driver-development/02-spi-driver-impl.md`

**Step 1: 研究目标芯片**

- Winbond W25Q128JV数据手册
- 镁光MT25QL128ABA数据手册
- SPI命令序列整理

**Step 2: 编写SPI驱动文档**

```markdown
# SPI驱动实现

## 4.1 芯片选型
- Winbond W25Q128JV
- 镁光MT25QL128ABA

## 4.2 SPI配置
### 4.2.1 SPI模式配置
- Mode 0 (CPOL=0, CPHA=0)
- 时钟频率选择

## 4.3 读操作
### 4.3.1 标准读取 (0x03)
[命令序列和时序图]

### 4.3.2 快速读取 (0x0B)
[命令序列和时序图]

## 4.4 写操作
### 4.4.1 页面编程 (0x02)
[命令序列和时序图]

## 4.5 擦除操作
### 4.5.1 扇区擦除 (0x20)
### 4.5.2 块擦除 (0xD8)
### 4.5.3 整片擦除 (0xC7)
```

**Step 3: 编写示例代码**

- `examples/spi-flash/basic_read_write.c`
- `examples/spi-flash/sector_erase.c`

### Task 9: SFDP参数解析驱动

**Files:**
- Create: `docs/02-driver-development/03-sfdp-parser.md`

**Step 1: 设计SFDP解析器架构**

- SFDP读取状态机
- 参数表解析流程
- 错误处理机制

**Step 2: 编写解析器文档和代码**

```c
// SFDP参数解析示例
typedef struct {
    uint32_t signature;
    uint8_t minor_version;
    uint8_t major_version;
    uint8_t num_param_headers;
} sfdp_header_t;

typedef struct {
    uint8_t param_id_lsb;
    uint8_t param_table_major;
    uint8_t param_table_minor;
    uint32_t param_table_ptr;
    uint8_t param_table_length;
} sfdp_param_header_t;

// SFDP读取函数
int sfdp_read(sfpi_device_t *dev, uint32_t addr, uint8_t *buf, uint32_t len);
int sfdp_parse_header(sfdp_header_t *header, const uint8_t *data);
int sfdp_parse_bfpt(sfdp_bfpt_t *bfpt, const uint8_t *data);
```

**Step 3: 创建示例代码**

- `examples/spi-flash/sfdp_parse.c`

### Task 10: 并行驱动实现

**Files:**
- Create: `docs/02-driver-development/04-parallel-driver-impl.md`

### Task 11: CFI接口驱动实现

**Files:**
- Create: `docs/02-driver-development/05-cfi-impl.md`

**Step 1: 设计CFI驱动架构**

- CFI查询状态机
- 参数解析层
- 设备识别与匹配

**Step 2: 编写CFI驱动文档**

```markdown
# CFI接口驱动实现

## 5.1 CFI查询流程
1. 写入CFI查询命令 (0x98)
2. 读取QRY签名验证
3. 读取核心参数
4. 退出查询模式

## 5.2 参数解析
### 5.2.1 厂商识别
### 5.2.2 容量解析
### 5.2.3 扇区地图构建
```

**Step 3: 编写示例代码**

- `examples/parallel-flash/cfi_parse.c`
- `examples/parallel-flash/cfi_detection.c`

### Task 12: Flash算法详解

**Files:**
- Create: `docs/02-driver-development/06-flash-algorithm.md`

### Task 13: 启动加载器集成

**Files:**
- Create: `docs/02-driver-development/07-bootloader-integration.md`

---

## Phase 3: 高级话题篇

### Task 14: 磨损均衡机制

**Files:**
- Create: `docs/03-advanced-topics/01-wear-leveling.md`

### Task 15: ECC纠错机制

**Files:**
- Create: `docs/03-advanced-topics/02-ecc-error-correction.md`

### Task 16: 位操作与位编程

**Files:**
- Create: `docs/03-advanced-topics/03-bit-operation.md`

### Task 17: 掉电保护策略

**Files:**
- Create: `docs/03-advanced-topics/04-power-fail-protection.md`

### Task 18: 安全特性

**Files:**
- Create: `docs/03-advanced-topics/05-security-features.md`

---

## Phase 4: 调试优化篇

### Task 19: 常见问题与排查

**Files:**
- Create: `docs/04-debug-optimization/01-common-issues.md`

### Task 20: 性能调优

**Files:**
- Create: `docs/04-debug-optimization/02-performance-tuning.md`

### Task 21: 测试与验证

**Files:**
- Create: `docs/04-debug-optimization/03-testing-verification.md`

### Task 22: 调试工具使用

**Files:**
- Create: `docs/04-debug-optimization/04-debug-tools.md`

---

## Phase 5: 典型应用篇

### Task 23: 启动加载器设计

**Files:**
- Create: `docs/05-typical-applications/01-bootloader-design.md`

### Task 24: 文件系统支持

**Files:**
- Create: `docs/05-typical-applications/02-filesystem-support.md`

### Task 25: OTA升级实现

**Files:**
- Create: `docs/05-typical-applications/03-ota-upgrade.md`

### Task 26: 数据存储方案

**Files:**
- Create: `docs/05-typical-applications/04-data-storage.md`

---

## Phase 6: 附录篇

### Task 27: 芯片选型指南

**Files:**
- Create: `docs/06-appendix/01-chip-selection-guide.md`

### Task 28: 命令速查表

**Files:**
- Create: `docs/06-appendix/02-command-reference.md`

### Task 29: 时序参数汇总

**Files:**
- Create: `docs/06-appendix/03-timming-parameters.md`

### Task 30: 术语表

**Files:**
- Create: `docs/06-appendix/04-glossary.md`

---

## 收尾工作

### Task 31: 创建目录和索引

**Files:**
- Create: `docs/00-table-of-contents.md`
- Create: `README.md`

### Task 32: 示例代码整理

**Files:**
- Create: `examples/README.md`
- Verify: 所有示例代码完整性

---

## 里程碑

| 里程碑 | 内容 | 预期完成 |
|--------|------|---------|
| M1 | 硬件基础篇（6章） | 25% |
| M2 | 驱动开发篇（7章） | 50% |
| M3 | 高级话题篇（5章） | 65% |
| M4 | 调试优化篇（4章） | 75% |
| M5 | 典型应用篇（4章） | 85% |
| M6 | 附录篇（4章）+收尾 | 100% |

---

*计划版本: v1.0*
*创建日期: 2026-03-09*

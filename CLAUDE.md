# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a **documentation repository** containing study materials for embedded/MCU interview preparation. The content focuses on ARM Cortex processors, specifically covering:

- **Cache architecture** - MESI protocol, cache coherency
- **TLB/MMU** - Memory management, virtual memory
- **Pipeline** - ARM processor pipeline technology
- **Memory model** - Acquire/release semantics, LDREX/STREX
- **Memory barriers** - DMB/DSB/ISB operations
- **GIC** - Generic Interrupt Controller (GICv3/v4) for Cortex-R52
- **Hypervisor** - Cortex-R52 virtualization (EL2, HVC, VM management)

## Directory Structure

```
interview2026/
├── cache/docs/arm-cortex-learning/   # ARM Cortex processor fundamentals
│   ├── chapter1-overview.md
│   ├── chapter2-cache.md
│   ├── chapter3-tlb-mmu.md
│   ├── chapter4-pipeline.md
│   ├── chapter5-memory-model.md
│   ├── chapter6-memory-barriers.md
│   └── chapter7-practice.md
├── gic/                              # GIC interrupt controller docs
│   ├── chapter1-gic-overview.md
│   ├── chapter2-gic-concepts-*.md   # GICv3/v4 concepts
│   ├── chapter3-gic-hardware.md      # Hardware registers
│   ├── chapter4-sdk-source-*.md      # SDK source code analysis
│   ├── chapter5-interrupt-config.md  # Interrupt configuration
│   ├── chapter6-isr-writing.md       # ISR implementation
│   ├── chapter7-multicore-routing.md # Multi-core routing
│   └── chapter8-advanced-features.md
├── hypervisor/                       # Hypervisor documentation
│   ├── part1/                        # Basic concepts
│   ├── part2/                        # Hypervisor framework
│   ├── part3/                        # Core module implementation
│   └── part4/                        # Practice and debugging
├── interview_mcu.md                  # MCU interview notes
├── interview_mcu_qa.md               # MCU Q&A
├── interview_rtos.md                 # RTOS notes
├── interview_rtos_qa.md              # RTOS Q&A
├── interview_linux.md                # Linux notes
└── interview_linux_qa.md             # Linux Q&A
```

## Key Topics

### GIC (Generic Interrupt Controller)
- **Target chip**: Stellar SR6P3C4 (Cortex-R52)
- **SDK reference**: StellarSDK 5.0.0
- **Core concepts**: SGI/PPI/SPI, Priority, Group0/1, Affinity Routing
- **Key APIs**: `irq_init()`, `irq_config()`, `irq_enable()`, `irq_disable()`, `irq_notify()`

### Hypervisor
- **Architecture**: ARMv8-R (EL2)
- **Reference SDK**: Stellar SDK 5.0.0
- **Key components**: Scheduler, VM management, MPU configuration, peripheral virtualization
- **Core files**: `parts/virt/st_hypervisor/`

### ARM Cortex Learning
- Three-layer structure: 基础层 (Basic) → 原理层 (Principle) → 实战层 (Practice)
- Covers: Cache, Pipeline, Memory Model, Memory Barriers

## Working with This Repository

Since this is a documentation repository (markdown files only), there are no build or test commands. The main tasks involve:

1. **Reading/analyzing** the documentation
2. **Adding new chapters** following existing patterns
3. **Cross-referencing** between documents for consistency
4. **Updating Q&A files** with new questions and answers

When adding content:
- Follow the existing chapter naming convention (e.g., `chapterN-topic.md`)
- Include code examples with file path references (format: `source_path:line_number`)
- Maintain consistent terminology (Chinese with English terms where appropriate)

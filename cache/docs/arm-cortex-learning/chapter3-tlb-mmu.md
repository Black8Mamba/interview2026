# 第3章 TLB与内存管理

> **📖 基础层** | **⚙️ 原理层** | **🔧 实战层**

---

## 📖 基础层：虚拟内存概念

### 什么是虚拟内存

虚拟内存是操作系统提供的一种内存管理机制，让应用程序认为自己拥有连续的地址空间，而实际物理内存可能是不连续的。

**核心优势：**
- 内存隔离：每个进程有独立的虚拟地址空间
- 内存扩展：提供比物理内存更大的地址空间
- 简化编程：程序员无需关心物理内存分配

### 分页机制

虚拟内存和物理内存被划分为固定大小的块：
- **页（Page）**：虚拟内存的基本单位（通常4KB）
- **页框（Page Frame）**：物理内存的基本单位

地址转换过程：
```
虚拟地址 (VA)
    │
    ▼
┌─────────────────────────────────┐
│         MMU (Memory            │
│      Management Unit)           │
└─────────────────────────────────┘
    │
    ▼
物理地址 (PA)
```

> **术语对照**
> - VA: Virtual Address，虚拟地址
> - PA: Physical Address，物理地址
> - MMU: Memory Management Unit，内存管理单元
> - Page: 页，虚拟内存单位
> - Page Frame: 页框，物理内存单位

---

## ⚙️ 原理层：TLB与页表机制

### TLB结构与工作原理

TLB（Translation Lookaside Buffer）是MMU中的地址转换缓存，存储最近使用的虚拟地址到物理地址的映射。

```
┌─────────────────────────────────────────────────────┐
│                     TLB                              │
│  ┌───────────────────────────────────────────────┐  │
│  │  Virtual Address  →  Physical Address          │  │
│  │  0xFFFF0000       →  0x12345000              │  │
│  │  0xDEAD1000       →  0x56789000              │  │
│  │  ...                                        │  │
│  └───────────────────────────────────────────────┘  │
│                                                      │
│  典型配置：                                          │
│  - L1 TLB: 32-48项 (I-TLB, D-TLB各一组)             │
│  - L2 TLB: 1024项 (统一)                             │
│  - 命中率: 95%+                                     │
└─────────────────────────────────────────────────────┘
```

**地址转换过程：**
```
虚拟地址 (VA)
    │
    ▼
┌─────────────────────────────────────────┐
│  分离 VA 的三个部分：                     │
│  ┌──────────┬──────────┬──────────┐     │
│  │   VPN    │  Offset  │          │     │
│  │ (页号)   │ (页内偏移)│          │     │
│  └──────────┴──────────┘          │     │
│                    │             │     │
│                    ▼             │     │
│         检查TLB中是否有VPN        │     │
│                    │             │     │
│           ┌────────┴────────┐    │     │
│           │   TLB Hit/Miss  │    │     │
│           └────────┬────────┘    │     │
│                    │             │     │
│        ┌───────────┴───────────┐│     │
│        ▼                       ▼│     │
│  TLB Hit                   TLB Miss│
│  ┌─────────┐                查页表  │
│  │ 返回PPN │                 │    │
│  │ + Offset│                 ▼    │
│  │ = PA   │           更新TLB     │
│  └─────────┘                │    │
│                              ▼    │
│                        返回PA     │
└─────────────────────────────────────────┘
```

### 页表格式（Armv8/AArch64）

Arm64使用4级页表结构：

```
┌─────────────────────────────────────────────────────┐
│         48位虚拟地址结构 (4KB页)                     │
│  ┌────────────┬────────────┬─────────┬──────────┐   │
│  │   PGD      │   PUD     │  PMD   │   PTE   │   │
│  │ (一级页表)  │ (二级页表) │(三级页表)│ (四级页表)│   │
│  │   9位      │   9位     │  9位   │   9位   │   │
│  └────────────┴────────────┴─────────┴──────────┘   │
│                      │          │                  │
│                      └──────────┴──────────────────┘
│                                │                    │
│                            Offset                   │
│                             12位                    │
└─────────────────────────────────────────────────────┘
```

**页表描述符格式：**
```
┌────────────────────────────────────────┐
│  Page Table Entry (PTE) - 64位          │
├────────┬────────┬───────┬──────┬───────┤
│  Phys  │  Attr  │  NS   │ Valid│ XN/PXN│
│  Addr  │  (属性) │(非安全)│ (有效)│(执行)  │
│  [39:12]│        │       │      │       │
└────────┴────────┴───────┴──────┴───────┘

属性位说明：
- UXN/User XN: 用户空间执行控制
- PXN/Priv XN: 特权模式执行控制
- Contiguous: 连续位，提示TLB批量缓存
- nG: 非全局位，影响ASID
```

### 多级页表机制

```
虚拟地址访问流程：

VA[47:39] → PGD Index → 页表级别0 (PGD)
                │
                ▼
VA[38:30] → PUD Index → 页表级别1 (PUD)
                │
                ▼
VA[29:21] → PMD Index → 页表级别2 (PMD)
                │
                ▼
VA[20:12] → PTE Index → 页表级别3 (PTE)
                │
                ▼
                    物理页框号 + Offset = PA
```

**多级页表的优势：**
- 按需分配：未分配的虚拟地址不需要页表项
- 节省内存：稀疏地址空间只需少量页表
- 权限控制：每级都可设置访问权限

### MMU配置流程

```c
// Arm64 MMU配置示例
void mmu_init(void) {
    // 1. 创建页表
    create_page_table(ttb0);

    // 2. 配置各属性
    //    - 内存类型: Normal/W Device/Strongly Ordered
    //    - 缓存策略: WT/WB/RA
    //    - 访问权限: PL0/PL1 RW/RO

    // 3. 设置TTBR0_EL1 (Translation Table Base Register)
    write_ttbr0_el1(ttb0);

    // 4. 配置TCR_EL1 (Translation Control Register)
    //    - 页面大小: 4KB/16KB/64KB
    //    - 地址空间大小: 48位
    //    - 缓存属性
    write_tcr_el1(TCR_TG0_4K | TCR_IPS_48BIT | TCR_SH0_INNER);

    // 5. 启用MMU
    enable_mmu();
}
```

> **术语对照**
> - TLB: Translation Lookaside Buffer，地址转换后备缓冲
> - VPN: Virtual Page Number，虚拟页号
> - PPN: Physical Page Number，物理页号
> - ASID: Address Space Identifier，地址空间标识符
> - PGD/PUD/PMD/PTE: 页表各级名称
> - TTBR: Translation Table Base Register，页表基址寄存器

---

## 🔧 实战层：TLB Miss优化与配置

### TLB Miss优化策略

**1. 大页（Huge Page）**
```c
// 使用2MB/1GB大页，减少TLB项使用
// Linux: mmap时使用MAP_HUGETLB
void *large_page = mmap(NULL, size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                        -1, 0);
```

**2. 预取TLB项**
```c
// 显式预取虚拟地址到TLB
void prefetch_tlb(void *ptr) {
    // 读取一个字节触发页表遍历
    volatile char c = *(char *)ptr;
}
```

**3. 数据布局优化**
```c
// 将热点数据集中，减少TLB项切换
struct hot_data {
    int array[256];
    char buffer[256];
};

// 栈变量 vs 静态/堆变量
void process(void) {
    // 好：TLB已预热
    static struct hot_data data;
    process_data(&data);
}
```

### TLB Shootdown处理

当一个核修改页表时，其他核的TLB需要失效：

```c
// Arm64 TLB失效指令
// 失效所有TLB
tlbi vmalle1;          // EL1所有

// 失效指定地址的TLB
tlbi vae1, x0;         // 失效VA到x0的映射

// 失效当前ASID的TLB
tlbi aside1;          // 失效ASID匹配的TLB项

// 多核系统需要核间通信触发shootdown
// Linux: flush_tlb_all() → IPI到所有CPU
```

### 实际配置案例

**Linux页表配置：**
```bash
# 查看当前页表配置
cat /proc/cpuinfo | grep -i "mmu"
cat /proc/meminfo | grep -i "huge"

# 大页配置
echo 1024 > /proc/sys/vm/nr_hugepages
```

**内核启动参数：**
```
# 4KB页
default_hugepagesz=4k hugepagesz=4k hugepages=1024

# 1GB大页
default_hugepagesz=1G hugepagesz=1G hugepages=4
```

### 调试工具

```bash
# Linux perf TLB统计
perf stat -e dtlb-load-misses,dtlb-store-misses, \
         itlb-misses,iTLB-load-misses ./program

# 查看TLB大小
# /sys/devices/system/cpu/cpu0/tlb/
cat /sys/devices/system/cpu/cpu0/cache/index0/size

# strace观察页错误
strace -e mmap,brk,mprotect ./program
```

---

## 本章小结

- 虚拟内存通过MMU实现地址转换
- TLB是MMU中的地址映射缓存
- Arm64采用4级页表结构（PGD/PUD/PMD/PTE）
- 优化TLB性能：大页、预取、热点数据集中
- 多核系统中TLB一致性需要特殊处理

---

**下一章**：我们将探索 [流水线技术](./chapter4-pipeline.md)，了解处理器如何高效执行指令。

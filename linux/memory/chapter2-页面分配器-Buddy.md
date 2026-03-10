# 第2章 页面分配器(Buddy)

> 本章介绍Linux内核的页面分配器——Buddy系统，包括算法原理、分配API和高级特性。

## 目录

## 2.1 Buddy算法原理

### 伙伴系统概念

Buddy(伙伴)系统是一种物理内存分配算法，其核心思想是将物理内存划分为大小不同的块，这些块按照2的幂次方大小进行组织。

**Buddy算法的主要特点：**

1. **按2的幂次分配**
   - 最小分配单元为1页(4KB)
   - 最大可分配2^10页(4MB)或更高

2. **伙伴关系**
   - 两个相邻的同等大小的空闲块称为"伙伴"
   - 伙伴可以合并成更大的块

3. **分配流程**
   - 查找满足需求的最小空闲块
   - 如果没有则从更大的块分裂

### 阶(Order)的定义

Linux使用"阶"(order)来表示页面块的大小：

| Order | 页数 | 大小 |
|-------|------|------|
| 0 | 1 | 4KB |
| 1 | 2 | 8KB |
| 2 | 4 | 16KB |
| 3 | 8 | 32KB |
| 4 | 16 | 64KB |
| 5 | 32 | 128KB |
| 6 | 64 | 256KB |
| 7 | 128 | 512KB |
| 8 | 256 | 1MB |
| 9 | 512 | 2MB |
| 10 | 1024 | 4MB |

**内核中的定义：**

```c
// include/linux/gfp.h
#define MAX_ORDER 11

// mm/page_alloc.c
struct free_area {
    struct list_head    free_list[MIGRATE_TYPES];
    unsigned long       nr_free;
};

struct zone {
    // ...
    struct free_area   free_area[MAX_ORDER];
    // ...
};
```

### 分配/释放流程

**分配流程：**

```
1. 接收分配请求(比如需要2页，即order=1)
2. 在order=1的空闲链表中查找
3. 如果找到，直接分配
4. 如果没找到，继续查找order=2
5. 如果找到order=2的块，将其分裂
6. 一半加入order=1链表，另一半分配出去
7. 如果order=2也没有，继续向上查找
```

**释放流程（合并）：**

```
1. 释放order=1的块
2. 检查其伙伴是否也是空闲
3. 如果伙伴空闲，合并成order=2的块
4. 继续检查order=2块的伙伴
5. 重复直到无法合并或达到最大order
```

**Buddy算法示例：**

```
初始状态：有一个order=3(8页)的空闲块

分配2页(order=1):
+---+---+---+---+---+---+---+---+
| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |  (8页)
+---+---+---+---+---+---+---+---+
         ↓ 分裂
+---+---+---+---+---+---+---+---+
| A | A | B | B | B | B | B | B |  (A分配2页，B空闲6页)
+---+---+---+---+---+---+---+---+
         ↓ 再次分裂B
+---+---+---+---+---+---+---+---+
| A | A | C | C | D | D | D | D |  (C分配2页，D空闲4页)
+---+---+---+---+---+---+---+---+

释放A(2页):
+---+---+---+---+---+---+---+---+
| F | F | C | C | D | D | D | D |  (F和C是伙伴，合并)
+---+---+---+---+---+---+---+---+
         ↓ 合并
+---+---+---+---+---+---+---+---+
|    G    | C | C | D | D | D | D |  (G是4页的合并块)
+---+---+---+---+---+---+---+---+
```

---

## 2.2 页面分配API

### 核心分配函数

**alloc_pages - 推荐使用**

```c
// include/linux/gfp.h
static inline struct page *
alloc_pages(gfp_t gfp_mask, unsigned int order)
{
    return alloc_pages_current(gfp_mask, order);
}

// mm/page_alloc.c
struct page *alloc_pages_current(gfp_t gfp, unsigned order)
{
    return alloc_pages_node(numa_node_id(), gfp, order);
}

struct page *alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order)
{
    struct page *page;

    if (nid < 0)
        nid = numa_mem_id();

    page = __alloc_pages_node(nid, gfp_mask, order);
    return page;
}
```

**__get_free_pages**

```c
// mm/page_alloc.c
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
    struct page *page;

    page = alloc_pages(gfp_mask, order);
    if (!page)
        return 0;

    return (unsigned long)page_address(page);
}
```

**get_free_page**

```c
// 分配一页
unsigned long get_free_page(gfp_t gfp_mask)
{
    return __get_free_pages(gfp_mask, 0);
}
```

### 分配标志(GFP Mask)

**分配修饰符：**

| 标志 | 说明 |
|------|------|
| GFP_KERNEL | 普通内核分配，可能阻塞 |
| GFP_ATOMIC | 原子分配，不能阻塞 |
| GFP_HIGHUSER | 高优先级用户分配 |
| GFP_NOIO | 不能进行IO操作 |
| GFP_NOFS | 不能调用文件系统 |
| GFP_USER | 用户进程分配 |
| GFP_DMA | 从DMA区域分配 |
| GFP_HIGHMEM | 从高端内存分配 |

**修饰符组合：**

```c
// 常用组合
GFP_KERNEL          // 普通内核内存分配
GFP_ATOMIC          // 中断/原子上下文分配
GFP_KERNEL | __GFP_HIGHMEM   // 内核高端内存分配
GFP_USER | __GFP_IO          // 用户内存，可进行IO
GFP_NOIO | __GFP_COMP        // 无IO，分配复合页
```

**常用场景：**

| 场景 | 推荐标志 |
|------|----------|
| 进程上下文 | GFP_KERNEL |
| 中断上下文 | GFP_ATOMIC |
| DMA缓冲区 | GFP_KERNEL \| GFP_DMA |
| 用户进程 | GFP_USER |
| 页面回收期间 | GFP_NOIO |

### 释放函数

```c
// 释放页面
void __free_pages(struct page *page, unsigned int order);

void free_pages(unsigned long addr, unsigned int order);

// 释放单页
void free_page(unsigned long addr);
```

**使用示例：**

```c
// 分配2页
struct page *pages = alloc_pages(GFP_KERNEL, 1);
if (!pages) {
    return -ENOMEM;
}
void *addr = page_address(pages);

// ... 使用内存 ...

// 释放2页
__free_pages(pages, 1);

// 或使用地址方式
unsigned long addr = __get_free_pages(GFP_KERNEL, 1);
// ... 使用 ...
free_pages(addr, 1);
```

---

## 2.3 高阶内存分配

### CMA连续内存分配器

CMA(Contiguous Memory Allocator)用于分配连续的物理内存区域，常用于DMA缓冲区。

**CMA工作原理：**

1. 系统启动时预留一块内存区域
2. 平时可被其他功能使用
3. 需要时回收用于DMA分配
4. 分配后保证物理连续

**CMA配置：**

```c
// 内核配置
CONFIG_CMA=y
CONFIG_CMA_AREAS=7

// 设备树配置
reserved-memory {
    #address-cells = <1>;
    #size-cells = <1>;
    ranges;

    cma_region: cma@72000000 {
        compatible = "shared-dma-pool";
        reusable;
        reg = <0x72000000 0x2000000>;  // 32MB
        linux,cma-default;
    };
};
```

**使用CMA分配：**

```c
// 分配DMA缓冲区
struct page *cma_page;
dma_addr_t dma_handle;

cma_page = dma_alloc_from_contiguous(dev, size >> PAGE_SHIFT, order, GFP_KERNEL);
if (!cma_page)
    return -ENOMEM;

dma_handle = page_to_phys(cma_page);

// 释放
dma_release_from_contiguous(dev, cma_page, size >> PAGE_SHIFT);
```

### 内存碎片处理

**内存碎片类型：**

1. **内部碎片**
   - 分配单元内部未使用的空间
   - 无法避免，但可以优化

2. **外部碎片**
   - 空闲内存总量足够，但无法分配连续大块
   - Buddy系统的主要问题

**解决策略：**

1. **页面迁移**
   - 将已分配的页面移动到其他位置
   - 合并碎片区域

2. **内存压缩**
   - 将已分配的页面紧凑排列
   - 释放连续大块

3. **预留内存池**
   - 预先分配保留区域
   - 关键分配使用

4. **分配时重试**
   - 内存不足时触发回收
   - 重新尝试分配

---

## 本章小结

本章介绍了Buddy页面分配器：

1. **Buddy算法**按2的幂次划分内存块，支持伙伴合并
2. **分配API**包括alloc_pages、__get_free_pages等
3. **GFP标志**控制分配行为和内存区域选择
4. **CMA**提供连续物理内存分配能力
5. **碎片处理**通过迁移和压缩技术

---

## 思考题

1. Buddy算法为什么不能完全避免外部碎片？
2. GFP_ATOMIC和GFP_KERNEL的区别是什么？什么情况会导致分配失败？
3. CMA和普通的alloc_pages有什么区别？
4. 如何计算order值才能最有效利用内存？

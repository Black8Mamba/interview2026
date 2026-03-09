# 第5章 常用内存类型分配及用途

> 本章详细介绍内核态和用户态各种内存分配方式的API、特点和适用场景，并提供对比表格便于选择。

## 目录

## 5.1 内核态内存分配

### 物理页分配

**alloc_pages / __get_free_pages**

适用于需要物理连续内存的场景，如DMA缓冲区、页面分配。

```c
#include <linux/gfp.h>

// 分配2^order个连续物理页
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

// 分配2^order页，返回虚拟地址
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);

// 分配单页
struct page *alloc_page(gfp_t gfp_mask);
unsigned long get_free_page(gfp_t gfp_mask);

// 释放
void __free_pages(struct page *page, unsigned int order);
void free_pages(unsigned long addr, unsigned int order);

// 使用示例
struct page *pages = alloc_pages(GFP_KERNEL, 2);  // 分配4页(16KB)
if (!pages)
    return -ENOMEM;

void *addr = page_address(pages);
__free_pages(pages, 2);
```

**适用场景：**
- DMA一致性缓冲区
- 页面缓存
- 驱动帧缓冲区

---

### 小块连续内存: kmalloc / kzalloc

适用于内核数据结构分配，大小通常小于一页。

```c
#include <linux/slab.h>

// 分配size字节，返回虚拟地址（物理连续）
void *kmalloc(size_t size, gfp_t flags);

// 分配并清零
void *kzalloc(size_t size, gfp_t flags);

// 分配数组
void *kmalloc_array(size_t n, size_t size, gfp_t flags);

// 分配并清零的数组
void *kcalloc(size_t n, size_t size, gfp_t flags);

// 释放
void kfree(const void *objp);

// 使用示例
struct my_struct *p = kmalloc(sizeof(*p), GFP_KERNEL);
if (!p)
    return -ENOMEM;

// 使用kzalloc
struct my_struct *p2 = kzalloc(sizeof(*p2), GFP_KERNEL);
// 无需memset清零

kfree(p);
kfree(p2);
```

**kzalloc vs kmalloc：**

| 函数 | 说明 |
|------|------|
| kmalloc | 分配后内容未初始化，可能有旧数据 |
| kzalloc | 分配后内容全部清零，等价于kmalloc + memset(0) |

**适用场景：**
- 内核数据结构（task_struct、file等）
- 网络包缓冲区
- 驱动数据结构

---

### 大块非连续内存: vmalloc

适用于需要大块虚拟连续内存的场景。

```c
#include <linux/vmalloc.h>

// 分配size字节，虚拟地址连续，物理地址非连续
void *vmalloc(unsigned long size);

// 分配并清零
void *vzalloc(unsigned long size);

// 释放
void vfree(const void *addr);

// 使用示例
void *buf = vmalloc(1024 * 1024);  // 分配1MB
if (!buf)
    return -ENOMEM;

// 使用后释放
vfree(buf);
```

**vmalloc vs kmalloc：**

| 特性 | kmalloc | vmalloc |
|------|---------|---------|
| 虚拟地址 | 连续 | 连续 |
| 物理地址 | 连续 | 非连续 |
| 最大大小 | ~4MB | 无限制 |
| 性能 | 快 | 较慢 |
| 实现 | Buddy系统 | 页表映射 |

**适用场景：**
- 大缓冲区（如网络包缓冲）
- 模块加载
- 临时大块内存

---

### 对象缓存池: kmem_cache_create

适用于频繁分配/释放相同大小对象的场景。

```c
#include <linux/slab.h>

// 创建对象缓存池
struct kmem_cache *kmem_cache_create(
    const char *name,      // 缓存名称
    size_t size,          // 对象大小
    size_t align,         // 对齐要求
    unsigned long flags,  // 标志
    void (*ctor)(void *)  // 构造函数
);

// 销毁缓存池
void kmem_cache_destroy(struct kmem_cache *cache);

// 从缓存分配对象
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags);

// 从缓存分配并清零
void *kmem_cache_zalloc(struct kmem_cache *cache, gfp_t flags);

// 释放对象到缓存
void kmem_cache_free(struct kmem_cache *cache, void *obj);

// 使用示例
struct kmem_cache *my_cache = kmem_cache_create(
    "my_struct_cache",
    sizeof(struct my_struct),
    0,
    SLAB_HWCACHE_ALIGN,
    NULL
);

struct my_struct *obj = kmem_cache_alloc(my_cache, GFP_KERNEL);
// ...
kmem_cache_free(my_cache, obj);

kmem_cache_destroy(my_cache);
```

**适用场景：**
- 频繁创建/销毁的对象
- 网络缓冲区
- inode、dentry等VFS对象

---

### DMA一致性内存: dma_alloc_coherent

适用于DMA操作需要物理连续且一致的内存。

```c
#include <linux/dma-mapping.h>

// 分配DMA一致内存
void *dma_alloc_coherent(struct device *dev, size_t size,
                         dma_addr_t *dma_handle, gfp_t flags);

// 释放DMA内存
void dma_free_coherent(struct device *dev, size_t size,
                      void *cpu_addr, dma_addr_t dma_handle);

// 使用示例
void *buf;
dma_addr_t dma_handle;

buf = dma_alloc_coherent(dev, 1024, &dma_handle, GFP_KERNEL);
if (!buf)
    return -ENOMEM;

// 设置DMA寄存器
writel(dma_handle, dev->regs + DMA_SRC_REG);

// 使用后释放
dma_free_coherent(dev, 1024, buf, dma_handle);
```

**适用场景：**
- 网络设备DMA发送/接收缓冲区
- 声卡缓冲区
- 存储设备DMA

---

### IO映射内存: ioremap / iounmap

适用于将设备寄存器或IO内存映射到内核地址空间。

```c
#include <linux/io.h>

// IO映射（物理地址到虚拟地址）
void *ioremap(resource_size_t phys_addr, size_t size);

// IO映射（带缓存属性）
void *ioremap_cache(resource_size_t phys_addr, size_t size);

// 只读映射
void *ioremap_prot(resource_size_t phys_addr, size_t size,
                   unsigned long prot);

// 解除映射
void iounmap(void *addr);

// 使用示例
void __iomem *regs;

regs = ioremap(0xFED00000, 0x1000);  // 映射CReg空间
if (!regs)
    return -ENOMEM;

// 读写寄存器
writel(0x1234, regs + REG_CTRL);
val = readl(regs + REG_STATUS);

iounmap(regs);
```

**ioremap vs vmalloc：**

| 特性 | ioremap | vmalloc |
|------|---------|---------|
| 用途 | IO设备寄存器 | 普通内存 |
| 缓存属性 | 可配置 | 一般不可缓存 |
| 访问方式 | 通过读写函数 | 直接指针访问 |

**适用场景：**
- 设备寄存器映射
- MMIO内存访问
- 嵌入式系统外设

---

## 5.2 用户态内存分配

### 堆内存: malloc / free

C标准库提供的堆内存分配函数。

```c
#include <stdlib.h>

// 分配内存
void *malloc(size_t size);

// 分配并清零
void *calloc(size_t nmemb, size_t size);

// 调整分配大小
void *realloc(void *ptr, size_t size);

// 释放内存
void free(void *ptr);

// 使用示例
int *arr = malloc(100 * sizeof(int));
if (!arr)
    return -ENOMEM;

// 使用calloc
int *arr2 = calloc(100, sizeof(int));  // 自动清零

// 扩展
arr = realloc(arr, 200 * sizeof(int));

free(arr);
free(arr2);
```

---

### 内存映射: mmap

提供更灵活的内存分配方式。

```c
#include <sys/mman.h>

// 创建内存映射
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);

// 解除映射
int munmap(void *addr, size_t len);

// 使用示例：匿名私有映射
void *mem = mmap(NULL, 4096,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1, 0);
if (mem == MAP_FAILED)
    return -errno;

// 使用
memcpy(mem, "hello", 5);

// 释放
munmap(mem, 4096);
```

---

### 堆调整: brk

直接调整堆顶指针。

```c
#include <unistd.h>

// 调整堆大小
int brk(void *addr);

// 更安全的方式
void *sbrk(intptr_t increment);

// 使用示例
void *old_brk = sbrk(0);      // 获取当前堆顶
void *new_brk = sbrk(4096);   // 扩展4KB
if (new_brk == (void*)-1)
    return -errno;
```

---

### 对齐内存: posix_memalign

分配对齐的内存。

```c
#include <stdlib.h>

// 分配对齐内存
int posix_memalign(void **memptr, size_t alignment, size_t size);

// 释放(使用free)
void free(void *ptr);

// 使用示例
void *ptr;
int ret = posix_memalign(&ptr, 4096, 8192);  // 4KB对齐
if (ret)
    return ret;

// 使用后释放
free(ptr);

// C11标准
void *aligned_alloc(size_t alignment, size_t size);
```

---

### 内存分配参数: mallopt

调整内存分配器行为。

```c
#include <malloc.h>

// 参数选项
#define M_MMAP_THRESHOLD    3   // mmap阈值
#define M_TRIM_THRESHOLD    4   // trim阈值
#define M_TOP_PAD          5   // 顶部填充

// 设置参数
int mallopt(int param, int value);

// 使用示例
// 小于此阈值使用brk，大于使用mmap
mallopt(M_MMAP_THRESHOLD, 128 * 1024);  // 128KB阈值
```

---

## 5.3 分配方式对比

### 内核分配方式对比

| 分配方式 | 内存类型 | 连续性 | 最大大小 | 速度 | 典型用途 |
|----------|----------|--------|----------|------|----------|
| alloc_pages | 物理连续 | 物理连续 | 无限制 | 快 | DMA、页面缓存 |
| kmalloc | 虚拟连续 | 物理连续 | ~4MB | 快 | 内核数据结构 |
| vmalloc | 虚拟连续 | 物理非连续 | 无限制 | 较慢 | 大缓冲区 |
| kmem_cache | 虚拟连续 | 物理连续 | 缓存限制 | 快 | 频繁对象 |
| dma_alloc_coherent | 物理连续 | 物理连续 | 受限 | 快 | DMA设备 |
| ioremap | IO地址 | 不适用 | 受限 | 中 | 设备寄存器 |

### 用户态分配方式对比

| 分配方式 | 内存类型 | 分配速度 | 释放方式 | 典型用途 |
|----------|----------|----------|----------|----------|
| malloc | 堆 | 中等 | free | 通用分配 |
| mmap | 映射区 | 中等 | munmap | 大块内存 |
| brk/sbrk | 堆 | 快 | 自动 | 调整堆大小 |
| posix_memalign | 堆 | 中等 | free | 对齐内存 |

### 选择建议

**内核代码选择：**

1. **需要DMA** → `dma_alloc_coherent` 或 `alloc_pages`
2. **小对象、频繁分配** → `kmem_cache` 或 `kmalloc`
3. **大块缓冲区** → `vmalloc`
4. **设备寄存器** → `ioremap`

**用户代码选择：**

1. **普通使用** → `malloc`
2. **大块内存(>1MB)** → `mmap`
3. **需要对齐** → `posix_memalign`
4. **频繁调整大小** → `realloc`

---

## 本章小结

本章详细介绍了各种内存分配方式：

1. **内核态**：alloc_pages、kmalloc、vmalloc、kmem_cache、dma_alloc_coherent、ioremap
2. **用户态**：malloc、mmap、brk、posix_memalign、mallopt
3. **对比表格**：帮助在实际开发中做出正确选择

正确选择内存分配方式对系统性能和稳定性至关重要。

---

## 思考题

1. 为什么DMA缓冲区需要物理连续？
2. kmalloc和vmalloc哪个更适合中断处理上下文？为什么？
3. 为什么用户态malloc分配不需要考虑物理连续性？
4. ioremap映射的内存和普通内存访问方式有什么不同？

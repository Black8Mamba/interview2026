# 第3章 Slab分配器

> 本章介绍Linux内核的Slab分配器，包括Slab/Slub/Slob三种实现的区别、kmem_cache工作原理和kmalloc机制。

## 目录

## 3.1 Slab/Slub/Slob概述

### 三种分配器对比

Linux内核提供了三种Slab分配器实现：

| 分配器 | 说明 | 特点 | 使用场景 |
|--------|------|------|----------|
| Slab | 原始实现 | 功能完整，调试能力强 | 早期版本 |
| **Slub** | 优化实现 | 简单高效，减少开销 | **主流(默认)** |
| Slob | 简化实现 | 代码量小 | 嵌入式系统 |

**内核配置：**

```c
// mm/slab.h
enum slab_mode {
    SLAB,        // 传统Slab
    SLUB,        // Slub (默认)
    SLOB         // Slob (嵌入式)
};

// 选择编译
CONFIG_SLUB=y      // 默认使用Slub
CONFIG_SLOB=y      // 嵌入式使用Slob
```

### Slub是当前主流

Slub相对于Slab的优点：

1. **更简单的数据结构**
   - 不需要复杂的队列管理
   - 每个页面有一个简单的freelist

2. **更少的内存开销**
   - 每个对象只需要一个指针
   - 不需要额外的着色对象

3. **更好的调试支持**
   - 完整的错误检测
   - 红色的空洞(red zoning)

4. **更好的缓存局部性**
   - 单页内对象连续存放

---

## 3.2 kmem_cache工作原理

### 对象缓存池

kmem_cache是Slab分配器的核心结构，用于管理特定类型对象的缓存池。

**kmem_cache结构：**

```c
// mm/slub.c
struct kmem_cache {
    struct kmem_cache_cpu *cpu_slab;  // 每CPU缓存
    struct kmem_cache_node *node[MAX_NUMNODES];  // 节点缓存
    unsigned long flags;              // 标志
    unsigned long size;               // 对象大小
    unsigned long object_size;        // 实际对象大小
    unsigned long offset;             // freelist偏移
    unsigned int order;               // 页阶数
    unsigned int min_partial;         // 最小部分缓存数
    const char *name;                 // 缓存名称
    struct list_head list;            // 缓存链表
    int refcount;                     // 引用计数
    int cpu_partial;                  // 每CPU部分缓存数
    // ...
};
```

### 创建缓存池

```c
// 创建kmem_cache
struct kmem_cache *kmem_cache_create(
    const char *name,           // 缓存名称
    size_t size,                // 对象大小
    size_t align,               // 对齐要求
    unsigned long flags,       // 标志
    void (*ctor)(void *)        // 构造函数
);

// 示例：创建task_struct缓存
struct kmem_cache *task_cache = kmem_cache_create(
    "task_struct",
    sizeof(struct task_struct),
    ARCH_MIN_TASKALIGN,
    SLAB_PANIC | SLAB_NOTRACK,
    NULL
);

// 销毁kmem_cache
void kmem_cache_destroy(struct kmem_cache *s);
```

**flags选项：**

| 标志 | 说明 |
|------|------|
| SLAB_HWCACHE_ALIGN | 对象按缓存行对齐 |
| SLAB_POISON | 填充已知模式(调试) |
| SLAB_RED_ZONE | 红色区域(调试) |
| SLAB_PANIC | 分配失败时panic |
| SLAB_NOTRACK | 不跟踪分配信息 |
| SLAB_ACCOUNT | 计入memcg |

### 分配/释放对象

```c
// 从缓存池分配对象
void *kmem_cache_alloc(struct kmem_cache *s, gfp_t flags);

// 分配并清零
void *kmem_cache_zalloc(struct kmem_cache *s, gfp_t flags);

// 释放对象到缓存池
void kmem_cache_free(struct kmem_cache *s, void *obj);

// 使用示例
struct kmem_cache *my_cache;
void *obj;

my_cache = kmem_cache_create("my_obj", sizeof(struct my_struct),
                               0, SLAB_HWCACHE_ALIGN, NULL);

obj = kmem_cache_alloc(my_cache, GFP_KERNEL);
// ... 使用obj ...
kmem_cache_free(my_cache, obj);

kmem_cache_destroy(my_cache);
```

### 着色(Cache Coloring)

着色是一种优化技术，用于减少不同对象映射到同一缓存行导致的缓存冲突。

**原理：**

```
没有着色：
+---+---+---+---+---+---+---+---+
| A | A | A | A | A | A | A | A |  (对象A全部映射到同一缓存行)
+---+---+---+---+---+---+---+---+

着色后：
+---+---+---+---+---+---+---+---+
| A | A | A | A | A | A | A | A |  (对象A分散到不同缓存行)
+---+---+---+---+---+---+---+---+
+---+       (偏移offset)
```

**Slub中的实现：**

```c
// 每个slab有coloring偏移
struct page {
    void *freelist;         // freelist指针
    unsigned long inuse;    // 使用对象数
    // ...
};
```

---

## 3.3 kmalloc实现

### kmalloc接口

kmalloc用于分配小于页大小的内存块，返回虚拟地址连续的内存。

**函数原型：**

```c
// include/linux/slab.h
void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
void *kmalloc_array(size_t n, size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);

void kfree(const void *objp);
```

**大小限制：**

- 最大通常为4MB (KMALLOC_MAX_SIZE)
- 超过此大小使用vmalloc

### kmalloc与Slab对应关系

kmalloc使用一组kmem_cache，每个缓存对应特定大小范围：

| 大小范围 | 缓存名称 | 阶数 |
|----------|----------|------|
| 8 - 16B | kmalloc-16 | 0 |
| 17 - 32B | kmalloc-32 | 0 |
| 33 - 64B | kmalloc-64 | 0 |
| 65 - 128B | kmalloc-128 | 0 |
| 129 - 256B | kmalloc-256 | 0 |
| 257 - 512B | kmalloc-512 | 0 |
| 513 - 1024B | kmalloc-1k | 0 |
| 1025 - 2048B | kmalloc-2k | 1 |
| 2049 - 4096B | kmalloc-4k | 1 |

**内核启动时初始化：**

```c
// mm/slab_common.c
void __init create_kmalloc_caches(void)
{
    int i;

    for (i = 0; i < KMALLOC_SHIFT_HIGH; i++) {
        if (!kmalloc_caches[i]) {
            kmalloc_caches[i] = kmem_cache_create(
                kmalloc_info[i].name,
                1 << i,
                ARCH_KMALLOC_MINALIGN,
                SLAB_HWCACHE_ALIGN,
                NULL
            );
        }
    }
}
```

### 使用示例

```c
// 分配32字节
void *p = kmalloc(32, GFP_KERNEL);
if (!p)
    return -ENOMEM;

// 使用kzalloc分配并清零
struct my_struct *s = kzalloc(sizeof(*s), GFP_KERNEL);
if (!s)
    return -ENOMEM;

// 释放
kfree(p);
kfree(s);
```

---

## 3.4 Slab调试与统计

### Slab调试选项

**内核配置：**

```
CONFIG_SLUB_DEBUG=y              // 启用Slub调试
CONFIG_SLUB_DEBUG_ON=y           // 默认开启调试
CONFIG_DEBUG_KMEMLEAK=y          // 内存泄漏检测
```

**调试标志：**

| 标志 | 说明 |
|------|------|
| SLAB_POISON | 填充0x6b (0x6b6b6b6b) |
| SLAB_RED_ZONE | 红色区域检测溢出 |
| SLAB_STORE_USER | 跟踪分配者信息 |
| SLAB_TRACE | 记录分配/释放轨迹 |

### slabinfo工具

```bash
# 查看slab信息
cat /proc/slabinfo

# 常用命令
slabtop                  # 实时显示slab使用情况
```

**slabtop输出示例：**

```
 Active / Total Objects (% used)    : 1234567 / 2345678 (52.6%)
 Active / Total Slabs (% used)      : 12345 / 23456 (52.6%)
 Active / Total Caches (% used)     : 78 / 95 (82.1%)
 Active / Total Size (% used)       : 123.45M / 234.56M (52.6%)
 Minimum / Average / Maximum Object : 0.01K / 0.02K / 128.00K

  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME
 123456 123456 100%    0.50K  12345      10     12345K dentry
  23456  23456 100%    0.25K   2345      10      4690K kmalloc-256
```

### 调试内存问题

**检测内存损坏：**

```c
// 启用详细调试
echo 1 > /sys/kernel/debug/slub/verbose

// 检查特定缓存
cat /sys/kernel/debug/slub/kmalloc-256
```

**内存泄漏检测：**

```bash
# 启用kmemleak
echo scan > /sys/kernel/debug/kmemleak

# 查看报告
cat /sys/kernel/debug/kmemleak
```

---

## 本章小结

本章介绍了Slab分配器：

1. **三种实现**：Slab(功能全)、Slub(高效，默认)、Slob(嵌入式)
2. **kmem_cache**提供对象缓存池，支持构造函数和着色
3. **kmalloc**使用多级缓存池，大小从8B到4MB
4. **调试工具**包括slabtop、slabinfo、kmemleak

Slab分配器的核心思想是缓存常用对象，避免频繁的页分配和初始化开销。

---

## 思考题

1. Slab和Slub的主要区别是什么？为什么Slub成为主流？
2. kmalloc的大小为什么按2的幂次对齐？
3. 什么是cache coloring？它解决什么问题？
4. 如果分配失败，kmalloc和kzalloc的行为有什么区别？

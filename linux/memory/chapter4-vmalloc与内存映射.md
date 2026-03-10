# 第4章 vmalloc与内存映射

> 本章介绍vmalloc大块内存分配、mmap内存映射和brk堆内存管理机制。

## 目录

## 4.1 vmalloc原理

### vmalloc vs kmalloc

**主要区别：**

| 特性 | kmalloc | vmalloc |
|------|---------|---------|
| 内存类型 | 物理连续 | 物理非连续 |
| 虚拟地址 | 连续 | 连续 |
| 分配大小 | <4MB | 大块 |
| 性能 | 快 | 较慢 |
| 使用场景 | 内核数据结构 | 大缓冲区 |

**vmalloc原理：**

```
kmalloc分配:
虚拟地址: 0xFFFF800012340000 - 0xFFFF800012348000 (32KB)
物理地址: 0x12340000 - 0x12348000 (连续)

vmalloc分配:
虚拟地址: 0xFFFF800012400000 - 0xFFFF800012480000 (32KB)
物理地址: 0x10000000, 0x20000000, ... (非连续，多个页面)
```

### vmalloc区域布局

Linux内核虚拟地址空间布局：

```
+------------------+ 0xFFFFFFFFFFFF
|    vmalloc区     | 0xFFFF800000000000
+------------------+ (VMALLOC_START)
|   VMALLOC_END    |
+------------------+
|   I/O映射区      |
+------------------+
|   固定映射区     |
+------------------+
|   kernel image   |
+------------------+ 0xFFFF800000000000 (ARM64)
或                  0xC0000000 (x86)
|    内核空间      |
+------------------+ 0x0000000000000000
```

**关键参数：**

```c
// arch/x86/include/asm/pgtable_32_types.h (x86 32位)
#define VMALLOC_START  0xC0000000
#define VMALLOC_END    0xEFFFFFFF

// arch/arm64/include/asm/pgtable.h (ARM64)
#define VMALLOC_START  0xFFFFFC0000000000
#define VMALLOC_END    0xFFFFFD9F00000000
```

### vmalloc实现

**分配函数：**

```c
// mm/vmalloc.c
void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void *vmalloc_user(unsigned long size);
void *vmalloc_node(unsigned long size, int node);

// 示例
void *buf = vmalloc(1024 * 1024);  // 分配1MB
if (!buf)
    return -ENOMEM;

// 使用后释放
vfree(buf);
```

**内部实现：**

```c
// vmalloc核心实现
static void *__vmalloc_node(unsigned long size, unsigned long align,
                           gfp_t gfp_mask, pgprot_t prot,
                           int node, const void *caller)
{
    struct vm_struct *area;
    unsigned long addr;

    // 1. 分配vm_struct描述符
    area = __get_vm_area_node(size, align, VM_ALLOC | VM_UNINITIALIZED,
                              VMALLOC_START, VMALLOC_END, node,
                              gfp_mask, caller);

    // 2. 分配物理页面
    // 3. 建立页表映射
    addr = (unsigned long)area->addr;
    if (vmalloc_insert_page((void *)addr, page)) {
        vfree(area->addr);
        return NULL;
    }

    // 4. 返回虚拟地址
    return area->addr;
}
```

---

## 4.2 模块内存分配

### 内核模块内存布局

内核模块(.ko)加载时的内存布局：

```
+------------------+
|   代码段(.text)  |  可执行
+------------------+
|  只读段(.rodata)|
+------------------+
|   数据段(.data)  |  可读写
+------------------+
|  BSS段(.bss)     |  未初始化
+------------------+
|   堆             |  动态分配
+------------------+
```

### 模块内存分配方式

```c
// 模块中常用的内存分配方式

// 1. 使用kmalloc (推荐，小块)
void *buf = kmalloc(4096, GFP_KERNEL);

// 2. 使用vmalloc (大块)
void *buf = vmalloc(1024 * 1024);

// 3. 静态分配
static char my_buffer[4096];

// 模块卸载时释放
void cleanup_module(void)
{
    kfree(buf);
    vfree(buf);
}
MODULE_LICENSE("GPL");
```

---

## 4.3 mmap系统调用

### 内存映射类型

mmap创建内存映射，分为两种类型：

1. **文件映射**
   - 将文件内容映射到内存
   - 读写文件即读写内存

2. **匿名映射**
   - 不关联文件
   - 用于分配内存

**共享 vs 私有：**

| 类型 | 说明 |
|------|------|
| MAP_SHARED | 映射对所有进程可见，写入同步到文件 |
| MAP_PRIVATE | 私有映射，写入触发Copy-On-Write |
| MAP_ANONYMOUS | 匿名映射，不关联文件 |

### mmap系统调用

```c
// 用户空间
#include <sys/mman.h>

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);

// 参数说明:
// addr:   建议地址，NULL表示由内核选择
// len:    映射长度
// prot:   保护标志 (PROT_READ|PROT_WRITE|PROT_EXEC)
// flags:  映射类型 (MAP_SHARED|MAP_PRIVATE|MAP_ANONYMOUS)
// fd:     文件描述符 (匿名映射时为-1)
// off:    文件偏移量

// 释放映射
int munmap(void *addr, size_t len);
```

**使用示例：**

```c
// 匿名映射 (分配内存)
void *mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
if (mem == MAP_FAILED)
    return -errno;

// 文件映射
int fd = open("file.txt", O_RDONLY);
void *mem = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
close(fd);  // 映射仍有效

// 释放
munmap(mem, 4096);
```

### 内核实现

```c
// sys_mmap (简化版)
asmlinkage long sys_mmap(unsigned long addr, unsigned long len,
                        unsigned long prot, unsigned long flags,
                        unsigned long fd, unsigned long off)
{
    struct file *file = NULL;
    struct mm_struct *mm = current->mm;

    // 验证参数
    if (offset_in_page(off))
        return -EINVAL;

    // 获取文件结构
    if (flags & MAP_SHARED)
        file = fget(fd);

    // 调用驱动函数
    return vm_mmap(file, addr, len, prot, flags, off);
}
```

---

## 4.4 brk系统调用

### 堆内存管理

brk()系统调用用于调整进程的堆顶地址：

```
+------------------+ 0x00007FFFFFFFFFFF (用户空间顶部)
|     栈           |
+------------------+
|     映射区       |
+------------------+
|      堆 ->       |  brk()增长
+------------------+
|     BSS          |
+------------------+
|     数据         |
+------------------+
|     代码         |
+------------------+ 0x0000000000400000
```

### brk实现

```c
// 系统调用
asmlinkage long sys_brk(unsigned long brk);

// 用户空间库函数
int brk(void *addr);

// 简化实现
unsigned long do_brk(unsigned long addr, unsigned long len)
{
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;
    unsigned long newbrk, oldbrk;

    // 1. 检查地址合法性
    newbrk = PAGE_ALIGN(addr);
    oldbrk = PAGE_ALIGN(mm->brk);

    if (newbrk == oldbrk)
        return oldbrk;

    // 2. 如果收缩，释放VMA
    if (newbrk < oldbrk) {
        do_munmap(mm, newbrk, oldbrk - newbrk);
        goto out;
    }

    // 3. 如果扩展，创建新VMA
    vma = vm_area_alloc();
    vma->vm_start = oldbrk;
    vma->vm_end = newbrk;
    vma->vm_flags = VM_READ | VM_WRITE | VM_MAYEXPAND;
    vma->vm_file = NULL;

    // 4. 插入VMA树
    insert_vm_struct(mm, vma);

out:
    mm->brk = newbrk;
    return newbrk;
}
```

### malloc实现原理

malloc底层使用brk或mmap：

```c
// glibc实现
void *malloc(size_t bytes)
{
    // 小块使用brk() (默认<=128KB)
    if (bytes <= 128 * 1024) {
        return sbrk(bytes);
    }
    // 大块使用mmap
    else {
        return mmap(NULL, bytes, PROT_READ|PROT_WRITE,
                    MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    }
}
```

---

## 本章小结

本章介绍了vmalloc和内存映射机制：

1. **vmalloc**分配虚拟连续但物理非连续的内存，适合大块分配
2. **mmap**创建文件映射或匿名映射，是用户空间重要内存分配方式
3. **brk**调整堆顶，管理小规模内存分配
4. **malloc**底层结合brk和mmap，根据大小选择

理解这些机制对于掌握Linux内存管理至关重要。

---

## 思考题

1. vmalloc分配为什么比kmalloc慢？它适合什么场景？
2. MAP_SHARED和MAP_PRIVATE的区别是什么？
3. brk和mmap哪个适合大内存分配？为什么？
4. 为什么64位系统不存在内存碎片问题（相对于32位）？

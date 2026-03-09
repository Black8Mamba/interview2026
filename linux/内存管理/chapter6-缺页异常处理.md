# 第6章 缺页异常处理

> 本章介绍缺页异常(Page Fault)的处理机制，包括缺页异常分类、page fault处理流程、COW写时复制和swap机制。

## 目录

## 6.1 缺页异常分类

### 合法性分类

缺页异常按访问合法性分为两类：

1. **合法性缺页 (Valid Page Fault)**
   - 进程访问的虚拟地址合法
   - 但对应的物理页不在内存中
   - 需要分配物理页并建立映射

2. **非法缺页 (Invalid Page Fault)**
   - 进程访问的虚拟地址不合法
   - 如访问未映射的内存区域
   - 通常导致段错误(SEGFAULT)

### 错误码分类

x86架构的page fault错误码：

```
错误码格式:
+---+---+---+---+
| I | R | W | U |
+---+---+---+---+

- I (Instruction): 1=取指令触发
- R (Reserved): 1=访问保留位
- W (Write): 1=写操作触发
- U (User): 1=用户态访问
```

**常见错误码组合：**

| 错误码 | 含义 |
|--------|------|
| 0x01 | 用户态读 → 合法缺页 |
| 0x02 | 用户态写 → 写保护缺页(COW) |
| 0x04 | 内核态访问 |
| 0x07 | 内核态写 → 写保护缺页 |

### 缺页原因细分

| 原因 | 说明 |
|------|------|
| 第一次访问 | BSS段、全局未初始化变量 |
| 匿名映射 | malloc通过brk扩张堆 |
| 文件映射 | mmap打开的文件 |
| 写时复制 | fork后父子进程共享 |
| 越界访问 | 访问数组外内存 |
| 访问NULL | 解引用空指针 |

---

## 6.2 page fault处理流程

### 整体流程

```
CPU访问虚拟地址
        ↓
MMU查找TLB
        ↓ (TLB miss)
查找页表
        ↓
页表项无效?
        ↓ Yes
触发page fault
        ↓
进入内核异常处理
        ↓
do_page_fault()
        ↓
判断地址合法性
        ↓
处理缺页 (分配物理页)
        ↓
更新页表项
        ↓
刷新TLB
        ↓
返回用户态重试指令
```

### do_page_fault实现

```c
// arch/x86/mm/fault.c
static void __kprobes
noinline page_fault(struct pt_regs *regs, unsigned long error_code)
{
    // 获取触发异常的地址
    unsigned long address = read_cr2();

    // 调用主处理函数
    do_page_fault(regs, error_code, address);
}

void do_page_fault(struct pt_regs *regs, unsigned long error_code,
                  unsigned long address)
{
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma;
    int fault;

    // 1. 错误码解析
    // bit 0 = 0: 读缺页, 1: 写缺页
    // bit 1 = 0: 物理页不存在, 1: 保护错误
    // bit 2 = 0: 内核态, 1: 用户态

    // 2. 检查内核态缺页
    if (unlikely(fault_in_kernel_space(address))) {
        do_kernel_fault(regs, error_code, address);
        return;
    }

    // 3. 获取VMA
    down_read(&mm->mmap_sem);
    vma = find_vma(mm, address);

    // 4. 非法地址检查
    if (!vma || address < vma->vm_start) {
        up_read(&mm->mmap_sem);
        bad_area(regs, error_code, address);
        return;
    }

    // 5. 正常缺页处理
    fault = handle_mm_fault(vma, address, error_code);

    // 6. 处理结果
    if (unlikely(fault & VM_FAULT_ERROR)) {
        if (fault & VM_FAULT_OOM)
            page_fault_oom(regs, error_code);
        else if (fault & VM_FAULT_SIGBUS)
            do_sigbus(regs, error_code, address);
    }

    up_read(&mm->mmap_sem);
}
```

### handle_mm_fault

```c
// mm/memory.c
int handle_mm_fault(struct vm_area_struct *vma, unsigned long address,
                   unsigned int flags)
{
    struct mm_struct *mm = vma->vm_mm;
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    int ret;

    // 1. 获取页表项
    pgd = pgd_offset(mm, address);
    pud = pud_alloc(mm, pgd, address);
    if (!pud)
        return VM_FAULT_OOM;

    pmd = pmd_alloc(mm, pud, address);
    if (!pmd)
        return VM_FAULT_OOM;

    // 2. 处理PMD级映射(2MB大页)
    if (pmd_trans_huge(*pmd)) {
        // 处理透明大页
    }

    // 3. 处理PTE级映射
    ret = __handle_mm_fault(mm, vma, address, pmd, flags);

    return ret;
}
```

---

## 6.3 COW写时复制

### 原理

Copy-On-Write(写时复制)是一种优化技术：

- fork()后父子进程共享相同的物理页
- 只有当某个进程写入时才复制物理页
- 节省内存和复制时间

### fork中的COW

```c
// fork实现简化
long do_fork(unsigned long clone_flags, unsigned long stack_start,
            unsigned long stack_size,
            int __user *parent_tidptr, int __user *child_tidptr)
{
    struct task_struct *p;
    // ... 分配task_struct ...

    // 复制父进程内存
    if (clone_flags & CLONE_VM) {
        // 共享内存空间
        atomic_inc(&current->mm->mm_users);
        p->mm = current->mm;
    } else {
        // 复制内存空间
        p->mm = dup_mm(current->mm);  // 复制页表，不复制物理页
    }

    // 标记页表为只读(触发COW)
    copy_page_range(p->mm, current->mm);
}
```

### 写时复制处理

```c
// 写保护缺页处理
static int do_wp_page(struct mm_struct *mm, struct vm_area_struct *vma,
                     unsigned long address, pte_t *page_table,
                     pmd_t *pmd, spinlock_t *ptl, pte_t orig_pte)
{
    struct page *old_page, *new_page;
    void *old_addr;
    int ret = 0;

    old_page = vm_normal_page(vma, address, orig_pte);
    if (!old_page)
        return -EFAULT;

    // 如果是共享页面，需要复制
    if (PageAnon(old_page)) {
        // 检查是否可以复用
        if (PageAnon(old_page) && page_mapcount(old_page) == 1) {
            // 只有自己使用，可以直接写
            pte_unmap_unlock(page_table, ptl);
            return VM_FAULT_WRITE;
        }

        // 需要复制页面
        new_page = alloc_page_vma(GFP_HIGHUSER, vma, address);
        if (!new_page)
            return VM_FAULT_OOM;

        // 复制内容
        copy_user_highpage(new_page, old_page, address);

        // 建立新的映射
        flush_cache_page(vma, address, page_to_pfn(new_page));
        pte_val = mk_pte(new_page, vma->vm_page_prot);
        set_pte_at(mm, address, page_table, pte_val);

        // 释放旧页面引用
        page_remove_rmap(old_page);
        put_page(old_page);
    }

    pte_unmap_unlock(page_table, ptl);
    return ret;
}
```

### COW流程图

```
fork()
  ↓
复制页表，共享物理页
  ↓
标记页表为只读
  ↓
子进程写入 → page fault
  ↓
do_wp_page()
  ↓
分配新页面
  ↓
复制内容
  ↓
建立新映射
```

---

## 6.4 swap机制

### swap空间

swap空间用于扩展可用内存：

1. **swap分区** - 独立的磁盘分区
2. **swap文件** - 文件系统中的文件

**创建swap文件：**

```bash
# 创建1GB swap文件
dd if=/dev/zero of=/swapfile bs=1M count=1024
mkswap /swapfile
swapon /swapfile

# 关闭
swapoff /swapfile
```

### swap换入换出

**swap out (换出)：**

```c
// 页面换出流程
void pageout(struct page *page, struct address_space *mapping)
{
    // 1. 如果是脏页，先写回磁盘
    if (PageDirty(page)) {
        if (mapping->a_ops->writepage)
            mapping->a_ops->writepage(page, &wbc);
        return;
    }

    // 2. 添加到swap缓存
    if (!add_to_swap_cache(page, swap, GFP_ATOMIC)) {
        // 3. 从页表中移除
        delete_from_page_table(page);

        // 4. 释放物理页
        free_page_and_swap_cache(page);
    }
}
```

**swap in (换入)：**

```c
// 页面换入流程
struct page *swapin_readahead(swp_entry_t entry, gfp_t gfp_mask,
                              struct vm_area_struct *vma,
                              unsigned long addr)
{
    struct page *page;

    // 1. 分配物理页
    page = alloc_page_vma(gfp_mask, vma, addr);
    if (!page)
        return NULL;

    // 2. 从swap读取数据
    if (swap_readpage(page, frontswap)) {
        put_page(page);
        return NULL;
    }

    // 3. 建立页表映射
    set_pte_at(vma->vm_mm, addr, pte, mk_pte(page, vma->vm_page_prot));

    // 4. 激活页面
    activate_page(page);

    return page;
}
```

---

## 本章小结

本章介绍了缺页异常处理机制：

1. **缺页异常分类**：合法vs非法，错误码含义
2. **处理流程**：do_page_fault → handle_mm_fault → __handle_mm_fault
3. **COW写时复制**：fork优化，共享页面，写时复制
4. **swap机制**：swap空间，换入换出流程

理解page fault处理对于调试内存问题和优化性能非常重要。

---

## 思考题

1. page fault和TLB miss有什么区别？哪个开销更大？
2. 为什么fork后不立即复制所有页面？
3. COW会导致物理页数量增加还是减少？
4. swap空间不足时会发生什么？

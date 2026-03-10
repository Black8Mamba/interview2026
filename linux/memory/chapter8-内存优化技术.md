# 第8章 内存优化技术

> 本章介绍Linux内存优化技术，包括大页内存(HugeTLB)、内存压缩(compaction)和内存峰值控制(memcg)。

## 目录

## 8.1 大页内存(HugeTLB)

### HugePage原理

HugePage(大页)使用比标准4KB更大的页面：

| 页面大小 | 名称 | 典型用途 |
|----------|------|----------|
| 2MB | HugePage | 大型应用 |
| 1GB | THP (透明大页) | 数据库 |

**标准页 vs 大页：**

```
标准4KB页:
虚拟地址: [页号:12位][偏移:12位]
TLB条目数: 需要大量条目

2MB大页:
虚拟地址: [页号:21位][偏移:21位]
TLB条目数: 减少到1/512
```

### HugePage配置

**内核配置：**

```
CONFIG_HUGETLBFS=y
CONFIG_HUGETLB_PAGE=y
CONFIG_TRANSPARENT_HUGEPAGE=y
```

**配置大页数量：**

```bash
# 查看当前大页配置
cat /proc/meminfo | grep Huge

# 设置2MB大页数量 (预留内存)
echo 256 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_free_hugepages

# 永久配置 (sysctl.conf)
echo "vm.nr_hugepages = 256" >> /etc/sysctl.conf
sysctl -p
```

### 使用HugePage

**系统调用：**

```c
#include <sys/mman.h>

// 使用HugePage
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                 -1, 0);

// 使用SHM_HUGETLB (System V共享内存)
key_t key = ftok("/tmp", 1);
int shmid = shmget(key, size, IPC_CREAT | SHM_HUGETLB | 0666);
void *ptr = shmat(shmid, NULL, 0);
```

**mount hugetlbfs：**

```bash
# 挂载hugetlbfs文件系统
mount -t hugetlbfs none /mnt/hugepages

# 创建大页文件
dd if=/dev/zero of=/mnt/hugepages/file bs=1M count=2

# 应用程序使用
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
```

### 透明大页(THP)

Transparent Huge Page (THP) 自动使用大页，无需应用修改。

**启用/禁用：**

```bash
# 查看状态
cat /sys/kernel/mm/transparent_hugepage/enabled

# 启用 (always/never/madvise)
echo always > /sys/kernel/mm/transparent_hugepage/enabled
echo never > /sys/kernel/mm/transparent_hugepage/enabled
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled
```

**madvise使用：**

```c
// 建议内核使用大页
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// 告诉内核这个区域适合使用大页
madvise(ptr, size, MADV_HUGEPAGE);
```

---

## 8.2 内存压缩(compaction)

### 压缩回收原理

内存压缩解决内存碎片问题：

1. **问题**：Buddy系统分配大块内存时，物理地址可能不连续
2. **解决**：将已分配的页面移动到一起，释放连续空间

**压缩前后对比：**

```
压缩前 (碎片化):
[使用][使用][空闲][使用][空闲][使用][使用]
  ↓ 压缩
[使用][使用][使用][使用][使用][空闲][空闲]
```

### 压缩实现

**触发条件：**

```c
// 触发内存压缩的条件
static inline bool should_compact_defer(struct zone *zone,
                                        unsigned int order)
{
    int free_pages = zone_page_state(zone, NR_FREE_PAGES);
    int migrate_pages = zone_page_state(zone, NR_MIGRATE_ISOLATE);

    // 如果空闲页和可移动页充足
    if (free_pages > (1 << order))
        return false;

    return true;
}
```

**压缩流程：**

```
开始compact
  ↓
扫描源区域 (migrate type)
  ↓
识别可移动页面
  ↓
移动页面到目标区域
  ↓
更新页表
  ↓
完成compact
```

### compaction配置

```bash
# 查看compaction状态
cat /proc/zoneinfo | grep -A 10 compaction

# 手动触发compaction
echo 1 > /sys/kernel/mm/transparent_hugepage/defrag

# 查看碎片指数
cat /proc/buddyinfo
```

---

## 8.3 内存峰值控制(memcg)

### cgroup内存限制

Memory Cgroup (memcg) 提供进程组的内存限制：

**创建cgroup：**

```bash
# 创建内存限制组
mkdir -p /sys/fs/cgroup/memory/limit_group

# 设置内存上限 (100MB)
echo 104857600 > /sys/fs/cgroup/memory/limit_group/memory.limit_in_bytes

# 设置内存+swap上限
echo 209715200 > /sys/fs/cgroup/memory/limit_group/memory.memsw.limit_in_bytes

# 添加进程
echo PID > /sys/fs/cgroup/memory/limit_group/tasks
```

### 内存峰值统计

**监控峰值内存：**

```bash
# 查看内存使用
cat /sys/fs/cgroup/memory/limit_group/memory.usage_in_bytes

# 查看峰值内存
cat /sys/fs/cgroup/memory/limit_group/memory.max_usage_in_bytes

# 重置峰值统计
echo 0 > /sys/fs/cgroup/memory/limit_group/memory.max_usage_in_bytes
```

**软限制：**

```bash
# 设置软限制 (内存不足时触发)
echo 52428800 > /sys/fs/cgroup/memory/limit_group/memory.soft_limit_in_bytes
```

### OOM控制

**oom_group：**

```bash
# 当内存不足时杀死整个组
echo 1 > /sys/fs/cgroup/memory/limit_group/memory.oom_group

# 查看oom事件
cat /sys/fs/cgroup/memory/limit_group/memory.oom_control
```

---

## 本章小结

本章介绍了内存优化技术：

1. **HugeTLB**：大页内存减少TLB miss，提高性能
2. **Compaction**：内存压缩解决碎片，支持大块分配
3. **memcg**：cgroup内存控制，限制和统计内存使用

这些技术在大规模系统和性能敏感场景中非常重要。

---

## 思考题

1. HugePage有什么优缺点？什么场景适合使用？
2. THP和手动配置的HugePage有什么区别？
3. 内存压缩的开销是什么？为什么不能在任何时候都使用？
4. cgroup内存限制和应用内的内存限制有什么区别？

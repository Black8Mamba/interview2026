# 第9章 内存调试与工具

> 本章介绍Linux内存调试和分析工具，包括/proc文件系统、常用命令、perf性能和页面缓存。

## 目录

## 9.1 /proc文件系统

### /proc/meminfo详解

```bash
# 查看内存信息
cat /proc/meminfo
```

**输出解析：**

```
MemTotal:        8000000 kB     # 总物理内存
MemFree:         4000000 kB     # 空闲内存
MemAvailable:    6000000 kB     # 可用内存(估算)
Buffers:           50000 kB     # 缓冲区
Cached:          2000000 kB     # 页面缓存
SwapCached:         1000 kB    # 换入的缓存
Active:          1500000 kB     # 活动内存(最近使用)
Inactive:        1000000 kB     # 非活动内存
Active(anon):     800000 kB     # 匿名活动内存
Inactive(anon):   500000 kB     # 匿名非活动内存
Active(file):     700000 kB     # 文件活动内存
Inactive(file):   500000 kB     # 文件非活动内存
Unevictable:          100 kB    # 不可回收内存
Mlocked:               100 kB    # 锁定的内存
SwapTotal:       2000000 kB     # swap总大小
SwapFree:        1990000 kB     # swap空闲
Dirty:               1000 kB    # 脏页(待写回)
Writeback:             500 kB    # 正在写回的页
AnonPages:        1300000 kB    # 匿名页(堆栈)
Mapped:            500000 kB     # 映射的内存
Shmem:             100000 kB    # 共享内存
Slab:              200000 kB    # Slab分配器使用
SReclaimable:      150000 kB    # 可回收Slab
SUnreclaim:         50000 kB    # 不可回收Slab
KernelStack:         10000 kB    # 内核栈
PageTables:         10000 kB    # 页表内存
NFS_Unstable:           0 kB    # NFS不稳定页
Bounce:                0 kB     # 弹跳缓冲区
WritebackTmp:          0 kB     # 写回临时内存
CommitLimit:      6000000 kB    # 提交限制
Committed_AS:     3000000 kB    # 已提交内存
VmallocTotal:    100000000 kB   # vmalloc总空间
VmallocUsed:        50000 kB    # vmalloc已使用
VmallocChunk:       10000 kB    # vmalloc剩余大块
```

### /proc/buddyinfo

查看Buddy系统碎片状态：

```bash
cat /proc/buddyinfo
```

**输出：**

```
Node 0, zone    DMA      0      0      0      1      1      1      0      0      1      1
Node 0, zone   DMA32   1000   800   400   200   100    50    20    10     5     2
Node 0, zone  Normal  50000  40000  30000  20000  10000  5000  2000  1000   500   200
```

**列含义：**
- 每个数字表示该阶(order)的空闲块数量
- order 0 = 1页(4KB), order 1 = 2页(8KB), ...
- 数字越小说明碎片化越严重

### /proc/vmallocinfo

查看vmalloc使用情况：

```bash
cat /proc/vmallocinfo
```

**输出：**

```
0xffffc90000000000-0xffffc90000200000 2097152 vmalloc  iomap_copy+0x0/0x80 [kernel]
0xffffc90000200000-0xffffc90000400000 2097152 vmalloc  module_memmap+0x46/0x100 [ext4]
0xffffc90010000000-0xffffc90020000000 268435456 vmalloc  ioremap+0x0/0x1000 [kernel]
```

### /proc/slabinfo

查看Slab分配器状态(需要root权限)：

```bash
cat /proc/slabinfo
```

**输出：**

```
slabinfo - version: 2.1
# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>
: slabdata <active_slabs> <num_slabs> <shared_avail>
kmalloc-256           12345   15000    256   16     1   1234   1000   0
kmalloc-128           23456   30000    128   32     1   2345   1000   0
dentry               50000   60000    192   21     1   3000   1000   0
inode_cache         10000   12000    512    8     1   1500   1000   0
```

---

## 9.2 常用命令

### slabtop

实时显示Slab使用情况：

```bash
# 安装
apt-get install procps

# 运行
slabtop
```

**常用选项：**

```bash
slabtop -s c   # 按cache大小排序
slabtop -s a   # 按活动对象排序
slabtop -o     # 单次显示
```

### vmstat

虚拟内存统计：

```bash
# 1秒刷新
vmstat 1

# 显示详细信息
vmstat -a
```

**输出列说明：**

```
procs:
  r: 运行队列(等待CPU的进程)
  b: 不可中断睡眠(通常IO等待)

memory:
  swpd: 使用的swap
  free: 空闲内存
  buff: 缓冲区
  cache: 缓存

swap:
  si: 从swap换入
  so: 换出到swap

io:
  bi: 块设备接收
  bo: 块设备发送

system:
  in: 中断数
  cs: 上下文切换

cpu:
  us: 用户态
  sy: 内核态
  id: 空闲
  wa: IO等待
```

### ps/mem

查看进程内存使用：

```bash
# 查看进程内存(RSS)
ps aux --sort=-rss | head

# 查看VMRSS
cat /proc/PID/status | grep -i vm

# 查看smaps (详细内存映射)
cat /proc/PID/smaps
```

**smaps输出：**

```
00400000-004df000 r-xp 00000000 fd:00 123456    /bin/prog
Size:               892 kB
Rss:                500 kB
Pss:                300 kB
Shared_Clean:      400 kB
Shared_Dirty:        0 kB
Private_Clean:      80 kB
Private_Dirty:      20 kB
Referenced:        500 kB
Anonymous:          20 kB
AnonHugePages:       0 kB
Swap:                0 kB
KernelPageSize:      4 kB
MMUPageSize:        4 kB
```

---

## 9.3 perf性能分析

### 内存相关事件

```bash
# 列出内存事件
perf list | grep -i memory

# 常见事件:
#   mem-loads           - 内存加载
#   mem-stores          - 内存存储
#   cache-references   - 缓存引用
#   cache-misses       - 缓存未命中
#   page-faults        - 缺页异常
```

### 内存热点分析

```bash
# 记录perf数据
perf record -g -a -e mem-loads,mem-stores -- sleep 10

# 查看报告
perf report

# 查看热点函数
perf report -g graph
```

### 缺页异常分析

```bash
# 分析缺页异常
perf record -e page-faults -g -- sleep 10

# 查看缺页异常分布
perf report

# 按pid过滤
perf record -e page-faults -p 1234 -- sleep 10
```

---

## 9.4 页面缓存

### 页缓存原理

页缓存(Page Cache)是磁盘块的内存缓存：

```
读取文件:
  磁盘 → 页面缓存 → 用户空间

写入文件:
  用户空间 → 页面缓存 → 延迟写回磁盘
```

### cache操作

```bash
# 手动刷新缓存到磁盘
sync

# 释放页面缓存(慎用)
echo 1 > /proc/sys/vm/drop_caches

# 释放dentries和inodes
echo 2 > /proc/sys/vm/drop_caches

# 释放所有缓存
echo 3 > /proc/sys/vm/drop_caches
```

### 监控缓存

```bash
# 查看缓存命中率
cat /proc/vmstat | grep -E "pgpgin|pgpgout|pswpin|pswpout"

# 查看dentries和inodes
cat /proc/vmstat | grep -E "dentry|inode"

# 查看slab缓存
slabtop
```

---

## 本章小结

本章介绍了内存调试工具：

1. **/proc文件系统**：meminfo、buddyinfo、vmallocinfo、slabinfo
2. **常用命令**：slabtop、vmstat、ps
3. **perf**：性能分析、热点分析、缺页异常分析
4. **页面缓存**：缓存刷新和监控

这些工具是排查内存问题的必备技能。

---

## 思考题

1. /proc/meminfo中MemAvailable和MemFree有什么区别？
2. 如何判断系统内存碎片化程度？
3. drop_caches会不会影响正在运行的程序？
4. perf如何帮助定位内存性能问题？

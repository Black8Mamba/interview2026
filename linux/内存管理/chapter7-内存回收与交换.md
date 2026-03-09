# 第7章 内存回收与交换

> 本章介绍Linux内存回收机制，包括LRU算法、kswapd页面回收、内存阈值和swap机制。

## 目录

## 7.1 LRU算法

### LRU基本原理

LRU(Least Recently Used，最近最少使用)是一种页面置换算法：

- 优先淘汰最长时间未被访问的页面
- 基于"在过去一段时间内未被访问的页面，未来也很可能不会被访问"的假设

### 双向链表实现

Linux使用双向链表实现LRU：

```c
// include/linux/mm.h
struct page {
    // ...
    struct {
        struct page *next;
        struct page *prev;
    } lru;
    // ...
};

// LRU链表类型
enum {
    LRU_INACTIVE_ANON = 0,    // 非活动匿名页
    LRU_ACTIVE_ANON,          // 活动匿名页
    LRU_INACTIVE_FILE,        // 非活动文件页
    LRU_ACTIVE_FILE,          // 活动文件页
    LRU_UNEVICTABLE,         // 不可回收页
};
```

**LRU链表组织：**

```
+----------------------------------------------------------+
|                      内存节点                             |
|  +--------------------------------------------------+    |
|  |              LRU_active                         |    |
|  |  [recently used] <---> ... <---> [least used] |    |
|  +--------------------------------------------------+    |
|  +--------------------------------------------------+    |
|  |              LRU_inactive                       |    |
|  |  [recently used] <---> ... <---> [least used] |    |
|  +--------------------------------------------------+    |
+----------------------------------------------------------+
```

### LRU分组

Linux将页面分为多个LRU组：

| LRU组 | 说明 | 典型页面 |
|-------|------|----------|
| LRU_INACTIVE_ANON | 非活动匿名页 | 堆、栈 |
| LRU_ACTIVE_ANON | 活动匿名页 | 频繁访问的匿名页 |
| LRU_INACTIVE_FILE | 非活动文件页 | 文件缓存 |
| LRU_ACTIVE_FILE | 活动文件页 | 频繁访问的文件页 |
| LRU_UNEVICTABLE | 不可回收页 | 锁定的页面 |

**页面如何移动：**

```
访问页面 → 从inactive移动到active (如果存在且最近使用)
回收时 → 优先从inactive选择
```

### pagevec和LRU操作

```c
// 使用pagevec操作LRU
#include/linux/pagevec.h

// 将页面添加到LRU
void pagevec_add(struct pagevec *pvec, struct page *page);

// 刷新LRU
void __pagevec_lru_add(struct pagevec *pvec);

// 示例：遍历inactive LRU
void scan_inactive_list(struct zone *zone)
{
    struct pagevec pvec;
    struct page *page;
    int i;

    pagevec_init(&pvec);

    while (pagevec_lru_move_fn(&pvec, zone, 0,
                               sc, &inactive_list_is_empty)) {
        for (i = 0; i < pvec.nr; i++) {
            page = pvec.pages[i];
            // 处理页面
        }
        pagevec_release(&pvec);
    }
}
```

---

## 7.2 kswapd回收机制

### 回收线程

kswapd是Linux内核的页面回收线程：

1. **周期性运行** - 检查内存压力
2. **异步回收** - 在内存不足前主动回收
3. **平衡内存** - 保持各zone内存平衡

**启动kswapd：**

```c
// mm/vmscan.c
static int __init kswapd_init(void)
{
    int nid;

    // 为每个节点创建kswapd
    for_each_node_state(nid, N_MEMORY)
        kswapd_run(nid);

    return 0;
}
late_initcall(kswapd_run);
```

### 回收触发条件

kswapd基于内存阈值触发：

| 阈值 | 说明 |
|------|------|
| pages_high | 高水位，触发后台回收 |
| pages_low | 低水位，触发激进回收 |
| pages_min | 最小水位，触发直接回收 |

**阈值计算：**

```c
// 计算zone水位
void calculate_zone_inactive_ratio(struct zone *zone)
{
    unsigned long pages_high, pages_low, pages_min;

    // 基于zone大小计算
    pages_min = zone->present_pages / 100;        // 1%
    pages_low = pages_min * 2;                     // 2%
    pages_high = pages_min * 3;                    // 3%

    zone->pages_high = pages_high;
    zone->pages_low = pages_low;
    zone->pages_min = pages_min;
}
```

### 回收流程

```c
// kswapd主循环
int kswapd_run(int nid)
{
    struct task_struct *kswapd;

    kswapd = kthread_run(kswapd, (void *)nid, "kswapd%d", nid);
    if (IS_ERR(kswapd))
        return PTR_ERR(kswapd);

    return 0;
}

static int kswapd(void *p)
{
    int nid = (int)p;
    struct zone *zone;

    for (;;) {
        // 1. 检查是否需要回收
        for_each_zone(zone) {
            if (zone_watermark_ok(zone, 0, 0, 0, 0))
                continue;  // 内存足够
            // 需要回收
            wakeup_kswapd(zone);
        }

        // 2. 执行回收
       kswapd_shrink_node();

        // 3. 休眠等待
        wait_event_freezable(kswapd_wait,
            condition_check());
    }
}
```

---

## 7.3 页面回收策略

### 回收算法

**内存压力分类：**

1. **kswapd回收** (后台)
   - 内存轻度压力
   - 异步进行，不阻塞进程

2. **直接回收** (同步)
   - 内存严重压力
   - 阻塞分配请求

3. **OOM Killer** (最后手段)
   - 所有回收失败
   - 杀死进程释放内存

**回收目标：**

```c
// 回收函数
unsigned long shrink_zones(int priority, struct zonelist *zonelist,
                           struct scan_control *sc)
{
    struct zoneref *z;
    struct zone *zone;
    unsigned long nr_reclaimed = 0;

    // 按优先级扫描
    for_each_zone_zonelist_nodemask(zone, z, zonelist,
                                    gfp_zone(sc->gfp_mask), NULL) {
        if (!cpuset_zone_allowed(zone, sc->gfp_mask))
            continue;

        // 扫描LRU链表
        nr_reclaimed += shrink_zone(priority, zone, sc);
    }

    return nr_reclaimed;
}
```

### 文件页vs匿名页回收

**文件页回收：**

- 直接释放，无需写入磁盘
- 干净的文件页可直接回收
- 脏的文件页需先写回

**匿名页回收：**

- 需要swap到磁盘
- 换出到swap分区/文件
- 释放后内存可用于其他用途

**回收优先级：**

```
优先回收:
1. 干净的文件页 (无任何损失)
2. 干净的映射页 (可重新读取)
3. 脏的文件页 (需要写回)
4. 匿名页 (需要swap)

不回收:
1. 锁定的页面 (PG_locked)
2. 正在IO的页面 (PG_writeback)
3. 核心内核页面
```

---

## 7.4 swap机制详解

### swap配置

**swappiness参数：**

```bash
# 查看当前值 (默认60)
cat /proc/sys/vm/swappiness

# 修改
echo 60 > /proc/sys/vm/swappiness
# 或
sysctl vm.swappiness=60
```

**参数含义：**

| 值 | 行为 |
|----|------|
| 0 | 倾向于回收文件页，尽量避免swap |
| 60 | 默认值 |
| 100 | 积极使用swap，优先回收匿名页 |

### swap读写

**swap缓存：**

```c
// swap缓存结构
struct swap_info_struct {
    unsigned int flags;          // 标志
    spinlock_t lock;             // 锁
    struct block_device *bdev;   // 块设备
    struct list_head entries;   // swap条目
    unsigned long max;           // 最大页数
    unsigned long inuse_pages;   // 已使用页数
    char *filename;              // swap文件路径
};
```

**读写流程：**

```
写入swap:
  页面被回收
    ↓
  检查是否可交换
    ↓
  分配swap entry
    ↓
  写如磁盘 (swap_writepage)
    ↓
  更新swap缓存

读入swap:
  访问交换出的页面
    ↓
  触发page fault
    ↓
  从磁盘读取 (swap_readpage)
    ↓
  恢复页面映射
```

### 监控swap使用

```bash
# 查看swap使用情况
swapon -s

# 或
cat /proc/meminfo | grep Swap

# 查看具体swap分区
fdisk -l

# 关闭所有swap
swapoff -a
```

---

## 本章小结

本章介绍了内存回收与交换机制：

1. **LRU算法**：双向链表实现，多个LRU分组
2. **kswapd**：后台回收线程，基于水位触发
3. **页面回收策略**：文件页优先，匿名页次之
4. **swap机制**：swap空间管理，换入换出流程

理解内存回收机制有助于系统性能调优和问题排查。

---

## 思考题

1. 为什么Linux将文件页和匿名页分开处理？
2. swappiness参数对系统有什么影响？
3. 如果禁用swap，对系统有什么影响？
4. kswapd和direct reclaim有什么区别？

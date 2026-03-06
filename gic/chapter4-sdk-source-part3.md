# 第四章：SDK中断框架源码解析 (下)

## 4.7 中断类型判断函数

SDK提供了一组便捷的内联函数用于判断中断类型：

```c
/**
 * @brief    Check if an interrupt is an SGI
 * @noapi
 */
static inline uint32_t gic_is_sgi(irq_id_t int_id)
{
    // SGI范围: 0-15
    return (int_id <= IRQ_SGI_MAX_INT_NUMBER) ? 1UL : 0UL;
}

/**
 * @brief    Check if an interrupt is a PPI
 * @noapi
 */
static inline uint32_t gic_is_ppi(irq_id_t int_id)
{
    // PPI范围: 16-31
    return ((int_id >= IRQ_PPI_MIN_INT_NUMBER) &&
            (int_id <= IRQ_PPI_MAX_INT_NUMBER)) ? 1UL : 0UL;
}

/**
 * @brief    Check if an interrupt is an SPI
 * @noapi
 */
static inline uint32_t gic_is_spi(irq_id_t int_id)
{
    // SPI范围: > 31
    return (int_id > IRQ_PPI_MAX_INT_NUMBER) ? 1UL : 0UL;
}
```

---

## 4.8 SGI配置函数详解

### gic_configure_sgi() - SGI中断配置

```c
/**
 * @brief    Configure an SGI interrupt
 * @noapi
 *
 * @param    int_id     interrupt id (0-15)
 * @param    group      interrupt group (0 = FIQ, 1 = IRQ)
 * @param    priority   interrupt priority
 * @param    core       core in the cluster
 */
static void gic_configure_sgi(irq_id_t int_id, irq_group_t group,
                              irq_signal_t signal, uint32_t priority,
                              uint32_t core)
{
    (void)signal;  // SGI固定为边沿触发
    GICR_t  *gicr;
    uint8_t *prio_regs;

    // 1. 获取指定核心的GICR指针
    gicr = GICR_SGI_PPI(core);

    // 2. 设置中断分组 (GICR_IGROUPR0)
    // 位操作: 保留其他位，只设置对应中断ID的位
    // group: 0 = Group 0 (FIQ), 1 = Group 1 (IRQ)
    gicr->GICR_IGROUPR0.R =
        (gicr->GICR_IGROUPR0.R & ~(1UL << (uint32_t)int_id)) |
        ((uint32_t)group << (uint32_t)int_id);

    // 3. 设置中断优先级
    // 每个GICR_IPRIORITYR寄存器存储4个中断的优先级
    // int_id >> 2: 除以4，确定使用哪个寄存器
    // int_id & 0x3: 取低2位，确定在该寄存器的哪个字节
    prio_regs = (uint8_t *)(&(gicr->GICR_IPRIORITYR[(uint32_t)int_id >> 2U]));
    // 优先级只使用高5位 (0xF8掩码)
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));
}
```

### gic_enable_sgi() - SGI中断使能

```c
/**
 * @brief    Enable an SGI interrupt
 * @noapi
 */
static void gic_enable_sgi(irq_id_t int_id, uint32_t core)
{
    GICR_t *gicr;

    gicr = GICR_SGI_PPI(core);

    // 写入1到对应位使能中断
    // ISENABLER0: Interrupt Set Enable Register 0
    gicr->GICR_ISENABLER0.R = (1UL << (uint32_t)int_id);
}
```

### gic_disable_sgi() - SGI中断禁用

```c
/**
 * @brief    Disable an SGI interrupt
 * @noapi
 */
static void gic_disable_sgi(irq_id_t int_id, uint32_t core)
{
    GICR_t *gicr;

    gicr = GICR_SGI_PPI(core);

    // 写入1到对应位禁用中断
    // ICENABLER0: Interrupt Clear Enable Register 0
    gicr->GICR_ICENABLER0.R = (1UL << (uint32_t)int_id);
}
```

---

## 4.9 PPI配置函数详解

### gic_configure_ppi() - PPI中断配置

```c
/**
 * @brief    Configure a PPI interrupt
 * @noapi
 *
 * @param    int_id     interrupt id (16-31)
 * @param    group      interrupt group (0 = FIQ, 1 = IRQ)
 * @param    signal     edge triggered or level sensitive
 * @param    priority   interrupt priority
 * @param    core       core in the cluster
 */
static void gic_configure_ppi(irq_id_t int_id, irq_group_t group,
                              irq_signal_t signal, uint32_t priority,
                              uint32_t core)
{
    uint8_t *prio_regs;
    GICR_t  *gicr;

    // 1. 获取指定核心的GICR指针
    gicr = GICR_SGI_PPI(core);

    // 2. 设置中断分组
    gicr->GICR_IGROUPR0.R =
        (gicr->GICR_IGROUPR0.R & ~(1UL << (uint32_t)int_id)) |
        ((uint32_t)group << (uint32_t)int_id);

    // 3. 设置中断优先级
    prio_regs = (uint8_t *)(&(gicr->GICR_IPRIORITYR[(uint32_t)int_id >> 2U]));
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));

    // 4. 设置触发类型 (边沿/电平)
    // ICFGR1: Interrupt Configuration Register 1 (PPI使用)
    // 每个PPI占2位: [2n+1:2n]
    // 16号中断对应位[17:16], 17号对应[19:18], 以此类推
    // 计算: (int_id - 16) * 2
    gicr->GICR_ICFGR1.R =
        (gicr->GICR_ICFGR1.R & ~(3UL << (2U * ((uint32_t)int_id - 16U)))) |
        ((uint32_t)signal << (1U + 2U * ((uint32_t)int_id - 16U)));
}
```

**触发类型配置说明:**

```c
// irq_signal_t 定义
typedef enum {
    IRQ_LEVEL_SENSITIVE = 0,  // 电平触发
    IRQ_EDGE_TRIGGERED = 2,   // 边沿触发 (注意: 值为2!)
    IRQ_SIGNAL_DONT_CARE
} irq_signal_t;

// ICFGR寄存器位定义
// bit[2n+1]: 1=边沿触发, 0=电平触发
// bit[2n]:   对于PPI, 固定为1 (0b10 = 边沿)
```

### gic_enable_ppi() / gic_disable_ppi()

```c
// 使能PPI
static void gic_enable_ppi(irq_id_t int_id, uint32_t core)
{
    GICR_t *gicr;
    gicr = GICR_SGI_PPI(core);

    // 清除pending状态 (建议在使能前清除)
    gicr->GICR_ICPENDR0.R = (1UL << (uint32_t)int_id);

    // 使能中断
    gicr->GICR_ISENABLER0.R = (1UL << (uint32_t)int_id);
}

// 禁用PPI
static void gic_disable_ppi(irq_id_t int_id, uint32_t core)
{
    GICR_t *gicr;
    gicr = GICR_SGI_PPI(core);
    gicr->GICR_ICENABLER0.R = (1UL << (uint32_t)int_id);
}
```

---

## 4.10 SPI配置函数详解

### gic_configure_spi() - SPI中断配置

```c
/**
 * @brief    Configure an SPI interrupt
 * @noapi
 */
static void gic_configure_spi(irq_id_t int_id, irq_group_t group,
                               irq_signal_t signal, uint32_t priority,
                               uint32_t core)
{
    uint8_t *prio_regs;

    // 1. 设置中断分组 (GICD_IGROUPR)
    // SPI中断使用GICD_IGROUPR数组, 每32个中断一组
    // int_id >> 5: 除以32，确定使用哪个寄存器
    // int_id & 0x1f: 取低5位，确定在该寄存器的哪个位
    GICD.GICD_IGROUPR[(uint32_t)int_id >> 5U].B.GROUP_STATUS_BIT =
        (GICD.GICD_IGROUPR[(uint32_t)int_id >> 5U].B.GROUP_STATUS_BIT &
         ~(1UL << ((uint32_t)int_id & 0x1fU))) |
        ((uint32_t)group << ((uint32_t)int_id & 0x1fU));

    // 2. 设置中断优先级
    // GICD_IPRIORITYR数组, 每4个中断一组
    prio_regs = (uint8_t *)(&(GICD.GICD_IPRIORITYR[(uint32_t)int_id >> 2U]));
    prio_regs[(uint32_t)int_id & 0x3U] = (0xF8U & ((uint8_t)priority << 3U));

    // 3. 设置目标核心 (Affinity Routing)
    // GICD_IROUTER[int_id]: 64位寄存器
    // 直接写入目标核心ID
    GICD.GICD_IROUTER[int_id].R = (uint64_t)core;

    // 4. 设置触发类型 (GICD_ICFGR)
    // SPI使用GICD_ICFGR, 每16个中断一组
    // int_id >> 4: 除以16，确定使用哪个寄存器
    // 每个中断占2位
    GICD.GICD_ICFGR[(uint32_t)int_id >> 4U].R =
        (GICD.GICD_ICFGR[(uint32_t)int_id >> 4U].R &
         ~(3UL << (2U * ((uint32_t)int_id & 0x1FU)))) |
        ((uint32_t)signal << (1U + 2U * ((uint32_t)int_id & 0xFU)));
}
```

### gic_enable_spi() / gic_disable_spi()

```c
// 使能SPI
static void gic_enable_spi(irq_id_t int_id)
{
    // GICD_ISENABLER: 每32个中断一个寄存器
    // int_id >> 5: 计算寄存器索引
    // 1UL << (int_id & 0x1F): 计算位偏移
    GICD.GICD_ISENABLER[(uint32_t)int_id >> 5UL].B.SET_ENABLE_BIT =
        (1UL << ((uint32_t)int_id & 0x1FUL));
}

// 禁用SPI
static void gic_disable_spi(irq_id_t int_id)
{
    GICD.GICD_ICENABLER[(uint32_t)int_id >> 5UL].B.CLEAR_ENABLE_BIT =
        (1UL << ((uint32_t)int_id & 0x1FUL));
}
```

---

## 4.11 统一API函数

SDK提供了统一的配置/使能/禁用接口，内部自动判断中断类型：

### irq_config() - 统一配置

```c
/**
 * @brief   Configure an interrupt
 *
 * @param   int_id     interrupt id
 * @param   group      interrupt group
 * @param   signal     interrupt signal type
 * @param   priority   interrupt priority
 */
void irq_config(irq_id_t int_id, irq_group_t group,
                irq_signal_t signal, uint32_t priority)
{
    // 自动判断中断类型并调用对应配置函数
    if (gic_is_sgi(int_id) != 0u) {
        gic_configure_sgi(int_id, group, signal, priority, get_core_id());
    } else if (gic_is_ppi(int_id) != 0u) {
        gic_configure_ppi(int_id, group, signal, priority, get_core_id());
    } else if (gic_is_spi(int_id) != 0u) {
        gic_configure_spi(int_id, group, signal, priority, get_core_id());
    }
}
```

### irq_enable() / irq_disable() - 统一使能/禁用

```c
void irq_enable(irq_id_t int_id)
{
    if (gic_is_sgi(int_id) != 0u) {
        gic_enable_sgi(int_id, get_core_id());
    } else if (gic_is_ppi(int_id) != 0u) {
        gic_enable_ppi(int_id, get_core_id());
    } else if (gic_is_spi(int_id) != 0u) {
        gic_enable_spi(int_id);
    }
}

void irq_disable(irq_id_t int_id)
{
    if (gic_is_sgi(int_id) != 0u) {
        gic_disable_sgi(int_id, get_core_id());
    } else if (gic_is_ppi(int_id) != 0u) {
        gic_disable_ppi(int_id, get_core_id());
    } else if (gic_is_spi(int_id) != 0u) {
        gic_disable_spi(int_id);
    }
}
```

---

## 4.12 GICR指针获取函数

```c
/**
 * @brief    Returns the pointer to the GICR of the specified core
 * @noapi
 */
static inline GICR_t *GICR_SGI_PPI(uint32_t core)
{
    return GICR_tags[core];
}
```

---

## 本章小结

本章下部分详细解析了中断配置函数：

1. **中断类型判断**
   - gic_is_sgi() / gic_is_ppi() / gic_is_spi()

2. **SGI配置**
   - 配置分组和优先级
   - SGI固定为边沿触发

3. **PPI配置**
   - 配置分组、优先级、触发类型
   - 使用ICFGR1寄存器

4. **SPI配置**
   - 配置分组、优先级、触发类型
   - 配置目标核心(Affinity Routing)
   - 使用ICFGR0寄存器

5. **统一API**
   - irq_config() / irq_enable() / irq_disable()
   - 自动识别中断类型

---

## 下章预告

第五章将进入实战环节：
- 如何配置一个SPI中断
- 如何配置PPI/SGI中断
- 中断优先级设置
- 完整代码示例

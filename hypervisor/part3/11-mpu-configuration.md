# 第 11 章：内存保护（MPU）配置

## 11.1 MPU 架构概述

### 11.1.1 概述

MPU（Memory Protection Unit）是 Cortex-R52 的重要特性，用于实现虚拟机之间的内存隔离。每个 VM 只能访问分配给自己的内存区域，不能访问其他 VM 或 Hypervisor 的内存。

### 11.1.2 MPU 特性

- **保护区数量**：最多支持 16 个保护区
- **区域大小**：从 4KB 到 4GB
- **访问控制**：读、写、执行权限
- **内存属性**：缓存属性、共享属性

### 11.1.3 虚拟化中的角色

在 Hypervisor 环境中，MPU 用于：

1. 隔离 Hypervisor 与 VM 的内存
2. 隔离不同 VM 之间的内存
3. 保护敏感外设地址空间

## 11.2 VM 内存隔离配置

### 11.2.1 内存布局

典型的 VM 内存布局：

```
+------------------+ 0x00000000
|   Hypervisor    |  (Reserved)
+------------------+ 0x10000000
|     VM0         |
|  - Code         |
|  - Data         |
|  - Stack        |
+------------------+ 0x10080000
|     VM1         |
|  - Code         |
|  - Data         |
|  - Stack        |
+------------------+ 0x20000000
|   Shared Mem   |
+------------------+
```

### 11.2.2 VM 内存配置结构

```c
typedef struct {
    uint32_t base_addr;    // 区域基址
    uint32_t size;         // 区域大小
    uint32_t attr;         // 属性
} vm_memory_region_t;

typedef struct {
    vm_memory_region_t code;      // 代码区域
    vm_memory_region_t data;      // 数据区域
    vm_memory_region_t stack;     // 栈区域
    vm_memory_region_t heap;      // 堆区域
    uint32_t num_regions;         // 区域数量
} vm_memory_config_t;
```

### 11.2.3 内存属性

```c
// 内存属性定义
#define MPU_REGION_VALID     (1 << 0)
#define MPU_REGION_READ      (1 << 1)
#define MPU_REGION_WRITE     (1 << 2)
#define MPU_REGION_EXECUTE   (1 << 3)
#define MPU_REGION_CACHEABLE (1 << 4)
#define MPU_REGION_SHARED    (1 << 5)

// 常用属性组合
#define MPU_ATTR_RWX    (MPU_REGION_READ | MPU_REGION_WRITE | MPU_REGION_EXECUTE)
#define MPU_ATTR_RW     (MPU_REGION_READ | MPU_REGION_WRITE)
#define MPU_ATTR_RO     (MPU_REGION_READ)
#define MPU_ATTR_XN     (MPU_REGION_EXECUTE)  // Execute Never
```

## 11.3 内存区域划分

### 11.3.1 系统内存布局

```
+------------------+ 0x00000000
|   Boot ROM     |
+------------------+ 0x00100000
|   SRAM         |
+------------------+ 0x20000000
|   Flash        |
+------------------+ 0x40000000
|   Peripherals  |
+------------------+ 0x60000000
|   External Mem  |
+------------------+
```

### 11.3.2 VM 内存分配

```c
// VM 内存池
typedef struct {
    uint32_t vm_id;
    uint32_t base_addr;
    uint32_t size;
    uint32_t used;
} vm_memory_pool_t;

// 预定义的 VM 内存池
static vm_memory_pool_t vm_pools[VM_MAX_NUMBER] = {
    {0, 0x10000000, 0x00100000},  // VM0: 1MB
    {1, 0x10100000, 0x00100000},  // VM1: 1MB
    // ...
};
```

### 11.3.3 共享内存

VM 之间可以共享内存区域：

```c
// 共享内存配置
typedef struct {
    uint32_t base_addr;
    uint32_t size;
    uint32_t vm_mask;        // 可以访问的 VM 位掩码
} shared_memory_region_t;

static shared_memory_region_t shared_regions[] = {
    {0x60000000, 0x00010000, 0x03},  // VM0 和 VM1 可访问
    {0x60010000, 0x00010000, 0x05},  // VM0 和 VM2 可访问
};
```

## 11.4 用户 MPU 配置

### 11.4.1 配置接口

参考源码：`parts/virt/st_hypervisor/demos/Hypervisor/src/user_mpu_config.c`

用户可以通过配置文件定义 VM 的 MPU 区域：

```c
// 用户 MPU 配置函数
void user_mpu_config(vm_id_t vm_id)
{
    switch (vm_id) {
    case 0:
        configure_vm0_mpu();
        break;
    case 1:
        configure_vm1_mpu();
        break;
    // ...
    }
}
```

### 11.4.2 VM0 配置示例

```c
static void configure_vm0_mpu(void)
{
    // 1. 禁用 MPU
    mpu_disable();

    // 2. 配置 VM0 代码区域 (只读可执行)
    mpu_configure_region(0,
                        0x10000000,    // Base
                        0x00020000,    // Size: 128KB
                        MPU_ATTR_RO | MPU_REGION_EXECUTE);

    // 3. 配置 VM0 数据区域 (读写)
    mpu_configure_region(1,
                        0x10020000,
                        0x00020000,
                        MPU_ATTR_RW);

    // 4. 配置 VM0 栈区域 (读写)
    mpu_configure_region(2,
                        0x10040000,
                        0x00010000,
                        MPU_ATTR_RW);

    // 5. 配置外设区域 (不可执行)
    mpu_configure_region(3,
                        0x40000000,
                        0x20000000,
                        MPU_ATTR_RW | MPU_REGION_XN);

    // 6. 使能 MPU
    mpu_enable();
}
```

### 11.4.3 MPU 配置 API

```c
// MPU 配置函数
void mpu_configure_region(uint8_t region,
                          uint32_t base_addr,
                          uint32_t size,
                          uint32_t attr)
{
    // 1. 计算区域大小编码
    uint32_t size_encoded = calculate_size_encoding(size);

    // 2. 设置基址
    MPU->RBAR = base_addr | region | MPU_REGION_VALID;

    // 3. 设置属性
    MPU->RASR = attr | size_encoded | MPU_REGION_ENABLE;
}

// 计算区域大小编码
static uint32_t calculate_size_encoding(uint32_t size)
{
    // 2^n size encoding for MPU
    int n = 0;
    while ((size >> n) > 1) {
        n++;
    }
    return ((n - 1) << 1);
}
```

## 11.5 MPU 切换

### 11.5.1 VM 切换时的 MPU 重新配置

```c
void switch_vm_mpu(vm_id_t from, vm_id_t to)
{
    // 1. 保存当前 VM 的 MPU 配置（如果需要）
    // 2. 加载新 VM 的 MPU 配置
    user_mpu_config(to);
}
```

### 11.5.2 上下文保存

```c
// 保存 VM 的 MPU 上下文
void save_mpu_context(vm_id_t vm_id, mpu_context_t *ctx)
{
    for (int i = 0; i < 16; i++) {
        ctx->rbar[i] = MPU->RBAR;
        ctx->rasr[i] = MPU->RASR;
    }
}

// 恢复 VM 的 MPU 上下文
void restore_mpu_context(vm_id_t vm_id, mpu_context_t *ctx)
{
    mpu_disable();
    for (int i = 0; i < 16; i++) {
        MPU->RBAR = ctx->rbar[i];
        MPU->RASR = ctx->rasr[i];
    }
    mpu_enable();
}
```

## 11.6 本章小结

本章详细介绍了内存保护（MPU）配置：

1. **MPU 架构**：16 个保护区的访问控制
2. **VM 内存隔离**：每个 VM 独立的内存区域
3. **内存区域划分**：代码、数据、栈、堆、外设区域
4. **用户 MPU 配置**：user_mpu_config 函数的使用
5. **MPU 切换**：VM 切换时的 MPU 重新配置

---

*参考资料：*
- `parts/virt/st_hypervisor/demos/Hypervisor/src/user_mpu_config.c`
- ARM Cortex-R52 技术参考手册

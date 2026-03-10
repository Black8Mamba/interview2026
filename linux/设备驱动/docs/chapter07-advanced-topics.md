# 第7章 进阶话题

在前几章中，我们已经深入学习了 Linux 驱动模型的核心机制，包括 kobject、总线设备驱动、平台设备、字符设备以及 I2C/SPI 总线驱动等内容。本章将进入进阶话题，探讨电源管理与驱动的关系、Linux 5.x 到 6.x 驱动模型的重要变化、内核模块签名机制，以及驱动开发中必备的调试技巧与工具。这些内容对于开发高质量、生产级别的 Linux 驱动具有重要的实践意义。

## 7.1 电源管理与驱动

电源管理是现代嵌入式系统和服务器系统中至关重要的功能。对于移动设备而言，优秀的电源管理可以显著延长电池续航时间；对于服务器而言，电源管理可以降低能耗、减少热量产生从而降低冷却成本。Linux 内核提供了完善的电源管理框架，驱动开发者需要理解并正确实现电源管理功能，以配合系统实现高效的电源利用。

### 7.1.1 PM 框架

Linux 电源管理框架（Power Management Framework）是内核中负责协调系统电源状态变化的核心子系统。它主要分为两大类：系统级电源管理（System Power Management）和运行时电源管理（Runtime Power Management）。系统级电源管理涉及整个系统的睡眠、唤醒等状态变化，而运行时电源管理则关注单个设备在运行过程中的动态电源管理。

Linux PM 框架的核心结构定义在 `include/linux/pm.h` 文件中。这个框架定义了电源管理的通用接口和数据结构，使得不同类型的设备可以采用统一的机制进行电源管理。

`struct dev_pm_ops` 是驱动电源管理操作的核心结构体，它定义了一系列回调函数用于处理设备的电源状态变化：

```c
// 文件位置：include/linux/pm.h
struct dev_pm_ops {
    // prepare 回调在系统进入睡眠状态前被调用
    // 用于准备设备进入低功耗状态
    int (*prepare)(struct device *dev);

    // complete 回调在系统唤醒完成后被调用
    // 用于恢复设备的正常运行状态
    void (*complete)(struct device *dev);

    // suspend 回调在系统进入睡眠状态时被调用
    int (*suspend)(struct device *dev);

    // resume 回调在系统唤醒时被调用
    int (*resume)(struct device *dev);

    // freeze 回调在系统休眠（hibernate）时被调用
    int (*freeze)(struct device *dev);

    // thaw 回调在系统从休眠状态恢复时被调用
    int (*thaw)(struct device *dev);

    // poweroff 回调在系统完全关机时被调用
    int (*poweroff)(struct device *dev);

    // restore 回调在系统从关机状态恢复时被调用
    int (*restore)(struct device *dev);

    // suspend_late 回调在 suspend 之后、真正进入睡眠前调用
    int (*suspend_late)(struct device *dev);

    // resume_early 回调在唤醒开始时、resume 之前调用
    int (*resume_early)(struct device *dev);

    // freeze_late 和 thaw_early 用于休眠
    int (*freeze_late)(struct device *dev);
    int (*thaw_early)(struct device *dev);

    // poweroff_late 和 restore_early 用于关机/恢复
    int (*poweroff_late)(struct device *dev);
    int (*restore_early)(struct device *dev);

    // Runtime PM 回调
    int (*runtime_suspend)(struct device *dev);
    int (*runtime_resume)(struct device *dev);
    int (*runtime_idle)(struct device *dev);
};
```

这个结构体中的回调函数按照调用顺序可以分为多组。**suspend 系列**（prepare、suspend、suspend_late）用于系统进入睡眠状态的过程，而 **resume 系列**（resume_early、resume、complete）则用于系统唤醒的过程。类似的，freeze/thaw 和 poweroff/restore 分别用于系统的休眠和关机操作。

驱动开发者需要在设备驱动中实现这些回调函数，并在设备结构体中通过 `pm` 成员关联这些操作。下面的代码展示了如何在平台设备驱动中实现电源管理：

```c
/*
 * 带电源管理功能的平台设备驱动示例
 * 文件位置：drivers/platform/my_device_pm.c
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

// 设备私有数据结构
struct my_device_data {
    struct platform_device *pdev;
    void __iomem *regs;
    int irq;
    // 其他设备特定的数据
};

// Suspend 回调：系统进入睡眠状态时调用
static int my_device_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct my_device_data *data = platform_get_drvdata(pdev);
    int ret;

    dev_info(dev, "Suspending device...\n");

    // 保存设备状态
    // 例如：保存寄存器配置、停止设备操作等
    if (data->regs) {
        // 读取当前寄存器值并保存
        // data->saved_config = readl(data->regs + CONFIG_REG);
    }

    // 禁用中断
    if (data->irq >= 0) {
        disable_irq(data->irq);
    }

    // 关闭设备时钟（如果使用 clk API）
    // clk_disable_unprepare(data->clk);

    dev_info(dev, "Device suspended successfully\n");
    return 0;
}

// Resume 回调：系统唤醒时调用
static int my_device_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct my_device_data *data = platform_get_drvdata(pdev);

    dev_info(dev, "Resuming device...\n");

    // 重新使能时钟
    // clk_prepare_enable(data->clk);

    // 恢复设备配置
    if (data->regs) {
        // writel(data->saved_config, data->regs + CONFIG_REG);
    }

    // 重新使能中断
    if (data->irq >= 0) {
        enable_irq(data->irq);
    }

    dev_info(dev, "Device resumed successfully\n");
    return 0;
}

// Runtime PM: 运行时挂起
static int my_device_runtime_suspend(struct device *dev)
{
    dev_info(dev, "Runtime suspending device...\n");

    // 关闭设备的主要功能
    // 保存上下文、停止操作等

    return 0;
}

// Runtime PM: 运行时恢复
static int my_device_runtime_resume(struct device *dev)
{
    dev_info(dev, "Runtime resuming device...\n");

    // 恢复设备到工作状态

    return 0;
}

// Runtime PM: 空闲回调
static int my_device_runtime_idle(struct device *dev)
{
    // 当设备空闲时可以进入低功耗状态
    // 使用 pm_runtime_autosuspend(dev) 自动进入挂起状态

    return 0;
}

// 定义 PM 操作结构体
static const struct dev_pm_ops my_device_pm_ops = {
    .suspend    = my_device_suspend,
    .resume     = my_device_resume,
    .freeze     = my_device_suspend,      // 休眠可以复用 suspend
    .thaw       = my_device_resume,       // 解冻可以复用 resume
    .poweroff   = my_device_suspend,      // 关机可以复用 suspend
    .restore    = my_device_resume,       // 恢复可以复用 resume
    .runtime_suspend = my_device_runtime_suspend,
    .runtime_resume  = my_device_runtime_resume,
    .runtime_idle    = my_device_runtime_idle,
};

// 平台驱动结构体
static struct platform_driver my_device_driver = {
    .driver = {
        .name = "my_device",
        .pm   = &my_device_pm_ops,    // 关联 PM 操作
        .of_match_table = my_device_of_match,
    },
    .probe  = my_device_probe,
    .remove = my_device_remove,
};

module_platform_driver(my_device_driver);
```

在这个示例中，我们定义了完整的电源管理回调函数。注意 `suspend` 和 `resume` 分别被复用到 `freeze/thaw` 和 `poweroff/restore`，这是因为对于大多数设备而言，这些操作的逻辑是相似的。当然，如果设备需要区分不同的电源状态，可以在各自的回调中实现不同的处理逻辑。

### 7.1.2 Runtime PM

Runtime PM（运行时电源管理）是 Linux 2.6.32 引入的重要特性，它允许设备在运行过程中根据实际使用情况动态地进入或退出低功耗状态。与系统睡眠（需要用户主动触发）不同，Runtime PM 是完全自动化的，基于设备的使用计数来控制电源状态。

Runtime PM 的核心思想是为每个设备维护一个引用计数（usage count）。当设备被使用时（如打开文件、进行 I/O 操作），计数增加；当设备不再被使用时，计数减少。当计数降为零时，设备可以进入空闲状态，此时 Runtime PM 框架会根据配置决定是否将设备置于低功耗状态。

Runtime PM 相关的核心 API 定义在 `include/linux/pm_runtime.h` 文件中：

```c
// 文件位置：include/linux/pm_runtime.h

// 递增设备的使用计数
static inline void pm_runtime_get(struct device *dev);

// 递减设备的使用计数
static inline void pm_runtime_put(struct device *dev);

// 递增使用计数并阻止设备挂起（即使调用 put 也不会立即挂起）
static inline void pm_runtime_get_noresume(struct device *dev);

// 递减使用计数，但允许设备挂起
static inline void pm_runtime_put_noidle(struct device *dev);

// 自动挂起：延迟后如果仍无操作则挂起设备
static inline void pm_runtime_autosuspend(struct device *dev);

// 同步等待设备完成挂起/唤醒操作
static inline int pm_runtime_barrier(struct device *dev);

// 启用设备的 Runtime PM 功能
static inline void pm_runtime_enable(struct device *dev);

// 禁用设备的 Runtime PM 功能
static inline void pm_runtime_disable(struct device *dev);

// 强制设备进入活动状态
static inline void pm_runtime_active(struct device *dev);

// 强制设备进入挂起状态
static inline void pm_runtime_suspend(struct device *dev);

// 获取设备的当前电源状态
static inline int pm_runtime_status_suspended(struct device *dev);
```

下面通过一个具体的字符设备驱动示例，展示如何在实际驱动中集成 Runtime PM：

```c
/*
 * 集成 Runtime PM 的字符设备驱动示例
 * 文件位置：drivers/char/my_char_pm.c
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>

#define DEVICE_NAME "my_pm_device"
#define CLASS_NAME  "my_pm_class"

static int major_number;
static struct class *device_class;
static struct device *device_node;
static struct cdev my_cdev;

// 设备私有数据
struct my_char_data {
    struct device *dev;
    // 设备特定的数据
};

// 打开设备文件
static int my_device_open(struct inode *inode, struct file *file)
{
    struct my_char_data *data;

    // 每次打开设备时，增加 Runtime PM 使用计数
    pm_runtime_get(file->private_data);

    // 分配私有数据
    data = kzalloc(sizeof(struct my_char_data), GFP_KERNEL);
    if (!data) {
        pm_runtime_put(file->private_data);
        return -ENOMEM;
    }

    data->dev = file->private_data;
    file->private_data = data;

    pr_info("Device opened, PM usage count incremented\n");
    return 0;
}

// 关闭设备文件
static int my_device_release(struct inode *inode, struct file *file)
{
    struct my_char_data *data = file->private_data;

    pr_info("Device closed, PM usage count will be decremented\n");

    // 释放私有数据
    kfree(data);

    // 关闭设备时，减少 Runtime PM 使用计数
    pm_runtime_put(file->private_data);

    return 0;
}

// 读取设备数据
static ssize_t my_device_read(struct file *file, char __user *buf,
                              size_t count, loff_t *offset)
{
    // 在读取数据前确保设备处于工作状态
    // Runtime PM 会自动处理设备的唤醒

    // 执行读取操作...
    pr_info("Reading from device\n");

    return 0;
}

// 写入设备数据
static ssize_t my_device_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *offset)
{
    // 在写入数据前确保设备处于工作状态
    // Runtime PM 会自动处理设备的唤醒

    // 执行写入操作...
    pr_info("Writing to device\n");

    return count;
}

// 文件操作结构体
static const struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = my_device_open,
    .release = my_device_release,
    .read    = my_device_read,
    .write   = my_device_write,
};

// Probe 函数
static int my_driver_probe(struct platform_device *pdev)
{
    struct my_char_data *data;
    dev_t dev_num;
    int ret;

    data = devm_kzalloc(&pdev->dev, sizeof(struct my_char_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    // 初始化 Runtime PM
    pm_runtime_enable(&pdev->dev);

    // 将私有数据与设备关联
    dev_set_drvdata(&pdev->dev, data);
    data->dev = &pdev->dev;

    // 注册字符设备
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate chrdev region\n");
        goto err_pm_disable;
    }

    major_number = MAJOR(dev_num);

    cdev_init(&my_cdev, &fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto err_unregister_chrdev;
    }

    // 创建设备类
    device_class = class_create(CLASS_NAME);
    if (IS_ERR(device_class)) {
        pr_err("Failed to create class\n");
        ret = PTR_ERR(device_class);
        goto err_cdev_del;
    }

    // 创建设备节点
    device_node = device_create(device_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(device_node)) {
        pr_err("Failed to create device\n");
        ret = PTR_ERR(device_node);
        goto err_class_destroy;
    }

    // 将设备节点与 PM 关联
    pm_runtime_set_active(&device_node->kobj);
    pm_runtime_enable(&device_node->kobj);

    pr_info("Device probe completed\n");
    return 0;

err_class_destroy:
    class_destroy(device_class);
err_cdev_del:
    cdev_del(&my_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
err_pm_disable:
    pm_runtime_disable(&device_node->kobj);

    return ret;
}

// Remove 函数
static int my_driver_remove(struct platform_device *pdev)
{
    dev_t dev_num = MKDEV(major_number, 0);

    // 禁用 Runtime PM
    pm_runtime_disable(&device_node->kobj);

    // 销毁设备
    device_destroy(device_class, dev_num);
    class_destroy(device_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);

    pm_runtime_disable(&device_node->kobj);

    pr_info("Device removed\n");
    return 0;
}

// PM 操作结构体
static const struct dev_pm_ops my_driver_pm_ops = {
    .runtime_suspend = my_device_runtime_suspend,
    .runtime_resume  = my_device_runtime_resume,
    .runtime_idle    = my_device_runtime_idle,
};

static struct platform_driver my_platform_driver = {
    .driver = {
        .name = "my_pm_device",
        .pm   = &my_driver_pm_ops,
    },
    .probe  = my_driver_probe,
    .remove = my_driver_remove,
};

module_platform_driver(my_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example");
MODULE_DESCRIPTION("Runtime PM Demo Driver");
```

Runtime PM 的工作流程可以总结为以下几个要点：首先，驱动需要在设备初始化时调用 `pm_runtime_enable()` 启用 Runtime PM 功能；其次，在设备被使用时（通常是 file_operations 的 open 调用），调用 `pm_runtime_get()` 增加使用计数；然后，在设备不再使用时（file_operations 的 release 调用），调用 `pm_runtime_put()` 减少使用计数；最后，当计数归零且设备空闲时，Runtime PM 框架会调用驱动的 `runtime_suspend` 回调将设备置于低功耗状态。

### 7.1.3 系统睡眠

系统睡眠（System Sleep）是电源管理中的重要概念，它允许系统在不使用时进入低功耗状态，从而显著降低能耗。Linux 支持多种系统睡眠状态，从浅睡眠到深睡眠不等。理解这些睡眠状态以及驱动在其中扮演的角色，对于开发可靠的嵌入式系统至关重要。

Linux 系统睡眠状态主要分为以下几类：

**Suspend to Idle（S0i0）**：这是最浅的睡眠状态，CPU 进入空闲状态但保持内存供电。设备可以进入低功耗状态，但系统整体状态基本保持。

**Standby（S1）**：浅睡眠状态，CPU 暂停执行但保持内存供电。唤醒速度快，但节能效果有限。

**Suspend to RAM（S3）**：内存自刷新模式，系统状态保存在内存中，内存保持供电以维护数据，其他设备断电。这是笔记本电脑最常用的睡眠状态。

**Hibernate（S4）**：休眠状态，系统状态保存到磁盘（swap 分区），所有设备断电。唤醒后系统完全恢复，但唤醒时间较长。

**Power Off（S5）**：完全关机状态。

系统睡眠的过程涉及多个阶段，以 Suspend to RAM 为例：

1. **用户空间准备**：系统通知所有用户空间进程准备睡眠，进程可以执行必要的清理工作。

2. **Freeze 阶段**：系统冻结所有用户空间进程和内核线程。

3. **Suspend 阶段**：调用各设备的 `suspend` 回调，设备进入低功耗状态。

4. **Suspend Late 阶段**：最后的设备挂起操作，此时系统已经非常接近睡眠状态。

5. **Syscore Suspend**：核心系统组件（如时钟、IRQ）的挂起。

6. **进入睡眠**：CPU 进入低功耗模式。

唤醒过程则是上述的逆序。驱动开发者需要确保 `suspend` 和 `resume` 回调正确实现，以使设备在睡眠/唤醒过程中正常工作。

下面的代码展示了如何在 I2C 传感器驱动中实现完整的系统睡眠支持：

```c
/*
 * 带系统睡眠支持的 I2C 传感器驱动
 * 文件位置：drivers/i2c/bus/sensor_with_sleep.c
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

// 设备私有数据
struct sensor_data {
    struct i2c_client *client;
    struct device *dev;
    u8 reg_cache[16];        // 寄存器缓存
    bool cache_valid;
    // 其他成员
};

// 定义 PM 操作结构体
static const struct dev_pm_ops sensor_pm_ops = {
    // 系统睡眠回调
    .suspend  = sensor_suspend,
    .resume   = sensor_resume,
    .freeze   = sensor_freeze,
    .thaw     = sensor_thaw,
    .poweroff = sensor_poweroff,
    .restore  = sensor_restore,

    // Runtime PM 回调
    .runtime_suspend = sensor_runtime_suspend,
    .runtime_resume  = sensor_runtime_resume,
};

// 系统睡眠：Suspend 回调
static int sensor_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sensor_data *data = i2c_get_clientdata(client);
    int ret;

    dev_info(dev, "Suspending sensor...\n");

    // 停止传感器测量
    // ret = sensor_stop_measurement(data);
    // if (ret < 0)
    //     dev_warn(dev, "Failed to stop measurement: %d\n", ret);

    // 保存当前配置到缓存
    // ret = sensor_save_config(data);
    // if (ret < 0)
    //     dev_warn(dev, "Failed to save config: %d\n", ret);

    // 禁用设备中断
    // if (data->irq > 0)
    //     disable_irq(data->irq);

    // 设置设备进入低功耗模式
    // sensor_set_low_power_mode(data);

    dev_info(dev, "Sensor suspended\n");
    return 0;
}

// 系统唤醒：Resume 回调
static int sensor_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sensor_data *data = i2c_get_clientdata(client);

    dev_info(dev, "Resuming sensor...\n");

    // 恢复设备配置
    // sensor_restore_config(data);

    // 重新使能设备中断
    // if (data->irq > 0)
    //     enable_irq(data->irq);

    // 重新启动传感器测量
    // sensor_start_measurement(data);

    dev_info(dev, "Sensor resumed\n");
    return 0;
}

// 休眠：Freeze 回调
static int sensor_freeze(struct device *dev)
{
    // 对于 Hibernate，我们只需要保存关键状态
    // 因为内存会被保存到磁盘
    dev_info(dev, "Freezing sensor for hibernate...\n");

    // 保存寄存器状态
    // 与 suspend 类似，但可以更简洁

    return sensor_suspend(dev);
}

// 解冻：Thaw 回调
static int sensor_thaw(struct device *dev)
{
    dev_info(dev, "Thawing sensor from hibernate...\n");

    // 恢复状态
    return sensor_resume(dev);
}

// 关机：Poweroff 回调
static int sensor_poweroff(struct device *dev)
{
    // Poweroff 与 suspend 类似
    dev_info(dev, "Powering off sensor...\n");
    return sensor_suspend(dev);
}

// 恢复：Restore 回调
static int sensor_restore(struct device *dev)
{
    dev_info(dev, "Restoring sensor from poweroff...\n");
    return sensor_resume(dev);
}

// Runtime PM: 运行时挂起
static int sensor_runtime_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sensor_data *data = i2c_get_clientdata(client);

    dev_vdbg(dev, "Runtime suspending sensor...\n");

    // 减少功耗：停止测量、关闭时钟等
    // 注意：不要保存寄存器值，因为这不是真正的睡眠

    return 0;
}

// Runtime PM: 运行时恢复
static int sensor_runtime_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct sensor_data *data = i2c_get_clientdata(client);

    dev_vdbg(dev, "Runtime resuming sensor...\n");

    // 恢复设备到工作状态
    // 可能需要重新初始化设备

    return 0;
}

// I2C 驱动定义
static struct i2c_driver sensor_driver = {
    .driver = {
        .name = "sensor_with_sleep",
        .pm   = &sensor_pm_ops,
        .of_match_table = sensor_of_match,
    },
    .probe    = sensor_probe,
    .remove   = sensor_remove,
    .id_table = sensor_id_table,
};

module_i2c_driver(sensor_driver);
```

在实际开发中，驱动开发者需要根据设备的具体特性来决定在各个回调中执行什么操作。以下是一些通用的指导原则：

对于 **suspend** 回调，应该停止设备的所有活动、保存必要的设备状态、禁用中断、关闭设备时钟（如果有）。对于 **resume** 回调，应该恢复设备配置、重新使能中断、重新启动设备活动。对于 **freeze/thaw** 和 **poweroff/restore**，如果设备的处理逻辑与 suspend/resume 相同，可以直接复用。

## 7.2 Linux 5.x 到 6.x 驱动模型变化

Linux 内核在 5.x 到 6.x 的演进过程中，对驱动模型进行了多项改进。这些变化旨在提高代码质量、增强安全性和简化驱动开发。本节将介绍与驱动开发密切相关的主要变化，帮助开发者编写兼容新版内核的驱动代码。

### 7.2.1 主要变化

从 Linux 5.x 到 6.x，驱动模型领域发生了多个重要变化。这些变化涉及设备链接（device link）、总线类型（bus_type）以及设备属性（device attribute）等方面。

**Device Link 的改进**

Device Link 是用于表示两个设备之间依赖关系的结构体。在 Linux 6.x 中，device_link 的 API 和功能得到了显著增强。

Linux 5.x 中的 device_link 主要用于表示父子设备之间的电源依赖关系。驱动可以通过 `device_link_add()` 创建设备链接，并通过 `device_link_remove()` 删除链接。链接有两种类型：`DL_FLAG_STATELESS`（无状态）和带有电源管理标志的链接。

Linux 6.x 中引入了更多的设备链接标志和改进的 API：

```c
// Linux 6.x 中新增的设备链接标志
#define DL_FLAG_PM_RUNTIME     BIT(7)   // 运行时 PM 标志
#define DL_FLAG_RPM_ACTIVE     BIT(8)   // 主动模式标志

// Linux 6.x 中改进的 API
struct device_link *device_link_add(struct device *consumer,
                                    struct device *supplier,
                                    u32 flags);

void device_link_remove(struct device *consumer,
                        struct device *supplier);

// 新的辅助函数
int device_link_suspend(struct device *link, bool synchronous);
int device_link_resume(struct device *link, bool synchronous);
```

这些改进使得驱动可以更精细地控制设备之间的依赖关系，特别是在复杂的外设系统中。

**Bus Type 的变化**

总线类型（bus_type）在 Linux 6.x 中也经历了变化。总线是设备与驱动之间的桥梁，它定义了设备与驱动的匹配方式以及总线特定的操作。

```c
// Linux 6.x 中 bus_type 的变化
struct bus_type {
    const char *name;
    const char *dev_name;
    struct device *dev_root;

    // 匹配函数
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);

    // Linux 6.x 新增：探针前检查函数
    int (*probe_check)(struct device *dev);

    // ... 其他成员
};
```

Linux 6.x 引入了 `probe_check` 回调，这个函数在 probe 之前被调用，允许总线在驱动绑定前进行额外的检查。例如，某些总线可能需要验证设备是否支持特定的驱动版本。

**设备属性的变化**

设备属性（device_attribute）是驱动向用户空间暴露信息的主要方式。Linux 6.x 对设备属性的创建和使用方式进行了一些改进。

```c
// 传统的设备属性定义方式
static DEVICE_ATTR(name, mode, show, store);

// Linux 6.x 推荐的方式：使用 DEVICE_ATTR_RO 和 DEVICE_ATTR_WO
static DEVICE_ATTR_RO(name);   // 只读属性
static DEVICE_ATTR_WO(name);   // 只写属性
static DEVICE_ATTR(name, mode, show, store);  // 读写属性

// 属性组
static struct attribute *my_attrs[] = {
    &dev_attr_my_attr.attr,
    NULL,
};

static const struct attribute_group my_attr_group = {
    .attrs = my_attrs,
};

// Linux 6.x 新增：属性组数组可以直接在驱动中指定
static const struct attribute_group *my_attr_groups[] = {
    my_attr_group,
    NULL,
};
```

设备属性的一个重要变化是在 Linux 6.x 中，`device_create()` 和 `device_create_file()` 等函数的错误处理变得更加严格。如果驱动试图创建一个已存在的属性，内核会发出警告。

**Driver 核心的变化**

设备驱动的核心结构体 `device_driver` 在 Linux 6.x 中也有一些值得注意的变化：

```c
struct device_driver {
    const char *name;
    struct bus_type *bus;

    // Linux 6.x 中增强了 probe 函数的返回值说明
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);

    void (*shutdown)(struct device *dev);
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    // Linux 6.x 新增：软绑定回调
    int (*soft_bind)(struct device *dev);
    void (*soft_unbind)(struct device *dev);

    const struct attribute_group **groups;
    const struct dev_pm_ops *pm;

    struct driver_private *p;
    // ...
};
```

`soft_bind` 和 `soft_unbind` 是 Linux 6.x 引入的新回调函数。它们与普通的 bind/unbind 类似，但不会真正将驱动绑定到设备上。这个功能主要用于驱动的加载测试和调试场景。

**Device 核心的变化**

设备结构体 `device` 也有一些变化：

```c
struct device {
    struct device *parent;
    struct device_private *p;
    struct kobject kobj;
    const char *init_name;

    // 设备类型
    const struct device_type *type;

    // 总线和驱动信息
    struct bus_type *bus;
    struct device_driver *driver;

    // 设备节点
    void *platform_data;
    void *driver_data;
    struct device_node *of_node;
    struct fwnode_handle *fwnode;

    // Linux 6.x 新增：设备链接
    struct device_link *link;

    // 电源管理
    struct dev_pm_info power;
    struct dev_pm_domain *pm_domain;

    // ...
};
```

Linux 6.x 增加了对 `device_link` 的原生支持，设备结构体现在直接包含一个指向设备链接的指针。这简化了设备依赖关系的管理。

**兼容性与迁移建议**

对于从 Linux 5.x 迁移到 6.x 的驱动代码，开发者需要注意以下几点：

首先，确保正确初始化所有新增的设备结构体成员。虽然大多数新成员都有默认值，但显式初始化可以避免潜在的问题。

其次，注意错误返回值的语义变化。在某些情况下，Linux 6.x 的 probe 函数对错误的处理更加严格。

第三，及时更新驱动代码以使用新的 API。例如，使用 `DEVICE_ATTR_RO()` 和 `DEVICE_ATTR_WO()` 宏代替传统的 `DEVICE_ATTR()`。

第四，在使用 device_link 时，确保理解链接的生命周期。Linux 6.x 对设备链接的引用计数管理更加严格。

## 7.3 内核模块签名与安全

内核模块签名是 Linux 安全机制的重要组成部分，它可以防止未经授权的模块被加载到内核中。在生产系统和安全敏感的应用中，启用模块签名验证可以显著提高系统的安全性。本节将详细介绍 Linux 内核模块签名机制的原理和实现方法。

### 7.3.1 模块签名机制

Linux 内核模块签名机制的核心思想是为每个模块生成一个数字签名，这个签名由内核的私钥生成，内核在加载模块时会验证签名的有效性。只有签名验证通过的模块才能被加载到内核中。

模块签名机制涉及以下几个关键概念：

**签名密钥**：签名过程使用的非对称密钥对，包括私钥（用于签名）和公钥（内置于内核中用于验证）。私钥应当妥善保管，而公钥会被编译进内核镜像。

**签名证书**：签名密钥的 X.509 证书格式，包含公钥和密钥持有者信息。

**模块签名格式**：签名信息以特定格式附加在 ELF 可执行文件末尾，包含签名算法标识、签名数据和时间戳等信息。

模块签名的工作流程如下：

1. **密钥生成**：使用 OpenSSL 或其他工具生成签名密钥对。
2. **内核配置**：在内核编译时启用模块签名支持，配置公钥。
3. **模块签名**：在模块编译时，使用私钥对模块进行签名。
4. **模块加载**：内核在加载模块时验证签名，无效签名将被拒绝。

### 7.3.2 CONFIG_MODULE_SIG

`CONFIG_MODULE_SIG` 是 Linux 内核中控制模块签名机制的核心配置选项。它有多个可选值，不同的值决定了不同的签名验证策略。

```bash
# 内核配置选项（make menuconfig / make nconfig）
# Location: -> Enable loadable module support
#                   -> Module signature verification

CONFIG_MODULE_SIG=y           # 启用模块签名验证
CONFIG_MODULE_SIG_FORCE=y    # 强制要求签名，无签名模块将被拒绝
CONFIG_MODULE_SIG_ALL=y      # 对所有模块进行签名
CONFIG_MODULE_SIG_KEY="certs/signing_key.pem"  # 指定签名密钥文件
CONFIG_MODULE_SIG_HASH_TYPE=sha256  # 签名哈希算法
```

**CONFIG_MODULE_SIG** 有以下几个可选配置：

- **不启用（n）**：完全禁用模块签名验证，任何模块都可以加载。

- **启用（y）**：启用签名验证，但允许加载未签名的模块（仅进行日志警告）。

- **强制启用（y with CONFIG_MODULE_SIG_FORCE）**：强制要求所有模块都有有效签名，拒绝加载未签名或签名无效的模块。

**签名密钥的生成与管理**

在内核源码目录下，可以使用以下命令生成签名密钥：

```bash
# 进入内核源码目录
cd /path/to/kernel/source

# 使用内核提供的脚本生成签名密钥
# 这会在 certs/ 目录下创建 signing_key.pem（私钥）和 signing_key.x509（证书）
make -C certs/signing_key.pl x509

# 或者使用更简单的方式，在首次编译模块时自动生成
make modules
```

生成的密钥文件应当妥善保管：

- `certs/signing_key.pem`：私钥，必须保密。一旦泄露，他人可以签名任意模块。

- `certs/signing_key.x509`：公钥证书，会被内置到内核镜像中。

**手动签名模块**

如果需要为特定模块签名，可以使用 `sign-file` 工具：

```bash
# 语法：sign-file <hash_algo> <key_file> <x509_file> <module_file>
# 示例：使用 SHA256 哈希算法对模块进行签名
sign-file sha256 signing_key.pem signing_key.x509 my_module.ko

# 签名后可以验证签名
# 使用 GPG 或 openssl 查看签名信息
```

**模块签名验证**

在已加载模块签名支持的内核中，可以查看模块的签名状态：

```bash
# 查看模块签名信息
modinfo my_module.ko

# 输出中会包含类似以下行：
# signer:     Build time autogenerated kernel key
# signature:  RSA-PSS, key id abcd1234
# dave:       Modular_Core

# 检查系统是否强制要求签名
cat /proc/cmdline | grep module.sig

# 查看内核模块签名配置
cat /proc/config.gz | gunzip | grep MODULE_SIG
```

**在内核模块中检查签名状态**

驱动代码可以通过以下方式检查模块签名状态：

```c
#include <linux/module.h>
#include <linux/verification.h>

// 检查模块签名状态
static int check_module_signature(void)
{
    struct module *mod = THIS_MODULE;

    if (mod->sig_ok) {
        pr_info("Module signature is valid\n");
        return 0;
    } else {
        pr_warn("Module signature is invalid or not present\n");
        return -ENOKEY;
    }
}

// 模块初始化时检查
static int __init my_driver_init(void)
{
    int ret;

    ret = check_module_signature();
    if (ret < 0) {
        pr_err("Module signature check failed\n");
        return ret;
    }

    // 继续驱动初始化...
    return 0;
}
```

**模块签名的实际应用**

在企业级部署中，模块签名是确保系统安全的关键措施：

```bash
# 1. 生成企业级签名密钥（离线环境）
openssl genrsa -out enterprise_signing_key.pem 4096

# 2. 生成自签名证书
openssl req -new -x509 -key enterprise_signing_key.pem \
    -out enterprise_signing_key.x509 \
    -days 3650 \
    -subj "/CN=Enterprise Kernel Module Signing/OU=Security/O=Company/"

# 3. 将证书编译进内核
# 在内核配置中指定：
# CONFIG_MODULE_SIG_KEY="certs/enterprise_signing_key.pem"
# CONFIG_SYSTEM_TRUSTED_KEYRING=y
# 将证书添加到 system keyring

# 4. 使用企业密钥签名模块
sign-file sha256 enterprise_signing_key.pem \
    enterprise_signing_key.x509 my_driver.ko

# 5. 分发签名后的模块
# 只能分发 .ko 文件，不要分发私钥
```

**无签名模块的处理**

在开发和测试阶段，可能需要绕过签名验证。以下是一些方法（仅用于开发环境）：

```bash
# 方法1：禁用内核的强制签名要求
# 在内核启动参数中添加：
module.sig_enforce=0

# 方法2：临时禁用签名检查（不推荐用于生产环境）
# 修改内核配置，重新编译

# 方法3：使用 mokutil 管理签名密钥（UEFI Secure Boot 环境）
# 将自定义密钥注册到 MOK (Machine Owner Key)
mokutil --import enterprise_signing_key.x509
```

**模块签名与 Secure Boot**

在启用 UEFI Secure Boot 的系统上，模块签名尤为重要。Secure Boot 要求所有在启动时加载的代码都必须有有效签名，而模块签名则扩展了这个要求到可加载模块。

```bash
# 检查 Secure Boot 状态
mokutil --sb-state

# 如果 Secure Boot 已启用，必须使用已注册的密钥签名模块
# 否则模块加载会被拒绝

# 查看已注册的 MOK 密钥
mokutil --list-enrolled
```

理解模块签名机制对于开发生产级别的 Linux 驱动非常重要。正确的签名策略可以有效防止恶意模块的加载，保护系统安全。

## 7.4 调试技巧与工具

驱动开发与其他软件开发一样，免不了需要进行调试。与用户空间程序不同，内核驱动的调试有其特殊性：不能使用传统的调试器直接调试内核代码，错误往往会导致系统崩溃。本节将介绍在 Linux 驱动开发中常用的调试技巧和工具，帮助开发者快速定位和解决问题。

### 7.4.1 sysfs 调试接口系统性方法

sysfs 是 Linux 内核向用户空间暴露设备信息和控制接口的重要机制。通过 sysfs，用户可以查看设备状态、修改设备参数、触发特定操作。掌握 sysfs 的系统性调试方法，是驱动开发者必备的技能。

**查看设备信息**

sysfs 中的设备信息通常按照总线和层级结构组织。以下是常用的查看命令：

```bash
# 查看系统中的所有设备（按总线分类）
ls -la /sys/bus/

# 查看特定总线上的设备
ls -la /sys/bus/i2c/devices/
ls -la /sys/bus/spi/devices/
ls -la /sys/bus/platform/devices/

# 查看设备详细信息
# 例如，查看 I2C 设备
ls -la /sys/bus/i2c/devices/i2c-1/
cat /sys/bus/i2c/devices/i2c-1/name

# 查看设备属性
# 列出设备的所有属性
ls -la /sys/devices/platform/my_device/

# 读取设备的 uevent 信息（包含设备的热插拔事件数据）
cat /sys/devices/platform/my_device/uevent

# 读取设备驱动信息
cat /sys/devices/platform/my_device/driver

# 查看设备树信息（如果使用设备树）
cat /sys/firmware/devicetree/base/my_device@address/compatible
```

**控制设备状态**

通过 sysfs 可以直接控制设备的状态，这是调试驱动问题非常有用的方法：

```bash
# 查看设备的电源状态
cat /sys/devices/platform/my_device/power/control
cat /sys/devices/platform/my_device/power/runtime_status

# 控制设备的运行时电源管理
# auto：启用运行时 PM
# on：禁用运行时 PM，保持设备开启
echo auto > /sys/devices/platform/my_device/power/control
echo on > /sys/devices/platform/my_device/power/control

# 触发设备重新探测
echo 1 > /sys/bus/platform/drivers/my_driver/unbind
echo 1 > /sys/bus/platform/drivers/my_driver/bind

# 查看设备链接
ls -la /sys/devices/platform/my_device/links/
cat /sys/devices/platform/my_device/links/consumer
cat /sys/devices/platform/my_device/links/supplier
```

**调试参数**

驱动通常会在 sysfs 中暴露可配置的参数：

```bash
# 查看驱动参数
ls -la /sys/module/my_driver/parameters/

# 读取参数值
cat /sys/module/my_driver/parameters/debug_level
cat /sys/module/my_driver/parameters/enable_feature

# 修改参数值（如果参数是可写的）
echo 3 > /sys/module/my_driver/parameters/debug_level

# 查看模块信息
cat /sys/module/my_driver/version
cat /sys/module/my_driver/srcversion
cat /sys/module/my_driver/taint_flags
```

下面的代码展示了如何在驱动中创建有用的 sysfs 调试接口：

```c
/*
 * sysfs 调试接口示例
 * 文件位置：drivers/platform/my_debug_sysfs.c
 */

#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/string.h>

// 驱动私有数据
struct my_driver_data {
    int debug_level;
    bool enable_feature;
    int counter;
    struct kobject *kobj;
};

// debug_level 属性
static ssize_t debug_level_show(struct kobject *kobj,
                                 struct kobj_attribute *attr,
                                 char *buf)
{
    struct my_driver_data *data = container_of(kobj, struct my_driver_data, kobj);
    return sprintf(buf, "%d\n", data->debug_level);
}

static ssize_t debug_level_store(struct kobject *kobj,
                                  struct kobj_attribute *attr,
                                  const char *buf, size_t count)
{
    struct my_driver_data *data = container_of(kobj, struct my_driver_data, kobj);
    int ret;

    ret = kstrtoint(buf, 10, &data->debug_level);
    if (ret < 0)
        return ret;

    pr_info("Debug level set to %d\n", data->debug_level);
    return count;
}

// enable_feature 属性
static ssize_t enable_feature_show(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     char *buf)
{
    struct my_driver_data *data = container_of(kobj, struct my_driver_data, kobj);
    return sprintf(buf, "%d\n", data->enable_feature);
}

static ssize_t enable_feature_store(struct kobject *kobj,
                                     struct kobj_attribute *attr,
                                     const char *buf, size_t count)
{
    struct my_driver_data *data = container_of(kobj, struct my_driver_data, kobj);

    if (sysfs_streq(buf, "1") || sysfs_streq(buf, "true"))
        data->enable_feature = true;
    else if (sysfs_streq(buf, "0") || sysfs_streq(buf, "false"))
        data->enable_feature = false;
    else
        return -EINVAL;

    pr_info("Feature %s\n", data->enable_feature ? "enabled" : "disabled");
    return count;
}

// 只读属性：counter
static ssize_t counter_show(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             char *buf)
{
    struct my_driver_data *data = container_of(kobj, struct my_driver_data, kobj);
    return sprintf(buf, "%d\n", data->counter);
}

// 定义属性
static struct kobj_attribute debug_level_attr =
    __ATTR(debug_level, 0644, debug_level_show, debug_level_store);

static struct kobj_attribute enable_feature_attr =
    __ATTR(enable_feature, 0644, enable_feature_show, enable_feature_store);

static struct kobj_attribute counter_attr =
    __ATTR_RO(counter);

// 属性组
static struct attribute *my_attrs[] = {
    &debug_level_attr.attr,
    &enable_feature_attr.attr,
    &counter_attr.attr,
    NULL,
};

static struct attribute_group my_attr_group = {
    .name = "debug",      // 创建 debug 子目录
    .attrs = my_attrs,
};

// 创建 sysfs 接口
int my_create_sysfs(struct device *dev)
{
    struct my_driver_data *data = dev_get_drvdata(dev);
    int ret;

    // 在设备目录下创建属性组
    ret = sysfs_create_group(&dev->kobj, &my_attr_group);
    if (ret)
        dev_err(dev, "Failed to create sysfs group: %d\n", ret);

    return ret;
}

// 销毁 sysfs 接口
void my_remove_sysfs(struct device *dev)
{
    sysfs_remove_group(&dev->kobj, &my_attr_group);
}
```

### 7.4.2 uevent 事件监控与调试

uevent 是 Linux 内核与用户空间进行热插拔通信的机制。当设备被添加、移除或发生其他事件时，内核会通过 netlink 套接字向用户空间发送 uevent 消息。监控 uevent 对于调试热插拔问题、理解设备枚举过程非常有帮助。

**监控 uevent 事件**

```bash
# 使用 udevadm 监控 uevent 事件（实时监控）
udevadm monitor

# 只监控设备添加事件
udevadm monitor --subsystem-match=block

# 只监控设备移除事件
udevadm monitor --action=remove

# 监控特定设备的 uevent
udevadm monitor --device-match=/dev/sda

# 详细模式，显示完整的环境变量
udevadm monitor -p
```

**查看设备 uevent 信息**

每个设备在 sysfs 中都有一个 `uevent` 文件，包含设备的热插拔事件数据：

```bash
# 查看块设备的 uevent
cat /sys/block/sda/uevent

# 输出示例：
# MAJOR=8
# MINOR=0
# DEVNAME=sda
# DEVTYPE=disk
# SUBSYSTEM=block

# 查看网络设备的 uevent
cat /sys/class/net/eth0/uevent

# 查看平台设备的 uevent
cat /sys/devices/platform/serial8250/tty/ttyUSB0/uevent

# 查看 USB 设备的 uevent
ls -la /sys/bus/usb/devices/
cat /sys/bus/usb/devices/1-1:1.0/uevent
```

**分析 uevent 调试问题**

当设备未能正确枚举时，检查 uevent 信息是定位问题的关键：

```bash
# 1. 查看设备是否出现在 sysfs 中
ls -la /sys/bus/platform/devices/ | grep my_device

# 2. 如果设备存在，检查 uevent 是否有问题
cat /sys/bus/platform/devices/my_device/uevent

# 3. 检查驱动是否绑定到设备
cat /sys/bus/platform/devices/my_device/driver

# 4. 手动触发 uevent（需要 root 权限）
echo add > /sys/bus/platform/devices/my_device/uevent

# 5. 检查 dmesg 中的相关消息
dmesg | grep -i "my_device\|uevent\|hotplug"

# 6. 使用 udevadm info 查看完整的设备信息
udevadm info /dev/my_device
udevadm info -a -p /sys/class/my_class/my_device
```

**在驱动代码中发送 uevent**

驱动可以在特定事件发生时主动发送 uevent 通知用户空间：

```c
#include <linux/device.h>
#include <linux/kobject.h>

// 在设备结构体中定义
struct my_device {
    struct device dev;
    int status;
};

// 发送设备状态变化的 uevent
static void my_device_send_uevent(struct my_device *mdev, const char *event)
{
    char *envp[2] = { NULL, NULL };
    char status_str[32];

    snprintf(status_str, sizeof(status_str), "STATUS=%d", mdev->status);
    envp[0] = status_str;

    kobject_uevent_env(&mdev->dev.kobj, KOBJ_CHANGE, envp);
}

// 在设备探测成功时发送
static int my_device_probe(struct platform_device *pdev)
{
    struct my_device *mdev;

    // ... 设备初始化 ...

    // 发送 uevent 通知用户空间
    kobject_uevent(&mdev->dev.kobj, KOBJ_ADD);

    return 0;
}

// 在设备发生错误时发送
static void my_device_error(struct my_device *mdev, int error)
{
    char error_str[32];

    snprintf(error_str, sizeof(error_str), "ERROR=%d", error);
    kobject_uevent_env(&mdev->dev.kobj, KOBJ_CHANGE, &error_str);
}
```

### 7.4.3 内核调试工具

Linux 内核提供了多种调试工具，帮助开发者定位驱动问题。本节介绍最常用的几种：dynamic_debug、tracepoint 和 ftrace。

**dynamic_debug**

dynamic_debug 是内核提供的运行时调试消息控制机制。它允许在不重新编译内核或模块的情况下，动态地启用或禁用调试消息。

```bash
# 查看 dynamic_debug 的帮助信息
cat /proc/dynamic_debug/control

# 启用特定文件的调试消息
echo "file my_driver.c +p" > /proc/dynamic_debug/control

# 启用特定函数的调试消息
echo "func my_function +p" > /proc/dynamic_debug/control

# 启用特定行的调试消息
echo "line 123 +p" > /proc/dynamic_debug/control

# 启用特定模块的调试消息
echo "module my_driver +p" > /proc/dynamic_debug/control

# 使用通配符
echo "file drivers/platform/* +p" > /proc/dynamic_debug/control
echo "func my_* +p" > /proc/dynamic_debug/control

# 禁用调试消息
echo "file my_driver.c -p" > /proc/dynamic_debug/control
```

在驱动代码中使用 dynamic_debug：

```c
#include <linux/dynamic_debug.h>

// 定义调试消息
// pr_debug() 消息默认不显示，需要通过 dynamic_debug 启用
static void my_function(struct my_device *dev)
{
    pr_debug("Entering my_function\n");

    // 使用动态调试宏
    ddrm_printf(dev->dev, "Device state: %d\n", dev->state);
}

// 或者在代码中直接使用
if (dev->debug)
    dev_info(dev->dev, "Debug info: ...\n");
```

**tracepoint**

tracepoint 是内核静态跟踪点机制，它在内核代码的关键位置插入探测点，可以在不修改内核代码的情况下收集运行时信息。

```bash
# 查看可用的 tracepoint
ls /sys/kernel/debug/tracing/events/

# 查看特定子系统的 tracepoint
ls /sys/kernel/debug/tracing/events/i2c/
ls /sys/kernel/debug/tracing/events/spi/
ls /sys/kernel/debug/tracing/events/block/

# 启用 tracepoint
echo 1 > /sys/kernel/debug/tracing/events/i2c/i2c_probe/enable

# 查看跟踪输出
cat /sys/kernel/debug/tracing/trace_pipe

# 设置过滤器
echo "address=0x12345678" > /sys/kernel/debug/tracing/events/i2c/i2c_probe/filter
```

在驱动中使用 tracepoint：

```c
#include <linux/tracepoint.h>

// 定义 tracepoint（在驱动源文件中）
#define CREATE_TRACE_POINTS
#include "my_driver_trace.h"

// 头文件 my_driver_trace.h 内容：
/*
 * #undef TRACE_SYSTEM
 * #define TRACE_SYSTEM my_driver
 *
 * TRACE_EVENT(my_driver_event,
 *     TP_PROTO(int status),
 *     TP_ARGS(status),
 *     TP_STRUCT__entry(
 *         __field(int, status)
 *     ),
 *     TP_fast_assign(
 *         __entry->status = status;
 *     ),
 *     TP_printk("status=%d", __entry->status)
 * );
 */

// 使用 tracepoint
trace_my_driver_event(dev->status);
```

**ftrace**

ftrace（Function Tracer）是 Linux 内核最强大的跟踪工具之一。它可以跟踪函数的调用过程、测量函数执行时间、分析性能问题。

```bash
# 查看 ftrace 的可用功能
cat /sys/kernel/debug/tracing/available_tracers

# 常用的跟踪器：
# function - 跟踪所有函数调用
# function_graph - 跟踪函数调用图
# latency - 跟踪中断/调度延迟
# blk - 跟踪块设备 I/O

# 设置跟踪器
echo function > /sys/kernel/debug/tracing/current_tracer

# 设置跟踪过滤器（只跟踪特定函数）
echo my_driver_* > /sys/kernel/debug/tracing/set_ftrace_filter

# 开始跟踪
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 查看跟踪结果
cat /sys/kernel/debug/tracing/trace

# 停止跟踪
echo 0 > /sys/kernel/debug/tracing/tracing_on

# 清除跟踪数据
echo > /sys/kernel/debug/tracing/trace

# 使用 function_graph 跟踪函数调用
echo function_graph > /sys/kernel/debug/tracing/current_tracer
cat /sys/kernel/debug/tracing/trace
```

ftrace 还可以用于测量函数执行时间：

```bash
# 启用函数执行时间跟踪
echo function > /sys/kernel/debug/tracing/current_tracer
echo 1 > /sys/kernel/debug/tracing/options/functions

# 查看跟踪结果（包含执行时间）
cat /sys/kernel/debug/tracing/trace
```

### 7.4.4 常见问题与排查思路

本节总结驱动开发中最常见的问题类型和排查思路，帮助开发者快速定位问题。

**驱动加载失败**

当驱动无法加载时，首先检查以下内容：

```bash
# 1. 检查模块是否成功加载
lsmod | grep my_driver

# 2. 查看模块加载错误信息
dmesg | tail -50

# 3. 检查模块依赖
modinfo my_driver.ko

# 4. 检查内核符号是否缺失
dmesg | grep "Unknown symbol"

# 5. 检查设备是否被其他驱动占用
cat /sys/bus/platform/drivers/my_driver/bind
cat /sys/bus/platform/devices/*/driver

# 6. 验证模块签名（如果启用了模块签名）
dmesg | grep "module signature"
cat /proc/cmdline | grep module.sig_enforce
```

**设备未能枚举**

设备未能正确枚举是常见的驱动问题：

```bash
# 1. 检查设备树节点是否存在
ls -la /proc/device-tree/

# 2. 检查设备是否被正确创建
ls -la /sys/bus/platform/devices/ | grep my_device

# 3. 查看 uevent 信息是否完整
cat /sys/bus/platform/devices/my_device/uevent

# 4. 检查驱动是否与设备匹配
cat /sys/bus/platform/devices/my_device/driver

# 5. 检查驱动绑定是否成功
echo "add" > /sys/kernel/debug/udev_trigger  # 手动触发 uevent

# 6. 查看 dmesg 中的匹配过程
dmesg | grep -i "my_device\|match\|probe"
```

**系统崩溃或挂起**

当驱动导致系统崩溃时：

```bash
# 1. 配置内核以捕获崩溃信息
# 在 /etc/default/grub 中添加：
# GRUB_CMDLINE_LINUX="nmi_watchdog=1 oops=panic"

# 2. 查看 oops/panic 信息
dmesg | grep -A 50 "Oops\|Kernel panic"

# 3. 使用 kdump 捕获崩溃转储
# 配置 kdump：
yum install kexec-tools crash
systemctl enable kdump
systemctl start kdump

# 4. 分析崩溃转储
crash /var/crash/vmcore /usr/lib/debug/$(uname -r)/vmlinux
```

**电源管理问题**

电源管理相关的调试：

```bash
# 1. 查看设备的电源状态
cat /sys/devices/platform/my_device/power/control
cat /sys/devices/platform/my_device/power/runtime_status
cat /sys/devices/platform/my_device/power/active_time
cat /sys/devices/platform/my_device/power/autosuspend_delay_ms

# 2. 启用 PM 调试
echo 1 > /sys/power/pm_debug_messages

# 3. 查看系统睡眠过程
dmesg | grep -i "suspend\|resume\|sleep"

# 4. 测试系统睡眠
# 进入待机
echo mem > /sys/power/state

# 唤醒（按电源键或特定键）

# 5. 检查唤醒源
cat /sys/kernel/debug/rtc0/date
cat /sys/power/wakeup_count
```

**调试辅助技巧**

```bash
# 添加内核打印语句（临时）
# 编辑驱动源文件，添加：
printk(KERN_DEBUG "Debug: var=%d\n", var);

# 重新编译模块
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules

# 使用 strace 跟踪用户空间应用与驱动的交互
strace -e open,ioctl,read,write /usr/bin/my_app

# 使用 ltrace 跟踪库调用
ltrace -e "*my_driver*" /usr/bin/my_app

# 检查内核配置
zcat /proc/config.gz | grep -i debug
cat /boot/config-$(uname -r) | grep -i debug
```

## 本章面试题

### 面试题1：驱动中如何实现电源管理？

电源管理是现代 Linux 驱动开发中的重要组成部分。驱动开发者需要理解并实现电源管理功能，以配合系统实现高效的电源利用，同时保证设备在状态变化时的正确行为。

**电源管理的层次**：Linux 电源管理可以分为两个层次：系统级电源管理（System Power Management）和运行时电源管理（Runtime Power Management）。系统级电源管理涉及整个系统的睡眠、唤醒等状态变化，如 Suspend to RAM、Hibernate 等；运行时电源管理则关注单个设备在运行过程中的动态电源管理，根据设备的使用情况自动进入或退出低功耗状态。

**系统睡眠的实现**：对于系统睡眠，驱动需要实现 `struct dev_pm_ops` 中的回调函数。主要包括 `suspend`（系统进入睡眠时调用）和 `resume`（系统唤醒时调用）。在 `suspend` 回调中，驱动应该保存设备状态、停止设备操作、禁用中断、关闭时钟；在 `resume` 回调中，驱动应该恢复设备配置、重新使能中断、恢复设备操作。对于 Hibernate（休眠），还需要实现 `freeze`/`thaw` 和 `poweroff`/`restore` 回调。

**运行时 PM 的实现**：运行时电源管理通过引用计数机制来控制设备的电源状态。驱动需要在设备被使用时调用 `pm_runtime_get()` 增加使用计数，在设备不再使用时调用 `pm_runtime_put()` 减少使用计数。当计数归零时，Runtime PM 框架会调用驱动的 `runtime_suspend` 回调将设备置于低功耗状态；当设备再次被使用时，会调用 `runtime_resume` 回调恢复设备。

**PM 回调的注意事项**：实现电源管理回调时需要注意几个关键点。首先是返回值，`suspend` 回调如果返回错误，系统会中止睡眠过程；其次是睡眠阶段顺序，`prepare` 在最前，`suspend_late` 在最后；最后是错误处理，任何阶段的错误都应该正确传播，以便系统可以安全地回滚。

### 面试题2：如何调试驱动中的问题？

驱动开发中的调试与用户空间程序有很大不同，因为驱动运行在内核空间，错误往往会导致系统不稳定或崩溃。掌握正确的调试方法和工具是驱动开发者的必备技能。

**使用 printk 进行调试**：最基本也是最有效的调试方法是使用 `printk()` 输出调试信息。`printk` 有多个日志级别，从 `KERN_EMERG`（紧急）到 `KERN_DEBUG`（调试）。在开发阶段，可以使用 `KERN_DEBUG` 级别输出详细的调试信息。需要注意，`printk` 是异步的，在中断上下文中使用要格外小心。

**利用 sysfs 调试接口**：sysfs 是驱动向用户空间暴露信息的主要渠道。开发者应该在驱动中创建有用的调试接口，如设备状态参数、错误计数器、控制开关等。通过读取这些接口的值，可以了解设备的当前状态；通过写入这些接口，可以触发特定的操作或改变设备行为。

**使用 dynamic_debug**：对于更细粒度的调试控制，`dynamic_debug` 是非常强大的工具。它允许在不重新编译模块的情况下，动态地启用或禁用特定的调试消息。通过向 `/proc/dynamic_debug/control` 写入特定格式的命令，可以控制特定文件、函数甚至行的调试输出。

**利用 tracepoint 和 ftrace**：对于性能分析和复杂问题的调试，`tracepoint` 和 `ftrace` 是不可替代的工具。tracepoint 是在内核代码中预定义的跟踪点，可以收集特定的运行时信息；ftrace 可以跟踪函数的调用过程，帮助理解代码的执行流程。

**监控 uevent**：对于热插拔和设备枚举问题，监控 uevent 事件非常有用。使用 `udevadm monitor` 可以实时看到设备的添加、移除等事件；查看 `/sys/device/uevent` 可以获取设备的详细信息。

**系统崩溃分析**：当驱动导致内核崩溃时（oops 或 panic），需要分析崩溃信息。使用 `dmesg` 查看内核消息缓冲区的内容；对于 kdump 生成的转储文件，可以使用 `crash` 工具进行分析。分析时需要对应的内核调试信息文件（vmlinux）。

**调试驱动加载问题**：如果驱动无法加载，首先使用 `modinfo` 检查模块信息；然后查看 `dmesg` 中的错误消息；检查模块依赖是否满足；如果是签名问题，需要正确签名模块。

调试驱动问题需要综合运用多种工具和方法，并根据问题的性质选择合适的调试策略。对于简单的逻辑错误，`printk` 和 dynamic_debug 通常足够；对于复杂的状态问题，tracepoint 和 ftrace 更有帮助；对于系统崩溃，则需要分析内核转储。

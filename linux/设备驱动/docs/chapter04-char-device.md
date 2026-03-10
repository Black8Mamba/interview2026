# 第4章 字符设备驱动案例

字符设备是 Linux 设备驱动开发中最基础也是最常见的一类设备。与块设备和网络设备相比，字符设备的访问方式更加直接——它以字节流为单位进行数据传输，没有缓冲区的概念。在本章中，我们将深入探讨字符设备驱动的核心框架，并通过 LED 和按键两个实际案例来演示驱动的开发流程。通过本章的学习，读者将能够掌握字符设备驱动的基本开发方法，并理解驱动与用户空间交互的机制。

## 4.1 字符设备驱动框架

字符设备驱动是 Linux 驱动开发的基础，几乎所有非存储类的硬件设备都可以通过字符设备的方式进行访问。从本质上讲，字符设备驱动就是内核空间与用户空间之间的一座桥梁，它将用户对设备节点的读写操作转换为对硬件寄存器的访问。理解字符设备驱动的框架结构，是进行任何 Linux 驱动开发的前提条件。

### 4.1.1 字符设备结构体

在 Linux 内核中，字符设备的核心抽象是 `struct cdev` 结构体。这个结构体封装了字符设备的所有必要信息，包括设备号、操作集等。理解 cdev 结构体的各个成员，对于掌握字符设备驱动的注册流程至关重要。

`struct cdev` 定义位于 Linux 内核源码的 `include/linux/cdev.h` 文件中，以下是 Linux 5.x 内核中的完整定义：

```c
// 文件位置：include/linux/cdev.h
struct cdev {
    // kobject 是设备模型的基础，提供了引用计数、sysfs 导出等能力
    struct kobject kobj;
    // owner 指向拥有该 cdev 的模块，通常设置为 THIS_MODULE
    // 确保模块在使用期间不会被卸载
    struct module *owner;
    // ops 指向文件操作集，定义了 open、read、write 等回调函数
    const struct file_operations *ops;
    // dev 存储设备号，高位为主设备号，低位为次设备号
    dev_t dev;
    // count 表示该设备占用的连续设备号数量
    // 当一个驱动管理多个相似的字符设备时使用
    unsigned int count;
    // list 用于将 cdev 加入到内核的字符设备链表中
    struct list_head list;
};
```

下面对 cdev 结构体的核心成员进行详细解析，帮助读者深入理解每个成员的作用和意义。

**kobj 成员**是 cdev 结构体中最重要的成员之一。它是 Linux 设备模型的基础，通过内嵌 kobject，cdev 自动获得了以下能力：引用计数管理，确保设备在使用期间不会被意外释放；sysfs 导出，在 /sys/class/ 或 /sys/devices/ 下创建相应的目录结构；热插拔事件支持，当设备添加或移除时向用户空间发送 uevent 事件。kobj 的引入使得字符设备可以无缝接入 Linux 的设备层次结构。

**owner 成员**指向拥有该 cdev 的内核模块。在大多数情况下，这个成员被设置为 THIS_MODULE，这是一个宏定义，代表当前模块本身。设置这个成员的主要目的是确保模块在其设备正在被使用时不会被卸载。如果在设备仍处于打开状态时卸载模块，将会导致系统崩溃。

**ops 成员**是指向 `struct file_operations` 结构体的指针，这是字符设备驱动的核心。它定义了用户空间对设备进行操作时内核调用的回调函数，如打开设备、读取数据、写入数据等。我们将在后续章节详细介绍 file_operations 结构体。

**dev 成员**存储设备的设备号（device number）。在 Linux 中，设备号是一个 32 位整数，其中高 12 位（12-31 位）表示主设备号（major number），低 20 位（0-19 位）表示次设备号（minor number）。主设备号用于标识设备所属的驱动，次设备号用于区分同一驱动管理的多个设备。

**count 成员**表示该 cdev 占用的连续设备号数量。当一个驱动需要管理多个连续的设备号时（例如，tty 驱动需要为每个终端分配一个设备号），这个成员用于指定设备号范围。

#### 分配和初始化 cdev

在内核源码中，cdev 的分配和初始化通常有以下几种方式：

```c
// 方法1：静态分配 cdev
static struct cdev my_cdev;

// 方法2：动态分配 cdev
struct cdev *cdev_alloc(void)
{
    struct cdev *p;

    p = kzalloc(sizeof(struct cdev), GFP_KERNEL);
    if (p) {
        // 初始化链表节点
        INIT_LIST_HEAD(&p->list);
        // kobject 的初始化稍后完成
        kobject_init(&p->kobj, &cdev_ktype);
    }
    return p;
}
```

初始化 cdev 使用 `cdev_init` 函数：

```c
// 文件位置：fs/char_dev.c
void cdev_init(struct cdev *cdev, const struct file_operations *fops)
{
    memset(cdev, 0, sizeof(*cdev));
    // 初始化 kobject，设置在 cdev_kset 中
    kobject_init(&cdev->kobj, &cdev_ktype);
    // 初始化链表
    INIT_LIST_HEAD(&cdev->list);
    // 设置文件操作集
    cdev->ops = fops;
    // 设置拥有者模块
    cdev->owner = THIS_MODULE;
}
```

从上述代码可以看到，`cdev_init` 函数主要完成以下工作：首先使用 `memset` 将 cdev 结构体清零，然后初始化内嵌的 kobject，接着初始化链表，最后设置文件操作集和拥有者模块。

### 4.1.2 注册流程

字符设备驱动的注册流程是驱动开发中的核心知识点。整个注册过程涉及多个步骤，包括设备号分配、cdev 初始化、设备添加、以及设备节点创建。理解这个流程对于开发稳定的字符设备驱动至关重要。

#### 设备号分配

设备号是 Linux 系统中标识字符设备的唯一标识符。在注册字符设备之前，首先需要分配设备号。内核提供了两种分配设备号的方式：静态分配和动态分配。

**静态分配方式**使用 `register_chrdev_region` 函数，适用于已知设备号的情况：

```c
// 文件位置：fs/char_dev.c
/**
 * register_chrdev_region - 注册指定的字符设备号范围
 * @from: 起始设备号
 * @count: 连续设备号的数量
 * @name: 设备名称，用于 /proc/devices 显示
 *
 * 返回值：成功返回 0，失败返回负数错误码
 */
int register_chrdev_region(dev_t from, unsigned int count, const char *name)
{
    struct char_device_struct *cd;
    dev_t to = from + count;
    dev_t n, next;

    // 遍历请求的设备号范围
    for (n = from; n < to; n = next) {
        next = MKDEV(MAJOR(n) + 1, 0);
        if (next > to)
            next = to;

        // 分配并注册设备号
        cd = __register_chrdev_region(MAJOR(n), MINOR(n),
                       next - n, name);
        if (IS_ERR(cd))
            goto fail;
    }
    return 0;

fail:
    to = n;
    // 发生错误时，回滚已注册的部分
    for (n = from; n < to; n = next) {
        next = MKDEV(MAJOR(n) + 1, 0);
        kfree(__register_chrdev_region(MAJOR(n), MINOR(n),
                     next - n, name));
    }
    return PTR_ERR(cd);
}
```

静态分配方式的主要缺点是需要事先知道设备号，这在与其它驱动发生冲突时会导致注册失败。因此，在开发新驱动时，更推荐使用动态分配方式。

**动态分配方式**使用 `alloc_chrdev_region` 函数，由内核自动分配一个可用的设备号：

```c
// 文件位置：fs/char_dev.c
/**
 * alloc_chrdev_region - 动态分配字符设备号范围
 * @dev: 输出参数，用于存储分配到的起始设备号
 * @baseminor: 起始次设备号
 * @count: 连续设备号的数量
 * @name: 设备名称，用于 /proc/devices 显示
 *
 * 返回值：成功返回 0，失败返回负数错误码
 */
int alloc_chrdev_region(dev_t *dev, unsigned int baseminor,
            unsigned int count, const char *name)
{
    struct char_device_struct *cd;
    dev_t from;
    int ret;

    // 分配设备号结构
    cd = __register_chrdev_region(baseminor, -1, count, name);
    if (IS_ERR(cd))
        return PTR_ERR(cd);

    // 获取分配到的设备号
    from = MKDEV(cd->major, cd->baseminor);
    *dev = from;

    return 0;
}
```

动态分配返回的设备号可以通过 `MAJOR` 和 `MINOR` 宏来提取主次设备号：

```c
// 提取主设备号
int major = MAJOR(dev);
// 提取次设备号
int minor = MINOR(dev);
```

#### cdev 初始化和添加

分配设备号后，需要初始化 cdev 结构体并将其添加到内核的字符设备链表中。这一步骤使用 `cdev_init` 和 `cdev_add` 函数完成：

```c
// 文件位置：fs/char_dev.c
/**
 * cdev_add - 将字符设备添加到系统中
 * @p: 指向 cdev 结构体的指针
 * @dev: 起始设备号
 * @count: 设备号数量
 *
 * 返回值：成功返回 0，失败返回负数错误码
 */
int cdev_add(struct cdev *p, dev_t dev, unsigned int count)
{
    int ret;

    // 设置设备号和数量
    p->dev = dev;
    p->count = count;

    // 检查是否有有效的 file_operations
    if (!p->ops) {
        ret = -EINVAL;
        goto out;
    }

    // 将 cdev 添加到内核的字符设备链表中
    // 这是设备可以接受访问的关键步骤
    p->owner = THIS_MODULE;

    // 调用 kobject 添加，将设备导出到 sysfs
    ret = kobj_map(cdev_map, p->dev, count, p,
           p->owner, cdev_default_release);
    if (ret)
        goto out;

    // 添加到字符设备列表（用于 /proc/devices）
    mutex_lock(&cdev_lock);
    list_add(&p->list, &cdev_list);
    mutex_unlock(&cdev_lock);

out:
    return ret;
}
```

`cdev_add` 函数完成以下关键工作：首先设置 cdev 的设备号和数量，然后检查是否有有效的 file_operations，接着通过 `kobj_map` 将设备映射到内核的字符设备查找表中，最后将 cdev 添加到全局链表中。

需要特别注意的是，`cdev_add` 成功返回后，设备就已经可以被访问了。这意味着在此之前，必须确保所有的初始化工作都已完成，包括设置正确的 file_operations、分配任何需要的内存等。

#### 设备节点创建

在早期的 Linux 系统中，设备节点需要手动使用 mknod 命令创建。现代 Linux 系统则通过 udev 机制自动创建设备节点。udev 需要知道设备的主次设备号和设备名称才能创建设备节点。

为了支持 udev 自动创建设备节点，驱动需要创建类和设备。这一步骤使用 `class_create` 和 `device_create` 函数：

```c
// 文件位置：drivers/base/class.c
/**
 * class_create - 创建一个设备类
 * @owner: 指向模块的指针
 * @name: 类的名称，会在 /sys/class/ 下创建对应目录
 *
 * 返回值：成功返回指向 struct class 的指针，失败返回 ERR_PTR
 */
struct class *class_create(struct module *owner, const char *name)
{
    return class_create_compat(name);
}

// 文件位置：drivers/base/core.c
/**
 * device_create - 创建设备并注册到系统中
 * @class: 设备所属的类
 * @parent: 父设备（通常为 NULL）
 * @devt: 设备号
 * @drvdata: 驱动私有数据
 * @fmt: 设备名称格式化字符串
 *
 * 返回值：成功返回指向 struct device 的指针，失败返回 ERR_PTR
 */
struct device *device_create(struct class *class, struct device *parent,
                 dev_t devt, void *drvdata, const char *fmt, ...)
{
    va_list vargs;
    struct device *dev;

    va_start(vargs, fmt);
    dev = device_create_vargs(class, parent, devt, drvdata, fmt, vargs);
    va_end(vargs);

    return dev;
}
```

#### 完整的注册流程示例

以下是一个完整的字符设备注册流程示例：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME     "my_device"
#define CLASS_NAME      "my_class"

static int dev_major;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static int my_open(struct inode *inode, struct file *file)
{
    pr_info("my_device: open\n");
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buf,
            size_t count, loff_t *offset)
{
    pr_info("my_device: read\n");
    return 0;
}

static ssize_t my_write(struct file *file, const char __user *buf,
             size_t count, loff_t *offset)
{
    pr_info("my_device: write\n");
    return count;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("my_device: release\n");
    return 0;
}

static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .read    = my_read,
    .write   = my_write,
    .release = my_release,
};

static int __init my_init(void)
{
    dev_t dev_num;
    int ret;

    // 步骤1：动态分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        return ret;
    }
    dev_major = MAJOR(dev_num);
    pr_info("Device registered with major %d\n", dev_major);

    // 步骤2：初始化 cdev
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;

    // 步骤3：添加 cdev 到系统
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto err_cdev_add;
    }

    // 步骤4：创建设备类
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_class)) {
        pr_err("Failed to create class\n");
        ret = PTR_ERR(my_class);
        goto err_class_create;
    }

    // 步骤5：创建设备节点
    my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        pr_err("Failed to create device\n");
        ret = PTR_ERR(my_device);
        goto err_device_create;
    }

    pr_info("Driver initialized successfully\n");
    return 0;

err_device_create:
    class_destroy(my_class);
err_class_create:
    cdev_del(&my_cdev);
err_cdev_add:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit my_exit(void)
{
    // 逆向注销
    device_destroy(my_class, MKDEV(dev_major, 0));
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(MKDEV(dev_major, 0), 1);
    pr_info("Driver removed\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example Author");
MODULE_DESCRIPTION("A simple character device driver");
```

### 4.1.3 file_operations

`struct file_operations` 是字符设备驱动的核心结构体，它定义了一组函数指针，这些函数指针指定了用户空间对设备文件进行操作时内核调用的处理函数。每个字符设备驱动都需要实现自己的 file_operations 结构体，并将其与 cdev 关联起来。

`file_operations` 结构体定义位于 Linux 内核源码的 `include/linux/fs.h` 文件中。以下是 Linux 5.x 内核中的完整定义：

```c
// 文件位置：include/linux/fs.h
struct file_operations {
    // 模块拥有者，通常设置为 THIS_MODULE
    struct module *owner;

    // 打开设备文件时调用
    loff_t (*llseek) (struct file *, loff_t, int);

    // 读取设备数据
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);

    // 写入设备数据
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);

    // 读取文件属性
    int (*readdir) (struct file *, void *, filldir_t);

    // 轮询操作，用于实现非阻塞 I/O
    __poll_t (*poll) (struct file *, struct poll_table_struct *);

    // ioctl 命令处理
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);

    // 兼容的 ioctl 处理（已废弃）
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);

    // 内存映射
    int (*mmap) (struct file *, struct vm_area_struct *);

    // 打开设备文件
    int (*open) (struct inode *, struct file *);

    // 释放设备文件
    int (*release) (struct inode *, struct file *);

    // 同步文件操作
    int (*fsync) (struct file *, loff_t, loff_t, int);

    // 异步 fsync
    int (*aio_fsync) (struct kiocb *, int);

    // 异步读取
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);

    // 异步写入
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);

    // 文件标志操作
    int (*flock) (struct file *, int, struct file_lock *);

    // 稀疏文件操作
    int (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
    int (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);

    // 文件特性标志
    unsigned int (*sendfile) (struct file *, loff_t *, size_t, read_actor_t, void *);
    unsigned long (*sendfile64)(struct file *, loff_t *, size_t, read_actor_t, void *);

    // 设置文件租约
    int (*setlease)(struct file *, long, struct file_lock **, void **);

    // 原子文件锁定
    long (*fallocate)(struct file *, int, loff_t, loff_t);

    // 目录通知
    int (*show_fdinfo)(struct seq_file *m, struct file *);

    // 打印操作
    int (*uring_cmd)(struct io_uring_cmd *, unsigned int);
    int (*uring_cmd_iopoll)(struct io_uring_cmd *, struct io_comp_batch *,
                unsigned long poll_nelems);
    ...
};
```

在实际驱动开发中，我们通常只需要实现其中几个常用的回调函数。以下是各个核心函数的详细说明。

#### open 和 release 函数

`open` 函数在用户空间打开设备文件时调用，通常用于初始化设备、获取必要的资源等。以下是 open 函数的典型实现：

```c
// 文件位置：fs/open.c
/**
 * chrdev_open - 打开字符设备
 * @inode: 关联的 inode 结构
 * @filp: 关联的文件结构
 *
 * 这是字符设备的默认 open 实现，它负责：
 * 1. 根据设备号查找对应的 cdev
 * 2. 将 cdev 与文件结构关联
 * 3. 调用驱动的 open 回调
 */
static int chrdev_open(struct inode *inode, struct file *filp)
{
    struct cdev *p;
    int ret = -ENXIO;

    // 从 inode 中获取 cdev
    p = inode->i_cdev;
    if (!p)
        return ret;

    // 增加 cdev 的引用计数
    if (!kobj_get(p->kobj.parent))
        return -ENXIO;

    // 将 cdev 与文件结构关联
    filp->f_op = fops_get(p->ops);

    // 调用驱动的 open 回调
    if (filp->f_op->open) {
        ret = filp->f_op->open(inode, filp);
        if (ret)
            put_ops(filp->f_op);
    }

    return ret;
}
```

驱动的 open 函数实现示例：

```c
static int my_open(struct inode *inode, struct file *file)
{
    struct my_device *dev;

    // 从 inode 中获取设备的私有数据
    dev = container_of(inode->i_cdev, struct my_device, cdev);
    // 将设备结构体关联到文件结构
    file->private_data = dev;

    // 初始化设备（如果是第一次打开）
    atomic_inc(&dev->open_count);

    pr_info("Device opened, open count: %d\n", atomic_read(&dev->open_count));
    return 0;
}
```

`release` 函数在设备文件关闭时调用，通常用于释放资源、更新状态等：

```c
static int my_release(struct inode *inode, struct file *file)
{
    struct my_device *dev = file->private_data;

    atomic_dec(&dev->open_count);
    pr_info("Device closed, open count: %d\n", atomic_read(&dev->open_count));

    return 0;
}
```

#### read 和 write 函数

`read` 函数用于从设备读取数据并传递给用户空间，`write` 函数用于将用户空间的数据写入设备。这两个函数是字符设备驱动中最核心的函数。

`read` 函数的原型：

```c
ssize_t (*read)(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
```

参数说明：
- `filp`：指向已打开的文件结构
- `buf`：用户空间缓冲区的地址
- `count`：要读取的字节数
- `f_pos`：文件位置指针

返回值：成功读取的字节数，负值表示错误

`read` 函数实现的注意事项：
1. 必须使用 `copy_to_user` 函数将数据从内核空间复制到用户空间
2. 需要更新 `f_pos` 表示当前读取位置
3. 应该处理各种边界情况，如文件结束、错误等

```c
static ssize_t my_read(struct file *filp, char __user *buf,
            size_t count, loff_t *f_pos)
{
    struct my_device *dev = filp->private_data;
    ssize_t ret;
    size_t bytes_to_read;

    // 检查文件位置是否超过数据末尾
    if (*f_pos >= dev->data_size)
        return 0;  // 读到文件末尾

    // 计算实际可以读取的字节数
    bytes_to_read = min(count, dev->data_size - *f_pos);

    // 复制数据到用户空间
    if (copy_to_user(buf, dev->data + *f_pos, bytes_to_read))
        return -EFAULT;

    // 更新文件位置
    *f_pos += bytes_to_read;

    pr_info("Read %zu bytes from device\n", bytes_to_read);
    return bytes_to_read;
}
```

`write` 函数的原型：

```c
ssize_t (*write)(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
```

参数说明：
- `filp`：指向已打开的文件结构
- `buf`：用户空间缓冲区的地址
- `count`：要写入的字节数
- `f_pos`：文件位置指针

返回值：成功写入的字节数，负值表示错误

`write` 函数的实现：

```c
static ssize_t my_write(struct file *filp, const char __user *buf,
             size_t count, loff_t *f_pos)
{
    struct my_device *dev = filp->private_data;
    ssize_t ret;
    size_t bytes_to_write;

    // 检查写入位置是否超出缓冲区
    if (*f_pos >= MY_DEVICE_BUFFER_SIZE)
        return -ENOSPC;

    // 计算实际可以写入的字节数
    bytes_to_write = min(count, MY_DEVICE_BUFFER_SIZE - *f_pos);

    // 从用户空间复制数据
    if (copy_from_user(dev->data + *f_pos, buf, bytes_to_write))
        return -EFAULT;

    // 更新文件位置
    *f_pos += bytes_to_write;

    // 更新数据大小
    if (*f_pos > dev->data_size)
        dev->data_size = *f_pos;

    pr_info("Wrote %zu bytes to device\n", bytes_to_write);
    return bytes_to_write;
}
```

#### copy_to_user 和 copy_from_user

这两个函数是内核空间与用户空间数据交换的桥梁。它们之所以存在，是因为内核空间和用户空间的内存地址空间是隔离的，直接访问用户空间指针会导致内核崩溃。

```c
// 文件位置：arch/arm64/lib/uaccess_with_memcpy.c
/**
 * copy_to_user - 将数据从内核空间复制到用户空间
 * @to:   用户空间目标地址
 * @from: 内核空间源地址
 * @n:    复制字节数
 *
 * 返回值：成功复制返回 0，失败返回未复制的字节数
 */
static __always_inline unsigned long
__must_check copy_to_user(void __user *to, const void *from, unsigned long n)
{
    if (access_ok(to, n)) {
        n = raw_copy_to_user(to, from, n);
    }
    return n;
}

/**
 * copy_from_user - 将数据从用户空间复制到内核空间
 * @to:   内核空间目标地址
 * @from: 用户空间源地址
 * @n:    复制字节数
 *
 * 返回值：成功复制返回 0，失败返回未复制的字节数
 */
static __always_inline unsigned long
__must_check copy_from_user(void *to, const void __user *from, unsigned long n)
{
    if (access_ok(from, n)) {
        n = raw_copy_from_user(to, from, n);
    }
    return n;
}
```

使用这两个函数时需要注意以下几点：
1. 函数返回值表示未成功复制的字节数，为 0 表示完全成功
2. 如果返回值不为 0，应该返回 -EFAULT 表示复制失败
3. 在访问用户空间指针之前，`access_ok` 函数会检查指针是否合法

#### ioctl 函数

`ioctl`（I/O Control）是设备驱动提供的一种特殊接口，用于执行设备特定的命令。与普通的读写操作不同，`ioctl` 通常用于配置设备、获取设备状态等控制操作。

`ioctl` 函数的原型：

```c
long (*unlocked_ioctl)(struct file *filp, unsigned int cmd, unsigned long arg);
```

参数说明：
- `filp`：指向已打开的文件结构
- `cmd`：设备特定的命令码
- `arg`：命令的参数（可以是整数或指针）

命令码的设计遵循一定的规范，通常包含以下信息：
- 幻数（Magic Number）：用于标识特定的设备
- 命令编号：区分不同的命令
- 方向：表示数据传输的方向
- 数据大小：参数的大小

```c
// ioctl 命令码的定义规范
/*
 * _IO(type, nr):        无参数
 * _IOR(type, nr, size): 读操作
 * _IOW(type, nr, size): 写操作
 * _IOWR(type, nr, size): 读写操作
 */
#define MY_DEVICE_IOC_MAGIC    'M'
#define MY_DEVICE_IOC_RESET   _IO(MY_DEVICE_IOC_MAGIC, 0)
#define MY_DEVICE_IOC_GET_STATUS   _IOR(MY_DEVICE_IOC_MAGIC, 1, int)
#define MY_DEVICE_IOC_SET_MODE _IOW(MY_DEVICE_IOC_MAGIC, 2, int)
#define MY_DEVICE_IOC_MAXNR    3
```

`ioctl` 函数的实现：

```c
static long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct my_device *dev = filp->private_data;
    int ret = 0;
    int status;
    int mode;

    // 检查命令码是否有效
    if (_IOC_TYPE(cmd) != MY_DEVICE_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > MY_DEVICE_IOC_MAXNR)
        return -ENOTTY;

    switch (cmd) {
    case MY_DEVICE_IOC_RESET:
        // 重置设备
        dev->status = 0;
        dev->mode = 0;
        pr_info("Device reset\n");
        break;

    case MY_DEVICE_IOC_GET_STATUS:
        // 获取设备状态
        status = dev->status;
        if (copy_to_user((int __user *)arg, &status, sizeof(status)))
            return -EFAULT;
        break;

    case MY_DEVICE_IOC_SET_MODE:
        // 设置设备模式
        if (copy_from_user(&mode, (int __user *)arg, sizeof(mode)))
            return -EFAULT;
        dev->mode = mode;
        pr_info("Device mode set to %d\n", mode);
        break;

    default:
        return -ENOTTY;
    }

    return ret;
}
```

#### poll 函数

`poll` 函数用于实现设备的非阻塞 I/O 访问。当设备没有数据可读或无法立即写入时，使用 `poll` 可以让应用程序知道设备的状态，避免阻塞等待。

```c
// 文件位置：include/linux/poll.h
__poll_t (*poll)(struct file *filp, struct poll_table_struct *wait);
```

`poll` 函数需要调用 `poll_wait` 将当前进程添加到设备的等待队列中，并返回设备当前的状态：

```c
static __poll_t my_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct my_device *dev = filp->private_data;
    __poll_t mask = 0;

    // 将进程添加到设备的等待队列
    poll_wait(filp, &dev->waitq, wait);

    // 检查设备状态并设置相应的标志
    if (dev->has_data)
        mask |= EPOLLIN | EPOLLRDNORM;    // 设备可读

    if (dev->has_space)
        mask |= EPOLLOUT | EPOLLWRNORM;   // 设备可写

    return mask;
}
```

`poll_wait` 函数不会阻塞，它只是将进程添加到等待队列并返回。当设备状态变化时，应该调用 `wake_up_interruptible` 唤醒等待队列中的进程。

### 4.1.4 file 结构体

在字符设备驱动中，`struct file` 是一个核心的结构体，它代表一个已打开的设备文件。每次打开设备文件都会创建一个新的 file 结构体，直到文件被关闭。理解 file 结构体对于正确实现设备驱动至关重要。

`file` 结构体定义位于 Linux 内核源码的 `include/linux/fs.h` 文件中：

```c
// 文件位置：include/linux/fs.h
struct file {
    // 文件操作集，由驱动提供
    const struct file_operations    *f_op;

    // 文件标志，如 O_RDONLY、O_NONBLOCK 等
    unsigned int            f_flags;

    // 文件模式，如 S_IRUSR、S_IWUSR 等
    fmode_t                 f_mode;

    // 文件位置偏移
    loff_t                  f_pos;

    // 读取/写入的最小/首选字节数
    size_t                  f_reada, f_ramax, f_raend, f_ralen, f_rawin;

    // 文件相关的安全上下文
    void                    *f_security;

    // 指向内核分配的私有数据
    void                    *private_data;

    // 文件锁列表
    struct file_lock        *f_locks;

    // 文件系统特定的私有数据
    struct address_space    *f_mapping;

    // 指向关联的 inode
    struct inode            *f_inode;

    // 写回缓存
    struct writeback_control *f_wb;

    // cred 结构体
    const struct cred       *f_cred;

    // 错误状态
    int                     f_error;

    // 用户信息
    atomic_long_t           f_count;
    unsigned int            f_uid;
    unsigned int            f_gid;

    // 文件路径
    struct path             f_path;

    // 目录项
    struct dentry           *f_dentry;

    ...
};
```

在驱动开发中，最常用的成员是 `private_data`。驱动可以使用这个成员来存储设备的私有数据，这样在后续的操作（如 read、write、ioctl 等）中就可以通过 file 结构体访问这些数据。

```c
// 在 open 函数中设置 private_data
static int my_open(struct inode *inode, struct file *file)
{
    struct my_device *dev;

    // 假设在 cdev 中存储了设备结构体的指针
    dev = container_of(inode->i_cdev, struct my_device, cdev);

    // 将设备结构体保存到 file->private_data
    file->private_data = dev;

    return 0;
}

// 在其他函数中访问 private_data
static ssize_t my_read(struct file *file, char __user *buf,
            size_t count, loff_t *f_pos)
{
    struct my_device *dev = file->private_data;

    // 访问设备结构体
    // ...

    return 0;
}
```

使用 `private_data` 的好处是可以为每个打开的文件维护独立的状态，这在处理多个并发打开的情况时非常有用。

## 4.2 LED 驱动实例

LED（发光二极管）驱动是嵌入式系统中最简单的字符设备驱动实例之一。通过实现 LED 驱动，读者可以掌握字符设备驱动的完整开发流程，包括设备号的申请、cdev 的初始化和添加、file_operations 的实现、以及 sysfs 接口的创建等。

### 4.2.1 硬件描述

在 ARM 嵌入式系统中，LED 通常通过 GPIO（通用输入输出）引脚连接到处理器。每个 GPIO 引脚可以被配置为输入或输出模式，用于连接 LED 时，通常配置为输出模式。

典型的 LED 连接方式如下：

```
+3.3V ----[电阻]----+---- LED ----+---- GPIO 引脚
```

当 GPIO 引脚输出高电平时，LED 两端没有电压差，LED 熄灭；当 GPIO 引脚输出低电平时，LED 两端有电压差，LED 点亮。当然，也可以反过来连接（输出高电平时 LED 点亮）。

在设备树中，LED 设备通常这样描述：

```dts
// 设备树中的 LED 节点
leds {
    compatible = "gpio-leds";

    led-green {
        gpios = <&gpio1 16 GPIO_ACTIVE_HIGH>;
        default-state = "off";
    };

    led-red {
        gpios = <&gpio1 17 GPIO_ACTIVE_HIGH>;
        default-state = "off";
    };
};
```

在这个设备树节点中，`gpios` 属性指定了 LED 连接的 GPIO 控制器和引脚号，`GPIO_ACTIVE_HIGH` 表示高电平点亮 LED。

### 4.2.2 驱动代码

以下是完整的 LED 驱动代码框架，包含了设备号分配、cdev 初始化、file_operations 实现等核心部分：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#define LED_NAME        "my_led"
#define LED_CLASS_NAME  "led_class"

// LED 设备结构体
struct led_device {
    dev_t           dev_num;      // 设备号
    struct cdev     cdev;         // 字符设备
    struct class    *class;       // 设备类
    struct device   *device;      // 设备
    int             gpio;         // GPIO 编号
    int             state;        // LED 状态：0=灭，1=亮
    struct device   *dev;         // 指向父设备的指针
};

// 全局 LED 设备
static struct led_device *led_dev;

// 文件操作集
static int led_open(struct inode *inode, struct file *file)
{
    struct led_device *led;

    // 从 cdev 获取 led_device
    led = container_of(inode->i_cdev, struct led_device, cdev);
    file->private_data = led;

    pr_info("LED device opened\n");
    return 0;
}

static int led_release(struct inode *inode, struct file *file)
{
    pr_info("LED device closed\n");
    return 0;
}

static ssize_t led_read(struct file *file, char __user *buf,
            size_t count, loff_t *offset)
{
    struct led_device *led = file->private_data;
    char state_str[2];

    // 返回 LED 状态（0 或 1）
    state_str[0] = led->state ? '1' : '0';
    state_str[1] = '\0';

    if (copy_to_user(buf, state_str, 1))
        return -EFAULT;

    return 1;
}

static ssize_t led_write(struct file *file, const char __user *buf,
             size_t count, loff_t *offset)
{
    struct led_device *led = file->private_data;
    char cmd;

    if (count == 0)
        return 0;

    // 从用户空间读取命令
    if (copy_from_user(&cmd, buf, 1))
        return -EFAULT;

    // 设置 LED 状态
    if (cmd == '1') {
        led->state = 1;
        gpio_set_value(led->gpio, 1);
        pr_info("LED turned ON\n");
    } else if (cmd == '0') {
        led->state = 0;
        gpio_set_value(led->gpio, 0);
        pr_info("LED turned OFF\n");
    }

    return count;
}

static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .read    = led_read,
    .write   = led_write,
    .release = led_release,
};

// 设备树匹配表
static const struct of_device_id led_of_match[] = {
    { .compatible = "vendor,my-led", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, led_of_match);

// probe 函数（当设备树节点匹配时调用）
static int led_probe(struct device *dev)
{
    struct device_node *node = dev->of_node;
    int ret;

    pr_info("LED probe called\n");

    // 分配 LED 设备结构体
    led_dev = kzalloc(sizeof(struct led_device), GFP_KERNEL);
    if (!led_dev)
        return -ENOMEM;

    led_dev->dev = dev;

    // 从设备树获取 GPIO 编号
    led_dev->gpio = of_get_named_gpio(node, "gpios", 0);
    if (!gpio_is_valid(led_dev->gpio)) {
        pr_err("Invalid GPIO: %d\n", led_dev->gpio);
        ret = led_dev->gpio;
        goto err_gpio;
    }

    // 请求 GPIO
    ret = gpio_request(led_dev->gpio, "led-gpio");
    if (ret) {
        pr_err("Failed to request GPIO: %d\n", ret);
        goto err_gpio_request;
    }

    // 配置 GPIO 为输出模式，默认关闭 LED
    ret = gpio_direction_output(led_dev->gpio, 0);
    if (ret) {
        pr_err("Failed to set GPIO direction: %d\n", ret);
        goto err_gpio_direction;
    }

    led_dev->state = 0;

    // 动态分配设备号
    ret = alloc_chrdev_region(&led_dev->dev_num, 0, 1, LED_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        goto err_dev_num;
    }

    pr_info("LED device major: %d, minor: %d\n",
        MAJOR(led_dev->dev_num), MINOR(led_dev->dev_num));

    // 初始化 cdev
    cdev_init(&led_dev->cdev, &led_fops);
    led_dev->cdev.owner = THIS_MODULE;

    // 添加 cdev
    ret = cdev_add(&led_dev->cdev, led_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto err_cdev_add;
    }

    // 创建设备类
    led_dev->class = class_create(THIS_MODULE, LED_CLASS_NAME);
    if (IS_ERR(led_dev->class)) {
        pr_err("Failed to create class\n");
        ret = PTR_ERR(led_dev->class);
        goto err_class;
    }

    // 创建设备
    led_dev->device = device_create(led_dev->class, NULL,
                    led_dev->dev_num, NULL, LED_NAME);
    if (IS_ERR(led_dev->device)) {
        pr_err("Failed to create device\n");
        ret = PTR_ERR(led_dev->device);
        goto err_device;
    }

    // 将 led_dev 保存到 device 的私有数据中
    dev_set_drvdata(dev, led_dev);

    pr_info("LED driver initialized\n");
    return 0;

err_device:
    class_destroy(led_dev->class);
err_cdev_add:
    cdev_del(&led_dev->cdev);
    unregister_chrdev_region(led_dev->dev_num, 1);
err_dev_num:
err_gpio_direction:
    gpio_free(led_dev->gpio);
err_gpio_request:
err_gpio:
    kfree(led_dev);
    return ret;
}

// remove 函数
static int led_remove(struct device *dev)
{
    if (!led_dev)
        return -ENODEV;

    // 关闭 LED
    gpio_set_value(led_dev->gpio, 0);

    // 注销设备
    device_destroy(led_dev->class, led_dev->dev_num);
    class_destroy(led_dev->class);
    cdev_del(&led_dev->cdev);
    unregister_chrdev_region(led_dev->dev_num, 1);

    // 释放 GPIO
    gpio_free(led_dev->gpio);

    // 释放内存
    kfree(led_dev);
    led_dev = NULL;

    pr_info("LED driver removed\n");
    return 0;
}

// 平台驱动结构体
static struct platform_driver led_driver = {
    .probe  = led_probe,
    .remove = led_remove,
    .driver = {
        .name   = "my-led",
        .of_match_table = led_of_match,
    },
};

module_platform_driver(led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example Author");
MODULE_DESCRIPTION("A simple LED driver");
MODULE_VERSION("1.0");
```

### 4.2.3 添加 sysfs 调试接口

sysfs 是 Linux 内核提供的一种虚拟文件系统，它以文件的形式呈现内核对象和它们的属性。通过 sysfs，用户空间程序可以读取和修改内核对象的状态。在驱动开发中，创建 sysfs 接口是一种常见的调试手段。

#### sysfs 属性定义

在 Linux 内核中，可以使用 `DEVICE_ATTR` 宏来定义设备属性：

```c
// 文件位置：include/linux/device.h
#define DEVICE_ATTR(name, mode, show, store) \
    DEVICE_ATTR_##name

#define DEVICE_ATTR_##name(__name, __mode, __show, __store) \
    static struct device_attribute dev_attr_##__name = __ATTR(__name, __mode, __show, __store)
```

以下是为 LED 驱动添加的 sysfs 属性：

```c
// 显示 LED 状态
static ssize_t led_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    struct led_device *led = dev_get_drvdata(dev);

    return sprintf(buf, "%d\n", led->state);
}

// 设置 LED 状态
static ssize_t led_store(struct device *dev,
             struct device_attribute *attr,
             const char *buf, size_t count)
{
    struct led_device *led = dev_get_drvdata(dev);
    int value;

    // 将字符串转换为整数
    if (kstrtoint(buf, 10, &value))
        return -EINVAL;

    // 设置 LED 状态
    if (value)
        led->state = 1;
    else
        led->state = 0;

    // 实际控制 GPIO
    gpio_set_value(led->gpio, led->state);

    return count;
}

// 定义设备属性
static DEVICE_ATTR(led, 0644, led_show, led_store);
```

#### 在驱动中创建和删除 sysfs 属性

在驱动的 probe 函数中创建 sysfs 属性：

```c
static int led_probe(struct device *dev)
{
    // ... 前面代码省略 ...

    // 创建设备属性文件
    ret = device_create_file(led_dev->device, &dev_attr_led);
    if (ret) {
        pr_err("Failed to create sysfs file\n");
        goto err_sysfs;
    }

    pr_info("LED driver initialized with sysfs\n");
    return 0;

err_sysfs:
    device_destroy(led_dev->class, led_dev->dev_num);
    // ... 其他清理代码 ...
}
```

在驱动的 remove 函数中删除 sysfs 属性：

```c
static int led_remove(struct device *dev)
{
    if (!led_dev)
        return -ENODEV;

    // 删除设备属性文件
    device_remove_file(led_dev->device, &dev_attr_led);

    // ... 其他清理代码 ...
}
```

#### 使用 sysfs 接口

sysfs 接口创建后，用户可以通过文件系统访问 LED 状态：

```bash
# 读取 LED 状态
cat /sys/class/led_class/my_led/led
# 输出: 0 (表示 LED 熄灭)

# 点亮 LED
echo 1 > /sys/class/led_class/my_led/led

# 熄灭 LED
echo 0 > /sys/class/led_class/my_led/led
```

## 4.3 按键驱动实例

按键驱动是字符设备驱动的另一个典型实例。与 LED 驱动不同，按键驱动需要处理中断事件，这涉及到 Linux 内核的中断处理机制。此外，为了支持非阻塞读取，还需要实现 poll 或 select 接口。

### 4.3.1 硬件描述

按键是嵌入式系统中最常见的人机交互输入设备。按键的基本原理是按下时使两个触点导通，释放时触点断开。在硬件连接上，按键通常采用以下方式：

```
+3.3V ----[上拉电阻]----+---- GPIO 引脚
                         |
                        按键
                         |
                        GND
```

这种连接方式称为上拉模式：当按键未按下时，GPIO 引脚被上拉电阻拉高，读取的值为 1（高电平）；当按键按下时，GPIO 引脚被短路到地，读取的值为 0（低电平）。

也可以采用下拉模式：

```
GND ----[下拉电阻]----+---- GPIO 引脚
                       |
                      按键
                       |
                      +3.3V
```

在设备树中，按键设备通常这样描述：

```dts
// 设备树中的按键节点
buttons {
    compatible = "vendor,my-button";

    button-1 {
        gpios = <&gpio1 18 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_POWER>;
        wakeup-source;
    };
};
```

在这个设备树节点中，`gpios` 属性指定了按键连接的 GPIO，`linux,code` 表示按键对应的键码，`wakeup-source` 表示该按键可以用于从睡眠状态唤醒系统。

### 4.3.2 驱动代码（带中断和 sysfs）

以下是完整的按键驱动代码，包含了中断处理、等待队列、poll 接口和 sysfs 接口：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>

#define BUTTON_NAME      "my_button"
#define BUTTON_CLASS_NAME "button_class"

// 按键设备结构体
struct button_device {
    dev_t           dev_num;
    struct cdev     cdev;
    struct class    *class;
    struct device   *device;
    int             gpio;          // GPIO 编号
    int             irq;           // 中断号
    int             pressed;       // 按键状态：0=未按下，1=按下
    int             released;      // 按键释放状态
    wait_queue_head_t waitq;      // 等待队列
    struct device   *dev;
};

// 全局按键设备
static struct button_device *btn_dev;

// 文件操作集
static int button_open(struct inode *inode, struct file *file)
{
    struct button_device *btn;

    btn = container_of(inode->i_cdev, struct button_device, cdev);
    file->private_data = btn;

    pr_info("Button device opened\n");
    return 0;
}

static int button_release(struct inode *inode, struct file *file)
{
    pr_info("Button device closed\n");
    return 0;
}

static ssize_t button_read(struct file *file, char __user *buf,
            size_t count, loff_t *offset)
{
    struct button_device *btn = file->private_data;
    int ret;

    // 如果是非阻塞模式且没有数据可读，立即返回
    if (file->f_flags & O_NONBLOCK) {
        if (btn->pressed == 0 && btn->released == 0)
            return -EAGAIN;
    } else {
        // 阻塞模式：等待按键事件
        ret = wait_event_interruptible(btn->waitq,
                        btn->pressed || btn->released);
        if (ret)
            return ret;
    }

    // 返回按键状态
    if (btn->pressed) {
        if (copy_to_user(buf, "pressed\n", 8))
            return -EFAULT;
        btn->pressed = 0;
        return 8;
    } else if (btn->released) {
        if (copy_to_user(buf, "released\n", 9))
            return -EFAULT;
        btn->released = 0;
        return 9;
    }

    return 0;
}

static __poll_t button_poll(struct file *file, struct poll_table_struct *wait)
{
    struct button_device *btn = file->private_data;
    __poll_t mask = 0;

    // 将当前进程添加到等待队列
    poll_wait(file, &btn->waitq, wait);

    // 检查是否有按键事件
    if (btn->pressed || btn->released)
        mask = EPOLLIN | EPOLLRDNORM;

    return mask;
}

static const struct file_operations button_fops = {
    .owner   = THIS_MODULE,
    .open    = button_open,
    .read    = button_read,
    .release = button_release,
    .poll    = button_poll,
};

// 中断处理函数
static irqreturn_t button_isr(int irq, void *dev_id)
{
    struct button_device *btn = dev_id;
    int value;

    // 读取 GPIO 状态，判断是按下还是释放
    value = gpio_get_value(btn->gpio);

    if (value == 0) {
        // 按键按下（低电平）
        btn->pressed = 1;
        pr_info("Button pressed\n");
    } else {
        // 按键释放（高电平）
        btn->released = 1;
        pr_info("Button released\n");
    }

    // 唤醒等待队列中的进程
    wake_up_interruptible(&btn->waitq);

    return IRQ_HANDLED;
}

// 设备树匹配表
static const struct of_device_id button_of_match[] = {
    { .compatible = "vendor,my-button", },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, button_of_match);

// probe 函数
static int button_probe(struct device *dev)
{
    struct device_node *node = dev->of_node;
    int ret;
    enum gpio_flags flags;

    pr_info("Button probe called\n");

    // 分配按键设备结构体
    btn_dev = kzalloc(sizeof(struct button_device), GFP_KERNEL);
    if (!btn_dev)
        return -ENOMEM;

    btn_dev->dev = dev;

    // 从设备树获取 GPIO 编号
    btn_dev->gpio = of_get_named_gpio(node, "gpios", 0);
    if (!gpio_is_valid(btn_dev->gpio)) {
        pr_err("Invalid GPIO: %d\n", btn_dev->gpio);
        ret = btn_dev->gpio;
        goto err_gpio;
    }

    // 请求 GPIO
    ret = gpio_request(btn_dev->gpio, "button-gpio");
    if (ret) {
        pr_err("Failed to request GPIO: %d\n", ret);
        goto err_gpio_request;
    }

    // 配置 GPIO 为输入模式
    // 使用 GPIO_PULL_UP 表示内部上拉
    flags = GPIOF_IN;
    ret = gpio_direction_input(btn_dev->gpio);
    if (ret) {
        pr_err("Failed to set GPIO direction: %d\n", ret);
        goto err_gpio_direction;
    }

    // 获取中断号
    btn_dev->irq = gpio_to_irq(btn_dev->gpio);
    if (btn_dev->irq < 0) {
        pr_err("Failed to get IRQ: %d\n", btn_dev->irq);
        ret = btn_dev->irq;
        goto err_irq;
    }

    // 初始化等待队列
    init_waitqueue_head(&btn_dev->waitq);

    // 注册中断处理函数
    // IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING 表示当 GPIO
    // 处于下降沿或上升沿时触发中断
    ret = request_irq(btn_dev->irq, button_isr,
              IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
              "button-irq", btn_dev);
    if (ret) {
        pr_err("Failed to request IRQ: %d\n", ret);
        goto err_request_irq;
    }

    btn_dev->pressed = 0;
    btn_dev->released = 0;

    // 动态分配设备号
    ret = alloc_chrdev_region(&btn_dev->dev_num, 0, 1, BUTTON_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        goto err_dev_num;
    }

    pr_info("Button device major: %d, minor: %d\n",
        MAJOR(btn_dev->dev_num), MINOR(btn_dev->dev_num));

    // 初始化 cdev
    cdev_init(&btn_dev->cdev, &button_fops);
    btn_dev->cdev.owner = THIS_MODULE;

    // 添加 cdev
    ret = cdev_add(&btn_dev->cdev, btn_dev->dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto err_cdev_add;
    }

    // 创建设备类
    btn_dev->class = class_create(THIS_MODULE, BUTTON_CLASS_NAME);
    if (IS_ERR(btn_dev->class)) {
        pr_err("Failed to create class\n");
        ret = PTR_ERR(btn_dev->class);
        goto err_class;
    }

    // 创建设备
    btn_dev->device = device_create(btn_dev->class, NULL,
                    btn_dev->dev_num, NULL, BUTTON_NAME);
    if (IS_ERR(btn_dev->device)) {
        pr_err("Failed to create device\n");
        ret = PTR_ERR(btn_dev->device);
        goto err_device;
    }

    // 将 btn_dev 保存到 device 的私有数据中
    dev_set_drvdata(dev, btn_dev);

    // 创建 sysfs 属性
    ret = device_create_file(btn_dev->device, &dev_attr_button);
    if (ret) {
        pr_err("Failed to create sysfs file\n");
        goto err_sysfs;
    }

    pr_info("Button driver initialized\n");
    return 0;

err_sysfs:
    device_destroy(btn_dev->class, btn_dev->dev_num);
err_device:
    class_destroy(btn_dev->class);
err_cdev_add:
    cdev_del(&btn_dev->cdev);
    unregister_chrdev_region(btn_dev->dev_num, 1);
err_dev_num:
    free_irq(btn_dev->irq, btn_dev);
err_request_irq:
err_irq:
err_gpio_direction:
    gpio_free(btn_dev->gpio);
err_gpio_request:
err_gpio:
    kfree(btn_dev);
    return ret;
}

// remove 函数
static int button_remove(struct device *dev)
{
    if (!btn_dev)
        return -ENODEV;

    // 删除 sysfs 属性
    device_remove_file(btn_dev->device, &dev_attr_button);

    // 注销中断
    free_irq(btn_dev->irq, btn_dev);

    // 注销设备
    device_destroy(btn_dev->class, btn_dev->dev_num);
    class_destroy(btn_dev->class);
    cdev_del(&btn_dev->cdev);
    unregister_chrdev_region(btn_dev->dev_num, 1);

    // 释放 GPIO
    gpio_free(btn_dev->gpio);

    // 释放内存
    kfree(btn_dev);
    btn_dev = NULL;

    pr_info("Button driver removed\n");
    return 0;
}

// 平台驱动结构体
static struct platform_driver button_driver = {
    .probe  = button_probe,
    .remove = button_remove,
    .driver = {
        .name   = "my-button",
        .of_match_table = button_of_match,
    },
};

module_platform_driver(button_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Example Author");
MODULE_DESCRIPTION("A simple button driver with interrupt support");
MODULE_VERSION("1.0");
```

### 4.3.3 sysfs 调试接口

按键驱动的 sysfs 接口可以提供轮询方式读取按键状态，这对于调试和测试非常有用：

```c
// 显示按键状态
static ssize_t button_show(struct device *dev,
               struct device_attribute *attr, char *buf)
{
    struct button_device *btn = dev_get_drvdata(dev);
    int value;

    // 读取 GPIO 当前状态
    value = gpio_get_value(btn->gpio);

    // 假设低电平表示按下
    if (value == 0)
        return sprintf(buf, "pressed\n");
    else
        return sprintf(buf, "released\n");
}

// 定义设备属性
static DEVICE_ATTR(button, 0444, button_show, NULL);
```

使用 sysfs 接口读取按键状态：

```bash
# 读取按键状态
cat /sys/class/button_class/my_button/button
# 输出: released

# 按下按键后再次读取
cat /sys/class/button_class/my_button/button
# 输出: pressed
```

#### 用户空间测试程序

以下是一个简单的用户空间测试程序，用于测试按键驱动：

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

#define BUTTON_DEVICE "/dev/my_button"

int main(void)
{
    int fd;
    char buf[16];
    struct pollfd fds;

    // 打开设备
    fd = open(BUTTON_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    // 设置 poll 结构
    fds.fd = fd;
    fds.events = POLLIN;

    printf("Waiting for button events...\n");

    while (1) {
        // 使用 poll 等待按键事件
        int ret = poll(&fds, 1, -1);
        if (ret < 0) {
            perror("poll error");
            break;
        }

        if (fds.revents & POLLIN) {
            // 读取按键事件
            memset(buf, 0, sizeof(buf));
            read(fd, buf, sizeof(buf));
            printf("Button event: %s", buf);
        }
    }

    close(fd);
    return 0;
}
```

编译和运行：

```bash
# 编译测试程序
gcc -o test_button test_button.c

# 运行测试程序
./test_button
```

## 本章面试题

### 面试题1：字符设备和块设备的主要区别是什么？

**参考答案：**

字符设备和块设备是 Linux 系统中两种主要的设备类型，它们在数据传输方式、访问模式、使用场景等方面存在显著差异。

**1. 数据传输方式的区别**

字符设备以字符为单位进行数据传输，没有缓冲区的概念。数据像字节流一样被连续处理，例如键盘、鼠标、串口等设备。字符设备的 read 和 write 操作通常是同步的，每次调用都会直接访问硬件。

块设备则以块为单位进行数据传输，每个块通常为 512 字节或更大。块设备有缓冲区（buffer cache）的概念，可以一次性读取或写入多个块。磁盘、U盘、内存盘等存储设备属于块设备。

**2. 访问模式的区别**

字符设备支持顺序访问，可以随机读写字节流，但通常是连续处理。块设备支持随机访问，可以任意寻址到任何块位置进行读写，这使得块设备适合存储需要随机访问的数据。

**3. 使用场景的区别**

字符设备通常用于不需要文件系统支持的简单硬件，或者需要实时响应的场景，如串口通信、显示控制、GPIO 等。块设备主要用于需要文件系统支持的存储设备，所有文件系统都建立在块设备之上。

**4. 设备节点的区别**

字符设备的主设备号标识驱动，次设备号用于区分同一驱动管理的多个设备。块设备也使用主次设备号的机制，但块设备通常通过文件系统访问，而字符设备通过设备节点直接访问。

**5. 内核子系统的区别**

字符设备通过 cdev 结构体和 file_operations 操作集进行管理，内核提供字符设备子系统的接口。块设备通过 gendisk 结构体和 block_device_operations 操作集进行管理，内核提供块设备子系统和 I/O 调度器。

**6. 性能特点的区别**

字符设备的性能通常取决于硬件本身的速度，没有缓存机制。块设备则可以通过 I/O 调度器进行排序和合并，提高磁盘访问效率。

**代码层面的区别**

从驱动开发的角度，字符设备和块设备的主要区别在于：

```c
// 字符设备使用 cdev 和 file_operations
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;
    dev_t dev;
    unsigned int count;
    struct list_head list;
};

// 块设备使用 gendisk 和 block_device_operations
struct block_device_operations {
    int (*open) (struct block_device *, fmode_t);
    int (*release) (struct gendisk *, fmode_t);
    int (*ioctl) (struct block_device *, fmode_t, unsigned int, unsigned long);
    // ...
};
```

### 面试题2：描述字符设备驱动注册的过程。

**参考答案：**

字符设备驱动的注册过程是 Linux 驱动开发的核心知识点。整个过程可以分为以下几个主要步骤，每个步骤都有特定的内核函数来完成相应的工作。

**1. 分配设备号**

设备号是 Linux 系统中标识字符设备的唯一标识符，由主设备号和次设备号组成。在注册字符设备之前，首先需要分配设备号。

动态分配方式（推荐）：

```c
dev_t dev_num;
int ret;

ret = alloc_chrdev_region(&dev_num, 0, 1, "my_device");
if (ret < 0) {
    pr_err("Failed to allocate device number\n");
    return ret;
}
```

`alloc_chrdev_region` 函数会从内核的空闲设备号池中动态分配一个可用的设备号。第一个参数是输出参数，用于返回分配到的起始设备号；第二个参数是起始次设备号；第三个参数是连续设备号的数量；第四个参数是设备名称，用于在 /proc/devices 中显示。

静态分配方式（已知设备号时使用）：

```c
dev_t dev_num = MKDEV(240, 0);  // 主设备号 240，次设备号 0
int ret = register_chrdev_region(dev_num, 1, "my_device");
```

**2. 初始化 cdev**

分配设备号后，需要初始化 cdev 结构体：

```c
struct cdev my_cdev;
struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    .release = my_release,
};

cdev_init(&my_cdev, &my_fops);
my_cdev.owner = THIS_MODULE;
```

`cdev_init` 函数完成以下初始化工作：清零 cdev 结构体、初始化内嵌的 kobject、初始化链表、设置文件操作集和拥有者模块。

**3. 添加 cdev 到系统**

初始化 cdev 后，需要将其添加到内核的字符设备系统中：

```c
int ret = cdev_add(&my_cdev, dev_num, 1);
if (ret < 0) {
    pr_err("Failed to add cdev\n");
    goto err_cdev_add;
}
```

`cdev_add` 函数将 cdev 添加到内核的字符设备映射表中。从这个时刻起，设备就可以被访问了。需要注意的是，`cdev_add` 成功返回后，驱动的 file_operations 会被内核调用，因此在此之前必须确保所有初始化工作都已完成。

**4. 创建设备类**

为了支持 udev 自动创建设备节点，需要创建设备类：

```c
struct class *my_class;
my_class = class_create(THIS_MODULE, "my_class");
if (IS_ERR(my_class)) {
    pr_err("Failed to create class\n");
    ret = PTR_ERR(my_class);
    goto err_class_create;
}
```

`class_create` 函数在 /sys/class/ 目录下创建一个新的类目录。这个类用于组织具有相似功能的设备。

**5. 创建设备节点**

最后，创建设备节点：

```c
struct device *my_device;
my_device = device_create(my_class, NULL, dev_num, NULL, "my_device");
if (IS_ERR(my_device)) {
    pr_err("Failed to create device\n");
    ret = PTR_ERR(my_device);
    goto err_device_create;
}
```

`device_create` 函数完成以下工作：在 /sys/class/my_class/ 目录下创建一个设备目录；创建 dev 属性文件，包含设备号信息；发送 uevent 事件给 udev，udev 根据规则文件创建设备节点 /dev/my_device。

**完整的注册流程图**

```
alloc_chrdev_region() / register_chrdev_region()
    ↓
cdev_init()
    ↓
cdev_add()
    ↓
class_create()
    ↓
device_create()
    ↓
udev 创建设备节点 /dev/xxx
```

**完整的注销流程**

```c
static void __exit my_exit(void)
{
    // 逆向注销
    device_destroy(my_class, dev_num);       // 销毁设备
    class_destroy(my_class);                    // 销毁类
    cdev_del(&my_cdev);                       // 删除 cdev
    unregister_chrdev_region(dev_num, 1);      // 释放设备号

    pr_info("Driver removed\n");
}
```

**注意事项**

在整个注册过程中，任何步骤失败都需要进行正确的错误处理，回滚已经分配的资源。这包括释放已分配的设备号、删除已添加的 cdev、销毁已创建的类和设备等。

另外，从 Linux 6.4 版本开始，`class_create` 的函数签名发生了变化，不再接受 owner 参数。如果需要兼容新旧版本，可以使用条件编译或提供 wrapper 函数。

理解字符设备驱动的注册过程，对于调试驱动问题和理解 Linux 设备模型都至关重要。整个注册流程体现了 Linux 设备模型的层次化设计思想，从底层的设备号管理，到中层的 cdev 管理，再到上层的类设备管理，每一层都有清晰的职责划分。

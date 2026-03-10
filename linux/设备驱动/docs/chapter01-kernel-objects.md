# 第1章 底层核心机制

Linux 驱动模型的核心建立在 kobject、kset 和 ktype 这三个基础概念之上。它们构成了内核对象系统（Kernel Object System）的基石，为整个设备模型提供了统一的抽象层。本章将深入剖析这三个核心组件的原理、结构和用法，帮助读者建立对 Linux 设备模型底层的深刻理解。

## 1.1 kobject 核心对象

### 1.1.1 kobject 概述

kobject 是 Linux 内核设备模型中最基础的构建块（building block）。它的设计目标是提供一种统一的面向对象机制，使得内核中的各种对象（如设备、驱动、总线等）能够以一致的方式进行管理。kobject 本身并不是一个独立使用的结构体，而是一个被嵌入到其他更大结构体中的"元对象"（meta-object），它为这些结构体提供了通用的功能支持。

kobject 的核心功能可以归纳为以下几个方面：

**引用计数管理**：kobject 内嵌了 kref 结构体，用于实现引用计数机制。当一个对象被引用时，引用计数增加；当引用被释放时，引用计数减少。当引用计数降为 0 时，对象会被自动释放。这种机制有效地避免了内存泄漏问题，是内核对象生命周期管理的基石。

**层次结构管理**：通过 parent 指针和 entry 链表，kobject 可以形成树形的层次结构。这种层次结构直接映射到 sysfs 文件系统的目录结构，使得用户空间可以通过文件系统浏览内核对象的组织关系。

**sysfs 集成**：每个 kobject 在 sysfs 文件系统中都对应一个目录。目录中包含了对象的属性文件（attribute），用户空间可以通过读取这些属性获取对象的状态信息，也可以通过写入属性来修改对象的配置。

**热插拔事件支持**：当 kobject 的状态发生变化（如添加、移除）时，可以触发 uevent 事件。udev 守护进程收到这些事件后，会执行相应的操作，如创建设备节点、设置权限等。这是 Linux 实现设备热插拔的基础机制。

理解 kobject 的最佳方式是将它看作一个"基础设施提供者"。大多数情况下，开发者不会直接创建和使用 kobject 结构体，而是创建包含 kobject 的更高级结构体（如 device、driver、bus_type 等）。这些高级结构体通过内嵌的 kobject 自动获得了上述所有功能。

### 1.1.2 kobject 结构体定义

kobject 的结构体定义位于 Linux 内核源码的 `include/linux/kobject.h` 文件中。以下是 Linux 5.x 内核中 kobject 的完整结构体定义：

```c
struct kobject {
    const char              *name;              // 对象的名称，对应 sysfs 目录名
    struct list_head        entry;             // 链表节点，用于加入到 kset 的链表中
    struct kobject         *parent;           // 指向父对象的指针，形成层次结构
    struct kset            *kset;              // 指向所属 kset 的指针
    struct kobj_type       *ktype;            // 指向对象类型定义的指针
    struct kref            kref;               // 引用计数结构体
    struct sysfs_dirent    *sd;               // 指向 sysfs 目录项的指针
    unsigned int state_initialized:1;         // 对象是否已初始化的标志
    unsigned int state_in_sysfs:1;            // 对象是否已在 sysfs 中显示
    unsigned int state_add_uevent_sent:1;     // 是否已发送添加 uevent 事件
    unsigned int state_remove_uevent_sent:1; // 是否已发送移除 uevent 事件
    unsigned int uevent_suppress:1;           // 是否压制 uevent 事件
    struct delayed_work    deferred;          // 延迟工作队列，用于延迟清理
    ...
};
```

下面对 kobject 的各个成员进行详细解析：

**name**：指向对象名称的字符串指针。这个名称非常重要，它直接决定了对象在 sysfs 中的目录名称。例如，如果一个 kobject 的 name 成员指向字符串 "block"，那么在 sysfs 中就会创建一个名为 "block" 的目录。

**entry**：这是一个 list_head 类型的链表节点。当 kobject 属于某个 kset 时，它会通过这个节点加入到 kset 的链表中。链表的使用使得 kset 可以高效地遍历其所有成员。

**parent**：指向父 kobject 的指针。这个指针定义了对象在层次结构中的位置。如果 parent 为 NULL 且 kset 不为空，对象会使用 kset 的 kobject 作为其父对象；如果两者都为 NULL，则对象没有父对象（这种情况通常用于根级别的对象）。

**kset**：指向该对象所属 kset 的指针。kset 是 kobject 的集合，提供对一组相关 kobject 的统一管理。一个 kobject 可以属于某个 kset，也可以不属于（此时 kset 成员为 NULL）。

**ktype**：指向 kobj_type 结构体的指针。ktype 定义了 kobject 的类型特性，包括释放函数、sysfs 操作函数、默认属性等。ktype 使得不同类型的对象可以有不同的行为。

**kref**：这是引用计数的核心结构体。kref 内部只有一个 atomic_t 类型的计数器，用于记录对象被引用的次数。引用计数机制是内核对象生命周期管理的关键。

**sd**：指向 sysfs_dirent 结构体的指针。sysfs 是内核用于导出对象层次结构的虚拟文件系统，每个 kobject 在 sysfs 中都对应一个目录项，sd 指针指向这个目录项结构。

**state_initialized**：一个位域标志，表示 kobject 是否已经完成初始化。在调用 kobject_init() 之后，此标志被设置为 1。

**state_in_sysfs**：表示 kobject 是否已经在 sysfs 中创建了对应的目录。只有调用 kobject_add() 成功后，此标志才会被设置。

**state_add_uevent_sent 和 state_remove_uevent_sent**：这两个标志分别记录是否已经发送了添加和移除的 uevent 事件。uevent 事件用于通知用户空间有设备的热插拔发生。

**deferred**：这是一个 delayed_work 结构体，用于延迟工作的调度。有些清理操作不能在对象释放的立即执行，需要延迟到安全的时间点执行。

### 1.1.3 kobject 核心函数

kobject 提供了一系列核心函数来管理对象的生命周期。以下是这些函数的详细解析：

#### 1. kobject_init() - 初始化 kobject

kobject_init() 函数用于初始化一个 kobject 结构体。在使用 kobject 之前，必须先调用此函数进行初始化。函数定义如下：

```c
// 文件位置：lib/kobject.c
void kobject_init(struct kobject *kobj, struct kobj_type *ktype)
{
    if (!kobj)
        return;

    // 初始化引用计数为 1
    kref_init(&kobj->kref);

    // 初始化链表节点
    INIT_LIST_HEAD(&kobj->entry);

    // 设置 ktype
    kobj->ktype = ktype;

    // 设置初始状态标志
    kobj->state_initialized = 1;
    kobj->state_in_sysfs = 0;
    kobj->state_add_uevent_sent = 0;
    kobj->state_remove_uevent_sent = 0;
    kobj->uevent_suppress = 0;
}
```

从代码可以看出，kobject_init() 做了以下几项工作：

1. 初始化引用计数，将其设置为 1。这意味着对象在初始化后至少有一个引用。
2. 初始化链表节点 entry，使其可以加入到某个链表中。
3. 设置 ktype，指定对象的类型定义。
4. 初始化各个状态标志，将 state_initialized 设置为 1，表示对象已经初始化。

需要注意的是，kobject_init() 不会分配内存，调用者需要先分配 kobject 的内存空间。通常情况下，kobject 是作为更大结构体的成员存在的，由包含它的结构体负责内存分配。

#### 2. kobject_add() - 添加到内核对象层次结构

kobject_add() 函数用于将 kobject 添加到内核的对象层次结构中，并在 sysfs 文件系统中创建对应的目录。这个函数是 kobject 生命周期中非常重要的一步。函数定义如下：

```c
// 文件位置：lib/kobject.c
int kobject_add(struct kobject *kobj, struct kobject *parent, const char *fmt, ...)
{
    va_list args;
    int retval;

    if (!kobj)
        return -EINVAL;

    // 检查是否已经初始化
    if (!kobj->state_initialized) {
        pr_err("kobject_add: called before init!\n");
        return -EINVAL;
    }

    // 如果传入了名称，先设置名称
    if (fmt) {
        va_start(args, fmt);
        retval = kobject_set_name(kobj, fmt, args);
        va_end(args);
        if (retval) {
            pr_err("kobject_add: cannot set name properly\n");
            return retval;
        }
    }

    // 设置父对象
    if (!kobj->parent)
        kobj->parent = parent;

    // 创建 sysfs 目录
    retval = sysfs_create_dir(kobj);
    if (retval) {
        pr_err("kobject_add: failed to create sysfs directory!\n");
        return retval;
    }

    // 设置状态标志
    kobj->state_in_sysfs = 1;

    // 将对象添加到 kset 的链表中（如果有 kset 的话）
    kobj_kset_join(kobj);

    // 设置对象名称在 sysfs 中的链接
    kobject_init_uevent_env(kobj);

    // 发送添加 uevent 事件
    kobject_uevent(kobj, KOBJ_ADD);

    return 0;
}
```

kobject_add() 的执行流程如下：

1. 参数验证：检查 kobj 是否有效，是否已经初始化。
2. 设置名称：如果传入了格式化字符串，使用 kobject_set_name() 设置对象名称。
3. 设置父对象：如果 kobject 还没有父对象（parent 为 NULL），使用传入的 parent 参数。
4. 创建 sysfs 目录：调用 sysfs_create_dir() 在 sysfs 中创建对应的目录。
5. 加入 kset：如果 kobject 属于某个 kset，调用 kobj_kset_join() 将其加入到 kset 的链表中。
6. 初始化 uevent 环境：设置对象的热插拔事件环境变量。
7. 发送事件：调用 kobject_uevent() 发送 KOBJ_ADD 事件，通知用户空间有新对象添加。

#### 3. kobject_del() - 从层次结构中移除

kobject_del() 函数用于从内核对象层次结构中移除 kobject，并删除 sysfs 中对应的目录。函数定义如下：

```c
// 文件位置：lib/kobject.c
void kobject_del(struct kobject *kobj)
{
    if (!kobj)
        return;

    // 如果已经在 sysfs 中显示，发送移除事件
    if (kobj->state_in_sysfs) {
        kobject_uevent(kobj, KOBJ_REMOVE);
        sysfs_remove_dir(kobj);
        kobj->state_in_sysfs = 0;
    }

    // 从 kset 的链表中移除
    kobj_kset_leave(kobj);

    // 清空父对象指针
    kobj->parent = NULL;
}
```

kobject_del() 的执行流程如下：

1. 发送移除事件：如果对象之前已经在 sysfs 中显示，先发送 KOBJ_REMOVE 事件通知用户空间。
2. 删除 sysfs 目录：调用 sysfs_remove_dir() 删除 sysfs 中对应的目录。
3. 离开 kset：如果对象属于某个 kset，调用 kobj_kset_leave() 将其从 kset 的链表中移除。
4. 清空父对象：将 parent 指针设置为 NULL。

需要注意的是，kobject_del() 不会释放 kobject 本身的内存，也不会减少引用计数。调用者需要通过 kobject_put() 来管理引用计数，只有当引用计数降为 0 时，对象才会被真正释放。

#### 4. kobject_get() / kobject_put() - 引用计数管理

引用计数是 Linux 内核对象生命周期管理的核心机制。kobject_get() 用于增加对象的引用计数，kobject_put() 用于减少引用计数。

**kobject_get() 函数**：

```c
// 文件位置：lib/kobject.c
struct kobject *kobject_get(struct kobject *kobj)
{
    if (!kobj)
        return NULL;

    // 增加引用计数
    kref_get(&kobj->kref);

    return kobj;
}
```

kobject_get() 的实现非常简单，它只是调用 kref_get() 来增加引用计数，并返回传入的 kobject 指针。这个函数通常在需要引用某个对象时调用，例如在数据结构中保存对象指针时。

**kobject_put() 函数**：

```c
// 文件位置：lib/kobject.c
void kobject_put(struct kobject *kobj)
{
    if (!kobj)
        return;

    // 减少引用计数
    if (kref_put(&kobj->kref, kobject_release)) {
        // 如果引用计数降为 0，调用 kobject_release 进行清理
        kobject_cleanup(kobj);
    }
}
```

kobject_put() 调用 kref_put() 来减少引用计数。如果引用计数降为 0，kref_put() 会返回 true，此时会调用 kobject_cleanup() 执行清理工作。

**kobject_release() 和 kobject_cleanup() 函数**：

```c
// 文件位置：lib/kobject.c
static void kobject_release(struct kref *kref)
{
    // kref 包含在 kobject 中，通过 container_of 获取 kobject 指针
    struct kobject *kobj = container_of(kref, struct kobject, kref);

    // 调用清理函数
    kobject_cleanup(kobj);
}

void kobject_cleanup(struct kobject *kobj)
{
    struct kobj_type *t = kobj->ktype;
    struct sysfs_ops *sops = t ? t->sysfs_ops : NULL;

    // 如果有 release 函数，调用它
    if (t && t->release)
        t->release(kobj);

    // 释放 sysfs 相关资源
    ...
}
```

当引用计数降为 0 时，kobject_cleanup() 会被调用。这个函数会执行以下操作：

1. 如果对象的 ktype 中定义了 release 函数，调用它来释放对象占用的资源。这是释放对象内存的主要地方。
2. 清理 sysfs 相关资源。

### 1.1.4 kobject 生命周期

理解 kobject 的生命周期对于正确使用内核对象系统至关重要。kobject 的生命周期可以分为以下几个阶段：

#### 1. 创建阶段

kobject 的创建通常不是通过单独的 alloc 函数完成的，而是通过包含它的父结构体间接创建的。例如，当调用 device_register() 注册一个设备时，设备结构体内的 kobject 会被初始化。

在实际使用中，kobject 通常作为更大结构体的第一个成员嵌入：

```c
// 典型用法：kobject 作为结构体的第一个成员
struct my_device {
    struct kobject kobj;        // kobject 必须是第一个成员
    char *name;
    int status;
    ...
};

struct my_device *dev;
dev = kmalloc(sizeof(*dev), GFP_KERNEL);
if (!dev)
    return -ENOMEM;

// 直接初始化 kobject 成员
dev->kobj.parent = parent_kobj;
kobject_init(&dev->kobj, &my_ktype);
```

将 kobject 放在结构体的第一个位置是一种常见且推荐的做法，这样可以通过简单的指针类型转换从 kobject 指针获取包含它的结构体指针。这种技巧在内核代码中广泛使用。

#### 2. 初始化阶段

在创建 kobject 后，必须调用 kobject_init() 进行初始化。初始化工作包括设置引用计数、初始化链表节点、设置 ktype 等。

```c
// 初始化 kobject
kobject_init(&my_kobj, &my_ktype);
```

#### 3. 添加阶段

初始化完成后，需要调用 kobject_add() 将对象添加到层次结构中。这个函数会：

- 在 sysfs 中创建对应的目录
- 将对象加入到 kset 的链表中（如果指定了 kset）
- 设置父对象关系
- 发送 KOBJ_ADD 事件通知用户空间

```c
// 添加 kobject 到层次结构
ret = kobject_add(&my_kobj, parent_kobj, "my_device");
if (ret) {
    pr_err("Failed to add kobject: %d\n", ret);
    goto err;
}
```

#### 4. 使用阶段

在对象被添加到层次结构后，它就处于"使用阶段"。此时：

- 对象可以通过 sysfs 被用户空间访问
- 对象可以接收和响应 uevent 事件
- 其他代码可以通过 kobject_get() 引用该对象

在这个阶段，开发者需要小心处理引用计数。当在数据结构中保存对象指针时，应该调用 kobject_get() 增加引用；当不再需要该引用时，应该调用 kobject_put() 释放引用。

#### 5. 删除阶段

当需要移除对象时，调用 kobject_del() 将对象从层次结构中删除。这个函数会：

- 发送 KOBJ_REMOVE 事件
- 删除 sysfs 目录
- 从 kset 链表中移除

```c
// 从层次结构中删除
kobject_del(&my_kobj);
```

#### 6. 释放阶段

当对象的引用计数降为 0 时，kobject_put() 会自动调用 ktype 中定义的 release 函数来释放对象。release 函数的典型实现是释放包含 kobject 的整个结构体的内存：

```c
// 定义 release 函数
static void my_release(struct kobject *kobj)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);

    // 释放设备相关的资源
    kfree(dev->name);
    // 释放整个结构体
    kfree(dev);
}

// 定义 ktype
static struct kobj_type my_ktype = {
    .release = my_release,
    .sysfs_ops = &my_sysfs_ops,
    .default_attrs = my_default_attrs,
};
```

整个生命周期的流程可以用下图表示：

```
创建 → 初始化 → 添加 → 使用 → 删除 → 释放
  ↓        ↓        ↓      ↓       ↓       ↓
 kmalloc kobject_ kobject_  ...   kobject_ release
        init()    add()          del()   函数
```

理解这个生命周期对于避免内存泄漏和 Use-After-Free 错误非常重要。在实际开发中，常见的错误包括：

1. 忘记调用 kobject_put() 释放引用，导致内存泄漏
2. 在对象被删除后仍然访问，导致 Use-After-Free
3. 没有正确定义 release 函数，导致对象无法被正确释放

## 1.2 kset 对象集合

### 1.2.1 kset 概述

kset（kernel set）是 Linux 内核对象系统中用于组织和管理一组相关 kobject 的容器结构体。它的设计理念类似于面向对象中的"集合"概念——将一组具有相似特征或用途的对象组织在一起进行统一管理。

在 Linux 设备模型中，kset 扮演着重要的角色。所有的块设备都组织在 block kset 中，所有的网络设备组织在 net kset 中，USB 设备组织在 usb kset 中。通过 kset，开发者可以将相关的设备归为一类，便于统一管理和操作。

kset 的主要功能包括：

**层次结构组织**：kset 本身包含一个 kobject，因此可以像普通 kobject 一样加入到层次结构中。kset 下的所有 kobject 都会以这个 kobject 为父对象，形成清晰的层次关系。

**统一引用计数**：kset 可以作为一组 kobject 的"代表"，当需要增加或减少整组对象的引用计数时，可以通过 kset 统一操作。

**热插拔事件分发**：kset 可以注册自己的 uevent 操作函数（kset_uevent_ops），当组内任何对象发生热插拔事件时，可以统一处理这些事件。

**链表管理**：kset 维护一个链表，包含其所有成员 kobject。这使得可以方便地遍历整个集合。

### 1.2.2 kset 结构体定义

kset 的结构体定义同样位于 Linux 内核源码的 `include/linux/kobject.h` 文件中：

```c
struct kset {
    struct list_head    list;               // 包含的所有 kobject 的链表头
    spinlock_t         list_lock;          // 保护链表的锁
    struct kobject     kobj;                // kset 自身的 kobject
    const struct kset_uevent_ops *uevent_ops;  // 热插拔事件操作函数
};
```

下面对 kset 的各个成员进行详细解析：

**list**：这是一个 list_head 类型的链表头，用于组织 kset 的所有成员。每个加入 kset 的 kobject 都会通过其 entry 成员挂入这个链表。通过这个链表，kset 可以高效地遍历所有成员。

**list_lock**：这是一个自旋锁，用于保护链表操作的并发安全。由于内核对象系统可能被多个执行路径同时访问，需要使用锁来防止竞争条件。

**kobj**：这是 kset 自身的 kobject。通过这个嵌入的 kobject，kset 可以参与到对象层次结构中，可以拥有自己的父对象、名称等。这个 kobject 也决定了 kset 在 sysfs 中的目录位置。

**uevent_ops**：这是一个指向 kset_uevent_ops 结构体的指针，用于定义 kset 的热插拔事件处理函数。当组内的 kobject 发生添加、移除等事件时，kset 可以选择拦截或修改这些事件。

kset_uevent_ops 结构体的定义如下：

```c
// 文件位置：include/linux/kobject.h
struct kset_uevent_ops {
    int (* const filter)(struct kset *kset, struct kobject *kobj);
    const char *(* const name)(struct kset *kset, struct kobject *kobj);
    int (* const uevent)(struct kset *kset, struct kobject *kobj,
              struct kobj_uevent_env *env);
};
```

这个结构体包含三个函数指针：

- **filter**：过滤器函数，用于决定是否允许某个 kobject 的事件发送到用户空间。返回 0 表示允许，返回非 0 表示过滤（阻止）该事件。
- **name**：名称函数，可以为事件设置额外的环境变量。
- **uevent**：事件处理函数，可以在事件发送前修改环境变量或执行其他操作。

### 1.2.3 kset 与 kobject 的关系

kset 与 kobject 之间的关系是理解 Linux 内核对象系统的关键。这两个概念既有区别又有联系：

**kset 包含 kobject**：从结构体定义可以看出，kset 内部包含了一个 struct kobject kobj 成员。这意味着每个 kset 本身也是一个 kobject，具有 kobject 的所有功能。它可以加入到其他 kset 的层次结构中，可以在 sysfs 中创建自己的目录，可以处理热插拔事件。

**kobject 可以属于 kset**：每个 kobject 结构体都有一个 kset 成员，指向它所属的 kset。当 kobject 被添加到某个 kset 时，它会通过 entry 链表节点加入到 kset 的链表中。

```
kset 结构
+------------------+
| list_head list   |----> +---------+    +---------+
+------------------+      | kobject |    | kobject |
| spinlock_t lock  |      | entry   |----| entry   |
+------------------+      +---------+    +---------+
| struct kobject   |              |            |
|     kobj          |              v            v
+------------------+         (更多成员...)
| uevent_ops       |
+------------------+
```

如上图所示，kset 通过 list 链表管理其所有成员 kobject。每个成员 kobject 通过其 entry 成员链接到链表中。同时，kset 自身的 kobject 可以作为父对象，使其成员 kobject 在 sysfs 中形成层次结构。

**父子关系的确定**：当一个 kobject 被添加到某个 kset 时，如果该 kobject 没有明确设置父对象（parent 为 NULL），系统会自动使用 kset 的 kobj 作为其父对象。这使得同一 kset 下的所有 kobject 在 sysfs 中都以 kset 的目录作为父目录。

```c
// kobject 添加到 kset 时自动设置父对象的逻辑
static void kobj_kset_join(struct kobject *kobj)
{
    struct kset *kset = kobj->kset;

    if (!kobj->parent)
        kobj->parent = &kset->kobj;

    list_add_tail(&kobj->entry, &kset->list);
}
```

从这段代码可以看出，当 kobject 没有设置父对象时，会自动将其父对象设置为所属 kset 的 kobject。这就是为什么同一个 kset 下的所有对象会在同一个目录下的原因。

**典型使用场景**：在实际开发中，kset 通常用于将相关类型的对象组织在一起。例如，内核中的 block kset 包含了所有的块设备：

```c
// 内核中的 block kset 定义（简化版）
struct kset block_kset = {
    .list = LIST_HEAD_INIT(block_kset.list),
    .kobj = {
        .name = "block",
        .kset = NULL,    // block 是顶层 kset
    },
    .uevent_ops = &block_kset_uevent_ops,
};
```

当开发者创建一个块设备时（例如一个磁盘分区），该设备的 kobject 会被添加到 block_kset 中，从而在 sysfs 的 /sys/block/ 目录下显示。

### 1.2.4 kset 核心函数

kset 提供了一系列核心函数来管理其成员：

#### 1. kset_create() - 创建 kset

```c
// 文件位置：lib/kobject.c
struct kset *kset_create(const char *name,
              struct kobject *parent,
              const struct kset_uevent_ops *uevent_ops)
{
    struct kset *kset;
    int retval;

    // 分配内存
    kset = kzalloc(sizeof(*kset), GFP_KERNEL);
    if (!kset)
        return NULL;

    // 设置父对象
    kobject_init(&kset->kobj, &kset_ktype);
    kset->kobj.parent = parent;

    // 设置名称
    retval = kobject_set_name(&kset->kobj, name);
    if (retval) {
        kfree(kset);
        return NULL;
    }

    // 设置 uevent 操作
    kset->uevent_ops = uevent_ops;

    // 初始化链表
    INIT_LIST_HEAD(&kset->list);
    spin_lock_init(&kset->list_lock);

    return kset;
}
```

#### 2. kset_register() - 注册 kset

```c
// 文件位置：lib/kobject.c
void kset_register(struct kset *kset)
{
    if (!kset)
        return;

    // 初始化并添加到层次结构
    kobject_init(&kset->kobj, &kset_ktype);
    kobject_add(&kset->kobj);

    // 创建 uevent 属性文件
    kset_uevent_ops_init(kset);
}
```

#### 3. kset_unregister() - 注销 kset

```c
// 文件位置：lib/kobject.c
void kset_unregister(struct kset *kset)
{
    if (!kset)
        return;

    // 从层次结构中删除
    kobject_del(&kset->kobj);
    kobject_put(&kset->kobj);
}
```

#### 4. kset_find_obj() - 查找成员

```c
// 文件位置：lib/kobject.c
struct kobject *kset_find_obj(struct kset *kset, const char *name)
{
    struct kobject *k;
    struct kobject *ret = NULL;

    spin_lock(&kset->list_lock);

    // 遍历 kset 的成员链表
    list_for_each_entry(k, &kset->list, entry) {
        if (kobject_name(k) && !strcmp(kobject_name(k), name)) {
            ret = kobject_get(k);
            break;
        }
    }

    spin_unlock(&kset->list_lock);

    return ret;
}
```

## 1.3 ktype 对象类型

### 1.3.1 ktype 概述

ktype（kernel object type）是 Linux 内核对象系统中定义 kobject 类型特性的结构体。它指定了同类型对象的共同行为和属性，包括对象的释放函数、sysfs 操作的实现、以及默认属性等。

如果说 kobject 是"对象实例"，那么 ktype 就是"对象的类定义"。这种设计类似于面向对象编程中的类（class）概念：所有同类对象共享相同的属性定义和行为实现，但每个对象实例有自己的状态数据。

ktype 的主要功能包括：

**资源释放**：定义 release 函数，用于在对象的引用计数降为 0 时释放对象占用的资源。这是 ktype 最重要的功能之一。

**sysfs 属性操作**：定义 sysfs_ops 结构体，指定如何读取和写入 sysfs 属性文件。这些函数决定了用户空间如何与内核对象进行交互。

**默认属性**：定义 default_attrs 数组，指定对象在 sysfs 中的默认属性文件。这些属性会在对象创建时自动创建。

**命名空间支持**：可选地定义子命名空间的处理函数，用于在 sysfs 中创建独立的命名空间视图。

### 1.3.2 ktype 结构体定义

ktype 的结构体定义位于 Linux 内核源码的 `include/linux/kobject.h` 文件中：

```c
struct kobj_type {
    void (*release)(struct kobject *kobj);                      // 释放函数
    struct sysfs_ops    *sysfs_ops;                            // sysfs 操作函数
    struct attribute   **default_attrs;                        // 默认属性数组
    struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);  // 子命名空间类型
    const void *(*namespace)(struct kobject *kobj);            // 命名空间回调
    void (*get_ownership)(struct kobject *kobj, kuid_t *uid, kgid_t *gid);  // 获取所有权
};
```

下面对 kobj_type 的各个成员进行详细解析：

**release**：这是一个函数指针，指向对象的释放函数。当对象的引用计数降为 0 时，这个函数会被调用来释放对象占用的资源。这是 ktype 最核心的成员，每个需要被正确释放的 kobject 都必须指定一个有效的 release 函数。

release 函数的典型实现是释放包含 kobject 的整个结构体：

```c
void my_device_release(struct kobject *kobj)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);

    // 释放设备持有的资源
    if (dev->name)
        kfree(dev->name);
    if (dev->data)
        kfree(dev->data);

    // 释放设备结构体本身
    kfree(dev);
}
```

**sysfs_ops**：指向 sysfs_ops 结构体的指针，定义了如何读取和写入 sysfs 属性文件。这个成员决定了用户空间与内核对象交互的方式。

sysfs_ops 的定义如下：

```c
// 文件位置：include/linux/kobject.h
struct sysfs_ops {
    ssize_t (*show)(struct kobject *kobj, struct attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
             const char *buf, size_t count);
};
```

- **show**：读取属性值的函数。当用户空间读取 sysfs 中的属性文件时，会调用这个函数。函数需要将属性的值格式化为字符串并写入 buf。
- **store**：写入属性值的函数。当用户空间向 sysfs 中的属性文件写入数据时，会调用这个函数。函数需要解析输入的字符串并更新对象的属性。

```c
// sysfs_ops 的典型实现示例
static ssize_t my_attr_show(struct kobject *kobj, struct attribute *attr,
               char *buf)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);
    struct attribute *attribute = attr;

    if (!strcmp(attribute->name, "status"))
        return sprintf(buf, "%d\n", dev->status);
    if (!strcmp(attribute->name, "name"))
        return sprintf(buf, "%s\n", dev->name);

    return -EIO;
}

static ssize_t my_attr_store(struct kobject *kobj, struct attribute *attr,
                const char *buf, size_t count)
{
    struct my_device *dev = container_of(kobj, struct my_device, kobj);
    struct attribute *attribute = attr;

    if (!strcmp(attribute->name, "status")) {
        if (kstrtoint(buf, 10, &dev->status))
            return -EINVAL;
    }

    return count;
}

static struct sysfs_ops my_sysfs_ops = {
    .show = my_attr_show,
    .store = my_attr_store,
};
```

**default_attrs**：指向 attribute 结构体数组的指针，定义了对象的默认属性。这些属性会在对象添加到 sysfs 时自动创建。

attribute 结构体的定义如下：

```c
// 文件位置：include/linux/kobject.h
struct attribute {
    const char      *name;        // 属性名称，对应 sysfs 中的文件名
    umode_t         mode;         // 属性文件的权限位
};
```

default_attrs 数组必须以 NULL 元素结尾：

```c
// 定义默认属性
static struct attribute my_attrs[] = {
    {
        .name = "status",
        .mode = S_IRUGO | S_IWUSR,
    },
    {
        .name = "name",
        .mode = S_IRUGO,
    },
    {
        .name = "enable",
        .mode = S_IRUGO | S_IWUSR,
    },
    NULL,  // 数组必须以 NULL 结尾
};
```

**child_ns_type**：函数指针，返回子对象的命名空间类型。在某些场景下，需要为不同层次的 kobject 创建不同的命名空间视图。

**namespace**：函数指针，返回用于 sysfs 目录的命名空间指针。如果返回 NULL，则使用默认的全局命名空间。

**get_ownership**：函数指针，用于确定在创建 sysfs 文件时使用的文件所有权（UID/GID）。这允许对象自定义其 sysfs 文件的访问权限。

### 1.3.3 ktype 的使用示例

在实际开发中，开发者需要为每种不同类型的对象定义一个 ktype。以下是一个完整的示例，展示了如何定义和使用 ktype：

```c
// 1. 定义包含 kobject 的设备结构体
struct my_device {
    struct kobject kobj;
    char *name;
    int status;
    bool enabled;
    struct list_head node;  // 用于链表
};

#define to_my_device(obj) container_of(obj, struct my_device, kobj)

// 2. 定义属性结构体（可选，用于更精细的属性控制）
struct my_attribute {
    struct attribute attr;
    ssize_t (*show)(struct my_device *dev, char *buf);
    ssize_t (*store)(struct my_device *dev, const char *buf, size_t count);
};

#define to_my_attr(attr) container_of(attr, struct my_attribute, attr)

// 3. 实现 show 和 store 函数
static ssize_t status_show(struct my_device *dev, char *buf)
{
    return sprintf(buf, "%d\n", dev->status);
}

static ssize_t status_store(struct my_device *dev, const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    dev->status = val;
    return count;
}

static ssize_t enabled_show(struct my_device *dev, char *buf)
{
    return sprintf(buf, "%d\n", dev->enabled);
}

static ssize_t enabled_store(struct my_device *dev, const char *buf, size_t count)
{
    if (!strncmp(buf, "1", 1))
        dev->enabled = true;
    else if (!strncmp(buf, "0", 1))
        dev->enabled = false;
    else
        return -EINVAL;
    return count;
}

// 4. 创建属性
static struct my_attribute status_attr = __ATTR(status, 0664, status_show, status_store);
static struct my_attribute enabled_attr = __ATTR(enabled, 0664, enabled_show, enabled_store);

// 5. 定义属性组
static struct attribute *my_default_attrs[] = {
    &status_attr.attr,
    &enabled_attr.attr,
    NULL,
};

// 6. 实现 sysfs_ops
static ssize_t my_sysfs_show(struct kobject *kobj, struct attribute *attr,
                 char *buf)
{
    struct my_device *dev = to_my_device(kobj);
    struct my_attribute *my_attr = to_my_attr(attr);

    return my_attr->show(dev, buf);
}

static ssize_t my_sysfs_store(struct kobject *kobj, struct attribute *attr,
                  const char *buf, size_t count)
{
    struct my_device *dev = to_my_device(kobj);
    struct my_attribute *my_attr = to_my_attr(attr);

    return my_attr->store(dev, buf, count);
}

static const struct sysfs_ops my_sysfs_ops = {
    .show = my_sysfs_show,
    .store = my_sysfs_store,
};

// 7. 实现 release 函数
static void my_device_release(struct kobject *kobj)
{
    struct my_device *dev = to_my_device(kobj);

    printk("Releasing my_device: %s\n", dev->name);
    kfree(dev->name);
    kfree(dev);
}

// 8. 定义 ktype
static struct kobj_type my_device_ktype = {
    .release = my_device_release,
    .sysfs_ops = &my_sysfs_ops,
    .default_attrs = my_default_attrs,
};
```

### 1.3.4 ktype 与 sysfs 的关系

ktype 是连接 kobject 和 sysfs 的桥梁。sysfs 是内核用于导出对象层次结构的虚拟文件系统，而 ktype 定义了对象在 sysfs 中的表现方式。

当一个 kobject 通过 kobject_add() 添加到层次结构时，内核会根据其 ktype 中定义的 default_attrs 在 sysfs 中创建对应的属性文件。同时，当用户空间读写这些属性文件时，内核会调用 ktype 中定义的 sysfs_ops 函数。

这种设计的好处是：

1. **统一接口**：所有 kobject 都可以通过相同的方式（sysfs）与用户空间交互。
2. **类型安全**：不同类型的对象可以定义不同的属性和操作函数。
3. **自动生成**：属性文件会在对象创建时自动生成，无需手动管理。

理解 ktype、kobject 和 sysfs 之间的关系对于调试设备模型问题非常重要。当你发现某个设备在 sysfs 中缺少某些属性时，很可能是对应的 ktype 没有正确定义。

## 1.4 uevent 事件机制

### 1.4.1 uevent 概述

uevent（user space event）是 Linux 内核向用户空间通知设备状态变化的重要机制。它是实现设备热插拔（hotplug）的核心技术，使得内核能够在设备状态发生变化时主动通知用户空间的守护进程（如 udev），从而动态地创建设备节点、加载驱动模块或执行其他必要的操作。

uevent 机制的工作流程可以概括为以下几个步骤：

1. **事件产生**：当内核中的设备状态发生变化时（如设备添加、移除），驱动模型会创建一个 uevent 事件。
2. **环境变量填充**：在发送事件之前，内核会填充一组环境变量，包含设备的相关信息（如设备路径、总线类型、驱动名称等）。
3. **事件发送**：内核通过多种方式向用户空间发送事件，包括：
   - _netlink socket_：最常用的方式，通过 netlink 协议将事件发送给用户空间
   - **uevent_helper**：可配置的外部程序路径（已不推荐使用）
4. **用户空间处理**：udev 守护进程接收到事件后，根据规则文件执行相应操作

uevent 机制的重要性体现在以下几个方面：

- **动态设备管理**：在设备插入或移除时，无需手动干预，系统自动完成设备节点的创建和清理
- **驱动自动加载**：当新设备添加时，udev 可以根据设备信息自动加载对应的驱动模块
- **设备状态同步**：用户空间可以及时获知内核设备的状态变化，保持与内核的一致性

### 1.4.2 uevent 触发时机

在 Linux 设备模型中，uevent 事件在多种设备状态变化时会被触发。理解这些触发时机对于调试设备热插拔问题非常重要。

#### 设备添加（KOBJ_ADD）

当一个设备被添加到系统中时，会触发 KOBJ_ADD 事件。这是设备热插拔中最常见的事件类型。以下是触发 KOBJ_ADD 的典型场景：

```c
// 文件位置：lib/kobject.c
// kobject_add() 函数中发送 ADD 事件的调用
static int kobject_add(struct kobject *kobj, struct kobject *parent,
               const char *fmt, ...)
{
    // ... 前面的代码 ...

    // 发送添加事件
    retval = kobject_uevent(kobj, KOBJ_ADD);

    return retval;
}
```

KOBJ_ADD 事件在以下情况下被触发：
- 调用 `device_register()` 注册新设备时
- 调用 `driver_register()` 注册新驱动时（对于某些驱动类型）
- 通过 kobject_add() 直接添加 kobject 时

#### 设备移除（KOBJ_REMOVE）

当一个设备从系统中移除时，会触发 KOBJ_REMOVE 事件：

```c
// 文件位置：lib/kobject.c
void kobject_del(struct kobject *kobj)
{
    if (!kobj)
        return;

    // 如果已经在 sysfs 中显示，发送移除事件
    if (kobj->state_in_sysfs) {
        kobject_uevent(kobj, KOBJ_REMOVE);
        sysfs_remove_dir(kobj);
        kobj->state_in_sysfs = 0;
    }

    // ...
}
```

KOBJ_REMOVE 事件在以下情况下被触发：
- 调用 `device_unregister()` 注销设备时
- 设备从总线拓扑中移除时（如 USB 设备拔出）
- kobject 被从层次结构中删除时

#### 设备绑定（KOBJ_BIND）和设备解绑（KOBJ_UNBIND）

这两个事件用于通知设备与驱动之间的绑定关系变化：

- **KOBJ_BIND**：当驱动成功绑定到设备时触发
- **KOBJ_UNBIND**：当驱动从设备解绑时触发

```c
// 文件位置：drivers/base/core.c
// 设备驱动绑定时的 uevent
static int device_bind_driver(struct device *dev)
{
    int ret;

    ret = driver_bind(dev->p->driver, dev);
    if (ret == 0)
        kobject_uevent(&dev->kobj, KOBJ_BIND);  // 绑定成功
    else
        kobject_uevent(&dev->kobj, KOBJ_UNBIND); // 绑定失败

    return ret;
}
```

#### 设备变化（KOBJ_CHANGE）

当设备的某些属性或状态发生变化时，会触发 KOBJ_CHANGE 事件。这个事件的触发场景比较多样：

```c
// 文件位置：include/linux/kobject.h
// uevent 事件类型定义
enum kobject_action {
    KOBJ_ADD,
    KOBJ_REMOVE,
    KOBJ_BIND,
    KOBJ_UNBIND,
    KOBJ_CHANGE,      // 设备属性变化
    KOBJ_MOVE,        // 设备位置变化
    KOBJ_ONLINE,      // 设备上线
    KOBJ_OFFLINE,    // 设备离线
    KOBJ_MAX
};
```

KOBJ_CHANGE 事件的典型触发场景：
- 网络设备收到数据包或状态改变时
- 电池电量变化时
- 输入设备状态变化时（如按键按下/释放）
- 设备属性通过 sysfs 被修改时

### 1.4.3 uevent 实现原理

uevent 机制的实现涉及内核的多个层面，包括 kobject 层、netlink 通信层和用户空间接口层。深入理解其实现原理有助于更好地调试热插拔相关问题。

#### uevent 事件结构

uevent 事件通过 `kobj_uevent_env` 结构体携带环境变量信息传递给用户空间：

```c
// 文件位置：include/linux/kobject.h
struct kobj_uevent_env {
    char envp[UEVENT_NUM_ENVP][ENV_VAR_PATH_LENGTH];  // 环境变量数组
    int envp_idx;                                      // 当前环境变量索引
    char buf[UEVENT_BUFFER_SIZE];                     // 缓冲区
    int buflen;                                       // 缓冲区长度
};
```

每个 uevent 事件都包含一组标准的环境变量：

- **ACTION**：事件动作类型（如 "add", "remove", "change"）
- **DEVPATH**：设备路径（相对于 /sys）
- **SUBSYSTEM**：子系统名称（如 "usb", "pci", "input"）
- **DEVNAME**：设备名称
- **DEVTYPE**：设备类型（可选）
- **DRIVER**：驱动名称（可选）
- **PRODUCT**：产品信息（可选，用于 USB 等设备）

#### kobject_uevent 函数分析

`kobject_uevent()` 是触发 uevent 事件的核心函数：

```c
// 文件位置：lib/kobject.c
int kobject_uevent(struct kobject *kobj, enum kobject_action action)
{
    struct kobj_uevent_env *env;
    const char *action_string;
    const char *path;
    int ret = 0;

    // 如果压制了 uevent，直接返回
    if (kobj->uevent_suppress)
        return 0;

    // 检查对象是否已初始化
    if (!kobj->state_initialized)
        return -EINVAL;

    // 根据动作类型获取动作字符串
    action_string = kobject_actions[action];
    if (!action_string)
        return -EINVAL;

    // 分配并初始化 uevent 环境
    env = kzalloc(sizeof(struct kobj_uevent_env), GFP_KERNEL);
    if (!env)
        return -ENOMEM;

    // 添加标准环境变量
    // 1. ACTION=动作类型
    ret = add_uevent_var(env, "ACTION=%s", action_string);
    if (ret)
        goto exit;

    // 2. DEVPATH=设备路径
    path = kobject_get_path(kobj, GFP_KERNEL);
    ret = add_uevent_var(env, "DEVPATH=%s", path);
    kfree(path);
    if (ret)
        goto exit;

    // 3. SUBSYSTEM=子系统名称
    ret = add_uevent_var(env, "SUBSYSTEM=%s", kobj->kset ?
             kobject_name(&kobj->kset->kobj) : kobj->ktype->name);
    if (ret)
        goto exit;

    // 调用 kset 的 filter 函数（如果存在）
    if (kobj->kset && kobj->kset->uevent_ops) {
        if (kobj->kset->uevent_ops->filter)
            ret = kobj->kset->uevent_ops->filter(kobj->kset, kobj);
        else
            ret = 0;

        // 如果 filter 返回非0，则过滤掉该事件
        if (ret)
            goto exit;
    }

    // 调用 kset 的 uevent 函数（如果存在），可以添加额外的环境变量
    if (kobj->kset && kobj->kset->uevent_ops &&
        kobj->kset->uevent_ops->uevent) {
        ret = kobj->kset->uevent_ops->uevent(kobj->kset, kobj, env);
        if (ret)
            goto exit;
    }

    // 调用 class 的 uevent 函数（如果有）
    if (dev->class && dev->class->dev_uevent) {
        ret = dev->class->dev_uevent(dev, env);
        if (ret)
            goto exit;
    }

    // 最后，发送 uevent 到用户空间
    ret = uevent_netlink_send(kobj, action, env);

exit:
    kfree(env);
    return ret;
}
```

从以上代码可以看出 uevent 事件的完整处理流程：

1. **环境变量初始化**：分配并初始化 uevent 环境结构体
2. **标准变量填充**：添加 ACTION、DEVPATH、SUBSYSTEM 等标准环境变量
3. **kset 过滤**：如果有 kset 的 filter 函数，返回非0则过滤掉该事件
4. **kset 事件处理**：调用 kset 的 uevent 函数，允许添加额外的环境变量
5. **class 事件处理**：如果有 class 的 dev_uevent 函数，调用它处理事件
6. **发送到用户空间**：最终调用 uevent_netlink_send() 发送事件

#### netlink 发送机制

uevent 事件通过 netlink 套接字发送到用户空间：

```c
// 文件位置：lib/kobject.c
static int uevent_netlink_send(struct kobject *kobj, enum kobject_action action,
                   struct kobj_uevent_env *env)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    char *pos;
    int len;
    int ret;

    // 创建 netlink 消息
    skb = nlmsg_new(NLMSG_GOODSIZE, GFP_KERNEL);
    if (!skb)
        return -ENOMEM;

    // 添加 netlink 消息头
    nlh = nlmsg_put(skb, 0, 0, UEVENT_MSG_KERNEL, 0, 0);
    if (!nlh)
        goto free_msg;

    // 将环境变量复制到消息中
    pos = NLMSG_DATA(nlh);
    len = 0;
    for (int i = 0; i < env->envp_idx; i++) {
        int msg_len = strlen(env->envp[i]) + 1;
        if (len + msg_len > NLMSG_PAYLOAD(nlh, 0))
            break;
        memcpy(pos, env->envp[i], msg_len);
        pos += msg_len;
        len += msg_len;
    }

    // 设置消息长度
    nlh->nlmsg_len = NLMSG_LENGTH(len);

    // 通过 netlink 多播发送
    ret = netlink_broadcast(uevent_sock, skb, 0, 1, GFP_KERNEL);

    return ret;

free_msg:
    kfree_skb(skb);
    return -ENOMEM;
}
```

uevent_sock 是一个初始化时创建的 netlink 套接字，用于向用户空间广播 uevent 消息。用户空间的 udev 守护进程订阅这个 netlink 组来接收事件。

## 1.5 class 设备类

### 1.5.1 class 概述

class（设备类）是 Linux 驱动模型中用于组织和管理同类型设备的抽象概念。它代表了一类具有相似功能或特性的设备，如输入设备（input）、串口（tty）、网络设备（net）、LED 灯（leds）等。

class 的主要作用包括：

**设备归类**：将系统中所有相同类型的设备组织在一起，便于用户空间查询和管理。例如，所有输入设备都归入 input 类，所有 LED 设备都归入 leds 类。

**统一接口提供**：class 可以定义一组标准的属性和操作函数，用户空间可以通过这些标准接口与设备进行交互，而无需关心设备的具体实现细节。

**热插拔支持**：class 与 uevent 机制紧密集成，当设备添加到系统中时，udev 可以根据设备所属的 class 执行相应的初始化操作。

**sysfs 入口**：class 在 sysfs 文件系统中创建对应的目录（/sys/class/），作为用户空间访问设备的统一入口点。

在 Linux 设备模型中，常见的 class 包括：

- **input**：输入设备类（键盘、鼠标、触摸屏等）
- **tty**：终端设备类
- **net**：网络设备类
- **block**：块设备类
- **spi**：SPI 设备类
- **i2c**：I2C 设备类
- **usb**：USB 设备类
- **leds**：LED 设备类
- **pwm**：PWM 设备类
- **regulator**：电压调节器类
- **thermal**：热管理设备类

### 1.5.2 class 结构体

class 的核心数据结构是 `struct class`，它定义在 Linux 内核源码的 `include/linux/device.h` 文件中：

```c
// 文件位置：include/linux/device.h
struct class {
    const char      *name;              // class 的名称，对应 /sys/class/ 下的目录名
    struct module   *owner;             // 所属模块，用于引用计数管理
    struct class_attribute    **class_attrs;    // class 级别的属性
    struct device_attribute   **dev_attrs;       // 设备级别的属性
    struct kobject           *dev_kobj;         // 设备在 sysfs 中的 kobject 位置
    int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);  // uevent 回调
    char *(*devnode)(struct device *dev, umode_t *mode);  // 设备节点路径回调
    void (*class_release)(struct class *class);   // class 释放回调
    void (*dev_release)(struct device *dev);      // 设备释放回调

    // 电源管理相关
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    // 设备链接管理
    int (*shutdown)(struct device *dev);

    // 设备属性组
    const struct attribute_group **dev_groups;

    // 类参数
    struct kobject *glue_dir;          // 指向 /sys/devices/ 下的连接目录
    struct semaphore sem;             // 保护 class 的信号量

    // 迭代器
    int (*dev_foreach)(struct device *dev, void *data,
               int (*callback)(struct device *dev, void *data));

    // 命名空间支持
    const struct class_ns_type *ns_type;
    const void *(*namespace)(struct device *dev);

    // 热插拔过滤
    int (*uevent_filter)(struct kset *kset, struct kobject *kobj);
    const char *(*uevent_name)(struct kset *kset, struct kobject *kobj);
    int (*uevent)(struct kset *kset, struct kobject *kobj,
          struct kobj_uevent_env *env);

    // 指针大小端
    struct pm_type *pm;

    struct class_private *p;
};
```

下面对 class 结构体的主要成员进行详细解析：

**name**：这是 class 的核心标识，对应 sysfs 中 /sys/class/ 下的目录名称。例如，如果 name 设为 "leds"，则会在 /sys/class/leds/ 下创建对应的目录。

**owner**：指向所属 module 的指针，用于模块引用计数管理。当模块被卸载时，系统会检查是否有 class 仍在使用该模块。

**class_attrs**：指向 class 级别属性的指针数组，这些属性对应 /sys/class/<class_name>/ 目录下的文件。

**dev_attrs**：指向设备级别属性的指针数组，这些属性对应 /sys/class/<class_name>/<device_name>/ 目录下的文件。

**dev_kobj**：指定设备在 sysfs 中的位置，通常指向 devices_kobj 或 class_dev_kobj。

**dev_uevent**：设备 uevent 回调函数，可以在发送 uevent 事件前修改环境变量或添加自定义变量。

**devnode**：回调函数，用于生成设备节点（如 /dev/ttyS0）的路径和权限。

**class_release**：class 释放回调，当 class 被注销时调用。

**dev_release**：设备释放回调，当设备从 class 中移除时调用。

**suspend/resume**：电源管理相关的回调函数，用于设备的挂起和恢复操作。

**dev_groups**：指向属性组数组的指针，用于定义设备的默认属性组。

#### class_private 结构体

class 结构体内部有一个指向 class_private 的指针，封装了 class 的内部实现细节：

```c
// 文件位置：include/linux/device.h
struct class_private {
    struct kset class_devices;     // 包含该 class 所有设备的 kset
    struct list_head class_interfaces;  // class 接口列表
    struct kobject *class_root;    // 指向 /sys/class/ 目录
};
```

class_private 中的 kset 用于组织该 class 下的所有设备，这与之前介绍的 kset 概念相呼应。

### 1.5.3 class 的创建和使用

在 Linux 设备模型中，创建和使用 class 主要通过以下 API 完成：

#### 创建 class

```c
// 文件位置：drivers/base/class.c
struct class *class_create(struct module *owner, const char *name);
```

这是创建 class 的主要接口函数。参数说明：
- **owner**：通常设置为 THIS_MODULE
- **name**：class 的名称，对应 sysfs 中的目录名

```c
// 示例：创建一个 leds class
static struct class *leds_class;

leds_class = class_create(THIS_MODULE, "leds");
if (IS_ERR(leds_class)) {
    pr_err("Failed to create leds class\n");
    return PTR_ERR(leds_class);
}
```

在早期版本的 Linux 内核中（2.x 到 4.x），class_create() 的参数只有 name。但从 Linux 5.x 开始，为了支持模块化 class，该函数的签名发生了变化，添加了 owner 参数。

#### class 属性定义

class 可以定义两类属性：class 级别属性和设备级别属性。

**class 级别属性**：

```c
// 定义 class 属性
static struct class_attribute leds_class_attr[] = {
    __ATTR_NULL,
};

// 显示所有 LED 状态的属性
static ssize_t status_show(struct class *class,
               struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "LED Class Active\n");
}

static struct class_attribute leds_class_attrs[] = {
    __ATTR(status, 0444, status_show, NULL),
    __ATTR_NULL,
};

// 在创建 class 时指定
leds_class = class_create(THIS_MODULE, "leds");
leds_class->class_attrs = leds_class_attrs;
```

**设备级别属性**：

```c
// 定义设备属性
static ssize_t brightness_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    struct led_device *led = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", led->brightness);
}

static ssize_t brightness_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct led_device *led = dev_get_drvdata(dev);
    int ret;
    unsigned long value;

    ret = kstrtoul(buf, 10, &value);
    if (ret)
        return ret;

    led->brightness = value;
    led_update_brightness(led);

    return count;
}

static DEVICE_ATTR_RW(brightness);

static struct device_attribute led_dev_attrs[] = {
    DEVICE_ATTR_RW(brightness),
    DEVICE_ATTR_RW(trigger),
    __ATTR_NULL,
};

// 在创建 class 时指定
leds_class = class_create(THIS_MODULE, "leds");
leds_class->dev_attrs = led_dev_attrs;
```

#### 注册设备到 class

当设备注册到系统时，可以将其添加到某个 class 中：

```c
// 创建设备
struct device *my_device = device_create(leds_class, parent,
                      MKDEV(LED_MAJOR, minor),
                      led_data, "led%d", minor);

if (IS_ERR(my_device)) {
    pr_err("Failed to create LED device\n");
    return PTR_ERR(my_device);
}
```

device_create() 函数的参数：
- 第一个参数是 class 指针
- 第二个参数是父设备
- 第三个参数是设备号
- 第四个参数是私有数据
- 第五个及后续参数用于构建设备名称

调用 device_create() 后，会在 /sys/class/leds/ 目录下创建对应的设备目录，并创建设备属性文件。

#### 完整的 class 使用示例

以下是一个完整的 LED class 创建和使用示例：

```c
// 1. 定义 LED 设备结构体
struct led_device {
    struct device *dev;
    struct led_classdev *led_cdev;
    int brightness;
    const char *name;
};

// 2. 定义并初始化 LED class（简化版）
static struct class *leds_class;

static int __init leds_class_init(void)
{
    // 创建 leds class
    leds_class = class_create(THIS_MODULE, "leds");
    if (IS_ERR(leds_class)) {
        pr_err("Failed to create leds class\n");
        return PTR_ERR(leds_class);
    }

    // 设置 uevent 回调
    leds_class->dev_uevent = led_uevent;

    // 设置设备节点回调
    leds_class->devnode = led_devnode;

    return 0;
}

static void __exit leds_class_exit(void)
{
    class_destroy(leds_class);
}

// 3. 定义设备属性
static ssize_t brightness_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
    struct led_device *led = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", led->brightness);
}

static ssize_t brightness_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct led_device *led = dev_get_drvdata(dev);
    unsigned long value;
    int ret;

    ret = kstrtoul(buf, 10, &value);
    if (ret)
        return ret;

    led->brightness = value;
    led_set_brightness(led, value);

    return count;
}

static DEVICE_ATTR_RW(brightness);

// 4. 创建 LED 设备
static int led_device_register(struct led_device *led)
{
    // 创建设备
    led->dev = device_create(leds_class, NULL, MKDEV(LED_MAJOR, led->minor),
                 led, "led%d", led->minor);
    if (IS_ERR(led->dev))
        return PTR_ERR(led->dev);

    // 创建设备属性文件
    ret = device_create_file(led->dev, &dev_attr_brightness);
    if (ret)
        goto err_device;

    dev_set_drvdata(led->dev, led);
    return 0;

err_device:
    device_destroy(leds_class, MKDEV(LED_MAJOR, led->minor));
    return ret;
}
```

## 1.6 sysfs 文件系统原理

### 1.6.1 sysfs 概述

sysfs 是 Linux 内核提供的一种虚拟文件系统（virtual filesystem），它以文件系统接口的形式向用户空间导出内核对象（kobject）的层次结构。sysfs 的设计目标是提供一个直观、易用的方式来浏览和操作内核中的设备、驱动和总线结构。

sysfs 与 procfs、devfs 等其他虚拟文件系统相比，具有以下特点：

**面向对象的层次结构**：sysfs 直接映射内核的 kobject 层次结构，每个目录对应一个 kobject，每个文件对应一个 kobject 属性。这种一对一的映射关系使得用户空间可以像浏览普通文件系统一样浏览内核对象。

**只读/可写属性**：sysfs 中的文件可以是只读的（用于导出内核信息）或可写的（用于接收用户空间的配置），为用户空间与内核的交互提供了统一的接口。

**与设备模型紧密集成**：sysfs 是 Linux 设备模型的核心组成部分，设备、驱动、总线、class 等都通过 sysfs 向用户空间呈现。

**支持多种数据类型**：通过适当的包装，sysfs 可以导出简单值（如整数、字符串）、复杂数据结构，甚至二进制数据。

从用户空间的角度来看，sysfs 提供了以下关键功能：
- 浏览系统中所有设备及其层次关系
- 查看设备的属性和状态
- 修改设备的配置参数
- 触发设备操作（如打开/关闭、复位等）

### 1.6.2 sysfs 目录结构

sysfs 的目录结构直接反映了内核对象系统的层次结构。以下是典型 Linux 系统中 /sys 目录的布局：

```
/sys
├── block/                      # 块设备目录
│   ├── loop0/
│   ├── sda/
│   │   ├── queue/
│   │   ├── removeable/
│   │   └── size
│   └── ...
├── bus/                        # 总线类型目录
│   ├── platform/               # 平台总线
│   │   ├── devices/
│   │   └── drivers/
│   ├── spi/                    # SPI 总线
│   │   ├── devices/
│   │   └── drivers/
│   ├── i2c/                    # I2C 总线
│   │   ├── devices/
│   │   └── drivers/
│   ├── usb/                    # USB 总线
│   │   ├── devices/
│   │   └── drivers/
│   ├── pci/                    # PCI 总线
│   │   ├── devices/
│   │   └── drivers/
│   └── ...
├── class/                      # 设备类目录
│   ├── input/                  # 输入设备
│   │   ├── mouse0/
│   │   │   ├── device/
│   │   │   ├── name
│   │   │   └── phys
│   │   ├── keyboard0/
│   │   └── ...
│   ├── leds/                   # LED 设备
│   │   ├── led0/
│   │   │   ├── brightness
│   │   │   ├── trigger
│   │   │   └── max_brightness
│   │   └── ...
│   ├── net/                    # 网络设备
│   ├── tty/                    # 终端设备
│   ├── spi_master/             # SPI 主控制器
│   ├── i2c_adapter/            # I2C 适配器
│   └── ...
├── devices/                    # 设备树（所有设备的层次结构）
│   ├── platform/
│   ├── system/
│   ├── pci0000:00/
│   └── ...
├── driver/                     # 当前加载的驱动（符号链接）
│   └── ...
├── fs/                         # 文件系统特定导出
│   ├── ext4/
│   ├── xfs/
│   └── ...
├── kernel/                     # 内核参数
│   ├── uevent_env/
│   ├── config/
│   └── ...
├── module/                     # 加载的内核模块
│   ├── ext4/
│   │   ├── parameters/
│   │   └── sections/
│   └── ...
└── power/                      # 电源管理
```

下面对主要目录进行详细说明：

#### /sys/block/

包含系统中所有块设备的目录。每个块设备（如 sda、sda1）都有一个对应的子目录，其中包含了该设备的队列参数、只读标志、大小等信息。

```
/sys/block/sda/
├── queue/               # I/O 调度队列参数
│   ├── read_ahead_kb
│   ├── scheduler
│   └── nr_requests
├── removable           # 是否可移动
├── size                # 设备大小（512字节块为单位)
├── stat                # 设备统计信息
└── device -> ../../devices/pci0000:00/...
```

#### /sys/bus/

按总线类型组织的目录，包含系统中所有支持的总线类型。每个总线类型目录下都有 devices/ 和 drivers/ 两个子目录：

- **devices/**：该总线下的所有设备（通常是符号链接，指向 /sys/devices/ 下的设备）
- **drivers/**：该总线下的所有驱动

```
/sys/bus/platform/
├── devices/                    # 平台设备
│   ├── i2c-gpio.0 -> ../../../devices/platform/i2c-gpio.0/
│   └── rtc@00000000 -> ../../../devices/platform/rtc@00000000/
└── drivers/                    # 平台驱动
    ├── gpio-keys
    ├── rtc-cmos
    └── ...
```

#### /sys/class/

按设备类（class）组织的目录。这是用户空间最常访问的目录之一，因为它提供了按功能类型访问设备的统一接口。

```
/sys/class/leds/
├── led0/
│   ├── brightness           # LED 亮度（可读可写）
│   ├── max_brightness      # 最大亮度
│   ├── trigger             # 触发器（可读可写）
│   ├── device -> ../../devices/platform/leds/
│   └── subsystem -> ../../../../class/leds/
└── led1/
    └── ...
```

#### /sys/devices/

这是 sysfs 中最重要的目录之一，包含了系统中所有设备的层次结构树。这个目录直接反映了设备模型的设备树结构。

```
/sys/devices/
├── platform/                # 平台设备
│   ├── rtc@00000000/
│   │   ├── name
│   │   ├── rtc0 -> ./
│   │   └── uevent
│   └── ...
├── pci0000:00/              # PCI 设备
│   ├── 0000:00:1f.2/
│   │   ├── ata1/
│   │   └── ...
│   └── ...
└── system/                 # 系统设备
    ├── cpu/
    └── memory/
```

### 1.6.3 sysfs 与驱动模型的关系

sysfs 是 Linux 驱动模型在用户空间的主要接口，它将驱动模型的各个组件以文件系统的方式呈现出来。理解 sysfs 与驱动模型的关系对于调试驱动问题和开发新驱动都非常重要。

#### sysfs 与 kobject

sysfs 的核心是 kobject。每个 kobject 在 sysfs 中都对应一个目录，目录名就是 kobject 的 name 成员。而 kobject 的属性（通过 ktype 的 default_attrs 定义）则以文件的形式存在于该目录下。

```
kobject 层次结构                sysfs 目录结构
+-----------------+             /sys/
| kset (block)    |             +-----------------+
+-----------------+             | block/          |
| kobject (sda)   |             |   +---------+  |
+-----------------+             |   | sda/    |  |
                                 |   | ...     |  |
                                 |   +---------+  |
                                 |              |
                                 +--------------+
```

#### sysfs 与 device

device 结构体内嵌了 kobject，当 device 注册到系统时，会自动在 sysfs 中创建对应的目录：

```c
// 文件位置：drivers/base/core.c
int device_add(struct device *dev)
{
    // ... 省略部分代码 ...

    // 添加到 kobject 层次结构
    kobject_add(&dev->kobj, dev->parent, "%s", dev_name(dev));

    // 添加设备属性
    if (dev->groups)
        sysfs_create_groups(&dev->kobj, dev->groups);

    // 如果有 class，创建 class 链接
    if (dev->class) {
        kobject_add(&dev->kobj, &dev->class->p->class_devices.kobj, NULL);
        /* tie the class and the device */
        sysfs_create_link(&dev->class->p->class_devices.kobj,
                  &dev->kobj, dev_name(dev));
    }

    // 发送 uevent
    kobject_uevent(&dev->kobj, KOBJ_ADD);

    return 0;
}
```

从代码可以看出 device_add() 的主要工作：
1. 在 sysfs 中创建设备目录（使用父设备和设备名称）
2. 创建设备属性文件组
3. 如果设备有 class，在 class 目录下创建设备链接
4. 发送 KOBJ_ADD 事件

#### sysfs 与 driver

driver 注册到系统时，同样会在 sysfs 中创建对应的目录：

```c
// 文件位置：drivers/base/bus.c
int bus_add_driver(struct bus_type *bus, struct device_driver *drv)
{
    // ... 省略部分代码 ...

    // 在 /sys/bus/<bus_name>/drivers/<driver_name>/ 创建目录
    kobject_add(&drv->p->kobj, &bus->p->drivers_kset->kobj, "%s",
            drv->name);

    // 添加驱动属性
    if (drv->groups)
        sysfs_create_groups(&drv->p->kobj, drv->groups);

    // 如果有设备与此驱动绑定，添加符号链接
    if (drv->p->devices_kset) {
        sysfs_create_link(&drv->kobj, &drv->p->devices_kset->kobj,
                  "devices");
    }

    // 添加属性文件
    driver_add_attrs(bus, drv);

    // 发送 uevent
    kobject_uevent(&drv->kobj, KOBJ_ADD);

    return 0;
}
```

#### sysfs 与 class

class 在 sysfs 中创建 /sys/class/<class_name>/ 目录，并在该目录下管理其所有设备：

```c
// 文件位置：drivers/base/class.c
int class_add_device(struct class *class, struct device *device)
{
    // 在 /sys/class/<class>/<device_name>/ 创建目录
    error = kobject_add(&device->kobj, &class->p->class_devices.kobj,
                device_name(device));

    // 创建 class 定义的属性文件
    if (class->dev_attrs)
        sysfs_create_files(&device->kobj, class->dev_attrs);

    return 0;
}
```

#### sysfs 属性文件与内核数据交互

sysfs 属性文件是用户空间与内核数据交互的主要通道。用户空间可以读取属性文件获取内核数据，也可以写入属性文件修改内核数据。

**属性读取流程**：

```
用户空间读取
    ↓
cat /sys/class/leds/led0/brightness
    ↓
VFS (虚拟文件系统)
    ↓
sysfs 文件系统操作
    ↓
kobject (led0 的 kobject)
    ↓
ktype->sysfs_ops->show()
    ↓
读取内核数据结构中的 brightness 值
    ↓
返回给用户空间
```

**属性写入流程**：

```
用户空间写入
    ↓
echo 255 > /sys/class/leds/led0/brightness
    ↓
VFS
    ↓
sysfs 文件系统操作
    ↓
kobject
    ↓
ktype->sysfs_ops->store()
    ↓
解析输入字符串
    ↓
更新内核数据结构中的 brightness 值
    ↓
返回写入的字节数
```

#### sysfs 的使用注意事项

在使用 sysfs 时，需要注意以下几点：

1. **属性缓冲区大小**：sysfs 属性的缓冲区大小有限（通常为 PAGE_SIZE，即 4KB），不应在属性中返回过大的数据。

2. **属性命名**：属性名应全部小写，避免使用特殊字符。

3. **权限设置**：根据需要设置适当的文件权限（只读、读写等），敏感属性不应有写权限。

4. **错误处理**：在 show 和 store 函数中应正确处理错误情况，返回合适的错误码。

5. **原子性**：对于复杂的数据更新，应考虑使用锁来保证数据一致性。

6. **避免递归**：避免在 sysfs 回调函数中执行可能导致递归的操作。

## 本章面试题

### 面试题1：kobject 的作用是什么？它与 device、driver 的关系是什么？

**参考答案：**

kobject 是 Linux 内核设备模型的基础构建块，它提供了以下核心功能：

1. **引用计数管理**：通过 kref 机制追踪对象被引用的次数，当引用计数降为 0 时自动释放对象。
2. **层次结构管理**：通过 parent 指针和 entry 链表形成树形结构。
3. **sysfs 集成**：每个 kobject 在 sysfs 中对应一个目录，用于导出对象信息到用户空间。
4. **热插拔事件支持**：状态变化时触发 uevent 事件，通知用户空间。

kobject 与 device、driver 的关系：device 和 driver 结构体都内嵌了 struct kobject kobj 成员。device 结构体代表硬件设备，driver 结构体代表设备驱动，它们通过内嵌的 kobject 接入到内核的对象系统中。这意味着 device 和 driver 可以：
- 参与 sysfs 层次结构
- 触发和响应热插拔事件
- 利用引用计数管理生命周期

### 面试题2：简述 kobject 的生命周期

**参考答案：**

kobject 的生命周期包括以下几个阶段：

1. **创建阶段**：分配包含 kobject 的结构体内存（通常是更大结构体的成员）。
2. **初始化阶段**：调用 kobject_init() 初始化 kobject，设置引用计数为 1，初始化链表节点，设置 ktype。
3. **添加阶段**：调用 kobject_add() 将对象添加到层次结构，创建 sysfs 目录，加入 kset，发送 KOBJ_ADD 事件。
4. **使用阶段**：对象可以被其他代码引用和使用，通过 kobject_get()/kobject_put() 管理引用计数。
5. **删除阶段**：调用 kobject_del() 从层次结构中移除，删除 sysfs 目录，发送 KOBJ_REMOVE 事件。
6. **释放阶段**：当引用计数降为 0 时，调用 ktype 中定义的 release 函数释放对象资源。

### 面试题3：kset 和 ktype 的区别是什么？

**参考答案：**

kset 和 ktype 是两个不同的概念，虽然它们都与 kobject 相关：

**kset（对象集合）**：
- 是 kobject 的容器，用于组织一组相关的 kobject
- 本身包含一个 kobject，因此可以参与层次结构
- 维护一个链表，包含其所有成员
- 主要用于将相关的对象分组管理（如 block kset 包含所有块设备）

**ktype（对象类型）**：
- 是 kobject 的类型定义，描述对象的"类"
- 定义了对象的释放函数、sysfs 操作、默认属性
- 同一类型的对象共享相同的 ktype
- 主要用于定义对象的行为和属性

简单来说：kset 回答"这个对象属于哪一组"的问题，ktype 回答"这个对象是什么类型、有什么行为"的问题。

### 面试题4：为什么 kobject 不能直接使用，而是要嵌入到其他结构体中？

**参考答案：**

kobject 设计上是一个"元对象"（meta-object），它本身并不代表任何具体的设备或驱动。它的主要作用是为其他结构体提供通用功能。

在实际使用中，kobject 被嵌入到更大的结构体中（如 device、device_driver、class、bus_type 等）。这样做的好处：

1. **面向对象**：通过嵌入 kobject，更大的结构体自动获得了对象管理的功能，无需重复实现引用计数、层次结构等功能。
2. **灵活性**：开发者可以自由定义包含 kobject 的结构体，根据具体需求添加业务相关的成员。
3. **统一接口**：所有内嵌 kobject 的结构体都可以通过统一的接口（kobject API）进行管理。

### 面试题5：描述 kobject_add() 函数的主要工作流程

**参考答案：**

kobject_add() 函数的主要工作流程如下：

1. **参数验证**：检查 kobj 是否有效，是否已经初始化。
2. **设置名称**：如果传入了格式化字符串，使用 kobject_set_name() 设置对象名称。
3. **设置父对象**：如果 kobject 的 parent 成员为 NULL，则使用传入的 parent 参数。
4. **创建 sysfs 目录**：调用 sysfs_create_dir() 在 sysfs 文件系统中创建对应的目录。
5. **设置状态标志**：将 state_in_sysfs 设置为 1，表示对象已在 sysfs 中显示。
6. **加入 kset**：如果对象指定了 kset，调用 kobj_kset_join() 将其加入到 kset 的链表中。
7. **初始化 uevent 环境**：调用 kobject_init_uevent_env() 初始化热插拔事件的环境变量。
8. **发送添加事件**：调用 kobject_uevent() 发送 KOBJ_ADD 事件，通知用户空间有新对象添加。


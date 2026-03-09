# Nor Flash 测试与验证

Nor Flash 的可靠性直接关系到整个系统的稳定性，因此必须进行全面的测试与验证。本章详细介绍单元测试方法、压力测试设计、数据完整性验证和边界条件测试等内容，帮助开发者建立完善的测试体系。

---

## 1. 单元测试方法

单元测试是软件质量保证的基础，通过对驱动各个功能模块进行独立测试，可以及早发现和修复缺陷。

### 1.1 测试框架设计

设计一个适用于 Nor Flash 驱动的单元测试框架，支持测试用例的注册、执行和结果报告。

**测试框架核心结构**

```c
/**
 * 测试用例类型定义
 */
typedef enum {
    TEST_TYPE_INIT,           /* 初始化测试 */
    TEST_TYPE_READ,          /* 读取测试 */
    TEST_TYPE_WRITE,         /* 写入测试 */
    TEST_TYPE_ERASE,         /* 擦除测试 */
    TEST_TYPE_STATUS,         /* 状态测试 */
    TEST_TYPE_SPECIAL,        /* 特殊功能测试 */
} test_type_t;

/**
 * 测试用例结构体
 */
typedef struct test_case {
    const char *name;                /* 测试名称 */
    test_type_t type;                /* 测试类型 */
    int (*setup)(void);             /* 测试前setup */
    int (*run)(void);               /* 测试执行函数 */
    int (*teardown)(void);          /* 测试后teardown */
    struct test_case *next;          /* 链表指针 */
} test_case_t;

/**
 * 测试结果
 */
typedef struct {
    const char *name;                /* 测试名称 */
    int passed;                      /* 是否通过 */
    const char *error_msg;           /* 错误信息 */
    uint32_t elapsed_ms;             /* 执行时间 */
} test_result_t;

/**
 * 测试套件
 */
typedef struct {
    test_case_t *cases;              /* 测试用例链表 */
    uint32_t total_count;            /* 总测试数 */
    uint32_t passed_count;           /* 通过数 */
    uint32_t failed_count;           /* 失败数 */
    test_result_t *results;          /* 结果数组 */
} test_suite_t;

/* 全件 */
static局测试套 test_suite_t g_test_suite = {
    .cases = NULL,
    .total_count = 0,
    .passed_count = 0,
    .failed_count = 0,
    .results = NULL,
};

/**
 * 注册测试用例
 */
void test_register(const char *name, test_type_t type,
                  int (*setup)(void), int (*run)(void),
                  int (*teardown)(void))
{
    test_case_t *new_case = (test_case_t *)malloc(sizeof(test_case_t));
    new_case->name = name;
    new_case->type = type;
    new_case->setup = setup;
    new_case->run = run;
    new_case->teardown = teardown;
    new_case->next = NULL;

    /* 添加到链表 */
    test_case_t **tail = &g_test_suite.cases;
    while (*tail) {
        tail = &(*tail)->next;
    }
    *tail = new_case;

    g_test_suite.total_count++;
}

/**
 * 运行测试套件
 */
int test_run_all(void)
{
    test_case_t *case_ptr = g_test_suite.cases;
    uint32_t case_idx = 0;

    printf("=== Running %u test cases ===\n", g_test_suite.total_count);

    while (case_ptr) {
        uint32_t start_time = nor_get_tick();
        int result = 0;

        printf("Running: %s ... ", case_ptr->name);
        fflush(stdout);

        /* 执行 setup */
        if (case_ptr->setup) {
            result = case_ptr->setup();
        }

        /* 执行测试 */
        if (result == 0 && case_ptr->run) {
            result = case_ptr->run();
        }

        /* 执行 teardown */
        if (case_ptr->teardown) {
            case_ptr->teardown();
        }

        uint32_t elapsed = nor_get_tick() - start_time;

        /* 记录结果 */
        if (result == 0) {
            printf("PASSED (%lu ms)\n", elapsed);
            g_test_suite.passed_count++;
        } else {
            printf("FAILED (%lu ms)\n", elapsed);
            g_test_suite.failed_count++;
        }

        case_ptr = case_ptr->next;
        case_idx++;
    }

    printf("\n=== Test Summary ===\n");
    printf("Total:  %u\n", g_test_suite.total_count);
    printf("Passed: %u\n", g_test_suite.passed_count);
    printf("Failed: %u\n", g_test_suite.failed_count);

    return (g_test_suite.failed_count == 0) ? 0 : -1;
}
```

### 1.2 基础功能测试

基础功能测试验证 Nor Flash 驱动的核心功能是否正常工作。

**初始化测试**

```c
/**
 * 初始化测试
 */
static int test_init_setup(void)
{
    /* 确保测试区域已擦除 */
    nor_erase_sector(g_test_dev, TEST_ADDR);
    return 0;
}

static int test_init_run(void)
{
    int ret;
    uint8_t mfg_id;
    uint16_t dev_id;

    /* 测试完整初始化流程 */
    ret = nor_init(g_test_dev, &g_test_config);
    if (ret != 0) {
        printf("Init failed: %d\n", ret);
        return ret;
    }

    /* 验证芯片 ID 读取 */
    ret = nor_read_id(g_test_dev, &mfg_id, &dev_id);
    if (ret != 0) {
        printf("Read ID failed: %d\n", ret);
        return ret;
    }

    /* 验证厂商 ID 有效性 */
    if (mfg_id == 0xFF || mfg_id == 0x00) {
        printf("Invalid manufacturer ID: 0x%02X\n", mfg_id);
        return -NOR_ERR_NO_DEVICE;
    }

    printf("Chip detected: MFG=0x%02X, DEV=0x%04X\n", mfg_id, dev_id);

    return 0;
}

/* 注册初始化测试 */
void test_init_register(void)
{
    test_register("Initialization", TEST_TYPE_INIT,
                  test_init_setup, test_init_run, NULL);
}
```

**读写功能测试**

```c
/**
 * 基本读写测试
 */
static int test_read_write_setup(void)
{
    /* 擦除测试区域 */
    return nor_erase_sector(g_test_dev, TEST_ADDR);
}

static int test_read_write_run(void)
{
    const uint8_t write_data[] = "Hello, Nor Flash!";
    uint8_t read_data[64];
    int ret;

    /* 写入数据 */
    ret = nor_write(g_test_dev, TEST_ADDR, write_data, sizeof(write_data));
    if (ret != sizeof(write_data)) {
        printf("Write failed: ret=%d\n", ret);
        return ret;
    }

    /* 读取数据 */
    ret = nor_read(g_test_dev, TEST_ADDR, read_data, sizeof(write_data));
    if (ret != sizeof(write_data)) {
        printf("Read failed: ret=%d\n", ret);
        return ret;
    }

    /* 验证数据 */
    if (memcmp(write_data, read_data, sizeof(write_data)) != 0) {
        printf("Data mismatch!\n");
        printf("Expected: %s\n", write_data);
        printf("Got:      %s\n", read_data);
        return -NOR_ERR_VERIFY;
    }

    return 0;
}

/**
 * 多地址读写测试
 */
static int test_multi_address_run(void)
{
    uint8_t write_buf[256];
    uint8_t read_buf[256];
    int ret;

    /* 填充测试数据 */
    for (int i = 0; i < 256; i++) {
        write_buf[i] = (uint8_t)i;
    }

    /* 测试多个地址 */
    for (uint32_t addr = 0; addr < FLASH_TOTAL_SIZE; addr += 4096) {
        /* 擦除 */
        ret = nor_erase_sector(g_test_dev, addr);
        if (ret != 0) {
            printf("Erase failed at 0x%08X\n", addr);
            return ret;
        }

        /* 写入 */
        ret = nor_write(g_test_dev, addr, write_buf, 256);
        if (ret != 256) {
            printf("Write failed at 0x%08X\n", addr);
            return ret;
        }

        /* 读取 */
        ret = nor_read(g_test_dev, addr, read_buf, 256);
        if (ret != 256) {
            printf("Read failed at 0x%08X\n", addr);
            return ret;
        }

        /* 验证 */
        if (memcmp(write_buf, read_buf, 256) != 0) {
            printf("Data mismatch at 0x%08X\n", addr);
            return -NOR_ERR_VERIFY;
        }
    }

    return 0;
}
```

### 1.3 边界条件测试

边界条件测试验证驱动在极端情况下的行为。

**地址边界测试**

```c
/**
 * 地址边界测试
 */
static int test_address_boundaries(void)
{
    uint8_t buf[16];
    uint32_t total_size = g_test_dev->total_size;
    int ret;

    /* 测试地址 0 */
    memset(buf, 0xAA, 16);
    ret = nor_write(g_test_dev, 0, buf, 16);
    if (ret != 16) {
        printf("Write at address 0 failed\n");
        return ret;
    }

    /* 测试最后一个有效地址 */
    memset(buf, 0xBB, 16);
    ret = nor_write(g_test_dev, total_size - 16, buf, 16);
    if (ret != 16) {
        printf("Write at last address failed\n");
        return ret;
    }

    /* 测试超出边界的写入（应该失败） */
    ret = nor_write(g_test_dev, total_size, buf, 16);
    if (ret >= 0) {
        printf("Write beyond boundary should fail\n");
        return -NOR_ERR_OUT_OF_RANGE;
    }

    return 0;
}

/**
 * 长度边界测试
 */
static int test_length_boundaries(void)
{
    uint8_t buf[512];
    int ret;

    /* 写入 0 字节 */
    ret = nor_write(g_test_dev, TEST_ADDR, buf, 0);
    if (ret != 0) {
        printf("Write 0 bytes failed\n");
        return ret;
    }

    /* 写入 1 字节 */
    buf[0] = 0x55;
    ret = nor_write(g_test_dev, TEST_ADDR, buf, 1);
    if (ret != 1) {
        printf("Write 1 byte failed\n");
        return ret;
    }

    /* 写入页大小 */
    uint32_t page_size = g_test_dev->page_size;
    ret = nor_write(g_test_dev, TEST_ADDR, buf, page_size);
    if (ret != (int)page_size) {
        printf("Write page size failed\n");
        return ret;
    }

    /* 写入超过页大小 */
    ret = nor_write(g_test_dev, TEST_ADDR, buf, page_size + 1);
    if (ret != (int)(page_size + 1)) {
        printf("Write page+1 size failed\n");
        return ret;
    }

    return 0;
}
```

---

## 2. 压力测试设计

压力测试通过模拟极端工作条件，验证系统在长时间、高负荷运行下的可靠性。

### 2.1 连续读写压力测试

连续读写压力测试验证长时间大量数据传输的稳定性。

**循环读写测试**

```c
/**
 * 循环读写压力测试
 */
typedef struct {
    uint32_t test_addr;              /* 测试地址 */
    uint32_t test_size;               /* 每次传输大小 */
    uint32_t iterations;              /* 迭代次数 */
    uint32_t current_iter;            /* 当前迭代 */
    uint32_t error_count;             /* 错误计数 */
    uint32_t total_time_ms;           /* 总耗时 */
    uint8_t *write_buffer;           /* 写入缓冲区 */
    uint8_t *read_buffer;            /* 读取缓冲区 */
} stress_test_ctx_t;

static stress_test_ctx_t g_stress_ctx;

int nor_stress_test_init(uint32_t addr, uint32_t size, uint32_t iterations)
{
    g_stress_ctx.test_addr = addr;
    g_stress_ctx.test_size = size;
    g_stress_ctx.iterations = iterations;
    g_stress_ctx.current_iter = 0;
    g_stress_ctx.error_count = 0;

    /* 分配缓冲区 */
    g_stress_ctx.write_buffer = (uint8_t *)malloc(size);
    g_stress_ctx.read_buffer = (uint8_t *)malloc(size);

    if (!g_stress_ctx.write_buffer || !g_stress_ctx.read_buffer) {
        return -NOR_ERR_NO_MEMORY;
    }

    /* 填充测试数据 */
    for (uint32_t i = 0; i < size; i++) {
        g_stress_ctx.write_buffer[i] = (uint8_t)(i & 0xFF);
    }

    return 0;
}

int nor_stress_test_run(void)
{
    int ret;

    printf("Starting stress test: %lu iterations, %lu bytes each\n",
           g_stress_ctx.iterations, g_stress_ctx.test_size);

    uint32_t start_time = nor_get_tick();

    for (g_stress_ctx.current_iter = 0;
         g_stress_ctx.current_iter < g_stress_ctx.iterations;
         g_stress_ctx.current_iter++) {

        /* 擦除 */
        ret = nor_erase(g_stress_ctx.test_addr, g_stress_ctx.test_size);
        if (ret != 0) {
            printf("Erase failed at iteration %lu\n",
                   g_stress_ctx.current_iter);
            g_stress_ctx.error_count++;
            continue;
        }

        /* 写入 */
        ret = nor_write(g_stress_ctx.test_addr,
                       g_stress_ctx.write_buffer,
                       g_stress_ctx.test_size);
        if (ret != (int)g_stress_ctx.test_size) {
            printf("Write failed at iteration %lu\n",
                   g_stress_ctx.current_iter);
            g_stress_ctx.error_count++;
            continue;
        }

        /* 读取 */
        ret = nor_read(g_stress_ctx.test_addr,
                      g_stress_ctx.read_buffer,
                      g_stress_ctx.test_size);
        if (ret != (int)g_stress_ctx.test_size) {
            printf("Read failed at iteration %lu\n",
                   g_stress_ctx.current_iter);
            g_stress_ctx.error_count++;
            continue;
        }

        /* 验证 */
        if (memcmp(g_stress_ctx.write_buffer,
                   g_stress_ctx.read_buffer,
                   g_stress_ctx.test_size) != 0) {
            printf("Verify failed at iteration %lu\n",
                   g_stress_ctx.current_iter);
            g_stress_ctx.error_count++;
        }

        /* 进度报告 */
        if ((g_stress_ctx.current_iter + 1) % 100 == 0) {
            printf("Progress: %lu/%lu (%.1f%%)\n",
                   g_stress_ctx.current_iter + 1,
                   g_stress_ctx.iterations,
                   (float)(g_stress_ctx.current_iter + 1) * 100.0 /
                   g_stress_ctx.iterations);
        }
    }

    g_stress_ctx.total_time_ms = nor_get_tick() - start_time;

    /* 打印结果 */
    printf("\n=== Stress Test Results ===\n");
    printf("Total iterations:  %lu\n", g_stress_ctx.iterations);
    printf("Total errors:      %lu\n", g_stress_ctx.error_count);
    printf("Success rate:      %.2f%%\n",
           (float)(g_stress_ctx.iterations - g_stress_ctx.error_count) *
           100.0 / g_stress_ctx.iterations);
    printf("Total time:        %lu ms\n", g_stress_ctx.total_time_ms);
    printf("Avg time/iter:     %.2f ms\n",
           (float)g_stress_ctx.total_time_ms / g_stress_ctx.iterations);

    return (g_stress_ctx.error_count == 0) ? 0 : -1;
}
```

### 2.2 并发访问测试

并发访问测试验证多个任务同时访问 Flash 时的稳定性和数据一致性。

**多任务读写测试**

```c
/**
 * 并发访问测试上下文
 */
typedef struct {
    TaskHandle_t task_handle;         /* 任务句柄 */
    uint32_t task_id;                /* 任务ID */
    uint32_t addr;                   /* 测试地址 */
    uint32_t size;                   /* 测试大小 */
    uint32_t iterations;             /* 迭代次数 */
    uint32_t error_count;            /* 错误计数 */
    uint8_t *buffer;                 /* 测试缓冲区 */
} concurrent_test_task_t;

static concurrent_test_task_t g_concurrent_tasks[4];
static StaticSemaphore_t g_test_mutex_buf;
static SemaphoreHandle_t g_test_mutex;

/**
 * 并发测试任务函数
 */
void concurrent_test_task(void *params)
{
    concurrent_test_task_t *task = (concurrent_test_task_t *)params;
    uint32_t pattern = 0x55AA0000 | task->task_id;

    printf("Task %lu started, addr=0x%08X, size=%lu\n",
           task->task_id, task->addr, task->size);

    for (uint32_t i = 0; i < task->iterations; i++) {
        /* 每个任务使用不同的数据模式 */
        memset(task->buffer, (uint8_t)(pattern & 0xFF), task->size);

        /* 获取互斥锁 */
        xSemaphoreTake(g_test_mutex, portMAX_DELAY);

        /* 擦除 */
        if (nor_erase(task->addr, task->size) != 0) {
            task->error_count++;
            xSemaphoreGive(g_test_mutex);
            continue;
        }

        /* 写入 */
        if (nor_write(task->addr, task->buffer, task->size) != (int)task->size) {
            task->error_count++;
            xSemaphoreGive(g_test_mutex);
            continue;
        }

        /* 读取 */
        if (nor_read(task->addr, task->buffer, task->size) != (int)task->size) {
            task->error_count++;
            xSemaphoreGive(g_test_mutex);
            continue;
        }

        /* 验证 */
        for (uint32_t j = 0; j < task->size; j++) {
            if (task->buffer[j] != (uint8_t)(pattern & 0xFF)) {
                task->error_count++;
                break;
            }
        }

        /* 释放互斥锁 */
        xSemaphoreGive(g_test_mutex);

        /* 任务延时，避免过度竞争 */
        vTaskDelay(1);
    }

    printf("Task %lu completed, errors=%lu\n",
           task->task_id, task->error_count);

    vTaskDelete(NULL);
}

/**
 * 并发访问测试
 */
int nor_concurrent_test(uint32_t num_tasks, uint32_t iterations)
{
    int ret;

    /* 创建互斥锁 */
    g_test_mutex = xSemaphoreCreateMutexStatic(&g_test_mutex_buf);

    /* 创建测试任务 */
    uint32_t task_size = 256;
    uint32_t base_addr = TEST_ADDR;

    for (uint32_t i = 0; i < num_tasks; i++) {
        g_concurrent_tasks[i].task_id = i;
        g_concurrent_tasks[i].addr = base_addr + (i * 4096);
        g_concurrent_tasks[i].size = task_size;
        g_concurrent_tasks[i].iterations = iterations;
        g_concurrent_tasks[i].error_count = 0;

        g_concurrent_tasks[i].buffer = (uint8_t *)malloc(task_size);
        if (!g_concurrent_tasks[i].buffer) {
            return -NOR_ERR_NO_MEMORY;
        }

        ret = xTaskCreate(concurrent_test_task,
                         "NorTest",
                         2048,
                         &g_concurrent_tasks[i],
                         5,
                         &g_concurrent_tasks[i].task_handle);

        if (ret != pdPASS) {
            printf("Failed to create task %lu\n", i);
            return -1;
        }
    }

    /* 等待所有任务完成 */
    vTaskDelay(60000); /* 等待足够长时间 */

    /* 汇总结果 */
    uint32_t total_errors = 0;
    printf("\n=== Concurrent Test Results ===\n");
    for (uint32_t i = 0; i < num_tasks; i++) {
        printf("Task %lu: %lu errors\n", i, g_concurrent_tasks[i].error_count);
        total_errors += g_concurrent_tasks[i].error_count;
    }

    vSemaphoreDelete(g_test_mutex);

    return (total_errors == 0) ? 0 : -1;
}
```

### 2.3 掉电恢复测试

掉电恢复测试验证系统在意外断电后的数据完整性和恢复能力。

**模拟掉电测试**

```c
/**
 * 掉电恢复测试
 */
typedef struct {
    uint32_t test_addr;
    uint32_t test_size;
    uint8_t *write_buf;
    uint8_t *read_buf;
} power_fail_test_ctx_t;

/**
 * 模拟写入过程中掉电
 */
int nor_power_fail_test_write(nor_device_t *dev, uint32_t addr,
                              const uint8_t *data, uint32_t len,
                              uint32_t fail_point)
{
    int ret;

    /* 擦除 */
    ret = nor_erase(dev, addr, len);
    if (ret != 0) {
        return ret;
    }

    /* 发送写使能 */
    ret = nor_write_enable(dev);
    if (ret != 0) {
        return ret;
    }

    /* 发送写命令 */
    nor_transport_cmd(dev->transport, dev->cmd.page_program);
    nor_send_address(dev, addr);

    /* 分批发送数据，在指定位置模拟掉电 */
    uint32_t sent = 0;
    while (sent < len) {
        uint32_t chunk = 16; /* 每批 16 字节 */
        if (sent + chunk > len) {
            chunk = len - sent;
        }

        nor_transport_send(dev->transport, data + sent, chunk);
        sent += chunk;

        /* 模拟掉电 */
        if (sent >= fail_point) {
            /* 突然断电：直接关闭电源或复位 */
            printf("Simulating power failure at byte %lu\n", sent);
            HAL_NVIC_SystemReset();  /* 或直接切断电源 */
        }
    }

    /* 等待完成 */
    return nor_wait_ready(dev, NOR_TIMEOUT_PAGE_PROGRAM);
}

/**
 * 掉电恢复验证
 */
int nor_power_fail_verify(nor_device_t *dev, uint32_t addr,
                          const uint8_t *expected, uint32_t len)
{
    uint8_t read_buf[256];
    int ret;

    /* 重新初始化 */
    ret = nor_init(dev, &g_test_config);
    if (ret != 0) {
        return ret;
    }

    /* 读取数据 */
    ret = nor_read(dev, addr, read_buf, len);
    if (ret != (int)len) {
        return ret;
    }

    /* 验证 */
    if (memcmp(expected, read_buf, len) != 0) {
        printf("Data corruption detected after power failure\n");
        return -NOR_ERR_VERIFY;
    }

    return 0;
}

/**
 * 完整掉电恢复测试流程
 */
int nor_power_fail_test(void)
{
    uint8_t write_buf[256];
    uint8_t read_buf[256];
    int ret;

    printf("=== Power Failure Test ===\n");

    /* 准备测试数据 */
    for (int i = 0; i < 256; i++) {
        write_buf[i] = (uint8_t)i;
    }

    /* 测试不同的掉电点 */
    uint32_t fail_points[] = {16, 32, 64, 128, 200, 256};

    for (int i = 0; i < sizeof(fail_points) / sizeof(fail_points[0]); i++) {
        printf("\nTesting fail point: %lu\n", fail_points[i]);

        /* 在写入过程中模拟掉电 */
        /* 注意：实际测试需要硬件支持或使用仿真器 */
        /* 这里仅记录测试逻辑 */

        /* 恢复后验证数据完整性 */
        ret = nor_power_fail_verify(g_test_dev, TEST_ADDR,
                                    write_buf, fail_points[i]);
        if (ret != 0) {
            printf("Power fail test failed at point %lu\n", fail_points[i]);
            /* 继续测试其他点或停止 */
        }
    }

    return 0;
}
```

---

## 3. 数据完整性验证

数据完整性验证是确保存储数据正确性的关键环节，需要采用多种校验手段。

### 3.1 校验和验证

校验和是最基本的数据完整性验证方法，实现简单但能有效检测大多数错误。

**常见校验算法实现**

```c
/**
 * CRC32 校验
 */
uint32_t nor_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

/**
 * 简单校验和
 */
uint32_t nor_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t sum = 0;

    for (uint32_t i = 0; i < len; i++) {
        sum += data[i];
    }

    return sum;
}

/**
 * XOR 校验
 */
uint8_t nor_xor_checksum(const uint8_t *data, uint32_t len)
{
    uint8_t xor_sum = 0;

    for (uint32_t i = 0; i < len; i++) {
        xor_sum ^= data[i];
    }

    return xor_sum;
}

/**
 * 带校验的读取
 */
int nor_read_with_checksum(nor_device_t *dev, uint32_t addr,
                          uint8_t *buf, uint32_t len,
                          uint32_t expected_checksum)
{
    int ret;

    ret = nor_read(dev, addr, buf, len);
    if (ret != (int)len) {
        return ret;
    }

    uint32_t actual_checksum = nor_checksum(buf, len);
    if (actual_checksum != expected_checksum) {
        printf("Checksum mismatch: expected=0x%08X, actual=0x%08X\n",
               expected_checksum, actual_checksum);
        return -NOR_ERR_VERIFY;
    }

    return len;
}

/**
 * 带校验的写入
 */
int nor_write_with_checksum(nor_device_t *dev, uint32_t addr,
                           const uint8_t *buf, uint32_t len,
                           uint32_t *checksum_out)
{
    int ret;

    /* 计算校验和 */
    uint32_t checksum = nor_checksum(buf, len);
    if (checksum_out) {
        *checksum_out = checksum;
    }

    /* 写入数据 */
    ret = nor_write(dev, addr, buf, len);
    if (ret != (int)len) {
        return ret;
    }

    /* 读取验证 */
    uint8_t verify_buf[256];
    if (len <= 256) {
        ret = nor_read(dev, addr, verify_buf, len);
        if (ret != (int)len) {
            return ret;
        }

        uint32_t verify_checksum = nor_checksum(verify_buf, len);
        if (verify_checksum != checksum) {
            printf("Verify failed: checksum mismatch\n");
            return -NOR_ERR_VERIFY;
        }
    }

    return len;
}
```

### 3.2 数据完整性架构

设计完整的数据完整性验证架构，支持多种校验级别：

```c
/**
 * 完整性验证级别
 */
typedef enum {
    INTEGRITY_NONE,        /* 不验证 */
    INTEGRITY_CHECKSUM,    /* 校验和验证 */
    INTEGRITY_CRC8,       /* CRC8 验证 */
    INTEGRITY_CRC16,      /* CRC16 验证 */
    INTEGRITY_CRC32,      /* CRC32 验证 */
    INTEGRITY_FULL,       /* 完全验证（多次读取比对） */
} integrity_level_t;

/**
 * 数据完整性元数据
 */
typedef struct {
    uint32_t magic;           /* 魔数，标识数据类型 */
    uint32_t version;         /* 数据版本 */
    uint32_t data_size;       /* 数据大小 */
    uint32_t checksum;        /* 校验和 */
    uint32_t crc32;           /* CRC32 */
    uint32_t reserved[8];    /* 保留字段 */
} integrity_metadata_t;

#define INTEGRITY_MAGIC       0x494E5445  /* "INTE" */
#define INTEGRITY_VERSION     1

/**
 * 写入带完整性保护的数据
 */
int nor_write_with_integrity(nor_device_t *dev, uint32_t addr,
                             const uint8_t *data, uint32_t len,
                             integrity_level_t level)
{
    int ret;
    uint32_t sector_addr = addr - (addr % dev->sector_size);
    uint32_t sector_size = dev->sector_size;

    /* 擦除整个扇区 */
    ret = nor_erase_sector(dev, sector_addr);
    if (ret != 0) {
        return ret;
    }

    /* 计算校验值 */
    uint32_t checksum = nor_checksum(data, len);
    uint32_t crc32 = nor_crc32(data, len);

    /* 写入元数据 */
    integrity_metadata_t meta = {
        .magic = INTEGRITY_MAGIC,
        .version = INTEGRITY_VERSION,
        .data_size = len,
        .checksum = checksum,
        .crc32 = crc32,
    };

    /* 写入元数据到扇区开头 */
    ret = nor_write(dev, sector_addr, (uint8_t *)&meta, sizeof(meta));
    if (ret != sizeof(meta)) {
        return ret;
    }

    /* 写入实际数据 */
    uint32_t data_addr = sector_addr + sizeof(meta);
    ret = nor_write(dev, data_addr, data, len);
    if (ret != (int)len) {
        return ret;
    }

    /* 如果需要填充整个扇区 */
    uint32_t used = sizeof(meta) + len;
    if (used < sector_size) {
        uint8_t fill_byte = 0xFF;
        ret = nor_write(dev, data_addr + len, &fill_byte,
                       sector_size - used);
    }

    return (int)len;
}

/**
 * 读取并验证带完整性保护的数据
 */
int nor_read_with_integrity(nor_device_t *dev, uint32_t addr,
                            uint8_t *data, uint32_t len,
                            integrity_level_t level)
{
    int ret;
    uint32_t sector_addr = addr - (addr % dev->sector_size);

    /* 读取元数据 */
    integrity_metadata_t meta;
    ret = nor_read(dev, sector_addr, (uint8_t *)&meta, sizeof(meta));
    if (ret != sizeof(meta)) {
        return ret;
    }

    /* 验证魔数 */
    if (meta.magic != INTEGRITY_MAGIC) {
        printf("Invalid magic number\n");
        return -NOR_ERR_VERIFY;
    }

    /* 验证版本 */
    if (meta.version != INTEGRITY_VERSION) {
        printf("Unsupported version: %lu\n", meta.version);
        return -NOR_ERR_VERIFY;
    }

    /* 读取实际数据 */
    uint32_t data_addr = sector_addr + sizeof(meta);
    ret = nor_read(dev, data_addr, data, len);
    if (ret != (int)len) {
        return ret;
    }

    /* 验证校验和 */
    uint32_t checksum = nor_checksum(data, len);
    if (checksum != meta.checksum) {
        printf("Checksum verification failed\n");
        return -NOR_ERR_VERIFY;
    }

    /* 验证 CRC32 */
    if (level >= INTEGRITY_CRC32) {
        uint32_t crc32 = nor_crc32(data, len);
        if (crc32 != meta.crc32) {
            printf("CRC32 verification failed\n");
            return -NOR_ERR_VERIFY;
        }
    }

    return (int)len;
}
```

---

## 4. 边界条件测试

边界条件测试验证系统在极端情况下行为的正确性和健壮性。

### 4.1 地址边界测试

验证地址边界处理是否正确：

```c
/**
 * 完整地址边界测试
 */
int nor_test_all_address_boundaries(void)
{
    uint8_t buf[32];
    uint32_t total_size = g_test_dev->total_size;
    int ret;

    printf("=== Address Boundary Tests ===\n");

    /* 测试1：地址 0 */
    printf("Test 1: Address 0... ");
    memset(buf, 0xAA, 32);
    ret = nor_write(g_test_dev, 0, buf, 32);
    if (ret != 32) { printf("FAILED\n"); return ret; }
    ret = nor_read(g_test_dev, 0, buf, 32);
    if (ret != 32) { printf("FAILED\n"); return ret; }
    printf("PASSED\n");

    /* 测试2：最后一个有效地址 */
    printf("Test 2: Last valid address... ");
    memset(buf, 0xBB, 32);
    ret = nor_write(g_test_dev, total_size - 32, buf, 32);
    if (ret != 32) { printf("FAILED\n"); return ret; }
    ret = nor_read(g_test_dev, total_size - 32, buf, 32);
    if (ret != 32) { printf("FAILED\n"); return ret; }
    printf("PASSED\n");

    /* 测试3：超出上界 */
    printf("Test 3: Beyond upper bound... ");
    ret = nor_write(g_test_dev, total_size, buf, 32);
    if (ret >= 0) { printf("FAILED (should fail)\n"); return -1; }
    printf("PASSED\n");

    /* 测试4：超出下界 */
    printf("Test 4: Below lower bound... ");
    ret = nor_write(g_test_dev, (uint32_t)-100, buf, 32);
    if (ret >= 0) { printf("FAILED (should fail)\n"); return -1; }
    printf("PASSED\n");

    /* 测试5：跨边界 */
    printf("Test 5: Cross boundary... ");
    memset(buf, 0xCC, 32);
    ret = nor_write(g_test_dev, total_size - 16, buf, 32);
    if (ret >= 0) { printf("FAILED (should fail)\n"); return -1; }
    printf("PASSED\n");

    return 0;
}
```

### 4.2 数据边界测试

验证各种数据长度和模式下的正确性：

```c
/**
 * 数据边界测试
 */
int nor_test_data_boundaries(void)
{
    uint8_t buf[512];
    int ret;

    printf("=== Data Boundary Tests ===\n");

    /* 测试各种长度 */
    uint32_t test_lengths[] = {1, 2, 3, 15, 16, 17, 31, 32, 33,
                               63, 64, 65, 127, 128, 129, 255, 256, 257, 511, 512};

    for (int i = 0; i < sizeof(test_lengths) / sizeof(test_lengths[0]); i++) {
        uint32_t len = test_lengths[i];

        /* 填充测试数据 */
        memset(buf, (uint8_t)(len & 0xFF), len);

        /* 写入 */
        ret = nor_write(g_test_dev, TEST_ADDR, buf, len);
        if (ret != (int)len) {
            printf("Write failed for length %lu\n", len);
            return ret;
        }

        /* 读取 */
        memset(buf, 0, len);
        ret = nor_read(g_test_dev, TEST_ADDR, buf, len);
        if (ret != (int)len) {
            printf("Read failed for length %lu\n", len);
            return ret;
        }

        /* 验证 */
        for (uint32_t j = 0; j < len; j++) {
            if (buf[j] != (uint8_t)(len & 0xFF)) {
                printf("Verify failed for length %lu at offset %lu\n", len, j);
                return -NOR_ERR_VERIFY;
            }
        }
    }

    printf("All data boundary tests passed\n");
    return 0;
}

/**
 * 数据模式测试
 */
int nor_test_data_patterns(void)
{
    uint8_t buf[256];
    uint32_t patterns[] = {
        0x00000000,
        0xFFFFFFFF,
        0xAAAAAAAA,
        0x55555555,
        0x12345678,
        0xAAAAAAAA,
    };
    int ret;

    printf("=== Data Pattern Tests ===\n");

    /* 测试各种数据模式 */
    for (int p = 0; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
        uint32_t pattern = patterns[p];

        /* 填充缓冲区 */
        for (int i = 0; i < 256; i += 4) {
            *(uint32_t *)&buf[i] = pattern;
        }

        /* 写入 */
        ret = nor_write(g_test_dev, TEST_ADDR, buf, 256);
        if (ret != 256) {
            printf("Write failed for pattern 0x%08X\n", pattern);
            return ret;
        }

        /* 读取 */
        memset(buf, 0, 256);
        ret = nor_read(g_test_dev, TEST_ADDR, buf, 256);
        if (ret != 256) {
            printf("Read failed for pattern 0x%08X\n", pattern);
            return ret;
        }

        /* 验证 */
        for (int i = 0; i < 256; i += 4) {
            if (*(uint32_t *)&buf[i] != pattern) {
                printf("Verify failed for pattern 0x%08X\n", pattern);
                return -NOR_ERR_VERIFY;
            }
        }
    }

    printf("All pattern tests passed\n");
    return 0;
}
```

### 4.3 异常情况测试

验证驱动在异常情况下的行为：

```c
/**
 * 异常情况测试
 */
int nor_test_error_conditions(void)
{
    uint8_t buf[32];
    int ret;

    printf("=== Error Condition Tests ===\n");

    /* 测试1：NULL 缓冲区 */
    printf("Test 1: NULL buffer... ");
    ret = nor_read(g_test_dev, TEST_ADDR, NULL, 32);
    if (ret >= 0) { printf("FAILED\n"); return -1; }
    printf("PASSED\n");

    /* 测试2：长度为 0 */
    printf("Test 2: Zero length... ");
    ret = nor_read(g_test_dev, TEST_ADDR, buf, 0);
    if (ret != 0) { printf("FAILED\n"); return -1; }
    printf("PASSED\n");

    /* 测试3：未初始化的设备 */
    printf("Test 3: Uninitialized device... ");
    nor_device_t uninit_dev;
    memset(&uninit_dev, 0, sizeof(uninit_dev));
    ret = nor_read(&uninit_dev, TEST_ADDR, buf, 32);
    if (ret >= 0) { printf("FAILED\n"); return -1; }
    printf("PASSED\n");

    /* 测试4：写入未擦除区域 */
    printf("Test 4: Write without erase... ");
    nor_erase_sector(g_test_dev, TEST_ADDR);
    nor_write(g_test_dev, TEST_ADDR, "Test", 4);
    /* 不擦除直接写入 */
    ret = nor_write(g_test_dev, TEST_ADDR, "Test", 4);
    if (ret < 0) {
        /* 某些实现会失败，某些会成功但数据不正确 */
        printf("PASSED (write without erase)\n");
    } else {
        /* 验证数据 */
        nor_read(g_test_dev, TEST_ADDR, buf, 4);
        printf("PASSED (write succeeded, data=%02X%02X%02X%02X)\n",
               buf[0], buf[1], buf[2], buf[3]);
    }

    return 0;
}
```

---

## 本章小结

本章详细介绍了 Nor Flash 测试与验证的完整方法体系，主要内容包括：

1. **单元测试方法**
   - 测试框架设计：测试用例注册、执行和结果报告
   - 基础功能测试：初始化、读写功能验证
   - 边界条件测试：地址边界、长度边界验证

2. **压力测试设计**
   - 连续读写压力测试：循环读写验证稳定性
   - 并发访问测试：多任务同时访问的可靠性
   - 掉电恢复测试：意外断电后的数据完整性

3. **数据完整性验证**
   - 校验和验证：CRC32、校验和、XOR 校验
   - 完整性架构：元数据设计、多级别验证

4. **边界条件测试**
   - 地址边界测试：边界地址和跨边界操作
   - 数据边界测试：各种数据长度的正确性
   - 异常情况测试：NULL 指针、零长度等异常处理

通过建立完善的测试体系，可以确保 Nor Flash 驱动的可靠性和稳定性，及时发现和修复潜在问题，为产品质量提供有力保障。

# 智能家居网关项目

## 项目概述

- **设备平台：** 基于 STM32F4 的智能家居网关
- **主要功能：** 支持 WiFi、BLE、ZigBee 多协议通信，远程控制，OTA 升级
- **软件架构：** FreeRTOS + LwIP + MbedTLS + Matter协议栈
- **项目规模：** 约 15000 行代码

## 系统架构

### 硬件资源

| 资源 | 规格 |
|------|------|
| MCU | STM32F429ZI (180MHz, 256KB RAM, 2MB Flash) |
| WiFi | ESP8266 (AT指令) |
| BLE | nRF52832 (BLE 5.0) |
| ZigBee | CC2530 (ZigBee 3.0) |
| 存储 | SPI Flash 8MB |

### 软件架构图

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐ │
│  │ HomeKit  │  │  Matter  │  │  MQTT   │  │  OTA   │ │
│  │  Task    │  │  Task    │  │  Task    │  │  Task  │ │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘  └────┬──┘ │
└────────┼─────────────┼─────────────┼─────────────┼────┘
         │             │             │             │
┌────────┼─────────────┼─────────────┼─────────────┼────┐
│        ▼             ▼             ▼             ▼    │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Communication Layer                 │   │
│  │  ┌────────┐  ┌────────┐  ┌────────┐  ┌───────┐  │   │
│  │  │  WiFi  │  │  BLE   │  │ ZigBee │  │  DB  │  │   │
│  │  │ Driver │  │ Driver │  │ Driver │  │Cache │  │   │
│  │  └────────┘  └────────┘  └────────┘  └───────┘  │   │
│  └─────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
         │             │             │             │
┌────────┼─────────────┼─────────────┼─────────────┼────┐
│        ▼             ▼             ▼             ▼    │
│  ┌─────────────────────────────────────────────────┐   │
│  │               FreeRTOS Kernel                    │   │
│  │  任务调度 │ 队列通信 │ 信号量同步 │ 内存管理   │   │
│  └─────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

## 任务划分与优先级

### 任务列表

| 任务名 | 优先级 | 栈大小 | 周期/触发 | 功能描述 |
|--------|--------|--------|-----------|----------|
| NetworkTask | 5 | 1024 | 事件触发 | WiFi/LTE 网络管理 |
| ProtocolTask | 4 | 1536 | 50ms 周期 | 协议解析与转换 |
| DeviceControlTask | 3 | 1024 | 事件触发 | 设备控制指令处理 |
| OTATask | 2 | 2048 | 事件触发 | OTA 升级管理 |
| SystemMonitorTask | 1 | 512 | 1s 周期 | 系统状态监控 |

### 优先级设计思路

```
优先级 5 (最高): NetworkTask
- 网络连接是所有功能的基础
- 需要快速响应网络状态变化

优先级 4: ProtocolTask  
- 协议解析有实时性要求
- 处理来自云端和本地的协议

优先级 3: DeviceControlTask
- 设备控制响应用户操作
- 需要及时但不是最高优先级

优先级 2: OTATask
- OTA 升级是后台任务
- 不影响正常功能

优先级 1 (最低): SystemMonitorTask
- 系统监控可以在系统空闲时运行
- 主要用于日志和状态上报
```

### 任务间通信

```
                    ┌──────────────┐
                    │  NetworkTask │
                    └──────┬───────┘
                           │ 网络状态变化
                           ▼
┌─────────────┐    ┌──────────────┐    ┌──────────────┐
│ DeviceCtrl  │◄───│ ProtocolTask │───►│  OTATask     │
│   Task      │    │              │    │              │
└──────┬──────┘    └──────┬───────┘    └──────────────┘
       │                  │
       │ 设备状态变化     │ 协议数据
       ▼                  ▼
┌──────────────────────────────────────────┐
│            队列/事件组                    │
│  - DeviceControlQueue (设备控制命令)     │
│  - DataReportQueue (数据上报)            │
│  - NetworkEventGroup (网络状态事件)      │
└──────────────────────────────────────────┘
       ▲                  ▲
       │                  │
       │ 周期/事件        │ 设备状态
       │                  │
┌──────┴──────┐    ┌──────┴──────┐
│ SystemMon   │    │ BLE/ZigBee  │
│   Task      │    │   Tasks     │
└─────────────┘    └─────────────┘
```

## 关键代码实现

### 1. 任务创建

```c
/* 设备控制任务 */
static BaseType_t DeviceControlTask_Create(void)
{
    TaskHandle_t xHandle = NULL;
    
    /* 创建设备控制队列 */
    DeviceControlQueue = xQueueCreate(10, sizeof(DeviceControlMsg_t));
    if (DeviceControlQueue == NULL) {
        return pdFAIL;
    }
    
    /* 创建任务 */
    BaseType_t xReturn = xTaskCreate(
        (TaskFunction_t)DeviceControlTask,      // 任务函数
        (const char *)"DeviceControl",           // 任务名称
        (uint32_t)1024,                          // 栈大小 (Words)
        (void *)NULL,                            // 参数
        (UBaseType_t)3,                          // 优先级
        (TaskHandle_t *)&xHandle                // 句柄
    );
    
    if (xReturn == pdPASS) {
        printf("DeviceControlTask created successfully\r\n");
    }
    
    return xReturn;
}
```

### 2. 队列通信

```c
/* 发送设备控制命令 */
static BaseType_t DeviceControl_SendCmd(DeviceControlMsg_t *msg)
{
    BaseType_t xReturn;
    
    /* 带超时发送，避免阻塞 */
    xReturn = xQueueSend(
        DeviceControlQueue,
        msg,
        pdMS_TO_TICKS(1000)
    );
    
    if (xReturn != pdTRUE) {
        printf("Failed to send device control cmd, queue full\r\n");
    }
    
    return xReturn;
}

/* 设备控制任务处理 */
static void DeviceControlTask(void *parameter)
{
    DeviceControlMsg_t msg;
    BaseType_t xResult;
    
    while (1) {
        /* 阻塞等待队列消息 */
        xResult = xQueueReceive(
            DeviceControlQueue,
            &msg,
            portMAX_DELAY
        );
        
        if (xResult == pdTRUE) {
            /* 处理控制命令 */
            switch (msg.cmd) {
                case CMD_DEVICE_ON:
                    Device_On(msg.device_id);
                    break;
                case CMD_DEVICE_OFF:
                    Device_Off(msg.device_id);
                    break;
                case CMD_DEVICE_SET:
                    Device_SetValue(msg.device_id, msg.value);
                    break;
                default:
                    break;
            }
        }
    }
}
```

### 3. 信号量同步

```c
/* WiFi 状态变化信号量 */
static SemaphoreHandle_t WiFiStatusSemaphore = NULL;

/* 初始化 WiFi 状态同步 */
static void WiFiStatus_SyncInit(void)
{
    /* 创建二值信号量 */
    WiFiStatusSemaphore = xSemaphoreCreateBinary();
    if (WiFiStatusSemaphore == NULL) {
        printf("WiFi semaphore create failed\r\n");
        return;
    }
    
    /* 创建 WiFi 状态处理任务 */
    xTaskCreate(
        WiFiStatusTask,
        "WiFiStatus",
        512,
        NULL,
        4,
        NULL
    );
}

/* WiFi 状态变化处理任务 */
static void WiFiStatusTask(void *parameter)
{
    while (1) {
        /* 等待信号量，阻塞 */
        if (xSemaphoreTake(WiFiStatusSemaphore, portMAX_DELAY) == pdTRUE) {
            /* 处理 WiFi 状态变化 */
            WiFiStatus_t status = WiFi_GetStatus();
            
            switch (status) {
                case WIFI_CONNECTED:
                    /* 重新连接 MQTT */
                    MQTT_Connect();
                    break;
                case WIFI_DISCONNECTED:
                    /* 断开 MQTT */
                    MQTT_Disconnect();
                    break;
                default:
                    break;
            }
        }
    }
}

/* WiFi 状态变化回调（在中 interrupt 中调用）*/
void WiFi_StatusCallback(WiFiStatus_t status)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* 在中断中释放信号量 */
    xSemaphoreGiveFromISR(WiFiStatusSemaphore, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
```

### 4. 事件组网络状态

```c
/* 网络事件组 */
static EventGroupHandle_t NetworkEventGroup = NULL;

/* 事件位定义 */
#define BIT_WIFI_CONNECTED    (1 << 0)
#define BIT_ETH_CONNECTED     (1 << 1)
#define BIT_MQTT_CONNECTED    (1 << 2)
#define BIT_CLOUD_CONNECTED  (1 << 3)

/* 初始化网络事件组 */
static void NetworkEvent_Init(void)
{
    NetworkEventGroup = xEventGroupCreate();
    
    if (NetworkEventGroup == NULL) {
        printf("Network event group create failed\r\n");
    }
}

/* 等待网络就绪 */
static BaseType_t Network_WaitReady(uint32_t timeout_ms)
{
    EventBits_t uxBits;
    
    uxBits = xEventGroupWaitBits(
        NetworkEventGroup,
        BIT_WIFI_CONNECTED | BIT_MQTT_CONNECTED,
        pdFALSE,          // 不清除已设置的位
        pdTRUE,           // 等待所有位
        pdMS_TO_TICKS(timeout_ms)
    );
    
    return (uxBits & (BIT_WIFI_CONNECTED | BIT_MQTT_CONNECTED)) ? pdTRUE : pdFAIL;
}

/* WiFi 连接成功处理 */
void WiFi_ConnectSuccess(void)
{
    xEventGroupSetBits(NetworkEventGroup, BIT_WIFI_CONNECTED);
}

/* MQTT 连接成功处理 */
void MQTT_ConnectSuccess(void)
{
    xEventGroupSetBits(NetworkEventGroup, BIT_MQTT_CONNECTED);
}
```

### 5. OTA 升级实现

```c
/* OTA 任务 */
static void OTATask(void *parameter)
{
    OTAContext_t ota_ctx;
    uint8_t buffer[1024];
    size_t bytes_read;
    
    while (1) {
        /* 等待 OTA 请求事件 */
        xEventGroupWaitBits(
            OTAEventGroup,
            BIT_OTA_START,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );
        
        /* 初始化 OTA 上下文 */
        OTA_InitContext(&ota_ctx);
        
        /* 下载固件 */
        while (1) {
            bytes_read = HTTP_Read(&ota_ctx, buffer, sizeof(buffer));
            
            if (bytes_read > 0) {
                /* 写入 Flash */
                Flash_Write(buffer, bytes_read);
                ota_ctx.downloaded_size += bytes_read;
                
                /* 打印进度 */
                printf("OTA Progress: %d%%\r\n", 
                    (ota_ctx.downloaded_size * 100) / ota_ctx.total_size);
            } else {
                break;
            }
        }
        
        /* 验证固件 */
        if (OTA_Verify(&ota_ctx) == pdTRUE) {
            /* 设置启动标志 */
            Flash_SetBootFlag(BOOT_NEW_FW);
            
            printf("OTA Success, rebooting...\r\n");
            System_Reboot();
        } else {
            printf("OTA Verify Failed\r\n");
            /* 恢复旧固件 */
            Flash_SetBootFlag(BOOT_OLD_FW);
        }
        
        /* 清除 OTA 开始事件 */
        xEventGroupClearBits(OTAEventGroup, BIT_OTA_START);
    }
}
```

## 项目面试重点

### 消费电子面试问题

**Q1: 如何设计低功耗模式？**
```
答案要点：
1. 使用 Tickless Idle 模式
2. 空闲任务中进入低功耗
3. 关闭不使用的外设时钟
4. 使用事件唤醒代替轮询
5. WiFi 模块使用低功耗模式
```

**Q2: OTA 升级如何保证可靠性？**
```
答案要点：
1. 双分区备份，升级失败可回滚
2. 下载校验（MD5/SHA256）
3. 断点续传支持
4. 升级过程不响应复位
5. 启动验证失败自动回滚
```

**Q3: 多协议通信如何设计？**
```
答案要点：
1. 统一协议抽象层
2. 各协议独立任务处理
3. 协议转换在中间层
4. 使用队列解耦各模块
5. 心跳检测连接状态
```

### 汽车电子延伸问题

**Q4: 如果用于车载环境需要做什么改进？**
```
答案要点：
1. 看门狗监控关键任务
2. 双备份固件机制
3. CAN 总线替代 WiFi/BLE
4. 功能安全等级 (ASIL)
5. 低温启动补偿
```

## 性能指标

| 指标 | 数值 |
|------|------|
| 系统启动时间 | < 2 秒 |
| 任务切换时间 | < 50us |
| 网络响应延迟 | < 200ms |
| 内存占用 | RAM: 180KB / 256KB |
| Flash 占用 | 1.2MB / 2MB |
| 功耗 (正常工作) | 80mA @ 5V |
| 功耗 (低功耗模式) | 5mA @ 5V |

## 技术总结

1. **任务划分清晰**：按功能模块划分，优先级合理
2. **通信机制选择正确**：队列用于数据传递，事件组用于状态同步，信号量用于简单同步
3. **低功耗设计**：使用 Tickless Idle + 外设电源管理
4. **OTA 可靠设计**：双分区 + 校验 + 回滚机制
5. **可扩展性强**：模块化设计便于添加新协议
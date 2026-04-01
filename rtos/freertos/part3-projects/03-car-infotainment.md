# 车载信息娱乐系统项目

## 项目概述

- **设备平台：** 基于 NXP i.MX8QM (Quad-core Cortex-A72 + Cortex-M4)
- **主要功能：** 数字仪表盘、车载信息娱乐、语音交互、CAN 总线通信、蓝牙电话
- **软件架构：** FreeRTOS (M4核) + Linux (A核) + AUTOSAR
- **安全等级：** ASIL-B

## 系统架构

### 硬件资源

| 资源 | 规格 |
|------|------|
| AP | i.MX8QM (4x Cortex-A72 + 2x Cortex-M4) |
| RAM | 4GB LPDDR4 |
| Display | 1920x720 LCD (仪表) + 1280x720 (中控) |
| Audio | 8通道 Class-D功放 |
| Connectivity | WiFi 6 / BT 5.0 / LTE Cat4 / CAN / USB |
| Storage | eMMC 32GB + SD卡槽 |

### 软件架构图

```
┌─────────────────────────────────────────────────────────────┐
│                      Linux (Cortex-A72)                     │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │ Android │  │ Qt UI   │  │ Media   │  │  Vehicle HAL   │ │
│  │ Auto    │  │ Cluster │  │ Server  │  │                 │ │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────────┬────────┘ │
└───────┼────────────┼────────────┼───────────────┼──────────┘
        │            │            │               │
        │   RPMSG    │   RPMSG    │    IPC       │
        │            │            │               │
┌───────┼────────────┼────────────┼───────────────┼──────────┐
│       ▼            ▼            ▼               ▼          │
│  ┌─────────────────────────────────────────────────────┐   │
│  │            FreeRTOS (Cortex-M4)                     │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐         │   │
│  │  │ CAN Task │  │Audio Task│  │ Sensor  │         │   │
│  │  │          │  │          │  │  Task   │         │   │
│  │  └──────────┘  └──────────┘  └──────────┘         │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐         │   │
│  │  │ Power    │  │ BT HFP   │  │Watchdog  │         │   │
│  │  │ Manager  │  │  Task    │  │  Task    │         │   │
│  │  └──────────┘  └──────────┘  └──────────┘         │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 任务划分与优先级

### 任务列表

| 任务名 | 优先级 | 栈大小 | 周期/触发 | 功能描述 | 安全等级 |
|--------|--------|--------|-----------|----------|----------|
| CANRxTask | 6 | 512 | 事件触发 | CAN 总线接收 | ASIL-B |
| AudioTask | 5 | 1024 | 事件触发 | 音频播放/处理 | ASIL-A |
| SensorTask | 5 | 512 | 20ms 周期 | 传感器数据采集 | ASIL-B |
| BTTask | 4 | 1024 | 事件触发 | 蓝牙通信 | QM |
| PowerTask | 3 | 512 | 100ms 周期 | 电源管理 | ASIL-A |
| UITask | 2 | 1536 | 33ms 周期 | UI 更新 | QM |
| MediaScanTask | 1 | 768 | 事件触发 | 媒体文件扫描 | QM |
| SystemMonitor | 1 | 512 | 1s 周期 | 系统监控 | QM |

### 优先级设计思路

```
优先级 6 (最高): CANRxTask - 安全关键，仪表盘显示需要
优先级 5: AudioTask / SensorTask - 实时性要求高
优先级 4: BTTask - 蓝牙电话需要及时响应
优先级 3: PowerTask - 电源管理需要及时处理
优先级 2: UITask - 界面更新 30fps
优先级 1 (最低): MediaScanTask / SystemMonitor - 后台任务
```

### 任务间通信

```
                        ┌─────────────┐
                        │  CANRxTask │
                        └──────┬──────┘
                               │ CAN 数据
            ┌──────────────────┼──────────────────┐
            ▼                  ▼                  ▼
    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
    │ AudioTask   │    │  UITask     │    │SensorTask   │
    │             │    │             │    │             │
    └──────┬──────┘    └──────┬──────┘    └─────────────┘
           │                   │
           │ 音频数据          │ 显示数据
           ▼                   ▼
    ┌───────────────────────────────────┐
    │          队列/事件组               │
    │  - CANDataQueue (CAN 数据)        │
    │  - AudioDataQueue (音频数据)      │
    │  - DisplayQueue (显示数据)        │
    │  - PowerEventGroup (电源事件)     │
    └───────────────────────────────────┘
           ▲                   ▲
           │                   │
           │ 周期/事件         │ 事件触发
           │                   │
    ┌──────┴──────┐    ┌──────┴──────┐
    │ MediaScan  │    │   BTTask    │
    │   Task     │    │              │
    └─────────────┘    └─────────────┘
```

## 关键代码实现

### 1. 安全关键 CAN 任务

```c
/* CAN 接收任务 - 安全关键 */
static void CANRxTask(void *parameter)
{
    CANMsg_t can_msg;
    BaseType_t xResult;
    
    /* 创建 CAN 接收队列 */
    CANDataQueue = xQueueCreate(20, sizeof(CANMsg_t));
    configASSERT(CANDataQueue != NULL);
    
    while (1) {
        /* 等待 CAN 数据，设置超时避免永久阻塞 */
        xResult = xQueueReceive(
            CANDataQueue,
            &can_msg,
            pdMS_TO_TICKS(100)
        );
        
        if (xResult == pdTRUE) {
            /* 处理 CAN 消息 */
            switch (can_msg.msg_id) {
                case CAN_MSG_SPEED:
                    /* 更新车速 - 发送给 UI 任务 */
                    DisplayData_t display_data = {
                        .type = DISPLAY_SPEED,
                        .value = can_msg.data.speed
                    };
                    xQueueSend(DisplayQueue, &display_data, 0);
                    break;
                    
                case CAN_MSG_ENGINE:
                    /* 更新发动机状态 */
                    EngineStatus_Update(&can_msg.data.engine);
                    break;
                    
                case CAN_MSG_WARNING:
                    /* 处理警告信息 */
                    Warning_Process(&can_msg.data.warning);
                    break;
                    
                default:
                    break;
            }
        } else {
            /* 超时 - 检查 CAN 总线是否正常 */
            if (++can_timeout_count > 10) {
                /* CAN 总线异常，上报错误 */
                Error_Report(ERROR_CAN_TIMEOUT);
                can_timeout_count = 0;
            }
        }
    }
}

/* CAN 接收中断处理 - 在中断中只做最小工作 */
void CAN_RxISR(void)
{
    CANMsg_t can_msg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* 从硬件读取 CAN 数据 */
    CAN_ReadFrame(&can_msg);
    
    /* 发送到队列，中断安全 */
    xQueueSendFromISR(CANDataQueue, &can_msg, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
```

### 2. 音频播放任务

```c
/* 音频任务 */
static void AudioTask(void *parameter)
{
    AudioBlock_t audio_block;
    BaseType_t xResult;
    
    /* 创建音频队列 */
    AudioDataQueue = xQueueCreate(8, sizeof(AudioBlock_t));
    configASSERT(AudioDataQueue != NULL);
    
    /* 初始化音频硬件 */
    Audio_Init();
    
    /* 设置音频回调函数 */
    Audio_SetCallback(Audio_Callback);
    
    while (1) {
        xResult = xQueueReceive(
            AudioDataQueue,
            &audio_block,
            portMAX_DELAY
        );
        
        if (xResult == pdTRUE) {
            /* 播放音频块 */
            Audio_PlayBlock(&audio_block);
            
            /* 音频缓冲低位检测 */
            if (Audio_GetBufferLevel() < LOW_THRESHOLD) {
                /* 通知源任务填充数据 */
                xEventGroupSetBits(AudioEventGroup, BIT_NEED_DATA);
            }
        }
    }
}

/* 音频缓冲回调 */
void Audio_Callback(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* 释放已播放的缓冲区 */
    Audio_ReleaseBuffer();
    
    /* 发送信号表示可以播放更多数据 */
    xEventGroupSetBitsFromISR(AudioEventGroup, BIT_BUFFER_FREE, 
                               &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
```

### 3. 电源管理任务

```c
/* 电源管理任务 - ASIL-A */
static void PowerTask(void *parameter)
{
    PowerState_t power_state = POWER_STATE_NORMAL;
    uint32_t tick_count = 0;
    
    /* 初始化电源管理硬件 */
    Power_Init();
    
    /* 初始化电源事件组 */
    PowerEventGroup = xEventGroupCreate();
    configASSERT(PowerEventGroup != NULL);
    
    while (1) {
        /* 100ms 周期 */
        vTaskDelay(pdMS_TO_TICKS(100));
        
        tick_count++;
        
        /* 检查电源状态 */
        PowerStatus_t status = Power_GetStatus();
        
        switch (power_state) {
            case POWER_STATE_NORMAL:
                if (status.battery < LOW_BATTERY_THRESHOLD) {
                    /* 进入低功耗 */
                    power_state = POWER_STATE_LOW_POWER;
                    /* 关闭非必要功能 */
                    Power_DisableModule(MODULE_WIFI);
                    Power_DisableModule(MODULE_BT);
                } else if (status.voltage < VOLTAGE_WARNING) {
                    /* 电压警告 */
                    xEventGroupSetBits(PowerEventGroup, BIT_VOLTAGE_WARNING);
                }
                break;
                
            case POWER_STATE_LOW_POWER:
                /* 检查是否可以退出低功耗 */
                if (status.battery >= LOW_BATTERY_EXIT_THRESHOLD) {
                    power_state = POWER_STATE_NORMAL;
                    Power_EnableModule(MODULE_WIFI);
                    Power_EnableModule(MODULE_BT);
                }
                /* 检查电池是否过低需要关机 */
                if (status.battery < BATTERY_CRITICAL) {
                    power_state = POWER_STATE_SHUTDOWN;
                    /* 保存关键数据 */
                    Data_SaveToFlash();
                }
                break;
                
            case POWER_STATE_SHUTDOWN:
                /* 发送关机请求到 Linux */
                RPC_Call("shutdown_system");
                /* 关闭外设 */
                Power_ShutdownPeripherals();
                break;
                
            default:
                break;
        }
        
        /* 周期性检查看门狗 */
        if (tick_count % 10 == 0) {  /* 每秒 */
            IWDG_Feed();
        }
    }
}

/* 低功耗模式实现 */
void Power_EnterLowPower(void)
{
    /* 暂停调度器 */
    vTaskSuspendAll();
    
    /* 配置唤醒源 */
    Power_ConfigWakeupSource(WAKEUP_CAN | WAKEUP_TIMER);
    
    /* 进入低功耗 */
    HAL_LP_EnterStopMode();
    
    /* 唤醒后恢复调度器 */
    xTaskResumeAll();
}
```

### 4. 看门狗监控

```c
/* 看门狗任务 - 监控所有安全关键任务 */
static void WatchdogTask(void *parameter)
{
    uint32_t tick_count = 0;
    
    /* 初始化看门狗 */
    IWDG_Init();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        tick_count++;
        
        /* 每秒检查所有任务状态 */
        if (tick_count % 10 == 0) {
            /* 检查 CAN 任务 */
            if (TaskMonitor_IsTaskAlive(CANRxTask_handle)) {
                CANTask_alive = pdTRUE;
            } else {
                CANTask_alive = pdFALSE;
                Error_Report(ERROR_CAN_TASK_DEAD);
            }
            
            /* 检查传感器任务 */
            if (TaskMonitor_IsTaskAlive(SensorTask_handle)) {
                SensorTask_alive = pdTRUE;
            } else {
                SensorTask_alive = pdFALSE;
                Error_Report(ERROR_SENSOR_TASK_DEAD);
            }
            
            /* 检查音频任务 */
            if (TaskMonitor_IsTaskAlive(AudioTask_handle)) {
                AudioTask_alive = pdTRUE;
            } else {
                AudioTask_alive = pdFALSE;
                Error_Report(ERROR_AUDIO_TASK_DEAD);
            }
            
            /* 如果任何关键任务异常，重置前尝试恢复 */
            if (!CANTask_alive || !SensorTask_alive) {
                /* 尝试重启任务 */
                TaskMonitor_Recover();
                
                /* 如果无法恢复，看门狗会超时复位 */
            }
        }
        
        /* 喂狗 */
        IWDG_Feed();
    }
}

/* 任务监控 - 检测任务是否正常运行 */
BaseType_t TaskMonitor_IsTaskAlive(TaskHandle_t task_handle)
{
    uint32_t current_tick = xTaskGetTickCount();
    TaskStatus_t status;
    
    vTaskGetTaskInfo(task_handle, &status, pdFALSE, eRunning);
    
    /* 检查任务是否在配置的周期内运行过 */
    if ((current_tick - status.ulRunTimeCounter) < TASK_TIMEOUT_TICKS) {
        return pdTRUE;
    }
    
    return pdFALSE;
}
```

### 5. 双备份固件升级

```c
/* 固件升级管理 - 支持 A/B 分区 */
static void FirmwareUpdateTask(void *parameter)
{
    FWUpdateEvent_t event;
    
    while (1) {
        /* 等待升级事件 */
        xEventGroupWaitBits(
            FWUpdateEventGroup,
            BIT_FW_UPDATE_START,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );
        
        /* 获取升级包信息 */
        FWInfo_t fw_info = FW_GetInfo();
        
        /* 验证固件签名 */
        if (Security_VerifySignature(fw_info.data, fw_info.size, 
                                      fw_info.signature) != pdTRUE) {
            Error_Report(ERROR_FW_SIGNATURE_INVALID);
            goto cleanup;
        }
        
        /* 下载到备份分区 */
        if (FW_DownloadToBackup(fw_info.url) != pdTRUE) {
            Error_Report(ERROR_FW_DOWNLOAD_FAILED);
            goto cleanup;
        }
        
        /* 验证备份固件 */
        if (FW_VerifyBackup() != pdTRUE) {
            Error_Report(ERROR_FW_VERIFY_FAILED);
            goto cleanup;
        }
        
        /* 设置下次启动标志 */
        Boot_SetNextPartition(PARTITION_B);
        
        /* 重启系统 */
        System_Reboot();
        
cleanup:
        xEventGroupClearBits(FWUpdateEventGroup, BIT_FW_UPDATE_START);
    }
}

/* 启动选择 - 双分区启动 */
void Boot_SelectPartition(void)
{
    PartitionInfo_t partition_a = Flash_GetPartitionInfo(PARTITION_A);
    PartitionInfo_t partition_b = Flash_GetPartitionInfo(PARTITION_B);
    
    /* 检查两个分区的有效性 */
    if (partition_a.valid && partition_b.valid) {
        /* 比较版本号，选择较新的 */
        if (partition_b.version > partition_a.version) {
            Boot_SetActivePartition(PARTITION_B);
        } else {
            Boot_SetActivePartition(PARTITION_A);
        }
    } else if (partition_a.valid) {
        Boot_SetActivePartition(PARTITION_A);
    } else if (partition_b.valid) {
        Boot_SetActivePartition(PARTITION_B);
    } else {
        /* 两个分区都无效，系统无法启动 */
        Error_Report(ERROR_NO_VALID_FW);
        Boot_EnterRecoveryMode();
    }
}
```

## 汽车电子面试重点

### Q1: 如何保证系统的实时性？

```
答案要点：
1. 合理分配任务优先级 - 安全关键任务最高优先级
2. 中断与任务配合 - 快速响应在中断，长处理在任务
3. 避免任务长时间阻塞 - 所有阻塞操作设置超时
4. 使用抢占式调度 - 确保高优先级任务立即执行
5. CAN 总线实时处理 - 10ms 周期数据处理

FreeRTOS 实时性保障机制：
- 抢占式调度保证高优先级任务先执行
- 时间片轮转保证同优先级任务公平
- 中断延迟处理模式减少中断处理时间
- 优先级继承解决优先级翻转
```

### Q2: 功能安全 (Functional Safety) 如何实现？

```
答案要点：
1. ASIL 分解
   - CAN 任务: ASIL-B
   - 传感器任务: ASIL-B
   - 电源任务: ASIL-A
   - UI 任务: QM (质量管理)

2. 看门狗监控
   - 监控安全关键任务
   - 任务异常检测
   - 自动恢复机制
   - 复位后状态记录

3. 双备份机制
   - A/B 分区固件
   - 启动失败自动回滚
   - 固件签名验证

4. 错误处理
   - 错误分类: 可恢复/不可恢复
   - 错误上报机制
   - 安全状态进入
```

### Q3: 如何处理 CAN 总线的实时性要求？

```
答案要点：
1. CAN 消息优先级
   - 安全相关消息高优先级
   - 信息娱乐消息低优先级

2. 接收处理
   - 中断接收，保证最小延迟
   - 队列缓冲，解耦处理
   - 10ms 周期任务处理

3. 发送策略
   - 周期性发送
   - 事件触发发送
   - 优先级调度

4. 错误处理
   - 总线错误检测
   - 节点离线处理
   - 故障码记录
```

### Q4: 车载环境对软件有什么特殊要求？

```
答案要点：
1. 温度范围
   - 工作温度: -40°C ~ 85°C
   - 启动温度: -40°C ~ 70°C
   - 低温补偿机制

2. 振动要求
   - 加速度: 6G
   - 振动测试
   - 连接器加固

3. 电源要求
   - 宽电压范围: 9V ~ 16V
   - 抛载测试
   - 低功耗模式

4. 电磁兼容
   - EMC 测试
   - 抗干扰设计

5. 长期运行
   - 内存碎片处理
   - 长期稳定性测试
   - 日志记录
```

## 性能指标

| 指标 | 数值 |
|------|------|
| 系统启动时间 | < 3 秒 (冷启动) |
| CAN 响应延迟 | < 5ms |
| UI 帧率 | 30 fps |
| 音频延迟 | < 50ms |
| 内存占用 | RAM: 512KB / 1MB |
| Flash 占用 | 2.5MB / 8MB |
| 功耗 (正常工作) | 1.5W @ 12V |
| 功耗 (待机) | < 50mW @ 12V |
| 工作温度 | -40°C ~ +85°C |

## 技术总结

1. **安全设计**：ASIL 分解、看门狗、双备份、固件签名
2. **实时性保证**：合理优先级、中断处理、队列解耦
3. **可靠性设计**：任务监控、错误恢复、日志记录
4. **电源管理**：低功耗模式、唤醒源管理、电池保护
5. **扩展性强**：模块化设计、支持多协议、灵活配置
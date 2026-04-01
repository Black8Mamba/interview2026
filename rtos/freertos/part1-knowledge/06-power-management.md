# 功耗管理 (Power Management)

功耗管理是嵌入式系统中的重要课题，尤其是在电池供电的设备和需要低功耗运行的场景中。FreeRTOS 提供了完善的功耗管理机制，帮助开发者在系统性能和功耗之间取得平衡。

## Tickless Idle ⭐⭐⭐

Tickless Idle 是 FreeRTOS 中最重要的低功耗特性，通过在系统空闲时动态调整或关闭系统节拍定时器来实现省电。

### 原理

#### 空闲时关闭 Tick

在传统的 RTOS 系统中，即使系统处于空闲状态（所有任务都在等待），系统节拍定时器（SysTick）仍会定期中断（通常每 1ms 或 10ms 一次），导致 CPU 无法进入深度睡眠状态。Tickless Idle 模式的核心思想是：

- 检测到系统空闲时，停止系统节拍定时器
- CPU 可以进入更深度的睡眠状态（如 Deep Sleep）
- 根据下一个任务的就绪时间，计算需要睡眠的时长
- 使用低功耗定时器在指定时间唤醒系统

#### 定时唤醒

Tickless Idle 的唤醒机制依赖于两个时间参数：

1. **ExpectedIdleTime**：预期空闲时间，即系统可以进入低功耗模式的最长时间
2. **下一个任务的就绪时间**：FreeRTOS 会计算最近就绪任务的时间

当 CPU 进入 Tickless Idle 模式后，会配置一个低功耗定时器（如 RTC、Low Power Timer）在指定时间唤醒，确保不会错过任何任务的截止时间。

#### configUSE_TICKLESS_IDLE 配置

```c
// FreeRTOSConfig.h
#define configUSE_TICKLESS_IDLE  1
```

启用 Tickless Idle 功能后，FreeRTOS 会在空闲任务中自动调用低功耗相关函数。

### 配置

#### ExpectedIdleTime

```c
// FreeRTOSConfig.h
// 定义系统预期的最大空闲时间（单位：tick）
// 如果空闲时间小于此值，则不会进入 Tickless Idle
#define configExpectedIdleTimeBeforeSleep  2
```

此配置定义了系统进入低功耗模式所需的最小空闲时间。如果系统空闲时间太短，频繁进出低功耗模式的开销可能反而大于节省的功耗。

#### 低功耗 tick 调整

当系统从低功耗模式唤醒时，需要调整系统节拍计数，确保时间准确性：

```c
// Tickless Idle 实现伪代码
void vPortSuppressTicksAndSleep(uint32_t expectedIdleTime)
{
    uint32_t currentTickCount = xTickCount;
    uint32_t wakeTickCount = currentTickCount + expectedIdleTime;
    
    // 配置低功耗定时器唤醒
    LPTIM_Config(wakeTickCount);
    
    // 进入低功耗模式
    Enter_LowPower_Mode();
    
    // 唤醒后调整 tick 计数
    xTickCount = wakeTickCount;
    
    // 更新任务等待时间
    vTaskStepTick(wakeTickCount - currentTickCount);
}
```

### Tickless Idle 工作流程

1. **空闲任务检测**：当空闲任务运行时，检查是否满足进入低功耗模式的条件
2. **计算空闲时间**：获取下一个任务的就绪时间，计算最大可睡眠时长
3. **验证空闲时间**：确认空闲时间大于 `configExpectedIdleTimeBeforeSleep`
4. **配置唤醒源**：设置低功耗定时器唤醒时间
5. **进入低功耗**：CPU 进入睡眠或深度睡眠模式
6. **唤醒处理**：定时器唤醒后，恢复系统节拍，调整时间计数
7. **任务调度**：检查是否有高优先级任务就绪，进行调度

### 注意事项

- **中断唤醒**：任何外设中断都可以唤醒系统，但不会恢复 tick 计数
- **时间精度**：长时间睡眠后，需要与外部时间源同步
- **功耗模式选择**：根据 expectedIdleTime 选择合适的睡眠深度

## 功耗模式

不同的功耗模式在性能和功耗之间提供不同的权衡。

### Active（主动模式）

#### 特性

- CPU 以最高频率运行
- 所有外设正常工作
- 所有时钟处于活动状态
- 功耗最高

#### 适用场景

- 需要快速响应的任务执行
- 数据处理密集型操作
- 用户交互响应

#### 功耗水平

典型 Cortex-M4 芯片在 Active 模式下功耗约为 50-150mA（取决于主频和负载）。

### Sleep（睡眠模式）

#### 特性

- CPU 停止执行指令（内核时钟停止）
- 外设可以继续工作
- 内存保持
- 唤醒源配置灵活
- 唤醒时间短（通常几微秒）

#### 唤醒源

- 外部中断（EXTI）
- 定时器中断
- 通信接口中断（UART、SPI、I2C）
- GPIO 状态变化

#### 功耗水平

典型 Cortex-M 芯片在 Sleep 模式下功耗约为 10-50mA。

#### 配置示例

```c
// 进入睡眠模式
void Enter_Sleep_Mode(void)
{
    // 配置唤醒源
    Enable_Wakeup_Source(WAKEUP_EXTI);
    
    // 进入睡眠
    __WFI();  // Wait For Interrupt
    
    // 唤醒后继续执行
}
```

### Deep Sleep（深度睡眠模式）

#### 特性

- 保留 RAM 内容（部分或全部）
- 更多时钟域关闭
- 唤醒时间较长（通常几十微秒到毫秒级）
- 功耗显著降低
- 可能需要恢复过程

#### 保留内容

- 全部 RAM 保留：功耗较高，但无需保存/恢复状态
- 部分 RAM 保留：仅保留关键数据，需要软件管理状态
- 仅 CPU 寄存器：需要完整的初始化流程

#### 功耗水平

典型 Cortex-M 芯片在 Deep Sleep 模式下功耗约为 1-10mA，极低功耗芯片可达亚微安级。

#### 配置示例

```c
// 进入深度睡眠
void Enter_DeepSleep_Mode(void)
{
    // 保存关键状态到保留 RAM
    Save_Critical_State();
    
    // 配置深度睡眠唤醒源
    Enable_Wakeup_Source(WAKEUP_RTC);
    Enable_Wakeup_Source(WAKEUP_GPIO);
    
    // 配置保留 RAM 域
    Configure_Retention_RAM();
    
    // 进入深度睡眠
    __DSB();
    __WFI();
    
    // 唤醒后恢复
    Restore_Critical_State();
}
```

### 功耗模式对比

| 模式 | CPU 状态 | 唤醒时间 | 典型功耗 | 保留内容 |
|------|----------|----------|----------|----------|
| Active | 运行 | 即时 | 50-150mA | 全部 |
| Sleep | 停止 | <10μs | 10-50mA | 全部 |
| Deep Sleep | 停止 | 50μs-1ms | 1-10mA | 部分 RAM |

## 消费电子重点

消费电子产品（如智能家居设备、可穿戴设备、IoT 传感器）对功耗有严格要求，直接影响用户体验和产品竞争力。

### OTA 更新功耗

OTA（Over-The-Air）更新是智能设备的重要功能，需要在更新过程中合理管理功耗：

#### 更新阶段功耗管理

1. **下载阶段**
   - 使用 Wi-Fi/蓝牙传输
   - 可以进入浅睡眠，定期唤醒检查数据
   - 功耗：中等

2. **校验阶段**
   - 需要 CPU 全速运行进行哈希计算
   - 可在 Active 模式完成，时间较短

3. **写入阶段**
   - Flash 写入需要较高电压
   - 短时峰值功耗较高
   - 建议在充电状态下完成

#### 功耗优化策略

```c
// OTA 更新功耗管理示例
void OTA_Update_Process(void)
{
    // 1. 下载阶段：低功耗等待
    Set_Power_Mode(SLEEP_MODE);
    while (!Download_Complete()) {
        // 定期唤醒检查数据
        Wait_For_Data_Interrupt();
        Process_Incoming_Data();
        Enter_Sleep_Mode();
    }
    
    // 2. 校验阶段：全速运行
    Set_Power_Mode(ACTIVE_MODE);
    if (!Verify_Image()) {
        OTA_Error();
        return;
    }
    
    // 3. 写入阶段
    Set_Power_Mode(ACTIVE_MODE);
    Flash_Update();
    
    // 4. 重启
    Reboot();
}
```

### 电池续航优化

#### 动态功耗管理策略

1. **任务分级**
   - 关键任务：实时响应，保持 Active 模式
   - 周期任务：合并执行，减少唤醒次数
   - 非紧急任务：延迟处理，利用睡眠周期

2. **智能唤醒**
   - 传感器数据变化唤醒
   - 定时唤醒 + 事件驱动唤醒
   - 自适应唤醒间隔

3. **外设管理**
   - 不使用的外设彻底断电
   - 使用时序控制外设开关
   - 选择低功耗外设型号

#### 功耗优化示例

```c
// 传感器采样优化
void Sensor_Sampling_Task(void *param)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    while (1) {
        // 根据电池电量调整采样间隔
        uint32_t interval = Get_Optimal_Interval();
        
        // 进入低功耗前关闭所有非必要外设
        Disable_Peripherals();
        Enable_Sensor_Power();
        
        // 采样
        Read_Sensor_Data();
        
        // 关闭传感器
        Disable_Sensor_Power();
        
        // 等待下次采样
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(interval));
    }
}

// 根据电池电量动态调整间隔
uint32_t Get_Optimal_Interval(void)
{
    uint32_t batteryLevel = Get_Battery_Level();
    
    if (batteryLevel > 70) {
        return 1000;  // 高电量：1秒间隔
    } else if (batteryLevel > 30) {
        return 5000;  // 中电量：5秒间隔
    } else {
        return 30000; // 低电量：30秒间隔
    }
}
```

#### 低功耗设计原则

1. **静态功耗优先**：选择低泄漏电流的芯片和器件
2. **动态功耗管理**：根据工作负载动态调整功耗模式
3. **唤醒效率**：最小化唤醒时间和能耗
4. **批量处理**：集中处理数据，减少 CPU 唤醒次数
5. **智能传感器**：使用具有智能功能的传感器，减少主控负载

## 汽车电子重点

汽车电子系统对功耗管理有特殊要求，需要在低功耗和快速响应之间取得平衡。

### 启动时间要求

#### 上电启动时间

汽车 ECU（Electronic Control Unit）通常要求上电后在毫秒级时间内响应：

- **Cold Start（冷启动）**：50-200ms 完成基本初始化
- **Warm Start（温启动）**：10-50ms 恢复运行状态

#### 低功耗模式下的启动

```c
// 汽车 ECU 启动策略
typedef enum {
    POWER_ON_RESET,      // 上电复位
    DEEP_SLEEP_WAKEUP,   // 深度睡眠唤醒
    SLEEP_WAKEUP,        // 睡眠唤醒
    NORMAL_RESET        // 正常复位
} Boot_Type_e;

void ECU_Boot(Boot_Type_e bootType)
{
    switch (bootType) {
        case POWER_ON_RESET:
            // 完整初始化
            Full_Peripheral_Init();
            Load_Calibration_Data();
            break;
            
        case DEEP_SLEEP_WAKEUP:
            // 恢复保留 RAM 中的状态
            Restore_From_Retention_RAM();
            Reinitialize_Essential_Peripherals();
            break;
            
        case SLEEP_WAKEUP:
            // 快速恢复
            Restore_Context();
            break;
            
        case NORMAL_RESET:
            // 软件复位，恢复运行
            Restore_Context();
            break;
    }
}
```

### 唤醒响应速度

#### 唤醒源优先级

汽车系统中，不同唤醒源的响应时间要求不同：

| 唤醒源 | 响应时间 | 优先级 |
|--------|----------|--------|
| 碰撞传感器 | <1ms | 最高 |
| 防盗系统 | <5ms | 高 |
| 钥匙信号 | <10ms | 中 |
| 定时唤醒 | 可配置 | 低 |
| CAN 消息 | <5ms | 高 |

#### 快速唤醒实现策略

1. **预启动序列**
   - 关键外设提前初始化
   - 保留运行状态在 RAM 中

2. **分层唤醒**
   - 第一层：快速响应唤醒源
   - 第二层：完整功能恢复

3. **事件驱动**
   - 中断驱动的快速响应
   - 避免轮询

```c
// 汽车 ECU 快速唤醒实现
// 保留 RAM 区域（深度睡眠不丢失）
__attribute__((section(".retention_ram")))
volatile ECU_Context_s g_ecuContext;

// 唤醒中断处理
void WWDG_Wakeup_IRQHandler(void)
{
    // 清除唤醒标志
    Clear_Wakeup_Flag();
    
    // 快速恢复关键功能
    Enable_Clock_Domain(ESSENTIAL_CLOCKS);
    Reinitialize_WWDG();
    
    // 通知系统唤醒
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(xWdogTaskHandle, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// 保留上下文恢复
void Restore_ECU_Context(void)
{
    // 恢复 CAN 控制器状态
    CAN_Restore_State(&g_ecuContext.canState);
    
    // 恢复通信缓冲区
    Restore_Communication_Buffers();
    
    // 恢复传感器数据
    Restore_Sensor_Data();
}
```

#### 汽车级低功耗要求

1. **工作温度范围**：-40°C 至 85°C 或更宽
2. **可靠性**：满足汽车级可靠性标准
3. **EMI/EMC**：低功耗模式下的电磁兼容性
4. **诊断能力**：功耗异常检测和报告
5. **安全相关**：安全关键系统的冗余设计

#### 典型汽车 ECU 功耗要求

| 工作模式 | 典型功耗 | 典型应用 |
|----------|----------|----------|
| Active | 50-200mA | 正常运行 |
| Sleep | 5-20mA | 停车状态 |
| Deep Sleep | <5mA | 长时间停车 |
| Power Off | <100μA | 电池断开 |

## 总结

功耗管理是嵌入式系统设计中的关键环节，需要根据应用场景选择合适的策略：

- **消费电子**：注重电池续航，通过动态调整和智能唤醒优化功耗
- **汽车电子**：注重快速启动和可靠唤醒，满足严格的时间要求
- **工业应用**：平衡可靠性和能效，适应恶劣环境

FreeRTOS 的 Tickless Idle 机制为嵌入式系统提供了开箱即用的低功耗解决方案，配合各芯片厂商的低功耗特性，可以实现优秀的功耗表现。

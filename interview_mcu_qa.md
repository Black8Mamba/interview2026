# 嵌入式MCU面试题汇总

## 1. ARM Cortex-M系列基础

### 1.1 Cortex-M3/M4/M7/M33的区别是什么？

**答案：**

| 特性 | Cortex-M3 | Cortex-M4 | Cortex-M7 | Cortex-M33 |
|------|-----------|-----------|-----------|------------|
| 架构版本 | ARMv7-M | ARMv7E-M | ARMv7E-M | ARMv8-M |
| 指令集 | Thumb-2 | Thumb-2 + DSP | Thumb-2 + DSP | Thumb-2 + DSP + 安全扩展 |
| DSP指令 | 无 | 有 | 有 | 有 |
| FPU | 可选 | 可选(单精度) | 可选(单/双精度) | 可选 |
| 缓存 | 可选 | 可选 | 可选(I/D-Cache) | 可选 |
| MPU | 有 | 有 | 有 | 有(支持PXN) |
| 性能(DMIPS/MHz) | 1.25 | 1.5 | 2.14 | 1.5 |
| 典型应用 | 工业控制 | 电机控制 | 高性能应用 | IoT安全设备 |

**M33相比前代的改进：**
- 引入TrustZone安全扩展，实现安全/非安全隔离
- 增强的MPU支持特权和非特权模式
- 改进的异常处理模型
- 支持浮点单元的更灵活配置

---

### 1.2 请说明Cortex-M的寄存器组

**答案：**

Cortex-M处理器包含以下核心寄存器：

```
通用寄存器:
R0-R7: 低寄存器，Thumb指令可访问
R8-R12: 高寄存器，某些指令限制

特殊寄存器:
R13(SP): 栈指针
  - Main Stack Pointer (MSP): 复位后默认使用
  - Process Stack Pointer (PSP): 用户线程使用

R14(LR): 链接寄存器，保存函数返回地址

R15(PC): 程序计数器

程序状态寄存器(xPSR):
  - APSR: 应用状态标志(N,Z,C,V,Q)
  - IPSR: 中断号
  - EPSR: 执行状态
```

** CONTROL寄存器：**
- bit[0]: 0=特权模式, 1=用户模式
- bit[1]: 0=使用MSP, 1=使用PSP

---

### 1.3 Cortex-M的中断处理流程是什么？

**答案：**

```
中断响应时间组成:
1. 指令完成延迟: 1-2个周期
2. 异常进入: 12个周期(保存状态)
3. 向量获取: 1-3个周期
4. 总计: 约16个周期

中断处理流程:
1. 异常检测: 指令执行期间检测
2. 异常进入:
   - xPSR, PC, LR, R12, R3-R0入栈
   - 更新PSR, PC, LR
   - 加载向量到PC
3. 中断执行: 执行中断服务程序
4. 异常退出:
   - 恢复寄存器
   - 返回操作触发栈弹出
```

**中断尾链(Tail-chaining)：**
当一个中断处理完毕后，如果还有挂起的中断，可以直接跳转到下一个中断处理，减少开销。

---

### 1.4 NVIC有哪些特性？

**答案：**

**Nested Vectored Interrupt Controller (NVIC) 特性：**

1. **可编程优先级**
   - 优先级位数: Cortex-M3/M4为8位(256级)，M7/M33可配置
   - 优先级分组: 抢占优先级和子优先级

2. **中断使能/挂起**
   - 每个中断独立使能控制
   - 可查询挂起状态

3. **中断管理**
   - 支持嵌套中断
   - 支持咬尾中断
   - 支持动态修改优先级

4. **特殊中断**
   - NMI(不可屏蔽中断)
   - SysTick定时器中断
   - PendSV(可挂起系统服务)

**编程示例：**
```c
// 设置优先级分组
NVIC_SetPriorityGrouping(3);  // 3位抢占, 1位子优先级

// 使能中断
NVIC_EnableIRQ(TIM2_IRQn);

// 设置优先级
NVIC_SetPriority(TIM2_IRQn, 5);

// 检查挂起状态
if(NVIC_GetPendingIRQ(TIM2_IRQn)) {
    NVIC_ClearPendingIRQ(TIM2_IRQn);
}
```

---

### 1.5 MPU(内存保护单元)的作用是什么？如何配置？

**答案：**

**MPU作用：**
1. 内存区域保护: 定义不同区域的访问权限
2. 防止非法内存访问
3. 实现用户/特权模式隔离
4. 支持内存属性配置(Cache, Shareable等)

**典型配置示例：**
```c
void MPU_Config(void) {
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    // 禁止MPU，进行配置
    HAL_MPU_Disable();

    // 配置Flash区域: 只读, 执行, Cache
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x08000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_PRERO;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 配置SRAM区域: 读写, 无Cache(用于DMA)
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x20000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    // 使能MPU
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

**典型应用场景：**
- 保护关键数据(如栈边界)
- 隔离用户任务内存
- 配置DMA缓冲区(关闭Cache避免一致性问题)

---

## 2. 中断与异常

### 2.1 什么是中断延迟？如何优化？

**答案：**

**中断延迟定义：**
从外设中断信号有效到CPU开始执行中断服务程序(ISR)第一条指令的时间。

**中断延迟组成：**
```
中断延迟 = 检测延迟 + 响应延迟 + 入口开销

检测延迟: 1个周期(每个周期检测一次)
响应延迟: 当前指令完成时间
入口开销: 12个周期(入栈+向量取址)
```

**典型值：**
- Cortex-M3/M4: 12-16个周期
- Cortex-M7: 12-20个周期

**优化方法：**
```c
// 1. 使用中断优先级优化
// 高优先级中断可打断低优先级

// 2. 中断处理函数简化
void TIM2_IRQHandler(void) {
    // 最小化处理: 只设置标志
    g_tim2_flag = 1;
    TIM2->SR = 0;  // 清除中断标志
    // 业务逻辑放到主循环或RTOS任务中
}

// 3. 使用DMA减少中断频率
// UART接收使用DMA instead of interrupt

// 4. 中断线程化(Linux)/Deferred Call(RTOS)
// 复杂处理延迟到任务中执行
```

---

### 2.2 什么是PendSV异常？有什么作用？

**答案：**

**PendSV(可挂起系统服务)特性：**
1. 可软件触发: 通过设置ICSR.PENDSVSET
2. 优先级可编程: 可设置为最低优先级
3. 常用于RTOS上下文切换

**典型应用 - RTOS上下文切换：**
```c
// 在SysTick中断中触发PendSV
void SysTick_Handler(void) {
    // 计数处理
    // 触发PendSV进行上下文切换
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

// PendSV处理(最低优先级)
void PendSV_Handler(void) {
    // 保存当前任务上下文
    // 选择下一个任务
    // 恢复新任务上下文
}
```

**为什么用PendSV而不是直接在SysTick切换：**
- 可将上下文切换延迟到所有中断处理完成后
- 确保中断响应实时性
- 支持在更高优先级ISR中使用OS API

---

### 2.3 什么是中断嵌套？需要注意什么？

**答案：**

**中断嵌套机制：**
```
优先级高可以打断优先级低的中断
优先级相同按向量表顺序
```

**配置示例：**
```c
// 设置优先级分组
// 假设使用4位优先级，2位分组
// 抢占优先级: 0-15, 子优先级: 0-3
NVIC_SetPriorityGrouping(0x03);  // 2位抢占, 2位子优先级

// TIM2中断: 抢占2, 子优先级0
NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(2, 2, 0));

// USART中断: 抢占3, 子优先级0
NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(2, 3, 0));
// TIM2可以打断USART1的中断处理
```

**注意事项：**
1. **栈空间**: 嵌套会使用更多栈空间，需确保栈足够大
2. **优先级设置**: 合理规划优先级避免优先级反转
3. **临界区**: 在临界区中应禁止中断或使用BASEPRI
4. **中断延迟**: 嵌套会增加整体中断响应时间

---

## 3. DMA与缓存

### 3.1 DMA与缓存一致性问题如何解决？

**答案：**

**问题根源：**
```
CPU写入 -> 写入Cache但未写入Memory
DMA从Memory读取 -> 读到旧数据

DMA写入 -> 写入Memory但Cache未更新
CPU从Cache读取 -> 读到旧数据
```

**解决方案：**

**方案1: 禁用Cache**
```c
// MPU配置DMA缓冲区为Non-Cacheable
MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
```
适用于: 频繁DMA操作的缓冲区

**方案2: 软件维护一致性**
```c
// CPU写DMA缓冲区后，清除Cache
void DMA_Clean(void *buf, uint32_t size) {
    SCB_InvalidateDCache_by_Addr((uint32_t*)buf, size);
}

// CPU读DMA缓冲区前，使无效Cache
void DMA_Invalidate(void *buf, uint32_t size) {
    SCB_InvalidateDCache_by_Addr((uint32_t*)buf, size);
}

// 使用示例
void UART_RX_DMA_Handler(void) {
    // DMA完成，处理数据前使无效Cache
    DMA_Invalidate(rx_buffer, BUFFER_SIZE);
    // 处理数据...
}
```

**方案3: 使用硬件一致性(DMA with Cache Coherency)**
```c
// 部分MCU支持硬件维护
// 如STM32H7的D-Cache维护由DMA自动处理
// 但仍需在特定场景手动处理
```

**最佳实践：**
1. DMA缓冲区使用Non-Cacheable区域
2. 必须使用Cache时，保证正确的Clean/Invalidate顺序
3. 多核系统需考虑更多一致性维护

---

### 3.2 DMA有哪些工作模式？

**答案：**

**1. 循环模式(Circular Mode)**
```c
hdma_usart_rx.Instance = DMA1_Stream0;
hdma_usart_rx.Init.Mode = DMA_CIRCULAR;  // 循环模式
hdma_usart_rx.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
```
- 自动循环，不需要重新配置
- 适合连续数据采集(UART, ADC)

**2. 双缓冲模式(Double Buffer)**
```c
hdma.Init.Mode = DMA_DOUBLE_BUFFER;
hdma.Init.DoubleBufferMode = DMA_DOUBLE_BUFFER_MODE_ENABLE;
hdma.Init.Memory0BaseAddr = (uint32_t)buffer0;
hdma.Init.Memory1BaseAddr = (uint32_t)buffer1;
```
- 两个缓冲区交替使用
- 可在处理一个缓冲区时DMA填充另一个

**3. 内存到内存模式**
```c
hdma.Init.Direction = DMA_MEMORY_TO_MEMORY;
hdma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
```
- 不需要外设触发
- 使用软件触发或定时触发
- 适合数据复制

**4. 流控制模式**
```c
// 硬件流控制
hdma.Init.Request = DMA_REQUEST_USART1_RX;

// 或软件流控制，通过手动触发
hdma.Init.Trigger = DMA_TRIG_SOFTWARE;
```

---

### 3.3 DMA传输异常如何处理？

**答案：**

**常见异常及处理：**

**1. 传输错误**
```c
void DMA1_Stream0_IRQHandler(void) {
    if(__HAL_DMA_GET_FLAG_SOURCE(&hdma_usart_rx, DMA_FLAG_TCIF0_4)) {
        // 传输完成
        __HAL_DMA_CLEAR_FLAG(&hdma_usart_rx, DMA_FLAG_TCIF0_4);
    }

    if(__HAL_DMA_GET_FLAG_SOURCE(&hdma_usart_rx, DMA_FLAG_TEIF0_4)) {
        // 传输错误处理
        // 1. 停止DMA
        HAL_DMA_Abort(&hdma_usart_rx);

        // 2. 重新配置
        hdma_usart_rx.Instance->CR &= ~DMA_SxCR_EN;
        hdma_usart_rx.Instance->CR |= DMA_SxCR_EN;

        // 3. 记录错误
        error_count++;
        log_error("DMA transfer error");
    }
}
```

**2. FIFO错误**
- 检查FIFO阈值配置
- 验证数据宽度匹配

**3. 缓冲区溢出**
- 使用双缓冲或循环模式
- 增加缓冲区大小
- 提高处理优先级

---

## 4. 通信接口

### 4.1 I2C主从通信配置要点

**答案：**

**I2C主模式配置：**
```c
void I2C_Master_Config(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;      // 400KHz
    hi2c1.Init.DutyCycle = I2C_DUTY_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

// 发送数据
HAL_I2C_Master_Transmit(&hi2c1, slave_addr<<1, data, len, timeout);

// 接收数据
HAL_I2C_Master_Receive(&hi2c1, slave_addr<<1, data, len, timeout);
```

**I2C从模式配置：**
```c
void I2C_Slave_Config(void) {
    hi2c1.Init.OwnAddress1 = 0x50;  // 从机地址
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    HAL_I2C_Init(&hi2c1);

    // 使能地址匹配中断
    HAL_I2C_EnableListen_IT(&hi2c1);
}
```

**常见问题及解决方案：**

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| ACK未收到 | 从机地址错误/线路问题 | 检查地址，上拉电阻 |
| 总线忙 | 上次通信未正确结束 | 发送STOP或总线复位 |
| 数据错误 | 时钟过快/干扰 | 降低时钟，添加滤波 |
| 死锁(SCL常低) | 从机拉低时钟 | 使用超时检测，总线复位 |

---

### 4.2 SPI双机通信配置要点

**答案：**

**SPI主机配置：**
```c
void SPI_Master_Config(void) {
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    HAL_SPI_Init(&hspi1);
}

// 全双工发送接收
HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, len, timeout);
```

**SPI从机配置：**
```c
void SPI_Slave_Config(void) {
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_SLAVE;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
    HAL_SPI_Init(&hspi1);
}
```

**多从机配置：**
```c
// 方法1: 软件控制NSS
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // 选择从机
HAL_SPI_Transmit(&hspi1, data, len, timeout);
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);    // 取消选择

// 方法2: 使用硬件NSS管理
hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
```

**常见问题：**
1. **数据错位**: 检查CPOL/CPHA配置是否匹配
2. **速度过快**: 从机可能不支持高速，降低波特率
3. **MOSI/MISO连接错误**: 确认连接正确

---

### 4.3 CAN总线错误处理机制

**答案：**

**CAN错误状态：**
```
Error Active: 发送错误计数<128, 正常通信
Error Passive: 128<=发送错误计数<256, 受限通信
Bus Off: 发送错误计数>=256, 脱离总线
```

**错误处理代码：**
```c
void CAN_Error_Handler(CAN_HandleTypeDef *hcan) {
    uint32_t can_error = hcan->ErrorCode;

    if(can_error & HAL_CAN_ERROR_EWG) {
        // 错误警告限
        log_warning("CAN error warning");
    }

    if(can_error & HAL_CAN_ERROR_EPV) {
        // 错误被动限
        log_warning("CAN error passive");
    }

    if(can_error & HAL_CAN_ERROR_BOF) {
        // 总线关闭
        log_error("CAN bus off");

        // 总线关闭恢复策略
        // 方法1: 自动恢复(需等待128*11位时间)
        // 方法2: 手动恢复
        HAL_CAN_Stop(hcan);
        HAL_CAN_Start(hcan);
    }

    if(can_error & HAL_CAN_ERROR_STF) {
        //  Stuff Error: 位填充错误
    }
    if(can_error & HAL_CAN_ERROR_FOR) {
        // Form Error: 格式错误
    }
    if(can_error & HAL_CAN_ERROR_ACK) {
        // Acknowledgement Error: 确认错误
    }
    if(can_error & HAL_CAN_ERROR_BR) {
        // Bit Error: 位错误
    }
    if(can_error & HAL_CAN_ERROR_CR) {
        // CRC Error: CRC错误
    }
}
```

**CAN-FD与CAN2.0区别：**
| 特性 | CAN2.0 | CAN-FD |
|------|--------|--------|
| 速率 | 1Mbps | 8Mbps |
| 数据场 | 8字节 | 64字节 |
| 帧格式 | 兼容 | 新格式 |
| 编码 | 位填充 | 非对称编码 |

---

## 5. 存储器

### 5.1 Flash编程要点及注意事项

**答案：**

**Flash擦除：**
```c
// 解锁Flash
HAL_FLASH_Unlock();

// 擦除扇区
FLASH_EraseInitTypeDef EraseInitStruct;
EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
EraseInitStruct.Sector = FLASH_SECTOR_5;
EraseInitStruct.NbSectors = 1;
EraseInitStruct.VoltageRange = VOLTAGE_RANGE_3;

uint32_t SectorError;
HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

// 锁定Flash
HAL_FLASH_Lock();
```

**Flash编程：**
```c
// 解锁
HAL_FLASH_Unlock();

// 编程(以32位为单位)
for(uint32_t i = 0; i < size; i += 8) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                      address + i,
                      data[i/8]);
}

// 锁定
HAL_FLASH_Lock();
```

**注意事项：**
1. **擦写寿命**: Flash擦写次数有限(通常10K-100K次)
   - 解决方案: 磨损均衡算法
2. **对齐要求**: 写入地址和数据需对齐
3. **擦除单位**: 最小擦除单元为扇区
4. **数据保护**: 避免在代码运行区域进行编程

**Flash应用 - 固件升级：**
```c
// A/B升级方案
// Flash布局:
// 0x08000000 - 0x0803FFFF: Bootloader (256KB)
// 0x08040000 - 0x0807FFFF: App A (256KB)
// 0x08080000 - 0x080BFFFF: App B (256KB)
// 0x080C0000 - 0x080DFFFF: Config (128KB)

// Bootloader跳转
void jump_to_app(uint32_t app_address) {
    // 检查栈指针
    if(((*(__IO uint32_t*)app_address) & 0x2FFE0000) == 0x20000000) {
        // 关闭中断
        __disable_irq();

        // 设置栈指针
        __set_MSP(*(__IO uint32_t*)app_address);

        // 设置PC
        void (*app_reset)(void) = (void(*)(void))(*(__IO uint32_t*)(app_address + 4));
        app_reset();
    }
}
```

---

### 5.2 启动流程分析

**答案：**

**Cortex-M启动流程：**
```
1. 上电/复位
2. 复位向量:
   - 地址0x00000000: 栈顶地址(MSP)
   - 地址0x00000004: 复位向量(PC)
3. SystemInit(): 系统初始化(时钟配置)
4. __main(): C库初始化
   - .data初始化(从Flash复制到RAM)
   - .bss初始化(零填充)
5. main(): 用户程序入口
```

**启动文件分析(Keil/ARM)：**
```assembly
; 启动文件关键部分
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors

__Vectors       DCD     __initial_sp          ; 栈顶
                DCD     Reset_Handler         ; 复位处理
                DCD     NMI_Handler           ; NMI
                DCD     HardFault_Handler     ; 硬件错误
                ; ... 更多向量

                AREA    |.text|, CODE, READONLY

Reset_Handler   PROC
                EXPORT  Reset_Handler [WEAK]
                IMPORT  SystemInit
                IMPORT  __main

                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP
```

**分散加载配置：**
```ld
/* 链接脚本关键部分 */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        . = ALIGN(4);
    } >FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        _edata = .;
    } >RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM
}
```

---

## 6. 低功耗

### 6.1 STM32功耗模式有哪些？如何选择？

**答案：**

**功耗模式对比：**

| 模式 | 进入方式 | 唤醒时间 | 功耗(典型) | 特点 |
|------|----------|----------|------------|------|
| Sleep | WFI/WFE | 立即 | ~30mA | CPU停止，外设运行 |
| Low Power Sleep | WFI/WFE | 立即 | ~10mA | 降低时钟频率 |
| Stop 0/1/2 | HAL_PWR_EnterSTOPMode | ~5us | ~45uA | 保留RAM和寄存器 |
| Standby | HAL_PWR_EnterSTANDBYMode | ~50ms | ~2uA | 仅备份域供电 |
| VBAT | - | - | ~1uA | 电池供电RTC等 |

**低功耗模式选择原则：**
```c
// 1. 短期休眠(毫秒级) - Sleep模式
void sleep_mode(void) {
    __WFI();  // Wait For Interrupt
}

// 2. 中期休眠(秒级) - Stop模式
void stop_mode(void) {
    // 进入Stop模式，保留所有寄存器
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERMODE_STOP,
                          PWR_STOPENTRY_WFI);
    // 唤醒后需重新配置时钟
    SystemClock_Config();
}

// 3. 长期休眠(分钟/小时级) - Standby模式
void standby_mode(void) {
    // 使能唤醒源
    HAL_PWREx_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    // 进入Standby
    HAL_PWR_EnterSTANDBYMode();
}

// 4. 电池供电 - VBAT模式
void vbat_mode(void) {
    // RTC配置在VBAT域
    // 可使用LSE(32.768KHz)作为RTC时钟源
    // 待机功耗约1uA
}
```

**功耗优化技巧：**
1. 关闭未使用的外设时钟
2. 降低系统时钟频率
3. 使用内部RC振荡器替代晶振
4. 优化IO状态(模拟输入功耗最低)
5. 减少高频中断

---

## 7. 看门狗

### 7.1 独立看门狗(IWDG)与窗口看门狗(WWDG)区别

**答案：**

**对比：**

| 特性 | 独立看门狗(IWDG) | 窗口看门狗(WWDG) |
|------|------------------|------------------|
| 时钟源 | 独立LSI(约32KHz) | APB1时钟 |
| 计数器 | 12位(最大4096) | 7位(最大127) |
| 喂狗窗口 | 无窗口期 | 有窗口期限制 |
| 超时时间 | ~26秒(最大值) | ~58ms(最大值) |
| 中断支持 | 无 | 有(EWI) |
| 适用场景 | 低精度容错 | 精密超时控制 |

**IWDG使用：**
```c
void IWDG_Init(void) {
    // 启用IWDG，设置预分频和重装载值
    // 预分频: 4*2^prer = 4~262144
    // 超时时间 = (4*2^prer * rlr) / 32000
    IWDG->KR = 0x5555;  // 解除写保护
    IWDG->PR = 4;       // 预分频64
    IWDG->RLR = 1250;   // 重装载值, 约2.5秒
    IWDG->KR = 0xAAAA;  // 启动IWDG
}

void IWDG_Feed(void) {
    IWDG->KR = 0xAAAA;  // 喂狗
}
```

**WWDG使用：**
```c
void WWDG_Init(void) {
    // 启用WWDG
    // 计数器初值: 0x7F
    // 窗口值: 0x40-0x7F
    WWDG->CFR = 0x60 | (124 & 0x7F);  // 窗口值 + 预分频
    WWDG->CR = 0x7F;                   // 启动，设置计数器
}

void WWDG_Feed(void) {
    // 必须在窗口期内喂狗
    WWDG->CR = 0x7F;  // 重装计数器
}

// 提前唤醒中断(EWI)
void WWDG_IRQHandler(void) {
    if(WWDG->SR & WWDG_SR_EWIF) {
        WWDG->SR = 0;  // 清除中断标志
        // 紧急处理
    }
}
```

---

## 8. 传感器

### 8.1 IMU传感器数据融合方法

**答案：**

**常见传感器融合方案：**

**1. 互补滤波(Complementary Filter)**
```c
// 互补滤波融合加速度计和陀螺仪
// 陀螺仪测量角度变化，加速度计修正漂移
float complementary_filter(float acc_angle, float gyro_rate, float dt) {
    static float angle = 0;
    // 加权融合: alpha通常取0.98
    float alpha = 0.98;
    angle = alpha * (angle + gyro_rate * dt) + (1 - alpha) * acc_angle;
    return angle;
}

// 加速度计计算倾角
float get_acc_angle(float ax, float ay, float az) {
    return atan2(ay, az) * 180 / M_PI;
}
```

**2. 卡尔曼滤波(EKF)**
```c
// 简化的卡尔曼滤波实现
typedef struct {
    float x;      // 估计值
    float p;      // 估计误差协方差
    float q;      // 过程噪声
    float r;      // 测量噪声
} KalmanFilter;

float kalman_update(KalmanFilter *kf, float measurement) {
    // 预测
    kf->p = kf->p + kf->q;

    // 更新
    float k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + k * (measurement - kf->x);
    kf->p = (1 - k) * kf->p;

    return kf->x;
}
```

**3. AHRS姿态解算**
```c
// 使用四元数的AHRS算法
void ahrs_update(AHRS *ahrs, float gx, float gy, float gz,
                 float ax, float ay, float az, float dt) {
    // 四元数微分方程
    float q0 = ahrs->q[0], q1 = ahrs->q[1], q2 = ahrs->q[2], q3 = ahrs->q[3];

    float qDot0 = 0.5f * (-q1*gx - q2*gy - q3*gz);
    float qDot1 = 0.5f * ( q0*gx + q2*gz - q3*gy);
    float qDot2 = 0.5f * ( q0*gy - q1*gz + q3*gx);
    float qDot3 = 0.5f * ( q0*gz + q1*gy - q2*gx);

    // 积分
    q0 += qDot0 * dt;
    q1 += qDot1 * dt;
    q2 += qDot2 * dt;
    q3 += qDot3 * dt;

    // 归一化
    float norm = sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    ahrs->q[0] = q0 / norm;
    ahrs->q[1] = q1 / norm;
    ahrs->q[2] = q2 / norm;
    ahrs->q[3] = q3 / norm;
}
```

---

### 8.2 GPS/RTK定位原理

**答案：**

**GPS定位原理：**
```
伪距定位: 通过测量4颗以上卫星的伪距
位置 = (卫星位置) + (伪距误差)

伪距 = 实际距离 + 时钟偏差*c + 大气延迟 + 多径误差

最少需要4颗卫星才能解算3维位置+时钟偏差
```

**RTK定位原理：**
```
RTK = Real-Time Kinematic(实时动态载波相位差分)

基本原理:
1. 基准站: 已知精确位置，观测卫星
2. 移动站: 同时观测卫星
3. 差分修正: 基准站发送修正数据给移动站
4. 精度: 厘米级(1cm + 1ppm)

载波相位测量:
- L1: 1575.42MHz, 波长约19cm
- L2: 1227.60MHz, 波长约24cm

整周模糊度解算(AR):
- 关键难点: 确定载波相位的整周数
- 方法: OTF(On-The-Fly)初始化
- 耗时: 数十秒到数分钟
```

**NMEA-0183协议：**
```c
// GGA语句(定位数据)
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
       |       |         |         | |  |   |     |
       时间     纬度       经度      质量 卫星 高度

// RMC语句(推荐定位信息)
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
```

**RTCM3数据格式：**
```
RTCM message types:
- 1001-1009: GPS观测值
- 1010-1012: GLONASS观测值
- 1071-1077: GPS MSM
- 1081-1087: GLONASS MSM
- 1091-1097: Galileo MSM
- 1121-1127: BeiDou MSM
```

---

## 9. RTOS相关

### 9.1 任务间通信方式对比

**答案：**

| 通信方式 | 优点 | 缺点 | 适用场景 |
|----------|------|------|----------|
| 消息队列 | 解耦、异步 | 有拷贝开销 | 大数据传递 |
| 信号量 | 轻量、快速 | 无法传递数据 | 同步/互斥 |
| 事件标志 | 多事件等待 | 只能状态通知 | 多条件同步 |
| 共享内存 | 无拷贝 | 需要同步 | 高速数据交换 |
| 互斥量 | 优先级继承 | 开销较大 | 资源保护 |

**消息队列使用：**
```c
// 创建队列
QueueHandle_t msg_queue = xQueueCreate(10, sizeof(char[50]));

// 发送
char msg[50] = "Hello";
xQueueSend(msg_queue, msg, 0);

// 接收
char buf[50];
xQueueReceive(msg_queue, buf, portMAX_DELAY);
```

**信号量使用：**
```c
// 二值信号量(同步)
SemaphoreHandle_t sem = xSemaphoreCreateBinary();

// 中断中释放
void ISR_Handler(void) {
    BaseType_t higher_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &higher_task_woken);
    portYIELD_FROM_ISR(higher_task_woken);
}

// 任务中获取
void Task(void *param) {
    while(1) {
        xSemaphoreTake(sem, portMAX_DELAY);
        // 处理
    }
}
```

---

### 9.2 优先级反转问题及解决

**答案：**

**优先级反转示例：**
```
任务L(低优先级): 获取互斥量M
任务M(中优先级): 打断L，运行
任务H(高优先级): 等待互斥量M，但被M阻塞

结果: H优先级实际低于L!
```

**解决方案：**

**1. 优先级继承(Priority Inheritance)**
```c
// FreeRTOS互斥量自动支持
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

// 任务L获取mutex
// 如果H尝试获取，RTOS临时提升L的优先级
```

**2. 优先级天花板(Priority Ceiling)**
```c
// 互斥量天花板优先级 = 所有可能访问资源的任务中最高优先级
// 任务获取互斥量时，优先级提升到天花板
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
vSemaphoreSetPriorityCeiling(mutex, HIGH_PRIORITY);
```

**3. 临界区保护**
```c
// 使用禁用调度器代替互斥量
void access_resource(void) {
    vTaskSuspendAll();  // 禁用调度器
    // 访问临界资源
    xTaskResumeAll();   // 恢复调度器
}
```

---

### 9.3 任务堆栈如何确定？

**答案：**

**栈大小计算方法：**

**理论计算：**
```
任务栈需求 = 函数调用深度 * 局部变量 + 中断栈帧 + 编译器开销

典型情况:
- 函数调用深度: 10层 * 32字节 = 320字节
- 局部变量: 128字节
- 中断栈帧: 32字节(保存寄存器)
- 浮点运算: 额外64字节
- 安全边界: x1.5 ~ x2
总计: 约1KB
```

**实际测量方法：**
```c
// 方法1: 栈水印检测
void Task_Function(void *param) {
    // 记录栈使用情况
    uint32_t watermark = uxTaskGetStackHighWaterMark(NULL);
    printf("Stack free: %lu words\r\n", watermark);
}

// 方法2: 填充测试(调试时)
void fill_stack(void) {
    uint32_t *ptr = (uint32_t *)task_stack;
    for(int i = 0; i < STACK_SIZE/4; i++) {
        ptr[i] = 0xDEADBEEF;
    }
}

void check_stack(void) {
    uint32_t *ptr = (uint32_t *)task_stack;
    int used = 0;
    for(int i = 0; i < STACK_SIZE/4; i++) {
        if(ptr[i] != 0xDEADBEEF) used++;
    }
    printf("Stack used: %d bytes\r\n", used * 4);
}
```

**配置建议：**
```c
// FreeRTOS任务创建
xTaskCreate(Task_Function,      // 任务函数
             "TaskName",        // 任务名
             2048,              // 栈大小(_words)，约8KB
             NULL,              // 参数
             3,                 // 优先级
             NULL);             // 句柄
```

---

## 10. 调试

### 10.1 常见死机问题如何分析？

**答案：**

**常见死机原因及分析方法：**

**1. 硬件断点观察**
```gJ-Link/Jdb
// 使用TAG调试器
// 1. 硬件断点
break hard_fault_handler

// 2. 观察HardFault状态
info registers
// 查看LR, PC, xPSR
// 分析栈回溯
```

**2. HardFault分析**
```c
void HardFault_Handler(void) {
    __asm volatile(
        "tst lr, #4 \n"
        "ite eq \n"
        "mrseq r0, msp \n"
        "mrsne r0, psp \n"
        "mov r1, lr \n"
        "b HardFault_Handler_C"
    );
}

void HardFault_Handler_C(uint32_t *stack, uint32_t lr) {
    // stack指向异常帧
    // R0-R3, R12, PC, xPSR, LR
    printf("PC=0x%08lX\r\n", stack[6]);
    printf("LR=0x%08lX\r\n", stack[5]);

    // 禁用所有中断
    __disable_irq();
    while(1);
}
```

**3. 栈回溯**
```c
void print_stack_trace(uint32_t *sp) {
    uint32_t pc = sp[6];
    uint32_t lr = sp[5];

    // 打印调用链
    printf("Crash at PC=0x%08lX, LR=0x%08lX\r\n", pc, lr);

    // 简单回溯(需要链接信息)
    for(int i = 0; i < 10; i++) {
        if(sp[i] < 0x20000000) break;  // 栈边界
        printf("Stack[%d]=0x%08lX\r\n", i, sp[i]);
    }
}
```

**常见死机原因：**
| 原因 | 症状 | 解决方法 |
|------|------|----------|
| 数组越界 | 随机值错误 | 增加数组边界检查 |
| 栈溢出 | HardFault | 增大栈空间 |
| 空指针 | HardFault | 检查指针有效性 |
| 除零 | HardFault | 添加除零检查 |
| 中断嵌套过深 | HardFault | 减少中断嵌套 |
| 内存泄漏 | 逐渐变慢 | 检查内存分配释放 |

---

### 10.2 如何测量中断响应时间？

**答案：**

**测量方法：**

**1. 硬件定时器法**
```c
volatile uint32_t irq_latency;

void TIM2_IRQHandler(void) {
    uint32_t enter_time = TIM2->CNT;
    uint32_t latency = enter_time - irq_trigger_time;

    if(latency > max_latency) max_latency = latency;

    // 清除中断标志
    TIM2->SR = 0;
}

// 触发中断的软件触发
void trigger_interrupt(void) {
    irq_trigger_time = TIM2->CNT;
    NVIC_SetPendingIRQ(TIM2_IRQn);
}
```

**2. GPIO翻转法**
```c
void irq_handler(void) {
    GPIOA->BSRR = GPIO_BSRR_BS0;  // 上升沿
    // 中断处理
    GPIOA->BSRR = GPIO_BSRR_BR0;   // 下降沿
}

// 测量: 上升沿到下降沿的时间
```

**3. DWT Cycle Counter(推荐)**
```c
void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void measure_irq(void) {
    uint32_t before = DWT->CYCCNT;
    // 触发中断
    // ...
    uint32_t after = DWT->CYCCNT;
    uint32_t cycles = after - before;
}
```

**典型中断响应时间：**
- Cortex-M3/M4: 12-16周期(12MHz系统约1.3μs)
- Cortex-M7: 12-20周期

---

## 11. 综合问题

### 11.1 项目中遇到的最难的问题是什么？如何解决的？

**建议回答结构：**

**STAR法则：**
- **Situation**: 项目背景
- **Task**: 遇到的问题
- **Action**: 采取的行动
- **Result**: 最终结果

**示例回答：**

**问题1: DMA缓存一致性导致数据丢失**
> "在开发一个高速数据采集项目时，发现DMA传输的数据偶尔出现错误。分析发现是CPU Cache和DMA之间的数据一致性问题。我通过将DMA缓冲区配置为Non-Cacheable区域解决了问题。同时增加了数据校验机制，确保数据传输的可靠性。"

**问题2: RTOS优先级反转导致系统卡顿**
> "系统在高负载情况下出现周期性卡顿。使用Tracealyzer分析发现是优先级反转问题。通过将互斥量改为支持优先级继承的 semaphore，并调整了任务优先级配置，问题得到解决。"

**问题3: GPS/RTK定位精度不稳定**
> "RTK定位精度在某些场景下波动较大。分析发现是多路径效应和基站距离过远导致。优化了天线布局，选择开阔环境部署基准站，并实现了基于SNR的卫星筛选算法，将精度从±5cm提升到±1cm。"

---

### 11.2 你的项目为什么选择这个RTOS/Linux方案？

**建议回答：**

**RTOS选择考量：**
```c
// 选型决策矩阵
/*
| 因素        | FreeRTOS | RT-Thread | uCOS-III |
|-------------|----------|-----------|----------|
| 实时性       | ★★★★☆   | ★★★★☆    | ★★★★★   |
| 资源占用     | ★★★★★   | ★★★★☆    | ★★★☆☆   |
| 生态完善     | ★★★★★   | ★★★★☆    | ★★★☆☆   |
| 商业授权     | ★★★★☆   | ★★★★☆    | ★★★☆☆   |
| 中文支持     | ★★☆☆☆   | ★★★★★    | ★★☆☆☆   |
| 组件丰富     | ★★★☆☆   | ★★★★★    | ★★☆☆☆   |
*/
```

**示例回答：**
> "选择FreeRTOS主要考虑以下几点：1) 资源占用极小，适合我们的MCU(64KB RAM)；2) 社区活跃，生态完善；3) 商业友好许可；4) 文档丰富便于团队学习。虽然RT-Thread组件更丰富，但对于我们的简单需求，FreeRTOS更合适。"

---

### 11.3 如何保证代码质量？

**建议回答：**

**代码质量保障措施：**

```c
// 1. 静态分析
// 使用PC-lint/Cpplint/Clang-Tidy
// 配置检查规则: 空指针解引用、内存泄漏等

// 2. 代码审查(Code Review)
// 提交前必须经过他人 review
// 重点检查: 边界条件、资源管理、并发安全

// 3. 单元测试
// 关键模块编写测试用例
// 使用Unity/CMock框架

// 4. 运行时检测
// 栈溢出检测: configCHECK_FOR_STACK_OVERFLOW
// 内存分配失败检测: configUSE_MALLOC_FAILED_HOOK
// 断言: assert() 宏

// 5. 编码规范
// MISRA C编码规范
// 命名规范、注释规范
```

---

## 12. 场景题

### 12.1 设计一个低功耗传感器数据采集系统

**答案：**

**系统设计要点：**

```c
/*
需求:
- 电池供电，要求1年续航
- 每分钟采集一次传感器数据
- 偶尔需要实时响应用户操作
*/

// 1. 硬件设计
// - MCU: Cortex-M0+ (低功耗)
// - 传感器: I2C接口
 - 通信: 低功耗蓝牙BLE

// 2. 功耗预算
// 传感器采样(10mA, 100ms) = 1mAh/天
// BLE发送(15mA, 50ms) = 0.4mAh/天
// MCU休眠(5uA) = 0.12mAh/天
// 总计约1.5mAh/天
// 2000mAh电池可工作3年

// 3. 软件架构
void low_power_sensor_task(void *param) {
    while(1) {
        // 进入低功耗
        enter_stop_mode();

        // 定时唤醒(每分钟)
        if(wakeup_reason == RTC_ALARM) {
            // 唤醒后先关闭不必要的时钟
            disable_unused_periph();

            // 采集传感器
            sensor_read_data();

            // 数据处理
            process_data();

            // BLE发送
            ble_send_data();
        }

        // 外部中断唤醒(用户操作)
        if(wakeup_reason == GPIO_WAKEUP) {
            // 快速响应
            handle_user_input();
        }
    }
}

// 4. 优化措施
// - 传感器使用低功耗模式
// - I2C使用DMA减少CPU占用
// - 使用RTC Alarm唤醒而非轮询
// - 减少IO口的漏电流
```

---

### 12.2 如何设计一个健壮的通信协议？

**答案：**

**协议设计要素：**

```c
/*
通信协议设计框架:
1. 帧格式
2. 校验机制
3. 重传机制
4. 状态机
*/

// 1. 帧格式设计
typedef struct {
    uint8_t  head[2];      // 帧头: 0xAA 0x55
    uint8_t  type;         // 消息类型
    uint8_t  len;         // 数据长度
    uint8_t  data[256];   // 数据
    uint16_t crc;         // CRC16校验
    uint8_t  tail;       // 帧尾: 0x0D 0x0A
} ProtocolFrame;

// 2. CRC校验
uint16_t calc_crc16(uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for(uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// 3. 状态机
typedef enum {
    STATE_IDLE,
    STATE_HEAD1,
    STATE_HEAD2,
    STATE_TYPE,
    STATE_LEN,
    STATE_DATA,
    STATE_CRC1,
    STATE_CRC2,
    STATE_TAIL
} ParserState;

// 4. 重传机制
typedef struct {
    uint8_t  seq;           // 序列号
    uint8_t  retry;         // 重试次数
    uint32_t timeout;       // 超时时间
    void    *data;          // 数据副本
    TickType_t send_time;   // 发送时间
} TransmitContext;
```

---

### 12.3 单片机程序卡死在while(1)里如何排查？

**答案：**

**排查步骤：**

```c
// 1. 首先判断是否真的卡死
// - 串口是否有打印
// - LED是否闪烁
// - 定时器是否还在运行

// 2. 添加调试信息
void debug_hook(void) {
    static uint32_t last_tick = 0;
    uint32_t current_tick = xTaskGetTickCount();

    if(current_tick - last_tick > 1000) {
        printf("System running: %lu\r\n", current_tick);
        last_tick = current_tick;
    }
}

// 3. 检查看门狗
// 如果开启了看门狗，长时间卡死会导致复位
// 观察复位次数判断

// 4. 使用调试器
// - 查看PC寄存器值
// - 查看Call Stack
// - 设置断点在可疑函数

// 5. 常见卡死原因
/*
| 位置 | 原因 | 解决方法 |
|------|------|----------|
| while(1) | 等待不发生的事件 | 检查事件源 |
| HAL_UART_Receive | 阻塞模式 | 改用DMA/中断 |
| xQueueReceive | 队列为空且无超时 | 检查发送任务 |
| malloc | 内存分配失败 | 使用静态分配 |
| assert_param | 参数错误 | 检查参数 |
*/

// 6. 添加超时机制
BaseType_t res = xQueueReceive(queue, &data, pdMS_TO_TICKS(1000));
if(res == pdFALSE) {
    printf("Queue receive timeout!\r\n");
    // 记录错误或重启
}
```

---

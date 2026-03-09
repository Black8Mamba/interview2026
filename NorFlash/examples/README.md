# NOR Flash 示例代码

本目录包含 SPI Flash 和并行 Flash 的示例代码，展示了 NOR Flash 驱动的各种使用场景。

## 目录结构

```
examples/
├── spi-flash/          # SPI Flash 示例代码
│   ├── basic_read_write.c
│   ├── sector_erase.c
│   ├── quad_mode.c
│   ├── xip_boot.c
│   ├── sfdp_parse.c
│   └── sfdp_timing.c
│
└── parallel-flash/    # 并行 Flash 示例代码
    ├── fsmc_basic.c
    ├── cfi_detection.c
    ├── cfi_parse.c
    ├── cfi_timing.c
    └── xilinx_interface.c
```

---

## SPI Flash 示例代码

### 1. basic_read_write.c
**基本读写操作示例**

展示 SPI Flash 的基础读写功能，包括：
- 芯片初始化和配置
- 字节页//批量数据读取
- 字节/页/批量数据写入
- 读取状态寄存器判断操作完成

适用于初学者学习 SPI Flash 的基本操作流程。

---

### 2. sector_erase.c
**扇区擦除操作示例**

演示扇区擦除功能的使用方法：
- 扇区擦除命令发送
- 等待擦除完成（轮询状态寄存器）
- 全片擦除操作
- 擦除验证方法

用于理解 Flash 存储介质的擦除特性。

---

### 3. quad_mode.c
**四线模式（QUAD SPI）示例**

展示如何配置和使用四线 SPI 模式：
- 进入四线模式（QE 位配置）
- 四线快速读取命令
- 四线页写入
- 性能对比测试

用于提升大数据量传输场景的效率。

---

### 4. xip_boot.c
**XIP（Execute-In-Place）启动示例**

实现代码原地执行功能：
- 将 Flash 映射到内存地址空间
- 配置 XIP 模式
- 从 Flash 直接启动程序
- XIP 性能优化技巧

适用于嵌入式系统的无盘启动方案。

---

### 5. sfdp_parse.c
**SFDP 参数解析示例**

演示 Serial Flash Discoverable Parameters 的解析：
- SFDP 头信息读取
- JEDEC Flash 参数表解析
- 扇区大小、页大小提取
- 电压范围、时序参数获取

用于实现通用 SPI Flash 驱动。

---

### 6. sfdp_timing.c
**SFDP 时序配置示例**

基于 SFDP 参数配置时序：
- 从 SFDP 提取时序参数
- 配置时钟分频
- 设置片选信号时序
- 动态调整时序优化性能

用于实现自适应时序配置。

---

## 并行 Flash 示例代码

### 1. fsmc_basic.c
**FSMC 控制器基本操作示例**

展示并口 Flash 与 FSMC 控制器的连接配置：
- FSMC 初始化配置
- 异步/同步读取模式
- 读写时序配置
- 内存映射访问方式

适用于 STM32 等具备 FSMC 接口的 MCU。

---

### 2. cfi_detection.c
**CFI 芯片检测示例**

实现 Common Flash Interface 芯片识别：
- CFI 查询模式进入
- 制造商 ID 和设备 ID 读取
- CFI 信息结构解析
- 芯片兼容性判断

用于实现通用并行 Flash 驱动。

---

### 3. cfi_parse.c
**CFI 参数解析示例**

解析 CFI 提供的芯片参数：
- 扇区布局信息提取
- 电气时序参数获取
- 芯片容量和特性解析
- 保护区域信息读取

用于实现自适应的驱动配置。

---

### 4. cfi_timing.c
**CFI 时序配置示例**

基于 CFI 参数配置硬件时序：
- 读取时序参数
- 配置 FSMC 时序寄存器
- 建立/保持时间设置
- 时序优化和验证

用于实现最佳性能的时序配置。

---

### 5. xilinx_interface.c
**Xilinx FPGA Flash 接口示例**

展示 FPGA 配置 Flash 的接口方式：
- Xilinx Platform Flash 识别
- FPGA 配置数据读取
- 边界扫描接口使用
- 多 Flash 级联配置

适用于 FPGA 开发板的 Flash 编程。

---

## 使用说明

1. **硬件准备**：根据示例需求准备对应的开发板和 Flash 芯片
2. **环境配置**：配置交叉编译工具链和调试器
3. **选择示例**：根据需求选择合适的示例进行学习和修改
4. **调试验证**：通过调试器验证功能正确性

## 注意事项

- 实际使用时需根据具体芯片型号调整代码
- 注意 Flash 的电压匹配和工作条件
- 擦写操作有次数限制，需考虑磨损均衡
- 重要数据建议做好备份

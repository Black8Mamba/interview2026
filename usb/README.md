# USB 协议与开发面试指南

> 面向中高级嵌入式开发者的 USB 协议栈学习资料

---

## 📚 内容导航

| 章节 | 主题 | 预估篇幅 |
|------|------|----------|
| [01](./01-usb-protocol-basics.md) | USB 协议基础 | 8-10 页 |
| [02](./02-usb-enumeration.md) | USB 枚举过程 | 10-12 页 |
| [03](./03-usb-class-drivers.md) | USB 类驱动 | 10-12 页 |
| [04](./04-usb-driver-development.md) | USB 驱动开发 | 8-10 页 |
| [05](./05-usb-debugging.md) | USB 调试与排查 | 8-10 页 |
| [06](./06-usb-high-speed.md) | USB 高速与超高速 | 6-8 页 |
| [07](./07-usb-otg-typec.md) | OTG 与 Type-C | 6-8 页 |

---

## 🎯 学习路径建议

### 面试突击路线（2-3天）
1. 协议基础 → 2. 枚举过程 → 3. 类驱动（CDC/HID）→ 5. 调试

### 系统学习路线（1-2周）
按顺序全部学习，配合实践项目加深理解

---

## 📋 文档特色

- **面试真题**: 每章末尾包含 3-5 道高频面试题
- **流程图**: 关键流程使用 Mermaid 图表示
- **⚠️ 注意事项**: 开发中容易犯的错误
- **标准版**: 每个知识点 1-2 段话，包含关键细节

---

## 🔧 配套工具

- [USBView](https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/usbview) - Windows USB 设备查看器
- [Bus Hound](https://www.perisoft.net/bushound/) - USB 协议分析工具
- [Wireshark](https://www.wireshark.org/) - 网络与 USB 协议分析
- [Linux USB](https://www.kernel.org/doc/html/latest/usb/) - Linux USB 驱动文档

---

## 目录结构

```
usb-learning/
├── README.md                    # 本文件
├── 01-usb-protocol-basics.md   # USB 协议基础
├── 02-usb-enumeration.md        # USB 枚举过程
├── 03-usb-class-drivers.md     # USB 类驱动
├── 04-usb-driver-development.md # USB 驱动开发
├── 05-usb-debugging.md          # USB 调试与排查
├── 06-usb-high-speed.md        # USB 高速与超高速
└── 07-usb-otg-typec.md          # OTG 与 Type-C
```

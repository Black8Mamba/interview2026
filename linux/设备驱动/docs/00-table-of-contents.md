# Linux 驱动模型学习指南 - 目录

## 第0章 概述
- [0.1 Linux 驱动模型全景图](./chapter00-overview.md)

## 第1章 底层核心机制
- [1.1 kobject 核心对象](./chapter01-kernel-objects.md#11-kobject-核心对象)
- [1.2 kset 对象集合](./chapter01-kernel-objects.md#12-kset-对象集合)
- [1.3 ktype 对象类型](./chapter01-kernel-objects.md#13-ktype-对象类型)
- [1.4 uevent 事件机制](./chapter01-kernel-objects.md#14-uevent-事件机制)
- [1.5 class 设备类](./chapter01-kernel-objects.md#15-class-设备类)
- [1.6 sysfs 文件系统原理](./chapter01-kernel-objects.md#16-sysfs-文件系统原理)
- [本章面试题](./chapter01-kernel-objects.md#本章面试题)

## 第2章 总线设备驱动模型
- [2.1 device 结构体与设备模型](./chapter02-bus-device-driver.md#21-device-结构体与设备模型)
- [2.2 bus 总线子系统](./chapter02-bus-device-driver.md#22-bus-总线子系统)
- [2.3 driver 驱动结构](./chapter02-bus-device-driver.md#23-driver-驱动结构)
- [2.4 device_driver 与 driver 的绑定机制](./chapter02-bus-device-driver.md#24-devicedriver-与-driver-的绑定机制)
- [2.5 热插拔与动态设备管理](./chapter02-bus-device-driver.md#25-热插拔与动态设备管理)
- [本章面试题](./chapter02-bus-device-driver.md#本章面试题)

## 第3章 设备树 (Device Tree)
- [3.1 设备树概述与语法](./chapter03-device-tree.md#31-设备树概述与语法)
- [3.2 设备树在内核中的解析](./chapter03-device-tree.md#32-设备树在内核中的解析)
- [3.3 设备树与 platform 设备](./chapter03-device-tree.md#33-设备树与-platform-设备)
- [3.4 Device Tree Overlay](./chapter03-device-tree.md#34-device-tree-overlay)
- [3.5 dtb 编译与打包](./chapter03-device-tree.md#35-dtb-编译与打包)
- [本章面试题](./chapter03-device-tree.md#本章面试题)

## 第4章 字符设备驱动案例
- [4.1 字符设备驱动框架](./chapter04-char-device.md#41-字符设备驱动框架)
- [4.2 LED 驱动实例](./chapter04-char-device.md#42-led-驱动实例)
- [4.3 按键驱动实例](./chapter04-char-device.md#43-按键驱动实例)
- [本章面试题](./chapter04-char-device.md#本章面试题)

## 第5章 平台设备驱动案例
- [5.1 Platform 驱动模型](./chapter05-platform-device.md#51-platform-驱动模型)
- [5.2 设备树节点到 platform 设备的转换](./chapter05-platform-device.md#52-设备树节点到-platform-设备的转换)
- [5.3 典型 Platform 驱动开发](./chapter05-platform-device.md#53-典型-platform-驱动开发)
- [本章面试题](./chapter05-platform-device.md#本章面试题)

## 第6章 I2C/SPI 总线驱动案例
- [6.1 I2C 主机驱动与设备驱动](./chapter06-i2c-spi.md#61-i2c-主机驱动与设备驱动)
- [6.2 SPI 主机驱动与设备驱动](./chapter06-i2c-spi.md#62-spi-主机驱动与设备驱动)
- [6.3 传感器驱动实例](./chapter06-i2c-spi.md#63-传感器驱动实例)
- [本章面试题](./chapter06-i2c-spi.md#本章面试题)

## 第7章 进阶话题
- [7.1 电源管理与驱动](./chapter07-advanced-topics.md#71-电源管理与驱动)
- [7.2 Linux 5.x 到 6.x 驱动模型变化](./chapter07-advanced-topics.md#72-linux-5x-到-6x-驱动模型变化)
- [7.3 内核模块签名与安全](./chapter07-advanced-topics.md#73-内核模块签名与安全)
- [7.4 调试技巧与工具](./chapter07-advanced-topics.md#74-调试技巧与工具)
- [本章面试题](./chapter07-advanced-topics.md#本章面试题)

## 第8章 面试题专题解析
- [8.1 驱动模型核心面试题](./chapter08-interview-qa.md#81-驱动模型核心面试题)
- [8.2 设备树高频面试题](./chapter08-interview-qa.md#82-设备树高频面试题)
- [8.3 驱动开发实战面试题](./chapter08-interview-qa.md#83-驱动开发实战面试题)
- [8.4 调试与性能优化面试题](./chapter08-interview-qa.md#84-调试与性能优化面试题)

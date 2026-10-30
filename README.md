# nRF52840 Wireless CMSIS-DAP v2 + CDC UART (v4.4 稳定版)

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![Unit Tests](https://img.shields.io/badge/Tests-13%2F13%20PASS-brightgreen.svg)](tests/)
[![Firmware](https://img.shields.io/badge/UF2-v4.4%20Ultra240B-orange.svg)](firmware/)

使用两块 SuperMini / nice!nano 兼容的 **Nordic nRF52840** 开发板，基于 Nordic 2.4 GHz 高速私有射频链路（Nordic ESB / RADIO 2 Mbps）实现的**低延迟无线 CMSIS-DAP v2（SWD 烧录与调试）**和**全双工无线虚拟串口（CDC ACM UART）**。

两块开发板刷入**完全相同的单固件**即可自动识别角色，无需烧录器，无需繁琐配对，真正即插即用！

> This project implements a high-performance wireless CMSIS-DAP v2 (WinUSB Bulk) probe and bidirectional CDC UART bridge on Nordic nRF52840 using a proprietary 2.4 GHz ESB 2 Mbps RF link. Both boards run the exact same unified firmware and automatically negotiate Host/Target roles.

---

## 🌟 核心特性与技术亮点 (v4.3)

* 🚀 **Ultra 240B 大包吞吐**：单次空口承载 240 字节大包，相比传统 64B HID 调试器，空中 RTT 交互轮次大幅削减近 70%，Flash 连续烧录速度成倍提升。
* ⚡ **CMSIS-DAP v2 原生 WinUSB Bulk**：基于 Windows 原生 WinUSB 驱动免驱直连，消除标准 HID 的 1ms 轮询延迟，全面释放 12Mbps USB 全速与 ESB 2Mbps 潜能；同时支持随时一键切换为 CMSIS-DAP v1 (HID) 兼容老旧 IDE。
* 📡 **AFH 智能自适应跳频 (Adaptive Frequency Hopping)**：
  内置空口丢包监控与多频点快速自愈机制。当遭遇复杂 2.4GHz Wi-Fi / 蓝牙同频强干扰导致丢包重发时，主从双机自动在基准频点（默认 CH80 / 2480MHz）与伪随机正交备用信道间毫秒级静默跳转，彻底杜绝 Keil `RDDI-DAP Error` 中断。
* 💾 **免驱虚拟配置磁盘 (ConfigDisk 1MB VFS)**：
  电脑端（主机）插入 USB 即自动挂载 1MB 虚拟 U 盘，无需安装任何专用上位机驱动：
  - `CONFIG.TXT`：文本化配置配对码（PairCode）、射频信道（Channel）、发射功率（TxPower）、协议版本（Protocol: 1=v1 HID, 2=v2 Bulk）、AFH 跳频开关。直接记事本修改保存即生效。
  - `STATUS.TXT`：实时链路状态遥测，包含无线连接状态、当前活跃信道、AFH 触发计数、空口往返 RTT 延时等。
  - `DAPLINK.exe`：内置 30KB 原生 Windows GUI 配置工具（纯 Win32 API 编写，零依赖，秒开），双击即可可视化读取与保存参数。
* 💡 **三段式智能 LED 状态机**：
  - **慢闪 (1 Hz)**：无线搜索配对中（未建立连接）。
  - **常亮 (Solid ON)**：无线链路已锁定就绪（Idle Connected）。
  - **快闪 (10 Hz 爆闪)**：SWD 烧录活跃 / 批量数据传输中。
* 🔄 **单固件自由角色 + 数据线/充电线智能识别**：
  - 双板共用一份 UF2 固件，硬件上电自动检测：接目标 MCU 的板自动成为**目标从机（A 端）**；接电脑 USB 且未接目标的板自动成为**电脑主机（B 端）**。
  - **充电线/数据线智能识别**：从机插充电头或充电宝供电（仅供电无 USB 数据枚举）不抢占本地模式，保持无线连接；插电脑且完成 USB 枚举时无缝切换为有线直连调试器，拔掉数据线即秒回无线模式。
* ⚡ **P1.13 烧录完成强制硬件复位**：
  - 在 DAP 会话批量写入 Flash 结束并收到 `DAP_Disconnect` 时，目标端自动向 `P1.13` 输出 20ms 开漏硬件复位低脉冲后释放。
  - 完美解决 Keil 未勾选 `Reset and Run` 或部分 MCU 需要物理硬件复位线的场景；若不接 `P1.13`，依然保留原有 4 线 SWD 软件复位机制，双向完全兼容。
* 📟 **低延迟全双工无线串口 (CDC UART)**：
  默认 `115200 8N1`，支持 `1200` ~ `1000000` 常用波特率及 `8N1/8N2/8E1/8E2` 参数同步，支持 1 ms 短帧合包与智能重传。

---

## 🔌 硬件引脚定义与接线

![nRF52840 引脚定义图](docs/images/pinout.png)

### 单排直连推荐引脚（目标从机 A 端）

| nRF52840 引脚 | 目标 MCU 引脚 | 功能说明 |
|:---|:---|:---|
| **`P0.31`** | **`SWCLK`** | SWD 硬件时钟输出（内部限幅 4 MHz） |
| **`P0.29`** | **`SWDIO`** | SWD 双向数据线 |
| **`GND`** | **`GND`** | **必须共地** |
| **`P0.02`** | **`UART RX`** | 探针 TX -> 接目标板 RX（需要串口时连接） |
| **`P1.15`** | **`UART TX`** | 探针 RX <- 接目标板 TX（需要串口时连接） |
| **`P1.13`** | **`nRESET`** | 目标硬件开漏复位（可选；连接后在烧录完成自动拉低 20ms 硬件复位；不接走 SWD 软件复位） |

> **⚠️ 接线与供电避坑指南**：
> 1. **供电电平**：所有 GPIO 为 **3.3V 电平**，严禁直连 5V 单片机引脚。
> 2. **A 端供电方式**：支持通过 `3.3V`、`BAT` 引脚或 **Type-C 接口插充电头/充电宝供电**。新版本已加入智能枚举识别，仅供电不枚举不会误切本地有线模式，保持稳定无线连接；若需本地有线烧录，直接使用数据线插电脑即可。
> 3. **板载 `RST` 孔位说明**：夹在右侧 `GND` 和 `3.3V` 之间的 `RST` 是 nRF52840 芯片自身的复位口，不是目标的复位口！若使用排线，可在该孔插一根空杜邦线头占位防止错位。

---

## 🚀 极速上手：拖拽刷入固件 (UF2)

本项目已提供硬件实测验证的最新稳定版 UF2 固件在 [`firmware/`](firmware/) 目录中：

* **固件文件**：[`firmware/uart_latency_packet240_unified_v3_p113_2m_fast_ch80_afh_v2.uf2`](firmware/uart_latency_packet240_unified_v3_p113_2m_fast_ch80_afh_v2.uf2)
* **SHA-256**：`0E18352C8134C9156C4E17B8434F618E2A2356773403EB95D4BCFEA31BFE3C6A`

### 刷录步骤：
1. 用 USB 数据线将 nRF52840 开发板连接至电脑。
2. **0.5 秒内双击开发板上的 `RST` 按钮**（无按键的可快速短接两次 `RST` 到 `GND`）。
3. 电脑会弹出一个名为 `NICENANO` 或 `NRF52BOOT` 的可移动磁盘。
4. 将上述 `.uf2` 文件拖入该磁盘，开发板会自动烧写并重启。
5. **两块开发板均按此步骤刷入同一份固件即可！**

---

## 🛠️ 免驱配置与内置上位机

插入电脑端（主机 B 端）后，Windows 会自动弹出 1MB 虚拟 U 盘：

### 方式一：使用内置图形上位机（推荐）
直接在虚拟磁盘根目录（或仓库 [`tools/DAPLINK.exe`](tools/DAPLINK.exe)）双击打开 **`DAPLINK.exe`**：
* 30KB 极简原生 Win32 程序，无任何 DLL 或运行时依赖。
* 可视化修改：**配对码 (Pair Code)**、**通信信道 (Channel 0~80)**、**发射功率 (Tx Power)**、**运行角色 (AUTO 自动协商 / 角色A 主机发起端 / 角色B 从机响应端)**、**DAP 协议版本 (v1 HID / v2 WinUSB Bulk)**。
* 特别针对**双车模块/纯硬件串口透传**场景：分别将两板设为**角色A**与**角色B**，两板仅接串口引脚（TX/RX）即可无线无缝对连，不接任何电脑 USB 也能永不掉线、绝不降级。
* 点击 **保存并重启**，配置将写入片上 Flash 掉电保存，探针自动重启生效。

### 方式二：直接修改文本
记事本打开虚拟磁盘中的 `CONFIG.TXT`，修改对应键值（如 `CHANNEL=80`、`ROLE=ROLE_A`、`PROTOCOL=2`）后保存，探针将在 2 秒后自动写入 Flash 并重启。

---

## 💻 源码构建与单元测试

本项目提供完整的构建与自动化单元测试套件：

### 1. 运行回归测试（需 MinGW / g++）
在 PowerShell 中运行根目录测试脚本，验证协议栈与无线状态机：
```powershell
.\run_tests.ps1
```
*已覆盖的 9 大测试用例（100% PASS）：*
- `free_role_test`：自由角色自适应探测与竞选
- `packet128_test`：大包吞吐与 4 包并发窗口
- `recovery_test`：断网超时、断线重连与丢包重传
- `uart_latency_test`：1ms 短帧合包与全双工回显
- `uart_timing_probe`：串口精确时序与速率测试
- `config_disk_test`：1MB 虚拟文件系统、FAT12 一致性及 Flash 持久化
- `afh_test`：AFH 智能自适应跳频全信道正交性校验
- `usb_compat_test`：WinUSB 零长包 (ZLP) 与响应序列化
- `radio_test`：Nordic ESB 状态机 1000 轮随机全双工压力测试

### 2. 从源码重新编译固件
```powershell
# 编译并生成 v4.3 稳定版 UF2 固件
.\build.ps1
```
构建产物将输出至 `firmware/` 目录并自动通过 UF2 格式与 SHA256 结构校验。

---

## 📚 详细文档导航

* [接线与 Keil MDK 详细配置指南](docs/USAGE.md)
* [源码构建与编译选项说明](docs/BUILD.md)
* [核心架构与协议设计](docs/DEVELOPMENT.md)
* [测试与工程验证方法](docs/VALIDATION.md)

---

## 📄 开源许可证

本项目核心源码遵循 [Apache License 2.0](LICENSE) 许可协议开源。CMSIS-DAP 协议层源码版权归 ARM 所有。

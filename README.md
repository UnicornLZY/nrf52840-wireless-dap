# nRF52840 Wireless CMSIS-DAP + UART

使用两块 SuperMini / nice!nano 兼容的 nRF52840 开发板，通过私有 2.4 GHz 射频链路实现**无线 CMSIS-DAP v2（SWD 烧录与调试）**和**双向无线串口（CDC UART）**。两块板刷入同一份固件即可自动识别角色；接目标芯片的板子直接插电脑时，也可直接作为有线高速 CMSIS-DAP v2 调试器使用。

This project implements a two-board wireless CMSIS-DAP v2 and bidirectional UART bridge on nRF52840 using a proprietary 2.4 GHz ESB/RADIO link. Both boards run the exact same unified firmware.

## 主要特性

- **高速 CMSIS-DAP v2 + CDC 复合设备**：基于 WinUSB Bulk 传输（免驱），支持 128 字节 DAP 逻辑包与 4 包并发窗口。
- **2.4 GHz 私有高速链路**：工作于 RADIO 2 Mbps（频道 40），SWD 硬件时钟上限 4 MHz。
- **单固件自由角色**：无需区分主从固件；上电或接入时自动检测 SWD 目标，接目标的板自动作为**目标端（A 端）**，连接电脑 USB 且无本地目标的板自动作为**电脑端（B 端）**。
- **有线 / 无线自动切换**：目标端直接插入电脑 USB 数据线时自动切换为有线直连优先模式；拔出数据线并保持目标端独立供电即可恢复无线模式。
- **低延迟双向无线串口**：默认 `115200 8N1`，支持 `1200` ~ `1000000` 常用波特率及 `8N1/8N2/8E1/8E2` 参数同步，具备 1 ms 短帧合包与自动超时重传机制。
- **链路可靠性保障**：内置无线会话隔离、CRC 校验、ACK 确认与重复包过滤、DAP 超时保护及断链自动恢复。

## 接线与刷入固件

![nRF52840 引脚定义图](docs/images/pinout.png)

| nRF52840 引脚 | 目标 MCU | 说明 |
|---|---|---|
| `P0.31` | `SWCLK` | SWD 时钟线 |
| `P0.29` | `SWDIO` | SWD 数据线 |
| `GND` | `GND` | 必须共地 |
| `P0.02` | `UART RX` | 探针 TX 接目标 RX（需串口时连接） |
| `P1.15` | `UART TX` | 探针 RX 接目标 TX（需串口时连接） |
| `P1.13` | `nRESET / NRST` | 目标硬件复位（可选，普通使用可不接） |

> **接线注意**：
> - 信号为 **3.3V 电平**，请确保与目标板共地，不要直接连接 5V 信号；目标板独立供电时不要将 3.3V 电源直接并联冲突。
> - 无线使用时，接目标的 A 端请通过 `3.3V` 引脚供电，**不要用 A 端 USB 口插手机充电头供电**（否则会触发 USB 检测进入本地有线模式）。
> - 板载 `RST` 是 nRF52840 本板复位（夹在右侧 `GND` 与 `3.3V` 之间，使用连排杜邦线时可在该孔插一个空杜邦线头占位以防排线松动，不要接到目标芯片）。板背面的 `DIO/CLK` 焊盘是 nRF52840 芯片自身的烧录口。
> - 详细操作说明与 Keil 配置截图请查看 **[接线与使用教程](docs/USAGE.md)**。

### 快速刷入 UF2 固件（免开发环境）

可直接前往本仓库右侧 **[Releases](https://github.com/UnicornLZY/nrf52840-wireless-dap/releases)** 下载预编译好的 `.uf2` 固件刷入两块开发板：

1. 用带数据传输功能的 USB 线将 nRF52840 开发板连接电脑。
2. **0.5 秒内短接两次 `RST` 到 `GND`**（带复位按键的板子直接在 0.5 秒内双击 `RST` 按键即可）。
3. 电脑会弹出一个约 32MB 的可移动磁盘，将下载的 `.uf2` 固件拖入该磁盘中（**两块板均需刷入同一份固件**）。
4. 固件复制完成后开发板会自动重启，即可投入使用。

## 从源码构建

如需自行修改或编译源码，需安装 Arduino CLI、Adafruit nRF52 Core **1.7.0** 与 Python 3。在项目根目录打开 PowerShell 执行：

```powershell
arduino-cli core update-index --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core install adafruit:nrf52@1.7.0 --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json

# 编译生成 UF2 固件（输出至 firmware/ 目录）
.\build.ps1

# （可选）运行桌面单元测试与协议回归测试（需安装 g++）
.\run_tests.ps1
```

详细编译参数与自定义工具路径说明请参见 **[构建说明](docs/BUILD.md)**。

## 目录结构

```text
libraries/WirelessDAP/src/      无线 DAP、USB、会话管理、UART 与 RADIO 核心实现
libraries/WirelessDAP/src/arm/  ARM CMSIS-DAP 协议层源码
variant/                        板级 GPIO 引脚映射配置
unified_dap/                    Arduino 固件入口
tests/                          桌面协议栈与无线状态机回归测试
tools/                          UF2 固件结构校验工具
docs/                           使用教程、构建说明与架构文档
```

## 相关链接

- [详细接线与 Keil 配置教程](docs/USAGE.md)
- [构建说明](docs/BUILD.md) | [架构与协议设计](docs/DEVELOPMENT.md) | [测试与验证说明](docs/VALIDATION.md)
- [nRF52840 SuperMini 开发板硬件资料](https://www.nologo.tech/product/otherboard/NRF52840.html)

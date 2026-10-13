# nRF52840 Wireless CMSIS-DAP + UART

使用两块 SuperMini / nice!nano 兼容 nRF52840 板，通过私有 2.4 GHz 链路提供无线 SWD 烧录和双向串口。两板使用同一固件；接目标的板连接电脑时也可作为有线 CMSIS-DAP v2 与 CDC 串口。

This project implements a two-board wireless CMSIS-DAP v2 and UART bridge for nRF52840. It uses a private 2.4 GHz protocol, not Bluetooth pairing. Both boards use the same firmware.

本次开源主线对应 `uart_latency_packet128_unified_v3_p113_2m_fast_ch40.uf2`，原文件 SHA256：

```text
7E3C4C9DACCF2E4EE8F4F7374D04B86C9DC7B918FE94259589ED8AC29D928147
```

## 功能和状态

- USB CMSIS-DAP v2 / WinUSB Bulk + CDC；128 字节 DAP 逻辑包、4 包窗口。
- 私有 RADIO 2 Mbps、频道 40，SWD 配置上限 4 MHz；实际 SWCLK 波形仍需测量。
- 单固件自由角色；检测目标后作为目标端，USB 数据连接且无本地目标时作为电脑端。
- 目标侧本地 USB 优先；拔线后需继续独立供电才能恢复无线。
- UART 冷启动 115200/8N1；支持离散波特率及8N1/8N2/8E1/8E2，奇校验不支持。
- UART 短帧合包1ms、丢应用ACK首次20ms补发，后续40/80/160ms退让；这些是策略时间，不是端到端延迟。
- 会话隔离、CRC、无线确认/去重、DAP超时及故障恢复；不自动重放状态未知的DAP写操作。

该版本在原交接记录中是已编译、桌面测试通过的低延迟候选，没有完整实板延迟/长期稳定验收数据。历史其他版本的成功次数不能替代本版本验收。不要将它描述为“带宽已拉满”或“零失败稳定版”。

## 接线与刷入固件

![nRF52840 引脚定义图](docs/images/pinout.png)

| nRF52840 引脚 | 目标 MCU |
|---|---|
| P0.31 | SWCLK |
| P0.29 | SWDIO |
| GND | GND（必须共地） |
| P0.02 / UART TX | UART RX |
| P1.15 / UART RX | UART TX |
| P1.13 | 可选 nRESET / NRST |

使用3.3V目标并共地；目标独立供电时不要直接并联电源。无线使用时，接目标的 A 端请由目标板或引脚供电，**不要用 A 端 USB 口插充电头供电**（否则可能触发进入本地有线模式）。板背DIO/CLK是nRF52840本身的调试口，不能代替上述SWD输出。板载 `RST` 是探针本板复位（位于 `GND` 与 `3.3V` 之间，使用连排杜邦线时可在该孔插个空杜邦线头仅作占位、防止排线松动或插错孔位，不要将其接到目标芯片）。完整图文步骤与 Keil 配置截图详见 [接线和使用](docs/USAGE.md)。

### 快速刷入 UF2 固件（无需编译环境）

1. 用带数据功能的 USB 线将 nRF52840 开发板插到电脑上。
2. **0.5 秒内短接两次 `RST` 到 `GND`**（带按键的板子直接在 0.5 秒内按两次 `RST` 按键即可）。
3. 电脑会弹出一个约 32MB 的可移动磁盘，将 Releases 或 `firmware/` 中的 `.uf2` 固件复制进去（两块板须刷入同一份固件）。
4. 复制完成后自动重启即可使用。

## 构建与测试

依赖 Arduino CLI、Adafruit nRF52 Core **1.7.0**、Python 3。项目已包含所需ARM源码和板级variant，无需原开发目录。Windows PowerShell：

```powershell
arduino-cli core update-index --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core install adafruit:nrf52@1.7.0 --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
.\build.ps1
# 已安装g++时运行桌面回归
.\run_tests.ps1
```

如果工具不在PATH，使用 `-Cli`、`-Python`、`-Cxx` 指定可执行文件；非默认SDK位置可传 `-Core`。细节见 [构建说明](docs/BUILD.md)。

输出位于 `firmware/uart_latency_packet128_unified_v3_p113_2m_fast_ch40.uf2`。两板均须刷同一版本。应用从0x26000开始，保留已有S140/引导程序；仅用于相容引导布局，不能整片擦除后只刷应用。双击Reset进入UF2盘再复制应用。

## 源码结构

```text
libraries/WirelessDAP/src/      DAP、USB、会话、UART、RADIO实现
libraries/WirelessDAP/src/arm/  ARM CMSIS-DAP源码
variant/                        板级GPIO映射
unified_dap/                    Arduino入口
tests/                          桌面模型和模拟外设
tools/verify_uf2.py             UF2结构检查
docs/                           使用、构建与验证说明
```

开发经过见 [开发摘要](docs/DEVELOPMENT.md)，板卡硬件参数参考 [nRF52840 开发板资料](https://www.nologo.tech/product/otherboard/NRF52840.html)。

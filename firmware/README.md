# Firmware Directory / 预编译固件目录

本目录包含经过硬件实测验证的正式发布固件。

## 📦 最新推荐稳定版固件（v7.1.1 · 全芯片 Keil 兼容修复 / 32KB 零丢包无线串口 / 双速率自适应）

经过 STM32F411 与 TI MSPM0G3507 多平台实板有线/无线验证，推荐作为当前主力稳定版固件使用。

* **固件文件名**：`wireless_daplink_v7.1.1_hs_pins.uf2`
* **应用起始地址**：`0x00026000`（适配搭载 nice!nano / ProMicro UF2 Bootloader 的 nRF52840 开发板）
* **固件大小**：540,672 字节（1,056 个 UF2 扇区块）
* **SHA-256 校验和**：
  `DF75B665A46E785687AA5C3ADA72A59F72C4D0AF798E56C74A79405C56D514B1`

### 核心特性与修复 (v7.1.1)
1. **修复 Keil 有线烧录 MSPM0G3507 及大 `.FLM` 算法芯片报 `Erase Failed! / RDDI-DAP Error` 问题**：
   - 将有线 CMSIS-DAP v2 单包上限规范为标准 `512B × 8`（完全对齐 Keil `CMSIS_AGDI.dll` 512B 规范，实测 TI MSPM0G3507 在 `10M / 5M / 1M / 500k` 全频段有线烧录校验 100% 通过，STM32F411 有线持续读达 `202.1 KB/s`）。
   - 优化有线 `8MHz` 稳态档位 SWD 包头半周期时序并补齐协议错误 33 时钟退避，提升长线与 Cortex-M0+ 兼容性。
2. **左高速射频隔离排针（`LEFT_HS` 默认主力 + `RIGHT_LP` 兼容切换）**：
   - 默认引脚 `SWCLK=P0.17 / SWDIO=P0.20 / TX=P0.22 / RX=P0.24 / NRST=P1.00`，无线烧录 480KB 固件实测 **26,262 帧 0.00% 重传**。
   - 兼容老硬件右侧引脚（`RIGHT_LP`），可通过 `WirelessDAP_Tool.exe` 或 `CONFIG.TXT` 热切换。
3. **有线硬件 `SPIM3` 加速 + `SWD_10M` 双策略可选（`8M` 默认稳态 / `16M` 超频）**：
   - Keil 选 `10 MHz` 时支持切换 `8M`（兼容长线与扩展板）或 `16M`（最高 228.50 KB/s SRAM 读取）。
4. **无线全双工串口 32KB 流控 + USB `NAK` 硬背压零丢包**：
   - 支持 `2400 ~ 1,000,000 bps` 14 档波特率双向零误码，64KB `@1M` 死灌达 `76.50 / 67.47 KB/s` 零丢包，45 秒混合高负载烤机 23,116 帧零坏包。
5. **`RF_PHY` 双速率自适应（`AUTO` / `1M` / `2M`）+ `AFH` 智能跳频**：
   - `AUTO` 模式下近距使用 `2 Mbps` 满速传输，弱信号自动降速 `1 Mbps` 提升链路余量与穿透力。

---

## 📦 历史版本固件（v6.0.0 / v5.0.0 / v4.4.0）

* **v6.0.0 (R4)**：`wireless_daplink_afh_v6.0.0.uf2` / `无线DAPLink_AFH_固定角色与充电识别_R4.uf2` (259,584 字节)
  - SHA-256: `D0FAEF6A256DF911387C64D1013F04D8C2B45F8B77EBA2B9E064F98B18183B0B`
* **v5.0.0 (R11)**：`无线DAPLink_一键Bootloader_瓶颈诊断_AFH_R11.uf2` (300,032 字节)
  - SHA-256: `81B9FA5E0F544C2B5D2C192483C729CBD6873285778BD1B124A488F2BB26D728`
* **v4.4.0**：`uart_latency_packet240_unified_v3_p113_2m_fast_ch80_afh_v2.uf2` (291,328 字节)
  - SHA-256: `0E18352C8134C9156C4E17B8434F618E2A2356773403EB95D4BCFEA31BFE3C6A`

---

## 🚀 刷录方法 (Drag-and-Drop)

1. 快速短接两次开发板上的 **`RST` 与 `GND`**（或双击物理 RST 键），PC 弹出名为 `NICENANO` 或 `NRF52BOOT` 的磁盘。
2. 将 `wireless_daplink_v7.1.1_hs_pins.uf2` 直接拖拽到该磁盘中。
3. 磁盘自动卸载并重启完成烧写。
4. **请务必为两块开发板（发射端与接收端）刷入同一版本的固件，并搭配新版 [`tools/WirelessDAP_Tool.exe`](../tools/WirelessDAP_Tool.exe) 使用！**

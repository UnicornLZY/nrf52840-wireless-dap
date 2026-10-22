# Firmware Directory / 预编译固件目录

本目录包含经过硬件实测验证的最新正式发布固件，以及源码编译脚本的默认输出目标。

## 📦 推荐量产稳定版固件（v4.3 稳定版）

* **固件文件名**：`uart_latency_packet240_unified_v3_p113_2m_fast_ch80_afh_v2.uf2`
* **应用起始地址**：`0x26000`（适配搭载 Adafruit S140 6.1.1 / nice!nano UF2 Bootloader 的 nRF52840 开发板）
* **固件大小**：257,024 字节（502 个 UF2 扇区块）
* **SHA-256 校验和**：
  `A0A3A4AEE877EAFFECBE6EA4ED2A77188A4318426A5FDE57BE03D3B4E2BD0145`

### 核心特性
1. **Ultra 240B 极速吞吐**：单次空口承载 240 字节大包，批量 Flash 写入无额外分包开销。
2. **AFH 智能自适应跳频**：遇到复杂 2.4GHz Wi-Fi / 蓝牙同频干扰时，在基准信道与伪随机正交信道间毫秒级静默跳转，0 丢包。
3. **免驱虚拟磁盘 ConfigDisk**：插入电脑自动挂载 1MB 虚拟 U 盘，内含 `CONFIG.TXT`、实时遥测 `STATUS.TXT` 以及 30KB 原生图形化配置工具 `DAPLINK.exe`。
4. **单固件双向自适应**：主机端与从机端刷入**同一份** `.uf2` 固件，硬件按是否连接 USB 自动竞选角色。

## 🚀 刷录方法 (Drag-and-Drop)

1. 双击开发板上的 **RST 按钮**，板载 LED 进入呼吸灯模式，PC 弹出名为 `NICENANO` 或 `NRF52BOOT` 的磁盘。
2. 将 `uart_latency_packet240_unified_v3_p113_2m_fast_ch80_afh_v2.uf2` 直接拖拽到该磁盘中。
3. 磁盘自动卸载并完成固件烧写。
4. **务必为两块开发板（主机和从机）刷入完全相同的固件！**

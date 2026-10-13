# Firmware Directory

运行仓库根目录的 `.\build.ps1` 后，编译生成的 `.uf2` 固件会输出到此目录：

- 默认输出文件名：`uart_latency_packet128_unified_v3_p113_2m_fast_ch40.uf2`
- 应用起始地址：`0x26000`（适用于带有 nice!nano / Adafruit S140 6.1.1 UF2 Bootloader 的 nRF52840 开发板）

如需直接下载编译好的固件刷机，请前往 GitHub 仓库的 **Releases** 页面下载。两块开发板必须刷入同一版本的 `.uf2` 固件。

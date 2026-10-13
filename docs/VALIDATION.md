# 测试与验证说明

本项目提供了一套脱离硬件即可在桌面运行的 C++ 回归测试套件（位于 `tests/` 目录），用于验证无线协议栈、USB 组包状态机及异常恢复逻辑。

## 一、运行桌面测试套件

确保本机已安装支持 C++11 的 `g++` 编译器，在项目根目录执行：

```powershell
.\run_tests.ps1
```

## 二、测试覆盖范围

- **`free_role_test.cpp`**：验证单固件上电探测、主从自由角色自动分配与有线/无线模式切换逻辑。
- **`packet128_test.cpp`**：验证 128 字节 CMSIS-DAP v2 请求/响应跨 64 字节 USB 事务的分片组包、窗口并发与背压状态机。
- **`recovery_test.cpp`**：验证无线断链、会话超时、旧会话包隔离及软复位恢复流程。
- **`uart_latency_test.cpp` & `uart_timing_probe.cpp`**：验证无线 UART 1 ms 短帧合包、参数同步、连续序号去重及 `20/40/80/160 ms` 退避重传时序。
- **`radio_test.cpp`**：基于模拟 `NRF_RADIO` 外设状态机的 1,000 组随机双向通信压力测试，覆盖丢包、丢 ACK、CRC 错误、重复包、计时器回绕与取消操作。

## 三、固件校验工具

- **`tools/verify_uf2.py`**：检查生成的 `.uf2` 固件块结构、Family ID（`0xADA52840`）及目标起始地址（`0x26000`）是否合法。
- **`tools/verify_source.py`**：对照 `source-manifest.json` 校验核心生产源码完整性。

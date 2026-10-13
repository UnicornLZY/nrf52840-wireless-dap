# Contributing

欢迎提交 Issue 或 Pull Request 参与改进本项目！

1. **反馈问题 (Issues)**：
   提交 Issue 时请尽量附上开发板与目标 MCU 型号、接线与供电方式、Keil/上位机版本以及具体的报错信息或复现步骤。
2. **提交代码 (Pull Requests)**：
   - 修改代码后请运行 `.\run_tests.ps1` 和 `.\build.ps1`，确保桌面回归测试与固件编译通过。
   - 若修改了无线通信协议格式，请确保收发两端兼容或同步更新协议版本号。
   - 请保持第三方开源组件（如 `libraries/WirelessDAP/src/arm/`）原有的版权声明与许可证头信息。

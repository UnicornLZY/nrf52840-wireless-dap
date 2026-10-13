# 构建说明

当前脚本固定复现低延迟主线配置，不默认切换到历史64字节、8MHz或V4实验。

## 环境

- Windows PowerShell（已在本机验证）；Arduino CLI，建议1.5.1。
- Adafruit nRF52 Core 1.7.0，包含该版本配套工具链、S140头文件、TinyUSB、FreeRTOS及Nordic SDK。
- Python 3，用于Core自带 `uf2conv.py` 和项目的UF2检查。
- MinGW g++或其他支持C++11的g++，仅用于桌面模型测试。

安装工具后确认 `arduino-cli version`、`python --version`、`g++ --version` 可运行。

```powershell
arduino-cli core update-index --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core install adafruit:nrf52@1.7.0 --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
.\build.ps1
.\run_tests.ps1
```

工具未在PATH时：

```powershell
.\build.ps1 -Cli '你的路径\arduino-cli.exe' -Python '你的路径\python.exe' -Core '你的路径\nrf52\1.7.0'
.\run_tests.ps1 -Cxx '你的路径\g++.exe'
```

`-Core` 默认根据当前用户的LOCALAPPDATA定位Arduino15目录，没有写死作者电脑用户名。脚本没有自动刷板步骤。

## 固定配置

FQBN为 `adafruit:nrf52:pca10056:softdevice=s140v6,debug=l0`；本地variant显式覆盖PCA10056引脚配置。实际板级编号使用raw GPIO，不是SuperMini丝印Dx编号。

构建参数包含ESB、DAP128、TinyUSB兼容发送、右侧P1.13映射、紧凑无线请求、会话恢复、软件复位状态确认、240条trace、2Mbps频道40和4MHz SWD限幅；完整参数见根目录 `build.ps1`。

`libraries/WirelessDAP/src/arm/` 已包含构建所需的固定ARM源码，无需克隆原工程vendor目录，也不改全局SDK。

## 输出和原固件核对

输出 `firmware/uart_latency_packet128_unified_v3_p113_2m_fast_ch40.uf2`。`build/` 中保留ELF、HEX及中间文件，但不提交Git。

原指定固件哈希为 `7E3C4C9DACCF2E4EE8F4F7374D04B86C9DC7B918FE94259589ED8AC29D928147`。构建环境不同可能导致二进制不同；查看 `docs/VALIDATION.md` 的本次重建结果，不能只凭文件名判断版本。

应用UF2从0x26000开始，不包含完整引导/SoftDevice安装。板子必须已有相容的S140 6.1.1和nice!nano兼容UF2引导布局；不要整片擦除后仅刷应用。

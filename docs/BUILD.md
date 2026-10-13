# 构建说明

本项目使用 Arduino CLI 与 Adafruit nRF52 Core **1.7.0** 进行编译，源码仓库已内置所需的 ARM CMSIS-DAP 协议层文件与板级 `variant` 引脚映射，可开箱即用。

## 一、环境准备

- **Windows PowerShell**
- **Arduino CLI**（建议使用 `1.5.x` 及以上版本）
- **Adafruit nRF52 Core `1.7.0`**（包含 ARM GCC 工具链、S140 头文件、TinyUSB、FreeRTOS 及 Nordic SDK）
- **Python 3**（用于调用 `uf2conv.py` 生成 UF2 镜像及运行 `tools/verify_uf2.py` 校验）
- **g++ 编译器**（可选，如 MinGW-w64，用于编译运行 `tests/` 下的桌面回归测试）

## 二、编译与测试命令

在仓库根目录下打开 PowerShell 执行：

```powershell
# 1. 安装 Adafruit nRF52 Core 1.7.0
arduino-cli core update-index --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core install adafruit:nrf52@1.7.0 --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json

# 2. 编译生成 UF2 固件
.\build.ps1

# 3. 运行桌面回归测试（可选）
.\run_tests.ps1
```

如果相关工具未加入系统 `PATH` 环境变量，可通过参数显式指定路径：

```powershell
.\build.ps1 -Cli 'C:\path\to\arduino-cli.exe' -Python 'C:\path\to\python.exe' -Core 'C:\path\to\nrf52\1.7.0'
.\run_tests.ps1 -Cxx 'C:\path\to\g++.exe'
```

## 三、编译配置说明

- **目标 FQBN**：`adafruit:nrf52:pca10056:softdevice=s140v6,debug=l0`，通过本地 `variant/` 目录覆盖引脚定义（代码中使用 nRF52840 原生 GPIO 编号，而非开发板丝印 `Dx` 编号）。
- **核心编译宏**：启用 2.4 GHz ESB 私有传输、128 字节 DAP 逻辑包、TinyUSB 兼容发送状态机、右侧 `P1.13` 复位引脚映射、紧凑无线请求、会话隔离与恢复、软件复位状态确认、2 Mbps 速率（频道 40）及 4 MHz SWD 时钟上限。完整宏定义见根目录 [`build.ps1`](../build.ps1)。
- **构建产物**：编译成功后输出至 `firmware/uart_latency_packet128_unified_v3_p113_2m_fast_ch40.uf2`（应用起始地址 `0x26000`，需配合板载 S140 6.1.1 与 UF2 Bootloader 使用）。

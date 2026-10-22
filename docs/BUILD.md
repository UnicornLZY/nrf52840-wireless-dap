# 构建与编译说明

本项目使用 Arduino CLI 与 Adafruit nRF52 Core **1.7.0** 进行固件构建，源码仓库已内置所需的 ARM CMSIS-DAP 协议层文件与板级 `variant` 引脚映射，可开箱即用。

---

## 一、环境准备

1. **操作系统**：Windows 10 / 11 配合 PowerShell
2. **Arduino CLI**：建议使用 `1.5.x` 及以上版本
3. **Adafruit nRF52 Core `1.7.0`**：
   包含 ARM GCC 交叉工具链、Nordic S140 协议栈头文件、TinyUSB 与 FreeRTOS 内核
4. **Python 3**：用于调用 `uf2conv.py` 转换生成 `.uf2` 镜像及执行结构哈希校验
5. **g++ (MinGW-w64)**（可选）：用于运行桌面自动化回归测试

---

## 二、安装依赖核心与工具

在 PowerShell 中执行以下命令安装 Adafruit nRF52 核心包：

```powershell
# 1. 更新索引并安装 Adafruit nRF52 1.7.0 核心
arduino-cli core update-index --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core install adafruit:nrf52@1.7.0 --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
```

---

## 三、编译生产 UF2 固件

在仓库根目录下执行构建脚本：

```powershell
# 编译默认 v4.3 稳定版（240B MTU / 2 Mbps / 信道 80 / AFH 智能跳频）
.\build.ps1
```

构建脚本会自动完成：
1. 查找并调用 `arduino-cli` 进行交叉编译
2. 提取编译产物中的 `.hex` 文件
3. 调用 `uf2conv.py` 将固件封装为适配 Adafruit / nice!nano Bootloader 的 `.uf2` 格式
4. 输出至 `firmware/uart_latency_packet240_unified_v3_p113_2m_fast_ch80_afh_v2.uf2`
5. 自动运行 `tools/verify_uf2.py` 执行 UF2 扇区边界及 SHA-256 结构自检

### 自定义编译参数

构建脚本支持灵活传参定制：
```powershell
# 示例：定制为 128 字节包、基准信道 40
.\build.ps1 -PacketBytes 128 -Channel 40

# 示例：显式指定外部工具路径
.\build.ps1 -Cli 'C:\Tools\arduino-cli.exe' -Python 'C:\Python312\python.exe'
```

---

## 四、运行桌面单元与协议回归测试

在不需要连接物理硬件的情况下，可通过桌面模拟器测试核心协议栈、链路重传机制与虚拟文件系统：

```powershell
.\run_tests.ps1
```

测试脚本将自动调用 `g++` 编译并运行以下 9 项测试用例：
1. **`free_role_test`**：双机无状态自由角色协商与有线插入平滑接管测试
2. **`packet128_test`**：大包传输窗口与并发吞吐模型测试
3. **`recovery_test`**：空口单向/双向丢包超时重试、会话自愈及 USB 管道冲刷测试
4. **`uart_latency_test`**：1ms 短帧自适应合包与全双工回显延时测试
5. **`uart_timing_probe`**：高波特率串口发送精度与抗抖动测试
6. **`config_disk_test`**：1MB 虚拟 U 盘 FAT12 文件系统一致性、`DAPLINK.exe` 逐字节校验与 Flash 模拟持久化测试
7. **`afh_test`**：AFH 智能自适应跳频全信道伪随机正交分布算法数学验证
8. **`usb_compat_test`**：WinUSB Bulk 零长包 (ZLP) 状态机与即时完成序列化测试
9. **`radio_test`**：Nordic 2.4G ESB 物理层 1000 轮随机全双工双向交互与抗误码压力测试

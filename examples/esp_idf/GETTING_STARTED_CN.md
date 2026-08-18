# 环境准备（VS Code + EIM）

本页面向第一次使用 ESP-IDF 的开发者。所有示例都是**独立工程**：打开对应示例目录即可编译，依赖由 [ESP Component Registry](https://components.espressif.com/) 在首次构建时自动下载，**不要**把本仓库其它目录加进工程。

硬件：VIEWE **UEDX48480040E-WB-A**（ESP32-S3，4 寸 480×480 RGB，触摸 FT5x06）。  
软件：ESP-IDF **≥ 5.5**（建议安装 **5.5.x**）。板级支持包：[`viewesmart/bsp_uedx48480040e_wb_a`](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a)。

[English](./GETTING_STARTED.md)

## 1. 安装 Visual Studio Code

1. 打开 [https://code.visualstudio.com/](https://code.visualstudio.com/) 下载并安装 VS Code。
2. Windows 用户建议用官网安装包，不要用商店精简版。
3. 安装时勾选 **Add to PATH**（添加到 PATH）。

## 2. 安装 ESP-IDF 扩展

1. 打开 VS Code。
2. 左侧点扩展图标，或按 `Ctrl+Shift+X`。
3. 搜索 **ESP-IDF**，发布者必须是 **Espressif Systems**。
4. 点 **Install**。

扩展页： [ESP-IDF for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)

## 3. 用 EIM 安装 ESP-IDF（推荐）

**EIM**（ESP-IDF Installation Manager）是乐鑫官方安装器。VS Code 扩展 v2 起也用它来安装和发现 IDF 版本，不必再手动填 `IDF_PATH`。

### 3.1 从 VS Code 启动 EIM

1. 按 `Ctrl+Shift+P` 打开命令面板。
2. 输入并选择：**ESP-IDF: Open ESP-IDF Installation Manager**。
3. 桌面环境会弹出 EIM 图形界面；若在 SSH / WSL / 无界面环境，会在终端里走向导。

也可单独下载 EIM：

- 下载页：[https://dl.espressif.com/dl/eim/](https://dl.espressif.com/dl/eim/)（国内较快）
- GitHub：[https://github.com/espressif/idf-im-ui/releases](https://github.com/espressif/idf-im-ui/releases)
- Windows 也可用：`winget install Espressif.EIM`
- 文档：[EIM 说明](https://docs.espressif.com/projects/idf-im-ui/zh_CN/latest/)

### 3.2 在 EIM 里安装 5.5.x

1. 欢迎页点 **Start Installation**。
2. 选 **Easy Installation**（新手请用这项）。
3. 镜像：在中国大陆建议选 **Espressif** 下载服务器，不要强行用 GitHub（容易超时）。
4. 版本选 **ESP-IDF 5.5.x**（本 BSP 要求 `idf >= 5.5`）。不要选 4.x。
5. 等待工具链、Python、调试器下载完成（可能十几分钟）。
6. 完成后可在 Dashboard 看到已安装版本，然后关掉 EIM。

Windows 默认安装位置大致为 `C:\Espressif\`，清单文件：`C:\Espressif\tools\eim_idf.json`。  
macOS / Linux：`~/.espressif/tools/eim_idf.json`。

### 3.3 让 VS Code 使用刚装的 IDF

1. 命令面板： **ESP-IDF: Select Current ESP-IDF Version**。
2. 选刚才安装的 **5.5.x**。
3. 可选：命令面板运行 **ESP-IDF: Doctor Command**，确认没有红色错误。

## 4. USB 驱动与串口

1. 用数据线把板子接到电脑（需能传数据，不要只用充电线）。
2. Windows：设备管理器里应出现 **USB-SERIAL CH340 / CP210x / FTDI** 之类端口，例如 `COM3`。
3. 没有端口时，按板子丝印安装对应 USB 转串口驱动。
4. 若提示占用，先关掉其它串口监视器（Arduino、其它 VS Code 窗口、`idf.py monitor`）。

## 5. 打开示例并编译烧录

**务必打开示例自己的文件夹**，不要打开上一级仓库根目录。例如 I2C 扫描：

`文件` → `打开文件夹` → 选 `examples/01_i2c_scan`

然后：

1. 命令面板：**ESP-IDF: Set Espressif Device Target** → **esp32s3**。
2. 状态栏选择串口（如 `COM3`）。
3. 点底部 **Build**（圆柱图标）或命令 **ESP-IDF: Build your project**。  
   第一次会联网下载 `managed_components/`（含 `viewesmart/bsp_uedx48480040e_wb_a`），需要能访问组件注册表。
4. **ESP-IDF: Flash your project** 烧录。
5. **ESP-IDF: Monitor your device** 看日志。退出监视：`Ctrl+]`。

状态栏快捷按钮一般是：芯片目标 | 串口 | 火焰（编译烧录监视）| 垃圾桶（清理）。

### 国内访问组件注册表较慢时

在 VS Code 终端（已激活 IDF 环境）可先设镜像再编译：

```powershell
$env:IDF_COMPONENT_REGISTRY_URL = "https://components-file.espressif.cn"
```

Linux / macOS：

```bash
export IDF_COMPONENT_REGISTRY_URL=https://components-file.espressif.cn
```

## 6. 命令行方式（可选）

EIM 安装完成后，用 **ESP-IDF 5.5 PowerShell**（开始菜单）或先执行 `export.ps1` / `export.sh`，再进入**示例目录**：

```powershell
cd examples/01_i2c_scan
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

Linux / macOS 把 `-p COM3` 换成 `-p /dev/ttyACM0` 或 `/dev/ttyUSB0`。

## 7. 所有示例共用的硬件注意

| 项目 | 说明 |
|------|------|
| 模组 | UEDX48480040E-WB-A，ESP32-S3 |
| Flash / PSRAM | 16 MB Flash + Octal PSRAM（`sdkconfig.defaults` 已按此配置） |
| 屏 | 480×480，GC9503，RGB565 |
| 触摸 | FT5x06，I2C 地址 `0x38`，SDA=GPIO40，SCL=GPIO41 |
| 背光 | GPIO38 |
| SD | SDSPI；CS（GPIO47）与 LCD 3-wire SDA 复用，必须先起屏再挂卡 |
| 卡格式 | FAT32；相册 JPEG 放到卡上的 `photos` 目录（挂载后为 `/sdcard/photos`） |

## 8. 常见安装问题

| 现象 | 处理 |
|------|------|
| 扩展找不到 IDF | 再用 EIM 装一次；执行 **Select Current ESP-IDF Version**；自定义路径时设置 `idf.eimIdfJsonPath` |
| 编译缺组件 / 下载失败 | 检查网络；设 `IDF_COMPONENT_REGISTRY_URL`；删除示例下 `managed_components` 与 `dependencies.lock` 后重编 |
| `idf.py` 不是命令 | 未进入 IDF 终端；VS Code 里用扩展自带的 ESP-IDF 终端 |
| 黑屏 | 确认 Target 是 esp32s3、Octal PSRAM 已开、烧录的是本示例固件 |
| 串口 Permission denied / 占用 | 关掉其它 monitor；Linux 把用户加入 `dialout` 组 |
| GCC ICE / 奇怪的编译器内部错误 | 关闭 ccache 后再编：环境变量 `IDF_CCACHE_ENABLE=0` |
| CMake 提示 object 路径超过 250 字符（05/06） | 把示例放到更短的路径（如 `C:\work\05_album`），或在系统中开启 Windows 长路径 |

更细的排障见各示例自己的 README。

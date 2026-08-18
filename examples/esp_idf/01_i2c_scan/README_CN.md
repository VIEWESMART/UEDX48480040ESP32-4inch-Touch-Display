# 01 — I2C 扫描（触摸 FT5x06）

扫描板上触摸芯片所在的 I2C 总线，并把结果画在 480×480 屏幕上。这是验证屏、背光、触摸供电和 I2C 接线的第一步。

[English](./README.md) · [环境准备](../GETTING_STARTED_CN.md) · [示例索引](../README_CN.md)

## 它做什么

1. 调用 `bsp_display_start()` 启动 LCD、背光、FT5x06 触摸和 LVGL。触摸初始化时会打开 I2C（SDA=GPIO40，SCL=GPIO41）。
2. 用 `bsp_i2c_get_handle()` 在同一条总线上扫描 7 位地址。
3. 屏幕显示徽章 + 地址列表。约每 4 秒重扫一次，方便你插拔排线时观察。

预期从器件：**0x38**（FT5x06）。

## 需要什么

- 开发板：VIEWE **UEDX48480040E-WB-A**（ESP32-S3，16 MB Flash + Octal PSRAM）
- ESP-IDF **5.5.x**（用 **EIM** 安装，见 [环境准备](../GETTING_STARTED_CN.md)）
- VS Code + 官方 **ESP-IDF** 扩展
- USB 数据线、能访问 [组件注册表](https://components.espressif.com/) 的网络（首次编译下载 BSP）

本示例 `main/idf_component.yml` 只声明：

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
```

不要把仓库根目录的 `components/` 或 `managed_components/` 加进本工程。

## 使用步骤（VS Code）

1. 按 [环境准备](../GETTING_STARTED_CN.md) 装好 VS Code、ESP-IDF 扩展，并用 **EIM** 安装 ESP-IDF **5.5.x**。命令面板执行 **ESP-IDF: Select Current ESP-IDF Version**。
2. `文件` → `打开文件夹`，选中 **本目录** `examples/01_i2c_scan`（不要打开上一级）。
3. `Ctrl+Shift+P` → **ESP-IDF: Set Espressif Device Target** → **esp32s3**。
4. 状态栏选择板子的串口（如 `COM3`）。
5. **ESP-IDF: Build your project**。第一次会生成 `managed_components/viewesmart__bsp_uedx48480040e_wb_a`，需要联网。
6. **ESP-IDF: Flash your project** 烧录。
7. **ESP-IDF: Monitor your device** 打开串口。退出：`Ctrl+]`。

也可用底部状态栏的编译 / 烧录 / 监视按钮。

### 命令行

在已激活 IDF 的终端中：

```bash
cd examples/01_i2c_scan
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

Linux / macOS 把 `COM3` 换成 `/dev/ttyACM0` 或 `/dev/ttyUSB0`。

## 如何验证

| 屏幕 | 含义 |
|------|------|
| 绿色大徽章 **PASS**，列表里有 `0x38` 或 `FT5x06` | 触摸 I2C 正常，本示例成功 |
| 红色 **FAIL**，列表为空或没有 0x38 | 触摸未应答：排线、供电、或不是 FT5x06 模组 |
| 完全黑屏 | 屏或背光未起来：先确认已选 esp32s3、Octal PSRAM，并烧录成功 |

串口（标签 `i2c_scan`）应周期性出现：

```text
Scan I2C bus 0
  found 0x38
FT5x06 PASS
```

## 常见问题

- **只亮背光没有字**：LVGL 未跑起来或锁没配对；确认工程就是本示例且编译无报错。
- **扫到别的地址、没有 0x38**：I2C 有从设备但不是这块触摸；检查模组型号。
- **组件下载失败**：见 [环境准备 §5](../GETTING_STARTED_CN.md) 的注册表镜像。
- **编译器 ICE**：设置 `IDF_CCACHE_ENABLE=0` 后清理再编。

## 代码入口

`main/i2c_scan_main.c`。公开头文件：`#include "bsp/esp-bsp.h"`。

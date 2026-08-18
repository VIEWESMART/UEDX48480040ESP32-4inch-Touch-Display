# 02 — Wi-Fi 扫描 / 连接

在屏幕上列出附近 AP。未填写 SSID 时，扫描成功即为 **PASS**；填写后需拿到 IP 才算 **PASS**。

[English](./README.md) · [环境准备](../GETTING_STARTED_CN.md) · [示例索引](../README_CN.md)

## 它做什么

1. `bsp_display_start()` 起屏和 LVGL。
2. 初始化 NVS，再 `bsp_wifi_init()` 进入 STA。
3. `bsp_wifi_scan()` 按 RSSI 列出热点（最多 `BSP_WIFI_SCAN_MAX` 个）。
4. 若在 menuconfig 里写了 SSID，会再 `bsp_wifi_connect()`，最多等约 15 秒。
5. 已请求连接时，约每 4 秒刷新状态字符串和 RSSI。

## 需要什么

- 与 [01_i2c_scan](../01_i2c_scan/README_CN.md) 相同的板子和 **ESP-IDF 5.5.x（EIM）**
- 2.4 GHz Wi-Fi（ESP32-S3 不支持 5 GHz 联网）
- 首次编译需访问组件注册表

依赖：

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
```

## 使用步骤（VS Code）

1. 完成 [环境准备](../GETTING_STARTED_CN.md)，用 EIM 安装 5.5.x。
2. 打开文件夹 **`examples/02_wifi`**（不要打开仓库根目录）。
3. **ESP-IDF: Set Espressif Device Target** → **esp32s3**，选好串口。
4. **只扫描、不连接（默认）**：SSID 留空即可，直接编译烧录。
5. **要连接家里的路由器**：  
   - 命令面板 **ESP-IDF: SDK Configuration editor (menuconfig)**  
   - 进入 **Example Configuration**  
   - 填写 **WiFi SSID** 和 **WiFi password**  
   - 保存后重新 **Build** 再 **Flash**
6. **ESP-IDF: Flash your project** → **Monitor**。

命令行配置：

```bash
cd examples/02_wifi
idf.py menuconfig    # Example Configuration
idf.py set-target esp32s3 build flash monitor
```

也可改 `sdkconfig` 里的 `CONFIG_EXAMPLE_WIFI_SSID` / `CONFIG_EXAMPLE_WIFI_PASSWORD`（`main/Kconfig.projbuild` 定义）。

## 如何验证

| 屏幕徽章 | 含义 |
|----------|------|
| 琥珀色 **SCAN** / **JOIN** | 进行中 |
| 绿色 **PASS**，SSID 为空 | 至少完成一次扫描（附近没热点时列表可能为空，仍算扫描成功） |
| 绿色 **PASS**，已填 SSID | 已关联并拿到 IP |
| 红色 **FAIL** | 扫描失败，或连接超时 / 密码错误 |

串口标签 `wifi_ex` 会打印扫描到的 SSID 和最终状态。

## 常见问题

- **扫描 PASS 但连不上**：确认是 2.4 GHz、密码无空格错误、距离不要太远。
- **一直 JOIN**：路由器隔离或 DHCP 异常；先把 SSID 留空确认射频正常。
- **编译下载 BSP 失败**：见 [环境准备](../GETTING_STARTED_CN.md)。

## 代码入口

`main/wifi_main.c`，Wi-Fi API 来自 BSP：`bsp_wifi_init` / `scan` / `connect`。

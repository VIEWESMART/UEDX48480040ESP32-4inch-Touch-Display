# UEDX48480040E-WB-A 示例

每个目录都是独立的 ESP-IDF 工程。首次编译会从 [乐鑫组件注册表](https://components.espressif.com/) 拉取 [`viewesmart/bsp_uedx48480040e_wb_a`](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a)，**不依赖本仓库其它组件**。

请先阅读：[环境准备（VS Code + EIM）](./GETTING_STARTED_CN.md) · [English getting started](./GETTING_STARTED.md)

[English](./README.md)

## 示例一览

| 目录 | 作用 | 怎样算成功 |
|------|------|------------|
| [01_i2c_scan](01_i2c_scan/README_CN.md) | 扫描触摸 I2C | 绿 **PASS**，列表含 **0x38** |
| [02_wifi](02_wifi/README_CN.md) | 扫描（可选连接）Wi-Fi | 绿 **PASS**，列出附近 AP |
| [03_lvgl_port](03_lvgl_port/README_CN.md) | LVGL 移植 / 官方 demo | 能看见界面；点 **Tap me** 徽章变绿 |
| [04_sd](04_sd/README_CN.md) | 挂载 SD 卡 | 绿 **PASS**，列出 `/sdcard` |
| [05_album](05_album/README_CN.md) | Phone 桌面 + 相册 | 中央弹出 LCD/Touch/SD，然后打开 Album |
| [06_brookesia_iphone](06_brookesia_iphone/README_CN.md) | 完整 Phone UI | 看到桌面图标；Screen/Touch Test 可用 |

建议按 01 → 02 → 03 → 04 的顺序验证硬件，再跑 05 / 06。

## 最短路径（已装好 IDF 时）

用 VS Code **打开示例文件夹**（例如 `examples/01_i2c_scan`），目标选 **esp32s3**，再 Build → Flash → Monitor。

命令行（在示例目录内）：

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

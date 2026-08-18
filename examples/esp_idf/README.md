# UEDX48480040E-WB-A examples

Each folder is a standalone ESP-IDF project. The first build fetches [`viewesmart/bsp_uedx48480040e_wb_a`](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a) from the [ESP Component Registry](https://components.espressif.com/). Examples do **not** depend on other components from this repository.

Start here: [Getting started (VS Code + EIM)](./GETTING_STARTED.md) · [中文环境准备](./GETTING_STARTED_CN.md)

[中文](./README_CN.md)

## List

| Folder | Purpose | Success on the LCD |
|--------|---------|---------------------|
| [01_i2c_scan](01_i2c_scan/README.md) | Scan touch I2C | Green **PASS**, list contains **0x38** |
| [02_wifi](02_wifi/README.md) | Scan (optional join) Wi-Fi | Green **PASS**, nearby APs listed |
| [03_lvgl_port](03_lvgl_port/README.md) | LVGL port / official demos | UI visible; **Tap me** turns the badge green |
| [04_sd](04_sd/README.md) | Mount the SD card | Green **PASS**, `/sdcard` listing |
| [05_album](05_album/README.md) | Phone launcher + album | LCD/Touch/SD banner, then open Album |
| [06_brookesia_iphone](06_brookesia_iphone/README.md) | Full Phone UI | Launcher icons; Screen/Touch Test work |

Bring up hardware in order 01 → 02 → 03 → 04, then run 05 / 06.

## Shortest path (IDF already installed)

In VS Code **open the example folder** (for example `examples/01_i2c_scan`), set target **esp32s3**, then Build → Flash → Monitor.

CLI, from inside the example directory:

```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

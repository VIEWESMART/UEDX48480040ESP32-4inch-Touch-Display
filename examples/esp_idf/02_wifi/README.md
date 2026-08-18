# 02 — Wi-Fi scan / connect

Lists nearby APs on the LCD. With an empty SSID, a successful scan is **PASS**. With SSID filled in, **PASS** only after the station gets an IP.

[中文](./README_CN.md) · [Getting started](../GETTING_STARTED.md) · [Example index](../README.md)

## What it does

1. Starts the panel and LVGL with `bsp_display_start()`.
2. Inits NVS, then `bsp_wifi_init()` in STA mode.
3. `bsp_wifi_scan()` lists APs by RSSI (up to `BSP_WIFI_SCAN_MAX`).
4. If menuconfig has an SSID, it calls `bsp_wifi_connect()` and waits up to ~15 s.
5. When a join was requested, status and RSSI refresh about every 4 seconds.

## Requirements

- Same board and **ESP-IDF 5.5.x (EIM)** as [01_i2c_scan](../01_i2c_scan/README.md)
- 2.4 GHz Wi-Fi (ESP32-S3 cannot join 5 GHz)
- Registry access on the first build

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
```

## Steps (VS Code)

1. Follow [Getting started](../GETTING_STARTED.md) and install 5.5.x with EIM.
2. Open folder **`examples/02_wifi`**.
3. Set target **esp32s3** and pick the serial port.
4. **Scan only (default):** leave SSID empty, build and flash.
5. **Join an AP:** **ESP-IDF: SDK Configuration editor (menuconfig)** → **Example Configuration** → SSID and password → save → rebuild and flash.
6. Flash, then Monitor.

CLI:

```bash
cd examples/02_wifi
idf.py menuconfig
idf.py set-target esp32s3 build flash monitor
```

## How to verify

| Badge | Meaning |
|-------|---------|
| Amber **SCAN** / **JOIN** | In progress |
| Green **PASS**, empty SSID | At least one scan finished |
| Green **PASS**, SSID set | Associated and got an IP |
| Red **FAIL** | Scan failed, or join timed out / rejected |

Log tag: `wifi_ex`.

## Troubleshooting

- Scan PASS but join fails: use 2.4 GHz, check the password, move closer.
- Stuck on JOIN: try empty SSID first to prove RF works.
- BSP download fails: [Getting started](../GETTING_STARTED.md).

## Source

`main/wifi_main.c`. Wi-Fi helpers: `bsp_wifi_init` / `scan` / `connect`.

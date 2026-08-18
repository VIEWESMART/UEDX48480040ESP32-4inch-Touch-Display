# 01 — I2C scan (FT5x06 touch)

Scans the touch I2C bus and draws the result on the 480×480 panel. Use this first to confirm the LCD, backlight, touch power, and I2C wiring.

[中文](./README_CN.md) · [Getting started](../GETTING_STARTED.md) · [Example index](../README.md)

## What it does

1. `bsp_display_start()` brings up the LCD, backlight, FT5x06, and LVGL. Touch init opens I2C (SDA=GPIO40, SCL=GPIO41).
2. The same bus is scanned via `bsp_i2c_get_handle()`.
3. The screen shows a badge plus addresses. The bus is re-scanned about every 4 seconds.

Expected device: **0x38** (FT5x06).

## Requirements

- Board: VIEWE **UEDX48480040E-WB-A** (ESP32-S3, 16 MB flash + octal PSRAM)
- ESP-IDF **5.5.x** installed with **EIM** — see [Getting started](../GETTING_STARTED.md)
- VS Code + the official **ESP-IDF** extension
- USB cable and internet access to the [Component Registry](https://components.espressif.com/) (first build)

`main/idf_component.yml` only lists:

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
```

Do not add this repository’s `components/` or `managed_components/` to the example.

## Steps (VS Code)

1. Follow [Getting started](../GETTING_STARTED.md): VS Code, ESP-IDF extension, **EIM** → ESP-IDF **5.5.x**, then **ESP-IDF: Select Current ESP-IDF Version**.
2. `File` → `Open Folder` → **this directory** `examples/01_i2c_scan` (not the parent repo).
3. Command Palette → **ESP-IDF: Set Espressif Device Target** → **esp32s3**.
4. Select the serial port on the status bar (e.g. `COM3`).
5. **ESP-IDF: Build your project**. The first build creates `managed_components/viewesmart__bsp_uedx48480040e_wb_a`.
6. **ESP-IDF: Flash your project**.
7. **ESP-IDF: Monitor your device**. Leave with `Ctrl+]`.

### CLI

```bash
cd examples/01_i2c_scan
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

On Linux / macOS use `/dev/ttyACM0` or `/dev/ttyUSB0`.

## How to verify

| On screen | Meaning |
|-----------|---------|
| Green **PASS**, list contains `0x38` / FT5x06 | Touch I2C is healthy |
| Red **FAIL**, no 0x38 | Touch did not ACK (flex cable, power, or wrong module) |
| Black screen | Panel/backlight did not start — check target, octal PSRAM, flash |

Serial log tag `i2c_scan`:

```text
Scan I2C bus 0
  found 0x38
FT5x06 PASS
```

## Troubleshooting

- Backlight only, no text: confirm this example built cleanly.
- Other addresses but not 0x38: I2C works, touch part number may differ.
- Component download fails: see [Getting started §5](../GETTING_STARTED.md).
- GCC ICE: rebuild with `IDF_CCACHE_ENABLE=0`.

## Source

`main/i2c_scan_main.c`. Include `"bsp/esp-bsp.h"`.

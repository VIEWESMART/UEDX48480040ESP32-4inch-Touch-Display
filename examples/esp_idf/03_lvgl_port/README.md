# 03 — LVGL port

Wires GC9503 + FT5x06 to LVGL 9 through the BSP. You can run a local tap-check UI or switch to official Widgets / Music / Benchmark demos.

[中文](./README_CN.md) · [Getting started](../GETTING_STARTED.md) · [Example index](../README.md)

## What it does

`bsp_display_start()` starts the RGB panel, touch, and `esp_lvgl_adapter`. Only one UI runs, selected by **`APP_UI_SELECT`** in `main/app_ui_select.h`.

| Macro | UI |
| --- | --- |
| `APP_UI_LOCAL` | Local: title, badge, **Tap me**, backlight slider |
| `APP_UI_DEMO_WIDGETS` | Official widgets (default) |
| `APP_UI_DEMO_MUSIC` | Official music player look (square layout; no audio decode) |
| `APP_UI_DEMO_BENCHMARK` | Official benchmark |
| `APP_UI_DEMO_STRESS` | Official stress test |
| `APP_UI_DEMO_KEYPAD_ENCODER` | Official keypad/encoder demo (this board is capacitive touch) |

`sdkconfig.defaults` already enables the matching `CONFIG_LV_USE_DEMO_*` options. Rebuild and flash after changing the macro.

## Requirements

- Same board and **ESP-IDF 5.5.x (EIM)** as [Getting started](../GETTING_STARTED.md)

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
  lvgl/lvgl:
    version: "^9"
    public: true
```

## Steps (VS Code)

1. Install IDF 5.5.x with EIM; **Select Current ESP-IDF Version**.
2. Open **`examples/03_lvgl_port`**.
3. Target **esp32s3**, pick the port.
4. Optional: edit `main/app_ui_select.h`. Beginners can try `APP_UI_LOCAL` first, then `APP_UI_DEMO_WIDGETS`.
5. Build → Flash → Monitor.

```bash
cd examples/03_lvgl_port
idf.py set-target esp32s3
idf.py build flash monitor
```

## How to verify

**Local UI (`APP_UI_LOCAL`)**

1. Title and badge visible → LCD + LVGL OK.
2. Tap **Tap me**, badge turns green **PASS** → touch OK.
3. Bottom slider changes brightness → backlight OK.

**Official Widgets (default)**

- Tabs and widgets respond to touch → port is good.
- Black screen: check the log for `bsp_display_start failed`.

**Music / Benchmark / Stress**

- Music: cover and spectrum only; no speaker is expected.
- Benchmark: FPS summary at the end.
- Stress: not a product UI; watch for glitches or leaks.

## Troubleshooting

- Macro change has no effect: rebuild/flash; keep demo Kconfig options from `sdkconfig.defaults`.
- Color swap: do not remap RGB data pins; the BSP already handles hardware R/B swap.
- Missing demo symbols: enable `CONFIG_LV_BUILD_DEMOS` and the matching `CONFIG_LV_USE_DEMO_*`.

## Source

`main/lvgl_port_main.c`, `main/app_ui_select.h`.

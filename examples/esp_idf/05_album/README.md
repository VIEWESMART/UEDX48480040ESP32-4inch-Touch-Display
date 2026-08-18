# 05 — Photo album (ESP-Brookesia Phone)

Starts a Brookesia Phone launcher on 480×480 with only **Album** installed. A centered LCD / touch / SD banner appears at boot. JPEGs are read from `/sdcard/photos`.

This folder is a **standalone project**. Brookesia, the decoder, LVGL, and the BSP come from the registry. Album sources live under this example’s `components/` — they do **not** reference the repository root.

[中文](./README_CN.md) · [Getting started](../GETTING_STARTED.md) · [Example index](../README.md)

## What it does

1. Init NVS.
2. `bsp_display_start_with_config()` with SD mount.
3. `esp_lv_decoder_init()` for JPEG in LVGL.
4. Create Phone, apply the 480×480 dark stylesheet, bind touch.
5. Install `PhotoAlbumApp` and show a self-test banner (5 s if all OK, sticky on FAIL).

Bring up [01](../01_i2c_scan/README.md), [03](../03_lvgl_port/README.md), and [04](../04_sd/README.md) first.

## Requirements

- Board + **ESP-IDF 5.5.x (EIM)** and internet on the first build
- FAT32 card with `photos/*.jpg` (keep images reasonably small)

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
  espressif/esp-brookesia: "^0.5.0"
  espressif/esp_lv_decoder: "*"
  lvgl/lvgl:
    version: "^9"
    public: true
```

Local to this example only:

- `components/galleria_apps` — album app
- `components/galleria_app_icons` — launcher icon
- `components/board_support` — thin wrappers over BSP SD / Wi-Fi / backlight

## Steps (VS Code)

1. [Getting started](../GETTING_STARTED.md): VS Code, extension, **EIM → 5.5.x**.
2. FAT32 card, `photos/*.jpg`, insert the card.
3. Open **`examples/05_album`**.
4. Target **esp32s3**, select the port.
5. Build (first time downloads Brookesia, LVGL, BSP, …).
6. Flash → Monitor.

```bash
cd examples/05_album
idf.py set-target esp32s3
idf.py build flash monitor
```

## How to verify

1. **Banner:** green LCD/Touch/SD PASS dismisses after ~5 s; red stays until you fix touch or SD.
2. **Launcher** shows the Album icon → Phone started.
3. Open **Album** and view a full-screen JPEG → SD + decoder OK.
4. Empty card: the app shows an empty state; that is not a build failure.

Log tag `album_ex`: `Album example ready`.

## Troubleshooting

- SD FAIL: pass example 04; FAT32; panel must start before CS reuse.
- Album glitches: use smaller baseline JPEGs.
- Link errors: delete this example’s `build/`, `managed_components/`, and `dependencies.lock`, then rebuild.
- Do **not** point `override_path` at another project’s `managed_components`.

## Source

`main/album_main.cpp`.

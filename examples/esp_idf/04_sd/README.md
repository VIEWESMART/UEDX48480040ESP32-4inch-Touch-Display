# 04 — SD card

Starts the LCD first (so GPIO47 can be reused as SD CS), mounts FAT over SDSPI, and shows the result on screen.

[中文](./README_CN.md) · [Getting started](../GETTING_STARTED.md) · [Example index](../README.md)

## What it does

1. `bsp_display_start_with_config()` with `flags.mount_sd = 1`.
2. Writes `bsp_sd_test.txt`, then lists a few names under the mount point.
3. Green **PASS** = mounted and probe write OK. Red **FAIL** = no card or mount failed.

The slot is **SDSPI**, not SDMMC. **GPIO47** is LCD 3-wire SDA during panel init, then SD CS. Always start the display before mounting.

## Requirements

- Board + **ESP-IDF 5.5.x (EIM)**
- MicroSD formatted **FAT32** (exFAT will not mount with this FATFS config)
- For the album later: create `photos` on the card and copy `.jpg` files (`/sdcard/photos` after mount)

```yaml
dependencies:
  idf: ">=5.5"
  viewesmart/bsp_uedx48480040e_wb_a: "^1.0.0"
```

## Steps (VS Code)

1. Install 5.5.x with EIM ([Getting started](../GETTING_STARTED.md)).
2. Format the card as FAT32, insert it, power the board.
3. Open **`examples/04_sd`**.
4. Target **esp32s3**, select the port.
5. Build → Flash → Monitor.

```bash
cd examples/04_sd
idf.py set-target esp32s3
idf.py build flash monitor
```

## How to verify

| On screen | Meaning |
|-----------|---------|
| Green **PASS** and a file list including `bsp_sd_test.txt` | SD OK |
| Red **FAIL** | No card, not FAT32, poor contact, or CS still used as LCD SDA |
| Black screen | Bring up [01](../01_i2c_scan/README.md) / [03](../03_lvgl_port/README.md) first |

Log tag: `sd_ex`.

On Windows, cards larger than 32 GB often format as exFAT only. Use a FAT32 formatter or a smaller card.

## Troubleshooting

- Intermittent FAIL: reseat the card or try another.
- Garbled names: long filenames need `CONFIG_FATFS_LFN_HEAP` (already on in `sdkconfig.defaults`).
- Album finds nothing: files must live in `photos` as `.jpg` / `.jpeg`.

## Source

`main/sd_main.c`. APIs: `bsp_sdcard_mount`, `BSP_SD_MOUNT_POINT` (default `/sdcard`).

# 06 — Brookesia Phone (full launcher)

Full Phone UI: Settings, Screen Test, Touch Test, Album, Weather. Use it as an end-to-end check of the BSP + LVGL + Brookesia stack.

This folder is **standalone**. The BSP, Brookesia, LVGL, and decoder come from the registry. App sources are under this example’s `components/` and do **not** use the repository root.

[中文](./README_CN.md) · [Getting started](../GETTING_STARTED.md) · [Example index](../README.md)

## What it does

Same bring-up as [05_album](../05_album/README.md), but installs every demo app and refreshes the status-bar clock and Wi-Fi icon.

| App | What you check |
|-----|----------------|
| Screen Test | Solid colors / patterns, RGB |
| Touch Test | Dots / trail vs finger position |
| Album | SD + JPEG, same as 05 |
| Settings | Backlight slider, Wi-Fi scan/join |
| Weather | Needs Wi-Fi; city and Amap key in `components/app/app_weather/weather_config.h` |

## Requirements

- Board + **ESP-IDF 5.5.x (EIM)**
- FAT32 SD card for the Album app (see below)
- Weather: 2.4 GHz Wi-Fi and your own Amap Web Service key (see below)

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

## SD card layout

Format the card as **FAT32** (MBR). After boot the BSP mounts it at **`/sdcard`**. On first mount the firmware also creates standard subfolders if they are missing.

```
/sdcard/
├── photos/          ← Album app (required for photos)
│   ├── 001.jpg
│   ├── 002.jpeg
│   └── ...
└── anim/            ← created by BSP; not used by this demo yet
```

| Path | Used by | Put here |
|------|---------|----------|
| `/sdcard/photos/` | **Album** | JPEG photos (`.jpg` / `.jpeg`, case-insensitive) |
| `/sdcard/anim/` | *(reserved)* | Nothing required today; folder is auto-created for future animation assets |

**Album (`photos/`) rules**

- Files must sit **directly** under `photos/` (no subfolders).
- Only **`.jpg`** and **`.jpeg`** are scanned; PNG/BMP are ignored.
- Max **4 MB** per file; keep resolution reasonable (e.g. long edge ≤ 2000 px) for faster decode on 480×480.
- Filenames are sorted alphabetically for prev/next order (e.g. `01.jpg`, `02.jpg`).
- Screen Test, Touch Test, Settings, and Weather do **not** read the SD card.

**Prepare the card (PC)**

1. Format as FAT32.
2. Create folder `photos`.
3. Copy JPEGs into `photos`.
4. Safely eject, insert into the board slot, then power on or reset.

At boot the self-test banner shows **SD PASS** when mount succeeds (empty `photos/` still passes mount; Album then shows “No JPG in /sdcard/photos/”).

## Gaode (Amap) API key

The Weather app calls the [Amap Web Service API](https://lbs.amap.com/api/webservice/summary) over HTTPS. **Do not commit a real key to a public repo.** On GitHub the placeholder in source is a dummy string; replace it locally before building.

**File:** `components/app/app_weather/weather_config.h`

```c
#define AMAP_WEATHER_API_KEY "YOUR_AMAP_API_KEY"   /* replace with your key */
#define AMAP_WEATHER_CITY_ADCODE "440300"          /* city adcode, e.g. Shenzhen */
#define WEATHER_CITY_DISPLAY_NAME "Shenzhen, Guangdong"
```

| Macro | Purpose |
|-------|---------|
| `AMAP_WEATHER_API_KEY` | Web Service Key from the Amap console |
| `AMAP_WEATHER_CITY_ADCODE` | 6-digit city adcode ([lookup](https://lbs.amap.com/api/webservice/download)) |
| `WEATHER_CITY_DISPLAY_NAME` | Label on the Weather screen (ASCII; font has no CJK) |

### How to apply for a key

1. Sign in at [Amap Open Platform](https://lbs.amap.com/) (register if needed).
2. Open **Console → Application management → Create application** (name/type as you like).
3. Under that app, **Add Key** → type **Web Service** (not JS / Android SDK).
4. In the key’s **Enabled services**, turn on at least:
   - **Weather** (`weatherInfo`, live + forecast)
   - **Air quality** (`airquality/now`, optional; AQI card shows N/A if missing)
5. Copy the Key string into `AMAP_WEATHER_API_KEY`, set your city adcode, then rebuild and flash.

Free tier has daily quotas; if requests fail, check the console for quota, service permissions, and that the key type is **Web Service**.

## Steps (VS Code)

1. [Getting started](../GETTING_STARTED.md): **EIM → 5.5.x**.
2. Prepare the SD card (`photos/*.jpg`) and edit `components/app/app_weather/weather_config.h` (API key + city).
3. Open **`examples/06_brookesia_iphone`**.
4. Target **esp32s3**, select the port.
5. Build (larger than 01–04) → Flash → Monitor.

```bash
cd examples/06_brookesia_iphone
idf.py set-target esp32s3
idf.py build flash monitor
```

## How to verify

1. Center banner: LCD PASS; Touch/SD green dismisses after ~5 s.
2. Dark launcher with round icons → Phone started.
3. **Screen Test**: full-screen colors, no large artifacts.
4. **Touch Test**: trail follows the finger.
5. **Album**: opens JPEGs in `/sdcard/photos`.
6. **Settings**: backlight changes immediately; 2.4 GHz scan; join updates the status-bar icon.
7. **Weather**: after Wi-Fi, a forecast appears (a bad API key fails this app only).

Log tag `phone_ex`: `Brookesia Phone UI started (480x480)`.

Suggested order: green banner → Screen Test → Touch Test → Album → backlight → Wi-Fi.

## Troubleshooting

- Icons do not respond: run example 01; check the overlay/film.
- No APs in Settings: 2.4 GHz only; compare with example 02.
- **Weather fails**: Wi-Fi connected? Replace `YOUR_AMAP_API_KEY` in `weather_config.h`; check adcode, Web Service key type, and enabled Weather/AQI services in the Amap console.
- Do **not** use `override_path` into another project.
- Link / RAM errors: octal PSRAM must be on; delete `build` and `managed_components` and rebuild.
- **Album empty / no photos**: card mounted? Files in `/sdcard/photos/` as `.jpg`/`.jpeg`? Under 4 MB each?

## Source

`main/phone_main.cpp`.

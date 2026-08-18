# Getting started (VS Code + EIM)

This page is for first-time ESP-IDF users. Every example is a **standalone project**: open that example folder and build. Dependencies are downloaded from the [ESP Component Registry](https://components.espressif.com/) on the first build. Do **not** add other folders from this repository as extra components.

Hardware: VIEWE **UEDX48480040E-WB-A** (ESP32-S3, 4-inch 480×480 RGB, FT5x06 touch).  
Software: ESP-IDF **≥ 5.5** (use **5.5.x**). Board support: [`viewesmart/bsp_uedx48480040e_wb_a`](https://components.espressif.com/components/viewesmart/bsp_uedx48480040e_wb_a).

[中文](./GETTING_STARTED_CN.md)

## 1. Install Visual Studio Code

1. Download VS Code from [https://code.visualstudio.com/](https://code.visualstudio.com/).
2. On Windows, use the official installer (not a Store stub).
3. Enable **Add to PATH** during setup.

## 2. Install the ESP-IDF extension

1. Open VS Code.
2. Extensions view: `Ctrl+Shift+X` (macOS: `Shift+Command+X`).
3. Search for **ESP-IDF** by **Espressif Systems**.
4. Click **Install**.

Marketplace: [ESP-IDF for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)

## 3. Install ESP-IDF with EIM (recommended)

**EIM** (ESP-IDF Installation Manager) is Espressif’s official installer. The VS Code extension (v2+) uses it to install and discover IDF versions, so you do not have to set `IDF_PATH` by hand.

### 3.1 Launch EIM from VS Code

1. Command Palette: `Ctrl+Shift+P` (`Shift+Command+P` on macOS).
2. Run **ESP-IDF: Open ESP-IDF Installation Manager**.
3. On a desktop this opens the EIM GUI. In SSH / WSL / headless sessions it uses a terminal wizard.

You can also install EIM separately:

- Download: [https://dl.espressif.com/dl/eim/](https://dl.espressif.com/dl/eim/)
- GitHub: [https://github.com/espressif/idf-im-ui/releases](https://github.com/espressif/idf-im-ui/releases)
- Windows: `winget install Espressif.EIM`
- Docs: [EIM documentation](https://docs.espressif.com/projects/idf-im-ui/en/latest/)

### 3.2 Install 5.5.x inside EIM

1. Click **Start Installation**.
2. Choose **Easy Installation**.
3. Mirror: in mainland China pick Espressif’s download servers; GitHub often times out.
4. Select **ESP-IDF 5.5.x** (this BSP needs `idf >= 5.5`). Do not use 4.x.
5. Wait for the toolchain, Python, and debug tools (often 10+ minutes).
6. Confirm the version on the Dashboard, then close EIM.

Default install roots:

- Windows: `C:\Espressif\`, manifest `C:\Espressif\tools\eim_idf.json`
- macOS / Linux: `~/.espressif/tools/eim_idf.json`

### 3.3 Point VS Code at that IDF

1. Command Palette: **ESP-IDF: Select Current ESP-IDF Version**.
2. Pick the **5.5.x** install.
3. Optional: **ESP-IDF: Doctor Command** — there should be no red errors.

## 4. USB serial

1. Connect the board with a data-capable USB cable.
2. Windows Device Manager should show a COM port (CH340 / CP210x / FTDI, e.g. `COM3`).
3. If no port appears, install the USB-UART driver printed on the board.
4. Close other serial monitors if the port is busy.

## 5. Open an example, build, flash

**Open the example folder itself**, not the parent repository. Example for the I2C scan:

`File` → `Open Folder` → `examples/01_i2c_scan`

Then:

1. **ESP-IDF: Set Espressif Device Target** → **esp32s3**.
2. Pick the serial port on the status bar.
3. **ESP-IDF: Build your project**. The first build downloads `managed_components/` (including `viewesmart/bsp_uedx48480040e_wb_a`) and needs internet access to the registry.
4. **ESP-IDF: Flash your project**.
5. **ESP-IDF: Monitor your device**. Leave the monitor with `Ctrl+]`.

### Slower Component Registry access

In an IDF-enabled terminal:

```powershell
$env:IDF_COMPONENT_REGISTRY_URL = "https://components-file.espressif.cn"
```

```bash
export IDF_COMPONENT_REGISTRY_URL=https://components-file.espressif.cn
```

## 6. Command line (optional)

From an ESP-IDF 5.5 terminal, `cd` into the **example directory**:

```bash
cd examples/01_i2c_scan
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

On Linux / macOS use `-p /dev/ttyACM0` or `/dev/ttyUSB0`.

## 7. Hardware shared by every example

| Item | Value |
|------|--------|
| Module | UEDX48480040E-WB-A, ESP32-S3 |
| Flash / PSRAM | 16 MB flash + octal PSRAM (`sdkconfig.defaults` matches this) |
| Panel | 480×480, GC9503, RGB565 |
| Touch | FT5x06, I2C `0x38`, SDA=GPIO40, SCL=GPIO41 |
| Backlight | GPIO38 |
| SD | SDSPI; CS (GPIO47) is shared with LCD 3-wire SDA — start the panel before mounting |
| Card format | FAT32; album JPEGs go in `photos` (`/sdcard/photos` after mount) |

## 8. Setup troubleshooting

| Symptom | What to try |
|---------|-------------|
| Extension cannot find IDF | Re-run EIM; **Select Current ESP-IDF Version**; custom installs need `idf.eimIdfJsonPath` |
| Missing components / download failed | Check the network; set `IDF_COMPONENT_REGISTRY_URL`; delete `managed_components` and `dependencies.lock`, rebuild |
| `idf.py` not found | Use the ESP-IDF terminal from the extension, not a plain shell |
| Black screen | Target must be esp32s3, octal PSRAM on, firmware is this example |
| Port busy / Permission denied | Close other monitors; on Linux add your user to `dialout` |
| GCC ICE | Rebuild with `IDF_CCACHE_ENABLE=0` |
| CMake object path > 250 characters (examples 05/06) | Move the example to a shorter path (e.g. `C:\work\05_album`) or enable Windows long paths |

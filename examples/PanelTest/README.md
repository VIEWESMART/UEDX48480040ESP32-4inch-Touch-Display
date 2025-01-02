# Panel Test Example(Arduino IDE)

The example demonstrates how to develop built-in or custom development boards using the `ESP_Panel` driver and tests by displaying color bars and printing touch point coordinates.

## How to Use
### Installing arduino-esp32
For installation of the esp32 in the Arduino IDE, refer to [How_To_Configure_Arduino-esp32](https://github.com/VIEWESMART/VIEWE-FAQ/blob/main/Arduino-FAQ/English/How_To_Configure_Arduino-esp32.md#install-esp32).

### Installing the Library
To use this example, please firstly install the following dependent libraries:

- ESP32_Display_Panel (> 0.2.1)
- ESP32_IO_Expander (>= 0.1.0 && < 0.2.0)

Follow the steps below to configure:

1. For **ESP32_Display_Panel**:

    `Note`:Since the latest version is still being updated and has not been released, please temporarily use the test version in this repository: `Libraries`->[`ESP32_Display_Panel`](https://github.com/VIEWESMART/UEDX24320028ESP32-3.5inch-320_480-Display/tree/main/Libraries/ESP32_Display_Panel).

     - Follow the [steps](https://github.com/VIEWESMART/VIEWE-FAQ/tree/main/Arduino-FAQ/English/How_To_Use.md#configuring-drivers) to configure drivers if needed.
    - If using a supported development board, follow the [steps](https://github.com/VIEWESMART/VIEWE-FAQ/tree/main/Arduino-FAQ/English/How_To_Use.md#using-supported-development-boards) to configure it.
    - If using a custom board, follow the [steps](https://github.com/VIEWESMART/VIEWE-FAQ/tree/main/Arduino-FAQ/English/How_To_Use.md#using-custom-development-boards) to configure it.

2. For **ESP32_IO_Expander**:
   - Just install it directly.

3. Navigate to the `Tools` menu in the Arduino IDE to choose a ESP board and configure its parameters.
    | Setting                               | Value                                 |
    | :-------------------------------: | :-------------------------------: |
    | Board                                 | ESP32S3 Dev Module           |
    | CPU Frequency                   | 240MHz (WiFi)                    |
    | Core Debug Level                | None                                 |
    | USB CDC On Boot                | Disabled                              |
    | USB DFU On Boot                | Disabled                             |
    | Events Run On                     | Core 1                               |  
    | Flash Mode                         | QIO 80MHz                         |
    | Flash Size                           | 16MB (128Mb)                    |
    | Arduino Runs On                  | Core 1                               |
    | USB Firmware MSC On Boot | Disabled                             |
    | Partition Scheme                | 16M Flash (3MB APP/9.9MB FATFS) |
    | PSRAM                                | OPI PSRAM                         |
    | Upload Mode                     |     UART0/Hardware CDC            |
    | Upload Speed                     | 921600                               |
    | USB Mode                           | Hardware CDC and JTAG     |
4. Verify and upload the example to your ESP board.
5. If burning fails Please hold down the "BOOT" button and try downloading the program again.

## Serial Output

```bash
...
Panel test example start
Initialize display panel
Turn off the backlight
Draw color bar from top to bottom, the order is B - G - R
Turn on the backlight
Panel test example end
Touch point(0): x 141, y 168, strength 47
Touch point(1): x 165, y 288, strength 45
Touch point(2): x 258, y 343, strength 33
Touch point(3): x 371, y 317, strength 24
...
```

## Troubleshooting

Please check the [FAQ](https://github.com/VIEWESMART/VIEWE-FAQ/tree/main/Arduino-FAQ/English/FAQ.md) first to see if the same question exists. If not, please create a [Github issue](https://github.com/VIEWESMART/VIEWE-FAQ/issues). We will get back to you as soon as possible.

# Start here — complete CYD weather-station source package

## What to upload to GitHub

Open this folder and upload **all its contents to the repository root**. The
README should be at the top level of the repository. Keep this structure:

```text
README.md
START_HERE.md
.gitignore
LICENSE
THIRD_PARTY_NOTICES.txt
Setup801_ESP32_CYD_ILI9341_240x320.h
Setup805_ESP32_CYD_ST7789_240x320.h
Esp32_CYD_TFT_eSPI_OpenWeather_LittleFS_v02/
    Esp32_CYD_TFT_eSPI_OpenWeather_LittleFS_v02.ino
    supporting .h and .cpp files
    MoonPhase.ino
    Secrets.example.h
    data/                    (all 66 files)
libraries/                   (all four ZIPs and manifest)
tools/                       (build/test helpers and bundled LittleFS uploader)
tests/
images/
```

The ZIP can also be attached to a GitHub Release as a convenient download.
The repository itself should contain the extracted files so people can browse
and clone the source. Keep `.gitignore`; it is included in this package.

This clean package contains placeholders. Do not add a personal `Secrets.h`,
compiled firmware, or private build folders to the public upload. Your existing
board firmware includes personal credentials, so it is not a public release
binary. Each builder fills their own `Secrets.h` locally.

## What a builder needs

- Hardware: ESP32-WROOM-32 CYD with ILI9341, XPT2046 touch, and 4 MB flash.
- Arduino IDE 2.2.1+ and **esp32 by Espressif Systems 3.2.0**, installed once.
- Their own 2.4 GHz Wi-Fi details and activated OpenWeather API key.

Arduino IDE and the ESP32 board/toolchain package are not bundled. Everything
specific to this project is included; no separate library or LittleFS-addon
search/download is required. Weather data still needs an internet connection.

## Short build/upload checklist

1. In Arduino IDE, install each ZIP from `libraries/` using **Sketch → Include
   Library → Add .ZIP Library**. The bundled TFT_eSPI already has the CYD pin setup.
   If TFT_eSPI is already installed, apply the supplied Setup801 header to its
   `User_Setup.h`, as explained in README. Library updates can reset that setting.
2. Copy the bundled LittleFS VSIX into the IDE's plugin folder and restart the
   IDE. Paths and steps are in `tools/littlefs-uploader/INSTALL.txt`.
3. Open the main `.ino` **inside its complete sketch folder**.
4. Copy `Secrets.example.h` to `Secrets.h` in that sketch folder; fill personal
   Wi-Fi and API settings. Location/timezone defaults are Bengaluru/IST.
5. Select **ESP32 Dev Module, DIO, 40 MHz, 4 MB, No OTA (2MB APP/2MB SPIFFS),
   PSRAM Disabled, Upload Speed 115200**, and the correct serial port.
6. Verify and upload the sketch.
7. Close Serial Monitor. Press **Ctrl+Shift+P** (macOS: Cmd+Shift+P), run
   **Upload LittleFS to Pico/ESP8266/ESP32**, and then reset the board.

Upload both the sketch and the assets with the same partition selection.
The firmware rejects generic TFT setups and the default partition scheme at
compile time. These checks catch the two settings that caused the blank screen.
See README.md for full setup, touch calibration, and troubleshooting details.

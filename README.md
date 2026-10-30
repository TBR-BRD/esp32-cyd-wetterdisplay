# esp32-cyd-wetterdisplay

My own weather display on an ESP32 "Cheap Yellow Display" (CYD): a 2.8-inch 320×240 touchscreen that shows current conditions and a forecast.

## Based on

This project is a rebuild of **[Rau7han/ESP32-CYD-Weather-Station](https://github.com/Rau7han/ESP32-CYD-Weather-Station)** (MIT License), used as the reference for hardware, firmware and enclosure. The reference point is commit [`8ff69cd`](https://github.com/Rau7han/ESP32-CYD-Weather-Station/tree/8ff69cd18980db0204cc178c4f7d10b8208bcfd9).

Upstream in turn adapts work by Michael Fehr, Bodmer and ThingPulse/Daniel Eichhorn; see its [credits](https://github.com/Rau7han/ESP32-CYD-Weather-Station#credits-and-license).

## Hardware

As specified by upstream:

| Part | Requirement |
| --- | --- |
| Board | ESP32-WROOM-32 CYD, 4 MB flash |
| Display | 2.8-inch 320×240 ILI9341 TFT |
| Touch | XPT2046 resistive |
| Wi-Fi | 2.4 GHz |
| Power | USB |

## Changes from upstream

- Location set to Falkensee with the Europe/Berlin time rules.
- Forecast days and on-screen labels use the configured time zone and city instead of hard-coded IST and Bengaluru values.
- Display text is German.
- Auto mode cycles only the three weather pages; System remains available by touch.
- `tools/flash.py` flashes firmware and assets without the Arduino IDE.
- Upstream's product images are not copied; see its README for screenshots.

## Setup

Requires [Arduino CLI](https://docs.arduino.cc/arduino-cli/installation/) and the ESP32 core:

```sh
brew install arduino-cli
arduino-cli core install esp32:esp32@3.2.0 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Copy `Esp32_CYD_TFT_eSPI_OpenWeather_LittleFS_v02/Secrets.example.h` to `Secrets.h` in the same folder and enter your Wi-Fi credentials and OpenWeather API key. Git ignores `Secrets.h`. Location and refresh intervals are in `All_Settings.h`.

Connect the board, then:

```sh
python3 tools/test.py   # optional host tests
python3 tools/flash.py  # build and flash firmware and assets
```

On Apple Silicon Macs, a newly connected board stays without a serial port until it is allowed under System Settings → Privacy & Security → Allow accessories to connect.

Touch controls and troubleshooting are described in the [upstream README](https://github.com/Rau7han/ESP32-CYD-Weather-Station#touch-controls).

## Status

See [STATUS.md](STATUS.md) and [DECISIONS.md](DECISIONS.md).

## License

MIT, see [LICENSE](LICENSE). Third-party notices are in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

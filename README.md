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

## Status

Firmware not imported yet. See [STATUS.md](STATUS.md) and [DECISIONS.md](DECISIONS.md).

# Status

## Done

- Repository created with a reference to upstream.
- Upstream firmware imported at commit `8ff69cd`.
- Location set to Falkensee with the Europe/Berlin time rules; hard-coded IST logic removed.
- Host tests pass and the firmware compiles for ESP32 core 3.2.0.
- `tools/flash.py` added and used to flash firmware and assets to the board (ESP32-D0WD-V3, CH340 USB chip).
- Serial log confirms on the device: 66 assets without errors, Wi-Fi connected, OpenWeather current and forecast and Open-Meteo UV all HTTP 200, touch initialized and page change by swipe.

- Display text is German, including weather descriptions, weekdays, wind directions and decimal commas. All four pages were checked from screen dumps taken over serial.
- Wind speed is shown in whole km/h.
- Auto mode cycles only Start (20 seconds), Vorhersage and Aktuell (6.5 seconds each). System stays reachable by touch and returns to Start after 30 seconds. An eight-minute test with Auto on showed only those three pages and 48 of 48 successful weather requests.

## Next

- Calibrate touch if taps land in the wrong place (hold any page for two seconds).

## Open issues

- OpenWeather requests failed with HTTP client code -1 for about four minutes on one boot, while Open-Meteo kept working. It did not recur in 66 further requests and the cause is unknown. Auto mode alone does not trigger it: the later requests ran with Auto on and off. The firmware keeps old data and retries after 60 seconds.
- Temperatures show whole degrees because the 36-point font has no comma.

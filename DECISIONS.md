# Decisions

## New repository instead of a fork

The project gets its own repository that references upstream by link, because it is meant to become my own display rather than a contribution back. Upstream's MIT license notice and third-party notices must be kept when its code is imported.

## Private for now

The repository starts private and can be made public later.

## OpenWeather as in upstream

Current conditions and the forecast come from OpenWeather and the UV index from Open-Meteo, as upstream does, to keep the rebuild close to the original.

## Local days from the configured time zone

Upstream computes forecast days with a fixed IST offset, so changing `TIMEZONE_POSIX` alone would misgroup the forecast. `localDayNumber()` now derives the date from the configured time zone, which also handles daylight saving time.

## German display text

The 15-point font has no umlauts, so it only carries text without them, and the built-in 5x7 font renders umlauts from CP437 bytes. Weather descriptions come from OpenWeather with `lang=de`, but its `main` field stays English, so the short Home label comes from `conditionLabel()`. Wind asset file names stay English and only the displayed direction is translated. Serial log messages other than the weather status stay English.

## Auto mode skips System

Auto cycles the three weather pages, showing Start for 20 seconds and the other two for 6.5 seconds each. With Auto on, the System page stays open for 30 seconds after the last touch and then returns to Start, so settings can be changed without being interrupted and the device cannot get stuck there.

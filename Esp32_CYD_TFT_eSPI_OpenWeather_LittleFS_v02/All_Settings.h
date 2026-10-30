#pragma once
#include <Arduino.h>

// Personal credentials belong in Secrets.h, which Git ignores.
// The example values allow verification builds without exposing real credentials.
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#include "Secrets.example.h"
#endif

inline constexpr char CITY_NAME[] = "Falkensee";
inline constexpr char LATITUDE[] = "52.56014";
inline constexpr char LONGITUDE[] = "13.0927";
inline constexpr char TIMEZONE_POSIX[] = "MEZ-1MESZ,M3.5.0,M10.5.0/3";
inline constexpr uint32_t UPDATE_INTERVAL_MS = 15UL * 60UL * 1000UL;
inline constexpr uint32_t RETRY_INTERVAL_MS = 60UL * 1000UL;
inline constexpr uint32_t AUTO_PAGE_INTERVAL_MS = 6500;
inline constexpr uint32_t HOME_PAGE_INTERVAL_MS = 20000;
// With Auto on, the System page stays open this long after the last touch, then returns to Home.
inline constexpr uint32_t SYSTEM_PAGE_TIMEOUT_MS = 30000;
inline constexpr uint8_t DISPLAY_ROTATION = 1;
inline constexpr uint8_t TOUCH_ROTATION = 1;
inline constexpr int TOUCH_CS_PIN = 33;
inline constexpr int TOUCH_IRQ_PIN = 36;
inline constexpr int TOUCH_CLK_PIN = 25;
inline constexpr int TOUCH_MISO_PIN = 39;
inline constexpr int TOUCH_MOSI_PIN = 32;

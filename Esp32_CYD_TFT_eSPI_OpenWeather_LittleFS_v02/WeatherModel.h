#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>
// Auto mode cycles through the first AUTO_PAGE_COUNT pages; the last page (System) is manual only.
constexpr int DISPLAY_WIDTH = 320, DISPLAY_HEIGHT = 240, PAGE_COUNT = 4, AUTO_PAGE_COUNT = 3;
struct WeatherPoint {
  time_t timestamp = 0;
  float temperature = 0, feelsLike = 0, low = 0, high = 0;
  float wind = 0, rain = 0, precipitationProbability = 0;
  int humidity = 0, pressure = 0, clouds = 0, windDegrees = 0, visibility = 0;
  uint16_t conditionId = 0;
  char icon[4] = "", condition[24] = "", description[64] = "";
};
struct CurrentWeather {
  bool valid = false;
  WeatherPoint point;
  time_t sunrise = 0, sunset = 0, fetched = 0;
};
struct DayForecast {
  bool valid = false;
  int32_t localDay = 0;
  int samples = 0;
  float low = 0, high = 0, rain = 0, probability = 0;
  WeatherPoint representative;
};
struct WeatherState {
  CurrentWeather current;
  DayForecast days[4];
  time_t forecastFetched = 0, uvFetched = 0, uvTimestamp = 0;
  float uv = 0;
  bool uvValid = false;
  int currentHttp = 0, forecastHttp = 0, uvHttp = 0;
  uint8_t activeKey = 1;
  char status[96] = "Verbinde mit WLAN";
  uint32_t generation = 0;
};
// Days since 1970-01-01 of the calendar date in the configured time zone.
int32_t localDayNumber(time_t utc);
void buildDailyForecast(const WeatherPoint* points, size_t count, time_t nowUtc, DayForecast (&days)[4]);
const char* weatherIconName(uint16_t conditionId, const char* apiIcon);
const char* conditionLabel(uint16_t conditionId);
const char* windDirection(int degrees);
const char* windDirectionLabel(int degrees);
// Converts UTF-8 to the CP437 bytes of the built-in 5x7 font. Unsupported characters become "?".
void utf8ToCp437(const char* input, char* output, size_t outputSize);
int wrapPage(int page);
int nextAutoPage(int page);
uint32_t secondsUntil(uint32_t nowMs, uint32_t deadlineMs);
int pageForGesture(int current, int startX, int startY, int endX, int endY);
struct TouchCalibration {
  float xx = 319.0f / 3600, xy = 0, xc = -200 * 319.0f / 3600;
  float yx = 0, yy = 239.0f / 3600, yc = -200 * 239.0f / 3600;
};
bool fitTouchCalibration(const int raw[4][2], TouchCalibration& result);
void mapTouch(const TouchCalibration& calibration, int rawX, int rawY, int& x, int& y);

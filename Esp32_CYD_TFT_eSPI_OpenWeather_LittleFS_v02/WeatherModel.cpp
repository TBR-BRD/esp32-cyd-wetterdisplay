#include "WeatherModel.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
int32_t localDayNumber(time_t utc) {
  struct tm local; localtime_r(&utc, &local);
  const int32_t y = local.tm_year + 1899;
  return 365 * y + y / 4 - y / 100 + y / 400 - 719162 + local.tm_yday;
}
static int secondsFromLocalNoon(time_t utc) {
  struct tm local; localtime_r(&utc, &local);
  return abs(local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec - 43200);
}
void buildDailyForecast(const WeatherPoint* points, size_t count, time_t nowUtc, DayForecast (&days)[4]) {
  const int32_t today = localDayNumber(nowUtc);
  for (int i = 0; i < 4; ++i) { days[i] = DayForecast{}; days[i].localDay = today + 1 + i; }
  for (size_t i = 0; i < count; ++i) {
    const WeatherPoint& p = points[i];
    const int index = localDayNumber(p.timestamp) - today - 1;
    if (index < 0 || index > 3) continue;
    DayForecast& day = days[index];
    const int distance = secondsFromLocalNoon(p.timestamp);
    const int oldDistance = secondsFromLocalNoon(day.representative.timestamp);
    if (!day.valid || distance < oldDistance) day.representative = p;
    if (!day.valid || p.low < day.low) day.low = p.low;
    if (!day.valid || p.high > day.high) day.high = p.high;
    day.rain += p.rain;
    if (p.precipitationProbability > day.probability) day.probability = p.precipitationProbability;
    ++day.samples;
    day.valid = true;
  }
}
const char* weatherIconName(uint16_t id, const char* icon) {
  const bool night = icon && strlen(icon) >= 3 && icon[2] == 'n';
  if (id >= 200 && id < 300) return "thunderstorm";
  if (id >= 300 && id < 400) return "drizzle";
  if (id == 500) return "lightRain";
  if (id == 511 || (id >= 611 && id <= 616)) return "sleet";
  if (id >= 500 && id < 600) return "rain";
  if (id >= 600 && id < 700) return "snow";
  if (id >= 700 && id < 800) return "fog";
  if (id == 800) return night ? "clear-night" : "clear-day";
  if (id == 801) return night ? "partly-cloudy-night" : "partly-cloudy-day";
  if (id >= 802 && id <= 804) return "cloudy";
  return "unknown";
}
const char* conditionLabel(uint16_t id) {
  if (id >= 200 && id < 300) return "Gewitter";
  if (id >= 300 && id < 400) return "Nieselregen";
  if (id == 500) return "Leichter Regen";
  if (id == 511 || (id >= 611 && id <= 616)) return "Schneeregen";
  if (id >= 500 && id < 600) return "Regen";
  if (id >= 600 && id < 700) return "Schnee";
  if (id >= 700 && id < 800) return "Nebel";
  if (id == 800) return "Klar";
  if (id == 801) return "Leicht bewölkt";
  if (id >= 802 && id <= 804) return "Bewölkt";
  return "Unbekannt";
}
static int directionIndex(int degrees) {
  const int normalized = ((degrees % 360) + 360) % 360;
  return ((normalized + 22) / 45) % 8;
}
// These names match the /wind/ asset files.
const char* windDirection(int degrees) {
  static const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return directions[directionIndex(degrees)];
}
const char* windDirectionLabel(int degrees) {
  static const char* directions[] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};
  return directions[directionIndex(degrees)];
}
void utf8ToCp437(const char* input, char* output, size_t outputSize) {
  size_t length = 0;
  for (const unsigned char* c = reinterpret_cast<const unsigned char*>(input); *c && length + 1 < outputSize; ++length) {
    if (*c < 0x80) { output[length] = *c++; continue; }
    char mapped = '?';
    if (c[0] == 0xC2 && c[1] == 0xB0) mapped = 0xF8;
    if (c[0] == 0xC3) switch (c[1]) {
      case 0xA4: mapped = 0x84; break;
      case 0xB6: mapped = 0x94; break;
      case 0xBC: mapped = 0x81; break;
      case 0x84: mapped = 0x8E; break;
      case 0x96: mapped = 0x99; break;
      case 0x9C: mapped = 0x9A; break;
      case 0x9F: mapped = 0xE1; break;
    }
    output[length] = mapped;
    ++c;
    while ((*c & 0xC0) == 0x80) ++c;
  }
  output[length] = '\0';
}
int wrapPage(int page) { return (page % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT; }
int nextAutoPage(int page) { return page + 1 < AUTO_PAGE_COUNT ? page + 1 : 0; }
uint32_t secondsUntil(uint32_t nowMs, uint32_t deadlineMs) {
  const int32_t remaining = static_cast<int32_t>(deadlineMs - nowMs);
  return remaining > 0 ? (static_cast<uint32_t>(remaining) + 999) / 1000 : 0;
}
int pageForGesture(int current, int x0, int y0, int x1, int y1) {
  const int dx = x1 - x0, dy = y1 - y0;
  if (abs(dx) >= 40 && abs(dx) > abs(dy)) return wrapPage(current + (dx < 0 ? 1 : -1));
  if (abs(dx) > 20 || abs(dy) > 20) return current;
  if (y0 >= 222 && y1 >= 222) return x1 / 80 < PAGE_COUNT ? x1 / 80 : current;
  return wrapPage(current + (x1 < 80 ? -1 : 1));
}
bool fitTouchCalibration(const int raw[4][2], TouchCalibration& r) {
  const float ax = raw[1][0] - raw[0][0], ay = raw[1][1] - raw[0][1];
  const float bx = raw[3][0] - raw[0][0], by = raw[3][1] - raw[0][1];
  const float determinant = ax * by - ay * bx;
  if (fabsf(determinant) < 100000) return false;
  r.xx = 280 * by / determinant; r.xy = -280 * bx / determinant;
  r.xc = 20 - r.xx * raw[0][0] - r.xy * raw[0][1];
  r.yx = -180 * ay / determinant; r.yy = 180 * ax / determinant;
  r.yc = 20 - r.yx * raw[0][0] - r.yy * raw[0][1];
  const float x = r.xx * raw[2][0] + r.xy * raw[2][1] + r.xc;
  const float y = r.yx * raw[2][0] + r.yy * raw[2][1] + r.yc;
  return fabsf(x - 300) < 20 && fabsf(y - 200) < 20;
}
void mapTouch(const TouchCalibration& c, int rx, int ry, int& x, int& y) {
  x = lroundf(c.xx * rx + c.xy * ry + c.xc); y = lroundf(c.yx * rx + c.yy * ry + c.yc);
  if (x < 0) x = 0;
  if (x >= DISPLAY_WIDTH) x = DISPLAY_WIDTH - 1;
  if (y < 0) y = 0;
  if (y >= DISPLAY_HEIGHT) y = DISPLAY_HEIGHT - 1;
}

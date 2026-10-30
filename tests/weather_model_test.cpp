#include "WeatherModel.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
  // 2026-10-24 is still CEST, so the local date changes 2 hours before UTC.
  const time_t day = 20750;
  const time_t midnightLocal = day * 86400 - 2 * 3600;
  assert(localDayNumber(midnightLocal - 1) == day - 1);
  assert(localDayNumber(midnightLocal) == day);
  // 2026-01-01 is CET, so the same boundary is only 1 hour before UTC.
  const time_t winterDay = 20454;
  assert(localDayNumber(winterDay * 86400 - 3600 - 1) == winterDay - 1);
  assert(localDayNumber(winterDay * 86400 - 3600) == winterDay);
  WeatherPoint points[7];
  points[0].timestamp = midnightLocal + 10 * 3600; // Today must not enter tomorrow's row.
  points[0].low = -99; points[0].high = 99;
  // Tomorrow is the 25-hour day when CEST ends: 13 elapsed hours are 12:00 CET.
  const int hours[] = {1, 7, 10, 13, 19, 22};
  for (int i = 1; i < 7; ++i) {
    points[i].timestamp = midnightLocal + 86400 + hours[i - 1] * 3600;
    points[i].low = 15 + i; points[i].high = 20 + i; points[i].rain = 0.5;
    points[i].precipitationProbability = i / 10.0f;
  }
  DayForecast days[4]; buildDailyForecast(points, 7, midnightLocal + 20 * 3600, days);
  assert(days[0].valid && days[0].samples == 6);
  assert(days[0].low == 16 && days[0].high == 26 && days[0].rain == 3.0f);
  assert(std::fabs(days[0].probability - 0.6) < 0.001);
  assert(days[0].representative.timestamp == midnightLocal + 86400 + 13 * 3600);
  assert(!days[1].valid && !days[2].valid && !days[3].valid);
  // No hard-coded offset of N array entries: missing slots cannot shift the day.
  points[0].timestamp = midnightLocal + 3 * 86400 + 12 * 3600;
  buildDailyForecast(points, 1, midnightLocal, days);
  assert(!days[0].valid && !days[1].valid && days[2].valid && !days[3].valid);
  assert(std::strcmp(weatherIconName(800, "01n"), "clear-night") == 0);
  assert(std::strcmp(weatherIconName(801, "02d"), "partly-cloudy-day") == 0);
  assert(std::strcmp(weatherIconName(500, "10d"), "lightRain") == 0);
  assert(std::strcmp(weatherIconName(511, "13d"), "sleet") == 0);
  assert(std::strcmp(weatherIconName(200, "11d"), "thunderstorm") == 0);
  assert(std::strcmp(weatherIconName(0, ""), "unknown") == 0);
  assert(std::strcmp(conditionLabel(803), "Bewölkt") == 0);
  assert(std::strcmp(conditionLabel(0), "Unbekannt") == 0);
  assert(std::strcmp(windDirection(-45), "NW") == 0);
  assert(std::strcmp(windDirection(90), "E") == 0 && std::strcmp(windDirectionLabel(90), "O") == 0);
  assert(std::strcmp(windDirection(45), "NE") == 0 && std::strcmp(windDirectionLabel(45), "NO") == 0);
  char cp437[32];
  utf8ToCp437("Mäßig Über öü€", cp437, sizeof(cp437));
  assert(std::strcmp(cp437, "M\x84\xE1ig \x9A" "ber \x94\x81?") == 0);
  utf8ToCp437("Regen", cp437, 4);
  assert(std::strcmp(cp437, "Reg") == 0);
  assert(std::strcmp(windDirection(360), "N") == 0);
  assert(pageForGesture(0, 250, 120, 30, 120) == 1);
  assert(pageForGesture(0, 30, 120, 250, 120) == 3);
  assert(pageForGesture(0, 285, 231, 285, 231) == 3);
  assert(pageForGesture(2, 20, 100, 20, 100) == 1);
  assert(pageForGesture(2, 150, 10, 151, 100) == 2); // Vertical movement is not a tap.
  assert(wrapPage(-1) == 3 && wrapPage(4) == 0);
  assert(nextAutoPage(0) == 1 && nextAutoPage(1) == 2);
  assert(nextAutoPage(2) == 0); // The loop skips System.
  assert(nextAutoPage(3) == 0); // Leaving System returns to Home.
  assert(secondsUntil(1000, 2500) == 2);
  assert(secondsUntil(2500, 2500) == 0);
  assert(secondsUntil(3000, 2500) == 0);
  assert(secondsUntil(UINT32_MAX - 499, 500) == 1);
  const int regular[4][2] = {{300,400},{3700,400},{3700,3600},{300,3600}};
  const int rotated[4][2] = {{3600,300},{3600,3700},{400,3700},{400,300}};
  const int bad[4][2] = {{100,100},{100,100},{100,100},{100,100}};
  TouchCalibration c;
  for (auto raw : {regular, rotated}) {
    assert(fitTouchCalibration(raw, c));
    int x, y; mapTouch(c, (raw[0][0]+raw[2][0])/2, (raw[0][1]+raw[2][1])/2, x, y);
    assert(std::abs(x-160) <= 1 && std::abs(y-110) <= 1);
  }
  assert(!fitTouchCalibration(bad, c));
  int outsideX, outsideY;
  mapTouch(TouchCalibration{}, -999, 9999, outsideX, outsideY);
  assert(outsideX == 0 && outsideY == 239);
  std::cout << "PASS: CET/CEST date rollover, daily aggregation, icons, German labels, CP437, navigation, auto cycle, affine calibration\n";
}

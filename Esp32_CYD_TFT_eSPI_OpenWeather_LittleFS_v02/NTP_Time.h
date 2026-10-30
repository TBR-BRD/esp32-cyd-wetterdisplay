#pragma once
#include <Arduino.h>
#include <time.h>
#include "All_Settings.h"
inline bool clockValid() { return time(nullptr) > 1700000000; }
inline void initClock() {
  // POSIX sign is reversed: MEZ-1 means UTC+01:00. MESZ applies from the last Sunday in March to the last Sunday in October.
  configTzTime(TIMEZONE_POSIX, "time.google.com", "pool.ntp.org", "time.cloudflare.com");
}
inline const char* localWeekday(time_t utc) {
  static const char* names[] = {"SO", "MO", "DI", "MI", "DO", "FR", "SA"};
  struct tm local; localtime_r(&utc, &local);
  return names[local.tm_wday];
}
inline String localTimeText(time_t utc, const char* format = "%H:%M") {
  if (utc <= 0) return "--:--";
  struct tm local; localtime_r(&utc, &local);
  char output[40]; strftime(output, sizeof(output), format, &local);
  return String(output);
}

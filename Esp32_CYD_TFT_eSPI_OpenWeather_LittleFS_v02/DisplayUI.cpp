/* Landscape adaptation of the original Daniel Eichhorn / ThingPulse and Bodmer
 * weather-station layout. MIT attribution is retained in THIRD_PARTY_NOTICES.txt.
 * Page geometry follows the user's 320x240 HTML reference. */
#include "DisplayUI.h"
#include "All_Settings.h"
#include "NTP_Time.h"
#include "WeatherNetwork.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <math.h>

TFT_eSPI tft;
namespace {
TFT_eSprite screen(&tft);
constexpr uint16_t BG = TFT_BLACK;
constexpr uint16_t WHITE = 0xF7BF, MUTED = 0xADFA, YELLOW = 0xF727;
constexpr uint16_t CYAN = 0x667F, LINE = 0x220A, GREEN = 0x47B0, ORANGE = 0xFDC9;
int selectedFont = 0, fileCount = 0, badFiles = 0, clippingWarnings = 0;
const char* pageNames[] = {"Start", "Vorhersage", "Aktuell", "System"};
void font(int id) {
  if (selectedFont == id) return;
  screen.unloadFont(); screen.setTextSize(1);
  // The built-in font is drawn from raw CP437 bytes; the loaded fonts decode UTF-8.
  screen.setAttribute(UTF8_SWITCH, id == 2 || id == 3);
  if (id == 2) screen.loadFont("fonts/NSBold15", LittleFS);
  else if (id == 3) screen.loadFont("fonts/NSBold36", LittleFS);
  else screen.setTextFont(id == 4 ? 4 : 1);
  selectedFont = id;
}
void text(String value, int x, int y, int width, uint16_t color = WHITE, int fontId = 1, int align = 0) {
  font(fontId);
  if (fontId == 1) { char converted[128]; utf8ToCp437(value.c_str(), converted, sizeof(converted)); value = converted; }
  if (x < 0 || x + width > 320 || y < 0 || y + screen.fontHeight() > 240) ++clippingWarnings;
  if (screen.textWidth(value) > width) {
    while (value.length() && screen.textWidth(value + "..") > width) value.remove(value.length() - 1);
    value += "..";
  }
  int start = x;
  if (align == 1) start += (width - screen.textWidth(value)) / 2;
  if (align == 2) start += width - screen.textWidth(value);
  screen.setTextDatum(TL_DATUM); screen.setTextPadding(0); screen.setTextColor(color, BG);
  screen.drawString(value, start, y);
}
void divider(int y) { screen.drawFastHLine(10, y, 300, LINE); }
String number(float n, int precision = 0) {
  // Rounding through lroundf avoids showing "-0".
  String result = precision ? String(n, precision) : String(lroundf(n));
  result.replace('.', ',');
  return result;
}
// OpenWeather reports wind in m/s.
String windKmh(float metersPerSecond) { return number(metersPerSecond * 3.6f); }
void temperatureText(float value, bool valid, int x, int y, int width, uint16_t color, bool right = false) {
  // The bundled 36-point font has digits and the degree sign, but no comma or C glyph, so this shows whole degrees.
  const String large = (valid ? number(value) : String("--")) + "°";
  font(3); const int largeWidth = screen.textWidth(large);
  font(2); const int unitWidth = screen.textWidth("C");
  const int total = largeWidth + unitWidth + 2;
  const int start = x + (right ? width - total : (width - total) / 2);
  text(large, start, y, largeWidth, color, 3);
  text("C", start + largeWidth + 2, y + 17, unitWidth, color, 2);
}
uint16_t read16(File& f) { uint8_t b[2]; if (f.read(b, 2) != 2) return 0; return b[0] | b[1] << 8; }
uint32_t read32(File& f) { uint8_t b[4]; if (f.read(b, 4) != 4) return 0; return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; }
bool bitmap(const String& path, int x, int y, int size) {
  if (x < 0 || y < 0 || x + size > 320 || y + size > 240) { ++clippingWarnings; return false; }
  File file = LittleFS.open(path, "r");
  if (!file || read16(file) != 0x4d42) return false;
  read32(file); read32(file); const uint32_t offset = read32(file); read32(file);
  const int32_t width = read32(file), height = read32(file);
  if (read16(file) != 1 || read16(file) != 24 || read32(file) != 0 || width <= 0 || height <= 0 || width > 120 || height > 120) return false;
  const int stride = (width * 3 + 3) & ~3;
  uint8_t row[360]; int previousRow = -1;
  for (int dy = 0; dy < size; ++dy) {
    const int sourceY = dy * height / size;
    if (sourceY != previousRow) {
      file.seek(offset + (height - 1 - sourceY) * stride);
      if (file.read(row, stride) != stride) return false;
      previousRow = sourceY;
    }
    for (int dx = 0; dx < size; ++dx) {
      const uint8_t* p = row + (dx * width / size) * 3;
      const uint16_t color = (p[2] & 0xF8) << 8 | (p[1] & 0xFC) << 3 | p[0] >> 3;
      if (color) screen.drawPixel(x + dx, y + dy, color);
    }
  }
  return true;
}
void weatherIcon(const WeatherPoint& point, int x, int y, int size, bool large = false) {
  const String path = String(large ? "/icon/" : "/icon50/") + weatherIconName(point.conditionId, point.icon) + ".bmp";
  if (!bitmap(path, x, y, size)) bitmap("/icon50/unknown.bmp", x, y, size);
}
String uvText(const WeatherState& s) {
  return s.uvValid && time(nullptr) - s.uvTimestamp < 7200 ? number(s.uv, 1) : "--";
}
uint16_t uvColor(const WeatherState& s) { return s.uv >= 8 ? ORANGE : s.uv >= 6 ? YELLOW : GREEN; }
bool stale(const WeatherState& s) { return s.current.valid && (time(nullptr) - s.current.fetched > 1800 || s.currentHttp != 200); }
void header(const char* title) { text(title, 12, 5, 296, WHITE, 2, 1); divider(23); }
void footer(int page, bool refreshing) {
  divider(223);
  for (int i = 0; i < PAGE_COUNT; ++i) {
    const int x = i * 80;
    if (i == page) screen.drawFastHLine(x + 10, 223, 60, CYAN);
    text(pageNames[i], x + 3, 230, 74, i == page ? CYAN : MUTED, 1, 1);
    screen.fillCircle(x + 40, 227, 1, i == page ? CYAN : LINE);
  }
  if (refreshing) screen.fillCircle(314, 227, 2, YELLOW);
}
struct MoonInfo { int bitmap = 0, phase = 0, illumination = 0; };
}
uint8_t moon_phase(int year, int month, int day, double hour, int* phase);
double Julian(int year, int month, double day);
double sun_position(double j);
double moon_position(double j, double longitude);
namespace {
MoonInfo moonInfo() {
  MoonInfo info; if (!clockValid()) return info;
  const time_t stamp = time(nullptr); struct tm utc; gmtime_r(&stamp, &utc);
  const double h = utc.tm_hour + utc.tm_min / 60.0;
  info.bitmap = moon_phase(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, h, &info.phase);
  const double j = Julian(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday + h / 24.0) - 2444238.5;
  const double sun = sun_position(j), elongation = moon_position(j, sun) - sun;
  info.illumination = lround(50 * (1 - cos(elongation * 3.141592653589793 / 180)));
  return info;
}
void home(const WeatherState& s, bool refreshing) {
  const CurrentWeather& current = s.current; const WeatherPoint& p = current.point;
  const String date = clockValid() ? String(localWeekday(time(nullptr))) + " " + localTimeText(time(nullptr), "%d.%m.%Y") : "Zeitabgleich...";
  text(date, 10, 6, 126, WHITE);
  text(refreshing ? "Aktualisiere..." : current.valid ? String(stale(s) ? "Veraltet " : "Stand ") + localTimeText(current.fetched, "%d.%m. %H:%M") : "Warte auf Wetterdaten", 143, 6, 167, stale(s) ? ORANGE : CYAN, 1, 2);
  divider(21);
  text(clockValid() ? localTimeText(time(nullptr)) : "--:--", 10, 31, 120, YELLOW, 3);
  temperatureText(p.temperature, current.valid, 205, 31, 105, YELLOW, true);
  weatherIcon(p, 135, 27, 56, true);
  text(current.valid ? conditionLabel(p.conditionId) : "Verbinde...", 118, 80, 89, WHITE, 1, 1);
  text(CITY_NAME, 213, 73, 97, WHITE, 2, 2);
  text(current.valid ? number(p.pressure) + " hPa" : "-- hPa", 12, 73, 98, WHITE, 2);
  text("UV " + uvText(s), 224, 90, 86, uvColor(s), 1, 2);
  divider(101);
  for (int i = 0; i < 4; ++i) {
    const DayForecast& d = s.days[i]; const int x = 10 + i * 75;
    text(d.valid ? localWeekday(static_cast<time_t>(d.localDay) * 86400 + 43200) : "---", x, 107, 74, YELLOW, 1, 1);
    text(d.valid ? number(d.high) + " / " + number(d.low) + " C" : "-- / --", x, 120, 74, WHITE, 1, 1);
    weatherIcon(d.representative, x + 20, 134, 34);
    if (i < 3) screen.drawFastVLine(x + 74, 106, 63, LINE);
  }
  divider(173);
  const char* labels[] = {"AUFGANG", "UNTERGANG", "", "WOLKEN", "FEUCHTE"};
  const MoonInfo moon = moonInfo();
  for (int i = 0; i < 5; ++i) {
    const int x = 10 + i * 60;
    if (i) screen.drawFastVLine(x, 178, 40, LINE);
    if (i != 2) text(labels[i], x + 2, 180, 56, YELLOW, 1, 1);
    String value;
    if (i == 0) value = current.valid ? localTimeText(current.sunrise) : "--:--";
    if (i == 1) value = current.valid ? localTimeText(current.sunset) : "--:--";
    if (i == 3) value = current.valid ? String(p.clouds) + "%" : "--";
    if (i == 4) value = current.valid ? String(p.humidity) + "%" : "--";
    if (i != 2) text(value, x + 2, 195, 56, WHITE, 2, 1);
    else {
      if (clockValid()) bitmap("/moon/moonphase_L" + String(moon.bitmap) + ".bmp", x + 12, 175, 36);
      else screen.drawCircle(x + 30, 193, 15, LINE);
      text(clockValid() ? String(moon.illumination) + "%" : "--%", x + 2, 213, 56, WHITE, 1, 1);
    }
  }
}
void forecastPage(const WeatherState& s) {
  header("Vier-Tage-Vorhersage");
  for (int i = 0; i < 4; ++i) {
    const DayForecast& d = s.days[i]; const WeatherPoint& p = d.representative; const int y = 29 + i * 48;
    if (!d.valid) { text("Warte auf Vorhersagedaten", 12, y + 12, 296, MUTED, 2, 1); continue; }
    weatherIcon(p, 10, y + 3, 38);
    const String date = String(localWeekday(p.timestamp)) + " " + localTimeText(p.timestamp, "%d.%m.");
    text(date, 55, y, 80, YELLOW);
    text(number(d.high) + "/" + number(d.low) + " C", 136, y, 77, WHITE);
    text("Regen " + number(d.rain, 1) + " mm", 215, y, 95, CYAN, 1, 2);
    text(p.description, 55, y + 12, 250, WHITE);
    text("F " + String(p.humidity) + "%  D " + String(p.pressure) + " hPa", 55, y + 25, 157, CYAN);
    text("Wind " + windKmh(p.wind) + " " + windDirectionLabel(p.windDegrees), 215, y + 25, 95, WHITE, 1, 2);
    if (i < 3) divider(y + 42);
  }
  text("3-h-Werte / Tages-Min-Max / Wind km/h", 12, 214, 296, MUTED, 1, 1);
}
void conditionsPage(const WeatherState& s) {
  header("Aktuelles Wetter");
  const WeatherPoint& p = s.current.point; const bool valid = s.current.valid;
  text(CITY_NAME, 75, 29, 170, CYAN, 2, 1);
  weatherIcon(p, 15, 45, 40);
  temperatureText(p.temperature, valid, 60, 49, 200, WHITE);
  bitmap(String("/wind/") + windDirection(p.windDegrees) + ".bmp", 265, 45, 40);
  text(valid ? String(p.description) : s.status, 12, 89, 296, YELLOW, 1, 1);
  divider(102);
  const String labels[] = {"Windgeschwindigkeit", "Windrichtung", "Luftfeuchte", "Luftdruck", "Bewölkung", "Sichtweite", "Regen / letzte Std.", "UV / Stundenmodell"};
  const String values[] = {windKmh(p.wind) + " km/h", String(windDirectionLabel(p.windDegrees)) + " " + String(p.windDegrees) + "°", String(p.humidity) + "%", String(p.pressure) + " hPa", String(p.clouds) + "%", number(p.visibility / 1000.0, 1) + " km", number(p.rain, 1) + " mm", uvText(s)};
  screen.drawFastVLine(160, 109, 109, LINE);
  for (int i = 0; i < 8; ++i) {
    const int x = 12 + (i % 2) * 160, y = 110 + (i / 2) * 27;
    text(labels[i], x, y, 136, YELLOW);
    text(i == 7 ? values[i] : valid ? values[i] : "--", x, y + 10, 136, i == 7 ? uvColor(s) : WHITE, 2);
  }
}
String httpText(int code) { return code == 200 ? "OK" : code == 0 ? "Warte" : String(code); }
void systemPage(const WeatherState& s, bool autoCycle, bool refreshing) {
  header("Systeminformationen");
  text(String(CITY_NAME) + " Wetter", 12, 29, 296, WHITE, 2, 1);
  const String labels[] = {"Display", "Uhrzeit", "WLAN", "Wetter / Vorhersage", "UV (Open-Meteo)", "Dateien / Heap", "Messung / UV-Stunde", "Nächstes Update"};
  const uint32_t remaining = weatherNextRefreshSeconds();
  char countdown[24];
  snprintf(countdown, sizeof(countdown), "In %02lu:%02lu min", static_cast<unsigned long>(remaining / 60), static_cast<unsigned long>(remaining % 60));
  const String values[] = {"320 x 240 / Touch", clockValid() ? localTimeText(time(nullptr), "%H:%M:%S %Z") : "Zeitabgleich...", WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : "Offline", httpText(s.currentHttp) + " / " + httpText(s.forecastHttp) + " / Key " + String(s.activeKey), httpText(s.uvHttp) + " / CAMS", String(fileCount) + " Dateien / " + String(ESP.getFreeHeap() / 1024) + " KB", (s.current.valid ? localTimeText(s.current.point.timestamp) : "--:--") + " / " + localTimeText(s.uvTimestamp), refreshing ? "Läuft gerade" : remaining ? String(countdown) : "Startet..."};
  for (int i = 0; i < 8; ++i) {
    const int y = 54 + i * 14;
    text(labels[i], 12, y, 135, YELLOW);
    text(values[i], 147, y, 161, WHITE, 1, 2);
  }
  text(s.status, 12, 171, 296, stale(s) ? ORANGE : CYAN, 1, 1);
  const char* buttons[] = {refreshing ? "Läuft..." : "Aktualisieren", autoCycle ? "Auto: ein" : "Auto: aus", "Kalibrieren"};
  for (int i = 0; i < 3; ++i) {
    const int x = 10 + i * 102;
    screen.drawRoundRect(x, 190, 96, 26, 4, LINE);
    text(buttons[i], x + 2, 199, 92, i == 0 && refreshing ? MUTED : i == 1 && autoCycle ? GREEN : WHITE, 1, 1);
  }
}
bool jpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* pixels) {
  if (y >= 240) return false;
  tft.pushImage(x, y, w, h, pixels); return true;
}
void scanDirectory(const char* path) {
  File directory = LittleFS.open(path);
  if (!directory || !directory.isDirectory()) { ++badFiles; return; }
  for (File f = directory.openNextFile(); f; f = directory.openNextFile()) {
    if (f.isDirectory()) continue;
    ++fileCount;
    const String name = f.name();
    if (f.size() == 0) { ++badFiles; continue; }
    if (name.endsWith(".bmp")) {
      const bool header = read16(f) == 0x4d42; read32(f); read32(f); const uint32_t offset = read32(f); read32(f);
      const uint32_t w = read32(f), h = read32(f);
      const bool format = read16(f) == 1 && read16(f) == 24 && read32(f) == 0;
      const uint32_t bytes = offset + ((w * 3 + 3) & ~3U) * h;
      if (!header || !format || w == 0 || w > 120 || h == 0 || h > 120 || f.size() < bytes) ++badFiles;
    } else if (name.endsWith(".jpg")) {
      if (f.read() != 0xff || f.read() != 0xd8) ++badFiles;
    } else if (name.endsWith(".vlw")) {
      uint8_t first[4]; if (f.read(first, 4) != 4 || first[0] || first[1] || first[2] || first[3] == 0) ++badFiles;
    }
  }
}
}
bool beginDisplay() {
  tft.begin(); tft.setRotation(DISPLAY_ROTATION); tft.fillScreen(BG);
  // A 76.8 KB buffer keeps enough heap for TLS while allowing flicker-free pages.
  screen.setColorDepth(8);
  screen.setAttribute(CP437_SWITCH, true);
  return screen.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT) != nullptr;
}
void showBootSplash() {
  TJpgDec.setJpgScale(1); TJpgDec.setSwapBytes(true); TJpgDec.setCallback(jpgOutput);
  if (LittleFS.exists("/splash/OpenWeather.jpg")) TJpgDec.drawFsJpg(40, 36, "/splash/OpenWeather.jpg", LittleFS);
  tft.setTextColor(TFT_CYAN, BG); tft.setTextDatum(MC_DATUM); tft.drawString(String(CITY_NAME) + " / Touch", 160, 194, 2);
}
void auditAssets() {
  fileCount = badFiles = 0;
  for (const char* path : {"/icon", "/icon50", "/moon", "/wind", "/fonts", "/splash"}) scanDirectory(path);
  Serial.printf("ASSETS files=%d errors=%d\n", fileCount, badFiles);
}
void renderPage(const WeatherState& weather, int page, bool autoCycle, bool refreshing) {
  clippingWarnings = 0; screen.fillSprite(BG);
  switch (wrapPage(page)) {
    case 0: home(weather, refreshing); break;
    case 1: forecastPage(weather); break;
    case 2: conditionsPage(weather); break;
    case 3: systemPage(weather, autoCycle, refreshing); break;
  }
  footer(wrapPage(page), refreshing); screen.pushSprite(0, 0);
  if (clippingWarnings) Serial.printf("Display bounds errors: %d\n", clippingWarnings);
}
void renderCalibration(int step, bool failed) {
  screen.fillSprite(BG); header("Touch-Kalibrierung");
  text(failed ? "Noch einmal: jedes Ziel ruhig halten" : "Ziel gedrückt halten, dann loslassen", 12, 86, 296, WHITE, 1, 1);
  text("Punkt " + String(step + 1) + " von 4 / wird automatisch gespeichert", 12, 107, 296, CYAN, 1, 1);
  text("Lange drücken startet die Kalibrierung", 12, 128, 296, MUTED, 1, 1);
  const int xs[] = {20, 300, 300, 20}, ys[] = {20, 20, 200, 200};
  screen.drawCircle(xs[step], ys[step], 10, YELLOW);
  screen.drawFastHLine(xs[step] - 14, ys[step], 29, YELLOW);
  screen.drawFastVLine(xs[step], ys[step] - 14, 29, YELLOW);
  screen.pushSprite(0, 0);
}

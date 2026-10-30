/*
 * Falkensee CYD weather station, landscape 320x240.
 * Based on the Daniel Eichhorn / ThingPulse weather station and Bodmer's
 * TFT_eSPI / LittleFS adaptation. MIT notices: THIRD_PARTY_NOTICES.txt.
 * Layout adapted from weather_station_multipage_320x240_v3.html.
 */
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>
#include "All_Settings.h"
#include "WeatherModel.h"
#include "WeatherNetwork.h"
#include "NTP_Time.h"
#include "DisplayUI.h"

#ifndef ARDUINO_PARTITION_no_ota
#error "In Arduino IDE Tools > Partition Scheme select No OTA (2MB APP/2MB SPIFFS), then upload both the sketch and LittleFS data."
#endif

SPIClass touchSPI(VSPI);
// Poll pressure: IRQ gating is unreliable on some CYD boards.
XPT2046_Touchscreen touchPanel(TOUCH_CS_PIN, 255);
Preferences touchPreferences;
TouchCalibration touchCalibration;
WeatherState weather;
int page = 0;
bool autoCycle = false, dirty = true, wasRefreshing = false, touchReady = false;
uint32_t lastPageChange = 0, lastClockTick = 0;
int lastMinute = -1;

bool calibrating = false, calibrationWaitRelease = false, calibrationFailed = false;
int calibrationStep = 0, calibrationRaw[4][2] = {};
uint32_t calibrationStarted = 0, calibrationPressStart = 0;
long calibrationSumX = 0, calibrationSumY = 0;
int calibrationSamples = 0;
bool fingerDown = false;
int startX = 0, startY = 0, endX = 0, endY = 0;
uint32_t fingerStart = 0, lastFingerSeen = 0;

void setAutoCycle(bool enabled) {
  if (enabled != autoCycle) {
    autoCycle = enabled;
    touchPreferences.putBool("auto", autoCycle);
  }
  lastPageChange = millis(); dirty = true;
}
void changePage(int next) {
  page = wrapPage(next); dirty = true; lastPageChange = millis();
  Serial.printf("PAGE %d %s\n", page, page == 0 ? "Home" : page == 1 ? "Forecast" : page == 2 ? "Conditions" : "System");
}
void startCalibration() {
  calibrating = true; calibrationStep = 0; calibrationStarted = millis();
  calibrationWaitRelease = true; calibrationFailed = false;
  calibrationSamples = 0; calibrationPressStart = 0; fingerDown = false;
  renderCalibration(0); Serial.println("CALIBRATION started");
}
void handleGesture(int x0, int y0, int x1, int y1) {
  lastPageChange = millis();
  if (page == 3 && abs(x1 - x0) < 20 && abs(y1 - y0) < 20 && y1 >= 190 && y1 <= 216) {
    if (x1 >= 10 && x1 <= 106) { requestWeatherRefresh(); dirty = true; }
    else if (x1 >= 112 && x1 <= 208) setAutoCycle(!autoCycle);
    else if (x1 >= 214 && x1 <= 310) startCalibration();
    return;
  }
  changePage(pageForGesture(page, x0, y0, x1, y1));
}
void calibrateSample(bool pressed, const TS_Point& raw) {
  if (millis() - calibrationStarted > 90000) {
    calibrating = false; dirty = true; lastPageChange = millis(); Serial.println("CALIBRATION timed out"); return;
  }
  if (calibrationWaitRelease) {
    if (!pressed) {
      calibrationWaitRelease = false; calibrationSamples = 0; calibrationPressStart = 0;
      calibrationSumX = calibrationSumY = 0;
      if (calibrationStep == 4) {
        TouchCalibration result;
        if (fitTouchCalibration(calibrationRaw, result)) {
          touchCalibration = result;
          touchPreferences.putBytes("affine", &touchCalibration, sizeof(touchCalibration));
          calibrating = false; dirty = true; lastPageChange = millis(); Serial.println("CALIBRATION saved"); return;
        }
        calibrationStep = 0; calibrationFailed = true; Serial.println("CALIBRATION rejected; retry");
      }
      renderCalibration(calibrationStep, calibrationFailed);
    }
    return;
  }
  if (!pressed) {
    calibrationSamples = 0; calibrationPressStart = 0; calibrationSumX = calibrationSumY = 0; return;
  }
  if (!calibrationSamples) calibrationPressStart = millis();
  calibrationSumX += raw.x; calibrationSumY += raw.y; ++calibrationSamples;
  if (millis() - calibrationPressStart >= 350 && calibrationSamples >= 8) {
    calibrationRaw[calibrationStep][0] = calibrationSumX / calibrationSamples;
    calibrationRaw[calibrationStep][1] = calibrationSumY / calibrationSamples;
    Serial.printf("CALIBRATION point=%d raw=%d,%d\n", calibrationStep,
                  calibrationRaw[calibrationStep][0], calibrationRaw[calibrationStep][1]);
    ++calibrationStep; calibrationWaitRelease = true;
  }
}
void pollTouch() {
  if (!touchReady) return;
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 12) return;
  lastPoll = millis();
  TS_Point raw; bool pressed = touchPanel.touched();
  if (pressed) { raw = touchPanel.getPoint(); pressed = raw.z >= 200; }
  if (calibrating) { calibrateSample(pressed, raw); return; }
  if (pressed) {
    int x, y; mapTouch(touchCalibration, raw.x, raw.y, x, y);
    lastFingerSeen = millis(); endX = x; endY = y;
    if (!fingerDown) { fingerDown = true; fingerStart = millis(); startX = x; startY = y; }
    if (millis() - fingerStart > 2000 && abs(endX - startX) < 20 && abs(endY - startY) < 20) startCalibration();
  } else if (fingerDown && millis() - lastFingerSeen > 70) {
    fingerDown = false;
    if (millis() - fingerStart >= 35) {
      Serial.printf("TOUCH %d,%d -> %d,%d\n", startX, startY, endX, endY);
      handleGesture(startX, startY, endX, endY);
    }
  }
}
void setup() {
  Serial.begin(115200); delay(200);
  Serial.printf("\n%s CYD landscape v3.1 / 320x240 / touch\n", CITY_NAME);
  if (!beginDisplay()) { Serial.println("FATAL: screen buffer allocation failed"); while (true) delay(1000); }
  if (!LittleFS.begin(false)) {
    tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Erst LittleFS-Daten hochladen", 160, 120, 2);
    Serial.println("FATAL: LittleFS mount failed"); while (true) delay(1000);
  }
  Serial.printf("Display setup=%d backlight=%d size=%dx%d\n", USER_SETUP_ID, TFT_BL, tft.width(), tft.height());
  auditAssets(); showBootSplash();
  pinMode(TOUCH_CS_PIN, OUTPUT); digitalWrite(TOUCH_CS_PIN, HIGH); pinMode(TOUCH_IRQ_PIN, INPUT);
  touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  touchReady = touchPanel.begin(touchSPI); touchPanel.setRotation(TOUCH_ROTATION);
  touchPreferences.begin("cyd-touch-v3", false);
  autoCycle = touchPreferences.getBool("auto", false);
  if (touchPreferences.getBytesLength("affine") == sizeof(TouchCalibration))
    touchPreferences.getBytes("affine", &touchCalibration, sizeof(touchCalibration));
  Serial.printf("TOUCH ready=%d / own SPI bus / long press to calibrate\n", touchReady);
  WiFi.mode(WIFI_STA); WiFi.setSleep(false); WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); initClock(); startWeatherNetwork();
  delay(600); lastPageChange = millis(); dirty = true;
}
void loop() {
  pollTouch();
  WeatherState latest; copyWeatherState(latest);
  if (latest.generation != weather.generation) {
    weather = latest; dirty = true;
    Serial.printf("WEATHER current=%d forecast=%d uv=%d / %s / heap=%u\n",
                  weather.currentHttp, weather.forecastHttp, weather.uvHttp, weather.status, ESP.getFreeHeap());
  }
  const bool refreshing = weatherIsRefreshing();
  if (refreshing != wasRefreshing) { wasRefreshing = refreshing; dirty = true; }
  if (millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    const time_t stamp = time(nullptr); const int minute = (stamp / 60) % 60;
    if (page == 3 || (page == 0 && minute != lastMinute)) dirty = true;
    lastMinute = minute;
  }
  const uint32_t pageTime = page == 0 ? HOME_PAGE_INTERVAL_MS : page < AUTO_PAGE_COUNT ? AUTO_PAGE_INTERVAL_MS : SYSTEM_PAGE_TIMEOUT_MS;
  if (!calibrating && !fingerDown && autoCycle && millis() - lastPageChange >= pageTime) changePage(nextAutoPage(page));
  if (dirty && !calibrating) { renderPage(weather, page, autoCycle, refreshing); dirty = false; }
  delay(3);
}

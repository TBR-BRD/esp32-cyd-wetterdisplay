#include "WeatherNetwork.h"
#include "WeatherJson.h"
#include "All_Settings.h"
#include "NTP_Time.h"
#include "ApiCertificates.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <atomic>
#include <memory>
namespace {
WeatherState published;
SemaphoreHandle_t stateMutex = nullptr;
std::atomic<bool> requested{true}, refreshing{false};
std::atomic<uint32_t> nextFetch{0};
constexpr int HTTP_TIMEOUT_MS = 8000;
void publish(WeatherState& s) {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  s.generation = published.generation + 1; published = s;
  xSemaphoreGive(stateMutex);
}
int fetchJson(const String& url, JsonDocument& result, JsonDocument& filter) {
  if (WiFi.status() != WL_CONNECTED) return -10;
  if (!clockValid()) return -11;
  WiFiClientSecure tls; tls.setCACert(API_ROOT_CERTIFICATES); tls.setHandshakeTimeout(8);
  HTTPClient http; http.setConnectTimeout(HTTP_TIMEOUT_MS); http.setTimeout(HTTP_TIMEOUT_MS);
  // Stream a non-chunked body, avoiding a second full JSON buffer in RAM.
  http.useHTTP10(true); http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(tls, url)) return -12;
  const int code = http.GET();
  if (code == HTTP_CODE_OK) {
    const DeserializationError error = deserializeJson(result, http.getStream(), DeserializationOption::Filter(filter));
    http.end(); return error ? -13 : code;
  }
  http.end(); return code;
}
int fetchOpenWeather(const char* endpoint, JsonDocument& doc, JsonDocument& filter, uint8_t& activeKey) {
  for (int key = 0; key < 2; ++key) {
    doc.clear();
    const String url = String("https://api.openweathermap.org/data/2.5/") + endpoint +
      "?lat=" + LATITUDE + "&lon=" + LONGITUDE + "&units=metric&lang=de&appid=" + OPENWEATHER_KEYS[key];
    const int status = fetchJson(url, doc, filter);
    if (status == 200) { activeKey = key + 1; return status; }
    if (key == 1 || OPENWEATHER_KEYS[1][0] == '\0' || (status != 401 && status != 403 && status != 429)) return status;
  }
  return -14;
}
void fetchCurrent(WeatherState& next) {
  JsonDocument doc, filter; currentWeatherFilter(filter);
  next.currentHttp = fetchOpenWeather("weather", doc, filter, next.activeKey);
  if (next.currentHttp != 200) return;
  CurrentWeather result; result.valid = parsePoint(doc.as<JsonObjectConst>(), result.point, true);
  result.sunrise = doc["sys"]["sunrise"] | static_cast<time_t>(0);
  result.sunset = doc["sys"]["sunset"] | static_cast<time_t>(0);
  if (!result.valid || result.sunrise <= 0 || result.sunset <= result.sunrise) { next.currentHttp = -14; return; }
  result.fetched = time(nullptr); next.current = result;
}
void fetchForecast(WeatherState& next) {
  JsonDocument doc, filter; forecastWeatherFilter(filter);
  next.forecastHttp = fetchOpenWeather("forecast", doc, filter, next.activeKey);
  if (next.forecastHttp != 200) return;
  JsonArrayConst list = doc["list"];
  if (list.size() == 0 || list.size() > 40) { next.forecastHttp = -14; return; }
  std::unique_ptr<WeatherPoint[]> points(new (std::nothrow) WeatherPoint[list.size()]);
  if (!points) { next.forecastHttp = -15; return; }
  size_t count = 0;
  for (JsonObjectConst entry : list) {
    if (!parsePoint(entry, points[count])) { next.forecastHttp = -14; return; }
    ++count;
  }
  DayForecast days[4]; buildDailyForecast(points.get(), count, time(nullptr), days);
  for (int i = 0; i < 4; ++i) if (!days[i].valid) { next.forecastHttp = -14; return; }
  for (int i = 0; i < 4; ++i) next.days[i] = days[i];
  next.forecastFetched = time(nullptr);
}
void fetchUv(WeatherState& next) {
  JsonDocument doc, filter;
  filter["hourly"]["time"] = true; filter["hourly"]["uv_index"] = true;
  const String url = String("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=") +
    LATITUDE + "&longitude=" + LONGITUDE + "&hourly=uv_index&timeformat=unixtime&forecast_days=2";
  next.uvHttp = fetchJson(url, doc, filter);
  if (next.uvHttp != 200) return;
  JsonArrayConst times = doc["hourly"]["time"], values = doc["hourly"]["uv_index"];
  const time_t nowUtc = time(nullptr); int selected = -1;
  for (size_t i = 0; i < times.size() && i < values.size(); ++i) {
    const time_t stamp = times[i].as<time_t>();
    if (stamp <= nowUtc && nowUtc < stamp + 3600) selected = i;
  }
  if (selected < 0 || !values[selected].is<float>()) { next.uvHttp = -14; return; }
  const float uv = values[selected];
  if (!isfinite(uv) || uv < 0 || uv > 30) { next.uvHttp = -14; return; }
  next.uv = uv; next.uvTimestamp = times[selected].as<time_t>(); next.uvFetched = nowUtc; next.uvValid = true;
}
void networkTask(void*) {
  for (;;) {
    const bool manual = requested.exchange(false);
    if (!manual && static_cast<int32_t>(millis() - nextFetch.load()) < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    refreshing.store(true); WeatherState next; copyWeatherState(next);
    if (WiFi.status() != WL_CONNECTED) {
      strlcpy(next.status, "Verbinde erneut mit WLAN", sizeof(next.status)); publish(next);
      // Let the initial WiFi.begin() finish before trying a reconnect.
      if (millis() >= 15000) WiFi.reconnect();
      const uint32_t started = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (WiFi.status() != WL_CONNECTED) {
      strlcpy(next.status, "WLAN offline, neuer Versuch in 60 s", sizeof(next.status));
      next.currentHttp = next.forecastHttp = next.uvHttp = -10;
    } else {
      const uint32_t started = millis();
      while (!clockValid() && millis() - started < 15000) vTaskDelay(pdMS_TO_TICKS(100));
      strlcpy(next.status, "Aktualisiere Wetter und UV", sizeof(next.status)); publish(next);
      fetchCurrent(next); fetchForecast(next); fetchUv(next);
      if (next.currentHttp == 200 && next.forecastHttp == 200 && next.uvHttp == 200)
        strlcpy(next.status, "Alle Quellen aktuell", sizeof(next.status));
      else snprintf(next.status, sizeof(next.status), "API-Status: %d / %d / %d", next.currentHttp, next.forecastHttp, next.uvHttp);
    }
    const bool success = next.currentHttp == 200 && next.forecastHttp == 200 && next.uvHttp == 200;
    nextFetch.store(millis() + (success ? UPDATE_INTERVAL_MS : RETRY_INTERVAL_MS));
    publish(next); refreshing.store(false);
  }
}
}
void copyWeatherState(WeatherState& destination) {
  if (!stateMutex) { destination = WeatherState{}; return; }
  xSemaphoreTake(stateMutex, portMAX_DELAY); destination = published; xSemaphoreGive(stateMutex);
}
bool requestWeatherRefresh() {
  if (refreshing.load() || requested.exchange(true)) return false;
  return true;
}
bool weatherIsRefreshing() { return refreshing.load(); }
uint32_t weatherNextRefreshSeconds() {
  return refreshing.load() || requested.load() ? 0 : secondsUntil(millis(), nextFetch.load());
}
void startWeatherNetwork() {
  stateMutex = xSemaphoreCreateMutex();
  if (!stateMutex || xTaskCreatePinnedToCore(networkTask, "weather", 12288, nullptr, 1, nullptr, 0) != pdPASS)
    Serial.println("ERROR: could not start weather worker");
}

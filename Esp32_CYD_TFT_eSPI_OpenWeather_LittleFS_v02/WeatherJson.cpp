#include "WeatherJson.h"
#include <string.h>
#include <math.h>
void pointFilter(JsonObject filter) {
  filter["dt"] = true;
  for (const char* field : {"temp", "feels_like", "temp_min", "temp_max", "humidity", "pressure"}) filter["main"][field] = true;
  for (const char* field : {"id", "main", "description", "icon"}) filter["weather"][0][field] = true;
  for (const char* field : {"speed", "deg"}) filter["wind"][field] = true;
  filter["clouds"]["all"] = true; filter["rain"]["1h"] = true; filter["rain"]["3h"] = true;
  filter["visibility"] = true; filter["pop"] = true;
}
bool parsePoint(JsonObjectConst object, WeatherPoint& p, bool current) {
  if (!object["dt"].is<int64_t>() || !object["main"]["temp"].is<float>() ||
      !object["main"]["humidity"].is<int>() || !object["main"]["pressure"].is<int>() ||
      !object["weather"][0]["id"].is<int>()) return false;
  p.timestamp = object["dt"].as<time_t>(); p.temperature = object["main"]["temp"];
  p.feelsLike = object["main"]["feels_like"] | p.temperature;
  p.low = object["main"]["temp_min"] | p.temperature; p.high = object["main"]["temp_max"] | p.temperature;
  p.humidity = object["main"]["humidity"]; p.pressure = object["main"]["pressure"];
  p.clouds = object["clouds"]["all"] | 0; p.wind = object["wind"]["speed"] | 0.0f;
  p.windDegrees = object["wind"]["deg"] | 0; p.visibility = object["visibility"] | 0;
  p.rain = object["rain"][current ? "1h" : "3h"] | 0.0f;
  p.precipitationProbability = object["pop"] | 0.0f; p.conditionId = object["weather"][0]["id"];
  strlcpy(p.icon, object["weather"][0]["icon"] | "", sizeof(p.icon));
  strlcpy(p.condition, object["weather"][0]["main"] | "Unbekannt", sizeof(p.condition));
  strlcpy(p.description, object["weather"][0]["description"] | "Unbekannt", sizeof(p.description));
  return p.timestamp > 1700000000 && isfinite(p.temperature) && p.temperature > -100 &&
    p.temperature < 75 && p.humidity >= 0 && p.humidity <= 100 && p.pressure > 700 && p.pressure < 1200;
}

void currentWeatherFilter(JsonDocument& filter) {
  pointFilter(filter.to<JsonObject>());
  filter["sys"]["sunrise"] = true;
  filter["sys"]["sunset"] = true;
}
void forecastWeatherFilter(JsonDocument& filter) {
  // A proxy converted to a null JsonVariant is unbound. Create the object first.
  pointFilter(filter["list"][0].to<JsonObject>());
}

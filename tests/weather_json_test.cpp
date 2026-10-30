#include "WeatherJson.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
  JsonDocument currentFilter, current;
  currentWeatherFilter(currentFilter);
  std::ifstream currentFile("tests/fixtures/current.json");
  assert(!deserializeJson(current,currentFile,DeserializationOption::Filter(currentFilter)));
  WeatherPoint observation;
  assert(parsePoint(current.as<JsonObjectConst>(),observation,true));
  assert(observation.conditionId > 0 && observation.humidity > 0);
  assert(current["sys"]["sunset"].as<time_t>() > current["sys"]["sunrise"].as<time_t>());
  JsonDocument forecastFilter, forecast;
  forecastWeatherFilter(forecastFilter);
  assert(forecastFilter["list"][0]["dt"].as<bool>());
  std::ifstream forecastFile("tests/fixtures/forecast.json");
  assert(!deserializeJson(forecast,forecastFile,DeserializationOption::Filter(forecastFilter)));
  assert(forecast["list"].size() == 40);
  WeatherPoint points[40];size_t count=0;
  for (JsonObjectConst entry:forecast["list"].as<JsonArrayConst>()) assert(parsePoint(entry,points[count++]));
  DayForecast days[4];buildDailyForecast(points,count,observation.timestamp,days);
  for (const auto& day:days) assert(day.valid && day.samples == 8 && day.high >= day.low);
  JsonDocument invalid;
  deserializeJson(invalid,"{\"cod\":401,\"message\":\"Invalid API key\"}");
  WeatherPoint noData;
  assert(!parsePoint(invalid.as<JsonObjectConst>(),noData));
  current["main"].remove("humidity");
  assert(!parsePoint(current.as<JsonObjectConst>(),noData));
  std::cout<<"PASS: current and 40-point forecast filters, daily coverage, API-error and missing-field rejection\n";
}

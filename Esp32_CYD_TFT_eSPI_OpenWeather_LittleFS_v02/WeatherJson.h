#pragma once
#include <ArduinoJson.h>
#include "WeatherModel.h"
void currentWeatherFilter(JsonDocument& filter);
void forecastWeatherFilter(JsonDocument& filter);
bool parsePoint(JsonObjectConst object, WeatherPoint& point, bool current = false);

#pragma once
#include "WeatherModel.h"
void startWeatherNetwork();
bool requestWeatherRefresh();
bool weatherIsRefreshing();
uint32_t weatherNextRefreshSeconds();
void copyWeatherState(WeatherState& destination);

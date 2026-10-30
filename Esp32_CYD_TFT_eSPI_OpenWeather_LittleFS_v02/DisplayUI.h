#pragma once
#include <TFT_eSPI.h>
#if !defined(USER_SETUP_ID) || (USER_SETUP_ID != 801 && USER_SETUP_ID != 805)
#error "Configure TFT_eSPI for this CYD: replace its User_Setup.h with the supplied Setup801_ESP32_CYD_ILI9341_240x320.h, then Verify again."
#endif
#include "WeatherModel.h"
extern TFT_eSPI tft;
bool beginDisplay();
void renderPage(const WeatherState& weather, int page, bool autoCycle, bool refreshing);
void renderCalibration(int step, bool failed = false);
void showBootSplash();
void auditAssets();

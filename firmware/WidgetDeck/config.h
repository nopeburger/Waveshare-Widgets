#pragma once
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef GAS_EIA_API_KEY
#define GAS_EIA_API_KEY ""
#endif
#ifndef GAS_WIFI_SSID
#define GAS_WIFI_SSID ""
#endif
#ifndef GAS_WIFI_PASSWORD
#define GAS_WIFI_PASSWORD ""
#endif
#define DECK_TIMEZONE "PST8PDT,M3.2.0,M11.1.0"
#define GAS_REFRESH_MS (6UL*60*60*1000)
#if __has_include("display_settings.h")
#include "display_settings.h"
#endif
#ifndef DECK_NIGHT_MODE_ENABLED
#define DECK_NIGHT_MODE_ENABLED 0
#endif
#ifndef DECK_DAY_BRIGHTNESS_PERCENT
#define DECK_DAY_BRIGHTNESS_PERCENT 85
#endif
#ifndef DECK_NIGHT_BRIGHTNESS_PERCENT
#define DECK_NIGHT_BRIGHTNESS_PERCENT 20
#endif
#ifndef DECK_TOUCH_BRIGHTNESS_PERCENT
#define DECK_TOUCH_BRIGHTNESS_PERCENT 100
#endif
#ifndef DECK_NIGHT_START_MINUTE
#define DECK_NIGHT_START_MINUTE (22*60+30)
#endif
#ifndef DECK_NIGHT_END_MINUTE
#define DECK_NIGHT_END_MINUTE (7*60)
#endif
#ifndef DECK_NIGHT_TOUCH_TIMEOUT_MS
#define DECK_NIGHT_TOUCH_TIMEOUT_MS 30000
#endif

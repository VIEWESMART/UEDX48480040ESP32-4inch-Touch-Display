#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Map Gaode Chinese weather text to English (ASCII-safe for LVGL fonts). */
const char *weather_i18n_condition(const char *zh);

/** Map Gaode Chinese wind direction to English abbreviation. */
const char *weather_i18n_wind_dir(const char *zh);

/** Map Gaode Chinese AQI quality label to English. */
const char *weather_i18n_aqi_quality(const char *zh);

#ifdef __cplusplus
}
#endif

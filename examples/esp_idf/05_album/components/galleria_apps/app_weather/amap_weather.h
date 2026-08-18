#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AMAP_WEATHER_MAX_DAYS 4

typedef struct {
  char date[12];
  char week[4];
  char dayweather[16];
  char nightweather[16];
  char daytemp[8];
  char nighttemp[8];
  char daywind[8];
  char daypower[12];
  char nightwind[8];
  char nightpower[12];
} amap_weather_day_t;

typedef struct {
  char city[32];
  char province[32];
  char reporttime[24];
  int day_count;
  amap_weather_day_t days[AMAP_WEATHER_MAX_DAYS];
} amap_weather_forecast_t;

typedef struct {
  bool valid;
  char weather[16];
  char temperature[8];
  char humidity[8];
  char winddirection[16];
  char windpower[8];
  char reporttime[24];
} amap_weather_live_t;

typedef struct {
  bool valid;
  char aqi[8];
  char quality[16];
  char pm25[8];
} amap_weather_aqi_t;

typedef struct {
  amap_weather_live_t live;
  amap_weather_forecast_t forecast;
  amap_weather_aqi_t aqi;
} weather_bundle_t;

esp_err_t weather_fetch_bundle(const char *city_adcode, weather_bundle_t *out);

#ifdef __cplusplus
}
#endif

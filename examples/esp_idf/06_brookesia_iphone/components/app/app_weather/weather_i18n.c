#include "weather_i18n.h"

#include <string.h>

typedef struct {
  const char *zh;
  const char *en;
} weather_map_t;

static const weather_map_t k_conditions[] = {
    {"雷阵雨伴有冰雹", "Thunderstorm w/ Hail"},
    {"强雷阵雨", "Heavy T-Storm"},
    {"雷阵雨", "Thunderstorm"},
    {"特大暴雨", "Extreme Rain"},
    {"大暴雨", "Very Heavy Rain"},
    {"暴雨", "Heavy Rain"},
    {"中雨", "Moderate Rain"},
    {"小雨", "Light Rain"},
    {"毛毛雨", "Drizzle"},
    {"阵雨", "Showers"},
    {"冻雨", "Freezing Rain"},
    {"雨夹雪", "Sleet"},
    {"暴雪", "Blizzard"},
    {"大雪", "Heavy Snow"},
    {"中雪", "Moderate Snow"},
    {"小雪", "Light Snow"},
    {"阵雪", "Snow Showers"},
    {"浮尘", "Dust"},
    {"扬沙", "Blowing Sand"},
    {"沙尘暴", "Sandstorm"},
    {"强沙尘暴", "Heavy Sandstorm"},
    {"雾", "Fog"},
    {"浓雾", "Dense Fog"},
    {"霾", "Haze"},
    {"多云", "Cloudy"},
    {"阴", "Overcast"},
    {"晴", "Clear"},
    {"雨", "Rain"},
    {"雪", "Snow"},
};

static const weather_map_t k_wind_dirs[] = {
    {"无风向", "Calm"},
    {"旋转风", "Variable"},
    {"东北", "NE"},
    {"东南", "SE"},
    {"西北", "NW"},
    {"西南", "SW"},
    {"东", "E"},
    {"南", "S"},
    {"西", "W"},
    {"北", "N"},
};

static const weather_map_t k_aqi_quality[] = {
    {"严重污染", "Hazardous"},
    {"重度污染", "Very Unhealthy"},
    {"中度污染", "Unhealthy"},
    {"轻度污染", "Moderate"},
    {"良", "Good"},
    {"优", "Excellent"},
};

static const char *lookup(const weather_map_t *table, size_t count,
                          const char *zh) {
  if (zh == NULL || zh[0] == '\0') {
    return "--";
  }
  for (size_t i = 0; i < count; i++) {
    if (strstr(zh, table[i].zh) != NULL) {
      return table[i].en;
    }
  }
  return "Unknown";
}

const char *weather_i18n_condition(const char *zh) {
  return lookup(k_conditions,
                sizeof(k_conditions) / sizeof(k_conditions[0]), zh);
}

const char *weather_i18n_wind_dir(const char *zh) {
  return lookup(k_wind_dirs, sizeof(k_wind_dirs) / sizeof(k_wind_dirs[0]),
                zh);
}

const char *weather_i18n_aqi_quality(const char *zh) {
  return lookup(k_aqi_quality,
                sizeof(k_aqi_quality) / sizeof(k_aqi_quality[0]), zh);
}

#include "amap_weather.h"

#include "weather_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "amap_weather";

static esp_err_t http_get(const char *url, char **out_body) {
  if (url == NULL || out_body == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  *out_body = NULL;

  esp_http_client_config_t config = {};
  config.url = url;
  config.timeout_ms = 15000;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.buffer_size = 4096;
  config.buffer_size_tx = 1024;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    return ESP_ERR_NO_MEM;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "HTTP open failed: %s (int=%u ext=%u B)", esp_err_to_name(err),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    esp_http_client_cleanup(client);
    return err;
  }

  const int content_length = esp_http_client_fetch_headers(client);
  int capacity = content_length > 0 ? content_length + 1 : 4096;
  char *body =
      (char *)heap_caps_malloc((size_t)capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (body == NULL) {
    body = (char *)malloc((size_t)capacity);
  }
  if (body == NULL) {
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_ERR_NO_MEM;
  }

  int total = 0;
  while (true) {
    if (total + 512 > capacity) {
      capacity += 4096;
      char *next =
          (char *)heap_caps_realloc(body, (size_t)capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (next == NULL) {
        next = (char *)realloc(body, (size_t)capacity);
      }
      if (next == NULL) {
        heap_caps_free(body);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
      }
      body = next;
    }
    const int read =
        esp_http_client_read(client, body + total, capacity - total - 1);
    if (read <= 0) {
      break;
    }
    total += read;
  }
  body[total] = '\0';

  const int status = esp_http_client_get_status_code(client);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (status != 200 || total <= 0) {
    ESP_LOGW(TAG, "HTTP %d for %s", status, url);
    heap_caps_free(body);
    return ESP_FAIL;
  }

  *out_body = body;
  return ESP_OK;
}

static void copy_str(char *dst, size_t dst_len, const char *src) {
  if (dst == NULL || dst_len == 0) {
    return;
  }
  if (src == NULL) {
    dst[0] = '\0';
    return;
  }
  snprintf(dst, dst_len, "%s", src);
}

static bool api_ok(const cJSON *root) {
  const cJSON *status = cJSON_GetObjectItem(root, "status");
  return cJSON_IsString(status) && strcmp(status->valuestring, "1") == 0;
}

static esp_err_t parse_live_json(const char *json, amap_weather_live_t *out) {
  memset(out, 0, sizeof(*out));
  cJSON *root = cJSON_Parse(json);
  if (root == NULL || !api_ok(root)) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  const cJSON *lives = cJSON_GetObjectItem(root, "lives");
  const cJSON *live0 =
      cJSON_IsArray(lives) ? cJSON_GetArrayItem(lives, 0) : NULL;
  if (live0 == NULL) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  copy_str(out->weather, sizeof(out->weather),
           cJSON_GetStringValue(cJSON_GetObjectItem(live0, "weather")));
  copy_str(out->temperature, sizeof(out->temperature),
           cJSON_GetStringValue(cJSON_GetObjectItem(live0, "temperature")));
  copy_str(out->humidity, sizeof(out->humidity),
           cJSON_GetStringValue(cJSON_GetObjectItem(live0, "humidity")));
  copy_str(out->winddirection, sizeof(out->winddirection),
           cJSON_GetStringValue(cJSON_GetObjectItem(live0, "winddirection")));
  copy_str(out->windpower, sizeof(out->windpower),
           cJSON_GetStringValue(cJSON_GetObjectItem(live0, "windpower")));
  copy_str(out->reporttime, sizeof(out->reporttime),
           cJSON_GetStringValue(cJSON_GetObjectItem(live0, "reporttime")));
  out->valid = out->weather[0] != '\0';
  cJSON_Delete(root);
  return out->valid ? ESP_OK : ESP_FAIL;
}

static esp_err_t parse_forecast_json(const char *json,
                                     amap_weather_forecast_t *out) {
  memset(out, 0, sizeof(*out));
  cJSON *root = cJSON_Parse(json);
  if (root == NULL || !api_ok(root)) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  const cJSON *forecasts = cJSON_GetObjectItem(root, "forecasts");
  const cJSON *forecast0 =
      cJSON_IsArray(forecasts) ? cJSON_GetArrayItem(forecasts, 0) : NULL;
  if (forecast0 == NULL) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  copy_str(out->city, sizeof(out->city),
           cJSON_GetStringValue(cJSON_GetObjectItem(forecast0, "city")));
  copy_str(out->province, sizeof(out->province),
           cJSON_GetStringValue(cJSON_GetObjectItem(forecast0, "province")));
  copy_str(out->reporttime, sizeof(out->reporttime),
           cJSON_GetStringValue(cJSON_GetObjectItem(forecast0, "reporttime")));

  const cJSON *casts = cJSON_GetObjectItem(forecast0, "casts");
  if (!cJSON_IsArray(casts)) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  const int count = cJSON_GetArraySize(casts);
  out->day_count =
      count > AMAP_WEATHER_MAX_DAYS ? AMAP_WEATHER_MAX_DAYS : count;
  for (int i = 0; i < out->day_count; i++) {
    const cJSON *cast = cJSON_GetArrayItem(casts, i);
    if (cast == NULL) {
      continue;
    }
    amap_weather_day_t *day = &out->days[i];
    copy_str(day->date, sizeof(day->date),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "date")));
    copy_str(day->week, sizeof(day->week),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "week")));
    copy_str(day->dayweather, sizeof(day->dayweather),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "dayweather")));
    copy_str(day->nightweather, sizeof(day->nightweather),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "nightweather")));
    copy_str(day->daytemp, sizeof(day->daytemp),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "daytemp")));
    copy_str(day->nighttemp, sizeof(day->nighttemp),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "nighttemp")));
    copy_str(day->daywind, sizeof(day->daywind),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "daywind")));
    copy_str(day->nightwind, sizeof(day->nightwind),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "nightwind")));
    copy_str(day->daypower, sizeof(day->daypower),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "daypower")));
    copy_str(day->nightpower, sizeof(day->nightpower),
             cJSON_GetStringValue(cJSON_GetObjectItem(cast, "nightpower")));
  }

  cJSON_Delete(root);
  return out->day_count > 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t parse_aqi_json(const char *json, amap_weather_aqi_t *out) {
  memset(out, 0, sizeof(*out));
  cJSON *root = cJSON_Parse(json);
  if (root == NULL || !api_ok(root)) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  const cJSON *data = cJSON_GetObjectItem(root, "data");
  if (data == NULL) {
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  copy_str(out->aqi, sizeof(out->aqi),
           cJSON_GetStringValue(cJSON_GetObjectItem(data, "aqi")));
  copy_str(out->quality, sizeof(out->quality),
           cJSON_GetStringValue(cJSON_GetObjectItem(data, "quality")));
  copy_str(out->pm25, sizeof(out->pm25),
           cJSON_GetStringValue(cJSON_GetObjectItem(data, "pm25")));
  out->valid = out->aqi[0] != '\0';
  cJSON_Delete(root);
  return out->valid ? ESP_OK : ESP_FAIL;
}

static esp_err_t fetch_live(const char *city_adcode, amap_weather_live_t *out) {
  char url[256];
  snprintf(url, sizeof(url),
           "https://restapi.amap.com/v3/weather/weatherInfo?city=%s&key=%s&"
           "extensions=base",
           city_adcode, AMAP_WEATHER_API_KEY);
  char *body = NULL;
  const esp_err_t err = http_get(url, &body);
  if (err != ESP_OK) {
    return err;
  }
  const esp_err_t parse_err = parse_live_json(body, out);
  free(body);
  return parse_err;
}

static esp_err_t fetch_forecast(const char *city_adcode,
                                amap_weather_forecast_t *out) {
  char url[256];
  snprintf(url, sizeof(url),
           "https://restapi.amap.com/v3/weather/weatherInfo?city=%s&key=%s&"
           "extensions=all",
           city_adcode, AMAP_WEATHER_API_KEY);
  char *body = NULL;
  const esp_err_t err = http_get(url, &body);
  if (err != ESP_OK) {
    return err;
  }
  const esp_err_t parse_err = parse_forecast_json(body, out);
  free(body);
  return parse_err;
}

static esp_err_t fetch_aqi(const char *city_adcode, amap_weather_aqi_t *out) {
  char url[256];
  snprintf(url, sizeof(url),
           "https://restapi.amap.com/v3/airquality/now?city=%s&key=%s",
           city_adcode, AMAP_WEATHER_API_KEY);
  char *body = NULL;
  const esp_err_t err = http_get(url, &body);
  if (err != ESP_OK) {
    return err;
  }
  const esp_err_t parse_err = parse_aqi_json(body, out);
  free(body);
  if (parse_err != ESP_OK) {
    ESP_LOGW(TAG, "AQI unavailable (Key permission?)");
  }
  return parse_err;
}

esp_err_t weather_fetch_bundle(const char *city_adcode,
                               weather_bundle_t *out) {
  if (city_adcode == NULL || out == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  memset(out, 0, sizeof(*out));

  esp_err_t forecast_err = fetch_forecast(city_adcode, &out->forecast);
  esp_err_t live_err = fetch_live(city_adcode, &out->live);
  fetch_aqi(city_adcode, &out->aqi);

  if (forecast_err == ESP_OK) {
    ESP_LOGI(TAG, "forecast %s %s, %d day(s)", out->forecast.province,
             out->forecast.city, out->forecast.day_count);
  }
  if (live_err == ESP_OK) {
    ESP_LOGI(TAG, "live %sC hum=%s%% wind=%s %s", out->live.temperature,
             out->live.humidity, out->live.winddirection, out->live.windpower);
  }

  if (forecast_err == ESP_OK || live_err == ESP_OK) {
    return ESP_OK;
  }
  return ESP_FAIL;
}

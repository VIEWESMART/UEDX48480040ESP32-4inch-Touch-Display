#pragma once

#include "amap_weather.h"

#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class WeatherApp : public ESP_Brookesia_PhoneApp {
public:
  WeatherApp();
  ~WeatherApp() override = default;

  bool run(void) override;
  bool pause(void) override;
  bool resume(void) override;
  bool back(void) override;
  bool close(void) override;

private:
  struct FetchResult {
    WeatherApp *app;
    uint32_t generation;
    esp_err_t err;
    weather_bundle_t bundle{};
  };

  lv_obj_t *_root = nullptr;
  lv_obj_t *_status = nullptr;
  lv_obj_t *_scroll = nullptr;
  lv_obj_t *_hero_card = nullptr;
  lv_obj_t *_hero_city = nullptr;
  lv_obj_t *_hero_temp = nullptr;
  lv_obj_t *_hero_cond = nullptr;
  lv_obj_t *_hero_range = nullptr;
  lv_obj_t *_metrics_grid = nullptr;
  lv_obj_t *_metric_humidity = nullptr;
  lv_obj_t *_metric_wind = nullptr;
  lv_obj_t *_metric_aqi = nullptr;
  lv_obj_t *_forecast_list = nullptr;
  lv_obj_t *_refresh_btn = nullptr;

  weather_bundle_t _bundle{};
  bool _has_data = false;
  bool _closing = false;
  bool _fetch_busy = false;
  bool _fetch_pending = false;
  uint32_t _generation = 0;

  lv_timer_t *_refresh_timer = nullptr;

  void buildUi(void);
  void setStatus(const char *text);
  void updateDisplay(void);
  void startFetch(void);
  void applyFetchResult(FetchResult *result);
  void prepareForSnapshot(void);
  void setUiVisible(bool visible);

  static lv_obj_t *createMetricCard(lv_obj_t *parent, const char *title,
                                    lv_obj_t **value_out);
  static void styleCard(lv_obj_t *obj, uint32_t bg_hex);
  static bool ensureFetchWorker(void);

  static void fetch_worker(void *arg);
  static void async_apply_cb(void *user_data);
  static void refresh_timer_cb(lv_timer_t *timer);
  static void refresh_btn_cb(lv_event_t *e);
};

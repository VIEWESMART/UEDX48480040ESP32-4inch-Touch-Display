#include "WeatherApp.hpp"

#include "app_icons.h"
#include "board_wifi.h"
#include "weather_config.h"
#include "weather_i18n.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lv_adapter.h"
#include "freertos/queue.h"

static const char *TAG = "Weather";
static constexpr uint32_t kAutoRefreshMs = 30U * 60U * 1000U;
static constexpr uint32_t kFetchStackBytes = 8192;

static QueueHandle_t s_wx_fetch_queue = nullptr;
static TaskHandle_t s_wx_fetch_worker = nullptr;

static constexpr uint32_t kBgScreen = 0x0F172A;
static constexpr uint32_t kBgCard = 0x1E293B;
static constexpr uint32_t kBgCardAlt = 0x172033;
static constexpr uint32_t kAccent = 0x38BDF8;
static constexpr uint32_t kTextPrimary = 0xF8FAFC;
static constexpr uint32_t kTextSecondary = 0x94A3B8;
static constexpr uint32_t kTextMuted = 0x64748B;

WeatherApp::WeatherApp()
    : ESP_Brookesia_PhoneApp("Weather", &galleria_app_icon_weather, true,
                             false, false) {}

void WeatherApp::styleCard(lv_obj_t *obj, uint32_t bg_hex) {
  lv_obj_remove_style_all(obj);
  lv_obj_set_style_bg_color(obj, lv_color_hex(bg_hex), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 10, 0);
  lv_obj_set_style_pad_all(obj, 10, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *WeatherApp::createMetricCard(lv_obj_t *parent, const char *title,
                                       lv_obj_t **value_out) {
  lv_obj_t *card = lv_obj_create(parent);
  styleCard(card, kBgCardAlt);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(card, 4, 0);

  lv_obj_t *title_lbl = lv_label_create(card);
  lv_label_set_text(title_lbl, title);
  lv_obj_set_style_text_color(title_lbl, lv_color_hex(kTextMuted), 0);
  lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, 0);

  lv_obj_t *value_lbl = lv_label_create(card);
  lv_label_set_text(value_lbl, "--");
  lv_obj_set_style_text_color(value_lbl, lv_color_hex(kTextPrimary), 0);
  lv_obj_set_style_text_font(value_lbl, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(value_lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(value_lbl, lv_pct(100));

  if (value_out != nullptr) {
    *value_out = value_lbl;
  }
  return card;
}

bool WeatherApp::run(void) {
  _closing = false;
  _fetch_busy = false;
  _fetch_pending = false;
  _has_data = false;
  _generation++;
  memset(&_bundle, 0, sizeof(_bundle));

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(kBgScreen), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  buildUi();
  setStatus("Loading forecast...");

  if (_refresh_timer != nullptr) {
    lv_timer_delete(_refresh_timer);
  }
  _refresh_timer = lv_timer_create(refresh_timer_cb, kAutoRefreshMs, this);

  startFetch();
  return true;
}

void WeatherApp::buildUi(void) {
  lv_area_t area = getVisualArea();
  const int w = lv_area_get_width(&area);
  const int h = lv_area_get_height(&area);

  _root = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(_root);
  lv_obj_set_size(_root, w, h);
  lv_obj_set_pos(_root, area.x1, area.y1);
  lv_obj_set_style_bg_color(_root, lv_color_hex(kBgScreen), 0);
  lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(_root, 10, 0);
  lv_obj_set_style_pad_top(_root, 6, 0);
  lv_obj_set_style_pad_bottom(_root, 8, 0);
  lv_obj_set_flex_flow(_root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(_root, 6, 0);
  lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

  _status = lv_label_create(_root);
  lv_obj_set_width(_status, w - 20);
  lv_label_set_long_mode(_status, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(_status, lv_color_hex(kTextMuted), 0);
  lv_obj_set_style_text_font(_status, &lv_font_montserrat_14, 0);

  _scroll = lv_obj_create(_root);
  lv_obj_remove_style_all(_scroll);
  lv_obj_set_width(_scroll, w - 20);
  lv_obj_set_flex_grow(_scroll, 1);
  lv_obj_set_style_bg_opa(_scroll, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(_scroll, 0, 0);
  lv_obj_set_style_pad_row(_scroll, 8, 0);
  lv_obj_set_flex_flow(_scroll, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(_scroll, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_scroll, LV_SCROLLBAR_MODE_AUTO);

  _hero_card = lv_obj_create(_scroll);
  styleCard(_hero_card, kBgCard);
  lv_obj_set_width(_hero_card, lv_pct(100));
  lv_obj_set_height(_hero_card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(_hero_card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(_hero_card, 2, 0);

  _hero_city = lv_label_create(_hero_card);
  lv_label_set_text(_hero_city, WEATHER_CITY_DISPLAY_NAME);
  lv_obj_set_style_text_color(_hero_city, lv_color_hex(kTextSecondary), 0);
  lv_obj_set_style_text_font(_hero_city, &lv_font_montserrat_14, 0);

  _hero_temp = lv_label_create(_hero_card);
  lv_label_set_text(_hero_temp, "--");
  lv_obj_set_style_text_color(_hero_temp, lv_color_hex(kAccent), 0);
  lv_obj_set_style_text_font(_hero_temp, &lv_font_montserrat_22, 0);

  _hero_cond = lv_label_create(_hero_card);
  lv_label_set_text(_hero_cond, "--");
  lv_obj_set_style_text_color(_hero_cond, lv_color_hex(kTextPrimary), 0);
  lv_obj_set_style_text_font(_hero_cond, &lv_font_montserrat_18, 0);

  _hero_range = lv_label_create(_hero_card);
  lv_label_set_text(_hero_range, "--");
  lv_obj_set_style_text_color(_hero_range, lv_color_hex(kTextSecondary), 0);
  lv_obj_set_style_text_font(_hero_range, &lv_font_montserrat_14, 0);

  _metrics_grid = lv_obj_create(_scroll);
  lv_obj_remove_style_all(_metrics_grid);
  lv_obj_set_width(_metrics_grid, lv_pct(100));
  lv_obj_set_height(_metrics_grid, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(_metrics_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(_metrics_grid, 0, 0);
  lv_obj_set_style_pad_column(_metrics_grid, 8, 0);
  lv_obj_set_style_pad_row(_metrics_grid, 8, 0);
  lv_obj_set_flex_flow(_metrics_grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_remove_flag(_metrics_grid, LV_OBJ_FLAG_SCROLLABLE);

  const int card_w = (w - 20 - 8) / 2;
  lv_obj_t *humidity_card =
      createMetricCard(_metrics_grid, "Humidity", &_metric_humidity);
  lv_obj_set_width(humidity_card, card_w);
  lv_obj_t *wind_card =
      createMetricCard(_metrics_grid, "Wind", &_metric_wind);
  lv_obj_set_width(wind_card, card_w);
  lv_obj_t *aqi_card =
      createMetricCard(_metrics_grid, "Air Quality", &_metric_aqi);
  lv_obj_set_width(aqi_card, lv_pct(100));

  lv_obj_t *forecast_title = lv_label_create(_scroll);
  lv_label_set_text(forecast_title, "Forecast");
  lv_obj_set_style_text_color(forecast_title, lv_color_hex(kTextSecondary), 0);
  lv_obj_set_style_text_font(forecast_title, &lv_font_montserrat_14, 0);

  _forecast_list = lv_obj_create(_scroll);
  styleCard(_forecast_list, kBgCard);
  lv_obj_set_width(_forecast_list, lv_pct(100));
  lv_obj_set_height(_forecast_list, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(_forecast_list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(_forecast_list, 6, 0);

  _refresh_btn = lv_button_create(_root);
  lv_obj_set_width(_refresh_btn, w - 20);
  lv_obj_set_height(_refresh_btn, 34);
  lv_obj_set_style_bg_color(_refresh_btn, lv_color_hex(0x2563EB), 0);
  lv_obj_set_style_radius(_refresh_btn, 8, 0);
  lv_obj_t *lbl = lv_label_create(_refresh_btn);
  lv_label_set_text(lbl, "Refresh");
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl);
  lv_obj_add_event_cb(_refresh_btn, refresh_btn_cb, LV_EVENT_CLICKED, this);
}

void WeatherApp::setStatus(const char *text) {
  if (_status != nullptr && text != nullptr) {
    lv_label_set_text(_status, text);
  }
}

static const char *week_label(const char *week) {
  if (week == NULL || week[0] == '\0') {
    return "";
  }
  switch (week[0]) {
  case '1':
    return "Mon";
  case '2':
    return "Tue";
  case '3':
    return "Wed";
  case '4':
    return "Thu";
  case '5':
    return "Fri";
  case '6':
    return "Sat";
  case '7':
    return "Sun";
  default:
    return week;
  }
}

static void format_forecast_date(const char *date, char *out, size_t out_len) {
  if (date == NULL || out == NULL || out_len == 0) {
    return;
  }
  int year = 0;
  int month = 0;
  int day = 0;
  if (sscanf(date, "%d-%d-%d", &year, &month, &day) == 3) {
    snprintf(out, out_len, "%02d/%02d", month, day);
  } else {
    snprintf(out, out_len, "%s", date);
  }
}

void WeatherApp::updateDisplay(void) {
  if (_hero_temp == nullptr || _forecast_list == nullptr) {
    return;
  }

  char line[96];
  const amap_weather_live_t *live = &_bundle.live;
  const amap_weather_forecast_t *fc = &_bundle.forecast;

  if (live->valid) {
    snprintf(line, sizeof(line), "%s C", live->temperature);
    lv_label_set_text(_hero_temp, line);
    lv_label_set_text(_hero_cond,
                      weather_i18n_condition(live->weather));

    snprintf(line, sizeof(line), "%s%%", live->humidity);
    lv_label_set_text(_metric_humidity, line);

    snprintf(line, sizeof(line), "%s Lv%s",
             weather_i18n_wind_dir(live->winddirection), live->windpower);
    lv_label_set_text(_metric_wind, line);
  } else if (fc->day_count > 0) {
    const amap_weather_day_t *d0 = &fc->days[0];
    snprintf(line, sizeof(line), "%s C", d0->daytemp);
    lv_label_set_text(_hero_temp, line);
    lv_label_set_text(_hero_cond, weather_i18n_condition(d0->dayweather));
    lv_label_set_text(_metric_humidity, "--");
    snprintf(line, sizeof(line), "%s Lv%s",
             weather_i18n_wind_dir(d0->daywind), d0->daypower);
    lv_label_set_text(_metric_wind, line);
  }

  if (fc->day_count > 0) {
    const amap_weather_day_t *d0 = &fc->days[0];
    snprintf(line, sizeof(line), "H %s C / L %s C", d0->daytemp,
             d0->nighttemp);
    lv_label_set_text(_hero_range, line);
  } else {
    lv_label_set_text(_hero_range, "--");
  }

  if (_bundle.aqi.valid) {
    if (_bundle.aqi.pm25[0] != '\0') {
      snprintf(line, sizeof(line), "%s %s\nPM2.5 %s", _bundle.aqi.aqi,
               weather_i18n_aqi_quality(_bundle.aqi.quality),
               _bundle.aqi.pm25);
    } else {
      snprintf(line, sizeof(line), "%s %s", _bundle.aqi.aqi,
               weather_i18n_aqi_quality(_bundle.aqi.quality));
    }
    lv_label_set_text(_metric_aqi, line);
  } else {
    lv_label_set_text(_metric_aqi, "N/A");
  }

  lv_obj_clean(_forecast_list);
  const int start_day = fc->day_count > 0 ? 1 : 0;
  for (int i = start_day; i < fc->day_count && i < AMAP_WEATHER_MAX_DAYS; i++) {
    const amap_weather_day_t *d = &fc->days[i];
    lv_obj_t *row = lv_obj_create(_forecast_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, lv_color_hex(kBgCardAlt), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char date_buf[16];
    format_forecast_date(d->date, date_buf, sizeof(date_buf));

    lv_obj_t *top = lv_obj_create(row);
    lv_obj_remove_style_all(top);
    lv_obj_set_width(top, lv_pct(100));
    lv_obj_set_height(top, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *day_lbl = lv_label_create(top);
    snprintf(line, sizeof(line), "%s  %s", week_label(d->week), date_buf);
    lv_label_set_text(day_lbl, line);
    lv_obj_set_style_text_color(day_lbl, lv_color_hex(kTextPrimary), 0);
    lv_obj_set_style_text_font(day_lbl, &lv_font_montserrat_14, 0);

    lv_obj_t *temp_lbl = lv_label_create(top);
    snprintf(line, sizeof(line), "%s~%s C", d->nighttemp, d->daytemp);
    lv_label_set_text(temp_lbl, line);
    lv_obj_set_style_text_color(temp_lbl, lv_color_hex(kAccent), 0);
    lv_obj_set_style_text_font(temp_lbl, &lv_font_montserrat_14, 0);

    lv_obj_t *detail_lbl = lv_label_create(row);
    snprintf(line, sizeof(line), "%s / %s   %s Lv%s",
             weather_i18n_condition(d->dayweather),
             weather_i18n_condition(d->nightweather),
             weather_i18n_wind_dir(d->daywind), d->daypower);
    lv_label_set_text(detail_lbl, line);
    lv_obj_set_style_text_color(detail_lbl, lv_color_hex(kTextSecondary), 0);
    lv_obj_set_style_text_font(detail_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(detail_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(detail_lbl, lv_pct(100));
  }

  const char *updated = live->valid ? live->reporttime : fc->reporttime;
  if (updated[0] != '\0') {
    snprintf(line, sizeof(line), "Updated: %s", updated);
    setStatus(line);
  } else {
    setStatus("Forecast updated");
  }
}

void WeatherApp::startFetch(void) {
  if (_closing) {
    return;
  }
  if (!board_wifi_is_initialized() || !board_wifi_is_connected()) {
    setStatus("Connect WiFi in Settings first");
    return;
  }
  if (_fetch_busy) {
    _fetch_pending = true;
    return;
  }

  _fetch_busy = true;
  _fetch_pending = false;
  _generation++;
  setStatus("Fetching forecast...");

  auto *args = new FetchResult{this, _generation, ESP_FAIL, {}};
  if (!ensureFetchWorker()) {
    _fetch_busy = false;
    delete args;
    setStatus("Fetch task failed");
    return;
  }
  if (xQueueSend(s_wx_fetch_queue, &args, 0) != pdPASS) {
    _fetch_busy = false;
    delete args;
    setStatus("Fetch busy");
  }
}

bool WeatherApp::ensureFetchWorker(void) {
  if (s_wx_fetch_worker != nullptr) {
    return true;
  }
  s_wx_fetch_queue = xQueueCreate(3, sizeof(FetchResult *));
  if (s_wx_fetch_queue == nullptr) {
    ESP_LOGE(TAG, "fetch queue create failed");
    return false;
  }
  const BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(
      fetch_worker, "wx_fetch", kFetchStackBytes, nullptr, 2,
      &s_wx_fetch_worker, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "fetch worker create failed (stack=%u ext=%u B)",
             (unsigned)kFetchStackBytes,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    vQueueDelete(s_wx_fetch_queue);
    s_wx_fetch_queue = nullptr;
    return false;
  }
  return true;
}

void WeatherApp::fetch_worker(void *arg) {
  (void)arg;
  FetchResult *result = nullptr;
  while (true) {
    if (xQueueReceive(s_wx_fetch_queue, &result, portMAX_DELAY) != pdPASS ||
        result == nullptr) {
      continue;
    }
    if (result->app != nullptr) {
      result->err = weather_fetch_bundle(AMAP_WEATHER_CITY_ADCODE,
                                         &result->bundle);
      if (esp_lv_adapter_lock(pdMS_TO_TICKS(5000)) == ESP_OK) {
        lv_async_call(async_apply_cb, result);
        esp_lv_adapter_unlock();
      } else {
        delete result;
      }
    } else {
      delete result;
    }
  }
}

void WeatherApp::async_apply_cb(void *user_data) {
  auto *result = static_cast<FetchResult *>(user_data);
  if (result != nullptr && result->app != nullptr) {
    result->app->applyFetchResult(result);
  } else if (result != nullptr) {
    delete result;
  }
}

void WeatherApp::applyFetchResult(FetchResult *result) {
  if (result == nullptr) {
    return;
  }

  const bool stale =
      _closing || result->generation != _generation || result->app != this;
  _fetch_busy = false;

  if (!stale && result->err == ESP_OK) {
    _bundle = result->bundle;
    _has_data = true;
    updateDisplay();
  } else if (!stale) {
    if (_has_data) {
      setStatus("Refresh failed, showing cached data");
    } else {
      setStatus("Weather fetch failed");
    }
  }

  delete result;

  if (_fetch_pending && !_closing) {
    _fetch_pending = false;
    startFetch();
  }
}

void WeatherApp::setUiVisible(bool visible) {
  const lv_obj_flag_t flag = LV_OBJ_FLAG_HIDDEN;
  lv_obj_t *objs[] = {_status, _scroll, _refresh_btn};
  for (lv_obj_t *obj : objs) {
    if (obj == nullptr) {
      continue;
    }
    if (visible) {
      lv_obj_remove_flag(obj, flag);
    } else {
      lv_obj_add_flag(obj, flag);
    }
  }
}

void WeatherApp::prepareForSnapshot(void) {
  if (_root != nullptr) {
    lv_obj_set_style_bg_color(_root, lv_color_hex(kBgScreen), 0);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
  }
  setUiVisible(false);
}

bool WeatherApp::pause(void) {
  prepareForSnapshot();
  return true;
}

bool WeatherApp::resume(void) {
  if (_closing || _root == nullptr) {
    return true;
  }
  setUiVisible(true);
  return true;
}

bool WeatherApp::back(void) {
  prepareForSnapshot();
  notifyCoreClosed();
  return true;
}

bool WeatherApp::close(void) {
  _closing = true;
  _generation++;
  if (_refresh_timer != nullptr) {
    lv_timer_delete(_refresh_timer);
    _refresh_timer = nullptr;
  }
  _root = nullptr;
  _status = nullptr;
  _scroll = nullptr;
  _hero_card = nullptr;
  _hero_city = nullptr;
  _hero_temp = nullptr;
  _hero_cond = nullptr;
  _hero_range = nullptr;
  _metrics_grid = nullptr;
  _metric_humidity = nullptr;
  _metric_wind = nullptr;
  _metric_aqi = nullptr;
  _forecast_list = nullptr;
  _refresh_btn = nullptr;
  return true;
}

void WeatherApp::refresh_timer_cb(lv_timer_t *timer) {
  auto *app = static_cast<WeatherApp *>(lv_timer_get_user_data(timer));
  if (app != nullptr && !app->_closing) {
    app->startFetch();
  }
}

void WeatherApp::refresh_btn_cb(lv_event_t *e) {
  auto *app = static_cast<WeatherApp *>(lv_event_get_user_data(e));
  if (app != nullptr) {
    app->startFetch();
  }
}

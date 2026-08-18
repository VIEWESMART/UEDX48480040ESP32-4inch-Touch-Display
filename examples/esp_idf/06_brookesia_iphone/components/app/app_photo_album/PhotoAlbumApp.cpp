#include "PhotoAlbumApp.hpp"

#include "app_icons.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <map>
#include <strings.h>

#include "board_sd.h"
#include "board_storage.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "photo_jpeg_load.h"

static const char *TAG = "PhotoAlbum";

static constexpr UBaseType_t kDecodeTaskPriority = 2;
static constexpr uint32_t kDecodeStackBytes = 32768;

struct AlbumDecodeJob {
  PhotoAlbumApp *app;
  uint32_t session;
  int index;
};

static QueueHandle_t s_album_decode_queue = nullptr;
static TaskHandle_t s_album_decode_worker = nullptr;

static void album_decode_queue_reset(void) {
  if (s_album_decode_queue != nullptr) {
    xQueueReset(s_album_decode_queue);
  }
}

static bool ensure_album_decode_worker(void) {
  if (s_album_decode_worker != nullptr) {
    return true;
  }
  s_album_decode_queue = xQueueCreate(2, sizeof(AlbumDecodeJob));
  if (s_album_decode_queue == nullptr) {
    return false;
  }
  if (xTaskCreatePinnedToCoreWithCaps(
          PhotoAlbumApp::decode_worker, "album_dec", kDecodeStackBytes, nullptr,
          kDecodeTaskPriority, &s_album_decode_worker, 1,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    vQueueDelete(s_album_decode_queue);
    s_album_decode_queue = nullptr;
    return false;
  }
  return true;
}

static constexpr uint32_t kEntryLoadDeferMs = 1500;
static constexpr uint32_t kNavQuietWarmupMs = 1200;
static constexpr uint32_t kNavQuietNormalMs = 600;
static constexpr uint32_t kNavHoldMaxMs = 2500;
static constexpr uint32_t kShowDeferColdFirstMs = 150;
static constexpr uint32_t kShowDeferWarmupMs = 96;
static constexpr uint32_t kShowDeferNormalMs = 32;
static constexpr uint32_t kSeqPreloadAfterRevealMs = 1200;
static constexpr uint32_t kChromeRevealDelayMs = 450;
static constexpr uint32_t kSeqPreloadGapMs = 500;
static constexpr int kWarmupCacheTarget = 3;
static constexpr uint32_t kLoadRetryMs = 1200;

struct CachedImage {
  uint8_t *buf = nullptr;
  uint16_t w = 0;
  uint16_t h = 0;
  bool hw_buf = false;
};

static std::map<std::string, CachedImage> s_photo_cache;

static CachedImage *cacheGet(const std::string &name) {
  auto it = s_photo_cache.find(name);
  if (it == s_photo_cache.end() || it->second.buf == nullptr) {
    return nullptr;
  }
  return &it->second;
}

static void cachePut(const std::string &name, uint8_t *buf, uint16_t w,
                     uint16_t h, bool hw_buf) {
  CachedImage &slot = s_photo_cache[name];
  if (slot.buf != nullptr && slot.buf != buf) {
    photo_jpeg_free_buf(slot.buf, slot.hw_buf);
  }
  slot.buf = buf;
  slot.w = w;
  slot.h = h;
  slot.hw_buf = hw_buf;
}

void photo_album_release_cache(void) {
  for (auto &entry : s_photo_cache) {
    if (entry.second.buf != nullptr) {
      photo_jpeg_free_buf(entry.second.buf, entry.second.hw_buf);
      entry.second.buf = nullptr;
    }
  }
  s_photo_cache.clear();
  ESP_LOGI(TAG, "photo cache released");
}

PhotoAlbumApp::PhotoAlbumApp()
    : ESP_Brookesia_PhoneApp("Album", &galleria_app_icon_album, true, false, false) {}

PhotoAlbumApp::~PhotoAlbumApp() {
  for (auto &entry : s_photo_cache) {
    if (entry.second.buf != nullptr) {
      photo_jpeg_free_buf(entry.second.buf, entry.second.hw_buf);
      entry.second.buf = nullptr;
    }
  }
  s_photo_cache.clear();
}

int PhotoAlbumApp::cacheCount(void) const {
  int count = 0;
  for (const auto &entry : s_photo_cache) {
    if (entry.second.buf == nullptr) {
      continue;
    }
    for (const auto &name : _files) {
      if (name == entry.first) {
        count++;
        break;
      }
    }
  }
  return count;
}

static int warmupTarget(int total) {
  if (total <= 1) {
    return 1;
  }
  return (total < kWarmupCacheTarget) ? total : kWarmupCacheTarget;
}

bool PhotoAlbumApp::warmupComplete(void) const {
  if (_warmup_done) {
    return true;
  }
  const int cached = cacheCount();
  const int total = (int)_files.size();
  if (total <= 1) {
    return cached >= 1 && _first_revealed;
  }
  if (cached >= warmupTarget(total)) {
    return true;
  }
  if (cached >= 2 && _switch_count >= 1) {
    return true;
  }
  if (_first_revealed && _first_reveal_tick != 0 &&
      lv_tick_elaps(_first_reveal_tick) >= 4000 && cached >= 1) {
    return true;
  }
  return false;
}

bool PhotoAlbumApp::preloadAllowed(void) const {
  if (_closing || !_preload_ready || !_warmup_done) {
    return false;
  }
  if (_last_nav_tick == 0) {
    return true;
  }
  return lv_tick_elaps(_last_nav_tick) >= navQuietMs();
}

uint32_t PhotoAlbumApp::navQuietMs(void) const {
  return _warmup_done ? kNavQuietNormalMs : kNavQuietWarmupMs;
}

uint32_t PhotoAlbumApp::showDeferMs(void) const {
  if (_cold_entry && !_first_revealed) {
    return kShowDeferColdFirstMs;
  }
  return _warmup_done ? kShowDeferNormalMs : kShowDeferWarmupMs;
}

void PhotoAlbumApp::updateWarmupState(void) {
  if (_warmup_done || _closing) {
    return;
  }
  if (!warmupComplete()) {
    return;
  }
  _warmup_done = true;
  _preload_ready = true;
  ESP_LOGI(TAG, "warmup done: cache=%d switches=%d", cacheCount(), _switch_count);
  if (preloadAllowed()) {
    if (_cold_entry) {
      deferSequentialPreload(kSeqPreloadGapMs);
    } else {
      preloadNeighbors();
    }
  }
}

void PhotoAlbumApp::tryEnableNavAfterPreload(void) {
  if (_closing || _nav_unrestricted) {
    return;
  }
  const int total = (int)_files.size();
  const int need = std::min(2, total);
  if (cacheCount() < need) {
    return;
  }
  if (_cold_entry) {
    if (_prev_btn != nullptr) {
      lv_obj_remove_flag(_prev_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_next_btn != nullptr) {
      lv_obj_remove_flag(_next_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  setNavEnabled(true);
  _nav_unrestricted = true;
  if (_nav_hold_timer != nullptr) {
    lv_timer_delete(_nav_hold_timer);
    _nav_hold_timer = nullptr;
  }
  ESP_LOGI(TAG, "nav enabled after preload, cache=%d", cacheCount());
}

void PhotoAlbumApp::scheduleSequentialPreload(void) {
  if (_closing || _files.size() <= 1 || _load_task != nullptr) {
    return;
  }

  if (_cold_entry && !_first_revealed) {
    return;
  }

  const int total = (int)_files.size();
  if (cacheCount() >= warmupTarget(total)) {
    updateWarmupState();
    tryEnableNavAfterPreload();
    return;
  }

  if (_last_nav_tick != 0 && lv_tick_elaps(_last_nav_tick) < navQuietMs()) {
    return;
  }

  for (int step = 1; step < total; step++) {
    const int idx = (_index + step) % total;
    if (cacheGet(_files[idx]) == nullptr) {
      beginLoadTask(idx, true);
      return;
    }
  }
  for (int step = 1; step < total; step++) {
    const int idx = (_index - step + total) % total;
    if (cacheGet(_files[idx]) == nullptr) {
      beginLoadTask(idx, true);
      return;
    }
  }

  updateWarmupState();
  tryEnableNavAfterPreload();
}

void PhotoAlbumApp::deferSequentialPreload(uint32_t delay_ms) {
  if (_closing) {
    return;
  }
  if (delay_ms == 0) {
    scheduleSequentialPreload();
    return;
  }
  if (_seq_preload_timer != nullptr) {
    lv_timer_delete(_seq_preload_timer);
  }
  _seq_preload_timer =
      lv_timer_create(seq_preload_timer_cb, delay_ms, this);
  lv_timer_set_repeat_count(_seq_preload_timer, 1);
}

void PhotoAlbumApp::stopDecodeTask(void) {
  album_decode_queue_reset();
  _load_task = nullptr;
}

void PhotoAlbumApp::markDecodeIdle(void) {
  if (s_album_decode_queue != nullptr &&
      uxQueueMessagesWaiting(s_album_decode_queue) == 0) {
    _load_task = nullptr;
  }
}

void PhotoAlbumApp::dispatchLoadResult(AlbumLoadResult *result) {
  if (result == nullptr) {
    return;
  }
  if (esp_lv_adapter_lock(pdMS_TO_TICKS(2000)) != ESP_OK) {
    ESP_LOGE(TAG, "adapter lock failed for load result");
    if (result->buf != nullptr) {
      photo_jpeg_free_buf(result->buf, result->hw_buf);
    }
    delete result;
    markDecodeIdle();
    return;
  }
  const lv_result_t ar = lv_async_call(async_apply_cb, result);
  esp_lv_adapter_unlock();
  if (ar != LV_RESULT_OK) {
    ESP_LOGE(TAG, "lv_async_call failed");
    if (result->buf != nullptr) {
      photo_jpeg_free_buf(result->buf, result->hw_buf);
    }
    delete result;
    markDecodeIdle();
  }
}

void PhotoAlbumApp::cancelAllTimers(void) {
  if (_defer_load_timer != nullptr) {
    lv_timer_delete(_defer_load_timer);
    _defer_load_timer = nullptr;
  }
  if (_show_timer != nullptr) {
    lv_timer_delete(_show_timer);
    _show_timer = nullptr;
  }
  if (_quiet_end_timer != nullptr) {
    lv_timer_delete(_quiet_end_timer);
    _quiet_end_timer = nullptr;
  }
  if (_nav_hold_timer != nullptr) {
    lv_timer_delete(_nav_hold_timer);
    _nav_hold_timer = nullptr;
  }
  if (_seq_preload_timer != nullptr) {
    lv_timer_delete(_seq_preload_timer);
    _seq_preload_timer = nullptr;
  }
  if (_chrome_reveal_timer != nullptr) {
    lv_timer_delete(_chrome_reveal_timer);
    _chrome_reveal_timer = nullptr;
  }
}

void PhotoAlbumApp::stripForSnapshot(void) {
  _reload_pending = false;
  stopDecodeTask();
  cancelAllTimers();

  lv_obj_t *screen = lv_screen_active();
  if (screen != nullptr) {
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  }

  if (_entry_overlay != nullptr) {
    lv_obj_add_flag(_entry_overlay, LV_OBJ_FLAG_HIDDEN);
  }
  if (_title != nullptr) {
    lv_obj_add_flag(_title, LV_OBJ_FLAG_HIDDEN);
  }
  if (_prev_btn != nullptr) {
    lv_obj_add_flag(_prev_btn, LV_OBJ_FLAG_HIDDEN);
  }
  if (_next_btn != nullptr) {
    lv_obj_add_flag(_next_btn, LV_OBJ_FLAG_HIDDEN);
  }

  if (_img != nullptr) {
    lv_display_t *disp = lv_obj_get_display(_img);
    if (disp != nullptr) {
      lv_display_enable_invalidation(disp, false);
    }
    lv_obj_add_flag(_img, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(_img, nullptr);
    if (disp != nullptr) {
      lv_display_enable_invalidation(disp, true);
    }
  }

  _decoded_buf = nullptr;
  memset(&_img_dsc, 0, sizeof(_img_dsc));
}

void PhotoAlbumApp::setNavEnabled(bool enabled) {
  if (_prev_btn != nullptr) {
    if (enabled) {
      lv_obj_remove_state(_prev_btn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(_prev_btn, LV_STATE_DISABLED);
    }
  }
  if (_next_btn != nullptr) {
    if (enabled) {
      lv_obj_remove_state(_next_btn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(_next_btn, LV_STATE_DISABLED);
    }
  }
}

bool PhotoAlbumApp::scanPhotos(void) {
  _files.clear();

  if (!board_sd_is_mounted()) {
    ESP_LOGI(TAG, "SD retry before scan...");
    vTaskDelay(pdMS_TO_TICKS(300));
    if (board_sd_init() != ESP_OK) {
      ESP_LOGW(TAG, "SD not mounted");
      return false;
    }
  }

  DIR *dir = opendir(BOARD_SD_PHOTO_DIR);
  if (dir == nullptr) {
    return false;
  }

  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr) {
    const char *name = ent->d_name;
    const size_t len = strlen(name);
    if (len < 5) {
      continue;
    }
    if (strcasecmp(name + len - 4, ".jpg") == 0 ||
        strcasecmp(name + len - 5, ".jpeg") == 0) {
      _files.emplace_back(name);
    }
  }
  closedir(dir);

  std::sort(_files.begin(), _files.end());
  return !_files.empty();
}

bool PhotoAlbumApp::run(void) {
  lv_area_t area = getVisualArea();
  const int w = lv_area_get_width(&area);
  const int h = lv_area_get_height(&area);

  _closing = false;
  _reload_pending = false;
  _preload_ready = false;
  _warmup_done = false;
  _first_revealed = false;
  _nav_unrestricted = false;
  _load_is_background = false;
  _cold_entry = false;
  _chrome_revealed = false;
  _switch_count = 0;
  _first_reveal_tick = 0;
  _load_task = nullptr;
  _defer_load_timer = nullptr;
  _show_timer = nullptr;
  _quiet_end_timer = nullptr;
  _nav_hold_timer = nullptr;
  _seq_preload_timer = nullptr;
  _chrome_reveal_timer = nullptr;
  _last_nav_tick = 0;
  _prev_btn = nullptr;
  _next_btn = nullptr;
  _entry_overlay = nullptr;
  _session++;

  static constexpr int kNavH = 40;
  const int side = (w < h) ? w : h;
  _img_area_w = side;
  _img_area_h = side;
  _img_y = (h - side) / 2;

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  _img = lv_image_create(lv_screen_active());
  lv_obj_set_size(_img, _img_area_w, _img_area_h);
  lv_obj_align(_img, LV_ALIGN_TOP_MID, 0, _img_y);
  lv_obj_add_flag(_img, LV_OBJ_FLAG_HIDDEN);

  _title = lv_label_create(lv_screen_active());
  lv_obj_set_width(_title, side - 16);
  lv_label_set_long_mode(_title, LV_LABEL_LONG_DOT);
  lv_obj_align(_title, LV_ALIGN_TOP_MID, 0, _img_y + 4);
  lv_obj_set_style_bg_color(_title, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(_title, LV_OPA_50, 0);
  lv_obj_set_style_pad_hor(_title, 6, 0);
  lv_obj_set_style_pad_ver(_title, 2, 0);
  lv_obj_set_style_radius(_title, 4, 0);

  if (!scanPhotos()) {
    lv_label_set_text(_title, "No JPG in /sdcard/photos/");
    return true;
  }

  _index = 0;
  lv_label_set_text(_title, "Loading...");

  const int total = (int)_files.size();
  if (cacheCount() >= warmupTarget(total)) {
    _warmup_done = true;
    _preload_ready = true;
    _first_revealed = true;
    _nav_unrestricted = true;
    ESP_LOGI(TAG, "reuse cache on entry, count=%d", cacheCount());
  } else {
    _cold_entry = true;
  }

  _prev_btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(_prev_btn, 72, kNavH);
  lv_obj_align(_prev_btn, LV_ALIGN_TOP_LEFT, 8, _img_y + side - kNavH - 6);
  lv_obj_t *pl = lv_label_create(_prev_btn);
  lv_label_set_text(pl, LV_SYMBOL_LEFT);
  lv_obj_center(pl);
  lv_obj_add_event_cb(_prev_btn, nav_btn_cb, LV_EVENT_CLICKED, this);

  _next_btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(_next_btn, 72, kNavH);
  lv_obj_align(_next_btn, LV_ALIGN_TOP_RIGHT, -8, _img_y + side - kNavH - 6);
  lv_obj_t *nl = lv_label_create(_next_btn);
  lv_label_set_text(nl, LV_SYMBOL_RIGHT);
  lv_obj_center(nl);
  lv_obj_add_event_cb(_next_btn, nav_btn_cb, LV_EVENT_CLICKED, this);
  setNavEnabled(_nav_unrestricted);

  if (_cold_entry) {
    lv_obj_add_flag(_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_prev_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_next_btn, LV_OBJ_FLAG_HIDDEN);

    _entry_overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(_entry_overlay);
    lv_obj_set_size(_entry_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_entry_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_entry_overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_entry_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_entry_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(_entry_overlay);
  }

  _defer_load_timer =
      lv_timer_create(defer_load_timer_cb, kEntryLoadDeferMs, this);
  lv_timer_set_repeat_count(_defer_load_timer, 1);

  return true;
}

void PhotoAlbumApp::defer_load_timer_cb(lv_timer_t *t) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_timer_get_user_data(t));
  if (app == nullptr) {
    return;
  }
  app->_defer_load_timer = nullptr;
  lv_timer_delete(t);
  if (!app->_closing) {
    app->scheduleLoad();
  }
}

void PhotoAlbumApp::show_timer_cb(lv_timer_t *t) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_timer_get_user_data(t));
  if (app == nullptr) {
    return;
  }
  app->_show_timer = nullptr;
  lv_timer_delete(t);
  app->revealImage();
}

void PhotoAlbumApp::seq_preload_timer_cb(lv_timer_t *t) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_timer_get_user_data(t));
  if (app == nullptr) {
    return;
  }
  app->_seq_preload_timer = nullptr;
  lv_timer_delete(t);
  if (!app->_closing) {
    app->scheduleSequentialPreload();
  }
}

void PhotoAlbumApp::chrome_reveal_timer_cb(lv_timer_t *t) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_timer_get_user_data(t));
  if (app == nullptr) {
    return;
  }
  app->_chrome_reveal_timer = nullptr;
  lv_timer_delete(t);
  if (!app->_closing) {
    app->revealChrome();
  }
}

void PhotoAlbumApp::revealChrome(void) {
  if (_closing || !_cold_entry || _chrome_revealed || _title == nullptr) {
    return;
  }
  _chrome_revealed = true;

  char title[96];
  snprintf(title, sizeof(title), "%s (%d/%d)", _files[_index].c_str(),
           _index + 1, (int)_files.size());
  lv_label_set_text(_title, title);
  lv_obj_remove_flag(_title, LV_OBJ_FLAG_HIDDEN);
}

void PhotoAlbumApp::revealImage(void) {
  if (_closing || _img == nullptr) {
    return;
  }

  if (_entry_overlay != nullptr) {
    lv_obj_add_flag(_entry_overlay, LV_OBJ_FLAG_HIDDEN);
  }

  lv_display_t *disp = lv_obj_get_display(_img);
  if (disp != nullptr) {
    lv_display_enable_invalidation(disp, false);
  }
  lv_obj_remove_flag(_img, LV_OBJ_FLAG_HIDDEN);
  if (disp != nullptr) {
    lv_display_enable_invalidation(disp, true);
  }
  lv_obj_invalidate(_img);

  if (!_cold_entry) {
    if (_title != nullptr) {
      lv_obj_remove_flag(_title, LV_OBJ_FLAG_HIDDEN);
    }
    if (_prev_btn != nullptr) {
      lv_obj_remove_flag(_prev_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_next_btn != nullptr) {
      lv_obj_remove_flag(_next_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (!_first_revealed) {
    _first_revealed = true;
    _first_reveal_tick = lv_tick_get();
    setNavEnabled(false);
    if (_nav_hold_timer != nullptr) {
      lv_timer_delete(_nav_hold_timer);
    }
    _nav_hold_timer = lv_timer_create(nav_hold_timer_cb, kNavHoldMaxMs, this);
    lv_timer_set_repeat_count(_nav_hold_timer, 1);
    if (_cold_entry) {
      ESP_LOGI(TAG, "first image revealed, staged chrome + preload");
      if (_chrome_reveal_timer != nullptr) {
        lv_timer_delete(_chrome_reveal_timer);
      }
      _chrome_reveal_timer = lv_timer_create(chrome_reveal_timer_cb,
                                             kChromeRevealDelayMs, this);
      lv_timer_set_repeat_count(_chrome_reveal_timer, 1);
      deferSequentialPreload(kSeqPreloadAfterRevealMs);
    } else {
      ESP_LOGI(TAG, "first image revealed, sequential preload");
      scheduleSequentialPreload();
    }
  } else if (_nav_unrestricted) {
    setNavEnabled(true);
  }
  updateWarmupState();
}

void PhotoAlbumApp::nav_hold_timer_cb(lv_timer_t *t) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_timer_get_user_data(t));
  if (app == nullptr) {
    return;
  }
  app->_nav_hold_timer = nullptr;
  lv_timer_delete(t);
  if (app->_closing) {
    return;
  }
  if (app->_cold_entry) {
    if (app->_prev_btn != nullptr) {
      lv_obj_remove_flag(app->_prev_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (app->_next_btn != nullptr) {
      lv_obj_remove_flag(app->_next_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  app->setNavEnabled(true);
  app->_nav_unrestricted = true;
  app->scheduleSequentialPreload();
}

void PhotoAlbumApp::quiet_end_timer_cb(lv_timer_t *t) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_timer_get_user_data(t));
  if (app == nullptr) {
    return;
  }
  app->_quiet_end_timer = nullptr;
  lv_timer_delete(t);
  if (!app->_closing) {
    app->updateWarmupState();
    app->scheduleSequentialPreload();
    if (app->preloadAllowed()) {
      app->preloadNeighbors();
    }
  }
}

void PhotoAlbumApp::async_apply_cb(void *user_data) {
  auto *result = static_cast<AlbumLoadResult *>(user_data);
  if (result != nullptr && result->app != nullptr) {
    result->app->applyLoadResult(result);
  }
}

void PhotoAlbumApp::decode_worker(void *arg) {
  (void)arg;
  AlbumDecodeJob job{};
  for (;;) {
    if (xQueueReceive(s_album_decode_queue, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (job.app == nullptr) {
      continue;
    }
    job.app->runDecodeWorker(job.session, job.index);
  }
}

bool PhotoAlbumApp::spawnDecodeTask(uint32_t session, int idx) {
  if (!ensure_album_decode_worker()) {
    return false;
  }
  AlbumDecodeJob job{this, session, idx};
  _load_task = s_album_decode_worker;
  if (xQueueSend(s_album_decode_queue, &job, 0) != pdTRUE) {
    _load_task = nullptr;
    return false;
  }
  return true;
}

bool PhotoAlbumApp::tryShowCached(int idx) {
  if (_closing || _img == nullptr || _title == nullptr) {
    return false;
  }
  if (idx < 0 || idx >= (int)_files.size()) {
    return false;
  }

  CachedImage *c = cacheGet(_files[idx]);
  if (c == nullptr || c->w == 0 || c->h == 0) {
    return false;
  }

  showDecodedImage(idx, c->buf, c->w, c->h);
  ESP_LOGI(TAG, "cache hit %s (%ux%u)", _files[idx].c_str(), c->w, c->h);
  return true;
}

void PhotoAlbumApp::showDecodedImage(int idx, uint8_t *buf, uint16_t w,
                                     uint16_t h) {
  if (_img == nullptr || _title == nullptr) {
    return;
  }

  if (_show_timer != nullptr) {
    lv_timer_delete(_show_timer);
    _show_timer = nullptr;
  }

  lv_obj_add_flag(_img, LV_OBJ_FLAG_HIDDEN);

  _decoded_buf = buf;

  const uint32_t data_size = (uint32_t)w * h * 2;
  _img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  _img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
  _img_dsc.header.flags = 0;
  _img_dsc.header.w = w;
  _img_dsc.header.h = h;
  _img_dsc.header.stride = w * 2;
  _img_dsc.data_size = data_size;
  _img_dsc.data = buf;

  lv_display_t *disp = lv_obj_get_display(_img);
  if (disp != nullptr) {
    lv_display_enable_invalidation(disp, false);
  }
  lv_image_set_src(_img, &_img_dsc);
  if (disp != nullptr) {
    lv_display_enable_invalidation(disp, true);
  }

  lv_obj_set_size(_img, _img_area_w, _img_area_h);
  lv_obj_align(_img, LV_ALIGN_TOP_MID, 0, _img_y);
  if (w == (uint16_t)_img_area_w && h == (uint16_t)_img_area_h) {
    lv_image_set_inner_align(_img, LV_IMAGE_ALIGN_DEFAULT);
  } else {
    lv_image_set_inner_align(_img, LV_IMAGE_ALIGN_STRETCH);
  }

  char title[96];
  snprintf(title, sizeof(title), "%s (%d/%d)", _files[idx].c_str(), idx + 1,
           (int)_files.size());
  if (!_cold_entry || _first_revealed) {
    lv_label_set_text(_title, title);
  }

  _show_timer = lv_timer_create(show_timer_cb, showDeferMs(), this);
  lv_timer_set_repeat_count(_show_timer, 1);
}

void PhotoAlbumApp::scheduleLoad(void) {
  if (_closing || _files.empty() || _img == nullptr || _title == nullptr) {
    return;
  }

  if (tryShowCached(_index)) {
    _reload_pending = false;
    updateWarmupState();
    if (preloadAllowed()) {
      preloadNeighbors();
    } else {
      scheduleSequentialPreload();
    }
    return;
  }

  if (_load_task != nullptr) {
    _reload_pending = true;
    return;
  }

  beginLoadTask(_index);
}

void PhotoAlbumApp::beginLoadTask(int idx, bool background) {
  if (_closing || _files.empty() || idx < 0 || idx >= (int)_files.size()) {
    return;
  }

  if (idx != _index && !background && !preloadAllowed()) {
    return;
  }

  if (cacheGet(_files[idx]) != nullptr) {
    if (idx == _index) {
      tryShowCached(idx);
      updateWarmupState();
      if (preloadAllowed()) {
        preloadNeighbors();
      } else {
        scheduleSequentialPreload();
      }
    }
    return;
  }

  _task_index = idx;
  _load_is_background = background;
  if (idx == _index) {
    lv_label_set_text(_title, "Loading...");
    lv_obj_add_flag(_img, LV_OBJ_FLAG_HIDDEN);
  }

  if (!spawnDecodeTask(_session, idx)) {
    _load_task = nullptr;
    if (idx == _index) {
      lv_label_set_text(_title, "Busy, retrying...");
    }
    ESP_LOGE(TAG, "decode worker create failed (stack=%u int=%u ext=%u B)",
             (unsigned)kDecodeStackBytes,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (_defer_load_timer == nullptr && !_closing) {
      _defer_load_timer =
          lv_timer_create(defer_load_timer_cb, kLoadRetryMs, this);
      lv_timer_set_repeat_count(_defer_load_timer, 1);
    }
    return;
  }
}

void PhotoAlbumApp::preloadNeighbors(void) {
  if (_closing || !preloadAllowed() || _files.size() <= 1 ||
      _load_task != nullptr) {
    return;
  }

  const int count = (int)_files.size();
  const int next = (_index + 1) % count;
  if (cacheGet(_files[next]) == nullptr) {
    beginLoadTask(next);
    return;
  }

  const int prev = (_index - 1 + count) % count;
  if (cacheGet(_files[prev]) == nullptr) {
    beginLoadTask(prev);
  }
}

void PhotoAlbumApp::runDecodeWorker(uint32_t session, int idx) {
  const bool background = _load_is_background;
  auto *result = new AlbumLoadResult{this, session, idx, nullptr, 0, 0, false,
                                     false};

  if (_closing || session != _session || idx < 0 || idx >= (int)_files.size()) {
    dispatchLoadResult(result);
    return;
  }

  if (idx != _index && !preloadAllowed() && !background) {
    dispatchLoadResult(result);
    return;
  }

  char posix[160];
  snprintf(posix, sizeof(posix), "%s/%s", BOARD_SD_PHOTO_DIR,
           _files[idx].c_str());

  result->ok = photo_jpeg_load_file(posix, &result->buf, &result->w, &result->h,
                                    &result->hw_buf);
  dispatchLoadResult(result);
}

void PhotoAlbumApp::applyLoadResult(AlbumLoadResult *result) {
  if (result == nullptr) {
    return;
  }

  const bool stale =
      _closing || result->session != _session || result->app != this;

  if (!stale && result->ok && result->buf != nullptr && result->w > 0 &&
      result->h > 0) {
    const int idx = result->index;
    cachePut(_files[idx], result->buf, result->w, result->h, result->hw_buf);
    result->buf = nullptr;

    if (idx == _index) {
      CachedImage *c = cacheGet(_files[idx]);
      if (c != nullptr) {
        showDecodedImage(idx, c->buf, c->w, c->h);
        ESP_LOGI(TAG, "show %s (%ux%u)", _files[idx].c_str(), c->w, c->h);
      }
    } else if (_load_is_background || preloadAllowed()) {
      ESP_LOGI(TAG, "preloaded %s (%ux%u)", _files[idx].c_str(), result->w,
               result->h);
    }
    updateWarmupState();
    tryEnableNavAfterPreload();
  } else if (!stale && !result->ok && result->index == _index) {
    char title[96];
    snprintf(title, sizeof(title), "Decode fail: %s",
             _files[result->index].c_str());
    lv_label_set_text(_title, title);
    ESP_LOGE(TAG, "decode failed: %s", _files[result->index].c_str());
  }

  if (result->buf != nullptr) {
    photo_jpeg_free_buf(result->buf, result->hw_buf);
  }
  delete result;

  if (_closing) {
    markDecodeIdle();
    return;
  }

  if (_reload_pending && !_closing) {
    _reload_pending = false;
    if (!tryShowCached(_index)) {
      beginLoadTask(_index);
    }
    markDecodeIdle();
    return;
  }

  if (preloadAllowed()) {
    if (_cold_entry) {
      deferSequentialPreload(kSeqPreloadGapMs);
    } else {
      preloadNeighbors();
    }
  } else if (_cold_entry && !_warmup_done && _first_revealed &&
             _load_is_background) {
    deferSequentialPreload(kSeqPreloadGapMs);
  } else {
    scheduleSequentialPreload();
  }
  _load_is_background = false;
  markDecodeIdle();
}

void PhotoAlbumApp::nav_btn_cb(lv_event_t *e) {
  auto *app = static_cast<PhotoAlbumApp *>(lv_event_get_user_data(e));
  if (app->_closing || app->_files.empty()) {
    return;
  }

  const int count = (int)app->_files.size();
  lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
  if (lv_obj_get_x(btn) < lv_obj_get_width(lv_obj_get_parent(btn)) / 2) {
    app->_index = (app->_index - 1 + count) % count;
  } else {
    app->_index = (app->_index + 1) % count;
  }
  app->_switch_count++;
  app->_last_nav_tick = lv_tick_get();

  app->_session++;
  album_decode_queue_reset();
  app->_load_task = nullptr;
  app->_reload_pending = false;

  if (app->_quiet_end_timer != nullptr) {
    lv_timer_delete(app->_quiet_end_timer);
    app->_quiet_end_timer = nullptr;
  }
  app->_quiet_end_timer =
      lv_timer_create(quiet_end_timer_cb, app->navQuietMs(), app);
  lv_timer_set_repeat_count(app->_quiet_end_timer, 1);

  app->scheduleLoad();
}

bool PhotoAlbumApp::pause(void) {
  _session++;
  stripForSnapshot();
  photo_album_release_cache();
  ESP_LOGI(TAG, "paused, stripped for recents snapshot");
  return true;
}

bool PhotoAlbumApp::resume(void) {
  if (_closing || _files.empty() || _img == nullptr) {
    return true;
  }

  if (_title != nullptr) {
    lv_obj_remove_flag(_title, LV_OBJ_FLAG_HIDDEN);
  }
  if (_prev_btn != nullptr) {
    lv_obj_remove_flag(_prev_btn, LV_OBJ_FLAG_HIDDEN);
  }
  if (_next_btn != nullptr) {
    lv_obj_remove_flag(_next_btn, LV_OBJ_FLAG_HIDDEN);
  }
  setNavEnabled(_nav_unrestricted);

  if (!tryShowCached(_index)) {
    scheduleLoad();
  }
  ESP_LOGI(TAG, "resumed at index=%d", _index);
  return true;
}

bool PhotoAlbumApp::back(void) {
  stripForSnapshot();
  notifyCoreClosed();
  return true;
}

bool PhotoAlbumApp::close(void) {
  _closing = true;
  _session++;
  _preload_ready = false;
  _warmup_done = false;

  stripForSnapshot();
  photo_album_release_cache();
  ESP_LOGI(TAG, "cache released on close");

  _files.clear();
  _img = nullptr;
  _title = nullptr;
  _prev_btn = nullptr;
  _next_btn = nullptr;
  _entry_overlay = nullptr;
  _decoded_buf = nullptr;
  memset(&_img_dsc, 0, sizeof(_img_dsc));
  return true;
}

#pragma once

#include <string>
#include <vector>

#include "esp_brookesia.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class PhotoAlbumApp;

struct AlbumLoadResult {
  PhotoAlbumApp *app;
  uint32_t session;
  int index;
  uint8_t *buf;
  uint16_t w;
  uint16_t h;
  bool ok;
  bool hw_buf;
};

class PhotoAlbumApp : public ESP_Brookesia_PhoneApp {
public:
  PhotoAlbumApp();
  ~PhotoAlbumApp() override;

  bool run(void) override;
  bool pause(void) override;
  bool resume(void) override;
  bool back(void) override;
  bool close(void) override;

  static void decode_worker(void *arg);

private:
  std::vector<std::string> _files;
  int _index = 0;
  lv_obj_t *_img = nullptr;
  lv_obj_t *_title = nullptr;
  lv_obj_t *_prev_btn = nullptr;
  lv_obj_t *_next_btn = nullptr;
  lv_obj_t *_entry_overlay = nullptr;
  int _img_area_w = 0;
  int _img_area_h = 0;
  int _img_y = 0;

  uint8_t *_decoded_buf = nullptr;
  lv_image_dsc_t _img_dsc{};

  TaskHandle_t _load_task = nullptr;
  int _task_index = 0;
  uint32_t _session = 0;
  bool _reload_pending = false;
  bool _closing = false;
  bool _preload_ready = false;
  bool _warmup_done = false;
  bool _first_revealed = false;
  bool _nav_unrestricted = false;
  bool _load_is_background = false;
  bool _cold_entry = false;
  bool _chrome_revealed = false;

  int _switch_count = 0;
  uint32_t _last_nav_tick = 0;
  uint32_t _first_reveal_tick = 0;

  lv_timer_t *_defer_load_timer = nullptr;
  lv_timer_t *_show_timer = nullptr;
  lv_timer_t *_quiet_end_timer = nullptr;
  lv_timer_t *_nav_hold_timer = nullptr;
  lv_timer_t *_seq_preload_timer = nullptr;
  lv_timer_t *_chrome_reveal_timer = nullptr;

  int cacheCount(void) const;
  bool warmupComplete(void) const;
  bool preloadAllowed(void) const;
  uint32_t navQuietMs(void) const;
  uint32_t showDeferMs(void) const;
  void updateWarmupState(void);
  void setNavEnabled(bool enabled);
  void tryEnableNavAfterPreload(void);
  void scheduleSequentialPreload(void);
  void deferSequentialPreload(uint32_t delay_ms);
  void revealImage(void);
  void revealChrome(void);
  void stopDecodeTask(void);
  void cancelAllTimers(void);
  void stripForSnapshot(void);

  bool scanPhotos(void);
  void scheduleLoad(void);
  void beginLoadTask(int idx, bool background = false);
  void preloadNeighbors(void);
  void runDecodeWorker(uint32_t session, int idx);
  void dispatchLoadResult(AlbumLoadResult *result);
  void markDecodeIdle(void);
  void applyLoadResult(AlbumLoadResult *result);
  bool tryShowCached(int idx);
  void showDecodedImage(int idx, uint8_t *buf, uint16_t w, uint16_t h);

  bool spawnDecodeTask(uint32_t session, int idx);

  static void async_apply_cb(void *user_data);
  static void defer_load_timer_cb(lv_timer_t *t);
  static void show_timer_cb(lv_timer_t *t);
  static void quiet_end_timer_cb(lv_timer_t *t);
  static void nav_hold_timer_cb(lv_timer_t *t);
  static void seq_preload_timer_cb(lv_timer_t *t);
  static void chrome_reveal_timer_cb(lv_timer_t *t);
  static void nav_btn_cb(lv_event_t *e);
};

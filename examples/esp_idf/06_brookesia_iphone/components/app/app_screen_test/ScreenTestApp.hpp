#pragma once

#include "esp_brookesia.hpp"

class ScreenTestApp : public ESP_Brookesia_PhoneApp {
public:
  ScreenTestApp();
  ~ScreenTestApp() override = default;

  bool run(void) override;
  bool back(void) override;
  bool close(void) override;

private:
  static constexpr uint32_t kAutoIntervalMs = 2000;

  lv_obj_t *_panel = nullptr;
  lv_obj_t *_hint = nullptr;
  lv_obj_t *_prev_btn = nullptr;
  lv_obj_t *_next_btn = nullptr;
  lv_obj_t *_play_btn = nullptr;
  lv_obj_t *_play_lbl = nullptr;
  lv_timer_t *_auto_timer = nullptr;
  int _pattern = 0;
  bool _auto_play = true;

  void showPattern(void);
  void toggleAutoPlay(void);
  void updateHint(void);
  void updatePlayButton(void);

  static void btn_event_cb(lv_event_t *e);
  static void auto_timer_cb(lv_timer_t *timer);
};

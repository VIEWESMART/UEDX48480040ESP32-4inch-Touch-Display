#include "ScreenTestApp.hpp"

#include "app_icons.h"

#include <cstdio>

static const char *kPatterns[] = {"Red", "Green", "Blue", "White", "Black",
                                  "Gray bars"};
static const int kPatternCount =
    sizeof(kPatterns) / sizeof(kPatterns[0]);

ScreenTestApp::ScreenTestApp()
    : ESP_Brookesia_PhoneApp("Screen Test", &galleria_app_icon_display, true,
                             true, false) {}

bool ScreenTestApp::run(void) {
  lv_area_t area = getVisualArea();
  const int w = lv_area_get_width(&area);
  const int h = lv_area_get_height(&area);

  _panel = lv_obj_create(lv_screen_active());
  lv_obj_remove_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(_panel, w, h - 56);
  lv_obj_align(_panel, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_radius(_panel, 0, 0);
  lv_obj_set_style_border_width(_panel, 0, 0);
  lv_obj_set_style_pad_all(_panel, 0, 0);

  _hint = lv_label_create(lv_screen_active());
  lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -36);

  _prev_btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(_prev_btn, 72, 36);
  lv_obj_align(_prev_btn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_obj_t *prev_lbl = lv_label_create(_prev_btn);
  lv_label_set_text(prev_lbl, LV_SYMBOL_LEFT);
  lv_obj_center(prev_lbl);
  lv_obj_add_event_cb(_prev_btn, btn_event_cb, LV_EVENT_CLICKED, this);

  _play_btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(_play_btn, 72, 36);
  lv_obj_align(_play_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
  _play_lbl = lv_label_create(_play_btn);
  lv_obj_center(_play_lbl);
  lv_obj_add_event_cb(_play_btn, btn_event_cb, LV_EVENT_CLICKED, this);

  _next_btn = lv_button_create(lv_screen_active());
  lv_obj_set_size(_next_btn, 72, 36);
  lv_obj_align(_next_btn, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
  lv_obj_t *next_lbl = lv_label_create(_next_btn);
  lv_label_set_text(next_lbl, LV_SYMBOL_RIGHT);
  lv_obj_center(next_lbl);
  lv_obj_add_event_cb(_next_btn, btn_event_cb, LV_EVENT_CLICKED, this);

  _pattern = 0;
  _auto_play = true;
  updatePlayButton();
  showPattern();

  _auto_timer = lv_timer_create(auto_timer_cb, kAutoIntervalMs, this);
  return true;
}

void ScreenTestApp::showPattern(void) {
  _pattern = (_pattern % kPatternCount + kPatternCount) % kPatternCount;
  updateHint();

  lv_obj_clean(_panel);
  lv_obj_set_style_bg_opa(_panel, LV_OPA_COVER, 0);

  switch (_pattern) {
  case 0:
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0xFF0000), 0);
    break;
  case 1:
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x00FF00), 0);
    break;
  case 2:
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x0000FF), 0);
    break;
  case 3:
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0xFFFFFF), 0);
    break;
  case 4:
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x000000), 0);
    break;
  default: {
    const int pw = lv_obj_get_width(_panel);
    const int ph = lv_obj_get_height(_panel);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x808080), 0);
    for (int i = 0; i < 8; i++) {
      lv_obj_t *bar = lv_obj_create(_panel);
      lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_size(bar, pw / 8, ph);
      lv_obj_set_pos(bar, i * (pw / 8), 0);
      lv_obj_set_style_radius(bar, 0, 0);
      lv_obj_set_style_border_width(bar, 0, 0);
      lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
      const uint8_t g = (uint8_t)(255 * i / 7);
      lv_obj_set_style_bg_color(bar, lv_color_make(g, g, g), 0);
    }
    break;
  }
  }
}

void ScreenTestApp::updateHint(void) {
  if (_hint == nullptr) {
    return;
  }
  char hint[64];
  snprintf(hint, sizeof(hint), "%s (%d/%d) %s", kPatterns[_pattern],
           _pattern + 1, kPatternCount,
           _auto_play ? "[Auto]" : "[Paused]");
  lv_label_set_text(_hint, hint);
}

void ScreenTestApp::updatePlayButton(void) {
  if (_play_lbl == nullptr) {
    return;
  }
  lv_label_set_text(_play_lbl, _auto_play ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

void ScreenTestApp::toggleAutoPlay(void) {
  _auto_play = !_auto_play;
  updatePlayButton();
  updateHint();
  if (_auto_timer == nullptr) {
    return;
  }
  if (_auto_play) {
    lv_timer_resume(_auto_timer);
  } else {
    lv_timer_pause(_auto_timer);
  }
}

void ScreenTestApp::btn_event_cb(lv_event_t *e) {
  auto *app = static_cast<ScreenTestApp *>(lv_event_get_user_data(e));
  lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));

  if (btn == app->_prev_btn) {
    app->_pattern--;
    app->showPattern();
  } else if (btn == app->_next_btn) {
    app->_pattern++;
    app->showPattern();
  } else if (btn == app->_play_btn) {
    app->toggleAutoPlay();
  }
}

void ScreenTestApp::auto_timer_cb(lv_timer_t *timer) {
  auto *app = static_cast<ScreenTestApp *>(lv_timer_get_user_data(timer));
  if (app == nullptr || !app->_auto_play) {
    return;
  }
  app->_pattern++;
  app->showPattern();
}

bool ScreenTestApp::back(void) {
  notifyCoreClosed();
  return true;
}

bool ScreenTestApp::close(void) {
  if (_auto_timer != nullptr) {
    lv_timer_delete(_auto_timer);
    _auto_timer = nullptr;
  }
  _panel = nullptr;
  _hint = nullptr;
  _prev_btn = nullptr;
  _next_btn = nullptr;
  _play_btn = nullptr;
  _play_lbl = nullptr;
  return true;
}

#include "SettingsApp.hpp"

#include "app_icons.h"

#include <cstdio>
#include <cstring>

#include "board_backlight.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "photo_jpeg_load.h"

static const char *TAG = "Settings";

static constexpr uint32_t kBgScreen = 0x0a0a0a;
static constexpr uint32_t kBgPanel = 0x141414;
static constexpr uint32_t kBgMenu = 0x1c1c1c;
static constexpr uint32_t kBgMenuActive = 0x2a3a4a;
static constexpr uint32_t kTextMuted = 0x909090;

SettingsApp::SettingsApp()
    : ESP_Brookesia_PhoneApp("Settings", &galleria_app_icon_settings, true, true, false) {}

void SettingsApp::applyDarkStyle(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(kBgPanel), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_text_color(obj, lv_color_hex(0xe8e8e8), 0);
  lv_obj_set_style_radius(obj, 6, 0);
}

bool SettingsApp::run(void) {
  _closing = false;
  _scan_count = 0;
  _scan_task = nullptr;
  _page = Page::Backlight;

  lv_area_t area = getVisualArea();
  const int w = lv_area_get_width(&area);
  const int h = lv_area_get_height(&area);
  static constexpr int kMenuH = 132;

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(kBgScreen), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  _root = lv_obj_create(lv_screen_active());
  lv_obj_set_size(_root, w, h);
  lv_obj_align(_root, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(_root, lv_color_hex(kBgScreen), 0);
  lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_root, 0, 0);
  lv_obj_set_style_pad_all(_root, 8, 0);
  lv_obj_set_style_pad_row(_root, 8, 0);
  lv_obj_set_flex_flow(_root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(_root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_remove_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

  buildMenu(w);
  buildContentPages(w, h - kMenuH - 16);

  showPage(Page::Backlight);
  return true;
}

void SettingsApp::buildMenu(int w) {
  _menu_cont = lv_obj_create(_root);
  lv_obj_set_width(_menu_cont, w - 16);
  lv_obj_set_height(_menu_cont, LV_SIZE_CONTENT);
  applyDarkStyle(_menu_cont);
  lv_obj_set_style_bg_color(_menu_cont, lv_color_hex(kBgMenu), 0);
  lv_obj_set_style_pad_all(_menu_cont, 6, 0);
  lv_obj_set_style_pad_row(_menu_cont, 4, 0);
  lv_obj_set_flex_flow(_menu_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(_menu_cont, LV_OBJ_FLAG_SCROLLABLE);

  struct Item {
    const char *label;
    Page page;
  };
  static const Item items[] = {
      {"Backlight", Page::Backlight},
      {"WiFi", Page::WiFi},
      {"Device Info", Page::Device},
  };

  for (const Item &item : items) {
    lv_obj_t *btn = lv_button_create(_menu_cont);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, 36);
    lv_obj_set_style_bg_color(btn, lv_color_hex(kBgMenu), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(kBgMenuActive), LV_STATE_CHECKED);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, item.label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_set_user_data(btn, (void *)(uintptr_t)item.page);
    lv_obj_add_event_cb(btn, menu_btn_cb, LV_EVENT_CLICKED, this);

    if (item.page == Page::Backlight) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
    }
  }
}

void SettingsApp::buildContentPages(int w, int content_h) {
  _content_cont = lv_obj_create(_root);
  lv_obj_set_size(_content_cont, w - 16, content_h);
  applyDarkStyle(_content_cont);
  lv_obj_set_style_pad_all(_content_cont, 10, 0);
  lv_obj_remove_flag(_content_cont, LV_OBJ_FLAG_SCROLLABLE);

  _page_bl = lv_obj_create(_content_cont);
  lv_obj_set_size(_page_bl, lv_pct(100), lv_pct(100));
  applyDarkStyle(_page_bl);
  lv_obj_set_style_pad_all(_page_bl, 4, 0);
  lv_obj_remove_flag(_page_bl, LV_OBJ_FLAG_SCROLLABLE);
  buildBacklightPage(_page_bl, w - 40);

  _page_wifi = lv_obj_create(_content_cont);
  lv_obj_set_size(_page_wifi, lv_pct(100), lv_pct(100));
  applyDarkStyle(_page_wifi);
  lv_obj_set_style_pad_all(_page_wifi, 4, 0);
  lv_obj_set_flex_flow(_page_wifi, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(_page_wifi, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(_page_wifi, 8, 0);
  lv_obj_remove_flag(_page_wifi, LV_OBJ_FLAG_SCROLLABLE);
  buildWifiPage(_page_wifi, w - 40, content_h - 20);

  _page_info = lv_obj_create(_content_cont);
  lv_obj_set_size(_page_info, lv_pct(100), lv_pct(100));
  applyDarkStyle(_page_info);
  lv_obj_set_style_pad_all(_page_info, 4, 0);
  lv_obj_remove_flag(_page_info, LV_OBJ_FLAG_SCROLLABLE);
  buildInfoPage(_page_info, w - 40);
}

void SettingsApp::showPage(Page page) {
  _page = page;
  lv_obj_add_flag(_page_bl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_page_wifi, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(_page_info, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *active = _page_bl;
  if (page == Page::WiFi) {
    active = _page_wifi;
  } else if (page == Page::Device) {
    active = _page_info;
  }
  lv_obj_remove_flag(active, LV_OBJ_FLAG_HIDDEN);

  if (_wifi_status_timer != nullptr) {
    if (page == Page::WiFi) {
      lv_timer_resume(_wifi_status_timer);
    } else {
      lv_timer_pause(_wifi_status_timer);
    }
  }

  uint32_t idx = 0;
  for (lv_obj_t *btn = lv_obj_get_child(_menu_cont, idx); btn != nullptr;
       btn = lv_obj_get_child(_menu_cont, ++idx)) {
    lv_obj_remove_state(btn, LV_STATE_CHECKED);
    const uintptr_t p = (uintptr_t)lv_obj_get_user_data(btn);
    if (p == (uintptr_t)page) {
      lv_obj_add_state(btn, LV_STATE_CHECKED);
    }
  }
}

void SettingsApp::buildBacklightPage(lv_obj_t *parent, int w) {
  lv_obj_t *title = lv_label_create(parent);
  lv_label_set_text(title, "Brightness (GPIO38 PWM)");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  _bl_value = lv_label_create(parent);
  char pct[16];
  snprintf(pct, sizeof(pct), "%u%%", board_backlight_get_percent());
  lv_label_set_text(_bl_value, pct);
  lv_obj_align(_bl_value, LV_ALIGN_TOP_RIGHT, 0, 0);

  _bl_slider = lv_slider_create(parent);
  lv_obj_set_width(_bl_slider, w - 8);
  lv_slider_set_range(_bl_slider, 5, 100);
  lv_slider_set_value(_bl_slider, board_backlight_get_percent(), LV_ANIM_OFF);
  lv_obj_align(_bl_slider, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_add_event_cb(_bl_slider, backlight_slider_cb, LV_EVENT_VALUE_CHANGED,
                      this);

  lv_obj_t *hint = lv_label_create(parent);
  lv_label_set_text(hint, "Drag to adjust. Saved automatically.");
  lv_obj_set_width(hint, w - 8);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(hint, lv_color_hex(kTextMuted), 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 72);
}

void SettingsApp::buildWifiPage(lv_obj_t *parent, int w, int h) {
  lv_obj_t *status_box = lv_obj_create(parent);
  lv_obj_set_width(status_box, lv_pct(100));
  lv_obj_set_height(status_box, LV_SIZE_CONTENT);
  applyDarkStyle(status_box);
  lv_obj_set_style_bg_color(status_box, lv_color_hex(kBgMenu), 0);
  lv_obj_set_style_pad_all(status_box, 8, 0);
  lv_obj_remove_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);

  _wifi_status = lv_label_create(status_box);
  lv_obj_set_width(_wifi_status, w - 16);
  lv_label_set_long_mode(_wifi_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(_wifi_status, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(_wifi_status, lv_color_hex(0xcccccc), 0);
  updateWifiStatus();

  _wifi_status_timer = lv_timer_create(wifi_status_timer_cb, 2000, this);
  lv_timer_pause(_wifi_status_timer);

  lv_obj_t *scan_btn = lv_button_create(parent);
  lv_obj_set_width(scan_btn, lv_pct(100));
  lv_obj_set_height(scan_btn, 38);
  lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x2563a8), 0);
  lv_obj_t *scan_lbl = lv_label_create(scan_btn);
  lv_label_set_text(scan_lbl, LV_SYMBOL_REFRESH "  Scan Networks");
  lv_obj_center(scan_lbl);
  lv_obj_add_event_cb(scan_btn, wifi_scan_btn_cb, LV_EVENT_CLICKED, this);

  _wifi_list_cont = lv_obj_create(parent);
  lv_obj_set_width(_wifi_list_cont, lv_pct(100));
  lv_obj_set_height(_wifi_list_cont, h - 120);
  applyDarkStyle(_wifi_list_cont);
  lv_obj_set_style_bg_color(_wifi_list_cont, lv_color_hex(kBgMenu), 0);
  lv_obj_set_style_pad_all(_wifi_list_cont, 4, 0);
  lv_obj_add_flag(_wifi_list_cont, LV_OBJ_FLAG_HIDDEN);

  _wifi_list = lv_list_create(_wifi_list_cont);
  lv_obj_set_size(_wifi_list, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(_wifi_list, lv_color_hex(kBgMenu), 0);
  lv_obj_set_style_pad_row(_wifi_list, 2, 0);

  _wifi_connect_cont = lv_obj_create(parent);
  lv_obj_set_width(_wifi_connect_cont, lv_pct(100));
  lv_obj_set_height(_wifi_connect_cont, LV_SIZE_CONTENT);
  applyDarkStyle(_wifi_connect_cont);
  lv_obj_set_style_bg_color(_wifi_connect_cont, lv_color_hex(kBgMenu), 0);
  lv_obj_set_style_pad_all(_wifi_connect_cont, 8, 0);
  lv_obj_set_style_pad_row(_wifi_connect_cont, 6, 0);
  lv_obj_set_flex_flow(_wifi_connect_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_add_flag(_wifi_connect_cont, LV_OBJ_FLAG_HIDDEN);

  _connect_title = lv_label_create(_wifi_connect_cont);
  lv_obj_set_width(_connect_title, w - 24);
  lv_label_set_long_mode(_connect_title, LV_LABEL_LONG_DOT);
  lv_label_set_text(_connect_title, "");

  _pwd_preview_btn = lv_button_create(_wifi_connect_cont);
  lv_obj_set_width(_pwd_preview_btn, lv_pct(100));
  lv_obj_set_height(_pwd_preview_btn, 44);
  lv_obj_set_style_bg_color(_pwd_preview_btn, lv_color_hex(0x222222), 0);
  lv_obj_add_event_cb(_pwd_preview_btn, pwd_preview_cb, LV_EVENT_CLICKED, this);

  _pwd_preview_lbl = lv_label_create(_pwd_preview_btn);
  lv_label_set_text(_pwd_preview_lbl, "Tap to enter password");
  lv_obj_set_width(_pwd_preview_lbl, w - 48);
  lv_label_set_long_mode(_pwd_preview_lbl, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(_pwd_preview_lbl, lv_color_hex(kTextMuted), 0);
  lv_obj_align(_pwd_preview_lbl, LV_ALIGN_LEFT_MID, 8, 0);

  lv_obj_t *btn_row = lv_obj_create(_wifi_connect_cont);
  lv_obj_set_width(btn_row, lv_pct(100));
  lv_obj_set_height(btn_row, 40);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *conn = lv_button_create(btn_row);
  lv_obj_set_size(conn, (w - 56) / 2, 36);
  lv_obj_set_style_bg_color(conn, lv_color_hex(0x2563a8), 0);
  lv_obj_t *cl = lv_label_create(conn);
  lv_label_set_text(cl, "Connect");
  lv_obj_center(cl);
  lv_obj_add_event_cb(conn, connect_btn_cb, LV_EVENT_CLICKED, this);

  lv_obj_t *cancel = lv_button_create(btn_row);
  lv_obj_set_size(cancel, (w - 56) / 2, 36);
  lv_obj_set_style_bg_color(cancel, lv_color_hex(0x444444), 0);
  lv_obj_t *xl = lv_label_create(cancel);
  lv_label_set_text(xl, "Cancel");
  lv_obj_center(xl);
  lv_obj_add_event_cb(cancel, cancel_btn_cb, LV_EVENT_CLICKED, this);

  lv_area_t area = getVisualArea();
  const int vw = lv_area_get_width(&area);
  const int vh = lv_area_get_height(&area);
  static constexpr int kKbH = 240;

  _kb_overlay = lv_obj_create(lv_screen_active());
  lv_obj_set_size(_kb_overlay, vw, vh);
  lv_obj_align(_kb_overlay, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(_kb_overlay, lv_color_hex(kBgScreen), 0);
  lv_obj_set_style_bg_opa(_kb_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_kb_overlay, 0, 0);
  lv_obj_set_style_pad_all(_kb_overlay, 10, 0);
  lv_obj_remove_flag(_kb_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(_kb_overlay, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *kb_title = lv_label_create(_kb_overlay);
  lv_label_set_text(kb_title, "WiFi Password");
  lv_obj_set_style_text_font(kb_title, &lv_font_montserrat_18, 0);
  lv_obj_align(kb_title, LV_ALIGN_TOP_LEFT, 4, 4);

  lv_obj_t *done_btn = lv_button_create(_kb_overlay);
  lv_obj_set_size(done_btn, 72, 36);
  lv_obj_align(done_btn, LV_ALIGN_TOP_RIGHT, -4, 0);
  lv_obj_set_style_bg_color(done_btn, lv_color_hex(0x2563a8), 0);
  lv_obj_t *done_lbl = lv_label_create(done_btn);
  lv_label_set_text(done_lbl, "Done");
  lv_obj_center(done_lbl);
  lv_obj_add_event_cb(done_btn, kb_done_btn_cb, LV_EVENT_CLICKED, this);

  _pwd_ta = lv_textarea_create(_kb_overlay);
  lv_obj_set_width(_pwd_ta, vw - 32);
  lv_obj_set_height(_pwd_ta, 52);
  lv_obj_align(_pwd_ta, LV_ALIGN_TOP_MID, 0, 44);
  lv_textarea_set_one_line(_pwd_ta, true);
  lv_textarea_set_password_mode(_pwd_ta, false);
  lv_textarea_set_placeholder_text(_pwd_ta, "Enter password");
  lv_textarea_set_max_length(_pwd_ta, 63);
  lv_obj_set_style_bg_color(_pwd_ta, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_text_color(_pwd_ta, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_color(_pwd_ta, lv_color_hex(0x666666),
                              LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_text_font(_pwd_ta, &lv_font_montserrat_18, 0);
  lv_obj_set_style_pad_all(_pwd_ta, 10, 0);

  _pwd_kb = lv_keyboard_create(_kb_overlay);
  lv_obj_set_width(_pwd_kb, vw - 20);
  lv_obj_set_height(_pwd_kb, kKbH);
  lv_obj_align(_pwd_kb, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_keyboard_set_mode(_pwd_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(_pwd_kb, _pwd_ta);
  lv_obj_set_style_pad_row(_pwd_kb, 6, LV_PART_ITEMS);
  lv_obj_set_style_pad_column(_pwd_kb, 4, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(_pwd_kb, 8, LV_PART_ITEMS);
  lv_obj_set_style_text_font(_pwd_kb, &lv_font_montserrat_18, LV_PART_ITEMS);
  lv_obj_add_event_cb(_pwd_kb, kb_event_cb, LV_EVENT_READY, this);
  lv_obj_add_event_cb(_pwd_kb, kb_event_cb, LV_EVENT_CANCEL, this);
}

void SettingsApp::buildInfoPage(lv_obj_t *parent, int w) {
  lv_obj_t *info = lv_label_create(parent);
  lv_obj_set_width(info, w - 8);
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  lv_label_set_text(info,
                    "Model: UEDX48480040E-WB-A\n"
                    "Module: ESP32-S3-WROOM-1 (N16R8)\n"
                    "Display: 480x480\n"
                    "LCD IC: GC9503V\n"
                    "Touch IC: FT6336U");
  lv_obj_set_style_text_font(info, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(info, lv_color_hex(0xcccccc), 0);
  lv_obj_set_style_text_line_space(info, 8, 0);
  lv_obj_align(info, LV_ALIGN_TOP_LEFT, 0, 0);
}

void SettingsApp::updateWifiStatus(void) {
  if (_wifi_status == nullptr) {
    return;
  }
  char status[96];
  board_wifi_copy_status(status, sizeof(status));
  const char *cur = lv_label_get_text(_wifi_status);
  if (cur != nullptr && strcmp(cur, status) == 0) {
    return;
  }
  lv_label_set_text(_wifi_status, status);
}

void SettingsApp::wifi_status_timer_cb(lv_timer_t *timer) {
  auto *app = static_cast<SettingsApp *>(lv_timer_get_user_data(timer));
  if (app == nullptr || app->_closing || app->_scan_busy ||
      app->_page != Page::WiFi) {
    return;
  }
  app->updateWifiStatus();
}

void SettingsApp::showWifiList(bool show) {
  if (_wifi_list_cont == nullptr) {
    return;
  }
  if (show) {
    lv_obj_remove_flag(_wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(_wifi_list_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void SettingsApp::showWifiConnectPanel(bool show) {
  if (_wifi_connect_cont == nullptr) {
    return;
  }
  if (show) {
    lv_obj_remove_flag(_wifi_connect_cont, LV_OBJ_FLAG_HIDDEN);
  } else {
    hideKeyboardOverlay();
    lv_obj_add_flag(_wifi_connect_cont, LV_OBJ_FLAG_HIDDEN);
  }
}

void SettingsApp::updatePwdPreview(void) {
  if (_pwd_preview_lbl == nullptr || _pwd_ta == nullptr) {
    return;
  }
  const char *txt = lv_textarea_get_text(_pwd_ta);
  if (txt != nullptr && txt[0] != '\0') {
    lv_label_set_text(_pwd_preview_lbl, txt);
    lv_obj_set_style_text_color(_pwd_preview_lbl, lv_color_hex(0xffffff), 0);
  } else {
    lv_label_set_text(_pwd_preview_lbl, "Tap to enter password");
    lv_obj_set_style_text_color(_pwd_preview_lbl, lv_color_hex(kTextMuted), 0);
  }
}

void SettingsApp::showKeyboardOverlay(void) {
  if (_kb_overlay == nullptr || _pwd_ta == nullptr) {
    return;
  }
  lv_obj_remove_flag(_kb_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(_kb_overlay);
  lv_keyboard_set_textarea(_pwd_kb, _pwd_ta);
  lv_obj_add_state(_pwd_ta, LV_STATE_FOCUSED);
}

void SettingsApp::hideKeyboardOverlay(void) {
  if (_kb_overlay == nullptr) {
    return;
  }
  lv_obj_add_flag(_kb_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(_pwd_ta, LV_STATE_FOCUSED);
  updatePwdPreview();
}

void SettingsApp::clearWifiList(void) {
  if (_wifi_list == nullptr) {
    return;
  }
  while (lv_obj_get_child_count(_wifi_list) > 0) {
    lv_obj_delete(lv_obj_get_child(_wifi_list, 0));
  }
}

void SettingsApp::populateWifiList(void) {
  clearWifiList();
  if (_wifi_list == nullptr) {
    return;
  }

  if (_scan_count == 0) {
    lv_list_add_text(_wifi_list, "No networks found");
    return;
  }

  for (uint16_t i = 0; i < _scan_count; i++) {
    char line[72];
    const char *lock = (_scan_aps[i].auth == WIFI_AUTH_OPEN) ? LV_SYMBOL_WIFI
                                                             : LV_SYMBOL_EYE_CLOSE;
    snprintf(line, sizeof(line), "%s %s  %ddBm", lock, _scan_aps[i].ssid,
             (int)_scan_aps[i].rssi);

    lv_obj_t *btn = lv_list_add_button(_wifi_list, NULL, line);
    lv_obj_add_event_cb(btn, wifi_item_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(btn, (void *)(uintptr_t)i);
  }
}

void SettingsApp::openConnectPanel(const char *ssid, wifi_auth_mode_t auth) {
  strncpy(_selected_ssid, ssid, BOARD_WIFI_SSID_MAX);
  _selected_ssid[BOARD_WIFI_SSID_MAX] = '\0';
  _selected_auth = auth;

  showWifiList(false);

  char title[56];
  snprintf(title, sizeof(title), "Connect: %s", ssid);
  lv_label_set_text(_connect_title, title);

  if (auth == WIFI_AUTH_OPEN) {
    lv_obj_add_flag(_pwd_preview_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_remove_flag(_pwd_preview_btn, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(_pwd_ta, "");
    updatePwdPreview();
    hideKeyboardOverlay();
  }

  showWifiConnectPanel(true);
}

void SettingsApp::closeConnectPanel(void) {
  hideKeyboardOverlay();
  showWifiConnectPanel(false);
  _selected_ssid[0] = '\0';
}

void SettingsApp::startWifiScan(void) {
  if (_closing || _scan_busy || _scan_task != nullptr || _connect_task != nullptr) {
    return;
  }

  hideKeyboardOverlay();
  closeConnectPanel();

  _scan_busy = true;
  if (_wifi_status_timer != nullptr) {
    lv_timer_pause(_wifi_status_timer);
  }

  showWifiList(true);
  clearWifiList();
  lv_list_add_text(_wifi_list, "Scanning...");

  if (xTaskCreatePinnedToCore(scan_task_entry, "wifi_scan", 8192, this, 5,
                              &_scan_task, 1) != pdPASS) {
    _scan_task = nullptr;
    _scan_busy = false;
    if (_wifi_status_timer != nullptr) {
      lv_timer_resume(_wifi_status_timer);
    }
    clearWifiList();
    lv_list_add_text(_wifi_list, "Scan task failed");
    ESP_LOGE(TAG, "scan task create failed");
  }
}

void SettingsApp::scan_task_entry(void *arg) {
  auto *app = static_cast<SettingsApp *>(arg);
  auto *result = new (std::nothrow) ScanResult{};
  if (result == nullptr) {
    lv_async_call(
        [](void *p) {
          auto *a = static_cast<SettingsApp *>(p);
          if (a != nullptr && !a->_closing) {
            a->clearWifiList();
            lv_list_add_text(a->_wifi_list, "Out of memory");
          }
          if (a != nullptr) {
            a->_scan_busy = false;
            a->_scan_task = nullptr;
            if (a->_wifi_status_timer != nullptr) {
              lv_timer_resume(a->_wifi_status_timer);
            }
          }
        },
        app);
    vTaskDelete(nullptr);
    return;
  }

  result->app = app;
  result->count = 0;
  result->err = ESP_OK;

  if (!board_wifi_is_initialized()) {
    result->err = board_wifi_init();
  }
  if (result->err == ESP_OK) {
    const size_t psram_free =
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (psram_free < 256 * 1024) {
      photo_album_release_cache();
    }
    result->err =
        board_wifi_scan(result->aps, &result->count, BOARD_WIFI_SCAN_MAX);
  }

  lv_async_call(scan_async_cb, result);
  app->_scan_task = nullptr;
  vTaskDelete(nullptr);
}

void SettingsApp::scan_async_cb(void *user_data) {
  auto *result = static_cast<ScanResult *>(user_data);
  if (result == nullptr) {
    return;
  }

  SettingsApp *app = result->app;
  if (app == nullptr || app->_closing) {
    delete result;
    return;
  }

  if (result->err != ESP_OK) {
    app->clearWifiList();
    if (result->err == ESP_ERR_NO_MEM) {
      lv_list_add_text(app->_wifi_list, "Out of memory");
    } else if (!board_wifi_is_initialized()) {
      lv_list_add_text(app->_wifi_list, "WiFi init failed");
    } else {
      lv_list_add_text(app->_wifi_list, "Scan failed, retry");
    }
    ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(result->err));
  } else {
    app->_scan_count = result->count;
    memcpy(app->_scan_aps, result->aps,
           result->count * sizeof(board_wifi_ap_info_t));
    app->populateWifiList();
    ESP_LOGI(TAG, "scan done, %u APs", result->count);
  }

  app->_scan_busy = false;
  app->updateWifiStatus();
  if (app->_wifi_status_timer != nullptr) {
    lv_timer_resume(app->_wifi_status_timer);
  }

  delete result;
}

void SettingsApp::menu_btn_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  if (app->_closing) {
    return;
  }
  lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
  app->showPage(static_cast<Page>((uintptr_t)lv_obj_get_user_data(btn)));
}

void SettingsApp::backlight_slider_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  if (app->_closing || app->_bl_slider == nullptr) {
    return;
  }

  const int val = lv_slider_get_value(app->_bl_slider);
  board_backlight_set_percent((uint8_t)val);

  if (app->_bl_value != nullptr) {
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", val);
    lv_label_set_text(app->_bl_value, pct);
  }
}

void SettingsApp::wifi_scan_btn_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  app->startWifiScan();
}

void SettingsApp::wifi_item_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  if (app->_closing) {
    return;
  }

  lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
  const uintptr_t idx = (uintptr_t)lv_obj_get_user_data(btn);
  if (idx >= app->_scan_count) {
    return;
  }

  app->openConnectPanel(app->_scan_aps[idx].ssid, app->_scan_aps[idx].auth);
}

void SettingsApp::connect_btn_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  if (app->_closing || app->_selected_ssid[0] == '\0' ||
      app->_connect_task != nullptr) {
    return;
  }

  auto *args = new ConnectArgs{};
  args->app = app;
  strncpy(args->ssid, app->_selected_ssid, BOARD_WIFI_SSID_MAX);
  args->ssid[BOARD_WIFI_SSID_MAX] = '\0';
  args->password[0] = '\0';
  if (app->_selected_auth != WIFI_AUTH_OPEN && app->_pwd_ta != nullptr) {
    strncpy(args->password, lv_textarea_get_text(app->_pwd_ta),
            sizeof(args->password) - 1);
  }

  app->hideKeyboardOverlay();
  app->closeConnectPanel();
  app->showWifiList(false);
  app->updateWifiStatus();

  if (xTaskCreatePinnedToCore(connect_task_entry, "wifi_conn", 6144, args, 5,
                              &app->_connect_task, 1) != pdPASS) {
    app->_connect_task = nullptr;
    delete args;
    ESP_LOGE(TAG, "connect task create failed");
  }
}

void SettingsApp::connect_task_entry(void *arg) {
  auto *args = static_cast<ConnectArgs *>(arg);
  if (!board_wifi_is_initialized()) {
    args->err = board_wifi_init();
  } else {
    args->err = ESP_OK;
  }
  if (args->err == ESP_OK) {
    args->err = board_wifi_connect(args->ssid, args->password);
  }
  lv_async_call(connect_async_cb, args);
  if (args->app != nullptr) {
    args->app->_connect_task = nullptr;
  }
  vTaskDelete(nullptr);
}

void SettingsApp::connect_async_cb(void *user_data) {
  auto *args = static_cast<ConnectArgs *>(user_data);
  if (args == nullptr) {
    return;
  }
  if (args->app != nullptr && !args->app->_closing) {
    args->app->updateWifiStatus();
  }
  if (args->err != ESP_OK) {
    ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(args->err));
  }
  delete args;
}

void SettingsApp::cancel_btn_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  app->closeConnectPanel();
  app->showWifiList(false);
}

void SettingsApp::pwd_preview_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  app->showKeyboardOverlay();
}

void SettingsApp::kb_event_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  app->hideKeyboardOverlay();
}

void SettingsApp::kb_done_btn_cb(lv_event_t *e) {
  auto *app = static_cast<SettingsApp *>(lv_event_get_user_data(e));
  app->hideKeyboardOverlay();
}

void SettingsApp::prepareForSnapshot(void) {
  if (_wifi_status_timer != nullptr) {
    lv_timer_pause(_wifi_status_timer);
  }
  hideKeyboardOverlay();
  closeConnectPanel();
  showWifiList(false);
  showPage(Page::Backlight);
  if (_root != nullptr) {
    lv_obj_set_style_bg_color(_root, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, 0);
  }
}

bool SettingsApp::pause(void) {
  prepareForSnapshot();
  return true;
}

bool SettingsApp::back(void) {
  if (_kb_overlay != nullptr &&
      !lv_obj_has_flag(_kb_overlay, LV_OBJ_FLAG_HIDDEN)) {
    hideKeyboardOverlay();
    return true;
  }
  if (_wifi_connect_cont != nullptr &&
      !lv_obj_has_flag(_wifi_connect_cont, LV_OBJ_FLAG_HIDDEN)) {
    closeConnectPanel();
    showWifiList(false);
    return true;
  }
  if (_wifi_list_cont != nullptr &&
      !lv_obj_has_flag(_wifi_list_cont, LV_OBJ_FLAG_HIDDEN)) {
    showWifiList(false);
    return true;
  }
  prepareForSnapshot();
  notifyCoreClosed();
  return true;
}

bool SettingsApp::close(void) {
  _closing = true;
  if (_wifi_status_timer != nullptr) {
    lv_timer_delete(_wifi_status_timer);
    _wifi_status_timer = nullptr;
  }
  hideKeyboardOverlay();
  closeConnectPanel();
  _scan_task = nullptr;
  _connect_task = nullptr;
  _scan_busy = false;
  _root = nullptr;
  _menu_cont = nullptr;
  _content_cont = nullptr;
  _page_bl = nullptr;
  _page_wifi = nullptr;
  _page_info = nullptr;
  _bl_slider = nullptr;
  _bl_value = nullptr;
  _wifi_status = nullptr;
  _wifi_list_cont = nullptr;
  _wifi_list = nullptr;
  _wifi_connect_cont = nullptr;
  _connect_title = nullptr;
  _pwd_preview_btn = nullptr;
  _pwd_preview_lbl = nullptr;
  _kb_overlay = nullptr;
  _pwd_ta = nullptr;
  _pwd_kb = nullptr;
  _scan_count = 0;
  return true;
}

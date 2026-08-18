#pragma once

#include "board_wifi.h"
#include "esp_brookesia.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

class SettingsApp : public ESP_Brookesia_PhoneApp {
public:
  SettingsApp();
  ~SettingsApp() override = default;

  bool run(void) override;
  bool pause(void) override;
  bool back(void) override;
  bool close(void) override;

private:
  enum class Page : uint8_t { Backlight, WiFi, Device };

  struct ScanResult {
    SettingsApp *app;
    esp_err_t err;
    uint16_t count;
    board_wifi_ap_info_t aps[BOARD_WIFI_SCAN_MAX];
  };

  struct ConnectArgs {
    SettingsApp *app;
    esp_err_t err;
    char ssid[BOARD_WIFI_SSID_MAX + 1];
    char password[64];
  };

  void applyDarkStyle(lv_obj_t *obj);
  void buildMenu(int w);
  void buildContentPages(int w, int content_h);
  void showPage(Page page);

  void buildBacklightPage(lv_obj_t *parent, int w);
  void buildWifiPage(lv_obj_t *parent, int w, int h);
  void buildInfoPage(lv_obj_t *parent, int w);

  void updateWifiStatus(void);
  void showWifiList(bool show);
  void showWifiConnectPanel(bool show);
  void clearWifiList(void);
  void populateWifiList(void);
  void openConnectPanel(const char *ssid, wifi_auth_mode_t auth);
  void closeConnectPanel(void);
  void showKeyboardOverlay(void);
  void hideKeyboardOverlay(void);
  void updatePwdPreview(void);
  void startWifiScan(void);
  void prepareForSnapshot(void);

  static void menu_btn_cb(lv_event_t *e);
  static void backlight_slider_cb(lv_event_t *e);
  static void wifi_scan_btn_cb(lv_event_t *e);
  static void wifi_item_cb(lv_event_t *e);
  static void connect_btn_cb(lv_event_t *e);
  static void cancel_btn_cb(lv_event_t *e);
  static void pwd_preview_cb(lv_event_t *e);
  static void kb_event_cb(lv_event_t *e);
  static void kb_done_btn_cb(lv_event_t *e);
  static void scan_task_entry(void *arg);
  static void scan_async_cb(void *user_data);
  static void connect_task_entry(void *arg);
  static void connect_async_cb(void *user_data);
  static void wifi_status_timer_cb(lv_timer_t *timer);

  lv_obj_t *_root = nullptr;
  lv_obj_t *_menu_cont = nullptr;
  lv_obj_t *_content_cont = nullptr;
  lv_obj_t *_page_bl = nullptr;
  lv_obj_t *_page_wifi = nullptr;
  lv_obj_t *_page_info = nullptr;

  lv_obj_t *_bl_slider = nullptr;
  lv_obj_t *_bl_value = nullptr;
  lv_obj_t *_wifi_status = nullptr;
  lv_obj_t *_wifi_list_cont = nullptr;
  lv_obj_t *_wifi_list = nullptr;
  lv_obj_t *_wifi_connect_cont = nullptr;
  lv_obj_t *_connect_title = nullptr;
  lv_obj_t *_pwd_preview_btn = nullptr;
  lv_obj_t *_pwd_preview_lbl = nullptr;
  lv_obj_t *_kb_overlay = nullptr;
  lv_obj_t *_pwd_ta = nullptr;
  lv_obj_t *_pwd_kb = nullptr;

  lv_timer_t *_wifi_status_timer = nullptr;
  board_wifi_ap_info_t _scan_aps[BOARD_WIFI_SCAN_MAX] = {};
  uint16_t _scan_count = 0;
  char _selected_ssid[BOARD_WIFI_SSID_MAX + 1] = {};
  wifi_auth_mode_t _selected_auth = WIFI_AUTH_OPEN;
  TaskHandle_t _scan_task = nullptr;
  TaskHandle_t _connect_task = nullptr;
  bool _scan_busy = false;
  Page _page = Page::Backlight;
  bool _closing = false;
};

#include "TouchTestApp.hpp"

#include "app_icons.h"

#include <cstdio>

static constexpr uint32_t kBgCanvas = 0x101010;

static ESP_Brookesia_PhoneAppData_t touch_test_phone_data(void) {
  ESP_Brookesia_PhoneAppData_t data = ESP_BROOKESIA_PHONE_APP_DATA_DEFAULT(
      &galleria_app_icon_touch, false, true);
  data.status_icon_data.icon.image_num = 0;
  data.flags.enable_navigation_gesture = 0;
  return data;
}

TouchTestApp::TouchTestApp()
    : ESP_Brookesia_PhoneApp(
          ESP_BROOKESIA_CORE_APP_DATA_DEFAULT("Touch Test",
                                              &galleria_app_icon_touch, true),
          touch_test_phone_data()) {}

bool TouchTestApp::run(void) {
  if (ESP_Brookesia_StatusBar *bar = getPhone()->getHome().getStatusBar()) {
    bar->setVisualMode(ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE);
  }
  if (ESP_Brookesia_NavigationBar *nav =
          getPhone()->getHome().getNavigationBar()) {
    nav->setVisualMode(ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_SHOW_FLEX);
  }

  lv_area_t area = getVisualArea();
  const int w = lv_area_get_width(&area);
  const int h = lv_area_get_height(&area);

  _canvas = lv_obj_create(lv_screen_active());
  lv_obj_remove_flag(_canvas, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(_canvas, w, h);
  lv_obj_set_pos(_canvas, area.x1, area.y1);
  lv_obj_set_style_radius(_canvas, 0, 0);
  lv_obj_set_style_border_width(_canvas, 0, 0);
  lv_obj_set_style_pad_all(_canvas, 0, 0);
  lv_obj_set_style_bg_color(_canvas, lv_color_hex(kBgCanvas), 0);
  lv_obj_set_style_bg_opa(_canvas, LV_OPA_COVER, 0);
  lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(_canvas, canvas_event_cb, LV_EVENT_PRESSED, this);
  lv_obj_add_event_cb(_canvas, canvas_event_cb, LV_EVENT_PRESSING, this);

  _coord_label = lv_label_create(_canvas);
  lv_label_set_text(_coord_label, "Touch to show (x, y)");
  lv_obj_set_style_text_color(_coord_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_align(_coord_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(_coord_label, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(_coord_label, LV_OPA_70, 0);
  lv_obj_set_style_pad_hor(_coord_label, 16, 0);
  lv_obj_set_style_pad_ver(_coord_label, 10, 0);
  lv_obj_set_style_radius(_coord_label, 8, 0);
  lv_obj_remove_flag(_coord_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(_coord_label, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_align(_coord_label, LV_ALIGN_CENTER, 0, 0);

  return true;
}

void TouchTestApp::updateCoordLabel(int x, int y) {
  if (_coord_label == nullptr) {
    return;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "(%d, %d)", x, y);
  lv_label_set_text(_coord_label, buf);
}

static bool readTouchPoint(lv_event_t *e, lv_point_t *pt) {
  lv_indev_t *indev = lv_indev_active();
  if (indev == nullptr) {
    indev = lv_event_get_indev(e);
  }
  if (indev == nullptr) {
    return false;
  }
  lv_indev_get_point(indev, pt);
  return true;
}

void TouchTestApp::canvas_event_cb(lv_event_t *e) {
  auto *app = static_cast<TouchTestApp *>(lv_event_get_user_data(e));
  if (app == nullptr) {
    return;
  }

  lv_point_t pt;
  if (!readTouchPoint(e, &pt)) {
    return;
  }
  app->updateCoordLabel((int)pt.x, (int)pt.y);
}

bool TouchTestApp::back(void) {
  notifyCoreClosed();
  return true;
}

bool TouchTestApp::close(void) {
  _canvas = nullptr;
  _coord_label = nullptr;
  return true;
}

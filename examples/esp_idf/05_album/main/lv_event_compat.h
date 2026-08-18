#pragma once

/*
 * esp_lvgl_adapter 0.5.3 calls lv_event_get_invalidated_area(), added in LVGL 9.3.
 * esp-brookesia 0.5.0 pins LVGL 9.2.*, where the invalidated area is the event param.
 * This header is force-included only when compiling the adapter (see CMakeLists.txt).
 */
#include "lv_version.h"

#if (LVGL_VERSION_MAJOR * 100 + LVGL_VERSION_MINOR) < 903
#define lv_event_get_invalidated_area(e) ((lv_area_t *)lv_event_get_param(e))
#endif

#pragma once

#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** 112x112 launcher icon corner radius (px). */
#define GALLERIA_APP_ICON_LAUNCHER_RADIUS 24
/** 36x36 recents title icon corner radius (px). */
#define GALLERIA_APP_ICON_RECENTS_RADIUS 8

/* Descriptors live in RAM so startup can punch rounded alpha into a copy. */
extern lv_image_dsc_t galleria_app_icon_settings;
extern lv_image_dsc_t galleria_app_icon_weather;
extern lv_image_dsc_t galleria_app_icon_album;
extern lv_image_dsc_t galleria_app_icon_display;
extern lv_image_dsc_t galleria_app_icon_touch;

void galleria_app_icons_apply_round_corners(void);

#ifdef __cplusplus
}
#endif

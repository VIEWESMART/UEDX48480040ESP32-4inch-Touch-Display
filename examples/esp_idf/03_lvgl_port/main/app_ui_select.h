/**
 * @file app_ui_select.h
 * @brief Pick which UI runs after boot. Change only APP_UI_SELECT below.
 *
 * This header switches between the board's local tap-check UI and the official
 * LVGL demos. Only one set can run: a demo owns the full screen and must not
 * be stacked on the local UI.
 *
 * How to switch
 * -------------
 * 1. Set APP_UI_SELECT to one of the APP_UI_* values, e.g. APP_UI_DEMO_WIDGETS.
 * 2. Rebuild and flash: idf.py build flash monitor
 *
 * sdkconfig.defaults already enables the matching demo sources
 * (CONFIG_LV_USE_DEMO_xxx). Selecting a demo whose Kconfig option is off
 * fails at compile time with a hint about which option to turn on.
 *
 * This panel is 480x480. The official Music demo uses LV_DEMO_MUSIC_SQUARE.
 */

#pragma once

#include "lvgl.h"

/* --------------------------------------------------------------------------
 * UI IDs (do not change these numbers; only change APP_UI_SELECT below)
 * -------------------------------------------------------------------------- */

/** Local UI: title + PASS badge + Tap me button + backlight slider (LCD / touch check). */
#define APP_UI_LOCAL                0

/**
 * Official LVGL Widgets demo.
 * Three tabs (Profile / Analytics / Shop) covering buttons, charts, calendar,
 * keyboard, and other stock widgets. 480x480 uses the MEDIUM layout.
 */
#define APP_UI_DEMO_WIDGETS         1

/**
 * Official LVGL Music player demo.
 * Phone-style player (cover, spectrum, playlist). Square 480x480 is enabled
 * via CONFIG_LV_DEMO_MUSIC_SQUARE in sdkconfig. This is UI only; it does not
 * decode or play audio.
 */
#define APP_UI_DEMO_MUSIC           2

/**
 * Official LVGL Benchmark.
 * Runs a set of scenes and reports FPS / CPU. A summary page is shown at the end.
 */
#define APP_UI_DEMO_BENCHMARK       3

/**
 * Official LVGL Stress test.
 * Creates/deletes objects, restyles, and runs animations at a high rate.
 * Useful for leaks or visual glitches; not a product UI.
 */
#define APP_UI_DEMO_STRESS          4

/**
 * Official LVGL Keypad & Encoder demo.
 * Shows keypad/encoder navigation without touch. This board is capacitive
 * touch; leave this off unless a keypad or encoder is wired.
 */
#define APP_UI_DEMO_KEYPAD_ENCODER  5

/* --------------------------------------------------------------------------
 * [CHANGE THIS] UI shown after boot
 * -------------------------------------------------------------------------- */
#define APP_UI_SELECT               APP_UI_DEMO_WIDGETS

/* --------------------------------------------------------------------------
 * Catch an invalid ID at compile time instead of booting to a black screen
 * -------------------------------------------------------------------------- */
#if (APP_UI_SELECT != APP_UI_LOCAL) && \
    (APP_UI_SELECT != APP_UI_DEMO_WIDGETS) && \
    (APP_UI_SELECT != APP_UI_DEMO_MUSIC) && \
    (APP_UI_SELECT != APP_UI_DEMO_BENCHMARK) && \
    (APP_UI_SELECT != APP_UI_DEMO_STRESS) && \
    (APP_UI_SELECT != APP_UI_DEMO_KEYPAD_ENCODER)
#error "Invalid APP_UI_SELECT. Use one of the APP_UI_xxx values in app_ui_select.h."
#endif

/* --------------------------------------------------------------------------
 * Match LVGL component options: fail clearly if demo sources were not built
 * (LV_USE_DEMO_xxx comes from menuconfig / sdkconfig.defaults)
 * -------------------------------------------------------------------------- */
#if APP_UI_SELECT == APP_UI_DEMO_WIDGETS && !LV_USE_DEMO_WIDGETS
#error "Widgets demo selected, but CONFIG_LV_USE_DEMO_WIDGETS is off. Check sdkconfig.defaults and rebuild."
#endif

#if APP_UI_SELECT == APP_UI_DEMO_MUSIC && !LV_USE_DEMO_MUSIC
#error "Music demo selected, but CONFIG_LV_USE_DEMO_MUSIC is off. Check sdkconfig.defaults and rebuild."
#endif

#if APP_UI_SELECT == APP_UI_DEMO_BENCHMARK && !LV_USE_DEMO_BENCHMARK
#error "Benchmark demo selected, but CONFIG_LV_USE_DEMO_BENCHMARK is off. Check sdkconfig.defaults and rebuild."
#endif

#if APP_UI_SELECT == APP_UI_DEMO_STRESS && !LV_USE_DEMO_STRESS
#error "Stress demo selected, but CONFIG_LV_USE_DEMO_STRESS is off. Check sdkconfig.defaults and rebuild."
#endif

#if APP_UI_SELECT == APP_UI_DEMO_KEYPAD_ENCODER && !LV_USE_DEMO_KEYPAD_AND_ENCODER
#error "Keypad/Encoder demo selected, but CONFIG_LV_USE_DEMO_KEYPAD_AND_ENCODER is off. Check sdkconfig.defaults and rebuild."
#endif

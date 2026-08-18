/**
 * @file phone_main.cpp
 * @brief Entry point for the ESP-Brookesia Phone demo on a 4-inch LCD board.
 *
 * Boot sequence:
 *   1. Initialize NVS (Wi-Fi credentials, app settings, etc.).
 *   2. Start the BSP display (and optionally mount the SD card).
 *   3. Register the LVGL image decoder used by album / icon assets.
 *   4. Create the Brookesia Phone shell, apply a 480x480 dark stylesheet,
 *      bind the touch device, and install demo apps.
 *   5. Periodically refresh the status-bar clock and Wi-Fi icon.
 *   6. Show a one-shot hardware self-test banner (touch + SD).
 *
 * LVGL is not thread-safe. All LVGL / Brookesia UI calls must be made while
 * holding the BSP display lock (see lvgl_lock_cb / lvgl_unlock_cb).
 */

#include "app_icons.h"
#include "apps.hpp"
#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lv_decoder.h"
#include "nvs_flash.h"
#include "private/esp_brookesia_utils.h"

#include <cstring>
#include <ctime>

static const char *TAG = "phone_ex";

/**
 * @brief LVGL lock callback registered with Brookesia.
 *
 * Brookesia invokes this before touching LVGL objects from a non-LVGL context.
 * A timeout of <= 0 means "wait forever", which matches bsp_display_lock(0).
 *
 * @param timeout_ms  Maximum wait in milliseconds, or <= 0 to block indefinitely.
 * @return true if the lock was acquired, false on timeout.
 */
static bool lvgl_lock_cb(int timeout_ms)
{
    return bsp_display_lock(timeout_ms <= 0 ? 0 : (uint32_t)timeout_ms);
}

/**
 * @brief LVGL unlock callback registered with Brookesia.
 *
 * Always returns true so Brookesia treats the unlock as successful.
 */
static bool lvgl_unlock_cb(void)
{
    bsp_display_unlock();
    return true;
}

/**
 * @brief Decide whether the status-bar timer should refresh clock / Wi-Fi icons.
 *
 * Full-screen apps (Album, Weather, Settings) draw over the status bar. Updating
 * the bar while they are open (or immediately after they close) causes visible
 * flicker. This helper:
 *   - Returns false while Album / Weather / Settings is the active app.
 *   - Starts a 3-second quiet window when leaving those apps (including when
 *     they are closed and no app is active).
 *   - Returns true only after the quiet window expires and the current app is
 *     not one of the full-screen apps.
 *
 * @param phone  Phone instance used to query the currently active app.
 * @return true if the status bar may be updated this tick.
 */
static bool status_bar_should_update(ESP_Brookesia_Phone *phone)
{
    /* Name of the app that was active on the previous timer tick. */
    static char s_prev_app[16] = "";
    /* LVGL tick (ms) until which status-bar updates are suppressed. */
    static uint32_t s_quiet_until = 0;

    auto mark_quiet = []() { s_quiet_until = lv_tick_get() + 3000; };

    ESP_Brookesia_CoreApp *active = phone->getManager().getActiveApp();
    if (active == nullptr) {
        /* Returning to the launcher from a full-screen app: keep the bar quiet
         * while the close animation finishes. */
        if (strcmp(s_prev_app, "Album") == 0 || strcmp(s_prev_app, "Weather") == 0 ||
            strcmp(s_prev_app, "Settings") == 0) {
            mark_quiet();
            s_prev_app[0] = '\0';
        }
        return lv_tick_get() >= s_quiet_until;
    }

    const char *name = active->getName();
    if (name == nullptr) {
        return lv_tick_get() >= s_quiet_until;
    }

    /* Detect a switch away from a full-screen app and start the quiet window. */
    if ((strcmp(s_prev_app, "Album") == 0 && strcmp(name, "Album") != 0) ||
        (strcmp(s_prev_app, "Weather") == 0 && strcmp(name, "Weather") != 0) ||
        (strcmp(s_prev_app, "Settings") == 0 && strcmp(name, "Settings") != 0)) {
        mark_quiet();
    }
    strncpy(s_prev_app, name, sizeof(s_prev_app) - 1);
    s_prev_app[sizeof(s_prev_app) - 1] = '\0';

    if (lv_tick_get() < s_quiet_until) {
        return false;
    }

    /* Do not refresh the bar while a full-screen app owns the display. */
    return strcmp(name, "Album") != 0 && strcmp(name, "Weather") != 0 &&
           strcmp(name, "Settings") != 0;
}

/**
 * @brief Map a Wi-Fi RSSI value to a Brookesia status-bar signal icon.
 *
 * Thresholds are typical consumer-device buckets:
 *   >= -55 dBm  strong  (3 bars)
 *   >= -70 dBm  medium  (2 bars)
 *   otherwise   weak    (1 bar)
 *
 * Disconnected / scanning states are handled by the caller, not here.
 *
 * @param rssi  Received signal strength in dBm (more negative = weaker).
 */
static ESP_Brookesia_StatusBar::WifiState wifi_level_from_rssi(int8_t rssi)
{
    if (rssi >= -55) {
        return ESP_Brookesia_StatusBar::WifiState::SIGNAL_3;
    }
    if (rssi >= -70) {
        return ESP_Brookesia_StatusBar::WifiState::SIGNAL_2;
    }
    return ESP_Brookesia_StatusBar::WifiState::SIGNAL_1;
}

/**
 * @brief LVGL timer callback that keeps the status-bar clock and Wi-Fi icon in sync.
 *
 * Period is ~2 s (see app_main). Updates are skipped while a full-screen app is
 * open (see status_bar_should_update). Clock and Wi-Fi icon are only written when
 * their values actually change, to avoid unnecessary LVGL invalidation.
 *
 * @param t  Timer whose user_data is the ESP_Brookesia_Phone instance.
 */
static void on_status_bar_timer_cb(lv_timer_t *t)
{
    auto *phone = static_cast<ESP_Brookesia_Phone *>(t->user_data);
    if (!status_bar_should_update(phone)) {
        return;
    }
    ESP_Brookesia_StatusBar *bar = phone->getHome().getStatusBar();
    if (bar == nullptr) {
        return;
    }

    /* Cached values so we only call into Brookesia when something changed. */
    static int s_last_hour = -1;
    static int s_last_min = -1;
    static bool s_last_is_pm = false;
    static ESP_Brookesia_StatusBar::WifiState s_last_wifi =
        ESP_Brookesia_StatusBar::WifiState::DISCONNECTED;

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    const bool is_pm = timeinfo.tm_hour >= 12;
    if (timeinfo.tm_hour != s_last_hour || timeinfo.tm_min != s_last_min || is_pm != s_last_is_pm) {
        bar->setClock(timeinfo.tm_hour, timeinfo.tm_min, is_pm);
        s_last_hour = timeinfo.tm_hour;
        s_last_min = timeinfo.tm_min;
        s_last_is_pm = is_pm;
    }

    ESP_Brookesia_StatusBar::WifiState level =
        ESP_Brookesia_StatusBar::WifiState::DISCONNECTED;
    /* RSSI is only meaningful when STA is associated and not in a scan. */
    if (bsp_wifi_is_initialized() && bsp_wifi_is_connected() && !bsp_wifi_is_scanning()) {
        level = wifi_level_from_rssi(bsp_wifi_get_rssi());
    }
    if (level != s_last_wifi) {
        bar->setWifiIconState(level);
        s_last_wifi = level;
    }
}

/**
 * @brief Overlay a centered self-test banner reporting LCD / touch / SD status.
 *
 * Green if both touch and SD passed, red otherwise. LCD is assumed present
 * because this function is only called after the display has started.
 * When everything passes, the banner auto-deletes after 5 seconds so it does
 * not block the launcher. On failure it stays on screen for diagnosis.
 *
 * Must be called with the LVGL display lock held.
 *
 * @param touch_ok  true if a touch input device was registered.
 * @param sd_ok     true if the SD card was mounted successfully.
 */
static void show_hw_banner(bool touch_ok, bool sd_ok)
{
    const bool all_ok = touch_ok && sd_ok;
    lv_obj_t *bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bar, 440, 72);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(bar, 16, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, all_ok ? lv_color_hex(0x1B7F4E) : lv_color_hex(0xB42318), 0);
    lv_obj_t *lab = lv_label_create(bar);
    lv_label_set_text_fmt(lab, "LCD PASS\nTouch %s   SD %s", touch_ok ? "PASS" : "FAIL",
                          sd_ok ? "PASS" : "FAIL");
    lv_obj_set_style_text_color(lab, lv_color_white(), 0);
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lab);
    if (all_ok) {
        /* One-shot timer: delete the banner after 5 s so the home screen is usable. */
        lv_timer_t *tmr = lv_timer_create(
            [](lv_timer_t *t) {
                lv_obj_t *obj = static_cast<lv_obj_t *>(lv_timer_get_user_data(t));
                if (obj != nullptr && lv_obj_is_valid(obj)) {
                    lv_obj_delete(obj);
                }
            },
            5000, bar);
        lv_timer_set_repeat_count(tmr, 1);
    }
}

/**
 * @brief ESP-IDF application entry. Brings up NVS, display, Phone UI, and demo apps.
 *
 * Does not return on success: FreeRTOS tasks (LVGL, Wi-Fi, etc.) keep running.
 * On a fatal display or Phone init failure the function logs and returns, which
 * leaves the system idle.
 */
extern "C" void app_main(void)
{
    /* NVS may need a full erase after a partition-layout or format-version change. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    bsp_display_cfg_t cfg = {};
    cfg.flags.mount_sd = 1; /* Album app reads photos from /sdcard/photos. */
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    if (bsp_sdcard_get_handle() == NULL) {
        ESP_LOGW(TAG, "SD card not ready; album app needs /sdcard/photos");
    }

    /* Decoder enables PNG/JPEG (and similar) images inside LVGL widgets. */
    esp_lv_decoder_handle_t decoder = NULL;
    ESP_ERROR_CHECK(esp_lv_decoder_init(&decoder));

    /* Hold the LVGL lock for the rest of UI construction. */
    bsp_display_lock(0);

    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone(disp);
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone, "Create phone failed");

    /* Stylesheet is copied into Phone on activate; the local copy can be freed. */
    auto *stylesheet = new ESP_Brookesia_PhoneStylesheet_t(
        ESP_BROOKESIA_PHONE_480_480_DARK_STYLESHEET());
    ESP_LOGI(TAG, "Using stylesheet (%s)", stylesheet->core.name);
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet),
                                   "Activate stylesheet failed");
    delete stylesheet;

    if (bsp_display_get_input_dev() != nullptr) {
        ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->setTouchDevice(bsp_display_get_input_dev()),
                                       "Set touch failed");
    }
    phone->registerLvLockCallback(lvgl_lock_cb, -1);
    phone->registerLvUnlockCallback(lvgl_unlock_cb);
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->begin(), "Phone begin failed");

    /* Round launcher icons so they match the iPhone-style home grid. */
    galleria_app_icons_apply_round_corners();

    SettingsApp *settings = new SettingsApp();
    ScreenTestApp *screen_test = new ScreenTestApp();
    TouchTestApp *touch_test = new TouchTestApp();
    PhotoAlbumApp *album = new PhotoAlbumApp();
    WeatherApp *weather = new WeatherApp();

    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->installApp(settings) >= 0, "Install Settings failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->installApp(screen_test) >= 0,
                                   "Install ScreenTest failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->installApp(touch_test) >= 0, "Install TouchTest failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->installApp(album) >= 0, "Install Album failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->installApp(weather) >= 0, "Install Weather failed");

    /* Refresh clock / Wi-Fi about twice per second is enough; 2 s keeps CPU load low. */
    lv_timer_create(on_status_bar_timer_cb, 2000, phone);

    show_hw_banner(bsp_display_get_input_dev() != nullptr,
                   bsp_sdcard_get_handle() != nullptr);

    bsp_display_unlock();
    ESP_LOGI(TAG, "Brookesia Phone UI started (%dx%d)", BSP_LCD_H_RES, BSP_LCD_V_RES);
}

/**
 * @file album_main.cpp
 * @brief Photo Album example entry point for a 480x480 LCD phone-style UI.
 *
 * Boot sequence:
 *   1. Initialize NVS (needed by Wi-Fi / system components that persist settings).
 *   2. Start the BSP display (and optionally mount the SD card).
 *   3. Register the JPEG decoder so LVGL can load photos from /sdcard/photos.
 *   4. Create an ESP-Brookesia Phone shell, apply the 480x480 dark stylesheet,
 *      and bind the touch input device.
 *   5. Install PhotoAlbumApp and show a short hardware self-test banner.
 *
 * Put JPEG files under /sdcard/photos on the SD card, then open the Album app
 * from the phone launcher.
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

static const char *TAG = "album_ex";

/**
 * @brief LVGL lock callback used by ESP-Brookesia.
 *
 * Brookesia calls this before touching LVGL objects from non-LVGL tasks.
 * timeout_ms <= 0 means "wait forever", which we map to BSP lock timeout 0
 * (BSP treats 0 as an infinite wait).
 *
 * @param timeout_ms Maximum time to wait for the display mutex, in milliseconds.
 * @return true if the lock was acquired, false on timeout.
 */
static bool lvgl_lock_cb(int timeout_ms)
{
    return bsp_display_lock(timeout_ms <= 0 ? 0 : (uint32_t)timeout_ms);
}

/**
 * @brief LVGL unlock callback used by ESP-Brookesia.
 *
 * Always returns true so Brookesia treats the unlock as successful.
 */
static bool lvgl_unlock_cb(void)
{
    bsp_display_unlock();
    return true;
}

/**
 * @brief Overlay a centered self-test banner on the top LVGL layer.
 *
 * Green banner  : LCD, touch, and SD card are all OK; auto-dismiss after 5 s.
 * Red banner    : touch and/or SD failed; stays on screen so the issue is visible.
 *
 * The banner is created on lv_layer_top() so it sits above the phone UI.
 *
 * @param touch_ok true if a touch input device was registered by the BSP.
 * @param sd_ok    true if the SD card was mounted successfully.
 */
static void show_hw_banner(bool touch_ok, bool sd_ok)
{
    const bool all_ok = touch_ok && sd_ok;

    /* Rounded status bar: 440x72, centered on the 480x480 panel. */
    lv_obj_t *bar = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bar, 440, 72);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(bar, 16, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    /* Green = all hardware OK; red = at least one check failed. */
    lv_obj_set_style_bg_color(bar, all_ok ? lv_color_hex(0x1B7F4E) : lv_color_hex(0xB42318), 0);

    lv_obj_t *lab = lv_label_create(bar);
    lv_label_set_text_fmt(lab, "LCD PASS\nTouch %s   SD %s", touch_ok ? "PASS" : "FAIL",
                          sd_ok ? "PASS" : "FAIL");
    lv_obj_set_style_text_color(lab, lv_color_white(), 0);
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lab);

    /* Auto-hide only when every check passed, so failures remain visible. */
    if (all_ok) {
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
 * @brief ESP-IDF application entry. Brings up display, Brookesia Phone, and Album.
 */
extern "C" void app_main(void)
{
    /* NVS is required by several ESP-IDF components. Erase and re-init if the
     * partition is empty or the NVS format version has changed. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Start LCD + LVGL. flags.mount_sd asks the BSP to mount FAT on the SD slot
     * so PhotoAlbumApp can read JPEGs from /sdcard/photos. */
    bsp_display_cfg_t cfg = {};
    cfg.flags.mount_sd = 1;
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    if (bsp_sdcard_get_handle() == NULL) {
        ESP_LOGW(TAG, "SD not mounted; put JPEGs in /sdcard/photos");
    }

    /* Hardware JPEG decoder plugin for LVGL (esp_lv_decoder). Album thumbnails
     * and full-size photos are decoded through this handle. */
    esp_lv_decoder_handle_t decoder = NULL;
    ESP_ERROR_CHECK(esp_lv_decoder_init(&decoder));

    /* Hold the LVGL lock for the rest of UI construction. Brookesia and the
     * hardware banner both create LVGL objects and must not race the refresh task. */
    bsp_display_lock(0);

    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone(disp);
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone, "Create phone failed");

    /* 480x480 dark phone stylesheet. addStylesheet() copies the data, so the
     * temporary object can be deleted after activateStylesheet(). */
    auto *stylesheet = new ESP_Brookesia_PhoneStylesheet_t(
        ESP_BROOKESIA_PHONE_480_480_DARK_STYLESHEET());
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet),
                                   "Activate stylesheet failed");
    delete stylesheet;

    /* Bind the BSP touch device when present. Without it, the launcher is
     * display-only and the banner will report Touch FAIL. */
    if (bsp_display_get_input_dev() != nullptr) {
        ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->setTouchDevice(bsp_display_get_input_dev()),
                                       "Set touch failed");
    }
    phone->registerLvLockCallback(lvgl_lock_cb, -1);
    phone->registerLvUnlockCallback(lvgl_unlock_cb);
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->begin(), "Phone begin failed");

    /* Round the launcher app icons to match the dark stylesheet look. */
    galleria_app_icons_apply_round_corners();

    /* Install the Album app into the phone launcher. installApp() returns a
     * non-negative id on success. Photos are expected at /sdcard/photos. */
    PhotoAlbumApp *album = new PhotoAlbumApp();
    ESP_BROOKESIA_CHECK_FALSE_EXIT(phone->installApp(album) >= 0, "Install Album failed");

    show_hw_banner(bsp_display_get_input_dev() != nullptr,
                   bsp_sdcard_get_handle() != nullptr);

    bsp_display_unlock();
    ESP_LOGI(TAG, "Album example ready. Open Album and use /sdcard/photos");
}

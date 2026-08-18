/**
 * @file wifi_main.c
 * @brief Scan nearby APs (and optionally join one) and show the result on the LCD.
 *
 * What this example does
 * ----------------------
 * 1. bsp_display_start() brings up the 480x480 RGB panel, touch, and LVGL so
 *    the result is visible without a serial monitor.
 * 2. NVS is initialised (Wi-Fi calibration / config live there), then
 *    bsp_wifi_init() starts STA mode.
 * 3. bsp_wifi_scan() fills a short RSSI-sorted list (max BSP_WIFI_SCAN_MAX).
 * 4. If Example Configuration → WiFi SSID is empty, a successful scan is PASS.
 *    If SSID is set, the example calls bsp_wifi_connect() and waits up to
 *    15 s; PASS only after an IP is obtained.
 * 5. When a connect was requested, the badge is refreshed every 4 s with the
 *    live BSP status string and RSSI.
 *
 * How to read the result
 * ----------------------
 * Green PASS, scan-only     -> at least one scan completed (list may be empty
 *                             if the air is quiet).
 * Green PASS, SSID set      -> associated and got an IP.
 * Red FAIL                  -> scan failed, or join timed out / rejected.
 * Amber SCAN / JOIN         -> work in progress.
 *
 * Configure SSID / password with:
 *   idf.py -C examples/02_wifi menuconfig
 *     → Example Configuration
 *
 * All lv_* calls run between bsp_display_lock() / unlock().
 */

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "wifi_ex";

/** Amber badge used for INIT / SCAN / JOIN. Child 0 is the caption label. */
static lv_obj_t *make_badge(lv_obj_t *parent)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(badge, 260, 56);
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_radius(badge, 12, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xB58114), 0);
    lv_obj_t *lab = lv_label_create(badge);
    lv_label_set_text(lab, "INIT");
    lv_obj_set_style_text_color(lab, lv_color_white(), 0);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_22, 0);
    lv_obj_center(lab);
    return badge;
}

/**
 * @brief Update badge fill and caption.
 *
 * @param color  0x1B7F4E PASS, 0xB42318 FAIL, 0xB58114 in-progress
 */
static void badge_set(lv_obj_t *badge, uint32_t color, const char *text)
{
    lv_obj_set_style_bg_color(badge, lv_color_hex(color), 0);
    lv_obj_t *lab = lv_obj_get_child(badge, 0);
    lv_label_set_text(lab, text);
    lv_obj_center(lab);
}

/**
 * @brief Format the strongest APs for the on-screen body label.
 *
 * At most 8 rows are shown so the 480x480 layout does not overflow. The BSP
 * already de-duplicates SSIDs and sorts by RSSI.
 */
static void fill_ap_list(char *buf, size_t buf_size, const bsp_wifi_ap_info_t *aps, uint16_t count)
{
    int n = snprintf(buf, buf_size, "Found %u AP(s)\n", (unsigned)count);
    uint16_t show = count > 8 ? 8 : count;
    for (uint16_t i = 0; i < show; i++) {
        if ((size_t)n >= buf_size) {
            break;
        }
        n += snprintf(buf + n, buf_size - (size_t)n, "  %s  %d dBm\n", aps[i].ssid,
                      (int)aps[i].rssi);
    }
}

void app_main(void)
{
    /* Show a screen first so a slow Wi-Fi init is not a black panel. */
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    bsp_display_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_t *badge = make_badge(scr);
    lv_obj_t *body = lv_label_create(scr);
    lv_obj_set_style_text_color(body, lv_color_hex(0xD0D6DE), 0);
    lv_obj_set_width(body, 420);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 148);
    lv_label_set_text(body, "Starting STA ...");
    bsp_display_unlock();

    /* Wi-Fi stack stores PHY cal / config in NVS; erase if the layout changed. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(bsp_wifi_init());

    bsp_display_lock(0);
    badge_set(badge, 0xB58114, "SCAN");
    lv_label_set_text(body, "Scanning APs ...");
    bsp_display_unlock();

    bsp_wifi_ap_info_t aps[BSP_WIFI_SCAN_MAX];
    uint16_t count = 0;
    char scan_text[384];
    bool scan_ok = false;

    ESP_LOGI(TAG, "Scanning...");
    err = bsp_wifi_scan(aps, &count, BSP_WIFI_SCAN_MAX);
    if (err != ESP_OK) {
        snprintf(scan_text, sizeof(scan_text), "Scan failed:\n%s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", scan_text);
    } else {
        scan_ok = true;
        fill_ap_list(scan_text, sizeof(scan_text), aps, count);
        ESP_LOGI(TAG, "Found %u AP(s)", (unsigned)count);
    }

    /* Empty SSID in menuconfig => scan-only; non-empty => try to join. */
    bool want_connect = CONFIG_EXAMPLE_WIFI_SSID[0] != '\0';
    bool connected = false;
    if (scan_ok && want_connect) {
        char joining[448];
        snprintf(joining, sizeof(joining), "%s\nConnecting to %s ...", scan_text,
                 CONFIG_EXAMPLE_WIFI_SSID);
        bsp_display_lock(0);
        badge_set(badge, 0xB58114, "JOIN");
        lv_label_set_text(body, joining);
        bsp_display_unlock();

        ESP_LOGI(TAG, "Connecting to '%s'...", CONFIG_EXAMPLE_WIFI_SSID);
        (void)bsp_wifi_connect(CONFIG_EXAMPLE_WIFI_SSID, CONFIG_EXAMPLE_WIFI_PASSWORD);
        /* 30 x 500 ms = 15 s, enough for DHCP on a typical AP. */
        for (int i = 0; i < 30; i++) {
            vTaskDelay(pdMS_TO_TICKS(500));
            if (bsp_wifi_is_connected()) {
                connected = true;
                break;
            }
        }
    }

    const bool pass = want_connect ? connected : scan_ok;
    char text[512];
    if (want_connect) {
        snprintf(text, sizeof(text), "%s\n%s", scan_text, bsp_wifi_status_text());
    } else {
        snprintf(text, sizeof(text), "%s\nScan-only. Set SSID in menuconfig to connect.",
                 scan_text);
    }

    bsp_display_lock(0);
    badge_set(badge, pass ? 0x1B7F4E : 0xB42318, pass ? "PASS" : "FAIL");
    lv_label_set_text(body, text);
    bsp_display_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(4000));
        if (!want_connect) {
            continue;
        }
        /* Keep the badge honest if the AP drops after the first PASS. */
        connected = bsp_wifi_is_connected();
        snprintf(text, sizeof(text), "%s\n%s  rssi=%d", scan_text, bsp_wifi_status_text(),
                 (int)bsp_wifi_get_rssi());
        bsp_display_lock(0);
        badge_set(badge, connected ? 0x1B7F4E : 0xB42318, connected ? "PASS" : "FAIL");
        lv_label_set_text(body, text);
        bsp_display_unlock();
    }
}

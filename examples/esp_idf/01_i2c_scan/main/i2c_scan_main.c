/**
 * @file i2c_scan_main.c
 * @brief Probe the BSP I2C bus and show every responding address on the LCD.
 *
 * Hardware under test
 * -------------------
 * The UEDX48480040E-WB touch controller is FT5x06 on I2C:
 *   SDA = GPIO40, SCL = GPIO41, expected 7-bit address = 0x38.
 *
 * What this example does
 * ----------------------
 * 1. bsp_display_start() brings up the 480x480 RGB panel, backlight, FT5x06
 *    touch, and the LVGL adapter task. Touch init also creates the I2C master
 *    bus, so the scan reuses bsp_i2c_get_handle() instead of opening a second
 *    bus on the same pins.
 * 2. A simple LVGL screen shows a PASS/FAIL badge plus the list of addresses.
 * 3. The bus is re-scanned every 4 seconds so a flaky cable shows up on screen
 *    without a reboot.
 *
 * How to read the result
 * ----------------------
 * Green PASS  + "0x38  FT5x06" in the list  -> touch I2C is healthy.
 * Red FAIL                                  -> 0x38 did not ACK (wiring, power,
 *                                             or the panel is not the FT5x06).
 *
 * All lv_* calls run between bsp_display_lock() / unlock() so they do not race
 * the LVGL task created inside the BSP.
 */

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdio.h>

static const char *TAG = "i2c_scan";

/** 7-bit I2C address used by the onboard FT5x06 (see FT5x06 datasheet). */
#define FT5X06_ADDR 0x38

/**
 * @brief Create the amber status badge shown while a scan is in progress.
 *
 * Child 0 is the label. Later updates go through badge_set() so colour and
 * text stay in sync.
 */
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
    lv_label_set_text(lab, "SCAN");
    lv_obj_set_style_text_color(lab, lv_color_white(), 0);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_22, 0);
    lv_obj_center(lab);
    return badge;
}

/**
 * @brief Set badge colour and caption.
 *
 * @param ok    true  -> green PASS, false -> red FAIL
 * @param text  Short status string drawn inside the badge
 */
static void badge_set(lv_obj_t *badge, bool ok, const char *text)
{
    lv_obj_set_style_bg_color(badge, ok ? lv_color_hex(0x1B7F4E) : lv_color_hex(0xB42318), 0);
    lv_obj_t *lab = lv_obj_get_child(badge, 0);
    lv_label_set_text(lab, text);
    lv_obj_center(lab);
}

/**
 * @brief Walk 0x08..0x77 and record every address that ACKs.
 *
 * Reserved addresses below 0x08 and the general-call / 10-bit range at the
 * top are skipped. Each probe waits up to 50 ms.
 *
 * @param[in]  bus        I2C master bus from the BSP
 * @param[out] ft_found   Set true if 0x38 ACKed
 * @param[out] body       Human-readable listing for the on-screen label
 * @param[in]  body_size  Capacity of @p body
 */
static void scan_bus(i2c_master_bus_handle_t bus, bool *ft_found, char *body, size_t body_size)
{
    int n = snprintf(body, body_size, "SDA=GPIO%d  SCL=GPIO%d\nExpect FT5x06 @ 0x%02X\n\n",
                     (int)BSP_I2C_SDA, (int)BSP_I2C_SCL, FT5X06_ADDR);
    *ft_found = false;
    int found = 0;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 50) != ESP_OK) {
            continue;
        }
        found++;
        if (addr == FT5X06_ADDR) {
            *ft_found = true;
        }
        if ((size_t)n < body_size) {
            n += snprintf(body + n, body_size - (size_t)n, "  0x%02X%s\n", addr,
                          addr == FT5X06_ADDR ? "  FT5x06" : "");
        }
        ESP_LOGI(TAG, "  found 0x%02X", addr);
    }

    if (found == 0 && (size_t)n < body_size) {
        snprintf(body + n, body_size - (size_t)n, "No devices. Check wiring.");
    }
}

void app_main(void)
{
    /* Panel + touch + LVGL. Also initialises the I2C bus used below. */
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not ready");
        return;
    }

    /* Build the static screen once, then only refresh the badge + body. */
    bsp_display_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "I2C Scan");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_t *badge = make_badge(scr);
    lv_obj_t *body = lv_label_create(scr);
    lv_obj_set_style_text_color(body, lv_color_hex(0xD0D6DE), 0);
    lv_obj_set_width(body, 420);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 148);
    lv_label_set_text(body, "Probing 0x08..0x77 ...");
    bsp_display_unlock();

    char text[384];
    while (1) {
        bool ft_ok = false;
        ESP_LOGI(TAG, "Scan I2C bus %d", BSP_I2C_NUM);
        scan_bus(bus, &ft_ok, text, sizeof(text));
        ESP_LOGI(TAG, "FT5x06 %s", ft_ok ? "PASS" : "FAIL");

        bsp_display_lock(0);
        badge_set(badge, ft_ok, ft_ok ? "PASS" : "FAIL");
        lv_label_set_text(body, text);
        bsp_display_unlock();

        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

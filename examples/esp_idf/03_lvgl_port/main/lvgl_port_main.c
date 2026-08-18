/**
 * @file lvgl_port_main.c
 * @brief LVGL entry for the 4-inch 480x480 panel: start BSP display/touch, then
 *        load either the local UI or an official demo.
 *
 * Switch UI by changing APP_UI_SELECT in app_ui_select.h, then rebuild and flash.
 */

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "app_ui_select.h"

static const char *TAG = "lvgl_port";

#if APP_UI_SELECT == APP_UI_LOCAL
static lv_obj_t *s_badge;
static lv_obj_t *s_count_label;
static int s_clicks;

/**
 * Refresh the top status badge colour and caption.
 * TAP  = touch present, waiting for the first tap
 * PASS = button was tapped, touch path is good
 * LCD  = no touch device, display-only check
 */
static void badge_set(lv_obj_t *badge, uint32_t color, const char *text)
{
    lv_obj_set_style_bg_color(badge, lv_color_hex(color), 0);
    lv_obj_t *lab = lv_obj_get_child(badge, 0);
    lv_label_set_text(lab, text);
    lv_obj_center(lab);
}

static void on_btn(lv_event_t *e)
{
    (void)e;
    s_clicks++;
    lv_label_set_text_fmt(s_count_label, "Clicks: %d  (touch PASS)", s_clicks);
    badge_set(s_badge, 0x1B7F4E, "PASS");
}

/** Bottom slider: write 10..100 into the backlight PWM. */
static void on_slider(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int brightness = (int)lv_slider_get_value(slider);
    bsp_display_brightness_set(brightness);
}

/**
 * Local tap-check UI.
 * Seeing this screen means LCD init succeeded; green PASS after Tap me means touch works.
 */
static void ui_create_local(bool touch_ok)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL Port");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    s_badge = lv_obj_create(scr);
    lv_obj_remove_flag(s_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_badge, 260, 56);
    lv_obj_align(s_badge, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_radius(s_badge, 12, 0);
    lv_obj_set_style_border_width(s_badge, 0, 0);
    lv_obj_t *badge_lab = lv_label_create(s_badge);
    lv_obj_set_style_text_color(badge_lab, lv_color_white(), 0);
    lv_obj_set_style_text_font(badge_lab, &lv_font_montserrat_22, 0);
    if (touch_ok) {
        badge_set(s_badge, 0xB58114, "TAP");
    } else {
        badge_set(s_badge, 0x1B7F4E, "LCD");
    }

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text_fmt(sub, "%dx%d  tap button to verify touch", BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_obj_set_style_text_color(sub, lv_color_hex(0x9AA4B2), 0);
    lv_obj_set_width(sub, 420);
    lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 140);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 220, 64);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(btn, on_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Tap me");
    lv_obj_center(btn_label);

    s_count_label = lv_label_create(scr);
    lv_label_set_text(s_count_label, touch_ok ? "Clicks: 0" : "No touch device");
    lv_obj_set_style_text_color(s_count_label, lv_color_white(), 0);
    lv_obj_align(s_count_label, LV_ALIGN_CENTER, 0, 90);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Backlight");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x9AA4B2), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -88);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 320);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_add_event_cb(slider, on_slider, LV_EVENT_VALUE_CHANGED, NULL);
}
#endif /* APP_UI_SELECT == APP_UI_LOCAL */

/** Create the UI selected by APP_UI_SELECT. Caller must hold the LVGL lock. */
static void ui_create_selected(bool touch_ok)
{
#if APP_UI_SELECT == APP_UI_LOCAL
    ui_create_local(touch_ok);
    ESP_LOGI(TAG, "UI: local touch/LCD check");
#elif APP_UI_SELECT == APP_UI_DEMO_WIDGETS
    (void)touch_ok;
    lv_demo_widgets();
    ESP_LOGI(TAG, "UI: LVGL official demo widgets");
#elif APP_UI_SELECT == APP_UI_DEMO_MUSIC
    (void)touch_ok;
    lv_demo_music();
    ESP_LOGI(TAG, "UI: LVGL official demo music");
#elif APP_UI_SELECT == APP_UI_DEMO_BENCHMARK
    (void)touch_ok;
    lv_demo_benchmark();
    ESP_LOGI(TAG, "UI: LVGL official demo benchmark");
#elif APP_UI_SELECT == APP_UI_DEMO_STRESS
    (void)touch_ok;
    lv_demo_stress();
    ESP_LOGI(TAG, "UI: LVGL official demo stress");
#elif APP_UI_SELECT == APP_UI_DEMO_KEYPAD_ENCODER
    (void)touch_ok;
    lv_demo_keypad_encoder();
    ESP_LOGI(TAG, "UI: LVGL official demo keypad_encoder");
#endif
}

void app_main(void)
{
    /* RGB panel + FT5x06 touch + LVGL adapter task, all started by the BSP. */
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    const bool touch_ok = bsp_display_get_input_dev() != NULL;

    /* Every lv_* call must sit between lock/unlock so it does not race the LVGL task. */
    bsp_display_lock(0);
    ui_create_selected(touch_ok);
    bsp_display_unlock();

    ESP_LOGI(TAG, "UI ready. LCD up, touch %s", touch_ok ? "present" : "missing");
}

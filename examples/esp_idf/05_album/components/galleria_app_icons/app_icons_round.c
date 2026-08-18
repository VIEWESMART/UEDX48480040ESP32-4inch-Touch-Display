#include "app_icons.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>

static const char *TAG = "APP_ICONS";

static uint8_t rounded_rect_alpha(int px, int py, int w, int h, int radius)
{
    const float r = (float)radius;
    const float cx = (float)px + 0.5f;
    const float cy = (float)py + 0.5f;
    float dx = fabsf(cx - (float)w * 0.5f) - ((float)w * 0.5f - r);
    float dy = fabsf(cy - (float)h * 0.5f) - ((float)h * 0.5f - r);
    if (dx < 0.f) {
        dx = 0.f;
    }
    if (dy < 0.f) {
        dy = 0.f;
    }
    const float dist = sqrtf(dx * dx + dy * dy) - r;
    if (dist <= -0.5f) {
        return 255;
    }
    if (dist >= 0.5f) {
        return 0;
    }
    return (uint8_t)((0.5f - dist) * 255.0f);
}

static bool round_rgb565_icon(lv_image_dsc_t *dsc, int radius)
{
    if (dsc == NULL || dsc->data == NULL) {
        return false;
    }
    if (dsc->header.cf != LV_COLOR_FORMAT_RGB565) {
        return true;
    }

    const int w = dsc->header.w;
    const int h = dsc->header.h;
    if (w <= 0 || h <= 0) {
        return false;
    }

    int r = radius;
    if (r > w / 2) {
        r = w / 2;
    }
    if (r > h / 2) {
        r = h / 2;
    }
    if (r < 1) {
        return true;
    }

    const size_t rgb_size = (size_t)w * (size_t)h * 2u;
    const size_t alpha_size = (size_t)w * (size_t)h;
    const size_t total = rgb_size + alpha_size;

    uint8_t *buf = (uint8_t *)heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        buf = (uint8_t *)heap_caps_malloc(total, MALLOC_CAP_8BIT);
    }
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes for rounded icon", (unsigned)total);
        return false;
    }

    memcpy(buf, dsc->data, rgb_size);
    uint8_t *alpha = buf + rgb_size;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            alpha[y * w + x] = rounded_rect_alpha(x, y, w, h, r);
        }
    }

    dsc->data = buf;
    dsc->data_size = (uint32_t)total;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = (uint16_t)(w * 2);
    return true;
}

void galleria_app_icons_apply_round_corners(void)
{
    const int radius = GALLERIA_APP_ICON_LAUNCHER_RADIUS;
    const bool ok =
        round_rgb565_icon(&galleria_app_icon_settings, radius) &&
        round_rgb565_icon(&galleria_app_icon_weather, radius) &&
        round_rgb565_icon(&galleria_app_icon_album, radius) &&
        round_rgb565_icon(&galleria_app_icon_display, radius) &&
        round_rgb565_icon(&galleria_app_icon_touch, radius);
    if (ok) {
        ESP_LOGI(TAG, "Launcher icons converted to RGB565A8 with radius %d", radius);
    } else {
        ESP_LOGW(TAG, "Some launcher icons were not rounded");
    }
}

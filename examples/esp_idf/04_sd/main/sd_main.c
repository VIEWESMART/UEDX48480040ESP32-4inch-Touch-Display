/**
 * @file sd_main.c
 * @brief SD card bring-up example for UEDX48480040E-WB (ESP32-S3 + 4-inch LCD).
 *
 * Boot sequence:
 *   1. Start the LCD / LVGL stack with `flags.mount_sd = 1`.
 *   2. After the 3-wire panel IO is released, the BSP mounts the card over SDSPI.
 *   3. Probe the mount by writing `bsp_sd_test.txt`, then list a few directory entries.
 *   4. Draw a PASS/FAIL badge and the probe text on the screen.
 *
 * Hardware notes:
 *   - The card uses SDSPI, not SDMMC 1-bit/4-bit.
 *   - CS (GPIO47) is shared with the LCD 3-wire SDA line. Mounting must happen
 *     only after `bsp_display_new()` has deleted the panel IO, which
 *     `bsp_display_start_with_config()` does when `mount_sd` is set.
 *   - Format the card as FAT32. JPEG files for the album demo belong in
 *     `/sdcard/photos` (`BSP_SD_PHOTO_DIR`).
 */

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/** Log tag used by ESP_LOGx macros in this example. */
static const char *TAG = "sd_ex";

/**
 * @brief Append up to 10 visible names from @p dir_path onto @p buf.
 *
 * Hidden entries (names starting with '.') are skipped. If the directory
 * cannot be opened, a short error line is appended instead. An empty
 * directory produces "(empty)".
 *
 * The listing is truncated on purpose so the LVGL label stays readable on
 * a 480x480 panel.
 *
 * @param[in,out] buf       NUL-terminated destination buffer (must already
 *                          contain a valid C string).
 * @param[in]     buf_size  Total size of @p buf, including the terminator.
 * @param[in]     dir_path  Absolute VFS path, e.g. `BSP_SD_MOUNT_POINT`.
 */
static void append_dir_listing(char *buf, size_t buf_size, const char *dir_path)
{
    /* Track how much of the buffer is already occupied so later snprintf()
     * calls append instead of overwriting. */
    size_t used = strlen(buf);
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        snprintf(buf + used, buf_size - used, "\n(cannot open %s)", dir_path);
        return;
    }

    int n = 0;
    struct dirent *ent;
    /* Cap at 10 names: enough to confirm the card is readable without
     * flooding the on-screen label. */
    while ((ent = readdir(dir)) != NULL && n < 10) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        used = strlen(buf);
        snprintf(buf + used, buf_size - used, "\n  %s", ent->d_name);
        n++;
    }
    closedir(dir);
    if (n == 0) {
        used = strlen(buf);
        snprintf(buf + used, buf_size - used, "\n  (empty)");
    }
}

/**
 * @brief Application entry: mount SDSPI, run a write probe, then show the result.
 *
 * `app_main()` never returns on a successful display start; LVGL keeps
 * refreshing from its own task after this function finishes drawing.
 */
void app_main(void)
{
    /* Zero-init so unused adapter fields keep BSP defaults
     * (32 KB LVGL task stack, priority 7, pinned to core 0). */
    bsp_display_cfg_t cfg = {0};

    /* Ask the BSP to mount the SD card after LCD panel IO is released.
     * That order is required because CS shares GPIO47 with LCD SDA. */
    cfg.flags.mount_sd = 1;
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    /* Status string shown in the body label. Sized large enough for the
     * CID name plus two short directory listings. */
    char text[768];
    bool pass = false;

    /* A NULL handle means mount never succeeded (no card, wrong FS, or
     * SDSPI init error). The CS pin is printed so the GPIO-share issue
     * is obvious when debugging. */
    if (bsp_sdcard_get_handle() == NULL) {
        snprintf(text, sizeof(text),
                 "SD mount failed\nInsert a FAT32 card\nCS=GPIO%d (shared with LCD SDA)",
                 (int)BSP_SD_SPI_CS);
        ESP_LOGE(TAG, "%s", text);
    } else {
        /* Write-probe: mount success is not enough — the volume must also
         * be writable. The file is left on the card as a visible marker. */
        const char *test_path = BSP_SD_MOUNT_POINT "/bsp_sd_test.txt";
        FILE *f = fopen(test_path, "w");
        if (f != NULL) {
            fprintf(f, "UEDX48480040E-WB SD OK\n");
            fclose(f);
            pass = true;
            ESP_LOGI(TAG, "Wrote %s", test_path);
        } else {
            ESP_LOGW(TAG, "Write failed: %s", test_path);
            snprintf(text, sizeof(text), "Mounted but write failed:\n%s", test_path);
        }

        if (pass) {
            /* CID product name is a cheap identity check (e.g. "SD32G"). */
            sdmmc_card_t *card = bsp_sdcard_get_handle();
            snprintf(text, sizeof(text), "Mounted at %s\nWrote bsp_sd_test.txt",
                     BSP_SD_MOUNT_POINT);
            if (card != NULL) {
                size_t used = strlen(text);
                snprintf(text + used, sizeof(text) - used, "\n%s", card->cid.name);
            }
            /* Root listing confirms VFS is live; photos/ is the album demo path. */
            append_dir_listing(text, sizeof(text), BSP_SD_MOUNT_POINT);
            append_dir_listing(text, sizeof(text), BSP_SD_PHOTO_DIR);
            ESP_LOGI(TAG, "\n%s", text);
        }
    }

    /* LVGL objects must be created under the display lock so the LVGL
     * task cannot flush a half-built tree. Timeout 0 = wait forever. */
    bsp_display_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SD Card");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    /* Colored status chip: green PASS = mounted and wrote the test file;
     * red FAIL = missing card, mount error, or write-only failure. */
    lv_obj_t *badge = lv_obj_create(scr);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(badge, 260, 56);
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_radius(badge, 12, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_bg_color(badge, pass ? lv_color_hex(0x1B7F4E) : lv_color_hex(0xB42318), 0);
    lv_obj_t *badge_lab = lv_label_create(badge);
    lv_label_set_text(badge_lab, pass ? "PASS" : "FAIL");
    lv_obj_set_style_text_color(badge_lab, lv_color_white(), 0);
    lv_obj_set_style_text_font(badge_lab, &lv_font_montserrat_22, 0);
    lv_obj_center(badge_lab);

    /* Body label holds the mount path, CID name, and directory snippets.
     * WRAP keeps long file names from running off the 480 px panel. */
    lv_obj_t *body = lv_label_create(scr);
    lv_label_set_text(body, text);
    lv_obj_set_style_text_color(body, lv_color_hex(0xD0D6DE), 0);
    lv_obj_set_width(body, 420);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 148);
    bsp_display_unlock();
}

#include "board_sd.h"
#include "board_storage.h"

#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "board_sd";

void board_sd_lock(void)
{
    bsp_sdcard_lock();
}

void board_sd_unlock(void)
{
    bsp_sdcard_unlock();
}

esp_err_t board_sd_init(void)
{
    return bsp_sdcard_mount();
}

bool board_sd_is_mounted(void)
{
    return bsp_sdcard_get_handle() != NULL;
}

const char *board_sd_mount_point(void)
{
    return BSP_SD_MOUNT_POINT;
}

esp_err_t board_sd_read_file(const char *path, uint8_t **out_data, size_t *out_size,
                             size_t max_size)
{
    if (path == NULL || out_data == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_data = NULL;
    *out_size = 0;

    board_sd_lock();

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "fopen failed '%s': errno=%d", path, errno);
        board_sd_unlock();
        return ESP_FAIL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "fseek failed '%s': errno=%d", path, errno);
        fclose(f);
        board_sd_unlock();
        return ESP_FAIL;
    }

    const long file_size = ftell(f);
    if (file_size <= 0) {
        ESP_LOGE(TAG, "invalid size %ld: %s", file_size, path);
        fclose(f);
        board_sd_unlock();
        return ESP_ERR_INVALID_SIZE;
    }
    if ((size_t)file_size > max_size) {
        ESP_LOGE(TAG, "file too large %ld (max %u): %s", file_size, (unsigned)max_size,
                 path);
        fclose(f);
        board_sd_unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (largest < (size_t)file_size + (256 * 1024)) {
        const size_t internal =
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (internal > largest) {
            largest = internal;
        }
    }
    if (largest < (size_t)file_size) {
        ESP_LOGE(TAG, "no memory for %ld bytes (largest=%u): %s", file_size,
                 (unsigned)largest, path);
        fclose(f);
        board_sd_unlock();
        return ESP_ERR_NO_MEM;
    }

    rewind(f);

    uint8_t *buf =
        heap_caps_malloc((size_t)file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf == NULL) {
        buf = malloc((size_t)file_size);
    }
    if (buf == NULL) {
        ESP_LOGE(TAG, "malloc failed %ld bytes: %s", file_size, path);
        fclose(f);
        board_sd_unlock();
        return ESP_ERR_NO_MEM;
    }

    size_t got = 0;
    while (got < (size_t)file_size) {
        size_t chunk = (size_t)file_size - got;
        if (chunk > 32768) {
            chunk = 32768;
        }
        const size_t n = fread(buf + got, 1, chunk, f);
        if (n == 0) {
            ESP_LOGE(TAG, "fread failed at %u/%ld errno=%d: %s", (unsigned)got, file_size,
                     errno, path);
            heap_caps_free(buf);
            fclose(f);
            board_sd_unlock();
            return ESP_FAIL;
        }
        got += n;
    }
    fclose(f);
    board_sd_unlock();

    *out_data = buf;
    *out_size = (size_t)file_size;
    ESP_LOGI(TAG, "read %s (%ld bytes)", path, file_size);
    return ESP_OK;
}

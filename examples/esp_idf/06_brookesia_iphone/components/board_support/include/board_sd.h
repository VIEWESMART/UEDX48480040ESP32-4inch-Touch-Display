#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_sd_init(void);
bool board_sd_is_mounted(void);
const char *board_sd_mount_point(void);

void board_sd_lock(void);
void board_sd_unlock(void);

/** 从 SD 读取整文件到 PSRAM（分块读、互斥保护）。调用方 heap_caps_free(*out_data)。 */
esp_err_t board_sd_read_file(const char *path, uint8_t **out_data, size_t *out_size,
                             size_t max_size);

#ifdef __cplusplus
}
#endif

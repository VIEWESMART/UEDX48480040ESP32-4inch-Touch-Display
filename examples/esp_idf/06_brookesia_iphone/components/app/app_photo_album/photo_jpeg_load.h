#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 从 POSIX 路径读取 JPEG，优先硬件解码为 RGB565。调用方用 photo_jpeg_free_buf 释放。 */
bool photo_jpeg_load_file(const char *posix_path, uint8_t **out_buf,
                          uint16_t *out_w, uint16_t *out_h, bool *out_hw_buf);

void photo_jpeg_free_buf(uint8_t *buf, bool hw_buf);

/** 释放相册全局 JPEG 缓存，供占内存 App 调用 */
void photo_album_release_cache(void);

#ifdef __cplusplus
}
#endif

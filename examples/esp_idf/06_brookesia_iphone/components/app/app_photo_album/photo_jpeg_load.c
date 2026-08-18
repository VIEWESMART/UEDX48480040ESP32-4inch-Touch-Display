#include "photo_jpeg_load.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_sd.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"
#endif

static const char *TAG = "photo_jpeg";

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
static jpeg_decoder_handle_t s_hw_decoder = NULL;

static bool ensure_hw_decoder(void) {
  if (s_hw_decoder != NULL) {
    return true;
  }
  jpeg_decode_engine_cfg_t eng_cfg = {
      .intr_priority = 0,
      .timeout_ms = 200,
  };
  return jpeg_new_decoder_engine(&eng_cfg, &s_hw_decoder) == ESP_OK;
}

static bool try_hw_decode(const uint8_t *jpg, uint32_t jpg_size, uint8_t **out_buf,
                          uint16_t *out_w, uint16_t *out_h) {
  if (!ensure_hw_decoder()) {
    return false;
  }

  jpeg_decode_picture_info_t pic = {0};
  if (jpeg_decoder_get_info(jpg, jpg_size, &pic) != ESP_OK) {
    return false;
  }
  if (pic.width < 64 || pic.height < 64 || (pic.width % 16) != 0 ||
      (pic.height % 16) != 0) {
    return false;
  }

  const uint32_t w = pic.width;
  const uint32_t h = pic.height;
  const size_t rgb_size = (size_t)w * h * 2;

  jpeg_decode_memory_alloc_cfg_t mem_cfg = {
      .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
  };
  size_t alloc_size = 0;
  uint8_t *rgb = jpeg_alloc_decoder_mem(rgb_size, &mem_cfg, &alloc_size);
  if (rgb == NULL) {
    return false;
  }

  static const jpeg_decode_cfg_t dec_cfg = {
      .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
      .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
  };

  uint32_t out_size = 0;
  if (jpeg_decoder_process(s_hw_decoder, &dec_cfg, jpg, jpg_size, rgb,
                           (uint32_t)alloc_size, &out_size) != ESP_OK) {
    free(rgb);
    return false;
  }

  *out_buf = rgb;
  *out_w = (uint16_t)w;
  *out_h = (uint16_t)h;
  return true;
}
#endif

static bool sw_decode_jpeg(uint8_t *jpg, long file_size, uint8_t **out_buf,
                           uint16_t *out_w, uint16_t *out_h) {
  jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
  config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
  config.rotate = JPEG_ROTATE_0D;

  jpeg_dec_handle_t dec = NULL;
  jpeg_dec_io_t *io = NULL;
  jpeg_dec_header_info_t *info = NULL;
  uint8_t *rgb = NULL;
  bool ok = false;

  if (jpeg_dec_open(&config, &dec) != JPEG_ERR_OK || dec == NULL) {
    ESP_LOGE(TAG, "jpeg_dec_open failed");
    goto sw_done;
  }

  io = calloc(1, sizeof(jpeg_dec_io_t));
  info = calloc(1, sizeof(jpeg_dec_header_info_t));
  if (io == NULL || info == NULL) {
    goto sw_done;
  }

  io->inbuf = jpg;
  io->inbuf_len = (int)file_size;
  if (jpeg_dec_parse_header(dec, io, info) != JPEG_ERR_OK) {
    ESP_LOGE(TAG, "parse header failed");
    goto sw_done;
  }

  int out_len = 0;
  if (jpeg_dec_get_outbuf_len(dec, &out_len) != JPEG_ERR_OK || out_len <= 0) {
    goto sw_done;
  }

  rgb = jpeg_calloc_align((size_t)out_len, 16);
  if (rgb == NULL) {
    goto sw_done;
  }

  io->outbuf = rgb;
  if (jpeg_dec_process(dec, io) != JPEG_ERR_OK) {
    goto sw_done;
  }

  *out_buf = rgb;
  *out_w = info->width;
  *out_h = info->height;
  rgb = NULL;
  ok = true;

sw_done:
  if (rgb != NULL) {
    jpeg_free_align(rgb);
  }
  if (info != NULL) {
    free(info);
  }
  if (io != NULL) {
    free(io);
  }
  if (dec != NULL) {
    jpeg_dec_close(dec);
  }
  return ok;
}

void photo_jpeg_free_buf(uint8_t *buf, bool hw_buf) {
  if (buf == NULL) {
    return;
  }
#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
  if (hw_buf) {
    free(buf);
    return;
  }
#endif
  jpeg_free_align(buf);
}

bool photo_jpeg_load_file(const char *posix_path, uint8_t **out_buf,
                          uint16_t *out_w, uint16_t *out_h, bool *out_hw_buf) {
  if (posix_path == NULL || out_buf == NULL || out_w == NULL || out_h == NULL) {
    return false;
  }

  board_sd_lock();

  const int64_t t0 = esp_timer_get_time();
  bool ok = false;
  FILE *f = NULL;
  uint8_t *jpg = NULL;
  long file_size = 0;
  int64_t t_read = 0;

  *out_buf = NULL;
  *out_w = 0;
  *out_h = 0;
  if (out_hw_buf != NULL) {
    *out_hw_buf = false;
  }

  f = fopen(posix_path, "rb");
  if (f == NULL) {
    ESP_LOGE(TAG, "open failed: %s", posix_path);
    goto done;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    goto done;
  }
  file_size = ftell(f);
  if (file_size <= 0 || file_size > (4 * 1024 * 1024)) {
    ESP_LOGE(TAG, "invalid size %ld: %s", file_size, posix_path);
    goto done;
  }
  rewind(f);

  jpg = heap_caps_malloc((size_t)file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (jpg == NULL) {
    jpg = malloc((size_t)file_size);
  }
  if (jpg == NULL) {
    goto done;
  }

  if (fread(jpg, 1, (size_t)file_size, f) != (size_t)file_size) {
    goto done;
  }
  fclose(f);
  f = NULL;

  t_read = esp_timer_get_time();

  vTaskDelay(pdMS_TO_TICKS(2));

#if CONFIG_SOC_JPEG_DECODE_SUPPORTED
  if (try_hw_decode(jpg, (uint32_t)file_size, out_buf, out_w, out_h)) {
    ok = true;
    if (out_hw_buf != NULL) {
      *out_hw_buf = true;
    }
    ESP_LOGI(TAG, "HW %s -> %ux%u read=%lldms decode=%lldms", posix_path,
             *out_w, *out_h, (long long)(t_read - t0) / 1000,
             (long long)(esp_timer_get_time() - t_read) / 1000);
    goto done;
  }
#endif

  if (sw_decode_jpeg(jpg, file_size, out_buf, out_w, out_h)) {
    ok = true;
    ESP_LOGI(TAG, "SW %s -> %ux%u read=%lldms decode=%lldms", posix_path,
             *out_w, *out_h, (long long)(t_read - t0) / 1000,
             (long long)(esp_timer_get_time() - t_read) / 1000);
  }

done:
  if (jpg != NULL) {
    heap_caps_free(jpg);
  }
  if (f != NULL) {
    fclose(f);
  }
  board_sd_unlock();
  return ok;
}

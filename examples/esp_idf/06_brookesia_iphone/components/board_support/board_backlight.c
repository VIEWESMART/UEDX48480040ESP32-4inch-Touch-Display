#include "board_backlight.h"

#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "board_bl";

#define BL_LEDC_TIMER LEDC_TIMER_0
#define BL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define BL_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define BL_LEDC_FREQUENCY 5000
#define BL_LEDC_MAX_DUTY ((1 << 10) - 1)

static gpio_num_t s_pin __attribute__((unused)) = GPIO_NUM_NC;
static bool s_inited = false;
static uint8_t s_percent = 80;

static void backlight_save_nvs(uint8_t percent) {
  nvs_handle_t nvs = 0;
  if (nvs_open("board", NVS_READWRITE, &nvs) != ESP_OK) {
    return;
  }
  nvs_set_u8(nvs, "bl_pct", percent);
  nvs_commit(nvs);
  nvs_close(nvs);
}

static uint8_t backlight_load_nvs(void) {
  nvs_handle_t nvs = 0;
  uint8_t percent = 80;
  if (nvs_open("board", NVS_READONLY, &nvs) != ESP_OK) {
    return percent;
  }
  nvs_get_u8(nvs, "bl_pct", &percent);
  nvs_close(nvs);
  return percent;
}

esp_err_t board_backlight_init(gpio_num_t pin) {
  if (pin == GPIO_NUM_NC) {
    return ESP_ERR_INVALID_ARG;
  }

  ledc_timer_config_t timer_cfg = {
      .speed_mode = BL_LEDC_MODE,
      .duty_resolution = BL_LEDC_DUTY_RES,
      .timer_num = BL_LEDC_TIMER,
      .freq_hz = BL_LEDC_FREQUENCY,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "ledc timer");

  ledc_channel_config_t ch_cfg = {
      .gpio_num = pin,
      .speed_mode = BL_LEDC_MODE,
      .channel = BL_LEDC_CHANNEL,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = BL_LEDC_TIMER,
      .duty = 0,
      .hpoint = 0,
  };
  ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "ledc channel");

  s_pin = pin;
  s_inited = true;
  board_backlight_set_percent(backlight_load_nvs());
  ESP_LOGI(TAG, "Backlight PWM on GPIO%d, %u%%", pin, s_percent);
  return ESP_OK;
}

void board_backlight_set_percent(uint8_t percent) {
  if (!s_inited) {
    return;
  }
  if (percent > 100) {
    percent = 100;
  }
  s_percent = percent;
  const uint32_t duty = (BL_LEDC_MAX_DUTY * (uint32_t)percent) / 100U;
  ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
  ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
  backlight_save_nvs(percent);
}

uint8_t board_backlight_get_percent(void) { return s_percent; }

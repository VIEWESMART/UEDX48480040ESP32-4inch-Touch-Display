#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_backlight_init(gpio_num_t pin);
void board_backlight_set_percent(uint8_t percent);
uint8_t board_backlight_get_percent(void);

#ifdef __cplusplus
}
#endif

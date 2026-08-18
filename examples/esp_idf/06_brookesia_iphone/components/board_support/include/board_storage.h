#pragma once

#include "bsp/esp-bsp.h"

/* SD 卡引脚与路径由 BSP 提供，此处保留 BOARD_* 别名供 App 使用 */
#define BOARD_SD_ENABLE
#define BOARD_SD_MOUNT_POINT BSP_SD_MOUNT_POINT
#define BOARD_SD_PHOTO_DIR   BSP_SD_PHOTO_DIR
#define BOARD_SD_ANIM_DIR    BSP_SD_ANIM_DIR
#define BOARD_SD_PIN_CS      BSP_SD_SPI_CS
#define BOARD_SD_PIN_MOSI    BSP_SD_SPI_MOSI
#define BOARD_SD_PIN_CLK     BSP_SD_SPI_CLK
#define BOARD_SD_PIN_MISO    BSP_SD_SPI_MISO

/* LVGL FATFS 路径前缀（盘符 S:） */
#define BOARD_LVGL_SD_PATH_PREFIX "S:"

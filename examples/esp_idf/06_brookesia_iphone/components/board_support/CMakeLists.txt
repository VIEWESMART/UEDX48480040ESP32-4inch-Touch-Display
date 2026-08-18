idf_component_register(SRCS "board_sd.c" "board_backlight.c"
                    INCLUDE_DIRS "include"
                    REQUIRES fatfs driver esp_driver_spi esp_driver_ledc
                             bsp_uedx48480040e_wb_a)

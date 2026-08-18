idf_component_register(
    SRCS "wifi_main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp_uedx48480040e_wb_a nvs_flash
)

idf_component_register(
    SRCS "phone_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES bsp_uedx48480040e_wb_a app app_icons
             esp-brookesia esp_lv_decoder nvs_flash
)

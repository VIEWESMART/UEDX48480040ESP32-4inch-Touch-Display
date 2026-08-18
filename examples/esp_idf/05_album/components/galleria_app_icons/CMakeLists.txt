idf_component_register(
    SRCS
        "icon_settings.c"
        "icon_weather.c"
        "icon_album.c"
        "icon_display.c"
        "icon_touch.c"
        "app_icons_round.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl__lvgl
)

target_compile_options(${COMPONENT_LIB} PRIVATE -DLV_LVGL_H_INCLUDE_SIMPLE)

idf_component_register(
    SRCS "album_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES bsp_uedx48480040e_wb_a galleria_apps galleria_app_icons
             esp-brookesia esp_lv_decoder nvs_flash
)

# Do not patch managed_components: inject LVGL 9.2-compatible API for the adapter.
# Use a single-token -include so CMake does not collapse it with the adapter's own -include.
idf_component_get_property(esp_lvgl_adapter_lib espressif__esp_lvgl_adapter COMPONENT_LIB)
target_compile_options(${esp_lvgl_adapter_lib} PRIVATE
    "-include${CMAKE_CURRENT_LIST_DIR}/lv_event_compat.h")


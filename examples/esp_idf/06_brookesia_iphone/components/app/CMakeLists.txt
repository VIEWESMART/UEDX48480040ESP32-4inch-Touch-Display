set(APPS_DIR ./)
file(GLOB_RECURSE APPS_C_SRCS ${APPS_DIR}/*.c)
file(GLOB_RECURSE APPS_CPP_SRCS ${APPS_DIR}/*.cpp)

idf_component_register(
    SRCS ${APPS_C_SRCS} ${APPS_CPP_SRCS}
    INCLUDE_DIRS ${APPS_DIR} ${APPS_DIR}/app_photo_album ${APPS_DIR}/app_weather
    REQUIRES esp-brookesia esp_lv_decoder esp_new_jpeg
             esp_driver_jpeg fatfs board_support esp_lvgl_adapter esp_wifi
             esp_netif nvs_flash app_icons esp_http_client json
             esp-tls
)

target_compile_options(
    ${COMPONENT_LIB}
    PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>
)

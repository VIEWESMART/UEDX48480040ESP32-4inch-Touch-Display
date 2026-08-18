#pragma once

#include "bsp/wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_WIFI_SSID_MAX BSP_WIFI_SSID_MAX
#define BOARD_WIFI_SCAN_MAX BSP_WIFI_SCAN_MAX

typedef bsp_wifi_ap_info_t board_wifi_ap_info_t;

static inline esp_err_t board_wifi_init(void)
{
    return bsp_wifi_init();
}

static inline bool board_wifi_is_initialized(void)
{
    return bsp_wifi_is_initialized();
}

static inline bool board_wifi_is_connected(void)
{
    return bsp_wifi_is_connected();
}

static inline bool board_wifi_is_scanning(void)
{
    return bsp_wifi_is_scanning();
}

static inline int8_t board_wifi_get_rssi(void)
{
    return bsp_wifi_get_rssi();
}

static inline const char *board_wifi_status_text(void)
{
    return bsp_wifi_status_text();
}

static inline void board_wifi_copy_status(char *out, size_t out_len)
{
    bsp_wifi_copy_status(out, out_len);
}

static inline esp_err_t board_wifi_scan(board_wifi_ap_info_t *results, uint16_t *count,
                                        uint16_t max_count)
{
    return bsp_wifi_scan(results, count, max_count);
}

static inline esp_err_t board_wifi_connect(const char *ssid, const char *password)
{
    return bsp_wifi_connect(ssid, password);
}

#ifdef __cplusplus
}
#endif

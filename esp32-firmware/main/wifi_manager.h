#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and connect to Wi-Fi station.
 * @param ssid Wi-Fi SSID.
 * @param password Wi-Fi Password.
 * @param timeout_ms Connection timeout in milliseconds.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_connect_sta(const char *ssid, const char *password, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H


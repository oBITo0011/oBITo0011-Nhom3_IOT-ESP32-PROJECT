#ifndef SNTP_SYNC_H
#define SNTP_SYNC_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SNTP and wait for time synchronization.
 * @param timeout_ms Maximum time in milliseconds to wait for time sync.
 * @return esp_err_t ESP_OK if synchronized, ESP_ERR_TIMEOUT if timed out.
 */
esp_err_t sntp_sync_init(uint32_t timeout_ms);

/**
 * @brief Get current UTC time formatted as ISO 8601 string (e.g. 2026-09-13T08:30:00Z).
 * @param buf Output buffer.
 * @param max_len Size of output buffer.
 * @return true if time is synchronized and formatted, false otherwise.
 */
bool sntp_get_iso8601_utc(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // SNTP_SYNC_H


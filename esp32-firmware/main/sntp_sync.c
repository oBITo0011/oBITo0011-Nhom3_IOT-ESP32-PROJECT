#include "sntp_sync.h"
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SNTP_SYNC";

esp_err_t sntp_sync_init(uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Initializing SNTP client (pool.ntp.org)...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    // Set timezone to UTC
    setenv("TZ", "UTC0", 1);
    tzset();

    // Wait for system time to be updated
    time_t now = 0;
    struct tm timeinfo = { 0 };
    uint32_t waited = 0;
    const uint32_t step = 200;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && waited < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(step));
        waited += step;
    }

    time(&now);
    gmtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "SNTP time synchronization timed out or year < 2020");
        return ESP_ERR_TIMEOUT;
    }

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    ESP_LOGI(TAG, "SNTP synchronized successfully! Current UTC time: %s", strftime_buf);
    return ESP_OK;
}

bool sntp_get_iso8601_utc(char *buf, size_t max_len)
{
    if (!buf || max_len < 21) {
        return false;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);

    // Format as ISO 8601 UTC: YYYY-MM-DDTHH:MM:SSZ
    size_t written = strftime(buf, max_len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return written > 0;
}


#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t tft_display_init(void);

void tft_display_update(
    float temperature,
    float humidity,
    float illuminance,
    float soil_moisture,
    bool led_on,
    bool buzzer_on
);

#endif
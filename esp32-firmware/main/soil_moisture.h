#ifndef SOIL_MOISTURE_H
#define SOIL_MOISTURE_H

#include "esp_err.h"

esp_err_t soil_moisture_init(void);

esp_err_t soil_moisture_read(
    int *raw_value,
    float *moisture_percent
);

#endif
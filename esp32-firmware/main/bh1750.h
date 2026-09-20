#pragma once

#include "esp_err.h"

/**
 * Initialize BH1750 on the I2C bus.
 *
 * Default address is auto-detected at 0x23, then 0x5C.
 */
esp_err_t bh1750_init(void);

/**
 * Read the current illuminance in lux.
 */
esp_err_t bh1750_read_lux(float *lux);
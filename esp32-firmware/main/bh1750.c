#include "bh1750.h"

#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BH1750";

/* =========================================================
 * I2C CONFIG
 * ========================================================= */

/*
 * IMPORTANT:
 * These pins must match main.c and the physical wiring.
 *
 * BH1750 SDA -> ESP32-S3 GPIO8
 * BH1750 SCL -> ESP32-S3 GPIO9
 */
#define I2C_PORT       I2C_NUM_0
#define I2C_SDA_PIN    GPIO_NUM_8
#define I2C_SCL_PIN    GPIO_NUM_9
#define I2C_SPEED_HZ   100000

/* =========================================================
 * BH1750 ADDRESS
 * ========================================================= */

#define BH1750_ADDR_LOW    0x23
#define BH1750_ADDR_HIGH   0x5C

/* =========================================================
 * BH1750 COMMAND
 * ========================================================= */

#define BH1750_POWER_ON    0x01

/*
 * Continuous High Resolution Mode
 *
 * Resolution: 1 lux
 * Measurement time: about 120 ms
 */
#define BH1750_CONT_H      0x10

#define BH1750_MEASURE_MS  180

/* =========================================================
 * GLOBAL
 * ========================================================= */

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_bh1750_dev = NULL;
static bool s_initialized = false;

/* =========================================================
 * TRY BH1750 ADDRESS
 * ========================================================= */

static esp_err_t try_addr(uint8_t addr)
{
    /*
     * Remove old device handle before trying another address.
     */
    if (s_bh1750_dev != NULL) {

        esp_err_t rm_err =
            i2c_master_bus_rm_device(
                s_bh1750_dev
            );

        if (rm_err != ESP_OK) {

            ESP_LOGW(
                TAG,
                "Remove old device 0x%02X failed: %s",
                addr,
                esp_err_to_name(rm_err)
            );
        }

        s_bh1750_dev = NULL;
    }

    /* =====================================================
     * PROBE DEVICE
     * ===================================================== */

    ESP_LOGI(
        TAG,
        "Probing BH1750 address 0x%02X...",
        addr
    );

    esp_err_t err =
        i2c_master_probe(
            s_i2c_bus,
            addr,
            1000
        );

    ESP_LOGI(
        TAG,
        "BH1750 probe 0x%02X -> %s",
        addr,
        esp_err_to_name(err)
    );

    if (err != ESP_OK) {

        if (err == ESP_ERR_TIMEOUT) {

            ESP_LOGE(
                TAG,
                "I2C TIMEOUT while probing 0x%02X. "
                "Check SDA/SCL wiring and pull-up resistors.",
                addr
            );

        } else if (err == ESP_ERR_NOT_FOUND) {

            ESP_LOGW(
                TAG,
                "No device ACK at address 0x%02X.",
                addr
            );
        }

        return err;
    }

    /* =====================================================
     * ADD I2C DEVICE
     * ===================================================== */

    const i2c_device_config_t dev_cfg = {

        .dev_addr_length =
            I2C_ADDR_BIT_LEN_7,

        .device_address =
            addr,

        .scl_speed_hz =
            I2C_SPEED_HZ,
    };

    err =
        i2c_master_bus_add_device(
            s_i2c_bus,
            &dev_cfg,
            &s_bh1750_dev
        );

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Cannot add BH1750 device 0x%02X: %s",
            addr,
            esp_err_to_name(err)
        );

        s_bh1750_dev = NULL;

        return err;
    }

    ESP_LOGI(
        TAG,
        "BH1750 device added at 0x%02X",
        addr
    );

    /* =====================================================
     * POWER ON
     * ===================================================== */

    uint8_t cmd =
        BH1750_POWER_ON;

    err =
        i2c_master_transmit(
            s_bh1750_dev,
            &cmd,
            1,
            1000
        );

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "BH1750 POWER_ON failed: %s",
            esp_err_to_name(err)
        );

        i2c_master_bus_rm_device(
            s_bh1750_dev
        );

        s_bh1750_dev = NULL;

        return err;
    }

    ESP_LOGI(
        TAG,
        "BH1750 POWER_ON OK"
    );

    /* =====================================================
     * START CONTINUOUS HIGH RESOLUTION MODE
     * ===================================================== */

    cmd =
        BH1750_CONT_H;

    err =
        i2c_master_transmit(
            s_bh1750_dev,
            &cmd,
            1,
            1000
        );

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "BH1750 CONT_H failed: %s",
            esp_err_to_name(err)
        );

        i2c_master_bus_rm_device(
            s_bh1750_dev
        );

        s_bh1750_dev = NULL;

        return err;
    }

    ESP_LOGI(
        TAG,
        "BH1750 continuous high-resolution mode started"
    );

    /* =====================================================
     * WAIT FOR FIRST MEASUREMENT
     * ===================================================== */

    vTaskDelay(
        pdMS_TO_TICKS(
            BH1750_MEASURE_MS
        )
    );

    /* =====================================================
     * READY
     * ===================================================== */

    ESP_LOGI(
        TAG,
        "BH1750 READY: address=0x%02X, SDA=GPIO%d, SCL=GPIO%d",
        addr,
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    return ESP_OK;
}

/* =========================================================
 * INITIALIZE BH1750
 * ========================================================= */

esp_err_t bh1750_init(void)
{
    if (s_initialized) {
        ESP_LOGI(
            TAG,
            "BH1750 already initialized"
        );

        return ESP_OK;
    }

    /* =====================================================
     * I2C BUS CONFIG
     * ===================================================== */

    const i2c_master_bus_config_t bus_cfg = {

        .i2c_port =
            I2C_PORT,

        .sda_io_num =
            I2C_SDA_PIN,

        .scl_io_num =
            I2C_SCL_PIN,

        .clk_source =
            I2C_CLK_SRC_DEFAULT,

        .glitch_ignore_cnt =
            7,

        .intr_priority =
            0,

        /*
         * IMPORTANT:
         *
         * Set to 0 because i2c_master_probe()
         * is not compatible with asynchronous
         * transaction queue mode.
         */
        .trans_queue_depth =
            0,

        /*
         * Internal pull-ups enabled.
         *
         * External pull-up resistors are still
         * recommended for reliable I2C.
         */
        .flags.enable_internal_pullup =
            true,
    };

    ESP_LOGI(
        TAG,
        "Creating I2C bus..."
    );

    ESP_LOGI(
        TAG,
        "I2C: port=%d SDA=GPIO%d SCL=GPIO%d speed=%d",
        I2C_PORT,
        I2C_SDA_PIN,
        I2C_SCL_PIN,
        I2C_SPEED_HZ
    );

    esp_err_t err =
        i2c_new_master_bus(
            &bus_cfg,
            &s_i2c_bus
        );

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "I2C bus initialization FAILED: %s",
            esp_err_to_name(err)
        );

        return err;
    }

    ESP_LOGI(
        TAG,
        "I2C bus initialized successfully"
    );

    /* =====================================================
     * TRY ADDRESS 0x23
     * ===================================================== */

    err =
        try_addr(
            BH1750_ADDR_LOW
        );

    if (err == ESP_OK) {

        s_initialized = true;

        ESP_LOGI(
            TAG,
            "BH1750 found at address 0x23"
        );

        return ESP_OK;
    }

    /* =====================================================
     * TRY ADDRESS 0x5C
     * ===================================================== */

    err =
        try_addr(
            BH1750_ADDR_HIGH
        );

    if (err == ESP_OK) {

        s_initialized = true;

        ESP_LOGI(
            TAG,
            "BH1750 found at address 0x5C"
        );

        return ESP_OK;
    }

    /* =====================================================
     * NOT FOUND
     * ===================================================== */

    ESP_LOGE(
        TAG,
        "BH1750 initialization FAILED."
    );

    ESP_LOGE(
        TAG,
        "Tried addresses: 0x23 and 0x5C"
    );

    ESP_LOGE(
        TAG,
        "Current wiring expected: SDA=GPIO%d, SCL=GPIO%d",
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    ESP_LOGE(
        TAG,
        "Check VCC=3.3V, GND=GND, SDA, SCL and I2C pull-up."
    );

    if (s_i2c_bus != NULL) {

        i2c_del_master_bus(
            s_i2c_bus
        );

        s_i2c_bus = NULL;
    }

    s_bh1750_dev = NULL;
    s_initialized = false;

    return ESP_ERR_NOT_FOUND;
}

/* =========================================================
 * READ LUX
 * ========================================================= */

esp_err_t bh1750_read_lux(float *lux)
{
    if (lux == NULL) {

        ESP_LOGE(
            TAG,
            "bh1750_read_lux(): lux is NULL"
        );

        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized ||
        s_bh1750_dev == NULL) {

        ESP_LOGE(
            TAG,
            "BH1750 is not initialized"
        );

        return ESP_ERR_INVALID_STATE;
    }

    /* =====================================================
     * READ 2 BYTES
     * ===================================================== */

    uint8_t raw[2] = {0};

    esp_err_t err =
        i2c_master_receive(
            s_bh1750_dev,
            raw,
            sizeof(raw),
            1000
        );

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "BH1750 READ FAILED: %s",
            esp_err_to_name(err)
        );

        return err;
    }

    /* =====================================================
     * CONVERT RAW DATA TO LUX
     * ===================================================== */

    uint16_t raw_value =
        ((uint16_t)raw[0] << 8) |
        raw[1];

    /*
     * BH1750 conversion:
     *
     * Lux = raw_value / 1.2
     */
    *lux =
        ((float)raw_value) / 1.2f;

    /* =====================================================
     * DEBUG LOG
     * ===================================================== */

    ESP_LOGI(
        TAG,
        "BH1750 READ OK: raw=0x%04X (%u) -> %.1f lux",
        raw_value,
        raw_value,
        *lux
    );

    return ESP_OK;
}
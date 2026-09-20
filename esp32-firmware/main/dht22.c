#include "dht22.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"


static gpio_num_t dht_pin;


// =====================================================
// WAIT LEVEL
// =====================================================

static int wait_for_level(
    int level,
    uint32_t timeout_us)
{
    int64_t start =
        esp_timer_get_time();


    while (
        gpio_get_level(dht_pin) != level
    )
    {
        if (
            (esp_timer_get_time() - start)
            > timeout_us
        )
        {
            return -1;
        }
    }


    return 0;
}


// =====================================================
// INIT
// =====================================================

void dht22_init(
    gpio_num_t pin)
{
    dht_pin = pin;


    gpio_config_t config =
    {
        .pin_bit_mask =
            (1ULL << dht_pin),

        .mode =
            GPIO_MODE_INPUT_OUTPUT_OD,

        .pull_up_en =
            GPIO_PULLUP_ENABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE
    };


    gpio_config(
        &config
    );


    gpio_set_level(
        dht_pin,
        1
    );
}


// =====================================================
// READ
// =====================================================

int dht22_read(
    float *temperature,
    float *humidity)
{
    uint8_t data[5] =
        {0, 0, 0, 0, 0};


    // =================================================
    // START SIGNAL
    // =================================================

    gpio_set_direction(
        dht_pin,
        GPIO_MODE_OUTPUT_OD
    );


    gpio_set_level(
        dht_pin,
        0
    );


    /*
     * DHT22 cần LOW ít nhất khoảng 1 ms.
     *
     * Dùng microsecond delay thay vì FreeRTOS delay
     * để timing chính xác.
     */
    esp_rom_delay_us(
        2000
    );


    // Release bus
    gpio_set_level(
        dht_pin,
        1
    );


    esp_rom_delay_us(
        30
    );


    // Sensor điều khiển DATA
    gpio_set_direction(
        dht_pin,
        GPIO_MODE_INPUT
    );


    // =================================================
    // SENSOR RESPONSE
    // =================================================

    // LOW response
    if (
        wait_for_level(
            0,
            200
        ) != 0
    )
    {
        return -1;
    }


    // HIGH response
    if (
        wait_for_level(
            1,
            200
        ) != 0
    )
    {
        return -2;
    }


    // LOW before first bit
    if (
        wait_for_level(
            0,
            200
        ) != 0
    )
    {
        return -3;
    }


    // =================================================
    // READ 40 BITS
    // =================================================

    for (
        int i = 0;
        i < 40;
        i++
    )
    {

        // Wait LOW -> HIGH
        if (
            wait_for_level(
                1,
                150
            ) != 0
        )
        {
            return -4;
        }


        int64_t high_start =
            esp_timer_get_time();


        // Wait HIGH -> LOW
        if (
            wait_for_level(
                0,
                200
            ) != 0
        )
        {
            return -5;
        }


        int64_t high_duration =
            esp_timer_get_time()
            - high_start;


        data[i / 8] <<= 1;


        /*
         * Bit 0: HIGH khoảng 26–28 us
         * Bit 1: HIGH khoảng 70 us
         */
        if (
            high_duration > 50
        )
        {
            data[i / 8] |= 1;
        }
    }


    // =================================================
    // CHECKSUM
    // =================================================

    uint8_t checksum =
        (uint8_t)(
            data[0] +
            data[1] +
            data[2] +
            data[3]
        );


    if (
        checksum != data[4]
    )
    {
        return -6;
    }


    // =================================================
    // HUMIDITY
    // =================================================

    uint16_t raw_humidity =
        ((uint16_t)data[0] << 8)
        | data[1];


    *humidity =
        raw_humidity / 10.0f;


    // =================================================
    // TEMPERATURE
    // =================================================

    uint16_t raw_temperature =
        ((uint16_t)
            (data[2] & 0x7F)
            << 8)
        | data[3];


    *temperature =
        raw_temperature / 10.0f;


    // Negative temperature
    if (
        data[2] & 0x80
    )
    {
        *temperature =
            -*temperature;
    }


    return 0;
}
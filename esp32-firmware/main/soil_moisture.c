#include "soil_moisture.h"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "SOIL";

/*
 * ESP32-S3
 *
 * GPIO5 = ADC1_CHANNEL_4
 */
#define SOIL_ADC_UNIT       ADC_UNIT_1
#define SOIL_ADC_CHANNEL    ADC_CHANNEL_4
#define SOIL_ADC_ATTEN      ADC_ATTEN_DB_12
#define SOIL_ADC_BITWIDTH   ADC_BITWIDTH_DEFAULT

/*
 * Calibration measured on this sensor:
 * - Outside air / dry: ADC 4095 -> 0%
 * - Moist (mouth test): ADC 2298 -> 100%
 *
 * This type of AO sensor produces a lower ADC value as moisture increases.
 * Re-measure SOIL_RAW_WET after testing in saturated soil or water if needed.
 */
#define SOIL_RAW_DRY        4095
#define SOIL_RAW_WET        2298

/*
 * ADC handle
 */
static adc_oneshot_unit_handle_t adc_handle = NULL;


/*
 * ============================================================
 * KHỞI TẠO CẢM BIẾN ĐỘ ẨM ĐẤT
 * ============================================================
 */
esp_err_t soil_moisture_init(void)
{
    ESP_LOGI(
        TAG,
        "Initializing soil moisture sensor on GPIO5..."
    );

    /*
     * Cấu hình ADC Unit 1
     */
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = SOIL_ADC_UNIT
    };

    esp_err_t ret = adc_oneshot_new_unit(
        &init_config,
        &adc_handle
    );

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "ADC unit initialization failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
     * Cấu hình ADC channel
     */
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = SOIL_ADC_BITWIDTH,
        .atten = SOIL_ADC_ATTEN
    };

    ret = adc_oneshot_config_channel(
        adc_handle,
        SOIL_ADC_CHANNEL,
        &config
    );

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "ADC channel configuration failed: %s",
            esp_err_to_name(ret)
        );

        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;

        return ret;
    }

    ESP_LOGI(
        TAG,
        "Soil moisture sensor initialized successfully"
    );

    ESP_LOGI(
        TAG,
        "GPIO5 -> ADC1_CHANNEL_4"
    );

    return ESP_OK;
}


/*
 * ============================================================
 * ĐỌC CẢM BIẾN ĐỘ ẨM ĐẤT
 * ============================================================
 */
esp_err_t soil_moisture_read(
    int *raw_value,
    float *moisture_percent
)
{
    /*
     * Kiểm tra tham số
     */
    if (raw_value == NULL ||
        moisture_percent == NULL) {

        return ESP_ERR_INVALID_ARG;
    }

    /*
     * Kiểm tra ADC đã được khởi tạo chưa
     */
    if (adc_handle == NULL) {

        ESP_LOGE(
            TAG,
            "ADC has not been initialized"
        );

        return ESP_ERR_INVALID_STATE;
    }

    /*
     * Đọc giá trị ADC
     */
    int raw = 0;

    esp_err_t ret = adc_oneshot_read(
        adc_handle,
        SOIL_ADC_CHANNEL,
        &raw
    );

    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "ADC read failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
     * ADC -> moisture percentage, calibrated with the measured dry/wet values.
     * The conversion is intentionally inverted: lower ADC means wetter soil.
     */
    float percent =
        ((float)(SOIL_RAW_DRY - raw) * 100.0f) /
        (float)(SOIL_RAW_DRY - SOIL_RAW_WET);


    /*
     * Giới hạn giá trị trong khoảng 0 - 100%
     */
    if (percent < 0.0f) {
        percent = 0.0f;
    }

    if (percent > 100.0f) {
        percent = 100.0f;
    }


    /*
     * Trả kết quả
     */
    *raw_value = raw;
    *moisture_percent = percent;


    /*
     * In log
     */
    ESP_LOGI(
        TAG,
        "Soil moisture: ADC=%d -> %.1f%%",
        raw,
        percent
    );


    return ESP_OK;
}

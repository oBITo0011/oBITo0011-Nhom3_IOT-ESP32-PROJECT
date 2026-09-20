#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "cJSON.h"

#include "dht22.h"
#include "bh1750.h"
#include "soil_moisture.h"
#include "sntp_sync.h"
#include "wifi_manager.h"

// TFT
#include "tft_display.h"


static const char *TAG = "IOT_ESP32";


#ifdef CONFIG_IOT_DEVICE_ID
#define DEVICE_ID CONFIG_IOT_DEVICE_ID
#else
#define DEVICE_ID "esp32-001"
#endif


#ifdef CONFIG_MQTT_BROKER_URL
#define MQTT_BROKER_URL CONFIG_MQTT_BROKER_URL
#else
#define MQTT_BROKER_URL "mqtt://10.41.84.231:1883"
#endif


#ifdef CONFIG_DHT_DATA_GPIO
#define DHT_PIN ((gpio_num_t)CONFIG_DHT_DATA_GPIO)
#else
#define DHT_PIN GPIO_NUM_15
#endif


#ifdef CONFIG_LED_GPIO
#define LED_PIN ((gpio_num_t)CONFIG_LED_GPIO)
#else
#define LED_PIN GPIO_NUM_2
#endif


#ifdef CONFIG_BUZZER_GPIO
#define BUZZER_PIN ((gpio_num_t)CONFIG_BUZZER_GPIO)
#else
#define BUZZER_PIN GPIO_NUM_4
#endif


// ============================================================
// I2C
// ============================================================

#define I2C_SDA_PIN GPIO_NUM_8
#define I2C_SCL_PIN GPIO_NUM_9


// ============================================================
// SOIL MOISTURE
// HW-103 AO -> GPIO5
// ============================================================

#define SOIL_MOISTURE_GPIO GPIO_NUM_5


// ============================================================
// RUNTIME ALERT SETTINGS
// ============================================================

static float s_buzzer_temp_threshold = 32.0f;
static float s_soil_moisture_min = 30.0f;
static float s_soil_moisture_max = 80.0f;


#ifdef CONFIG_ESP_WIFI_SSID
#define WIFI_SSID CONFIG_ESP_WIFI_SSID
#else
#define WIFI_SSID "vivo X200 Ultra"
#endif


#ifdef CONFIG_ESP_WIFI_PASSWORD
#define WIFI_PASSWORD CONFIG_ESP_WIFI_PASSWORD
#else
#define WIFI_PASSWORD "Pat270905"
#endif


// ============================================================
// GLOBAL STATE
// ============================================================

static bool s_led_state = false;
static bool s_buzzer_state = false;
/* Manual dashboard commands take priority until a new threshold is configured. */
static bool s_buzzer_manual_override = false;
static bool s_mqtt_connected = false;
static bool s_bh1750_ok = false;

/* Last telemetry values allow the TFT to refresh immediately after a command. */
static bool s_has_sensor_reading = false;
static float s_last_temperature = 0.0f;
static float s_last_humidity = 0.0f;
static float s_last_illuminance = 0.0f;
static float s_last_soil_moisture = 0.0f;

static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static char s_lwt_topic[128];
static char s_lwt_payload[192];


// ============================================================
// TIMESTAMP
// ============================================================

static void fill_timestamp(char *buf, size_t len)
{
    if (!sntp_get_iso8601_utc(buf, len)) {

        strncpy(
            buf,
            "2026-09-14T00:00:00Z",
            len - 1
        );

        buf[len - 1] = '\0';
    }
}


// ============================================================
// MQTT ONLINE STATUS
// ============================================================

static void publish_online_status(
    esp_mqtt_client_handle_t client
)
{
    char timestamp[32];

    fill_timestamp(
        timestamp,
        sizeof(timestamp)
    );


    char status_topic[128];

    snprintf(
        status_topic,
        sizeof(status_topic),
        "device/%s/status",
        DEVICE_ID
    );


    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(
        root,
        "deviceId",
        DEVICE_ID
    );

    cJSON_AddStringToObject(
        root,
        "status",
        "ONLINE"
    );

    cJSON_AddStringToObject(
        root,
        "timestamp",
        timestamp
    );


    char *payload = cJSON_PrintUnformatted(root);

    if (payload) {

        int msg_id =
            esp_mqtt_client_publish(
                client,
                status_topic,
                payload,
                0,
                1,
                1
            );

        ESP_LOGI(
            TAG,
            "Published ONLINE to %s (msg_id=%d): %s",
            status_topic,
            msg_id,
            payload
        );

        free(payload);
    }


    cJSON_Delete(root);
}


// ============================================================
// HANDLE MQTT COMMAND
// ============================================================

static void handle_command(
    esp_mqtt_client_handle_t client,
    const char *data,
    int data_len
)
{
    if (data_len <= 0) {
        return;
    }


    char *json_buf =
        malloc((size_t)data_len + 1);

    if (!json_buf) {

        ESP_LOGE(
            TAG,
            "Failed to allocate memory for command JSON"
        );

        return;
    }


    memcpy(
        json_buf,
        data,
        (size_t)data_len
    );

    json_buf[data_len] = '\0';


    ESP_LOGI(
        TAG,
        "Processing incoming command: %s",
        json_buf
    );


    cJSON *root =
        cJSON_Parse(json_buf);

    free(json_buf);


    if (!root) {

        ESP_LOGE(
            TAG,
            "JSON parsing error"
        );

        return;
    }


    cJSON *cmd_id_item =
        cJSON_GetObjectItem(
            root,
            "commandId"
        );


    cJSON *action_item =
        cJSON_GetObjectItem(
            root,
            "action"
        );


    if (!cJSON_IsString(cmd_id_item) ||
        !cJSON_IsString(action_item)) {

        ESP_LOGE(
            TAG,
            "Invalid command payload"
        );

        cJSON_Delete(root);

        return;
    }


    const char *command_id =
        cmd_id_item->valuestring;

    const char *action =
        action_item->valuestring;

    bool command_success = true;


    ESP_LOGI(
        TAG,
        "Command ID: %s, Action: %s",
        command_id,
        action
    );


    // ========================================================
    // LED
    // ========================================================

    if (strcmp(action, "LED_ON") == 0) {

        s_led_state = true;

        gpio_set_level(
            LED_PIN,
            1
        );

        ESP_LOGI(
            TAG,
            "LED ON (GPIO %d)",
            LED_PIN
        );

    }

    else if (strcmp(action, "LED_OFF") == 0) {

        s_led_state = false;

        gpio_set_level(
            LED_PIN,
            0
        );

        ESP_LOGI(
            TAG,
            "LED OFF (GPIO %d)",
            LED_PIN
        );
    }


    // ========================================================
    // BUZZER
    // ========================================================

    else if (strcmp(action, "BUZZER_ON") == 0) {

        s_buzzer_state = true;
        s_buzzer_manual_override = true;

        gpio_set_level(
            BUZZER_PIN,
            1
        );

        ESP_LOGI(
            TAG,
            "BUZZER ON (GPIO %d)",
            BUZZER_PIN
        );
    }


    else if (strcmp(action, "BUZZER_OFF") == 0) {

        s_buzzer_state = false;
        s_buzzer_manual_override = true;

        gpio_set_level(
            BUZZER_PIN,
            0
        );

        ESP_LOGI(
            TAG,
            "BUZZER OFF (GPIO %d)",
            BUZZER_PIN
        );
    }


    // ========================================================
    // ALERT THRESHOLDS
    // ========================================================

    else if (strcmp(action, "SET_THRESHOLD") == 0) {

        cJSON *temperature_item =
            cJSON_GetObjectItem(
                root,
                "temperatureThreshold"
            );

        cJSON *soil_min_item =
            cJSON_GetObjectItem(
                root,
                "soilMoistureMin"
            );

        cJSON *soil_max_item =
            cJSON_GetObjectItem(
                root,
                "soilMoistureMax"
            );


        if (!cJSON_IsNumber(temperature_item) ||
            !cJSON_IsNumber(soil_min_item) ||
            !cJSON_IsNumber(soil_max_item) ||

            temperature_item->valuedouble <= 0.0 ||
            temperature_item->valuedouble > 100.0 ||

            soil_min_item->valuedouble < 0.0 ||
            soil_min_item->valuedouble > 100.0 ||

            soil_max_item->valuedouble < 0.0 ||
            soil_max_item->valuedouble > 100.0 ||

            soil_min_item->valuedouble >
            soil_max_item->valuedouble) {

            command_success = false;

            ESP_LOGE(
                TAG,
                "Invalid SET_THRESHOLD payload"
            );

        }

        else {

            s_buzzer_temp_threshold =
                (float)temperature_item->valuedouble;

            s_soil_moisture_min =
                (float)soil_min_item->valuedouble;

            s_soil_moisture_max =
                (float)soil_max_item->valuedouble;

            /* Saving a threshold resumes automatic buzzer control. */
            s_buzzer_manual_override = false;


            ESP_LOGI(
                TAG,
                "Received command: SET_THRESHOLD"
            );

            ESP_LOGI(
                TAG,
                "Temperature threshold updated: %.1f C",
                s_buzzer_temp_threshold
            );

            ESP_LOGI(
                TAG,
                "Soil moisture range updated: %.1f%% - %.1f%%",
                s_soil_moisture_min,
                s_soil_moisture_max
            );
        }
    }


    // ========================================================
    // UNKNOWN COMMAND
    // ========================================================

    else {

        command_success = false;

        ESP_LOGW(
            TAG,
            "Unknown action: %s",
            action
        );
    }


    /* Reflect LED/Buzzer commands on the TFT without waiting for the next sample. */
    if (s_has_sensor_reading) {
        tft_display_update(
            s_last_temperature,
            s_last_humidity,
            s_last_illuminance,
            s_last_soil_moisture,
            s_led_state,
            s_buzzer_state
        );
    }


    // ========================================================
    // ACK
    // ========================================================

    char timestamp[32];

    fill_timestamp(
        timestamp,
        sizeof(timestamp)
    );


    char ack_topic[128];

    snprintf(
        ack_topic,
        sizeof(ack_topic),
        "device/%s/command/ack",
        DEVICE_ID
    );


    cJSON *ack =
        cJSON_CreateObject();


    cJSON_AddStringToObject(
        ack,
        "commandId",
        command_id
    );


    cJSON_AddStringToObject(
        ack,
        "deviceId",
        DEVICE_ID
    );


    cJSON_AddStringToObject(
        ack,
        "action",
        action
    );


    cJSON_AddStringToObject(
        ack,
        "status",
        command_success ? "SUCCESS" : "FAILED"
    );


    cJSON_AddBoolToObject(
        ack,
        "led",
        s_led_state
    );


    cJSON_AddStringToObject(
        ack,
        "timestamp",
        timestamp
    );


    char *ack_str =
        cJSON_PrintUnformatted(ack);


    if (ack_str) {

        int msg_id =
            esp_mqtt_client_publish(
                client,
                ack_topic,
                ack_str,
                0,
                1,
                0
            );


        ESP_LOGI(
            TAG,
            "Sent ACK to %s (msg_id=%d): %s",
            ack_topic,
            msg_id,
            ack_str
        );


        free(ack_str);
    }


    cJSON_Delete(ack);
    cJSON_Delete(root);
}


// ============================================================
// MQTT EVENT HANDLER
// ============================================================

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    (void)handler_args;
    (void)base;


    esp_mqtt_event_handle_t event =
        event_data;


    esp_mqtt_client_handle_t client =
        event->client;


    switch (
        (esp_mqtt_event_id_t)event_id
    ) {

    // --------------------------------------------------------
    // CONNECTED
    // --------------------------------------------------------

    case MQTT_EVENT_CONNECTED:

        s_mqtt_connected = true;


        ESP_LOGI(
            TAG,
            "MQTT_EVENT_CONNECTED to %s",
            MQTT_BROKER_URL
        );


        publish_online_status(client);


        {
            char cmd_topic[128];


            snprintf(
                cmd_topic,
                sizeof(cmd_topic),
                "device/%s/command",
                DEVICE_ID
            );


            int sub_id =
                esp_mqtt_client_subscribe(
                    client,
                    cmd_topic,
                    1
                );


            ESP_LOGI(
                TAG,
                "Subscribed to %s (sub_id=%d)",
                cmd_topic,
                sub_id
            );
        }

        break;


    // --------------------------------------------------------
    // DISCONNECTED
    // --------------------------------------------------------

    case MQTT_EVENT_DISCONNECTED:

        s_mqtt_connected = false;


        ESP_LOGW(
            TAG,
            "MQTT_EVENT_DISCONNECTED"
        );

        break;


    // --------------------------------------------------------
    // DATA
    // --------------------------------------------------------

    case MQTT_EVENT_DATA:

        ESP_LOGI(
            TAG,
            "MQTT_EVENT_DATA on %.*s",
            event->topic_len,
            event->topic
        );


        handle_command(
            client,
            event->data,
            event->data_len
        );

        break;


    // --------------------------------------------------------
    // ERROR
    // --------------------------------------------------------

    case MQTT_EVENT_ERROR:

        ESP_LOGE(
            TAG,
            "MQTT_EVENT_ERROR"
        );

        break;


    default:

        break;
    }
}


// ============================================================
// MQTT START
// ============================================================

static void mqtt_app_start(void)
{
    snprintf(
        s_lwt_topic,
        sizeof(s_lwt_topic),
        "device/%s/status",
        DEVICE_ID
    );


    char timestamp[32];

    fill_timestamp(
        timestamp,
        sizeof(timestamp)
    );


    snprintf(
        s_lwt_payload,
        sizeof(s_lwt_payload),
        "{\"deviceId\":\"%s\",\"status\":\"OFFLINE\",\"timestamp\":\"%s\"}",
        DEVICE_ID,
        timestamp
    );


    ESP_LOGI(
        TAG,
        "MQTT Last Will: %s %s",
        s_lwt_topic,
        s_lwt_payload
    );


    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri =
            MQTT_BROKER_URL,

        .session.last_will = {

            .topic = s_lwt_topic,

            .msg = s_lwt_payload,

            .msg_len =
                strlen(s_lwt_payload),

            .qos = 1,

            .retain = 1
        }
    };


    s_mqtt_client =
        esp_mqtt_client_init(
            &mqtt_cfg
        );


    esp_mqtt_client_register_event(
        s_mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );


    esp_mqtt_client_start(
        s_mqtt_client
    );
}


// ============================================================
// TELEMETRY TASK
// ============================================================

static void telemetry_task(
    void *pvParameters
)
{
    (void)pvParameters;


    char telemetry_topic[128];


    snprintf(
        telemetry_topic,
        sizeof(telemetry_topic),
        "device/%s/telemetry",
        DEVICE_ID
    );


    // Cho hệ thống ổn định trước khi đọc sensor
    vTaskDelay(
        pdMS_TO_TICKS(3000)
    );


    while (1) {

        if (s_mqtt_connected) {

            float temp = 0.0f;
            float hum = 0.0f;


            // =================================================
            // DHT22
            // =================================================

            int rc =
                dht22_read(
                    &temp,
                    &hum
                );


            if (rc == 0) {

                ESP_LOGI(
                    TAG,
                    "DHT22 OK -> T=%.1f C, H=%.1f %%",
                    temp,
                    hum
                );


                // =================================================
                // AUTOMATIC TEMPERATURE ALARM
                //
                // > threshold -> BUZZER ON
                // <= threshold -> BUZZER OFF
                // =================================================

                if (!s_buzzer_manual_override &&
                    temp > s_buzzer_temp_threshold) {

                    if (!s_buzzer_state) {

                        ESP_LOGW(
                            TAG,
                            "TEMPERATURE ALERT: %.1f C > %.1f C -> BUZZER ON",
                            temp,
                            s_buzzer_temp_threshold
                        );
                    }


                    s_buzzer_state = true;


                    gpio_set_level(
                        BUZZER_PIN,
                        1
                    );

                }

                else if (!s_buzzer_manual_override) {

                    if (s_buzzer_state) {

                        ESP_LOGI(
                            TAG,
                            "TEMPERATURE NORMAL: %.1f C <= %.1f C -> BUZZER OFF",
                            temp,
                            s_buzzer_temp_threshold
                        );
                    }


                    s_buzzer_state = false;


                    gpio_set_level(
                        BUZZER_PIN,
                        0
                    );
                }


                // =================================================
                // 35 C INFORMATION LOG
                // =================================================

                if (temp > 35.0f) {

                    ESP_LOGW(
                        TAG,
                        "ALERT: temperature %.1f C > 35",
                        temp
                    );
                }


                // =================================================
                // TIMESTAMP
                // =================================================

                char timestamp[32];


                fill_timestamp(
                    timestamp,
                    sizeof(timestamp)
                );


                // =================================================
                // ROUND VALUE
                // =================================================

                double rounded_temp =
                    ((int)(
                        temp * 10.0f +
                        (temp >= 0 ? 0.5f : -0.5f)
                    )) / 10.0;


                double rounded_hum =
                    ((int)(
                        hum * 10.0f +
                        0.5f
                    )) / 10.0;


                // =================================================
                // JSON ROOT
                // =================================================

                cJSON *root =
                    cJSON_CreateObject();


                cJSON_AddStringToObject(
                    root,
                    "deviceId",
                    DEVICE_ID
                );


                cJSON_AddNumberToObject(
                    root,
                    "temperature",
                    rounded_temp
                );


                cJSON_AddNumberToObject(
                    root,
                    "humidity",
                    rounded_hum
                );


                // =================================================
                // BH1750
                // =================================================

                float lux = 0.0f;


                if (s_bh1750_ok) {

                    if (
                        bh1750_read_lux(&lux)
                        == ESP_OK
                    ) {

                        double rounded_lux =
                            ((int)(
                                lux * 10.0f +
                                0.5f
                            )) / 10.0;


                        cJSON_AddNumberToObject(
                            root,
                            "illuminance",
                            rounded_lux
                        );


                        ESP_LOGI(
                            TAG,
                            "BH1750 real lux=%.1f",
                            lux
                        );

                    }

                    else {

                        cJSON_AddNullToObject(
                            root,
                            "illuminance"
                        );
                    }

                }

                else {

                    cJSON_AddNullToObject(
                        root,
                        "illuminance"
                    );
                }


                // =================================================
                // SOIL MOISTURE
                // HW-103 AO -> GPIO5
                // =================================================

                int soil_raw = 0;
                float soil_moisture = 0.0f;


                esp_err_t soil_ret =
                    soil_moisture_read(
                        &soil_raw,
                        &soil_moisture
                    );


                if (soil_ret == ESP_OK) {

                    ESP_LOGI(
                        TAG,
                        "SOIL MOISTURE OK -> ADC=%d, Moisture=%.1f%%",
                        soil_raw,
                        soil_moisture
                    );


                    // ---------------------------------------------
                    // Soil moisture alert information
                    // ---------------------------------------------

                    if (
                        soil_moisture <
                        s_soil_moisture_min
                    ) {

                        ESP_LOGW(
                            TAG,
                            "SOIL TOO DRY: %.1f%% < %.1f%%",
                            soil_moisture,
                            s_soil_moisture_min
                        );
                    }

                    else if (
                        soil_moisture >
                        s_soil_moisture_max
                    ) {

                        ESP_LOGW(
                            TAG,
                            "SOIL TOO WET: %.1f%% > %.1f%%",
                            soil_moisture,
                            s_soil_moisture_max
                        );
                    }


                    // ---------------------------------------------
                    // Add soil moisture to MQTT JSON
                    // ---------------------------------------------

                    double rounded_soil =
                        ((int)(
                            soil_moisture * 10.0f +
                            0.5f
                        )) / 10.0;


                    cJSON_AddNumberToObject(
                        root,
                        "soilMoisture",
                        rounded_soil
                    );

                }

                else {

                    ESP_LOGW(
                        TAG,
                        "Soil moisture read failed"
                    );


                    cJSON_AddNullToObject(
                        root,
                        "soilMoisture"
                    );
                }


                // =================================================
                // LED STATE
                // =================================================

                cJSON_AddBoolToObject(
                    root,
                    "led",
                    s_led_state
                );


                // =================================================
                // TIMESTAMP
                // =================================================

                cJSON_AddStringToObject(
                    root,
                    "timestamp",
                    timestamp
                );


                // =================================================
                // UPDATE TFT
                // =================================================
                //
                // Hiển thị:
                // Temperature
                // Humidity
                // Illuminance
                // Soil Moisture
                // LED
                // Buzzer
                //
                // =================================================

                tft_display_update(
                    temp,
                    hum,
                    lux,
                    soil_moisture,
                    s_led_state,
                    s_buzzer_state
                );

                s_last_temperature = temp;
                s_last_humidity = hum;
                s_last_illuminance = lux;
                s_last_soil_moisture = soil_moisture;
                s_has_sensor_reading = true;


                // =================================================
                // MQTT TELEMETRY
                // =================================================

                char *payload =
                    cJSON_PrintUnformatted(
                        root
                    );


                if (payload) {

                    int msg_id =
                        esp_mqtt_client_publish(
                            s_mqtt_client,
                            telemetry_topic,
                            payload,
                            0,
                            0,
                            0
                        );


                    ESP_LOGI(
                        TAG,
                        "Published Telemetry (msg_id=%d): %s",
                        msg_id,
                        payload
                    );


                    free(payload);
                }


                cJSON_Delete(root);

            }

            else {

                ESP_LOGE(
                    TAG,
                    "DHT22 read failed (rc=%d)",
                    rc
                );
            }

        }

        else {

            ESP_LOGW(
                TAG,
                "Waiting for MQTT before telemetry..."
            );
        }


        // =================================================
        // 5 SECOND INTERVAL
        // =================================================

        vTaskDelay(
            pdMS_TO_TICKS(5000)
        );
    }
}


// ============================================================
// APP MAIN
// ============================================================

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "=================================================="
    );

    ESP_LOGI(
        TAG,
        "  IoT Smart Environment Monitoring & Control"
    );

    ESP_LOGI(
        TAG,
        "  Device ID: %s",
        DEVICE_ID
    );

    ESP_LOGI(
        TAG,
        "  Buzzer temperature threshold: %.1f C",
        s_buzzer_temp_threshold
    );

    ESP_LOGI(
        TAG,
        "  Soil moisture range: %.1f%% - %.1f%%",
        s_soil_moisture_min,
        s_soil_moisture_max
    );

    ESP_LOGI(
        TAG,
        "  Soil sensor AO: GPIO%d",
        SOIL_MOISTURE_GPIO
    );

    ESP_LOGI(
        TAG,
        "=================================================="
    );


    // ========================================================
    // NVS
    // ========================================================

    esp_err_t ret =
        nvs_flash_init();


    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {

        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );


        ret =
            nvs_flash_init();
    }


    ESP_ERROR_CHECK(ret);


    // ========================================================
    // NETWORK
    // ========================================================

    ESP_ERROR_CHECK(
        esp_netif_init()
    );


    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );


    // ========================================================
    // LED GPIO
    // ========================================================

    gpio_reset_pin(
        LED_PIN
    );


    gpio_set_direction(
        LED_PIN,
        GPIO_MODE_OUTPUT
    );


    gpio_set_level(
        LED_PIN,
        0
    );


    ESP_LOGI(
        TAG,
        "LED GPIO %d = OFF",
        LED_PIN
    );


    // ========================================================
    // BUZZER GPIO
    // ========================================================

    gpio_reset_pin(
        BUZZER_PIN
    );


    gpio_set_direction(
        BUZZER_PIN,
        GPIO_MODE_OUTPUT
    );


    gpio_set_level(
        BUZZER_PIN,
        0
    );


    ESP_LOGI(
        TAG,
        "BUZZER GPIO %d = OFF",
        BUZZER_PIN
    );


    // ========================================================
    // DHT22
    // ========================================================

    dht22_init(
        DHT_PIN
    );


    // ========================================================
    // BH1750
    // ========================================================

    if (
        bh1750_init()
        == ESP_OK
    ) {

        s_bh1750_ok = true;


        ESP_LOGI(
            TAG,
            "BH1750 ready (SDA=%d, SCL=%d)",
            I2C_SDA_PIN,
            I2C_SCL_PIN
        );

    }

    else {

        s_bh1750_ok = false;


        ESP_LOGW(
            TAG,
            "BH1750 not found, illuminance will be null"
        );


        ESP_LOGW(
            TAG,
            "Check wiring: SDA=GPIO8, SCL=GPIO9, VCC=3V3, GND=GND, I2C addr=0x23 or 0x5C"
        );
    }


    // ========================================================
    // SOIL MOISTURE SENSOR
    // HW-103 AO -> GPIO5
    // ========================================================

    ESP_LOGI(
        TAG,
        "Initializing soil moisture sensor..."
    );


    ret =
        soil_moisture_init();


    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Soil moisture sensor initialization failed: %s",
            esp_err_to_name(ret)
        );

    }

    else {

        ESP_LOGI(
            TAG,
            "Soil moisture sensor initialized successfully"
        );

        ESP_LOGI(
            TAG,
            "HW-103 AO connected to GPIO%d",
            SOIL_MOISTURE_GPIO
        );
    }


    // ========================================================
    // TFT ST7789
    // ========================================================

    ESP_LOGI(
        TAG,
        "Initializing TFT ST7789..."
    );


    ret =
        tft_display_init();


    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "TFT initialization failed: %s",
            esp_err_to_name(ret)
        );

    }

    else {

        ESP_LOGI(
            TAG,
            "TFT ST7789 initialized successfully"
        );
    }


    // ========================================================
    // WIFI
    // ========================================================

    ESP_LOGI(
        TAG,
        "Connecting to Wi-Fi (%s)...",
        WIFI_SSID
    );


    ret =
        wifi_connect_sta(
            WIFI_SSID,
            WIFI_PASSWORD,
            20000
        );


    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Wi-Fi failed: %s",
            esp_err_to_name(ret)
        );
    }


    // ========================================================
    // SNTP
    // ========================================================

    sntp_sync_init(
        10000
    );


    // ========================================================
    // MQTT
    // ========================================================

    mqtt_app_start();


    // ========================================================
    // TELEMETRY TASK
    // ========================================================

    xTaskCreate(
        telemetry_task,
        "telemetry_task",
        4096,
        NULL,
        5,
        NULL
    );
}

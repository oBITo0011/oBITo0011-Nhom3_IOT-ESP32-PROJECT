idf_component_register(
    SRCS
        "soil_moisture.c"
        "tft_display.c"
        "main.c"
        "dht22.c"
        "bh1750.c"
        "wifi_manager.c"
        "sntp_sync.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        mqtt
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        json
        driver
        esp_lcd
        esp_adc
)
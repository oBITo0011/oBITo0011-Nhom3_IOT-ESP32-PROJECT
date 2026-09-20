#include "tft_display.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"


static const char *TAG = "TFT";


// ============================================================
// TFT PIN CONFIG
// ============================================================

#define TFT_SCLK    12
#define TFT_MOSI    11
#define TFT_RST     10
#define TFT_DC      13
#define TFT_CS      14

#define TFT_WIDTH   240
#define TFT_HEIGHT  240

#define TFT_SPI_HOST SPI2_HOST
#define TFT_SPI_FREQ_HZ (20 * 1000 * 1000)


// ============================================================
// RGB565 COLORS
// ============================================================

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_BLUE    0x001F
#define COLOR_GREEN   0x07E0
#define COLOR_RED     0xF800
#define COLOR_CYAN    0x07FF
#define COLOR_YELLOW  0xFFE0
#define COLOR_GRAY    0x8410


// ============================================================
// FRAME BUFFER
// ============================================================

static uint16_t frame_buffer[TFT_WIDTH * TFT_HEIGHT];


// ============================================================
// FONT 5x7
// ============================================================

static const uint8_t font_digits[10][5] =
{
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
};


// ============================================================
// DRAW PIXEL
// ============================================================

static void draw_pixel(
    int x,
    int y,
    uint16_t color
)
{
    if (x < 0 || x >= TFT_WIDTH ||
        y < 0 || y >= TFT_HEIGHT)
    {
        return;
    }

    frame_buffer[y * TFT_WIDTH + x] = color;
}


// ============================================================
// CLEAR SCREEN
// ============================================================

static void clear_screen(uint16_t color)
{
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++)
    {
        frame_buffer[i] = color;
    }
}


// ============================================================
// DRAW RECTANGLE
// ============================================================

static void draw_rect(
    int x,
    int y,
    int width,
    int height,
    uint16_t color
)
{
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            draw_pixel(x + i, y + j, color);
        }
    }
}


// ============================================================
// DRAW CHARACTER
// ============================================================

static void draw_char(
    int x,
    int y,
    char c,
    uint16_t color,
    int scale
)
{
    uint8_t bitmap[5] = {0};

    if (c >= '0' && c <= '9')
    {
        memcpy(bitmap, font_digits[c - '0'], 5);
    }
    else
    {
        switch (c)
        {
            case 'A':
                bitmap[0] = 0x7E;
                bitmap[1] = 0x11;
                bitmap[2] = 0x11;
                bitmap[3] = 0x11;
                bitmap[4] = 0x7E;
                break;

            case 'B':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x49;
                bitmap[2] = 0x49;
                bitmap[3] = 0x49;
                bitmap[4] = 0x36;
                break;

            case 'C':
                bitmap[0] = 0x3E;
                bitmap[1] = 0x41;
                bitmap[2] = 0x41;
                bitmap[3] = 0x41;
                bitmap[4] = 0x22;
                break;

            case 'D':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x41;
                bitmap[2] = 0x41;
                bitmap[3] = 0x22;
                bitmap[4] = 0x1C;
                break;

            case 'E':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x49;
                bitmap[2] = 0x49;
                bitmap[3] = 0x49;
                bitmap[4] = 0x41;
                break;

            case 'F':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x09;
                bitmap[2] = 0x09;
                bitmap[3] = 0x09;
                bitmap[4] = 0x01;
                break;

            case 'G':
                bitmap[0] = 0x3E;
                bitmap[1] = 0x41;
                bitmap[2] = 0x49;
                bitmap[3] = 0x49;
                bitmap[4] = 0x7A;
                break;

            case 'H':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x08;
                bitmap[2] = 0x08;
                bitmap[3] = 0x08;
                bitmap[4] = 0x7F;
                break;

            case 'I':
                bitmap[0] = 0x00;
                bitmap[1] = 0x41;
                bitmap[2] = 0x7F;
                bitmap[3] = 0x41;
                bitmap[4] = 0x00;
                break;

            case 'L':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x40;
                bitmap[2] = 0x40;
                bitmap[3] = 0x40;
                bitmap[4] = 0x40;
                break;

            case 'M':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x02;
                bitmap[2] = 0x0C;
                bitmap[3] = 0x02;
                bitmap[4] = 0x7F;
                break;

            case 'N':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x04;
                bitmap[2] = 0x08;
                bitmap[3] = 0x10;
                bitmap[4] = 0x7F;
                break;

            case 'O':
                bitmap[0] = 0x3E;
                bitmap[1] = 0x41;
                bitmap[2] = 0x41;
                bitmap[3] = 0x41;
                bitmap[4] = 0x3E;
                break;

            case 'P':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x09;
                bitmap[2] = 0x09;
                bitmap[3] = 0x09;
                bitmap[4] = 0x06;
                break;

            case 'S':
                bitmap[0] = 0x46;
                bitmap[1] = 0x49;
                bitmap[2] = 0x49;
                bitmap[3] = 0x49;
                bitmap[4] = 0x31;
                break;

            case 'T':
                bitmap[0] = 0x01;
                bitmap[1] = 0x01;
                bitmap[2] = 0x7F;
                bitmap[3] = 0x01;
                bitmap[4] = 0x01;
                break;

            case 'U':
                bitmap[0] = 0x3F;
                bitmap[1] = 0x40;
                bitmap[2] = 0x40;
                bitmap[3] = 0x40;
                bitmap[4] = 0x3F;
                break;

            case 'W':
                bitmap[0] = 0x7F;
                bitmap[1] = 0x20;
                bitmap[2] = 0x18;
                bitmap[3] = 0x20;
                bitmap[4] = 0x7F;
                break;

            case 'Y':
                bitmap[0] = 0x07;
                bitmap[1] = 0x08;
                bitmap[2] = 0x70;
                bitmap[3] = 0x08;
                bitmap[4] = 0x07;
                break;

            case '-':
                bitmap[0] = 0x08;
                bitmap[1] = 0x08;
                bitmap[2] = 0x08;
                bitmap[3] = 0x08;
                bitmap[4] = 0x08;
                break;

            case '.':
                bitmap[0] = 0x00;
                bitmap[1] = 0x60;
                bitmap[2] = 0x60;
                bitmap[3] = 0x00;
                bitmap[4] = 0x00;
                break;

            case ':':
                bitmap[0] = 0x00;
                bitmap[1] = 0x36;
                bitmap[2] = 0x36;
                bitmap[3] = 0x00;
                bitmap[4] = 0x00;
                break;

            case '%':
                bitmap[0] = 0x63;
                bitmap[1] = 0x13;
                bitmap[2] = 0x08;
                bitmap[3] = 0x64;
                bitmap[4] = 0x63;
                break;

            case ' ':
            default:
                bitmap[0] = 0x00;
                bitmap[1] = 0x00;
                bitmap[2] = 0x00;
                bitmap[3] = 0x00;
                bitmap[4] = 0x00;
                break;
        }
    }

    for (int col = 0; col < 5; col++)
    {
        for (int row = 0; row < 7; row++)
        {
            if (bitmap[col] & (1 << row))
            {
                for (int dx = 0; dx < scale; dx++)
                {
                    for (int dy = 0; dy < scale; dy++)
                    {
                        draw_pixel(
                            x + col * scale + dx,
                            y + row * scale + dy,
                            color
                        );
                    }
                }
            }
        }
    }
}


// ============================================================
// DRAW STRING
// ============================================================

static void draw_string(
    int x,
    int y,
    const char *text,
    uint16_t color,
    int scale
)
{
    while (*text)
    {
        draw_char(
            x,
            y,
            *text,
            color,
            scale
        );

        x += 6 * scale;
        text++;
    }
}


// ============================================================
// TFT OBJECTS
// ============================================================

static esp_lcd_panel_io_handle_t panel_io = NULL;
static esp_lcd_panel_handle_t panel = NULL;


// ============================================================
// REFRESH SCREEN
// ============================================================

static void refresh_screen(void)
{
    if (panel == NULL)
    {
        return;
    }

    esp_err_t ret = esp_lcd_panel_draw_bitmap(
        panel,
        0,
        0,
        TFT_WIDTH,
        TFT_HEIGHT,
        frame_buffer
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to refresh TFT: %s",
                 esp_err_to_name(ret));
    }
}


// ============================================================
// TFT INIT
// ============================================================

esp_err_t tft_display_init(void)
{
    ESP_LOGI(TAG, "Initializing TFT...");

    spi_bus_config_t buscfg =
    {
        .sclk_io_num = TFT_SCLK,
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz =
            TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t)
    };

    esp_err_t ret = spi_bus_initialize(
        TFT_SPI_HOST,
        &buscfg,
        SPI_DMA_CH_AUTO
    );

    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG,
                 "spi_bus_initialize failed: %s",
                 esp_err_to_name(ret));

        return ret;
    }


    esp_lcd_panel_io_spi_config_t io_config =
    {
        .dc_gpio_num = TFT_DC,
        .cs_gpio_num = TFT_CS,
        .pclk_hz = TFT_SPI_FREQ_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10
    };


    ret = esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)TFT_SPI_HOST,
        &io_config,
        &panel_io
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "esp_lcd_new_panel_io_spi failed: %s",
                 esp_err_to_name(ret));

        return ret;
    }


    esp_lcd_panel_dev_config_t panel_config =
    {
        .reset_gpio_num = TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16
    };


    ret = esp_lcd_new_panel_st7789(
        panel_io,
        &panel_config,
        &panel
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG,
                 "esp_lcd_new_panel_st7789 failed: %s",
                 esp_err_to_name(ret));

        return ret;
    }


    ret = esp_lcd_panel_reset(panel);

    if (ret != ESP_OK)
    {
        return ret;
    }


    ret = esp_lcd_panel_init(panel);

    if (ret != ESP_OK)
    {
        return ret;
    }


    ret = esp_lcd_panel_invert_color(panel, true);

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "invert_color failed: %s",
                 esp_err_to_name(ret));
    }


    ret = esp_lcd_panel_disp_on_off(panel, true);

    if (ret != ESP_OK)
    {
        return ret;
    }


    clear_screen(COLOR_BLACK);
    refresh_screen();


    ESP_LOGI(TAG, "TFT initialized successfully");

    return ESP_OK;
}


// ============================================================
// UPDATE TFT
// ============================================================

void tft_display_update(
    float temperature,
    float humidity,
    float illuminance,
    float soil_moisture,
    bool led_on,
    bool buzzer_on
)
{
    char temp_str[32];
    char hum_str[32];
    char light_str[32];
    char soil_str[32];


    // --------------------------------------------------------
    // FORMAT VALUES
    // --------------------------------------------------------

    snprintf(
        temp_str,
        sizeof(temp_str),
        "%.1f C",
        temperature
    );

    snprintf(
        hum_str,
        sizeof(hum_str),
        "%.1f %%",
        humidity
    );

    snprintf(
        light_str,
        sizeof(light_str),
        "%.1f lx",
        illuminance
    );

    snprintf(
        soil_str,
        sizeof(soil_str),
        "%.1f %%",
        soil_moisture
    );


    // --------------------------------------------------------
    // CLEAR
    // --------------------------------------------------------

    clear_screen(COLOR_BLACK);


    // --------------------------------------------------------
    // TITLE
    // --------------------------------------------------------

    draw_string(
        82,
        8,
        "ESP32",
        COLOR_CYAN,
        2
    );

    draw_string(
        94,
        28,
        "IOT",
        COLOR_CYAN,
        2
    );


    // --------------------------------------------------------
    // SEPARATOR
    // --------------------------------------------------------

    draw_rect(
        15,
        50,
        210,
        2,
        COLOR_GRAY
    );


    // --------------------------------------------------------
    // TEMPERATURE
    // --------------------------------------------------------

    draw_string(
        15,
        65,
        "TEMP",
        COLOR_YELLOW,
        2
    );

    draw_string(
        115,
        65,
        temp_str,
        COLOR_WHITE,
        2
    );


    // --------------------------------------------------------
    // HUMIDITY
    // --------------------------------------------------------

    draw_string(
        15,
        90,
        "HUM",
        COLOR_BLUE,
        2
    );

    draw_string(
        115,
        90,
        hum_str,
        COLOR_WHITE,
        2
    );


    // --------------------------------------------------------
    // LIGHT
    // --------------------------------------------------------

    draw_string(
        15,
        115,
        "LIGHT",
        COLOR_YELLOW,
        2
    );

    draw_string(
        115,
        115,
        light_str,
        COLOR_WHITE,
        2
    );


    // --------------------------------------------------------
    // SOIL MOISTURE
    // --------------------------------------------------------

    draw_string(
        15,
        140,
        "SOIL",
        COLOR_GREEN,
        2
    );

    draw_string(
        115,
        140,
        soil_str,
        COLOR_WHITE,
        2
    );


    // --------------------------------------------------------
    // LED
    // --------------------------------------------------------

    draw_string(
        15,
        170,
        "LED",
        COLOR_CYAN,
        2
    );

    if (led_on)
    {
        draw_string(
            115,
            170,
            "ON",
            COLOR_GREEN,
            2
        );
    }
    else
    {
        draw_string(
            115,
            170,
            "OFF",
            COLOR_RED,
            2
        );
    }


    // --------------------------------------------------------
    // BUZZER
    // --------------------------------------------------------

    draw_string(
        15,
        195,
        "BUZZER",
        COLOR_CYAN,
        2
    );

    if (buzzer_on)
    {
        draw_string(
            115,
            195,
            "ON",
            COLOR_RED,
            2
        );
    }
    else
    {
        draw_string(
            115,
            195,
            "OFF",
            COLOR_GREEN,
            2
        );
    }


    // --------------------------------------------------------
    // SEND TO TFT
    // --------------------------------------------------------

    refresh_screen();
}
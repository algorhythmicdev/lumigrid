#include "led_driver.h"
#include "config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/rmt.h>
#include <esp_log.h>

static const char *TAG = "LED";
static pixel_t *pixels = NULL;
static size_t pixel_count = 0;
static QueueHandle_t led_cmd_queue = NULL;

#define LED_RMT_TX_CHANNEL RMT_CHANNEL_0

// LED command types
typedef enum {
    LED_CMD_UPDATE,    // Update the LED strip
    LED_CMD_SET_PIXEL, // Set a specific pixel
    LED_CMD_SET_ALL,   // Set all pixels to the same color
    LED_CMD_CLEAR      // Clear all pixels
} led_cmd_type_t;

typedef struct {
    led_cmd_type_t type;
    uint16_t index;
    pixel_t pixel;
} led_cmd_t;

// RMT configuration for WS2812 LEDs
static void ws2812_rmt_init(uint8_t gpio_pin) {
    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = LED_RMT_TX_CHANNEL,
        .gpio_num = gpio_pin,
        .mem_block_num = 3,
        .tx_config = {
            .loop_en = false,
            .carrier_en = false,
            .idle_output_en = true,
            .idle_level = 0,
            .carrier_duty_percent = 50,
            .carrier_freq_hz = 10000,
            .carrier_level = 1
        },
        .clk_div = 8 // 100MHz / 8 = 12.5MHz
    };

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
}

// Convert RGB values to RMT pulses for WS2812
static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
                                        size_t wanted_num, size_t *translated_size, size_t *item_num) {
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }

    const rmt_item32_t bit0 = {{{ 4, 1, 8, 0 }}}; // ~0.4us high, ~0.8us low
    const rmt_item32_t bit1 = {{{ 8, 1, 4, 0 }}}; // ~0.8us high, ~0.4us low
    
    size_t size = wanted_num;
    if (src_size < wanted_num) {
        size = src_size;
    }

    const uint8_t *psrc = (const uint8_t *)src;
    rmt_item32_t *pdest = dest;
    
    *translated_size = size;
    *item_num = size * 8;

    for (size_t i = 0; i < size; i++) {
        uint8_t data = *psrc++;
        for (int bit = 0; bit < 8; bit++) {
            if (data & (1 << (7 - bit))) {
                *pdest++ = bit1;
            } else {
                *pdest++ = bit0;
            }
        }
    }
}

esp_err_t led_driver_init(void) {
    lumigrid_config_t config;
    if (config_load(&config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load configuration");
        return ESP_FAIL;
    }

    pixel_count = config.led_count;
    
    // Allocate memory for pixels
    size_t pixel_size = config.led_is_rgbw ? 4 : 3; // RGBW or RGB
    pixels = calloc(pixel_count, sizeof(pixel_t));
    if (pixels == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for pixels");
        return ESP_ERR_NO_MEM;
    }

    // Initialize RMT for WS2812
    ws2812_rmt_init(config.led_pin);
    
    // Create command queue
    led_cmd_queue = xQueueCreate(10, sizeof(led_cmd_t));
    if (led_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create LED command queue");
        free(pixels);
        pixels = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Register the RMT adapter for WS2812
    rmt_translator_init(LED_RMT_TX_CHANNEL, ws2812_rmt_adapter);

    ESP_LOGI(TAG, "LED driver initialized with %d pixels on pin %d", pixel_count, config.led_pin);
    
    // Clear LEDs on startup
    led_clear();
    led_show();
    
    return ESP_OK;
}

void led_render_task(void *pvParameter) {
    led_cmd_t cmd;

    for (;;) {
        if (xQueueReceive(led_cmd_queue, &cmd, portMAX_DELAY)) {
            switch (cmd.type) {
                case LED_CMD_UPDATE:
                    // Send pixel data to LEDs via RMT
                    size_t pixel_size = sizeof(pixel_t);
                    rmt_write_sample(LED_RMT_TX_CHANNEL, (uint8_t*)pixels, pixel_count * pixel_size, true);
                    break;
                
                case LED_CMD_SET_PIXEL:
                    if (cmd.index < pixel_count) {
                        pixels[cmd.index] = cmd.pixel;
                    }
                    break;
                
                case LED_CMD_SET_ALL:
                    for (int i = 0; i < pixel_count; i++) {
                        pixels[i] = cmd.pixel;
                    }
                    break;
                
                case LED_CMD_CLEAR:
                    memset(pixels, 0, pixel_count * sizeof(pixel_t));
                    break;
                
                default:
                    break;
            }
        }
    }
}

esp_err_t led_set_pixel(uint16_t index, pixel_t pixel) {
    if (index >= pixel_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    led_cmd_t cmd = {
        .type = LED_CMD_SET_PIXEL,
        .index = index,
        .pixel = pixel
    };
    
    if (xQueueSend(led_cmd_queue, &cmd, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t led_set_all(pixel_t pixel) {
    led_cmd_t cmd = {
        .type = LED_CMD_SET_ALL,
        .pixel = pixel
    };
    
    if (xQueueSend(led_cmd_queue, &cmd, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t led_show(void) {
    led_cmd_t cmd = {
        .type = LED_CMD_UPDATE
    };
    
    if (xQueueSend(led_cmd_queue, &cmd, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t led_clear(void) {
    led_cmd_t cmd = {
        .type = LED_CMD_CLEAR
    };
    
    if (xQueueSend(led_cmd_queue, &cmd, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

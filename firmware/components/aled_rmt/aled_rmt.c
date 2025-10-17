#include "aled_rmt.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ALED_RMT";

#define WS2812_T0H_NS 400
#define WS2812_T0L_NS 850
#define WS2812_T1H_NS 800
#define WS2812_T1L_NS 450
#define WS2812_RESET_US 50

typedef struct {
    rmt_channel_handle_t rmt_ch;
    rmt_encoder_handle_t encoder;
    gpio_num_t gpio;
    led_type_t type;
    uint16_t num_leds;
} aled_rmt_ctx_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                                    const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state) {
    return 0;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder) {
    if (encoder) {
        free(encoder);
    }
    return ESP_OK;
}

static esp_err_t rmt_reset_led_strip_encoder(rmt_encoder_t *encoder) {
    return ESP_OK;
}

esp_err_t aled_rmt_init_channel(aled_rmt_handle_t *handle, gpio_num_t pin, uint16_t num_leds, led_type_t type) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    aled_rmt_ctx_t *ctx = calloc(1, sizeof(aled_rmt_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    
    ctx->gpio = pin;
    ctx->type = type;
    ctx->num_leds = num_leds;
    
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    
    esp_err_t ret = rmt_new_tx_channel(&tx_config, &ctx->rmt_ch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %s", esp_err_to_name(ret));
        free(ctx);
        return ret;
    }
    
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = WS2812_T0H_NS / 100,
            .level1 = 0,
            .duration1 = WS2812_T0L_NS / 100,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = WS2812_T1H_NS / 100,
            .level1 = 0,
            .duration1 = WS2812_T1L_NS / 100,
        },
        .flags.msb_first = 1,
    };
    
    ret = rmt_new_bytes_encoder(&bytes_encoder_config, &ctx->encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT encoder: %s", esp_err_to_name(ret));
        rmt_del_channel(ctx->rmt_ch);
        free(ctx);
        return ret;
    }
    
    ret = rmt_enable(ctx->rmt_ch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %s", esp_err_to_name(ret));
        rmt_del_encoder(ctx->encoder);
        rmt_del_channel(ctx->rmt_ch);
        free(ctx);
        return ret;
    }
    
    handle->rmt_channel = ctx;
    handle->gpio = pin;
    handle->num_leds = num_leds;
    handle->type = type;
    
    ESP_LOGI(TAG, "RMT channel initialized: GPIO%d, %d LEDs, type=%d", pin, num_leds, type);
    return ESP_OK;
}

esp_err_t aled_rmt_write_pixels(aled_rmt_handle_t *handle, const px_rgba_t *pixels, uint16_t count) {
    if (!handle || !handle->rmt_channel || !pixels) {
        return ESP_ERR_INVALID_ARG;
    }
    
    aled_rmt_ctx_t *ctx = (aled_rmt_ctx_t *)handle->rmt_channel;
    
    size_t bytes_per_led = (ctx->type == LED_SK6812_RGBW) ? 4 : 3;
    size_t buffer_size = count * bytes_per_led;
    uint8_t *led_buffer = malloc(buffer_size);
    if (!led_buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    for (int i = 0; i < count; i++) {
        if (ctx->type == LED_SK6812_RGBW) {
            led_buffer[i * 4 + 0] = pixels[i].g;
            led_buffer[i * 4 + 1] = pixels[i].r;
            led_buffer[i * 4 + 2] = pixels[i].b;
            led_buffer[i * 4 + 3] = pixels[i].w;
        } else {
            led_buffer[i * 3 + 0] = pixels[i].g;
            led_buffer[i * 3 + 1] = pixels[i].r;
            led_buffer[i * 3 + 2] = pixels[i].b;
        }
    }
    
    rmt_transmit_config_t tx_conf = {
        .loop_count = 0,
    };
    
    esp_err_t ret = rmt_transmit(ctx->rmt_ch, ctx->encoder, led_buffer, buffer_size, &tx_conf);
    
    free(led_buffer);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT transmit failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t aled_rmt_clear(aled_rmt_handle_t *handle) {
    if (!handle || !handle->rmt_channel) {
        return ESP_ERR_INVALID_ARG;
    }
    
    aled_rmt_ctx_t *ctx = (aled_rmt_ctx_t *)handle->rmt_channel;
    
    px_rgba_t *black = calloc(ctx->num_leds, sizeof(px_rgba_t));
    if (!black) {
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = aled_rmt_write_pixels(handle, black, ctx->num_leds);
    free(black);
    
    return ret;
}

esp_err_t aled_rmt_deinit(aled_rmt_handle_t *handle) {
    if (!handle || !handle->rmt_channel) {
        return ESP_ERR_INVALID_ARG;
    }
    
    aled_rmt_ctx_t *ctx = (aled_rmt_ctx_t *)handle->rmt_channel;
    
    aled_rmt_clear(handle);
    
    rmt_disable(ctx->rmt_ch);
    rmt_del_encoder(ctx->encoder);
    rmt_del_channel(ctx->rmt_ch);
    
    free(ctx);
    handle->rmt_channel = NULL;
    
    ESP_LOGI(TAG, "RMT channel deinitialized");
    return ESP_OK;
}

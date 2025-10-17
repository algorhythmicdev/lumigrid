#include "led_effects.h"
#include "esp_log.h"
#include <stddef.h> // For NULL
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "../../main/config/board_pinmap.h" // For LED_RMT_GPIO_NUMS

static const char *TAG = "LED_EFFECTS";

// RMT transmission parameters for WS2812B/SK6812
#define T0H_NS 350  // WS2812B T0H (0 code high time)
#define T0L_NS 800  // WS2812B T0L (0 code low time)
#define T1H_NS 800  // WS2812B T1H (1 code high time)
#define T1L_NS 350  // WS2812B T1L (1 code low time)
#define TRST_NS 50000 // WS2812B reset code low time (min 50us)

#define BITS_PER_PIXEL 24 // 8 bits for R, G, B
#define BYTES_PER_PIXEL 3 // 1 byte for R, G, B

// Global array of addressable LED channels
static aled_channel_t s_aled_channels[LED_RMT_NUM_CHANNELS];

// Effect contract function pointers (moved from header)
typedef void (*fx_render_fn)(aled_channel_t *ch, const effect_params_t *p,
                             uint32_t t_ms, uint32_t t_end_ms);
typedef bool (*fx_init_fn)(aled_channel_t *ch, const effect_params_t *p);

// Effect vtable (virtual table) structure (moved from header)
typedef struct {
  uint32_t id;
  const char *name;
  fx_init_fn init;
  fx_render_fn render;
} effect_vtable_t;

// Placeholder for effect registry
static const effect_vtable_t *effect_registry[] = {
    // TODO: Add actual effect implementations here
    NULL
};

/**
 * @brief Looks up an effect in the registry by its ID.
 *
 * @param id The ID of the effect to look up.
 * @return A pointer to the effect_vtable_t if found, otherwise NULL.
 */
const effect_vtable_t *fx_lookup(uint32_t id) {
    for (int i = 0; effect_registry[i] && effect_registry[i]->id != 0; i++) {
        if (effect_registry[i]->id == id) {
            return effect_registry[i];
        }
    }
    return NULL;
}

/**
 * @brief Encodes a single pixel (RGB) into RMT symbols for WS2812B.
 *
 * @param[in] src Pointer to the px_rgba_t structure.
 * @param[out] dest Pointer to the RMT symbol buffer.
 * @param[in] num_symbols Number of symbols to write (should be BITS_PER_PIXEL).
 * @param[in] arg User-defined argument (not used here).
 * @return Number of symbols written.
 */
static size_t rmt_encode_ws2812b(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state) {
    rmt_encode_state_t state = RMT_ENCODE_CONTINUE;
    rmt_symbol_word_t *symbol;
    encoder->bytes = 0;
    encoder->len = 0;
    
    const px_rgba_t *pixels = (const px_rgba_t *)primary_data;
    size_t num_pixels = data_size / sizeof(px_rgba_t);

    for (size_t i = 0; i < num_pixels; i++) {
        uint32_t pixel_grb = (pixels[i].g << 16) | (pixels[i].r << 8) | pixels[i].b;
        for (int bit = BITS_PER_PIXEL - 1; bit >= 0; bit--) {
            symbol = encoder->get_space(encoder, 1);
            if (!symbol) {
                state = RMT_ENCODE_MEM_ALMOST_FULL;
                goto out;
            }
            if ((pixel_grb >> bit) & 1) {
                symbol->duration0 = T1H_NS;
                symbol->level0 = 1;
                symbol->duration1 = T1L_NS;
                symbol->level1 = 0;
            } else {
                symbol->duration0 = T0H_NS;
                symbol->level0 = 1;
                symbol->duration1 = T0L_NS;
                symbol->level1 = 0;
            }
            encoder->len++;
        }
    }
    state = RMT_ENCODE_DONE;

out:
    *ret_state = state;
    return encoder->len;
}

/**
 * @brief Initialize the LED effects engine.
 *
 * This function sets up the RMT channels for all addressable LED strips.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t led_effects_init(void) {
    esp_err_t ret = ESP_OK;

    // Configure RMT transmitter for each channel
    for (int i = 0; i < LED_RMT_NUM_CHANNELS; i++) {
        rmt_tx_channel_config_t tx_channel_cfg = {
            .gpio_num = LED_RMT_GPIO_NUMS[i],
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10000000, // 10 MHz RMT clock
            .mem_block_symbols = 64,   // 64 symbols per RMT block
            .trans_queue_depth = 4,    // 4 transactions in queue
            .with_dma = true,
        };
        ret = rmt_new_tx_channel(&tx_channel_cfg, &s_aled_channels[i].rmt_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create RMT TX channel %d", i);
            return ret;
        }

        rmt_encoder_config_t encoder_cfg = {
            .resolution = tx_channel_cfg.resolution_hz,
        };
        // TODO: Implement custom RMT encoder for WS2812B/SK6812 timing
        // For now, using a placeholder encoder
        rmt_new_bytes_encoder(&encoder_cfg, &s_aled_channels[i].rmt_channel); // This is not correct, need a custom encoder

        ret = rmt_enable(s_aled_channels[i].rmt_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable RMT TX channel %d", i);
            return ret;
        }
        ESP_LOGI(TAG, "RMT TX channel %d on GPIO %d initialized.", i, LED_RMT_GPIO_NUMS[i]);
    }

    return ret;
}

/**
 * @brief Set an effect for a specific addressable LED channel.
 *
 * @param ch The channel number (0-7).
 * @param params The effect parameters.
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t aled_set_effect(int ch, const effect_params_t *params) {
    if (ch < 0 || ch >= LED_RMT_NUM_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }
    // For now, just copy parameters. Later, this will involve allocating frame buffers and calling effect init functions.
    // s_aled_channels[ch].type = params->led_type; // Need to add led_type to effect_params_t or handle separately
    // s_aled_channels[ch].n_pixels = params->n_pixels;
    // s_aled_channels[ch].gamma = params->gamma;
    // s_aled_channels[ch].max_brightness = params->max_brightness;
    // active_clip_params[ch] = *params; // This part of the design needs refinement

    ESP_LOGI(TAG, "Effect %lu set for channel %d", params->effect_id, ch);
    return ESP_OK;
}

/**
 * @brief Renders the current frame for all active LED channels.
 *
 * This function should be called periodically by the EffectEngine task.
 *
 * @param t_ms Current time in milliseconds.
 */
void led_effects_render_all_channels(uint32_t t_ms) {
    for (int i = 0; i < LED_RMT_NUM_CHANNELS; i++) {
        // Placeholder for active clip data
        // In a real implementation, this would come from a global state or task-specific data
        // For demonstration, let's assume channel 0 has an active effect
        // if (i == 0) { // This conditional should be removed when actual clip management is implemented
            // const effect_vtable_t *fx = fx_lookup(active_clip_params[i].effect_id); // active_clip_params is not global
            // if (fx && fx->render) {
            //     fx->render(&s_aled_channels[i], &active_clip_params[i], t_ms, active_clip_t_end[i]);
            // } else {
            //     ESP_LOGD(TAG, "No effect or render function found for channel %d", i);
            // }
        // }
    }
    ESP_LOGD(TAG, "LED effects rendered at %lu ms", t_ms);
}
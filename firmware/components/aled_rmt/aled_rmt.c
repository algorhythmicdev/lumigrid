#include "aled_rmt.h"

#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ALED_RMT_MAX_CHANNELS 8
#define SYMBOL_CHUNK          512

static const char *TAG = "ALED_RMT";

static rmt_channel_handle_t s_channels[ALED_RMT_MAX_CHANNELS];
static gpio_num_t           s_channel_gpio[ALED_RMT_MAX_CHANNELS];

typedef struct {
  uint16_t t1h, t1l, t0h, t0l;
} ws_timing_t;

static inline ws_timing_t ws2812_t(void){
  // 100 ns ticks at 10 MHz (IDF default)
  return (ws_timing_t){ .t1h = 8, .t1l = 4, .t0h = 4, .t0l = 9 };
}

static inline rmt_symbol_word_t sym(uint16_t high, uint16_t low){
  return (rmt_symbol_word_t){
    .duration0 = high,
    .level0    = 1,
    .duration1 = low,
    .level1    = 0
  };
}

esp_err_t aled_rmt_init_chan(int idx, gpio_num_t pin){
  if (idx < 0 || idx >= ALED_RMT_MAX_CHANNELS){
    return ESP_ERR_INVALID_ARG;
  }

  if (s_channels[idx]){
    ESP_LOGW(TAG, "Channel %d already initialised on GPIO %d", idx, s_channel_gpio[idx]);
    return ESP_OK;
  }

  rmt_tx_channel_config_t cfg = {
    .gpio_num = pin,
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = 10 * 1000 * 1000, // 10 MHz -> 100 ns ticks
    .mem_block_symbols = 96,
    .trans_queue_depth = 4
  };

  esp_err_t err = rmt_new_tx_channel(&cfg, &s_channels[idx]);
  if (err != ESP_OK){
    ESP_LOGE(TAG, "rmt_new_tx_channel failed: %s", esp_err_to_name(err));
    return err;
  }

  err = rmt_enable(s_channels[idx]);
  if (err != ESP_OK){
    ESP_LOGE(TAG, "rmt_enable failed: %s", esp_err_to_name(err));
    rmt_del_channel(s_channels[idx]);
    s_channels[idx] = NULL;
    return err;
  }

  s_channel_gpio[idx] = pin;
  ESP_LOGI(TAG, "RMT channel %d initialised on GPIO %d", idx, pin);
  return ESP_OK;
}

static inline uint8_t component_from_pixel(const px_rgba_t* fb, int pixel_idx, int component_idx, int stride){
  const px_rgba_t* px = &fb[pixel_idx];
  if (stride == 4){
    switch (component_idx){
      case 0: return px->g;
      case 1: return px->r;
      case 2: return px->b;
      default: return px->w;
    }
  } else {
    switch (component_idx){
      case 0: return px->g;
      case 1: return px->r;
      default: return px->b;
    }
  }
}

esp_err_t aled_rmt_write(int idx, const px_rgba_t* fb, int npx, led_type_t type){
  if (idx < 0 || idx >= ALED_RMT_MAX_CHANNELS || !s_channels[idx] || !fb || npx <= 0){
    return ESP_ERR_INVALID_ARG;
  }

  const ws_timing_t T = ws2812_t();
  const int stride = (type == LED_SK6812_RGBW) ? 4 : 3;
  const int total_bits = npx * stride * 8;

  static rmt_symbol_word_t symbols[SYMBOL_CHUNK];
  rmt_transmit_config_t tc = {
    .loop_count = 0
  };

  int bit_index = 0;
  while (bit_index < total_bits){
    int take = total_bits - bit_index;
    if (take > SYMBOL_CHUNK){
      take = SYMBOL_CHUNK;
    }

    for (int k = 0; k < take; ++k){
      int global_bit = bit_index + k;
      int pixel = global_bit / (stride * 8);
      int within_pixel = global_bit % (stride * 8);
      int component = within_pixel / 8;
      int bit_pos = 7 - (within_pixel % 8);

      uint8_t value = 0;
      if (pixel < npx){
        value = component_from_pixel(fb, pixel, component, stride);
      }

      if (value & (1 << bit_pos)){
        symbols[k] = sym(T.t1h, T.t1l);
      } else {
        symbols[k] = sym(T.t0h, T.t0l);
      }
    }

    esp_err_t err = rmt_transmit(s_channels[idx], NULL, symbols, take * sizeof(rmt_symbol_word_t), &tc);
    if (err != ESP_OK){
      ESP_LOGE(TAG, "rmt_transmit failed ch%d: %s", idx, esp_err_to_name(err));
      return err;
    }

    bit_index += take;
  }

  // Reset pulse (60 us); 1 ms delay is safe.
  vTaskDelay(pdMS_TO_TICKS(1));
  return ESP_OK;
}

void aled_rmt_deinit(int idx){
  if (idx < 0 || idx >= ALED_RMT_MAX_CHANNELS){
    return;
  }
  if (s_channels[idx]){
    rmt_disable(s_channels[idx]);
    rmt_del_channel(s_channels[idx]);
    s_channels[idx] = NULL;
  }
}

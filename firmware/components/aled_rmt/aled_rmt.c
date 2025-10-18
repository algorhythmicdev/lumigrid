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

static inline int stride_for(led_type_t t){ return t==LED_SK6812_RGBW ? 4 : 3; }
static inline void pack(uint8_t* out, px_rgba_t p, color_order_t ord, bool rgbw){
  if(rgbw){
    if(ord==ORDER_RGBW){ out[0]=p.r; out[1]=p.g; out[2]=p.b; out[3]=p.w; }
    else /*GRBW*/       { out[0]=p.g; out[1]=p.r; out[2]=p.b; out[3]=p.w; }
  }else{
    if(ord==ORDER_RGB){ out[0]=p.r; out[1]=p.g; out[2]=p.b; }
    else              { out[0]=p.g; out[1]=p.r; out[2]=p.b; } // GRB default
  }
}

esp_err_t aled_rmt_write(int idx, const px_rgba_t* fb, int npx, led_type_t type, color_order_t order){
  if (idx < 0 || idx >= ALED_RMT_MAX_CHANNELS || !s_channels[idx] || !fb || npx <= 0){
    return ESP_ERR_INVALID_ARG;
  }

  const ws_timing_t T = ws2812_t();
  const bool rgbw = (type==LED_SK6812_RGBW);
  const int stride=stride_for(type);
  const int total_bytes = npx * stride;
  uint8_t byte_stream[total_bytes];

  for (int i = 0; i < npx; ++i){
    pack(&byte_stream[i * stride], fb[i], order, rgbw);
  }

  static rmt_symbol_word_t symbols[SYMBOL_CHUNK];
  rmt_transmit_config_t tc = {
    .loop_count = 0
  };

  int byte_index = 0;
  while (byte_index < total_bytes){
    int take_bytes = (total_bytes - byte_index);
    if (take_bytes * 8 > SYMBOL_CHUNK){
      take_bytes = SYMBOL_CHUNK / 8;
    }

    for (int k = 0; k < take_bytes * 8; ++k){
      int current_byte_idx = byte_index + k / 8;
      int bit_pos = 7 - (k % 8);
      if (byte_stream[current_byte_idx] & (1 << bit_pos)){
        symbols[k] = sym(T.t1h, T.t1l);
      } else {
        symbols[k] = sym(T.t0h, T.t0l);
      }
    }

    esp_err_t err = rmt_transmit(s_channels[idx], NULL, symbols, take_bytes * 8 * sizeof(rmt_symbol_word_t), &tc);
    if (err != ESP_OK){
      ESP_LOGE(TAG, "rmt_transmit failed ch%d: %s", idx, esp_err_to_name(err));
      return err;
    }
    byte_index += take_bytes;
  }

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

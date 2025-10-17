#include "pca9685_driver.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <math.h>
#include <string.h>

#define PCA9685_REG_MODE1        0x00
#define PCA9685_REG_MODE2        0x01
#define PCA9685_REG_LED0_ON_L    0x06
#define PCA9685_REG_ALL_LED_ON_L 0xFA
#define PCA9685_REG_ALL_LED_OFF_L 0xFC
#define PCA9685_REG_ALL_LED_OFF_H 0xFD
#define PCA9685_REG_PRESCALE     0xFE

#define MODE1_RESTART 0x80
#define MODE1_SLEEP   0x10
#define MODE1_AI      0x20
#define MODE2_OUTDRV  0x04

#define PCA9685_OSC_HZ 25000000.0f
#define PCA9685_MAX_CHANNEL 15
#define PCA9685_I2C_TIMEOUT_MS 50

static const char *TAG = "pca9685";

typedef struct {
  bool active;
  bool log_curve;
  float start_duty;
  float target_duty;
  uint64_t start_us;
  uint32_t duration_ms;
} fade_state_t;

typedef struct {
  pca9685_config_t cfg;
  bool initialized;
  float duty_cache[PCA9685_CHANNEL_COUNT];
  fade_state_t fades[PCA9685_CHANNEL_COUNT];
  pca9685_i2c_stub_t i2c_stub;
} pca9685_ctx_t;

static pca9685_ctx_t s_ctx;

static inline float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static esp_err_t write_reg(uint8_t reg, const uint8_t *data, size_t len) {
  if (s_ctx.i2c_stub) {
    return s_ctx.i2c_stub(reg, data, len);
  }
  uint8_t buffer[1 + 4]; // maximum write size we use
  ESP_RETURN_ON_FALSE(len <= 4, ESP_ERR_INVALID_SIZE, TAG, "len too large");
  buffer[0] = reg;
  memcpy(&buffer[1], data, len);
  return i2c_master_write_to_device(
      s_ctx.cfg.i2c_port,
      s_ctx.cfg.i2c_addr,
      buffer,
      len + 1,
      PCA9685_I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static esp_err_t write_reg8(uint8_t reg, uint8_t value) {
  return write_reg(reg, &value, 1);
}

static esp_err_t set_pwm_ticks(uint8_t channel, uint16_t on, uint16_t off) {
  uint8_t base = PCA9685_REG_LED0_ON_L + 4 * channel;
  uint8_t payload[4] = {
      on & 0xFF,
      (on >> 8) & 0x0F,
      off & 0xFF,
      (off >> 8) & 0x0F,
  };
  return write_reg(base, payload, sizeof(payload));
}

static esp_err_t set_all_leds_off_register(void) {
  uint8_t payload[4] = {0, 0, 0, 0x10};
  return write_reg(PCA9685_REG_ALL_LED_ON_L, payload, sizeof(payload));
}

static esp_err_t set_pwm_duty_internal(uint8_t channel, float duty01) {
  ESP_RETURN_ON_FALSE(channel <= PCA9685_MAX_CHANNEL, ESP_ERR_INVALID_ARG, TAG, "channel out of range");
  duty01 = clamp01(duty01);

  s_ctx.duty_cache[channel] = duty01;

  if (!s_ctx.initialized && !s_ctx.i2c_stub) {
    // During init we may call this before hardware is ready, but if stub not installed return OK.
    return ESP_OK;
  }

  uint16_t off_ticks = (uint16_t)lroundf(duty01 * 4095.0f);

  if (off_ticks >= 4095) {
    uint8_t payload[4] = {0, 0x10, 0, 0};
    return write_reg(PCA9685_REG_LED0_ON_L + 4 * channel, payload, sizeof(payload));
  }
  if (off_ticks == 0) {
    uint8_t payload[4] = {0, 0, 0, 0x10};
    return write_reg(PCA9685_REG_LED0_ON_L + 4 * channel, payload, sizeof(payload));
  }
  return set_pwm_ticks(channel, 0, off_ticks);
}

static uint8_t compute_prescale(float freq_hz) {
  float prescale = (PCA9685_OSC_HZ / (4096.0f * freq_hz)) - 1.0f;
  if (prescale < 3.0f) prescale = 3.0f;
  if (prescale > 255.0f) prescale = 255.0f;
  return (uint8_t)lroundf(prescale);
}

static void clear_fades_channel(uint8_t channel) {
  s_ctx.fades[channel].active = false;
}

esp_err_t pca9685_init(const pca9685_config_t *cfg) {
  ESP_RETURN_ON_FALSE(cfg, ESP_ERR_INVALID_ARG, TAG, "cfg null");

  memset(&s_ctx, 0, sizeof(s_ctx));
  s_ctx.cfg = *cfg;
  if (s_ctx.cfg.i2c_addr == 0) {
    s_ctx.cfg.i2c_addr = 0x40;
  }
  if (s_ctx.cfg.i2c_clk_speed_hz == 0) {
    s_ctx.cfg.i2c_clk_speed_hz = 400000;
  }
  if (s_ctx.cfg.pwm_freq_hz <= 0.0f) {
    s_ctx.cfg.pwm_freq_hz = 1000.0f;
  }
  if (s_ctx.cfg.sda_gpio == GPIO_NUM_NC || s_ctx.cfg.scl_gpio == GPIO_NUM_NC) {
    ESP_LOGE(TAG, "SDA/SCL pins must be specified");
    return ESP_ERR_INVALID_ARG;
  }
  i2c_config_t i2c_conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = s_ctx.cfg.sda_gpio,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = s_ctx.cfg.scl_gpio,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = s_ctx.cfg.i2c_clk_speed_hz,
      .clk_flags = 0,
  };

  esp_err_t err = i2c_param_config(cfg->i2c_port, &i2c_conf);
  ESP_RETURN_ON_ERROR(err, TAG, "i2c_param_config failed");

  err = i2c_driver_install(cfg->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_RETURN_ON_ERROR(err, TAG, "i2c_driver_install failed");
  }

  if (s_ctx.cfg.oe_gpio != GPIO_NUM_NC) {
    gpio_config_t gpio_conf = {
        .pin_bit_mask = 1ULL << s_ctx.cfg.oe_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gpio_conf), TAG, "gpio_config failed");
    gpio_set_level(s_ctx.cfg.oe_gpio, 1);
  }

  // Put device to sleep before setting prescale
  ESP_RETURN_ON_ERROR(write_reg8(PCA9685_REG_MODE1, MODE1_SLEEP), TAG, "MODE1 sleep write failed");

  uint8_t prescale = compute_prescale(s_ctx.cfg.pwm_freq_hz);
  ESP_RETURN_ON_ERROR(write_reg8(PCA9685_REG_PRESCALE, prescale), TAG, "prescale write failed");

  // Configure MODE1 and MODE2
  ESP_RETURN_ON_ERROR(write_reg8(PCA9685_REG_MODE1, MODE1_AI), TAG, "MODE1 write failed");
  esp_rom_delay_us(5000);
  ESP_RETURN_ON_ERROR(write_reg8(PCA9685_REG_MODE2, MODE2_OUTDRV), TAG, "MODE2 write failed");
  ESP_RETURN_ON_ERROR(write_reg8(PCA9685_REG_MODE1, MODE1_RESTART | MODE1_AI), TAG, "MODE1 restart failed");

  for (int ch = 0; ch < PCA9685_CHANNEL_COUNT; ++ch) {
    s_ctx.duty_cache[ch] = 0.0f;
    clear_fades_channel(ch);
  }

  ESP_RETURN_ON_ERROR(set_all_leds_off_register(), TAG, "all_off write failed");

  s_ctx.initialized = true;
  ESP_LOGI(TAG, "Initialized PCA9685 @0x%02X freq=%.1fHz prescale=%u", s_ctx.cfg.i2c_addr, s_ctx.cfg.pwm_freq_hz, prescale);
  return ESP_OK;
}

esp_err_t pca9685_set_duty(uint8_t channel, float duty01) {
  ESP_RETURN_ON_FALSE(s_ctx.initialized || s_ctx.i2c_stub, ESP_ERR_INVALID_STATE, TAG, "driver not initialized");
  ESP_RETURN_ON_FALSE(channel <= PCA9685_MAX_CHANNEL, ESP_ERR_INVALID_ARG, TAG, "channel out of range");
  clear_fades_channel(channel);
  return set_pwm_duty_internal(channel, duty01);
}

esp_err_t pca9685_fade_to(uint8_t channel, float duty01, uint32_t duration_ms, bool log_curve) {
  ESP_RETURN_ON_FALSE(s_ctx.initialized || s_ctx.i2c_stub, ESP_ERR_INVALID_STATE, TAG, "driver not initialized");
  ESP_RETURN_ON_FALSE(channel <= PCA9685_MAX_CHANNEL, ESP_ERR_INVALID_ARG, TAG, "channel out of range");

  duty01 = clamp01(duty01);
  if (duration_ms == 0) {
    return pca9685_set_duty(channel, duty01);
  }

  fade_state_t *fade = &s_ctx.fades[channel];
  fade->active = true;
  fade->log_curve = log_curve;
  fade->start_duty = s_ctx.duty_cache[channel];
  fade->target_duty = duty01;
  fade->duration_ms = duration_ms;
  fade->start_us = esp_timer_get_time();
  return ESP_OK;
}

esp_err_t pca9685_all_off(void) {
  ESP_RETURN_ON_FALSE(s_ctx.initialized || s_ctx.i2c_stub, ESP_ERR_INVALID_STATE, TAG, "driver not initialized");

  if (s_ctx.cfg.oe_gpio != GPIO_NUM_NC) {
    gpio_set_level(s_ctx.cfg.oe_gpio, 0);
    esp_rom_delay_us(50);
    gpio_set_level(s_ctx.cfg.oe_gpio, 1);
  }

  ESP_RETURN_ON_ERROR(set_all_leds_off_register(), TAG, "all_off write failed");

  for (int ch = 0; ch < PCA9685_CHANNEL_COUNT; ++ch) {
    s_ctx.duty_cache[ch] = 0.0f;
    clear_fades_channel(ch);
  }
  return ESP_OK;
}

void pca9685_tick(void) {
  if (!s_ctx.initialized && !s_ctx.i2c_stub) {
    return;
  }
  uint64_t now_us = esp_timer_get_time();
  for (int ch = 0; ch < PCA9685_CHANNEL_COUNT; ++ch) {
    fade_state_t *fade = &s_ctx.fades[ch];
    if (!fade->active) continue;

    uint64_t elapsed_us = now_us - fade->start_us;
    uint32_t elapsed_ms = (uint32_t)(elapsed_us / 1000ULL);
    float progress = (float)elapsed_ms / (float)fade->duration_ms;
    if (progress >= 1.0f) {
      fade->active = false;
      set_pwm_duty_internal(ch, fade->target_duty);
      continue;
    }

    if (fade->log_curve) {
      progress = log1pf(progress * 9.0f) / logf(10.0f);
    }

    float duty = fade->start_duty + (fade->target_duty - fade->start_duty) * progress;
    set_pwm_duty_internal(ch, duty);
  }
}

void pca9685_reset_state(void) {
  for (int ch = 0; ch < PCA9685_CHANNEL_COUNT; ++ch) {
    s_ctx.duty_cache[ch] = 0.0f;
    clear_fades_channel(ch);
  }
}

void pca9685_install_i2c_stub(pca9685_i2c_stub_t stub) {
  s_ctx.i2c_stub = stub;
}

void pca9685_remove_i2c_stub(void) {
  s_ctx.i2c_stub = NULL;
}

float pca9685_get_cached_duty(uint8_t channel) {
  if (channel > PCA9685_MAX_CHANNEL) return 0.0f;
  return s_ctx.duty_cache[channel];
}

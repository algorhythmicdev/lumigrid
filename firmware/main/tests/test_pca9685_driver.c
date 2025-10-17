#include "unity.h"

#include "pca9685_driver.h"
#include "config/board_pinmap.h"

#include <string.h>

#define PCA9685_LED_BASE_REG 0x06

typedef struct {
  uint8_t reg;
  uint8_t data[4];
  size_t len;
} write_log_t;

static write_log_t s_log[64];
static size_t s_log_count;

static void reset_log(void) {
  memset(s_log, 0, sizeof(s_log));
  s_log_count = 0;
}

static esp_err_t test_i2c_stub(uint8_t reg, const uint8_t *data, size_t len) {
  if (s_log_count < (sizeof(s_log) / sizeof(s_log[0]))) {
    s_log[s_log_count].reg = reg;
    s_log[s_log_count].len = len > sizeof(s_log[s_log_count].data) ? sizeof(s_log[s_log_count].data) : len;
    memcpy(s_log[s_log_count].data, data, s_log[s_log_count].len);
    s_log_count++;
  }
  return ESP_OK;
}

TEST_CASE("PCA9685 toggles first 8 channels", "[pca9685]") {
  reset_log();
  pca9685_install_i2c_stub(test_i2c_stub);

  const pca9685_config_t cfg = {
      .i2c_port = I2C_NUM_0,
      .i2c_addr = PCA9685_I2C_ADDR,
      .sda_gpio = I2C_SDA,
      .scl_gpio = I2C_SCL,
      .oe_gpio = GPIO_NUM_NC,
      .i2c_clk_speed_hz = 400000,
      .pwm_freq_hz = 1000.0f,
  };

  TEST_ASSERT_EQUAL(ESP_OK, pca9685_init(&cfg));
  reset_log(); // ignore init traffic

  for (uint8_t ch = 0; ch < 8; ++ch) {
    TEST_ASSERT_EQUAL(ESP_OK, pca9685_set_duty(ch, 1.0f));
    TEST_ASSERT_TRUE(s_log_count > 0);
    const write_log_t high = s_log[s_log_count - 1];
    TEST_ASSERT_EQUAL_UINT8(PCA9685_LED_BASE_REG + ch * 4, high.reg);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)high.len);
    TEST_ASSERT_EQUAL_UINT8(0x10, high.data[1]); // full ON bit
    TEST_ASSERT_EQUAL_UINT8(0x00, high.data[3]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, pca9685_get_cached_duty(ch));
    reset_log();

    TEST_ASSERT_EQUAL(ESP_OK, pca9685_set_duty(ch, 0.0f));
    TEST_ASSERT_TRUE(s_log_count > 0);
    const write_log_t low = s_log[s_log_count - 1];
    TEST_ASSERT_EQUAL_UINT8(PCA9685_LED_BASE_REG + ch * 4, low.reg);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)low.len);
    TEST_ASSERT_EQUAL_UINT8(0x00, low.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x10, low.data[3]); // full OFF bit
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pca9685_get_cached_duty(ch));
    reset_log();
  }

  pca9685_remove_i2c_stub();
  pca9685_reset_state();
  i2c_driver_delete(cfg.i2c_port);
}

#include "esp_log.h"
#include "esp_err.h"
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config/board_pinmap.h"
#include "pca9685_driver.h"

static const char *TAG = "lumigrid";

void app_main(void) {
  ESP_LOGI(TAG, "LumiGrid LED Node firmware bootstrap");

  const pca9685_config_t pwm_cfg = {
      .i2c_port = I2C_NUM_0,
      .i2c_addr = PCA9685_I2C_ADDR,
      .sda_gpio = I2C_SDA,
      .scl_gpio = I2C_SCL,
      .oe_gpio = PCA9685_OE,
      .i2c_clk_speed_hz = 400000,
      .pwm_freq_hz = 1000.0f,
  };

  esp_err_t err = pca9685_init(&pwm_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize PCA9685: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "PWM driver initialized");
    pca9685_all_off();
  }

  ESP_LOGI(TAG, "Running unit tests...");
  vTaskDelay(pdMS_TO_TICKS(2000));
  unity_run_tests_by_tag("[pca9685]", false);

  ESP_LOGI(TAG, "Tests finished. Halting.");
  vTaskDelay(portMAX_DELAY);
}

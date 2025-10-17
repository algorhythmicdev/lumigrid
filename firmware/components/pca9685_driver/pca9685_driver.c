#include "pca9685_driver.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "../../main/config/board_pinmap.h" // This include is for board-specific pin definitions, not generic PCA9685. Remove if not needed.

#define PCA9685_MODE1 0x00
#define PCA9685_PRESCALE 0xFE
#define PCA9685_LED0_ON_L 0x06

static const char *TAG = "PCA9685";

static i2c_port_t g_i2c_port;
static uint8_t g_pca9685_addr;
static gpio_num_t g_oe_gpio;

esp_err_t pca9685_init(const pca9685_config_t *config) {
    g_i2c_port = config->i2c_port;
    g_pca9685_addr = config->i2c_addr;
    g_oe_gpio = config->oe_gpio;

    // Configure Output Enable pin
    gpio_config_t oe_conf = {
        .pin_bit_mask = (1ULL << g_oe_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&oe_conf);
    gpio_set_level(g_oe_gpio, 1); // Disable output initially

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c_clk_speed_hz,
    };
    i2c_param_config(g_i2c_port, &conf);
    esp_err_t ret = i2c_driver_install(g_i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return ret;
    }

    // Wake up PCA9685
    uint8_t write_buf[2] = {PCA9685_MODE1, 0x00};
    ret = i2c_master_write_to_device(g_i2c_port, g_pca9685_addr, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake up PCA9685");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(1)); // Wait for oscillator

    // Set prescaler for target PWM frequency
    uint8_t prescale = (uint8_t)(25000000 / (4096 * config->pwm_freq_hz) - 1);
    write_buf[0] = PCA9685_MODE1;
    write_buf[1] = 0x10; // Sleep
    i2c_master_write_to_device(g_i2c_port, g_pca9685_addr, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
    
    write_buf[0] = PCA9685_PRESCALE;
    write_buf[1] = prescale;
    i2c_master_write_to_device(g_i2c_port, g_pca9685_addr, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));

    write_buf[0] = PCA9685_MODE1;
    write_buf[1] = 0x80; // Restart
    i2c_master_write_to_device(g_i2c_port, g_pca9685_addr, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
    
    gpio_set_level(g_oe_gpio, 0); // Enable output

    ESP_LOGI(TAG, "PCA9685 initialized with prescale value %d for %.1f Hz", prescale, config->pwm_freq_hz);
    return ESP_OK;
}

void pca9685_set_duty(uint8_t channel, float duty) {
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    uint16_t off = (uint16_t)(duty * 4095.0f);
    uint16_t on = 0;

    uint8_t write_buf[5];
    write_buf[0] = PCA9685_LED0_ON_L + 4 * channel;
    write_buf[1] = on & 0xFF;
    write_buf[2] = on >> 8;
    write_buf[3] = off & 0xFF;
    write_buf[4] = off >> 8;

    i2c_master_write_to_device(g_i2c_port, g_pca9685_addr, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
}

void pca9685_fade_to(uint8_t channel, float target_duty, uint32_t duration_ms, bool logarithmic) {
    // TODO: Implement fade logic
    pca9685_set_duty(channel, target_duty);
}

void pca9685_all_off(void) {
    gpio_set_level(g_oe_gpio, 1); // Disable output
    vTaskDelay(pdMS_TO_TICKS(1));      // Keep it off briefly
    gpio_set_level(g_oe_gpio, 0); // Enable output
}

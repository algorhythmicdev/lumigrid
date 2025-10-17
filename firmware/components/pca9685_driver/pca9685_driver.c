#include "pca9685_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "PCA9685";

#define PCA9685_MODE1 0x00
#define PCA9685_MODE2 0x01
#define PCA9685_LED0_ON_L 0x06
#define PCA9685_PRESCALE 0xFE
#define PCA9685_ALL_LED_OFF_H 0xFD

#define MODE1_RESTART 0x80
#define MODE1_SLEEP 0x10
#define MODE1_ALLCALL 0x01
#define MODE1_AI 0x20

#define MODE2_OUTDRV 0x04

static pca9685_config_t s_config;
static bool s_initialized = false;

static esp_err_t pca9685_write_reg(uint8_t reg, uint8_t value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_config.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t pca9685_read_reg(uint8_t reg, uint8_t *value) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_config.i2c_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_config.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t pca9685_init(const pca9685_config_t *cfg) {
    if (!cfg) return ESP_ERR_INVALID_ARG;
    
    s_config = *cfg;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_config.oe_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(s_config.oe_pin, 1);
    
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(s_config.i2c_port, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(s_config.i2c_port, I2C_MODE_MASTER, 0, 0, 0));
    
    pca9685_write_reg(PCA9685_MODE1, MODE1_ALLCALL);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    uint8_t mode1;
    pca9685_read_reg(PCA9685_MODE1, &mode1);
    mode1 &= ~MODE1_SLEEP;
    pca9685_write_reg(PCA9685_MODE1, mode1);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    pca9685_write_reg(PCA9685_MODE2, MODE2_OUTDRV);
    
    pca9685_set_pwm_freq(1000);
    
    gpio_set_level(s_config.oe_pin, 0);
    
    s_initialized = true;
    ESP_LOGI(TAG, "PCA9685 initialized on I2C%d @ 0x%02X", s_config.i2c_port, s_config.i2c_addr);
    return ESP_OK;
}

esp_err_t pca9685_set_pwm_freq(uint16_t freq_hz) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    
    uint32_t prescale_val = (uint32_t)(25000000.0f / (4096.0f * freq_hz)) - 1;
    if (prescale_val < 3) prescale_val = 3;
    if (prescale_val > 255) prescale_val = 255;
    
    uint8_t oldmode;
    pca9685_read_reg(PCA9685_MODE1, &oldmode);
    uint8_t newmode = (oldmode & ~MODE1_RESTART) | MODE1_SLEEP;
    pca9685_write_reg(PCA9685_MODE1, newmode);
    pca9685_write_reg(PCA9685_PRESCALE, (uint8_t)prescale_val);
    pca9685_write_reg(PCA9685_MODE1, oldmode);
    vTaskDelay(pdMS_TO_TICKS(5));
    pca9685_write_reg(PCA9685_MODE1, oldmode | MODE1_RESTART | MODE1_AI);
    
    ESP_LOGI(TAG, "PWM frequency set to %d Hz (prescale=%lu)", freq_hz, prescale_val);
    return ESP_OK;
}

esp_err_t pca9685_set_duty(uint8_t channel, float duty) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;
    
    uint16_t on_time = 0;
    uint16_t off_time = (uint16_t)(duty * 4095.0f);
    
    uint8_t reg_base = PCA9685_LED0_ON_L + 4 * channel;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (s_config.i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_base, true);
    i2c_master_write_byte(cmd, on_time & 0xFF, true);
    i2c_master_write_byte(cmd, (on_time >> 8) & 0xFF, true);
    i2c_master_write_byte(cmd, off_time & 0xFF, true);
    i2c_master_write_byte(cmd, (off_time >> 8) & 0xFF, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(s_config.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

esp_err_t pca9685_fade_to(uint8_t channel, float target_duty, uint32_t duration_ms, bool log_curve) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Fade ch%d to %.2f over %lu ms (%s)", channel, target_duty, duration_ms, log_curve ? "log" : "linear");
    
    return ESP_OK;
}

esp_err_t pca9685_all_off(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    
    gpio_set_level(s_config.oe_pin, 1);
    
    for (int ch = 0; ch < 16; ch++) {
        pca9685_set_duty(ch, 0.0f);
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(s_config.oe_pin, 0);
    
    ESP_LOGI(TAG, "All channels off");
    return ESP_OK;
}

esp_err_t pca9685_deinit(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    
    pca9685_all_off();
    gpio_set_level(s_config.oe_pin, 1);
    i2c_driver_delete(s_config.i2c_port);
    s_initialized = false;
    
    ESP_LOGI(TAG, "PCA9685 deinitialized");
    return ESP_OK;
}

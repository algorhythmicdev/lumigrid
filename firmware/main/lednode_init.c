#include "lednode_init.h"
#include "board_pinmap.h"
#include "pca9685_driver.h"
#include "effects.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "INIT";

void lednode_init(void) {
    ESP_LOGI(TAG, "Board bring-up");
    
    gpio_set_direction(PCA9685_OE, GPIO_MODE_OUTPUT);
    gpio_set_level(PCA9685_OE, 1);
    
    for (int i = 0; i < 8; i++) {
        gpio_set_direction(ALED_GPIO[i], GPIO_MODE_OUTPUT);
        gpio_set_level(ALED_GPIO[i], 0);
    }
    
    pca9685_config_t pca_cfg = {
        .i2c_port = I2C_NUM_0,
        .i2c_addr = PCA9685_I2C_ADDR,
        .oe_pin = PCA9685_OE,
    };
    ESP_ERROR_CHECK(pca9685_init(&pca_cfg));
    ESP_LOGI(TAG, "PCA9685 initialized");
    
    fx_init_all();
    ESP_LOGI(TAG, "Effects initialized");
    
    for (int ch = 0; ch < 8; ch++) {
        pca9685_set_duty(ch, 0.0f);
    }
    ESP_LOGI(TAG, "PWM channels initialized");
    
    ESP_LOGI(TAG, "Self-test: Pulse LEDch1");
    pca9685_set_duty(0, 0.5f);
    vTaskDelay(pdMS_TO_TICKS(500));
    pca9685_set_duty(0, 0.0f);
    
    ESP_LOGI(TAG, "Initialization complete");
}

#include "lednode_init.h"
#include "board_pinmap.h"
#include "pca9685_driver.h"
#include "effects.h"
#include "storage_fs.h"
#include "task_wifi.h"
#include "rest_api.h"
#include "ui_server.h"
#include "sync_protocol.h"
#include "mqtt_wrapper.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "INIT";

void lednode_init(void) {
    ESP_LOGI(TAG, "=== LumiGrid LED Node Initialization ===");
    
    ESP_LOGI(TAG, "[1/8] Board GPIO setup");
    gpio_set_direction(PCA9685_OE, GPIO_MODE_OUTPUT);
    gpio_set_level(PCA9685_OE, 1);
    
    for (int i = 0; i < 8; i++) {
        gpio_set_direction(ALED_GPIO[i], GPIO_MODE_OUTPUT);
        gpio_set_level(ALED_GPIO[i], 0);
    }
    
    ESP_LOGI(TAG, "[2/8] Filesystem mount");
    storage_fs_init();
    
    ESP_LOGI(TAG, "[3/8] PCA9685 PWM driver");
    pca9685_config_t pca_cfg = {
        .i2c_port = I2C_NUM_0,
        .i2c_addr = PCA9685_I2C_ADDR,
        .oe_pin = PCA9685_OE,
    };
    ESP_ERROR_CHECK(pca9685_init(&pca_cfg));
    
    ESP_LOGI(TAG, "[4/8] LED effects engine");
    fx_init_all();
    
    for (int ch = 0; ch < 8; ch++) {
        pca9685_set_duty(ch, 0.0f);
    }
    
    ESP_LOGI(TAG, "[5/8] Wi-Fi connection");
    task_wifi_init();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "[6/8] HTTP server (REST + UI)");
    rest_api_start();
    ui_server_start(rest_api_get_server());
    
    ESP_LOGI(TAG, "[7/8] Communication protocols");
    sync_protocol_init();
    mqtt_wrapper_init();
    
    ESP_LOGI(TAG, "[8/8] Self-test");
    pca9685_set_duty(0, 0.5f);
    vTaskDelay(pdMS_TO_TICKS(500));
    pca9685_set_duty(0, 0.0f);
    
    ESP_LOGI(TAG, "=== Initialization Complete ===");
    ESP_LOGI(TAG, "Access web UI: http://192.168.4.1 (AP mode)");
}

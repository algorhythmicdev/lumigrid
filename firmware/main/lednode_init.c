#include "lednode_init.h"
#include "board_pinmap.h"
#include "pca9685_driver.h"
#include "storage_fs.h"
#include "task_wifi.h"
#include "rest_api.h"
#include "ui_server.h"
#include "sync_protocol.h"
#include "mqtt_wrapper.h"
#include "task_effect_engine.h"
#include "task_pwm_driver.h"
#include "trigger_engine.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include <stddef.h>

static bool rest_bridge_set_base(int ch, const effect_params_t *params, uint32_t fade_ms){
    return effect_engine_set_base(ch, params, fade_ms);
}

static bool rest_bridge_set_overlay(int ch, const effect_params_t *params){
    return effect_engine_set_overlay(ch, params);
}

static void rest_bridge_clear_overlay(int ch){
    effect_engine_clear_overlay(ch);
}

static void rest_bridge_get_power_scale(float *out, size_t len){
    if (!out || len == 0){
        return;
    }
    effect_engine_stats_t stats;
    effect_engine_get_stats(&stats);
    size_t count = len < EFFECT_ENGINE_CH_MAX ? len : EFFECT_ENGINE_CH_MAX;
    for (size_t i = 0; i < count; ++i){
        out[i] = stats.power_scale[i];
    }
    for (size_t i = count; i < len; ++i){
        out[i] = 1.0f;
    }
}

static const rest_api_effect_ops_t REST_EFFECT_OPS = {
    .set_base = rest_bridge_set_base,
    .set_overlay = rest_bridge_set_overlay,
    .clear_overlay = rest_bridge_clear_overlay,
    .get_power_scale = rest_bridge_get_power_scale,
    .channel_count = effect_engine_channel_count,
    .get_channel_info = effect_engine_get_channel_info,
    .set_channel_type = effect_engine_set_channel_type
};

static const rest_api_pwm_ops_t REST_PWM_OPS = {
    .mode_static = pwm_set_mode_static,
    .mode_breath = pwm_set_mode_breath,
    .mode_candle = pwm_set_mode_candle,
    .mode_warmdim = pwm_set_mode_warmdim,
    .groups_count = pwm_groups_count,
    .get_group = pwm_groups_get,
    .replace_groups = pwm_groups_replace,
    .group_set_rgb = pwm_group_set_rgb,
    .group_set_rgbw = pwm_group_set_rgbw
};

static void rest_bridge_set_beat(float phase){
    trigger_set_beat(phase);
}

static const rest_api_trigger_ops_t REST_TRIGGER_OPS = {
    .set_beat = rest_bridge_set_beat,
    .strobe = trigger_strobe
};

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
    task_effect_engine_start();
    task_pwm_driver_start();
    rest_api_register_effect_ops(&REST_EFFECT_OPS);
    rest_api_register_pwm_ops(&REST_PWM_OPS);
    rest_api_register_trigger_ops(&REST_TRIGGER_OPS);
    
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

#include "config.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>
#include <esp_log.h>

static const char *TAG = "CFG";
static lumigrid_config_t current_config;
static nvs_handle_t config_nvs_handle;

esp_err_t config_init(void) {
    esp_err_t err = nvs_open("lumigrid", NVS_READWRITE, &config_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // Set defaults from Kconfig
    current_config.led_count = CONFIG_LUMIGRID_LED_COUNT;
    current_config.led_pin = CONFIG_LUMIGRID_LED_PIN;
    current_config.led_is_rgbw = false;
    current_config.udp_port = CONFIG_LUMIGRID_UDP_PORT;
    current_config.fps_limit = CONFIG_LUMIGRID_FPS_LIMIT;
    strncpy(current_config.wifi_ssid, CONFIG_ESP_WIFI_SSID, sizeof(current_config.wifi_ssid) - 1);
    strncpy(current_config.wifi_pass, CONFIG_ESP_WIFI_PASSWORD, sizeof(current_config.wifi_pass) - 1);
    current_config.ap_mode = true;

    // Try to load from NVS
    if (config_load(&current_config) != ESP_OK) {
        ESP_LOGW(TAG, "Using default configuration");
        // Save defaults to NVS
        config_save(&current_config);
    }

    config_print();
    return ESP_OK;
}

esp_err_t config_load(lumigrid_config_t *config) {
    esp_err_t err;
    size_t required_size;

    // Load each configuration item
    err = nvs_get_u16(config_nvs_handle, "led_count", &config->led_count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    err = nvs_get_u8(config_nvs_handle, "led_pin", &config->led_pin);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    uint8_t is_rgbw = 0;
    err = nvs_get_u8(config_nvs_handle, "led_is_rgbw", &is_rgbw);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    config->led_is_rgbw = is_rgbw != 0;

    err = nvs_get_u16(config_nvs_handle, "udp_port", &config->udp_port);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    err = nvs_get_u8(config_nvs_handle, "fps_limit", &config->fps_limit);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    required_size = sizeof(config->wifi_ssid);
    err = nvs_get_str(config_nvs_handle, "wifi_ssid", config->wifi_ssid, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    required_size = sizeof(config->wifi_pass);
    err = nvs_get_str(config_nvs_handle, "wifi_pass", config->wifi_pass, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;

    uint8_t ap_mode = 0;
    err = nvs_get_u8(config_nvs_handle, "ap_mode", &ap_mode);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    config->ap_mode = ap_mode != 0;

    return ESP_OK;
}

esp_err_t config_save(const lumigrid_config_t *config) {
    esp_err_t err;

    err = nvs_set_u16(config_nvs_handle, "led_count", config->led_count);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(config_nvs_handle, "led_pin", config->led_pin);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(config_nvs_handle, "led_is_rgbw", config->led_is_rgbw ? 1 : 0);
    if (err != ESP_OK) return err;

    err = nvs_set_u16(config_nvs_handle, "udp_port", config->udp_port);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(config_nvs_handle, "fps_limit", config->fps_limit);
    if (err != ESP_OK) return err;

    err = nvs_set_str(config_nvs_handle, "wifi_ssid", config->wifi_ssid);
    if (err != ESP_OK) return err;

    err = nvs_set_str(config_nvs_handle, "wifi_pass", config->wifi_pass);
    if (err != ESP_OK) return err;

    err = nvs_set_u8(config_nvs_handle, "ap_mode", config->ap_mode ? 1 : 0);
    if (err != ESP_OK) return err;

    err = nvs_commit(config_nvs_handle);
    if (err != ESP_OK) return err;

    // Update current_config with the new values
    memcpy(&current_config, config, sizeof(lumigrid_config_t));
    
    return ESP_OK;
}

void config_print(void) {
    ESP_LOGI(TAG, "Current configuration:");
    ESP_LOGI(TAG, "LED count: %d", current_config.led_count);
    ESP_LOGI(TAG, "LED pin: %d", current_config.led_pin);
    ESP_LOGI(TAG, "LED type: %s", current_config.led_is_rgbw ? "RGBW" : "RGB");
    ESP_LOGI(TAG, "UDP port: %d", current_config.udp_port);
    ESP_LOGI(TAG, "FPS limit: %d", current_config.fps_limit);
    ESP_LOGI(TAG, "WiFi mode: %s", current_config.ap_mode ? "AP" : "STA");
    ESP_LOGI(TAG, "WiFi SSID: %s", current_config.wifi_ssid);
}

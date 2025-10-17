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
    // Set defaults (could use sdkconfig here)
    current_config.led_count = 300;
    current_config.led_pin = 13;
    current_config.led_is_rgbw = false;
    current_config.udp_port = 4210;
    current_config.fps_limit = 60;
    strcpy(current_config.wifi_ssid, "LumiGridSetup");
    strcpy(current_config.wifi_pass, "lumigrid123");
    current_config.ap_mode = true;
    config_load(&current_config);
    config_print();
    return ESP_OK;
}

esp_err_t config_load(lumigrid_config_t *config) {
    size_t sz;
    nvs_get_u16(config_nvs_handle, "led_count", &config->led_count);
    nvs_get_u8(config_nvs_handle, "led_pin", &config->led_pin);
    uint8_t is_rgbw = 0;
    nvs_get_u8(config_nvs_handle, "led_is_rgbw", &is_rgbw);
    config->led_is_rgbw = is_rgbw;
    nvs_get_u16(config_nvs_handle, "udp_port", &config->udp_port);
    nvs_get_u8(config_nvs_handle, "fps_limit", &config->fps_limit);
    sz = sizeof(config->wifi_ssid);
    nvs_get_str(config_nvs_handle, "wifi_ssid", config->wifi_ssid, &sz);
    sz = sizeof(config->wifi_pass);
    nvs_get_str(config_nvs_handle, "wifi_pass", config->wifi_pass, &sz);
    uint8_t ap_mode = 1;
    nvs_get_u8(config_nvs_handle, "ap_mode", &ap_mode);
    config->ap_mode = ap_mode;
    return ESP_OK;
}

esp_err_t config_save(const lumigrid_config_t *config) {
    nvs_set_u16(config_nvs_handle, "led_count", config->led_count);
    nvs_set_u8(config_nvs_handle, "led_pin", config->led_pin);
    nvs_set_u8(config_nvs_handle, "led_is_rgbw", config->led_is_rgbw ? 1 : 0);
    nvs_set_u16(config_nvs_handle, "udp_port", config->udp_port);
    nvs_set_u8(config_nvs_handle, "fps_limit", config->fps_limit);
    nvs_set_str(config_nvs_handle, "wifi_ssid", config->wifi_ssid);
    nvs_set_str(config_nvs_handle, "wifi_pass", config->wifi_pass);
    nvs_set_u8(config_nvs_handle, "ap_mode", config->ap_mode ? 1 : 0);
    nvs_commit(config_nvs_handle);
    memcpy(&current_config, config, sizeof(lumigrid_config_t));
    return ESP_OK;
}

void config_print(void) {
    ESP_LOGI(TAG, "LED count: %d, pin: %d, RGBW: %d, UDP port: %d, FPS: %d, SSID: %s, AP: %d",
        current_config.led_count, current_config.led_pin, current_config.led_is_rgbw,
        current_config.udp_port, current_config.fps_limit, current_config.wifi_ssid, current_config.ap_mode);
}

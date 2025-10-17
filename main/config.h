#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

typedef struct {
    uint16_t led_count;
    uint8_t led_pin;
    bool led_is_rgbw;
    uint16_t udp_port;
    uint8_t fps_limit;
    char wifi_ssid[32];
    char wifi_pass[64];
    bool ap_mode;
} lumigrid_config_t;

esp_err_t config_init(void);
esp_err_t config_load(lumigrid_config_t *config);
esp_err_t config_save(const lumigrid_config_t *config);
void config_print(void);

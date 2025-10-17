#pragma once

#include "esp_err.h"

typedef struct {
    char ssid[33];
    char pass[65];
} wifi_creds_t;

void task_wifi_init(void);
esp_err_t wifi_start_sta(const wifi_creds_t* creds);
esp_err_t wifi_start_ap_fallback(void);
esp_err_t wifi_save_creds_to_nvs(const wifi_creds_t* creds);
esp_err_t wifi_load_creds_from_nvs(wifi_creds_t* creds);

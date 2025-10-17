#pragma once
#include <esp_err.h>

esp_err_t wifi_init(void);
void wifi_task(void *pvParameter);
esp_err_t wifi_start_ap(void);
esp_err_t wifi_connect_sta(const char *ssid, const char *password);
esp_err_t wifi_disconnect(void);
bool wifi_is_connected(void);
char* wifi_get_ip(void);

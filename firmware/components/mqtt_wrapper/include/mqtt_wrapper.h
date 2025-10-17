#pragma once

#include "esp_err.h"

esp_err_t mqtt_start(const char *uri);
esp_err_t mqtt_publish_status(const char *json);
esp_err_t mqtt_stop(void);
void mqtt_wrapper_init(void);

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t rest_api_start(void);
httpd_handle_t rest_api_get_server(void);
void rest_api_init(void);

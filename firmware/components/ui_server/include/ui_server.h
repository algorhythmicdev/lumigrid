#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t ui_server_start(httpd_handle_t server);
void ui_server_init(void);

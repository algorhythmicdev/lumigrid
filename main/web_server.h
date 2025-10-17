#pragma once
#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t web_server_init(void);
void web_server_task(void *pvParameter);

// API endpoints
esp_err_t api_get_info_handler(httpd_req_t *req);
esp_err_t api_get_config_handler(httpd_req_t *req);
esp_err_t api_set_config_handler(httpd_req_t *req);
esp_err_t api_preview_handler(httpd_req_t *req);
esp_err_t websocket_handler(httpd_req_t *req);

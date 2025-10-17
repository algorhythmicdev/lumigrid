#include "web_server.h"
#include "config.h"
#include "led_driver.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>

static const char *TAG = "HTTPD";
static httpd_handle_t server = NULL;

esp_err_t web_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 10;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server, "/api/info", api_get_info_handler);
        httpd_register_uri_handler(server, "/api/config", api_get_config_handler);
        httpd_register_uri_handler(server, "/api/config", api_set_config_handler);
        httpd_register_uri_handler(server, "/api/preview", api_preview_handler);
        httpd_register_uri_handler(server, "/ws", websocket_handler);
        
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}

void web_server_task(void *pvParameter) {
    // Web server runs in httpd task, this is for additional processing if needed
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t api_get_info_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "LumiGrid ESP32 Controller");
    cJSON_AddStringToObject(root, "version", "1.0.0");
    cJSON_AddStringToObject(root, "ip", wifi_get_ip());
    
    lumigrid_config_t config;
    if (config_load(&config) == ESP_OK) {
        cJSON_AddNumberToObject(root, "led_count", config.led_count);
        cJSON_AddNumberToObject(root, "udp_port", config.udp_port);
    }

    const char *response = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    free((void*)response);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_get_config_handler(httpd_req_t *req) {
    lumigrid_config_t config;
    if (config_load(&config) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load config");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "led_count", config.led_count);
    cJSON_AddNumberToObject(root, "led_pin", config.led_pin);
    cJSON_AddBoolToObject(root, "led_is_rgbw", config.led_is_rgbw);
    cJSON_AddNumberToObject(root, "udp_port", config.udp_port);
    cJSON_AddNumberToObject(root, "fps_limit", config.fps_limit);
    cJSON_AddStringToObject(root, "wifi_ssid", config.wifi_ssid);
    cJSON_AddBoolToObject(root, "ap_mode", config.ap_mode);

    const char *response = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    free((void*)response);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_set_config_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) {
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Method not allowed");
        return ESP_FAIL;
    }

    char content[512];
    int ret = httpd_req_recv(req, content, sizeof(content));
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request body");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    lumigrid_config_t config;
    if (config_load(&config) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load config");
        return ESP_FAIL;
    }

    // Update config from JSON
    cJSON *item = cJSON_GetObjectItem(root, "led_count");
    if (item) config.led_count = item->valueint;
    
    item = cJSON_GetObjectItem(root, "led_pin");
    if (item) config.led_pin = item->valueint;
    
    item = cJSON_GetObjectItem(root, "led_is_rgbw");
    if (item) config.led_is_rgbw = cJSON_IsTrue(item);
    
    item = cJSON_GetObjectItem(root, "udp_port");
    if (item) config.udp_port = item->valueint;
    
    item = cJSON_GetObjectItem(root, "fps_limit");
    if (item) config.fps_limit = item->valueint;
    
    item = cJSON_GetObjectItem(root, "wifi_ssid");
    if (item) strncpy(config.wifi_ssid, item->valuestring, sizeof(config.wifi_ssid) - 1);
    
    item = cJSON_GetObjectItem(root, "ap_mode");
    if (item) config.ap_mode = cJSON_IsTrue(item);

    if (config_save(&config) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    cJSON_Delete(root);
    httpd_resp_send(req, "Config updated", -1);
    return ESP_OK;
}

esp_err_t api_preview_handler(httpd_req_t *req) {
    // Simple preview endpoint - sets all LEDs to a test pattern
    pixel_t test_pixel = {255, 0, 255, 0}; // Magenta
    led_set_all(test_pixel);
    led_show();
    
    httpd_resp_send(req, "Preview activated", -1);
    return ESP_OK;
}

esp_err_t websocket_handler(httpd_req_t *req) {
    // Basic WebSocket handler - for now just echo
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    // Receive frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        if (ws_pkt.len > 0) {
            buf = calloc(1, ws_pkt.len + 1);
            if (buf == NULL) {
                ESP_LOGE(TAG, "Failed to calloc memory for buf");
                return ESP_ERR_NO_MEM;
            }
            ws_pkt.payload = buf;
            ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
                free(buf);
                return ret;
            }
            ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
        }
        
        // Echo back
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        }
        free(buf);
    }
    
    return ret;
}

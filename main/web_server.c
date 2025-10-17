#include "web_server.h"
#include "config.h"
#include "wifi.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>

static const char *TAG = "HTTPD";
static httpd_handle_t server = NULL;

static esp_err_t api_info(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "LumiGrid ESP32 Controller");
    cJSON_AddStringToObject(root, "ip", wifi_get_ip());
    cJSON_AddNumberToObject(root, "led_count", 300);
    const char *resp = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(root);
    free((void*)resp);
    return ESP_OK;
}

static esp_err_t api_config_get(httpd_req_t *req) {
    lumigrid_config_t cfg;
    config_load(&cfg);
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "led_count", cfg.led_count);
    cJSON_AddNumberToObject(root, "led_pin", cfg.led_pin);
    cJSON_AddBoolToObject(root, "led_is_rgbw", cfg.led_is_rgbw);
    cJSON_AddNumberToObject(root, "udp_port", cfg.udp_port);
    cJSON_AddNumberToObject(root, "fps_limit", cfg.fps_limit);
    cJSON_AddStringToObject(root, "wifi_ssid", cfg.wifi_ssid);
    cJSON_AddBoolToObject(root, "ap_mode", cfg.ap_mode);
    const char *resp = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(root);
    free((void*)resp);
    return ESP_OK;
}

static esp_err_t api_config_post(httpd_req_t *req) {
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (len <= 0) return ESP_FAIL;
    buf[len] = 0;
    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;
    lumigrid_config_t cfg;
    config_load(&cfg);
    cJSON *item;
    if ((item = cJSON_GetObjectItem(root, "led_count"))) cfg.led_count = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "led_pin"))) cfg.led_pin = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "led_is_rgbw"))) cfg.led_is_rgbw = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(root, "udp_port"))) cfg.udp_port = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "fps_limit"))) cfg.fps_limit = item->valueint;
    if ((item = cJSON_GetObjectItem(root, "wifi_ssid"))) strncpy(cfg.wifi_ssid, item->valuestring, sizeof(cfg.wifi_ssid)-1);
    if ((item = cJSON_GetObjectItem(root, "ap_mode"))) cfg.ap_mode = cJSON_IsTrue(item);
    config_save(&cfg);
    cJSON_Delete(root);
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t api_preview(httpd_req_t *req) {
    httpd_resp_send(req, "Preview", 7);
    return ESP_OK;
}

static esp_err_t ws_handler(httpd_req_t *req) {
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t web_server_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t info = { .uri="/api/info", .method=HTTP_GET, .handler=api_info, .user_ctx=NULL };
        httpd_uri_t cfg_get = { .uri="/api/config", .method=HTTP_GET, .handler=api_config_get, .user_ctx=NULL };
        httpd_uri_t cfg_post = { .uri="/api/config", .method=HTTP_POST, .handler=api_config_post, .user_ctx=NULL };
        httpd_uri_t preview = { .uri="/api/preview", .method=HTTP_GET, .handler=api_preview, .user_ctx=NULL };
        httpd_uri_t ws = { .uri="/ws", .method=HTTP_GET, .handler=ws_handler, .user_ctx=NULL };
        httpd_register_uri_handler(server, &info);
        httpd_register_uri_handler(server, &cfg_get);
        httpd_register_uri_handler(server, &cfg_post);
        httpd_register_uri_handler(server, &preview);
        httpd_register_uri_handler(server, &ws);
        // Serve static files (index.html) at /
        httpd_uri_t root = { .uri="/", .method=HTTP_GET, .handler=
            [](httpd_req_t *r) {
                extern const unsigned char index_html_start[] asm("_binary_webui_dist_index_html_start");
                extern const unsigned char index_html_end[]   asm("_binary_webui_dist_index_html_end");
                httpd_resp_set_type(r, "text/html");
                httpd_resp_send(r, (const char*)index_html_start, index_html_end - index_html_start);
                return ESP_OK;
            }, .user_ctx=NULL };
        httpd_register_uri_handler(server, &root);
        return ESP_OK;
    }
    return ESP_FAIL;
}

void web_server_task(void *pvParameter) {
    while (1) vTaskDelay(1000 / portTICK_PERIOD_MS);
}

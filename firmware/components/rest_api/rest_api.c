#include "rest_api.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "REST_API";
static httpd_handle_t s_server = NULL;

static esp_err_t json_reply(httpd_req_t *req, cJSON *root) {
    char *buf = cJSON_PrintUnformatted(root);
    if (!buf) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t get_status_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "node_type", "led-node");
    cJSON_AddStringToObject(root, "role", "Slave");
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);
    
    cJSON *fps = cJSON_AddObjectToObject(root, "fps");
    cJSON_AddNumberToObject(fps, "aled_ch1", 60);
    cJSON_AddNumberToObject(fps, "aled_ch2", 60);
    cJSON_AddNumberToObject(fps, "aled_ch3", 60);
    cJSON_AddNumberToObject(fps, "aled_ch4", 60);
    cJSON_AddNumberToObject(fps, "aled_ch5", 60);
    cJSON_AddNumberToObject(fps, "aled_ch6", 60);
    cJSON_AddNumberToObject(fps, "aled_ch7", 60);
    cJSON_AddNumberToObject(fps, "aled_ch8", 60);
    
    cJSON_AddNumberToObject(root, "pwm_max_duty", 0.85);
    cJSON_AddNullToObject(root, "last_error");
    cJSON_AddNumberToObject(root, "heap_free_kb", esp_get_free_heap_size() / 1024);
    
    return json_reply(req, root);
}

static esp_err_t get_config_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "node_type", "led-node");
    cJSON_AddStringToObject(root, "role", "Slave");
    cJSON_AddStringToObject(root, "control_mode", "Independent");
    
    return json_reply(req, root);
}

static esp_err_t post_config_handler(httpd_req_t *req) {
    char buf[2048];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Config update received");
    cJSON_Delete(json);
    
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    return ESP_OK;
}

static esp_err_t post_trigger_handler(httpd_req_t *req) {
    char buf[2048];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Trigger action received");
    cJSON_Delete(json);
    
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t post_cue_handler(httpd_req_t *req) {
    char buf[2048];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Cue received");
    cJSON_Delete(json);
    
    httpd_resp_sendstr(req, "{\"status\":\"queued\"}");
    return ESP_OK;
}

static esp_err_t get_presets_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateArray();
    
    cJSON *preset1 = cJSON_CreateObject();
    cJSON_AddStringToObject(preset1, "name", "solid_blue");
    cJSON_AddStringToObject(preset1, "effect", "solid");
    cJSON_AddItemToArray(root, preset1);
    
    return json_reply(req, root);
}

static esp_err_t post_presets_handler(httpd_req_t *req) {
    char buf[2048];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Preset saved");
    cJSON_Delete(json);
    
    httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
    return ESP_OK;
}

httpd_handle_t rest_api_get_server(void) {
    return s_server;
}

esp_err_t rest_api_start(void) {
    if (s_server) {
        ESP_LOGW(TAG, "Server already started");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    httpd_uri_t get_status = {
        .uri       = "/api/status",
        .method    = HTTP_GET,
        .handler   = get_status_handler,
    };
    httpd_register_uri_handler(s_server, &get_status);
    
    httpd_uri_t get_config = {
        .uri       = "/api/config",
        .method    = HTTP_GET,
        .handler   = get_config_handler,
    };
    httpd_register_uri_handler(s_server, &get_config);
    
    httpd_uri_t post_config = {
        .uri       = "/api/config",
        .method    = HTTP_POST,
        .handler   = post_config_handler,
    };
    httpd_register_uri_handler(s_server, &post_config);
    
    httpd_uri_t post_trigger = {
        .uri       = "/api/trigger",
        .method    = HTTP_POST,
        .handler   = post_trigger_handler,
    };
    httpd_register_uri_handler(s_server, &post_trigger);
    
    httpd_uri_t post_cue = {
        .uri       = "/api/cue",
        .method    = HTTP_POST,
        .handler   = post_cue_handler,
    };
    httpd_register_uri_handler(s_server, &post_cue);
    
    httpd_uri_t get_presets = {
        .uri       = "/api/presets",
        .method    = HTTP_GET,
        .handler   = get_presets_handler,
    };
    httpd_register_uri_handler(s_server, &get_presets);
    
    httpd_uri_t post_presets = {
        .uri       = "/api/presets",
        .method    = HTTP_POST,
        .handler   = post_presets_handler,
    };
    httpd_register_uri_handler(s_server, &post_presets);
    
    ESP_LOGI(TAG, "REST API server started");
    return ESP_OK;
}

void rest_api_init(void) {
    rest_api_start();
}

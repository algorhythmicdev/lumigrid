#include "ui_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "UI_SERVER";

static esp_err_t get_root_handler(httpd_req_t *req) {
    FILE *f = fopen("/spiffs/www/index.html.gz", "rb");
    if (!f) {
        f = fopen("/spiffs/www/index.html", "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open index.html");
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "UI file not found");
            return ESP_FAIL;
        }
        httpd_resp_set_type(req, "text/html; charset=utf-8");
    } else {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_set_type(req, "text/html; charset=utf-8");
    }
    
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
    
    char buf[2048];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t sse_events_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    httpd_resp_sendstr_chunk(req, ":ok\n\n");
    
    for (int i = 0; i < 10; i++) {
        char event_data[256];
        snprintf(event_data, sizeof(event_data),
                "event: status\ndata: {\"uptime\":%d,\"fps\":60,\"heap\":%lu}\n\n",
                i * 1000, esp_get_free_heap_size());
        
        if (httpd_resp_sendstr_chunk(req, event_data) != ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t ui_server_start(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "Invalid server handle");
        return ESP_FAIL;
    }
    
    httpd_uri_t get_root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = get_root_handler,
    };
    httpd_register_uri_handler(server, &get_root);
    
    httpd_uri_t sse_events = {
        .uri       = "/events",
        .method    = HTTP_GET,
        .handler   = sse_events_handler,
    };
    httpd_register_uri_handler(server, &sse_events);
    
    ESP_LOGI(TAG, "UI server handlers registered");
    return ESP_OK;
}

void ui_server_init(void) {
    ESP_LOGI(TAG, "UI server initialized (awaiting HTTP server)");
}

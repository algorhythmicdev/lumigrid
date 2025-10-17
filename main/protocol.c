#include "protocol.h"
#include "config.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

static const char *TAG = "PROTO";
static QueueHandle_t protocol_queue = NULL;
static int sock = -1;

esp_err_t protocol_init(void) {
    protocol_queue = xQueueCreate(10, sizeof(uint8_t*));
    if (protocol_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create protocol queue");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

void protocol_task(void *pvParameter) {
    lumigrid_config_t config;
    if (config_load(&config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load config");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config.udp_port);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket created, port %d", config.udp_port);

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket bound, port %d", config.udp_port);

    uint8_t rx_buffer[1024];
    
    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);
        
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                          (struct sockaddr *)&source_addr, &socklen);
        
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            continue;
        }
        
        // Process the received data
        protocol_handle_message(rx_buffer, len);
    }
}

esp_err_t protocol_handle_message(uint8_t *data, size_t len) {
    if (len < sizeof(message_header_t)) {
        ESP_LOGW(TAG, "Message too short");
        return ESP_ERR_INVALID_SIZE;
    }
    
    message_header_t *header = (message_header_t *)data;
    uint8_t *payload = data + sizeof(message_header_t);
    size_t payload_len = len - sizeof(message_header_t);
    
    if (payload_len != header->length) {
        ESP_LOGW(TAG, "Message length mismatch: expected %d, got %d", 
                header->length, payload_len);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGD(TAG, "Received message type: %d, length: %d", header->type, header->length);
    
    switch (header->type) {
        case MSG_SET_PIXEL: {
            if (payload_len < 5) break;
            uint16_t index = *(uint16_t*)payload;
            uint8_t r = payload[2];
            uint8_t g = payload[3];
            uint8_t b = payload[4];
            pixel_t pixel = {r, g, b, 0};
            led_set_pixel(index, pixel);
            break;
        }
        
        case MSG_SET_ALL: {
            if (payload_len < 3) break;
            uint8_t r = payload[0];
            uint8_t g = payload[1];
            uint8_t b = payload[2];
            pixel_t pixel = {r, g, b, 0};
            led_set_all(pixel);
            break;
        }
        
        case MSG_SHOW:
            led_show();
            break;
            
        case MSG_CLEAR:
            led_clear();
            led_show();
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown message type: %d", header->type);
            break;
    }
    
    return ESP_OK;
}

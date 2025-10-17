#include "sync_protocol.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "SYNC_PROTO";

#define MCAST_GRP "239.10.7.42"
#define MCAST_PORT 45454

typedef enum {
    SYNC_PACKET_TICK = 1,
    SYNC_PACKET_CUE = 2
} sync_packet_type_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;
    uint8_t type;
    uint8_t pad[3];
} sync_packet_t;

static int s_tx_socket = -1;
static int s_rx_socket = -1;
static bool s_running = false;

static void sync_tx_task(void *arg) {
    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(MCAST_PORT);
    inet_aton(MCAST_GRP, &dest_addr.sin_addr);
    
    ESP_LOGI(TAG, "TX task started, sending to %s:%d", MCAST_GRP, MCAST_PORT);
    
    while (s_running) {
        sync_packet_t packet = {
            .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
            .type = SYNC_PACKET_TICK,
            .pad = {0}
        };
        
        int err = sendto(s_tx_socket, &packet, sizeof(packet), 0,
                        (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        if (err < 0) {
            ESP_LOGW(TAG, "TX error: %d", errno);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    vTaskDelete(NULL);
}

static void sync_rx_task(void *arg) {
    ESP_LOGI(TAG, "RX task started, listening on %s:%d", MCAST_GRP, MCAST_PORT);
    
    while (s_running) {
        sync_packet_t packet;
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        
        int len = recvfrom(s_rx_socket, &packet, sizeof(packet), 0,
                          (struct sockaddr *)&source_addr, &addr_len);
        
        if (len == sizeof(packet)) {
            uint32_t local_ms = (uint32_t)(esp_timer_get_time() / 1000);
            int32_t drift_ms = local_ms - packet.timestamp_ms;
            
            if (packet.type == SYNC_PACKET_TICK) {
                ESP_LOGD(TAG, "Tick received, drift: %ld ms", drift_ms);
            } else if (packet.type == SYNC_PACKET_CUE) {
                ESP_LOGI(TAG, "Cue received at %lu", packet.timestamp_ms);
            }
        }
    }
    
    vTaskDelete(NULL);
}

esp_err_t sync_protocol_start_master(void) {
    if (s_running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    s_tx_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_tx_socket < 0) {
        ESP_LOGE(TAG, "Failed to create TX socket");
        return ESP_FAIL;
    }
    
    uint8_t ttl = 1;
    setsockopt(s_tx_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    
    s_running = true;
    xTaskCreate(sync_tx_task, "sync_tx", 4096, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "Master mode started");
    return ESP_OK;
}

esp_err_t sync_protocol_start_slave(void) {
    if (s_running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    s_rx_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_rx_socket < 0) {
        ESP_LOGE(TAG, "Failed to create RX socket");
        return ESP_FAIL;
    }
    
    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(MCAST_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(s_rx_socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind RX socket");
        close(s_rx_socket);
        return ESP_FAIL;
    }
    
    struct ip_mreq imreq = {0};
    inet_aton(MCAST_GRP, &imreq.imr_multiaddr);
    imreq.imr_interface.s_addr = htonl(INADDR_ANY);
    
    if (setsockopt(s_rx_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &imreq, sizeof(imreq)) < 0) {
        ESP_LOGE(TAG, "Failed to join multicast group");
        close(s_rx_socket);
        return ESP_FAIL;
    }
    
    s_running = true;
    xTaskCreate(sync_rx_task, "sync_rx", 4096, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "Slave mode started");
    return ESP_OK;
}

esp_err_t sync_protocol_stop(void) {
    s_running = false;
    
    if (s_tx_socket >= 0) {
        close(s_tx_socket);
        s_tx_socket = -1;
    }
    
    if (s_rx_socket >= 0) {
        close(s_rx_socket);
        s_rx_socket = -1;
    }
    
    ESP_LOGI(TAG, "Sync protocol stopped");
    return ESP_OK;
}

void sync_protocol_init(void) {
    ESP_LOGI(TAG, "Sync protocol initialized (call start_master or start_slave)");
}

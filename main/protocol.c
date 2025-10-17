#include "protocol.h"
#include "led_driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <esp_log.h>
static const char *TAG = "PROTO";

esp_err_t protocol_init(void) { return ESP_OK; }

void protocol_task(void *pvParameter) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) { vTaskDelete(NULL); return; }
    struct sockaddr_in addr = { .sin_family=AF_INET, .sin_port=htons(4210), .sin_addr.s_addr=INADDR_ANY };
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    uint8_t buf[64];
    while (1) {
        int len = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
        if (len > 0) {
            // TODO: parse protocol and call led_set_all/led_set_pixel/led_show
        }
    }
}

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "config.h"
#include "wifi.h"
#include "web_server.h"
#include "led_driver.h"
#include "protocol.h"
#include "mapper.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Starting LumiGrid ESP32 Controller");

    ESP_ERROR_CHECK(nvs_flash_init());
    config_init();
    mapper_init();
    wifi_init();
    led_driver_init();
    protocol_init();
    web_server_init();

    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);
    xTaskCreate(web_server_task, "web_server_task", 4096, NULL, 5, NULL);
    xTaskCreate(led_render_task, "led_render_task", 4096, NULL, 5, NULL);
    xTaskCreate(protocol_task, "protocol_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "All tasks started");
}

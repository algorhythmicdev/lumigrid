#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include "config.h"
#include "wifi.h"
#include "web_server.h"
#include "led_driver.h"
#include "protocol.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // Initialize NVS for configuration storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize all subsystems
    config_init();
    wifi_init();
    led_driver_init();
    protocol_init();
    web_server_init();
    
    ESP_LOGI(TAG, "LumiGrid ESP32 Controller initialized");
}

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lednode_init.h"
#include "version.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "LumiGrid LED Node v%s", LUMIGRID_VERSION_STRING);
    ESP_LOGI(TAG, "Build: %s %s", LUMIGRID_BUILD_DATE, LUMIGRID_BUILD_TIME);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Initializing LED Node...");
    lednode_init();
    
    ESP_LOGI(TAG, "LED Node running");
}

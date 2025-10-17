#include "unity.h"
#include "pca9685_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEST_PCA9685";

TEST_CASE("pca9685_set_duty_cycle", "[pca9685]")
{
    ESP_LOGI(TAG, "Running PCA9685 set duty cycle test...");

    ESP_LOGI(TAG, "Setting channel 0 to 50% duty cycle.");
    pca9685_set_duty(0, 0.5f);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Setting channel 1 to 25% duty cycle.");
    pca9685_set_duty(1, 0.25f);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Setting channel 7 to 100% duty cycle.");
    pca9685_set_duty(7, 1.0f);
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Turning all channels off.");
    for (int i = 0; i < 8; i++) {
        pca9685_set_duty(i, 0.0f);
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "Test complete.");
}

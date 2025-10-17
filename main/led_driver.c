#include "led_driver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "LED";
static QueueHandle_t led_cmd_queue = NULL;

esp_err_t led_driver_init(void) {
    led_cmd_queue = xQueueCreate(8, sizeof(pixel_t));
    return ESP_OK;
}

void led_render_task(void *pvParameter) {
    pixel_t px;
    while (1) {
        if (xQueueReceive(led_cmd_queue, &px, portMAX_DELAY)) {
            // TODO: send to WS2812 via RMT
        }
    }
}

esp_err_t led_set_pixel(uint16_t index, pixel_t pixel) { return ESP_OK; }
esp_err_t led_set_all(pixel_t pixel) {
    if (led_cmd_queue) xQueueSend(led_cmd_queue, &pixel, 0);
    return ESP_OK;
}
esp_err_t led_show(void) { return ESP_OK; }
esp_err_t led_clear(void) { return ESP_OK; }

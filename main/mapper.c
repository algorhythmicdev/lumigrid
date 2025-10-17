#include "mapper.h"
#include "config.h"
#include <esp_log.h>
#include <nvs.h>

static const char *TAG = "MAPPER";
static mapper_type_t current_mapper = MAPPER_LINEAR;
static uint16_t led_count = 0;

esp_err_t mapper_init(void) {
    lumigrid_config_t config;
    if (config_load(&config) == ESP_OK) {
        led_count = config.led_count;
    }
    
    // Load mapper type from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("lumigrid", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t mapper_type = 0;
        err = nvs_get_u8(nvs_handle, "mapper_type", &mapper_type);
        if (err == ESP_OK) {
            current_mapper = (mapper_type_t)mapper_type;
        }
        nvs_close(nvs_handle);
    }
    
    ESP_LOGI(TAG, "Mapper initialized with type: %d", current_mapper);
    return ESP_OK;
}

esp_err_t mapper_set_type(mapper_type_t type) {
    current_mapper = type;
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("lumigrid", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return err;
    
    err = nvs_set_u8(nvs_handle, "mapper_type", (uint8_t)type);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    return err;
}

uint16_t mapper_map_index(uint16_t logical_index) {
    if (logical_index >= led_count) {
        return logical_index; // Out of bounds, return as-is
    }
    
    switch (current_mapper) {
        case MAPPER_ZIGZAG: {
            // Simple zigzag pattern: even rows left-to-right, odd rows right-to-left
            uint16_t leds_per_row = 10; // Assume 10 LEDs per row for example
            uint16_t row = logical_index / leds_per_row;
            uint16_t col = logical_index % leds_per_row;
            
            if (row % 2 == 1) {
                col = leds_per_row - 1 - col; // Reverse direction for odd rows
            }
            
            return row * leds_per_row + col;
        }
        
        case MAPPER_SPIRAL: {
            // Simple spiral mapping (simplified)
            // This is a basic implementation - real spiral would be more complex
            return logical_index; // Placeholder
        }
        
        case MAPPER_LINEAR:
        default:
            return logical_index;
    }
}

esp_err_t mapper_reverse_map(uint16_t physical_index, uint16_t *logical_index) {
    // Reverse mapping - find logical index for physical position
    // This is a simplified implementation
    *logical_index = physical_index;
    return ESP_OK;
}

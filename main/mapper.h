#pragma once
#include <stdint.h>
#include <esp_err.h>

typedef enum {
    MAPPER_LINEAR = 0,
    MAPPER_ZIGZAG = 1,
    MAPPER_SPIRAL = 2
} mapper_type_t;

esp_err_t mapper_init(void);
esp_err_t mapper_set_type(mapper_type_t type);
uint16_t mapper_map_index(uint16_t logical_index);
esp_err_t mapper_reverse_map(uint16_t physical_index, uint16_t *logical_index);

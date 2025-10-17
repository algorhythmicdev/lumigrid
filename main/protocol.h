#pragma once
#include <esp_err.h>
#include "led_driver.h"

// Protocol message types
typedef enum {
    MSG_SET_PIXEL = 0x01,
    MSG_SET_RANGE = 0x02,
    MSG_SET_ALL = 0x03,
    MSG_SHOW = 0x04,
    MSG_CLEAR = 0x05,
    MSG_CONFIG = 0x06
} message_type_t;

// Protocol message header
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint16_t length;
} message_header_t;

esp_err_t protocol_init(void);
void protocol_task(void *pvParameter);
esp_err_t protocol_handle_message(uint8_t *data, size_t len);

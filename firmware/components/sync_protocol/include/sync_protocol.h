#pragma once

#include "esp_err.h"

esp_err_t sync_protocol_start_master(void);
esp_err_t sync_protocol_start_slave(void);
esp_err_t sync_protocol_stop(void);
void sync_protocol_init(void);

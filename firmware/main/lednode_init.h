#pragma once

#include "esp_err.h"
#include <stdbool.h>

void app_init(void);
bool set_last_error(esp_err_t err, const char *subsystem);
const char* get_last_error(void);

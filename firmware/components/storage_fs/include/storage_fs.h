#pragma once

#include "esp_err.h"

esp_err_t storage_fs_mount(void);
esp_err_t storage_fs_unmount(void);
void storage_fs_init(void);

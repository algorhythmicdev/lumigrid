#include "storage_fs.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "STORAGE_FS";
static bool s_mounted = false;

esp_err_t storage_fs_mount(void) {
    if (s_mounted) {
        ESP_LOGW(TAG, "Filesystem already mounted");
        return ESP_OK;
    }
    
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition info (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    s_mounted = true;
    ESP_LOGI(TAG, "LittleFS mounted successfully");
    return ESP_OK;
}

esp_err_t storage_fs_unmount(void) {
    if (!s_mounted) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_vfs_littlefs_unregister("storage");
    if (ret == ESP_OK) {
        s_mounted = false;
        ESP_LOGI(TAG, "LittleFS unmounted");
    }
    return ret;
}

void storage_fs_init(void) {
    storage_fs_mount();
}

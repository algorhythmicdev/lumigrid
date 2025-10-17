#include "mqtt_wrapper.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_WRAP";
static esp_mqtt_client_handle_t s_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            esp_mqtt_client_subscribe(s_client, "lumigrid/cmd/led-node/+", 0);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT data: %.*s = %.*s",
                    event->topic_len, event->topic,
                    event->data_len, event->data);
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
            
        default:
            break;
    }
}

esp_err_t mqtt_start(const char *uri) {
    if (!uri || strlen(uri) == 0) {
        ESP_LOGW(TAG, "No MQTT URI provided, skipping");
        return ESP_OK;
    }
    
    if (s_client) {
        ESP_LOGW(TAG, "MQTT client already started");
        return ESP_OK;
    }
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
    };
    
    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    
    ESP_LOGI(TAG, "MQTT client started: %s", uri);
    return ESP_OK;
}

esp_err_t mqtt_publish_status(const char *json) {
    if (!s_client || !json) {
        return ESP_FAIL;
    }
    
    int msg_id = esp_mqtt_client_publish(s_client,
                                         "lumigrid/tele/led-node/status",
                                         json, 0, 0, 0);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish status");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t mqtt_stop(void) {
    if (!s_client) {
        return ESP_OK;
    }
    
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    
    ESP_LOGI(TAG, "MQTT client stopped");
    return ESP_OK;
}

void mqtt_wrapper_init(void) {
    ESP_LOGI(TAG, "MQTT wrapper initialized (call mqtt_start with URI)");
}

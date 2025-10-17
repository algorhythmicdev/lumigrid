idf_component_register(
    SRCS 
        "app_main.c"
        "config.c"
        "wifi.c"
        "web_server.c"
        "led_driver.c"
        "protocol.c"
        "mapper.c"
    INCLUDE_DIRS "."
    REQUIRES 
        esp_wifi
        esp_http_server
        nvs_flash
        driver
        json
)

# Add embedded web UI files if enabled
if(CONFIG_EMBED_WEB_UI)
    file(GLOB_RECURSE web_files "../webui/dist/*")
    spiffs_create_partition_image(storage ../webui/dist FLASH_IN_PROJECT)
endif()

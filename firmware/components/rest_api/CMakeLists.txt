idf_component_register(
    SRCS "rest_api.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json esp_timer freertos led_effects
)

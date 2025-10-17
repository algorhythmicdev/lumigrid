idf_component_register(
    SRCS "rest_api.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json
)

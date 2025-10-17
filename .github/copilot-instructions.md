# LumiGrid ESP32 Controller – Copilot Instructions

## Overview
ESP-IDF project for an ESP32-based LED controller used by LumiGrid. Provides real-time LED rendering, device config via embedded web UI, and network ingestion of frames/commands.

## Repo structure (typical)
- main/: app_main.c and feature modules (wifi, web_server, led_driver, protocol, config)
- components/: reusable libs (e.g., led_strip/WS2812 via RMT, storage/NVS, httpd helpers, ota)
- webui/: web UI sources; built to dist/ and embedded (LittleFS/SPIFFS or EMBED_FILES)
- CMakeLists.txt, sdkconfig(.defaults), partitions.csv, Kconfig
- scripts/: helper scripts (flash, create FS image, send test frames)

## Build/Flash/Monitor
- idf.py set-target esp32
- idf.py build
- idf.py -p /dev/ttyUSB0 flash monitor
- idf.py menuconfig (configure Wi-Fi, LED type/pins, LumiGrid port/path)
- Reset device logs: idf.py erase_flash (then re-provision)

## Web UI pipeline
- From webui/: npm ci && npm run build (produces webui/dist)
- If using FS partition: idf.py littlefs_create_partition_image storage webui/dist (or spiffs_create_partition_image), then flash
- If embedding files: in main/CMakeLists.txt use idf_component_register(... EMBED_FILES "webui/dist/index.html" "webui/dist/assets/...")

## Runtime architecture
- FreeRTOS tasks: wifi/provisioning, httpd (esp_http_server), ws/ingest, led_render
- Config persisted in NVS; access via a config module (avoid direct scattered nvs_* calls)
- LED output via RMT (WS2812) or SPI/I2S (APA102); rendering runs on a dedicated task with queues/ringbuffers
- Web server serves UI + REST (e.g., /api/info, /api/config, /api/preview); WebSocket at /ws for live data if enabled

## LumiGrid integration
- Device receives frames/commands over network (UDP or WebSocket, see protocol module)
- sdkconfig exposes CONFIG_LUMIGRID_* (port, FPS limit, channel mapping); adjust via menuconfig and persist to NVS via API

## Conventions
- Logging tags: "NET", "HTTPD", "LED", "PROTO", "CFG"; set levels via esp_log_level_set
- No blocking work in httpd/WS callbacks; push to queues consumed by worker tasks
- Timing-critical code (RMT ISR, tight loops) avoids heap and uses IRAM_ATTR where needed
- Pin/LED layout comes from sdkconfig or NVS; mapping utilities live in a mapper module

## Common tasks
- Add REST route: register with httpd_register_uri_handler in web_server module; return JSON via cJSON
- Add WS message: extend protocol header/types; handle in ws task; enqueue to renderer
- Change LED mapping: update mapper and persist via config API; rebuild preview in UI
- OTA (if present): trigger via /api/ota with image URL; progress events over WS

## Testing/Debug
- Live logs: idf.py monitor; filter with LOG_LOCAL_LEVEL
- Network test: scripts/send_frame.py --host <ip> --port <port> (or WS client to /ws)
- Unit tests (if enabled): idf.py test

Key files to study first: main/app_main.c, main/web_server.c, main/led_driver.c, main/protocol.c, main/config.c, CMakeLists.txt, sdkconfig.defaults, partitions.csv.

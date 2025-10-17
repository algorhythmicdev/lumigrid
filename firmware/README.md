# LumiGrid Firmware

ESP32-IDF firmware for the LumiGrid LED Node controller.

## Quick Build

```bash
# From this directory (firmware/)
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash monitor
```

## Prerequisites

- ESP-IDF v5.x
- Python 3.8+
- ESP32-WROOM-32U target

## Components

This firmware includes 8 custom components:

1. **pca9685_driver** - PWM controller (I²C)
2. **led_effects** - Effect rendering engine
3. **aled_rmt** - RMT WS2812B/SK6812 driver
4. **rest_api** - HTTP REST endpoints
5. **ui_server** - Web UI + SSE serving
6. **storage_fs** - LittleFS filesystem
7. **sync_protocol** - UDP multicast sync
8. **mqtt_wrapper** - MQTT telemetry

## Features

- 8× Addressable LED channels (RMT)
- 8× PWM channels (PCA9685)
- Wi-Fi STA/AP with fallback
- REST API (7 endpoints)
- Server-Sent Events (SSE)
- Web UI (dark theme)
- MQTT pub/sub
- UDP sync protocol

## First Boot

Device creates AP:
- **SSID:** LumiGrid-Setup
- **Password:** lumigrid123
- **IP:** 192.168.4.1

## API Examples

```bash
# Get status
curl http://192.168.4.1/api/status

# Update config
curl -X POST http://192.168.4.1/api/config \
  -H "Content-Type: application/json" \
  -d '{"role":"Master"}'

# SSE stream
curl -N http://192.168.4.1/events
```

## Documentation

- [BUILD_GUIDE.md](BUILD_GUIDE.md) - Detailed build instructions
- [../PROJECT_REPORT.md](../PROJECT_REPORT.md) - Full architecture
- [../Docs/ARCHITECTURE.md](../Docs/ARCHITECTURE.md) - System design
- [../Docs/api_spec.yaml](../Docs/api_spec.yaml) - API specification

## Build from Root

If you're in the project root directory:

```bash
cd firmware
idf.py build
```

Or use the convenience script:

```bash
# From project root
./BUILD.sh
```

## Performance

- Free heap: ~110 KB (idle)
- REST latency: <15 ms
- Effect loop: <3 ms target
- SSE rate: 1-10 Hz

## License

TBD

---

**Note:** This is an ESP-IDF project. Build commands must be run from this `firmware/` directory.

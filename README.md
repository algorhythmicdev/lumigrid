# LumiGrid LED Node Firmware

**Version:** 1.0.0  
**Platform:** ESP32-WROOM-32U  
**Framework:** ESP-IDF v5.x

## Overview

LumiGrid is a professional-grade LED controller firmware for ESP32, supporting:
- 8× Addressable LED channels (WS2812B/SK6812 via RMT)
- 8× PWM channels (PCA9685 via I²C @ 1kHz)
- REST API + Web UI
- MQTT telemetry & control
- Master/Slave synchronization via UDP multicast
- Time-based scheduling with RRULE-like syntax

## Quick Start

### Prerequisites
```bash
# Install ESP-IDF v5.x
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# Verify installation
idf.py --version
```

### Build & Flash
```bash
cd firmware

# Configure (first time)
idf.py menuconfig

# Build
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure

```
firmware/
├── main/                 # Application code
│   ├── app_main.c        # Entry point
│   ├── config/           # Hardware & defaults
│   ├── tasks/            # FreeRTOS tasks
│   └── utils/            # Helper modules
├── components/           # Reusable components
│   ├── pca9685_driver/   # PWM controller
│   ├── led_effects/      # Effect engine
│   ├── rest_api/         # HTTP endpoints
│   ├── sync_protocol/    # UDP multicast
│   └── ...
└── assets/webapp/        # Web UI
```

## Hardware Pinout

| Function | GPIO | Notes |
|----------|------|-------|
| ALEDch1-8 | 16,4,17,18,19,23,26,27 | RMT/WS2812B |
| I²C SDA/SCL | 21/22 | PCA9685 @ 400kHz |
| PCA9685 OE | 25 | Output enable (active-low) |
| UART | 1/3 | Console |

## Configuration

Edit `main/config/default_config.json`:
```json
{
  "node_type": "led-node",
  "role": "Slave",
  "aled": [
    {"ch": 1, "pixels": 120, "type": "WS2812B", "gamma": 2.2}
  ],
  "pwm": [
    {"ch": 1, "freq_hz": 1000, "curve": "log", "max_duty": 0.85}
  ]
}
```

## REST API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Node health & telemetry |
| `/api/config` | GET/POST | Configuration |
| `/api/presets` | GET/POST | Effect presets |
| `/api/trigger` | POST | One-shot actions |
| `/events` | GET | SSE stream |

## Effects

Built-in effects (see `components/led_effects/`):
1. **Solid** — Uniform color
2. **Gradient** — Color interpolation
3. **Chase** — Moving pixel with trail
4. **Twinkle** — Randomized sparkle

## Documentation

- [ARCHITECTURE.md](Docs/ARCHITECTURE.md) — System design
- [AGENT_TASKS.md](Docs/AGENT_TASKS.md) — Development guide
- [api_spec.yaml](Docs/api_spec.yaml) — OpenAPI specification
- [PROJECT_REPORT.md](PROJECT_REPORT.md) — Full implementation report

## Development

### Add New Effect
1. Edit `components/led_effects/effects.c`
2. Add `fx_<name>_init()` and `fx_<name>_render()`
3. Register in `EFFECTS[]` array

### Testing
```bash
# Unit tests (planned)
idf.py build test

# Monitor logs
idf.py monitor
```

## Performance Targets

- Effect loop: ≤ 3 ms (8×180px @ 60fps)
- Free heap: ≥ 80 KB during playback
- Sync drift: < 5 ms over 60s

## Roadmap

- [x] Core architecture
- [x] PCA9685 driver
- [x] Effect engine
- [ ] RMT addressable driver
- [ ] Wi-Fi manager
- [ ] REST API handlers
- [ ] MQTT client
- [ ] UDP sync protocol
- [ ] OTA updates

## License

TBD

## Support

- **Repository:** https://github.com/algorhythmicdev/lumigrid
- **Issues:** https://github.com/algorhythmicdev/lumigrid/issues

---

**Built with ❤️ for the LED art community**

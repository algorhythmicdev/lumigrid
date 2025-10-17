# Agent Playbook — LumiGrid LED Node (Gemini Flash friendly)

Follow this exact order. Keep outputs small, modular, and compile often.

## 0) Environment
- ESP-IDF v5.x, Release `-Os`
- LittleFS partition for `/spiffs/www`

## 1) Skeleton
- Create directories exactly as in `ARCHITECTURE.md`
- Copy `board_pinmap.h` and `default_config.json` to `main/config/`

**Stop & Check:** `idf.py build` must succeed.

## 2) PCA9685
- Implement driver with init, set duty, fade, all_off
- I²C @ 400 kHz; prescale ~1 kHz

**Stop & Check:** Unit test toggles channels 0..7.

## 3) Addressable Engine
- Allocate per-channel frame buffers
- Implement RMT WS2812B/SK6812 writers
- Expose `aled_set_effect(ch, params)`

**Stop & Check:** Solid red on ALEDch1..8.

## 4) Effects
- Add registry from `components_led_effects_effects.*`
- Implement engine loop; support base + overlay

**Stop & Check:** Gradient, chase, twinkle animate correctly.

## 5) REST
- Implement endpoints in `api_spec.yaml`
- `/api/status` returns fps, pwm max, last_error

**Stop & Check:** `curl` returns valid JSON.

## 6) UI + SSE
- Serve gzipped assets with correct headers
- `/events` SSE emits playhead + telemetry

**Stop & Check:** Browser receives events at 1–10 Hz.

## 7) Sync
- UDP multicast tick/cue; Master/Slave roles
- Clock discipline via EWMA

**Stop & Check:** < 5 ms drift over 60 s in test.

## 8) Scheduler/Triggers
- RRULE-like windows; trigger overrides bounded

## 9) MQTT
- Sub `.../cmd`, Pub `.../telemetry`

## 10) OTA & Safety
- A/B OTA; watchdog; brownout caps

**Finish:** Ship demo presets + docs.

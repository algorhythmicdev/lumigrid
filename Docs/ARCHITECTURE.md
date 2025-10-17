# LumiGrid LED Node — Firmware Architecture (v1)

**Target MCU:** ESP32-WROOM-32U  
**Purpose:** Control 8 addressable LED channels and 8 PWM channels, act as Master/Slave in Sync mode, host a compact Web UI, and expose REST/MQTT APIs.

---

## 1. Hardware Overview (Binding Summary)
- **Addressable DI (8 ch):** GPIO16, GPIO4, GPIO17, GPIO18, GPIO19, GPIO23, GPIO26, GPIO27 → SN74HCT245 → ALEDch1..8
- **PWM (8 ch):** PCA9685 channels 0..7 → MOSFET Q1..Q8 → LEDch1..8
- **I²C:** SDA=GPIO21, SCL=GPIO22; PCA9685 OE# = GPIO25 (active low)
- **Service:** UART0 TX=GPIO1, RX=GPIO3; BOOT=GPIO0

EXP header PWM (PCA9685 ch 8..15) is **reserved** and not surfaced in firmware/UI for this node type.

---

## 2. Process/Task Model
| Task             | Priority | Purpose                                  |
|------------------|----------|------------------------------------------|
| EffectEngine     | 5        | Addressable effects + frame scheduler    |
| PWMDriver        | 6        | PCA9685 updates + fades                  |
| SyncManager      | 4        | UDP multicast tick/cue, clock discipline |
| RestServer       | 3        | REST API + static UI                     |
| MQTTClient       | 3        | Commands + telemetry                     |
| Scheduler        | 2        | RTC/calendar + triggers                  |
| Diagnostics      | 1        | Sensors, watchdog, brownout, logs        |

**Clocking:** Use esp_timer for periodic jobs. Keep allocations out of hot paths.

---

## 3. Source Tree (Recommended)
```
/firmware
├─ main/
│  ├─ app_main.c
│  ├─ lednode_init.c
│  ├─ tasks/
│  │   ├─ task_effect_engine.c
│  │   ├─ task_pwm_driver.c
│  │   ├─ task_sync_manager.c
│  │   ├─ task_rest_server.c
│  │   ├─ task_ui_fs.c
│  │   └─ task_mqtt_client.c
│  ├─ config/
│  │   ├─ board_pinmap.h
│  │   ├─ default_config.json
│  │   └─ version.h
│  └─ utils/
│      ├─ json_config.c
│      ├─ scheduler.c
│      ├─ trigger_engine.c
│      └─ diagnostics.c
├─ components/
│  ├─ led_effects/            # effect API + registry (see README)
│  ├─ pca9685_driver/         # thin HAL wrapper
│  ├─ sync_protocol/          # UDP tick/cue
│  ├─ mqtt_wrapper/
│  ├─ rest_api/
│  ├─ ui_server/
│  └─ storage_fs/
└─ assets/
   └─ webapp/                 # pre-gzipped Svelte build
```

---

## 4. Initialization Sequence
1. **Board bring-up:** GPIO directions, pull-ups, OE high, I²C @ 400 kHz.  
2. **Load config:** JSON from FS; if missing, seed defaults.  
3. **Start subsystems:** PWMDriver → EffectEngine → SyncManager → REST/MQTT → Scheduler.  
4. **UI mount:** Serve `/` and hashed assets from LittleFS.  
5. **Self-test (optional):** Pulse ALEDch1, fade LEDch1, report to logs.

---

## 5. Addressable Effects (Design)
- Each channel owns a ring buffer of pixels (`rgb/rgbw`).  
- A central **Effect Engine** advances time and calls effect `render(dt, state)` per active clip.  
- **Fairness:** Interleave per-channel RMT writes to avoid long-strip starvation.  
- **Composability:** Support a single “base effect” + one optional overlay with alpha or additive blend.

**Core structs:**
```c
typedef enum { LED_WS2812B, LED_SK6812_RGBW } led_type_t;

typedef struct {
  uint8_t r,g,b,w;
} px_rgba_t;

typedef struct {
  int ch;                 // 0..7
  led_type_t type;
  uint16_t n_pixels;
  float gamma;
  uint8_t max_brightness; // 0..255
  px_rgba_t *framebuf;    // len=n_pixels
} aled_channel_t;

typedef struct {
  // Effect parameters (variant)
  uint32_t effect_id;
  float speed;
  float intensity;
  uint32_t palette_id;
  px_rgba_t color1, color2, color3;
} effect_params_t;
```

**Effect contract:**
```c
typedef void (*fx_render_fn)(aled_channel_t *ch, const effect_params_t *p,
                             uint32_t t_ms, uint32_t t_end_ms);
typedef bool (*fx_init_fn)(aled_channel_t *ch, const effect_params_t *p);

typedef struct {
  uint32_t id;
  const char *name;
  fx_init_fn init;
  fx_render_fn render;
} effect_vtable_t;
```

**Registry & dispatch:**
```c
const effect_vtable_t *fx_lookup(uint32_t id);
void fx_render_channel(int ch, uint32_t t_ms) {
  const effect_vtable_t *fx = fx_lookup(active_clip[ch].effect_id);
  if (fx) fx->render(&aled[ch], &active_clip[ch].params, t_ms, active_clip[ch].t_end);
}
```

---

## 6. PWM Driver (Design)
- **Frequency:** default 1 kHz; per-channel fade curves (linear/log).  
- **OE control:** GPIO25 for instant blackout & mode-switch safety.

**API:**
```c
void pwm_init(void);                          // set prescale for ~1kHz
void pwm_set_duty(uint8_t ch, float duty01);  // 0.0..1.0
void pwm_fade_to(uint8_t ch, float d01, uint32_t ms, bool log_curve);
void pwm_all_off(void);                       // OE low, then restore
```

---

## 7. Sync Protocol (Design)
- **Multicast:** 239.10.7.42:45454  
- **Packets:** `tick` @ 60 Hz and sparse `cue` messages.  
- **Slave discipline:** EWMA on tick deltas; clip start aligns on next audio frame boundary if available; otherwise immediate.

**Examples:**
```json
{"type":"tick","t":123456,"tempo":120}
{"type":"cue","track":"ALEDch3","preset":"ocean","t0":123800,"t1":127000,"params_crc":314159}
```

---

## 8. REST & UI
- Routes in `api_spec.yaml`.  
- Gzipped static assets; cache immutable by hash.  
- `/events` SSE for playhead & telemetry at 1–10 Hz.

---

## 9. Safety & Diagnostics
- **Watchdog:** task-level with soft recovery of EffectEngine.  
- **Faults:** brownout/thermal → PWM OE low + addressable brightness cap.  
- **Telemetry:** fps per ALED channel, PWM max duty, heap stats, last error code.

---

## 10. Memory & Performance Targets
- Free heap during playback ≥ 80 KB.  
- EffectEngine loop ≤ 3 ms for 8×180 px strips @ 60 fps.  
- I²C transactions batched; avoid per-frame config writes.  

---

## 11. Build & OTA
- ESP-IDF v5.x, `-Os` release flags.  
- FS image tool to pack `/assets/webapp` into LittleFS.  
- OTA A/B with SHA256 + signature verification.

---

## 12. Unit & Bench Tests
- Mock PCA9685 I²C with loopback; verify duty cycles & fades.  
- Effect golden-images: CRC of framebuf for given seeds & times.  
- Sync drift test: simulate 1% clock drift; assert < 5 ms slip over 60 s.

---

## 13. Coding Conventions
- Snake_case for JSON, lowerCamel for C fields only where IDF requires.  
- One module = one public header.  
- No blocking I/O in render loop; use queues.

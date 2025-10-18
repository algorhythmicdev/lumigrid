#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { uint8_t r,g,b,w; } px_rgba_t;

typedef enum { LED_WS2812B, LED_SK6812_RGBW } led_type_t;

typedef enum {
  BLEND_NORMAL,
  BLEND_ADD,
  BLEND_SCREEN,
  BLEND_MULTIPLY,
  BLEND_LIGHTEN
} blend_mode_t;

typedef struct {
  int ch;                 // 0..7
  led_type_t type;
  uint16_t n_pixels;
  float gamma;
  uint8_t max_brightness; // 0..255
  px_rgba_t *framebuf;
} aled_channel_t;

typedef struct {
  uint32_t effect_id;
  float    speed;          // cycles/s or px/s (effect-defined)
  float    intensity;      // 0..1 scalar
  uint32_t palette_id;     // 0..N (into fx_palette registry)
  px_rgba_t color1, color2, color3;
  uint32_t seed;           // stable per-instance
  blend_mode_t blend;      // overlay blending
  uint8_t  opacity;        // 0..255 overlay opacity
  uint16_t seg_start;      // virtual segment start within channel
  uint16_t seg_len;        // 0 = full length
} effect_params_t;

typedef uint32_t (*fx_render_fn)(aled_channel_t *ch, const effect_params_t *p,
                                 uint32_t t_ms, uint32_t t_end_ms);
typedef bool (*fx_init_fn)(aled_channel_t *ch, const effect_params_t *p);

typedef struct {
  uint32_t id;
  const char *name;
  fx_init_fn init;
  fx_render_fn render;
} effect_vtable_t;

// Effect IDs
enum {
  FX_SOLID = 1,
  FX_GRADIENT = 2,
  FX_CHASE = 3,
  FX_TWINKLE = 4,
  FX_RAINBOW = 1001,
  FX_NOISE = 1002,
  FX_FIRE = 1003,
  FX_WAVES = 1004
};

const effect_vtable_t* fx_lookup(uint32_t id);
void fx_init_all(void);
void fx_render_channel(int ch, uint32_t t_ms);

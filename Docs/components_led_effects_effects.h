#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { uint8_t r,g,b,w; } px_rgba_t;

typedef enum { LED_WS2812B, LED_SK6812_RGBW } led_type_t;

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
  float speed;
  float intensity;
  uint32_t palette_id;
  px_rgba_t color1, color2, color3;
  uint32_t seed;
} effect_params_t;

typedef void (*fx_render_fn)(aled_channel_t *ch, const effect_params_t *p,
                             uint32_t t_ms, uint32_t t_end_ms);
typedef bool (*fx_init_fn)(aled_channel_t *ch, const effect_params_t *p);

typedef struct {
  uint32_t id;
  const char *name;
  fx_init_fn init;
  fx_render_fn render;
} effect_vtable_t;

const effect_vtable_t* fx_lookup(uint32_t id);
void fx_init_all(void);
void fx_render_channel(int ch, uint32_t t_ms);

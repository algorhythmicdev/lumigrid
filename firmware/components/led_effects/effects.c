#include "effects.h"
#include <math.h>
#include <string.h>

// --- helpers ---
static inline uint8_t clamp8(int v){ return (v<0)?0:((v>255)?255:v); }
static inline float fracf(float x){ return x - floorf(x); }
static inline uint8_t lerp8(uint8_t a, uint8_t b, float t){ return (uint8_t)(a + (b-a)*t); }

// --- registry ---
static bool fx_solid_init(aled_channel_t *ch, const effect_params_t *p){ (void)ch;(void)p; return true; }
static void fx_solid_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_ms;(void)t_end_ms;
  for (int i=0;i<ch->n_pixels;i++) ch->framebuf[i] = p->color1;
}

static bool fx_gradient_init(aled_channel_t *ch, const effect_params_t *p){ (void)ch;(void)p; return true; }
static void fx_gradient_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_ms;(void)t_end_ms;
  for (int i=0;i<ch->n_pixels;i++){
    float t = (float)i/(float)(ch->n_pixels-1);
    px_rgba_t c = {
      .r = lerp8(p->color1.r, p->color2.r, t),
      .g = lerp8(p->color1.g, p->color2.g, t),
      .b = lerp8(p->color1.b, p->color2.b, t),
      .w = lerp8(p->color1.w, p->color2.w, t)
    };
    ch->framebuf[i] = c;
  }
}

static bool fx_chase_init(aled_channel_t *ch, const effect_params_t *p){ (void)ch;(void)p; return true; }
static void fx_chase_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  float speed = (p->speed<=0?60:p->speed); // px/s
  float head = fmodf((t_ms/1000.0f)*speed, (float)ch->n_pixels);
  for (int i=0;i<ch->n_pixels;i++){
    float d = fabsf(i - head);
    float a = fmaxf(0.0f, 1.0f - d/10.0f);
    ch->framebuf[i].r = (uint8_t)(p->color1.r * a);
    ch->framebuf[i].g = (uint8_t)(p->color1.g * a);
    ch->framebuf[i].b = (uint8_t)(p->color1.b * a);
    ch->framebuf[i].w = (uint8_t)(p->color1.w * a);
  }
}

static bool fx_twinkle_init(aled_channel_t *ch, const effect_params_t *p){ (void)p; (void)ch; return true; }
static void fx_twinkle_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  uint32_t t = t_ms + p->seed*977u;
  for (int i=0;i<ch->n_pixels;i++){
    // simple LCG noise
    uint32_t x = (t*1664525u + (i*1013904223u));
    float v = ((x>>8)&0xFFFF)/65535.0f;
    float a = v*v * (p->intensity/255.0f);
    ch->framebuf[i].r = (uint8_t)(p->color1.r * a);
    ch->framebuf[i].g = (uint8_t)(p->color1.g * a);
    ch->framebuf[i].b = (uint8_t)(p->color1.b * a);
    ch->framebuf[i].w = (uint8_t)(p->color1.w * a);
  }
}

static const effect_vtable_t EFFECTS[] = {
  {1, "solid",    fx_solid_init,    fx_solid_render},
  {2, "gradient", fx_gradient_init, fx_gradient_render},
  {3, "chase",    fx_chase_init,    fx_chase_render},
  {4, "twinkle",  fx_twinkle_init,  fx_twinkle_render},
};

const effect_vtable_t* fx_lookup(uint32_t id){
  for (unsigned i=0;i<sizeof(EFFECTS)/sizeof(EFFECTS[0]); ++i)
    if (EFFECTS[i].id == id) return &EFFECTS[i];
  return NULL;
}

void fx_init_all(void){
  (void)EFFECTS; // placeholder for per-channel init if needed
}

void fx_render_channel(int ch_idx, uint32_t t_ms){
  (void)ch_idx; (void)t_ms; // actual dispatch happens in engine using active_clip[]
}

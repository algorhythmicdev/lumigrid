#include "effects.h"
#include "fx_util.h"
#include "fx_palette.h"
#include "fx_segments.h"
#include <math.h>
#include <string.h>

// --- helpers ---
static inline uint8_t clamp8(int v){ return (v<0)?0:((v>255)?255:v); }
static inline float fracf(float x){ return x - floorf(x); }
static inline uint8_t lerp8(uint8_t a, uint8_t b, float t){ return (uint8_t)(a + (b-a)*t); }

extern volatile float g_beat_phase;

// --- Basic Effects (Original) ---
static bool fx_solid_init(aled_channel_t *ch, const effect_params_t *p){ (void)ch;(void)p; return true; }
static uint32_t fx_solid_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_ms;(void)t_end_ms;
  segment_t s = seg_from_params(ch, p);
  uint32_t ma = 0;
  for (int i=0;i<s.len;i++) {
    ch->framebuf[s.start+i] = p->color1;
    ma += p->color1.r + p->color1.g + p->color1.b + p->color1.w;
  }
  return ma;
}

static bool fx_gradient_init(aled_channel_t *ch, const effect_params_t *p){ (void)ch;(void)p; return true; }
static uint32_t fx_gradient_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_ms;(void)t_end_ms;
  segment_t s = seg_from_params(ch, p);
  uint32_t ma = 0;
  for (int i=0;i<s.len;i++){
    float t = s.len > 1 ? (float)i/(float)(s.len-1) : 0.0f;
    px_rgba_t c = {
      .r = lerp8(p->color1.r, p->color2.r, t),
      .g = lerp8(p->color1.g, p->color2.g, t),
      .b = lerp8(p->color1.b, p->color2.b, t),
      .w = lerp8(p->color1.w, p->color2.w, t)
    };
    ch->framebuf[s.start+i] = c;
    ma += c.r + c.g + c.b + c.w;
  }
  return ma;
}

static bool fx_chase_init(aled_channel_t *ch, const effect_params_t *p){ (void)ch;(void)p; return true; }
static uint32_t fx_chase_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_end_ms;
  segment_t s = seg_from_params(ch, p);
  float speed = (p->speed<=0?60:p->speed);
  float head = fmodf((t_ms/1000.0f)*speed, (float)s.len);
  uint32_t ma = 0;
  for (int i=0;i<s.len;i++){
    float d = fabsf(i - head);
    float a = fmaxf(0.0f, 1.0f - d/10.0f);
    ch->framebuf[s.start+i].r = (uint8_t)(p->color1.r * a);
    ch->framebuf[s.start+i].g = (uint8_t)(p->color1.g * a);
    ch->framebuf[s.start+i].b = (uint8_t)(p->color1.b * a);
    ch->framebuf[s.start+i].w = (uint8_t)(p->color1.w * a);
    ma += ch->framebuf[s.start+i].r + ch->framebuf[s.start+i].g + ch->framebuf[s.start+i].b + ch->framebuf[s.start+i].w;
  }
  return ma;
}

static bool fx_twinkle_init(aled_channel_t *ch, const effect_params_t *p){ (void)p; (void)ch; return true; }
static uint32_t fx_twinkle_render(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_end_ms;
  segment_t s = seg_from_params(ch, p);
  uint32_t t = t_ms + p->seed*977u;
  uint32_t ma = 0;
  for (int i=0;i<s.len;i++){
    uint32_t x = (t*1664525u + (i*1013904223u));
    float v = ((x>>8)&0xFFFF)/65535.0f;
    float a = v*v * p->intensity;
    ch->framebuf[s.start+i].r = (uint8_t)(p->color1.r * a);
    ch->framebuf[s.start+i].g = (uint8_t)(p->color1.g * a);
    ch->framebuf[s.start+i].b = (uint8_t)(p->color1.b * a);
    ch->framebuf[s.start+i].w = (uint8_t)(p->color1.w * a);
    ma += ch->framebuf[s.start+i].r + ch->framebuf[s.start+i].g + ch->framebuf[s.start+i].b + ch->framebuf[s.start+i].w;
  }
  return ma;
}

// --- Advanced Effects (New) ---

// Rainbow with palette support
static uint32_t fx_rainbow(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_end_ms;
  const palette_t* pal = palette_by_id(p->palette_id);
  segment_t s = seg_from_params(ch,p);
  float spd = p->speed>0 ? p->speed : 0.2f;
  float t = (t_ms/1000.0f) * spd;
  uint32_t ma=0;
  for(int i=0;i<s.len;i++){
    float u = fmodf((i/(float)s.len) + t, 1.0f);
    rgb8_t c = palette_sample(pal, u);
    px_rgba_t px = {c.r, c.g, c.b, 0};
    px.r = util_gamma_u8(dither_ordered(px.r, i, ch->ch, t_ms));
    px.g = util_gamma_u8(dither_ordered(px.g, i, ch->ch, t_ms));
    px.b = util_gamma_u8(dither_ordered(px.b, i, ch->ch, t_ms));
    ch->framebuf[s.start+i] = px;
    ma += px.r + px.g + px.b;
  }
  return ma;
}

// Noise flow effect
static float hash11(float p){ p = sinf(p*127.1f)*43758.5453f; return p - floorf(p); }
static float noise1(float x){ float i=floorf(x), f=x-i; float a=hash11(i), b=hash11(i+1.0f); return a + (b-a)*(f*f*(3-2*f)); }

static uint32_t fx_noise(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_end_ms;
  segment_t s = seg_from_params(ch,p);
  float spd = p->speed>0?p->speed:1.2f;
  float t = (t_ms/1000.0f)*spd + p->seed*0.1f;
  float inten = (p->intensity>0.f)?p->intensity:1.f;
  uint32_t ma=0;
  for(int i=0;i<s.len;i++){
    float n = noise1(i*0.08f + t);
    float v = powf(n, 1.5f) * inten;
    px_rgba_t px = { (uint8_t)(p->color1.r*v), (uint8_t)(p->color1.g*v), (uint8_t)(p->color1.b*v), 0};
    ch->framebuf[s.start+i] = px;
    ma += px.r + px.g + px.b;
  }
  return ma;
}

// Fire effect
static uint32_t fx_fire(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_end_ms;
  segment_t s = seg_from_params(ch,p);
  float inten = (p->intensity>0.f)?p->intensity:1.f;
  uint32_t ma=0;
  for(int i=0;i<s.len;i++){
    float y = (float)i/s.len;
    float flick = noise1((i*0.15f) + (t_ms*0.006f) + p->seed)*0.7f + 0.3f;
    float heat = powf(1.0f - y, 2.0f) * flick * inten;
    px_rgba_t px = hsv_to_rgbw(0.08f + 0.05f*(1.0f-heat), 1.0f, heat, (ch->type==LED_SK6812_RGBW));
    ch->framebuf[s.start+i] = px;
    ma += px.r+px.g+px.b+px.w;
  }
  return ma;
}

// Waves effect (beat-sync ready)
static uint32_t fx_waves(aled_channel_t *ch, const effect_params_t *p, uint32_t t_ms, uint32_t t_end_ms){
  (void)t_end_ms;
  segment_t s = seg_from_params(ch,p);
  float t = t_ms/1000.0f;
  float inten = (p->intensity>0.f)?p->intensity:1.f;
  uint32_t ma=0;
  for(int i=0;i<s.len;i++){
    float x = (float)i/s.len;
    float w = 0.5f + 0.5f*sinf( (x*6.283f*(1.5f+inten*2.f)) + (t*2.0f) + g_beat_phase*3.1415f );
    px_rgba_t a=p->color1, b=p->color2;
    px_rgba_t px = { (uint8_t)(a.r + (b.r-a.r)*w),
                     (uint8_t)(a.g + (b.g-a.g)*w),
                     (uint8_t)(a.b + (b.b-a.b)*w), 0 };
    ch->framebuf[s.start+i] = px;
    ma += px.r+px.g+px.b;
  }
  return ma;
}

static const effect_vtable_t EFFECTS[] = {
  {FX_SOLID,    "solid",    fx_solid_init,    fx_solid_render},
  {FX_GRADIENT, "gradient", fx_gradient_init, fx_gradient_render},
  {FX_CHASE,    "chase",    fx_chase_init,    fx_chase_render},
  {FX_TWINKLE,  "twinkle",  fx_twinkle_init,  fx_twinkle_render},
  {FX_RAINBOW,  "rainbow",  NULL,             fx_rainbow},
  {FX_NOISE,    "noise",    NULL,             fx_noise},
  {FX_FIRE,     "fire",     NULL,             fx_fire},
  {FX_WAVES,    "waves",    NULL,             fx_waves},
};

const effect_vtable_t* fx_lookup(uint32_t id){
  for (unsigned i=0;i<sizeof(EFFECTS)/sizeof(EFFECTS[0]); ++i)
    if (EFFECTS[i].id == id) return &EFFECTS[i];
  return NULL;
}

void fx_init_all(void){
  util_init_gamma(2.2f);
  (void)EFFECTS;
}

void fx_render_channel(int ch_idx, uint32_t t_ms){
  (void)ch_idx; (void)t_ms; // actual dispatch happens in engine using active_clip[]
}

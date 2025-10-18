#include "fx_palette.h"
#include <math.h>
#include <stdint.h>

#define PAL_CNT(arr) (sizeof(arr)/sizeof((arr)[0]))

static const rgb8_t PAL_OCEAN[]  = {{0,64,128},{0,160,255},{0,64,128}};
static const rgb8_t PAL_SUNSET[] = {{255,80,0},{255,0,64},{64,0,96}};
static const palette_t REGS[] = {
  {"ocean",  PAL_OCEAN,  PAL_CNT(PAL_OCEAN)},
  {"sunset", PAL_SUNSET, PAL_CNT(PAL_SUNSET)}
};

const palette_t* palette_by_id(uint32_t id){
  if (id < PAL_CNT(REGS)){
    return &REGS[id];
  }
  return &REGS[0];
}

rgb8_t palette_sample(const palette_t* p, float t){
  if (!p || p->count == 0){
    return (rgb8_t){255,255,255};
  }
  if (t < 0.f) t = 0.f;
  if (t > 1.f) t = 1.f;
  float x=t*(p->count-1); int i=(int)floorf(x); float u=x-i;
  rgb8_t a=p->keys[i], b=p->keys[i<(p->count-1)?i+1:i];
  return (rgb8_t){
    (uint8_t)(a.r + (b.r-a.r)*u),
    (uint8_t)(a.g + (b.g-a.g)*u),
    (uint8_t)(a.b + (b.b-a.b)*u)
  };
}

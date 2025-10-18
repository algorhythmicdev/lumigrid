#pragma once
#include "effects.h"
static inline uint8_t clamp8i(int v){ return v<0?0:(v>255?255:v); }

static inline px_rgba_t blend_apply(blend_mode_t m, px_rgba_t base, px_rgba_t over){
  px_rgba_t o=base;
  switch(m){
    case BLEND_ADD:      o.r=clamp8i(base.r+over.r); o.g=clamp8i(base.g+over.g); o.b=clamp8i(base.b+over.b); o.w=clamp8i(base.w+over.w); break;
    case BLEND_SCREEN:   o.r=255-((255-base.r)*(255-over.r)/255);
                         o.g=255-((255-base.g)*(255-over.g)/255);
                         o.b=255-((255-base.b)*(255-over.b)/255);
                         o.w=clamp8i(base.w+over.w); break;
    case BLEND_MULTIPLY: o.r=(base.r*over.r)/255; o.g=(base.g*over.g)/255; o.b=(base.b*over.b)/255; o.w=clamp8i(base.w+over.w); break;
    case BLEND_LIGHTEN:  o.r=base.r>over.r?base.r:over.r; o.g=base.g>over.g?base.g:over.g; o.b=base.b>over.b?base.b:over.b; o.w=clamp8i(base.w+over.w); break;
    default:             o=over; break;
  } return o;
}

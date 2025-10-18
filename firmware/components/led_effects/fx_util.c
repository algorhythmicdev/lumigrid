#include "fx_util.h"
#include <math.h>

static uint8_t G[256];
void util_init_gamma(float g){ for(int i=0;i<256;i++){ G[i]=(uint8_t)(powf(i/255.f,g)*255.f+.5f);} }
uint8_t util_gamma_u8(uint8_t v){ return G[v]; }

static const uint8_t B4[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
uint8_t dither_ordered(uint8_t v, uint16_t x, uint16_t y, uint32_t t){
  uint8_t b=B4[y&3][x&3], j=(t>>4)&3; int out=v+((b+j)>8); return out<0?0:(out>255?255:out);
}

px_rgba_t hsv_to_rgbw(float h,float s,float v,int rgbw){
  h=fmodf(h,1.f); if(h<0) h+=1.f;
  float r,g,b; float i=floorf(h*6), f=h*6-i;
  float p=v*(1-s), q=v*(1-f*s), t=v*(1-(1-f)*s);
  switch((int)i%6){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t;break;
  case 3:r=p;g=q;b=v;break;case 4:r=t;g=p;b=v;break;default:r=v;g=p;b=q;break;}
  px_rgba_t c={(uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255),0};
  if(rgbw){ uint8_t w=(uint8_t)fminf(fminf(c.r,c.g),c.b); c.r-=w; c.g-=w; c.b-=w; c.w=w; }
  return c;
}

px_rgba_t rgb_to_rgbw(px_rgba_t in, float wmix){
  if (wmix <= 0.f){
    in.w = 0;
    return in;
  }
  if (wmix > 1.f){
    wmix = 1.f;
  }
  uint8_t base_w = (uint8_t)fminf(fminf(in.r, in.g), in.b);
  uint8_t w = (uint8_t)(base_w * wmix);
  in.r -= w;
  in.g -= w;
  in.b -= w;
  in.w = w;
  return in;
}

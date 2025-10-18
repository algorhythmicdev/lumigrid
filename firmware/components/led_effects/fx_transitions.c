#include <math.h>
#include "fx_transitions.h"

void xfade_begin(xfade_t* x, uint32_t now, uint32_t ms){ x->t0=now; x->t1=now+ms; x->active=ms>0; }
float xfade_mix(const xfade_t* x, uint32_t now){
  if(!x->active || now>=x->t1) return 1.f;
  if(now<=x->t0) return 0.f;
  float u=(now-x->t0)/(float)(x->t1-x->t0); // 0..1
  // smootherstep
  return u*u*(3-2*u);
}
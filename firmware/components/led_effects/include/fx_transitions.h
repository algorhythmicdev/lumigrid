#pragma once
#include "effects.h"
typedef struct { uint32_t t0,t1; uint8_t active; } xfade_t;
void xfade_begin(xfade_t* x, uint32_t now, uint32_t ms);
float xfade_mix(const xfade_t* x, uint32_t now);
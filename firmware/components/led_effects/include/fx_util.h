#pragma once
#include <stdint.h>
#include "effects.h"

void util_init_gamma(float gamma);
uint8_t util_gamma_u8(uint8_t v);
px_rgba_t hsv_to_rgbw(float h, float s, float v, int rgbw);
px_rgba_t rgb_to_rgbw(px_rgba_t in, float wmix);
uint8_t dither_ordered(uint8_t v8, uint16_t x, uint16_t y, uint32_t t_ms);

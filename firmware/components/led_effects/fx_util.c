#include "fx_util.h"
#include <math.h>

static uint8_t G[256];

void util_init_gamma(float gamma) {
    for (int i = 0; i < 256; i++) {
        G[i] = (uint8_t)(powf(i / 255.0f, gamma) * 255.0f + 0.5f);
    }
}

uint8_t util_gamma_u8(uint8_t v) {
    return G[v];
}

px_rgba_t hsv_to_rgbw(float h, float s, float v, int rgbw) {
    h = fmodf(h, 1.0f);
    if (h < 0) h += 1.0f;
    
    float r, g, b;
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    
    switch ((int)i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    
    px_rgba_t c = {
        (uint8_t)(r * 255),
        (uint8_t)(g * 255),
        (uint8_t)(b * 255),
        0
    };
    
    if (rgbw) {
        uint8_t w = c.r < c.g ? (c.r < c.b ? c.r : c.b) : (c.g < c.b ? c.g : c.b);
        c.r -= w;
        c.g -= w;
        c.b -= w;
        c.w = w;
    }
    
    return c;
}

px_rgba_t rgb_to_rgbw(px_rgba_t in, float wmix) {
    uint8_t w = in.r < in.g ? (in.r < in.b ? in.r : in.b) : (in.g < in.b ? in.g : in.b);
    uint8_t ws = (uint8_t)(w * wmix);
    in.r -= ws;
    in.g -= ws;
    in.b -= ws;
    in.w = ws;
    return in;
}

static const uint8_t B4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5}
};

uint8_t dither_ordered(uint8_t v8, uint16_t x, uint16_t y, uint32_t t_ms) {
    uint8_t b = B4[y & 3][x & 3];
    uint8_t jitter = (t_ms >> 4) & 3;
    int val = v8 + ((b + jitter) > 8 ? 1 : 0);
    return (uint8_t)(val > 255 ? 255 : (val < 0 ? 0 : val));
}

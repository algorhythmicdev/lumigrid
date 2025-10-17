#pragma once
#include "effects.h"

static inline uint8_t clamp8i(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static inline px_rgba_t blend_apply(blend_mode_t mode, px_rgba_t a, px_rgba_t b) {
    px_rgba_t o = a;
    
    switch (mode) {
        case BLEND_ADD:
            o.r = clamp8i(a.r + b.r);
            o.g = clamp8i(a.g + b.g);
            o.b = clamp8i(a.b + b.b);
            o.w = clamp8i(a.w + b.w);
            break;
            
        case BLEND_SCREEN:
            o.r = 255 - ((255 - a.r) * (255 - b.r) / 255);
            o.g = 255 - ((255 - a.g) * (255 - b.g) / 255);
            o.b = 255 - ((255 - a.b) * (255 - b.b) / 255);
            o.w = clamp8i(a.w + b.w);
            break;
            
        case BLEND_MULTIPLY:
            o.r = (a.r * b.r) / 255;
            o.g = (a.g * b.g) / 255;
            o.b = (a.b * b.b) / 255;
            o.w = clamp8i(a.w + b.w);
            break;
            
        case BLEND_LIGHTEN:
            o.r = a.r > b.r ? a.r : b.r;
            o.g = a.g > b.g ? a.g : b.g;
            o.b = a.b > b.b ? a.b : b.b;
            o.w = clamp8i(a.w + b.w);
            break;
            
        default: // BLEND_NORMAL
            o = b;
            break;
    }
    
    return o;
}

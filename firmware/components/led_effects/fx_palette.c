#include "fx_palette.h"
#include <math.h>

static const rgb8_t PAL_OCEAN[] = {
    {0, 64, 128},
    {0, 160, 255},
    {0, 64, 128}
};

static const rgb8_t PAL_SUNSET[] = {
    {255, 80, 0},
    {255, 0, 64},
    {64, 0, 96}
};

static const rgb8_t PAL_RAINBOW[] = {
    {255, 0, 0},
    {255, 127, 0},
    {255, 255, 0},
    {0, 255, 0},
    {0, 0, 255},
    {75, 0, 130},
    {148, 0, 211}
};

static const palette_t PALETTES[] = {
    {"ocean", PAL_OCEAN, sizeof(PAL_OCEAN) / sizeof(rgb8_t)},
    {"sunset", PAL_SUNSET, sizeof(PAL_SUNSET) / sizeof(rgb8_t)},
    {"rainbow", PAL_RAINBOW, sizeof(PAL_RAINBOW) / sizeof(rgb8_t)},
};

const palette_t* palette_by_id(uint32_t id) {
    if (id < sizeof(PALETTES) / sizeof(PALETTES[0])) {
        return &PALETTES[id];
    }
    return &PALETTES[0];
}

rgb8_t palette_sample(const palette_t* p, float t) {
    if (p->count == 0) {
        return (rgb8_t){255, 255, 255};
    }
    
    float x = t * (p->count - 1);
    int i = (int)floorf(x);
    float u = x - i;
    
    if (i >= p->count - 1) {
        return p->keys[p->count - 1];
    }
    
    rgb8_t a = p->keys[i];
    rgb8_t b = p->keys[i + 1];
    
    return (rgb8_t){
        (uint8_t)(a.r + (b.r - a.r) * u),
        (uint8_t)(a.g + (b.g - a.g) * u),
        (uint8_t)(a.b + (b.b - a.b) * u)
    };
}

#pragma once
#include <stdint.h>
typedef struct { uint8_t r,g,b; } rgb8_t;
typedef struct { const char* name; const rgb8_t* keys; uint8_t count; } palette_t;
const palette_t* palette_by_id(uint32_t id);
rgb8_t palette_sample(const palette_t* p, float t);

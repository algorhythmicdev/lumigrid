#pragma once
#include "effects.h"

typedef struct {
    uint16_t start;
    uint16_t len;
} segment_t;

static inline segment_t seg_from_params(const aled_channel_t* ch, const effect_params_t* p) {
    segment_t s = {
        .start = p->seg_start,
        .len = p->seg_len ? p->seg_len : ch->n_pixels
    };
    
    if (s.start >= ch->n_pixels) {
        s.start = 0;
        s.len = ch->n_pixels;
    }
    
    if (s.start + s.len > ch->n_pixels) {
        s.len = ch->n_pixels - s.start;
    }
    
    return s;
}

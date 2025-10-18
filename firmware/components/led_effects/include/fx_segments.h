#pragma once

#include "effects.h"
#include <stdbool.h>

typedef struct {
  uint16_t start;
  uint16_t len;
} segment_t;

segment_t seg_from_params(const aled_channel_t* ch, const effect_params_t* p);
bool      seg_is_full(const aled_channel_t* ch, segment_t s);

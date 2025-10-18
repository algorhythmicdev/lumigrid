#pragma once
#include "effects.h"
typedef struct {
  float per_led_mA;
  float limit_mA;
} power_cfg_t;

void               power_set_cfg(power_cfg_t cfg);
const power_cfg_t* power_get_cfg(void);
float              power_scale_for_frame(px_rgba_t* fb, int n, const power_cfg_t* cfg);

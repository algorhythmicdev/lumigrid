#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "effects.h"

#define EFFECT_ENGINE_CH_MAX 8

typedef struct {
  float power_scale[EFFECT_ENGINE_CH_MAX];
} effect_engine_stats_t;

void task_effect_engine_start(void);
bool effect_engine_set_base(int ch, const effect_params_t *params, uint32_t fade_ms);
bool effect_engine_set_overlay(int ch, const effect_params_t *params);
void effect_engine_clear_overlay(int ch);
void effect_engine_get_stats(effect_engine_stats_t *out);

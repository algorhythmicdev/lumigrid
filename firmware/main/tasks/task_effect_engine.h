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
int  effect_engine_channel_count(void);
bool effect_engine_get_channel_info(int ch, led_type_t *type, color_order_t *order, uint16_t *n_pixels);
bool effect_engine_set_channel_type(int ch, led_type_t type, color_order_t order);

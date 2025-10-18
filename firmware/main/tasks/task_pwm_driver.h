#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rest_api_types.h"

void task_pwm_driver_start(void);
void pwm_set_mode_static(uint8_t ch, float duty);
void pwm_set_mode_breath(uint8_t ch, float min_val, float max_val, float period_ms);
void pwm_set_mode_candle(uint8_t ch, float base, float flicker, uint32_t seed);
void pwm_set_mode_warmdim(uint8_t ch, float duty);

void pwm_groups_init_from_config(void);
void pwm_groups_replace(const pwm_group_t *groups, int count);
int  pwm_groups_count(void);
bool pwm_groups_get(int index, pwm_group_t *out);
void pwm_group_set_rgb(const char* name, float r, float g, float b);
void pwm_group_set_rgbw(const char* name, float r, float g, float b, float w);

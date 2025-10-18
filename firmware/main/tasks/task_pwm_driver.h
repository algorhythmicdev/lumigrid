#pragma once

#include <stdint.h>

void task_pwm_driver_start(void);
void pwm_set_mode_static(uint8_t ch, float duty);
void pwm_set_mode_breath(uint8_t ch, float min_val, float max_val, float period_ms);
void pwm_set_mode_candle(uint8_t ch, float base, float flicker, uint32_t seed);
void pwm_set_mode_warmdim(uint8_t ch, float duty);

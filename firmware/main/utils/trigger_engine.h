#pragma once
#include <stdint.h>

extern volatile float g_beat_phase; // 0..1, set via trigger or detector
void trigger_set_beat(float phase01);
void trigger_strobe(uint32_t ms);
float trigger_strobe_level(uint32_t now_ms);

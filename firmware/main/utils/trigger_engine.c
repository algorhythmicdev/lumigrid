#include "trigger_engine.h"
#include "esp_timer.h"
#include <math.h>

volatile float g_beat_phase = 0.f; // read by fx_waves()
static volatile uint32_t s_strobe_until_ms = 0;

void trigger_set_beat(float phase01){
  float wrapped = fmodf(phase01, 1.f);
  if (wrapped < 0.f){
    wrapped += 1.f;
  }
  g_beat_phase = wrapped;
}

void trigger_strobe(uint32_t ms){
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
  s_strobe_until_ms = now_ms + ms;
}

float trigger_strobe_level(uint32_t now_ms){
  return (now_ms < s_strobe_until_ms) ? 1.f : 0.f;
}

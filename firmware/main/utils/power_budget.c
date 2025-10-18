#include "power_budget.h"
#include <math.h>

unsigned g_power_estimate_mA = 0;
bool g_power_clamped = false;

static power_cfg_t s_cfg = {
    .per_led_mA = 60.0f,
    .limit_mA = 8000.0f
};

void power_set_cfg(power_cfg_t cfg){
    if (cfg.per_led_mA <= 0.f){
        cfg.per_led_mA = s_cfg.per_led_mA;
    }
    if (cfg.limit_mA <= 0.f){
        cfg.limit_mA = s_cfg.limit_mA;
    }
    s_cfg = cfg;
}

const power_cfg_t* power_get_cfg(void){
    return &s_cfg;
}

float power_scale_for_frame(px_rgba_t* fb,int n,const power_cfg_t* c){
  if (!fb || n <= 0){
    return 1.f;
  }
  const power_cfg_t* cfg = c ? c : &s_cfg;
  uint32_t sum = 0;
  for (int i=0;i<n;i++){
    sum += fb[i].r + fb[i].g + fb[i].b + fb[i].w;
  }
  float est_mA = (sum / 255.f) * (cfg->per_led_mA / 4.f);
  g_power_estimate_mA = (unsigned)est_mA;
  if (est_mA <= 0.f){
    g_power_clamped = false;
    return 1.f;
  }
  g_power_clamped = est_mA > cfg->limit_mA;
  return g_power_clamped ? (cfg->limit_mA / est_mA) : 1.f;
}

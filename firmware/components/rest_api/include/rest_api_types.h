#pragma once

#include <stdint.h>

typedef enum {
  PWMG_RGB = 0,
  PWMG_RGBW = 1
} pwm_group_kind_t;

typedef struct {
  char name[24];
  pwm_group_kind_t kind;
  int map_r;
  int map_g;
  int map_b;
  int map_w;
} pwm_group_t;

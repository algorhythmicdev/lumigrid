#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "effects.h"
#include "rest_api_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  bool (*set_base)(int ch, const effect_params_t *params, uint32_t fade_ms);
  bool (*set_overlay)(int ch, const effect_params_t *params);
  void (*clear_overlay)(int ch);
  void (*get_power_scale)(float *out, size_t len);
  int  (*channel_count)(void);
  bool (*get_channel_info)(int ch, led_type_t *type, color_order_t *order, uint16_t *n_pixels);
  bool (*set_channel_type)(int ch, led_type_t type, color_order_t order);
} rest_api_effect_ops_t;

typedef struct {
  void (*mode_static)(uint8_t ch, float duty);
  void (*mode_breath)(uint8_t ch, float min_val, float max_val, float period_ms);
  void (*mode_candle)(uint8_t ch, float base, float flicker, uint32_t seed);
  void (*mode_warmdim)(uint8_t ch, float duty);
  int  (*groups_count)(void);
  bool (*get_group)(int index, pwm_group_t *out);
  void (*replace_groups)(const pwm_group_t *groups, int count);
  void (*group_set_rgb)(const char* name, float r, float g, float b);
  void (*group_set_rgbw)(const char* name, float r, float g, float b, float w);
} rest_api_pwm_ops_t;

typedef struct {
  void (*set_beat)(float phase01);
  void (*strobe)(uint32_t ms);
} rest_api_trigger_ops_t;

esp_err_t rest_api_start(void);
httpd_handle_t rest_api_get_server(void);
void rest_api_init(void);

void rest_api_register_effect_ops(const rest_api_effect_ops_t *ops);
void rest_api_register_pwm_ops(const rest_api_pwm_ops_t *ops);
void rest_api_register_trigger_ops(const rest_api_trigger_ops_t *ops);

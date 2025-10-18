#include "task_effect_engine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "aled_rmt.h"
#include "board_pinmap.h"
#include "effects.h"
#include "fx_blend.h"
#include "fx_transitions.h"
#include "power_budget.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define CH_MAX                 8
#define DEFAULT_PIXELS        120
#define DEFAULT_FRAME_INTERVAL 16U
#define XFADE_COMPLETE_THRESH  0.995f

typedef struct {
  aled_channel_t   led;
  px_rgba_t       *overlay_buf;
  px_rgba_t       *xfade_buf;

  effect_params_t  current;
  bool             current_valid;

  effect_params_t  pending;
  bool             pending_valid;

  effect_params_t  overlay;
  bool             overlay_active;

  xfade_t          xfade;

  uint32_t         t_end_ms;
  uint32_t         next_deadline_ms;
  uint32_t         frame_interval_ms;
  float            last_power_scale;
  bool             rmt_ready;
} channel_ctx_t;

typedef struct {
  effect_params_t current;
  bool            current_valid;
  effect_params_t pending;
  bool            pending_valid;
  effect_params_t overlay;
  bool            overlay_active;
  xfade_t         xfade;
} channel_snapshot_t;

static channel_ctx_t       s_channels[CH_MAX];
static SemaphoreHandle_t   s_state_lock = NULL;
static const char         *TAG = "EFFECT_ENGINE";

static inline size_t frame_bytes(const channel_ctx_t *ctx){
  return ctx->led.n_pixels * sizeof(px_rgba_t);
}

static uint32_t render_into(channel_ctx_t *ctx, const effect_params_t *params,
                            px_rgba_t *dest, uint32_t now_ms, uint32_t t_end_ms){
  if (!params || !params->effect_id){
    memset(dest, 0, frame_bytes(ctx));
    return 0;
  }
  const effect_vtable_t *fx = fx_lookup(params->effect_id);
  if (!fx || !fx->render){
    memset(dest, 0, frame_bytes(ctx));
    return 0;
  }

  px_rgba_t *original = ctx->led.framebuf;
  ctx->led.framebuf = dest;
  memset(dest, 0, frame_bytes(ctx));
  uint32_t sum = fx->render(&ctx->led, params, now_ms, t_end_ms);
  ctx->led.framebuf = original;
  return sum;
}

static void apply_power_scale(channel_ctx_t *ctx, float scale){
  if (!ctx->led.framebuf){
    ctx->last_power_scale = 1.f;
    return;
  }
  if (scale >= 0.999f){
    ctx->last_power_scale = 1.f;
    return;
  }
  for (int i = 0; i < ctx->led.n_pixels; ++i){
    ctx->led.framebuf[i].r = (uint8_t)(ctx->led.framebuf[i].r * scale);
    ctx->led.framebuf[i].g = (uint8_t)(ctx->led.framebuf[i].g * scale);
    ctx->led.framebuf[i].b = (uint8_t)(ctx->led.framebuf[i].b * scale);
    ctx->led.framebuf[i].w = (uint8_t)(ctx->led.framebuf[i].w * scale);
  }
  ctx->last_power_scale = scale;
}

static void snapshot_channel(const channel_ctx_t *ctx, channel_snapshot_t *out){
  out->current        = ctx->current;
  out->current_valid  = ctx->current_valid;
  out->pending        = ctx->pending;
  out->pending_valid  = ctx->pending_valid;
  out->overlay        = ctx->overlay;
  out->overlay_active = ctx->overlay_active;
  out->xfade          = ctx->xfade;
}

static void init_channel(channel_ctx_t *ctx, int idx){
  memset(ctx, 0, sizeof(*ctx));

  ctx->led.ch = idx;
  ctx->led.type = LED_WS2812B;
  ctx->led.n_pixels = DEFAULT_PIXELS;
  ctx->led.gamma = 2.2f;
  ctx->led.max_brightness = 255;
  ctx->frame_interval_ms = DEFAULT_FRAME_INTERVAL;
  ctx->next_deadline_ms = 0;
  ctx->last_power_scale = 1.f;

  size_t bytes = frame_bytes(ctx);
  ctx->led.framebuf = heap_caps_calloc(ctx->led.n_pixels, sizeof(px_rgba_t),
                                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  ctx->overlay_buf = heap_caps_calloc(ctx->led.n_pixels, sizeof(px_rgba_t),
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  ctx->xfade_buf = heap_caps_calloc(ctx->led.n_pixels, sizeof(px_rgba_t),
                                    MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  if (!ctx->led.framebuf || !ctx->overlay_buf || !ctx->xfade_buf){
    ESP_LOGE(TAG, "Channel %d framebuffer allocation failed", idx);
  } else {
    memset(ctx->led.framebuf, 0, bytes);
    memset(ctx->overlay_buf, 0, bytes);
    memset(ctx->xfade_buf, 0, bytes);
  }

  if (idx < (int)(sizeof(ALED_GPIO)/sizeof(ALED_GPIO[0]))){
    ctx->rmt_ready = (aled_rmt_init_chan(idx, ALED_GPIO[idx]) == ESP_OK);
    if (!ctx->rmt_ready){
      ESP_LOGW(TAG, "RMT init failed for channel %d (GPIO %d)", idx, ALED_GPIO[idx]);
    }
  }
}

static void ensure_lock(void){
  if (!s_state_lock){
    s_state_lock = xSemaphoreCreateMutex();
  }
}

bool effect_engine_set_base(int ch, const effect_params_t *params, uint32_t fade_ms){
  if (ch < 0 || ch >= CH_MAX || !params){
    return false;
  }
  ensure_lock();

  effect_params_t sanitized = *params;
  if (sanitized.opacity == 0){
    sanitized.opacity = 255;
  }

  if (xSemaphoreTake(s_state_lock, pdMS_TO_TICKS(50)) != pdTRUE){
    return false;
  }

  channel_ctx_t *ctx = &s_channels[ch];
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);

  if (!ctx->current_valid || fade_ms == 0){
    ctx->current = sanitized;
    ctx->current_valid = true;
    ctx->pending_valid = false;
    ctx->xfade.active = 0;
  } else {
    ctx->pending = sanitized;
    ctx->pending_valid = true;
    xfade_begin(&ctx->xfade, now_ms, fade_ms);
  }

  xSemaphoreGive(s_state_lock);
  return true;
}

bool effect_engine_set_overlay(int ch, const effect_params_t *params){
  if (ch < 0 || ch >= CH_MAX){
    return false;
  }
  ensure_lock();

  if (xSemaphoreTake(s_state_lock, pdMS_TO_TICKS(50)) != pdTRUE){
    return false;
  }

  channel_ctx_t *ctx = &s_channels[ch];
  if (params){
    ctx->overlay = *params;
    if (ctx->overlay.opacity == 0){
      ctx->overlay.opacity = 255;
    }
    ctx->overlay_active = true;
  } else {
    ctx->overlay_active = false;
  }

  xSemaphoreGive(s_state_lock);
  return true;
}

void effect_engine_clear_overlay(int ch){
  effect_engine_set_overlay(ch, NULL);
}

void effect_engine_get_stats(effect_engine_stats_t *out){
  if (!out){
    return;
  }
  ensure_lock();
  if (xSemaphoreTake(s_state_lock, pdMS_TO_TICKS(20)) == pdTRUE){
    for (int ch = 0; ch < CH_MAX; ++ch){
      out->power_scale[ch] = s_channels[ch].last_power_scale;
    }
    xSemaphoreGive(s_state_lock);
  } else {
    memset(out, 0, sizeof(*out));
  }
}

static void render_channel(channel_ctx_t *ctx, uint32_t now_ms){
  channel_snapshot_t snap;

  ensure_lock();
  if (xSemaphoreTake(s_state_lock, portMAX_DELAY) == pdTRUE){
    snapshot_channel(ctx, &snap);
    xSemaphoreGive(s_state_lock);
  } else {
    return;
  }

  if (!ctx->led.framebuf){
    ctx->next_deadline_ms = now_ms + ctx->frame_interval_ms;
    return;
  }

  if (!snap.current_valid || !ctx->led.framebuf){
    if (ctx->led.framebuf){
      memset(ctx->led.framebuf, 0, frame_bytes(ctx));
    }
  } else {
    render_into(ctx, &snap.current, ctx->led.framebuf, now_ms, ctx->t_end_ms);

    if (snap.xfade.active && snap.pending_valid && ctx->xfade_buf){
      render_into(ctx, &snap.pending, ctx->xfade_buf, now_ms, ctx->t_end_ms);
      float mix = xfade_mix(&snap.xfade, now_ms);
      if (mix < 0.f) mix = 0.f;
      if (mix > 1.f) mix = 1.f;
      float inv = 1.f - mix;
      for (int i = 0; i < ctx->led.n_pixels; ++i){
        const px_rgba_t next = ctx->xfade_buf[i];
        px_rgba_t cur = ctx->led.framebuf[i];
        ctx->led.framebuf[i].r = (uint8_t)(cur.r * inv + next.r * mix);
        ctx->led.framebuf[i].g = (uint8_t)(cur.g * inv + next.g * mix);
        ctx->led.framebuf[i].b = (uint8_t)(cur.b * inv + next.b * mix);
        ctx->led.framebuf[i].w = (uint8_t)(cur.w * inv + next.w * mix);
      }
      if (mix >= XFADE_COMPLETE_THRESH){
        ensure_lock();
        if (xSemaphoreTake(s_state_lock, portMAX_DELAY) == pdTRUE){
          ctx->current = snap.pending;
          ctx->current_valid = true;
          ctx->pending_valid = false;
          ctx->xfade.active = 0;
          xSemaphoreGive(s_state_lock);
        }
      }
    }

    if (snap.overlay_active && ctx->overlay_buf && ctx->xfade_buf){
      memcpy(ctx->overlay_buf, ctx->led.framebuf, frame_bytes(ctx));
      render_into(ctx, &snap.overlay, ctx->xfade_buf, now_ms, ctx->t_end_ms);
      for (int i = 0; i < ctx->led.n_pixels; ++i){
        px_rgba_t base = ctx->overlay_buf[i];
        px_rgba_t over = ctx->xfade_buf[i];
        over.r = (uint8_t)((over.r * snap.overlay.opacity) / 255);
        over.g = (uint8_t)((over.g * snap.overlay.opacity) / 255);
        over.b = (uint8_t)((over.b * snap.overlay.opacity) / 255);
        over.w = (uint8_t)((over.w * snap.overlay.opacity) / 255);
        ctx->led.framebuf[i] = blend_apply(snap.overlay.blend, base, over);
      }
    }
  }

  float scale = power_scale_for_frame(ctx->led.framebuf, ctx->led.n_pixels, power_get_cfg());
  apply_power_scale(ctx, scale);

  if (ctx->rmt_ready){
    aled_rmt_write(ctx->led.ch, ctx->led.framebuf, ctx->led.n_pixels, ctx->led.type);
  }

  ctx->next_deadline_ms = now_ms + ctx->frame_interval_ms;
}

static void effect_engine_task(void *arg){
  (void)arg;
  ESP_LOGI(TAG, "Effect engine task running");

  fx_init_all();

  const power_cfg_t *cfg = power_get_cfg();
  if (cfg){
    power_set_cfg(*cfg);
  }

  effect_params_t off = {
    .effect_id = FX_SOLID,
    .intensity = 0.f,
    .blend = BLEND_NORMAL,
    .opacity = 255,
    .color1 = {0,0,0,0},
    .seg_len = 0,
    .seg_start = 0
  };

  for (int ch = 0; ch < CH_MAX; ++ch){
    effect_engine_set_base(ch, &off, 0);
  }

  while (1){
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    for (int ch = 0; ch < CH_MAX; ++ch){
      channel_ctx_t *ctx = &s_channels[ch];
      if (now_ms >= ctx->next_deadline_ms){
        render_channel(ctx, now_ms);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void task_effect_engine_start(void){
  ensure_lock();
  for (int ch = 0; ch < CH_MAX; ++ch){
    init_channel(&s_channels[ch], ch);
  }
  xTaskCreate(effect_engine_task, "effect_engine", 6144, NULL, 5, NULL);
}

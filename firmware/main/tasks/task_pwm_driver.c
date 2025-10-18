#include "task_pwm_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pca9685_driver.h"
#include <math.h>

static const char *TAG = "PWM_DRIVER";

typedef enum {
    PWM_MODE_STATIC,
    PWM_MODE_BREATH,
    PWM_MODE_CANDLE,
    PWM_MODE_WARMDIM
} pwm_mode_t;

typedef struct {
    uint8_t ch;
    pwm_mode_t mode;
    float target;
    float current;
    uint32_t t0;
    uint32_t duration_ms;
    bool log_curve;
    float breath_min;
    float breath_max;
    float breath_period_ms;
    float candle_base;
    float candle_flick;
    uint32_t seed;
} pwm_anim_t;

static pwm_anim_t s_anims[8] = {0};

void pwm_set_mode_static(uint8_t ch, float duty) {
    if (ch >= 8) return;
    s_anims[ch].mode = PWM_MODE_STATIC;
    s_anims[ch].target = duty;
    pca9685_set_duty(ch, duty);
}

void pwm_set_mode_breath(uint8_t ch, float min_val, float max_val, float period_ms) {
    if (ch >= 8) return;
    s_anims[ch].mode = PWM_MODE_BREATH;
    s_anims[ch].breath_min = min_val;
    s_anims[ch].breath_max = max_val;
    s_anims[ch].breath_period_ms = period_ms > 0 ? period_ms : 2000.f;
}

void pwm_set_mode_candle(uint8_t ch, float base, float flicker, uint32_t seed) {
    if (ch >= 8) return;
   s_anims[ch].mode = PWM_MODE_CANDLE;
   s_anims[ch].candle_base = base;
   s_anims[ch].candle_flick = flicker;
   s_anims[ch].seed = seed;
}

void pwm_set_mode_warmdim(uint8_t ch, float duty) {
    if (ch >= 8) return;
    s_anims[ch].mode = PWM_MODE_WARMDIM;
    s_anims[ch].target = duty;
    float clamped = duty;
    if (clamped < 0.f) clamped = 0.f;
    if (clamped > 1.f) clamped = 1.f;
    pca9685_set_duty(ch, powf(clamped, 2.0f));
}

static void pwm_update_channel(pwm_anim_t* anim, uint32_t t_ms) {
    float duty = 0.0f;
    
    switch (anim->mode) {
        case PWM_MODE_BREATH: {
            float period = anim->breath_period_ms > 10.f ? anim->breath_period_ms : 1000.f;
            float phase = fmodf((float)t_ms / period, 1.0f);
            float s = 0.5f - 0.5f * cosf(phase * 6.2831853f);
            duty = anim->breath_min + (anim->breath_max - anim->breath_min) * s;
            break;
        }

        case PWM_MODE_CANDLE: {
            uint32_t x = (t_ms * 1664525u + anim->seed * 1013904223u);
            float n = ((x >> 8) & 0xFFFF) / 65535.0f;
            duty = anim->candle_base + (n - 0.5f) * anim->candle_flick;
            if (duty < 0) duty = 0;
            if (duty > 1) duty = 1;
            break;
        }

        case PWM_MODE_WARMDIM: {
            float clamped = anim->target;
            if (clamped < 0.f) clamped = 0.f;
            if (clamped > 1.f) clamped = 1.f;
            duty = powf(clamped, 2.0f);
            break;
        }

        case PWM_MODE_STATIC:
        default:
            duty = anim->target;
            break;
    }
    
    pca9685_set_duty(anim->ch, duty);
}

static void pwm_driver_task(void *arg) {
    ESP_LOGI(TAG, "PWM driver task started");
    
    for (int i = 0; i < 8; i++) {
        s_anims[i].ch = i;
        s_anims[i].mode = PWM_MODE_STATIC;
        s_anims[i].target = 0.0f;
    }
    
    while (1) {
        uint32_t t_ms = (uint32_t)(esp_timer_get_time() / 1000);
        
        for (int i = 0; i < 8; i++) {
            if (s_anims[i].mode != PWM_MODE_STATIC) {
                pwm_update_channel(&s_anims[i], t_ms);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void task_pwm_driver_start(void) {
    xTaskCreate(pwm_driver_task, "pwm_drv", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "PWM driver task spawned");
}

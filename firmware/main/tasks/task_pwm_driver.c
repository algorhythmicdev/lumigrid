#include "task_pwm_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "pca9685_driver.h"
#include "cJSON.h"
#include "config_store.h"
#include "freertos/semphr.h"
#include "lednode_init.h"
#include <math.h>
#include <string.h>

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

static pwm_anim_t s_anims[CONFIG_PWM_COUNT] = {0};
static pwm_group_t s_groups[CONFIG_PWM_COUNT];
static int s_groups_len = 0;
static SemaphoreHandle_t s_anim_mtx;

static inline void set_channel(int ch, float value){
    if (ch >= 0 && ch < pwm_count()){
        if (value < 0.f) value = 0.f;
        if (value > 1.f) value = 1.f;
        pca9685_set_duty((uint8_t)ch, value);
    }
}

void pwm_set_mode_static(uint8_t ch, float duty) {
    if (ch >= pwm_count()) return;
    xSemaphoreTake(s_anim_mtx, portMAX_DELAY);
    s_anims[ch].mode = PWM_MODE_STATIC;
    s_anims[ch].target = duty;
    xSemaphoreGive(s_anim_mtx);
    pca9685_set_duty(ch, duty);
}

void pwm_set_mode_breath(uint8_t ch, float min_val, float max_val, float period_ms) {
    if (ch >= pwm_count()) return;
    xSemaphoreTake(s_anim_mtx, portMAX_DELAY);
    s_anims[ch].mode = PWM_MODE_BREATH;
    s_anims[ch].breath_min = min_val;
    s_anims[ch].breath_max = max_val;
    s_anims[ch].breath_period_ms = period_ms > 0 ? period_ms : 2000.f;
    xSemaphoreGive(s_anim_mtx);
}

void pwm_set_mode_candle(uint8_t ch, float base, float flicker, uint32_t seed) {
    if (ch >= pwm_count()) return;
    xSemaphoreTake(s_anim_mtx, portMAX_DELAY);
    s_anims[ch].mode = PWM_MODE_CANDLE;
    s_anims[ch].candle_base = base;
    s_anims[ch].candle_flick = flicker;
    s_anims[ch].seed = seed;
    xSemaphoreGive(s_anim_mtx);
}

void pwm_set_mode_warmdim(uint8_t ch, float duty) {
    if (ch >= pwm_count()) return;
    xSemaphoreTake(s_anim_mtx, portMAX_DELAY);
    s_anims[ch].mode = PWM_MODE_WARMDIM;
    s_anims[ch].target = duty;
    xSemaphoreGive(s_anim_mtx);
    float clamped = duty;
    if (clamped < 0.f) clamped = 0.f;
    if (clamped > 1.f) clamped = 1.f;
    pca9685_set_duty(ch, powf(clamped, 2.0f));
}

void pwm_driver_init_mutex(void){
  if(!s_anim_mtx) s_anim_mtx = xSemaphoreCreateMutex();
}

void pwm_groups_init_from_config(void){
  s_groups_len = 0;
  const cJSON* root = config_root(); if(!root) return;
  const cJSON* arr = cJSON_GetObjectItem(root, "pwm_groups"); if(!arr || !cJSON_IsArray(arr)) return;
  cJSON* it=NULL; cJSON_ArrayForEach(it, arr){
    if(s_groups_len >= pwm_count()) break;
    pwm_group_t* g = &s_groups[s_groups_len++];
    memset(g,0,sizeof(*g));
    snprintf(g->name, sizeof(g->name), "%s", cJSON_GetObjectItem(it,"name")->valuestring);
    const char* kind = cJSON_GetObjectItem(it,"kind")->valuestring;
    g->kind = (strcmp(kind,"RGBW")==0)? PWMG_RGBW : PWMG_RGB;
    const cJSON* map = cJSON_GetObjectItem(it,"map");
    g->map_r = cJSON_GetObjectItem(map,"R")->valueint;
    g->map_g = cJSON_GetObjectItem(map,"G")->valueint;
    g->map_b = cJSON_GetObjectItem(map,"B")->valueint;
    g->map_w = cJSON_HasObjectItem(map,"W")? cJSON_GetObjectItem(map,"W")->valueint : -1;
  }
}

void pwm_groups_replace(const pwm_group_t *groups, int count){
    if (!groups || count <= 0){
        s_groups_len = 0;
        return;
    }
    if (count > pwm_count()){
        count = pwm_count();
    }
    for (int i = 0; i < count; ++i){
        s_groups[i] = groups[i];
        if (s_groups[i].map_r < 0 || s_groups[i].map_r >= pwm_count()) s_groups[i].map_r = -1;
        if (s_groups[i].map_g < 0 || s_groups[i].map_g >= pwm_count()) s_groups[i].map_g = -1;
        if (s_groups[i].map_b < 0 || s_groups[i].map_b >= pwm_count()) s_groups[i].map_b = -1;
        if (s_groups[i].map_w < 0 || s_groups[i].map_w >= pwm_count()) s_groups[i].map_w = -1;
    }
    s_groups_len = count;
}

int pwm_groups_count(void){
    return s_groups_len;
}

const pwm_group_t* pwm_groups_get(int i){
    return (i>=0 && i<s_groups_len)? &s_groups[i] : NULL;
}

static inline void set_ch(int ch, float v){ if(ch>=0) pca9685_set_duty(ch, v); }

void pwm_group_set_rgb(const char* name, float r,float g,float b){
  for(int i=0;i<s_groups_len;i++) if(strcmp(s_groups[i].name,name)==0){
    set_ch(s_groups[i].map_r, r); set_ch(s_groups[i].map_g, g); set_ch(s_groups[i].map_b, b); return;
  }
}
void pwm_group_set_rgbw(const char* name,float r,float g,float b,float w){
  for(int i=0;i<s_groups_len;i++) if(strcmp(s_groups[i].name,name)==0){
    set_ch(s_groups[i].map_r, r); set_ch(s_groups[i].map_g, g); set_ch(s_groups[i].map_b, b); set_ch(s_groups[i].map_w, w); return;
  }
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
    
    xSemaphoreTake(s_anim_mtx, portMAX_DELAY);
    for (int i = 0; i < pwm_count(); i++) {
        s_anims[i].ch = i;
        s_anims[i].mode = PWM_MODE_STATIC;
        s_anims[i].target = 0.0f;
    }
    xSemaphoreGive(s_anim_mtx);
    
    while (1) {
        uint32_t t_ms = (uint32_t)(esp_timer_get_time() / 1000);
        
        xSemaphoreTake(s_anim_mtx, portMAX_DELAY);
        for (int i = 0; i < pwm_count(); i++) {
            if (s_anims[i].mode != PWM_MODE_STATIC) {
                pwm_update_channel(&s_anims[i], t_ms);
            }
        }
        xSemaphoreGive(s_anim_mtx);
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void task_pwm_driver_start(void) {
    pwm_driver_init_mutex();
    pwm_groups_init_from_config();
    xTaskCreate(pwm_driver_task, "pwm_drv", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "PWM driver task spawned");
}

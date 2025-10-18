#include "rest_api.h"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PRESET_DIR        "/spiffs/presets"
#define MAX_BODY_LENGTH   4096
#define MAX_NAME_LENGTH   48
#define SSE_INTERVAL_MS   250

#define EFFECT_CHANNELS   8

static rest_api_effect_ops_t  s_effect_ops   = {0};
static rest_api_pwm_ops_t     s_pwm_ops      = {0};
static rest_api_trigger_ops_t s_trigger_ops  = {0};

static const char *TAG = "REST_API";
static httpd_handle_t s_server = NULL;

static const char* strip_type_to_string(led_type_t type){
  switch (type){
    case LED_SK6812_RGBW: return "SK6812_RGBW";
    case LED_WS2812B:
    default: return "WS2812B";
  }
}

static const char* order_to_string(color_order_t order){
  switch (order){
    case ORDER_RGB:   return "RGB";
    case ORDER_GRB:   return "GRB";
    case ORDER_RGBW:  return "RGBW";
    case ORDER_GRBW:  return "GRBW";
    default:          return "GRB";
  }
}

static bool string_to_strip_type(const char *str, led_type_t *out){
  if (!str || !out){
    return false;
  }
  if (strcasecmp(str, "SK6812_RGBW") == 0){
    *out = LED_SK6812_RGBW;
    return true;
  }
  if (strcasecmp(str, "WS2812B") == 0){
    *out = LED_WS2812B;
    return true;
  }
  return false;
}

static bool string_to_order(const char *str, color_order_t *out){
  if (!str || !out){
    return false;
  }
  if (strcasecmp(str, "RGB") == 0){
    *out = ORDER_RGB;
    return true;
  }
  if (strcasecmp(str, "GRB") == 0){
    *out = ORDER_GRB;
    return true;
  }
  if (strcasecmp(str, "RGBW") == 0){
    *out = ORDER_RGBW;
    return true;
  }
  if (strcasecmp(str, "GRBW") == 0){
    *out = ORDER_GRBW;
    return true;
  }
  return false;
}

static esp_err_t json_reply(httpd_req_t *req, cJSON *root);

static void preset_path(const char *name, char *out, size_t out_len){
  snprintf(out, out_len, PRESET_DIR "/%s.json", name);
}

static bool ensure_dir(const char *path){
  struct stat st;
  if (stat(path, &st) == 0){
    return S_ISDIR(st.st_mode);
  }
  if (mkdir(path, 0755) == 0){
    return true;
  }
  if (errno == EEXIST){
    return true;
  }
  ESP_LOGE(TAG, "mkdir(%s) failed: %d", path, errno);
  return false;
}

static bool is_safe_token(const char *name){
  if (!name || !name[0]){
    return false;
  }
  for (const char *p = name; *p; ++p){
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-' )){
      return false;
    }
  }
  return strlen(name) < MAX_NAME_LENGTH;
}

static bool read_body(httpd_req_t *req, char *buf, size_t buf_len, size_t *out_len){
  size_t total = 0;
  int remaining = req->content_len;

  while (remaining > 0){
    if (total >= buf_len - 1){
      return false;
    }
    int to_read = remaining;
    if ((size_t)to_read > buf_len - 1 - total){
      to_read = (int)(buf_len - 1 - total);
    }
    int r = httpd_req_recv(req, buf + total, to_read);
    if (r <= 0){
      if (r == HTTPD_SOCK_ERR_TIMEOUT){
        continue;
      }
      return false;
    }
    total += r;
    remaining -= r;
  }
  buf[total] = '\0';
  if (out_len){
    *out_len = total;
  }
  return true;
}

static blend_mode_t parse_blend(const char *value){
  if (!value) return BLEND_NORMAL;
  if (strcasecmp(value, "add") == 0) return BLEND_ADD;
  if (strcasecmp(value, "screen") == 0) return BLEND_SCREEN;
  if (strcasecmp(value, "multiply") == 0) return BLEND_MULTIPLY;
  if (strcasecmp(value, "lighten") == 0) return BLEND_LIGHTEN;
  return BLEND_NORMAL;
}

static uint8_t clamp_u8(int v){
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

static px_rgba_t json_to_color(const cJSON *node, px_rgba_t fallback){
  px_rgba_t c = fallback;
  if (!node || !cJSON_IsObject(node)){
    return c;
  }
  cJSON *r = cJSON_GetObjectItemCaseSensitive(node, "r");
  cJSON *g = cJSON_GetObjectItemCaseSensitive(node, "g");
  cJSON *b = cJSON_GetObjectItemCaseSensitive(node, "b");
  cJSON *w = cJSON_GetObjectItemCaseSensitive(node, "w");
  if (cJSON_IsNumber(r)) c.r = clamp_u8((int)r->valuedouble);
  if (cJSON_IsNumber(g)) c.g = clamp_u8((int)g->valuedouble);
  if (cJSON_IsNumber(b)) c.b = clamp_u8((int)b->valuedouble);
  if (cJSON_IsNumber(w)) c.w = clamp_u8((int)w->valuedouble);
  return c;
}

static bool json_to_effect(const cJSON *json, effect_params_t *out){
  if (!json || !cJSON_IsObject(json) || !out){
    return false;
  }
  cJSON *eff = cJSON_GetObjectItemCaseSensitive(json, "effect_id");
  if (!cJSON_IsNumber(eff)){
    return false;
  }
  effect_params_t p = {0};
  p.effect_id = (uint32_t)eff->valuedouble;
  p.speed = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "speed"));
  p.intensity = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "intensity"));
  if (p.intensity <= 0.f){
    p.intensity = 1.f;
  }
  p.palette_id = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "palette_id"));
  p.seed = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "seed"));
  p.blend = parse_blend(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "blend")));
  cJSON *opacity = cJSON_GetObjectItemCaseSensitive(json, "opacity");
  if (cJSON_IsNumber(opacity)){
    int op = (int)opacity->valuedouble;
    if (op < 0) op = 0;
    if (op > 255) op = 255;
    p.opacity = (uint8_t)op;
  } else {
    p.opacity = 255;
  }
  p.seg_start = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "seg_start"));
  p.seg_len = (uint16_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "seg_len"));
  p.color1 = json_to_color(cJSON_GetObjectItemCaseSensitive(json, "color1"),
                           (px_rgba_t){255, 255, 255, 0});
  p.color2 = json_to_color(cJSON_GetObjectItemCaseSensitive(json, "color2"),
                           (px_rgba_t){0, 0, 0, 0});
  p.color3 = json_to_color(cJSON_GetObjectItemCaseSensitive(json, "color3"),
                           (px_rgba_t){0, 0, 0, 0});
  *out = p;
  return true;
}

static esp_err_t load_preset(const char *name, effect_params_t *out){
  if (!is_safe_token(name) || !out){
    return ESP_ERR_INVALID_ARG;
  }
  if (!ensure_dir(PRESET_DIR)){
    return ESP_FAIL;
  }
  char path[128];
  preset_path(name, path, sizeof(path));
  FILE *f = fopen(path, "r");
  if (!f){
    return ESP_ERR_NOT_FOUND;
  }
  char *buf = malloc(MAX_BODY_LENGTH);
  if (!buf){
    fclose(f);
    return ESP_ERR_NO_MEM;
  }
  size_t n = fread(buf, 1, MAX_BODY_LENGTH - 1, f);
  fclose(f);
  buf[n] = '\0';

  cJSON *json = cJSON_Parse(buf);
  free(buf);
  if (!json){
    return ESP_FAIL;
  }
  bool ok = json_to_effect(json, out);
  cJSON_Delete(json);
  return ok ? ESP_OK : ESP_FAIL;
}

static esp_err_t save_preset_json(const cJSON *json){
  if (!json){
    return ESP_ERR_INVALID_ARG;
  }
  cJSON *name_item = cJSON_GetObjectItemCaseSensitive(json, "preset_name");
  const char *name = cJSON_GetStringValue(name_item);
  if (!is_safe_token(name)){
    return ESP_ERR_INVALID_ARG;
  }
  effect_params_t tmp;
  if (!json_to_effect(json, &tmp)){
    return ESP_ERR_INVALID_ARG;
  }
  if (!ensure_dir(PRESET_DIR)){
    return ESP_FAIL;
  }
  char path[128], tmp_path[160];
  preset_path(name, path, sizeof(path));
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

  char *payload = cJSON_PrintUnformatted(json);
  if (!payload){
    return ESP_ERR_NO_MEM;
  }
  FILE *f = fopen(tmp_path, "w");
  if (!f){
    free(payload);
    return ESP_FAIL;
  }
  size_t written = fwrite(payload, 1, strlen(payload), f);
  fclose(f);
  free(payload);
  if (written == 0){
    unlink(tmp_path);
    return ESP_FAIL;
  }
  if (rename(tmp_path, path) != 0){
    unlink(tmp_path);
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Preset %s saved", name);
  return ESP_OK;
}

static int parse_channel(const char *target, const char *prefix, int max){
  if (!target || strncmp(target, prefix, strlen(prefix)) != 0){
    return -1;
  }
  char *end = NULL;
  long idx = strtol(target + strlen(prefix), &end, 10);
  if (!end || *end != '\0' || idx < 1 || idx > max){
    return -1;
  }
  return (int)(idx - 1);
}

static esp_err_t json_reply(httpd_req_t *req, cJSON *root){
  char *buf = cJSON_PrintUnformatted(root);
  if (!buf){
    cJSON_Delete(root);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t ret = httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
  free(buf);
  cJSON_Delete(root);
  return ret;
}

static esp_err_t get_status_handler(httpd_req_t *req){
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "node_type", "led-node");
  cJSON_AddStringToObject(root, "role", "Slave");
  cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000ULL);

  float power_scale[EFFECT_CHANNELS];
  for (int i = 0; i < EFFECT_CHANNELS; ++i){
    power_scale[i] = 1.0f;
  }
  if (s_effect_ops.get_power_scale){
    s_effect_ops.get_power_scale(power_scale, EFFECT_CHANNELS);
  }
  bool limit = false;
  for (int i = 0; i < EFFECT_CHANNELS; ++i){
    if (power_scale[i] < 0.99f){
      limit = true;
      break;
    }
  }

  int ch_total = EFFECT_CHANNELS;
  if (s_effect_ops.channel_count){
    int reported = s_effect_ops.channel_count();
    if (reported > 0){
      ch_total = reported;
    }
  }
  cJSON *aled = cJSON_AddArrayToObject(root, "aled");
  for (int i = 0; i < ch_total; ++i){
    led_type_t type;
    color_order_t order;
    uint16_t pixels = 0;
    bool have_info = false;
    if (s_effect_ops.get_channel_info){
      have_info = s_effect_ops.get_channel_info(i, &type, &order, &pixels);
    }
    if (have_info){
      cJSON *a = cJSON_CreateObject();
      cJSON_AddNumberToObject(a, "ch", i + 1);
      cJSON_AddNumberToObject(a, "pixels", pixels);
      cJSON_AddStringToObject(a, "strip_type", strip_type_to_string(type));
      cJSON_AddStringToObject(a, "order", order_to_string(order));
      if (i < EFFECT_CHANNELS){
        cJSON_AddNumberToObject(a, "power_scale", power_scale[i]);
      }
      cJSON_AddItemToArray(aled, a);
    }
  }

  cJSON *fps = cJSON_AddObjectToObject(root, "fps");
  for (int i = 0; i < ch_total; ++i){
    char key[24];
    snprintf(key, sizeof(key), "aled_ch%d", i + 1);
    cJSON_AddNumberToObject(fps, key, 60);
  }

  cJSON *pg = cJSON_AddArrayToObject(root, "pwm_groups");
  if (s_pwm_ops.groups_count && s_pwm_ops.get_group){
    int total = s_pwm_ops.groups_count();
    for (int i = 0; i < total; ++i){
      pwm_group_t info;
      if (s_pwm_ops.get_group(i, &info)){
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "name", info.name);
        cJSON_AddStringToObject(g, "kind", info.kind == PWMG_RGBW ? "RGBW" : "RGB");
        cJSON_AddItemToArray(pg, g);
      }
    }
  }

  cJSON_AddBoolToObject(root, "power_limit_active", limit);
  cJSON_AddNumberToObject(root, "pwm_max_duty", 0.85);
  cJSON_AddNullToObject(root, "last_error");
  cJSON_AddNumberToObject(root, "heap_free_kb", esp_get_free_heap_size() / 1024);

  return json_reply(req, root);
}

static esp_err_t get_presets_handler(httpd_req_t *req){
  cJSON *arr = cJSON_CreateArray();
  if (!ensure_dir(PRESET_DIR)){
    return json_reply(req, arr);
  }

  DIR *dir = opendir(PRESET_DIR);
  if (!dir){
    ESP_LOGW(TAG, "opendir %s failed: %d", PRESET_DIR, errno);
    return json_reply(req, arr);
  }

  struct dirent *ent;
  while ((ent = readdir(dir)) != NULL){
    const char *name = ent->d_name;
    size_t len = strlen(name);
    if (len > 5 && strcmp(name + len - 5, ".json") == 0){
      char preset[MAX_NAME_LENGTH] = {0};
      snprintf(preset, sizeof(preset), "%.*s", (int)(len - 5), name);
      cJSON *item = cJSON_CreateObject();
      cJSON_AddStringToObject(item, "preset_name", preset);
      cJSON_AddItemToArray(arr, item);
    }
  }
  closedir(dir);
  return json_reply(req, arr);
}

static esp_err_t get_config_handler(httpd_req_t *req){
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "node_type", "led-node");

  cJSON *aled = cJSON_AddArrayToObject(root, "aled");
  if (s_effect_ops.channel_count && s_effect_ops.get_channel_info){
    int total = s_effect_ops.channel_count();
    for (int i = 0; i < total; ++i){
      led_type_t type;
      color_order_t order;
      uint16_t pixels = 0;
      if (s_effect_ops.get_channel_info(i, &type, &order, &pixels)){
        cJSON *a = cJSON_CreateObject();
        cJSON_AddNumberToObject(a, "ch", i + 1);
        cJSON_AddNumberToObject(a, "pixels", pixels);
        cJSON_AddStringToObject(a, "strip_type", strip_type_to_string(type));
        cJSON_AddStringToObject(a, "order", order_to_string(order));
        cJSON_AddItemToArray(aled, a);
      }
    }
  }

  cJSON *pg = cJSON_AddArrayToObject(root, "pwm_groups");
  if (s_pwm_ops.groups_count && s_pwm_ops.get_group){
    int total = s_pwm_ops.groups_count();
    for (int i = 0; i < total; ++i){
      pwm_group_t info;
      if (s_pwm_ops.get_group(i, &info)){
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "name", info.name);
        cJSON_AddStringToObject(g, "kind", info.kind == PWMG_RGBW ? "RGBW" : "RGB");
        cJSON *map = cJSON_AddObjectToObject(g, "map");
        cJSON_AddNumberToObject(map, "R", info.map_r + 1);
        cJSON_AddNumberToObject(map, "G", info.map_g + 1);
        cJSON_AddNumberToObject(map, "B", info.map_b + 1);
        if (info.kind == PWMG_RGBW){
          cJSON_AddNumberToObject(map, "W", info.map_w + 1);
        }
        cJSON_AddItemToArray(pg, g);
      }
    }
  }

  return json_reply(req, root);
}

static esp_err_t post_config_handler(httpd_req_t *req){
  char *buf = malloc(MAX_BODY_LENGTH);
  if (!buf){
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  size_t len = 0;
  if (!read_body(req, buf, MAX_BODY_LENGTH, &len)){
    free(buf);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
    return ESP_FAIL;
  }

  cJSON *json = cJSON_ParseWithLength(buf, len);
  free(buf);
  if (!json){
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  cJSON *pg = cJSON_GetObjectItemCaseSensitive(json, "pwm_groups");
  if (pg && cJSON_IsArray(pg) && s_pwm_ops.replace_groups){
    pwm_group_t tmp[8];
    int count = 0;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, pg){
      if (!cJSON_IsObject(entry) || count >= 8){
        continue;
      }
      const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(entry, "name"));
      const char *kind_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(entry, "kind"));
      cJSON *map = cJSON_GetObjectItemCaseSensitive(entry, "map");
      if (!name || !kind_str || !cJSON_IsObject(map)){
        continue;
      }
      pwm_group_t group = {0};
      strncpy(group.name, name, sizeof(group.name) - 1);
      group.kind = (strcasecmp(kind_str, "RGBW") == 0) ? PWMG_RGBW : PWMG_RGB;
      group.map_r = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(map, "R")) - 1;
      group.map_g = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(map, "G")) - 1;
      group.map_b = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(map, "B")) - 1;
      group.map_w = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(map, "W")) - 1;
      tmp[count++] = group;
    }
    s_pwm_ops.replace_groups(tmp, count);
  }

  cJSON_Delete(json);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
  return ESP_OK;
}

static esp_err_t post_presets_handler(httpd_req_t *req){
  char *buf = malloc(MAX_BODY_LENGTH);
  if (!buf){
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  size_t len = 0;
  if (!read_body(req, buf, MAX_BODY_LENGTH, &len)){
    free(buf);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
    return ESP_FAIL;
  }

  cJSON *json = cJSON_ParseWithLength(buf, len);
  free(buf);
  if (!json){
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  esp_err_t res = save_preset_json(json);
  cJSON_Delete(json);
  if (res != ESP_OK){
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid preset");
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
  return ESP_OK;
}

static esp_err_t post_trigger_handler(httpd_req_t *req){
  char *buf = malloc(MAX_BODY_LENGTH);
  if (!buf){
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  size_t len = 0;
  if (!read_body(req, buf, MAX_BODY_LENGTH, &len)){
    free(buf);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
    return ESP_FAIL;
  }
  cJSON *json = cJSON_ParseWithLength(buf, len);
  free(buf);
  if (!json){
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }

  const char *action = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "action"));
  esp_err_t status = ESP_OK;

  if (!action){
    status = ESP_ERR_INVALID_ARG;
  } else if (strcasecmp(action, "play_preset") == 0){
    const char *target = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "target"));
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "name"));
    uint32_t fade_ms = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "fade_ms"));
    int ch = parse_channel(target, "ALEDch", EFFECT_CHANNELS);
    if (ch < 0 || !is_safe_token(name) || !s_effect_ops.set_base){
      status = ESP_ERR_INVALID_ARG;
    } else {
      effect_params_t params;
      status = load_preset(name, &params);
      if (status == ESP_OK){
        if (!s_effect_ops.set_base(ch, &params, fade_ms)){
          status = ESP_FAIL;
        }
      }
    }
  } else if (strcasecmp(action, "set_pwm") == 0){
    const char *target = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "target"));
    const char *mode = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "mode"));
    int ch = parse_channel(target, "LEDch", 8);
    if (ch < 0){
      status = ESP_ERR_INVALID_ARG;
    } else if (!mode || strcasecmp(mode, "static") == 0){
      if (!s_pwm_ops.mode_static){
        status = ESP_ERR_INVALID_STATE;
      } else {
        float duty = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "duty"));
        s_pwm_ops.mode_static((uint8_t)ch, duty);
      }
    } else if (strcasecmp(mode, "breath") == 0){
      if (!s_pwm_ops.mode_breath){
        status = ESP_ERR_INVALID_STATE;
      } else {
        float minv = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "min"));
        float maxv = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "max"));
        float period = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "period_ms"));
        s_pwm_ops.mode_breath((uint8_t)ch, minv, maxv, period);
      }
    } else if (strcasecmp(mode, "candle") == 0){
      if (!s_pwm_ops.mode_candle){
        status = ESP_ERR_INVALID_STATE;
      } else {
        float base = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "base"));
        float flick = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "flicker"));
        uint32_t seed = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "seed"));
        s_pwm_ops.mode_candle((uint8_t)ch, base, flick, seed);
      }
    } else if (strcasecmp(mode, "warmdim") == 0){
      if (!s_pwm_ops.mode_warmdim){
        status = ESP_ERR_INVALID_STATE;
      } else {
        float duty = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "duty"));
        s_pwm_ops.mode_warmdim((uint8_t)ch, duty);
      }
    } else {
      status = ESP_ERR_INVALID_ARG;
    }
  } else if (strcasecmp(action, "set_pwm_group") == 0){
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "name"));
    if (!name){
      status = ESP_ERR_INVALID_ARG;
    } else if (!s_pwm_ops.group_set_rgb || !s_pwm_ops.group_set_rgbw){
      status = ESP_ERR_INVALID_STATE;
    } else {
      float r = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "r"));
      float g = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "g"));
      float b = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "b"));
      cJSON *w_val = cJSON_GetObjectItemCaseSensitive(json, "w");
      if (w_val && cJSON_IsNumber(w_val)){
        float w = (float)w_val->valuedouble;
        s_pwm_ops.group_set_rgbw(name, r, g, b, w);
      } else {
        s_pwm_ops.group_set_rgb(name, r, g, b);
      }
    }
  } else if (strcasecmp(action, "set_strip_type") == 0){
    if (!s_effect_ops.set_channel_type){
      status = ESP_ERR_INVALID_STATE;
    } else {
      int ch = (int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "ch")) - 1;
      const char *type_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "strip_type"));
      const char *order_str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "order"));
      led_type_t type;
      color_order_t order;
      if (ch < 0 || !string_to_strip_type(type_str, &type) || !string_to_order(order_str, &order)){
        status = ESP_ERR_INVALID_ARG;
      } else if (!s_effect_ops.set_channel_type(ch, type, order)){
        status = ESP_FAIL;
      }
    }
  } else if (strcasecmp(action, "blackout") == 0){
    const char *target = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "target"));
    int aled_ch = parse_channel(target, "ALEDch", EFFECT_CHANNELS);
    int pwm_ch  = parse_channel(target, "LEDch", 8);
    if (aled_ch >= 0){
      if (!s_effect_ops.set_base){
        status = ESP_ERR_INVALID_STATE;
      } else {
        effect_params_t off = {
          .effect_id = FX_SOLID,
          .opacity = 255,
          .blend = BLEND_NORMAL,
          .color1 = {0,0,0,0}
        };
        s_effect_ops.set_base(aled_ch, &off, 0);
      }
    } else if (pwm_ch >= 0){
      if (!s_pwm_ops.mode_static){
        status = ESP_ERR_INVALID_STATE;
      } else {
        s_pwm_ops.mode_static((uint8_t)pwm_ch, 0.f);
      }
    } else {
      if (!s_effect_ops.set_base || !s_pwm_ops.mode_static){
        status = ESP_ERR_INVALID_STATE;
      } else {
        effect_params_t off = {
          .effect_id = FX_SOLID,
          .opacity = 255,
          .blend = BLEND_NORMAL,
          .color1 = {0,0,0,0}
        };
        for (int i = 0; i < EFFECT_CHANNELS; ++i){
          s_effect_ops.set_base(i, &off, 0);
        }
        for (int i = 0; i < 8; ++i){
          s_pwm_ops.mode_static((uint8_t)i, 0.f);
        }
      }
    }
  } else if (strcasecmp(action, "beat") == 0){
    if (!s_trigger_ops.set_beat){
      status = ESP_ERR_INVALID_STATE;
    } else {
      float phase = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "phase"));
      s_trigger_ops.set_beat(phase);
      uint32_t strobe_ms = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "strobe_ms"));
      if (strobe_ms > 0 && s_trigger_ops.strobe){
        s_trigger_ops.strobe(strobe_ms);
      }
    }
  } else {
    status = ESP_ERR_INVALID_ARG;
  }

  cJSON_Delete(json);
  if (status != ESP_OK){
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid trigger");
    return status;
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t post_cue_handler(httpd_req_t *req){
  char *buf = malloc(MAX_BODY_LENGTH);
  if (!buf){
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  size_t len = 0;
  if (!read_body(req, buf, MAX_BODY_LENGTH, &len)){
    free(buf);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
    return ESP_FAIL;
  }
  cJSON *json = cJSON_ParseWithLength(buf, len);
  free(buf);
  if (!json){
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    return ESP_FAIL;
  }
  const char *track = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "track"));
  const char *preset = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "preset"));
  uint32_t t_start = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "tStart"));
  uint32_t t_end = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "tEnd"));
  ESP_LOGI(TAG, "Cue track=%s preset=%s [%" PRIu32 ",%" PRIu32 "]",
           track ? track : "-", preset ? preset : "-", t_start, t_end);
  cJSON_Delete(json);

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "{\"status\":\"queued\"}");
  return ESP_OK;
}

static esp_err_t events_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/event-stream");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "keep-alive");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char line[256];
  while (1){
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    bool limit = false;
    float power_scale[EFFECT_CHANNELS];
    for (int i = 0; i < EFFECT_CHANNELS; ++i){
      power_scale[i] = 1.0f;
    }
    if (s_effect_ops.get_power_scale){
      s_effect_ops.get_power_scale(power_scale, EFFECT_CHANNELS);
    }
    for (int i = 0; i < EFFECT_CHANNELS; ++i){
      if (power_scale[i] < 0.99f){
        limit = true;
        break;
      }
    }
    snprintf(line, sizeof(line),
             "event:status\n"
             "data:{\"playhead\":%" PRIu32 ",\"limit\":%d}\n\n",
             now_ms, limit ? 1 : 0);
    esp_err_t err = httpd_resp_send_chunk(req, line, strlen(line));
    if (err != ESP_OK){
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(SSE_INTERVAL_MS));
  }
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

httpd_handle_t rest_api_get_server(void){
  return s_server;
}

esp_err_t rest_api_start(void){
  if (s_server){
    ESP_LOGW(TAG, "REST server already started");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;
  config.uri_match_fn = httpd_uri_match_wildcard;

  if (httpd_start(&s_server, &config) != ESP_OK){
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
  }

  httpd_uri_t get_status = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = get_status_handler,
  };
  httpd_register_uri_handler(s_server, &get_status);

  httpd_uri_t post_trigger = {
    .uri = "/api/trigger",
    .method = HTTP_POST,
    .handler = post_trigger_handler,
  };
  httpd_register_uri_handler(s_server, &post_trigger);

  httpd_uri_t get_config = {
    .uri = "/api/config",
    .method = HTTP_GET,
    .handler = get_config_handler,
  };
  httpd_register_uri_handler(s_server, &get_config);

  httpd_uri_t post_config = {
    .uri = "/api/config",
    .method = HTTP_POST,
    .handler = post_config_handler,
  };
  httpd_register_uri_handler(s_server, &post_config);

  httpd_uri_t post_cue = {
    .uri = "/api/cue",
    .method = HTTP_POST,
    .handler = post_cue_handler,
  };
  httpd_register_uri_handler(s_server, &post_cue);

  httpd_uri_t get_presets = {
    .uri = "/api/presets",
    .method = HTTP_GET,
    .handler = get_presets_handler,
  };
  httpd_register_uri_handler(s_server, &get_presets);

  httpd_uri_t post_presets = {
    .uri = "/api/presets",
    .method = HTTP_POST,
    .handler = post_presets_handler,
  };
  httpd_register_uri_handler(s_server, &post_presets);

  httpd_uri_t events = {
    .uri = "/events",
    .method = HTTP_GET,
    .handler = events_handler,
  };
  httpd_register_uri_handler(s_server, &events);

  ESP_LOGI(TAG, "REST API started");
  return ESP_OK;
}

void rest_api_init(void){
  rest_api_start();
}

void rest_api_register_effect_ops(const rest_api_effect_ops_t *ops){
  if (ops){
    s_effect_ops = *ops;
  } else {
    memset(&s_effect_ops, 0, sizeof(s_effect_ops));
  }
}

void rest_api_register_pwm_ops(const rest_api_pwm_ops_t *ops){
  if (ops){
    s_pwm_ops = *ops;
  } else {
    memset(&s_pwm_ops, 0, sizeof(s_pwm_ops));
  }
}

void rest_api_register_trigger_ops(const rest_api_trigger_ops_t *ops){
  if (ops){
    s_trigger_ops = *ops;
  } else {
    memset(&s_trigger_ops, 0, sizeof(s_trigger_ops));
  }
}

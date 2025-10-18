#include "self_test.h"
#include "aled_rmt.h"
#include "task_pwm_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lednode_init.h"
#include "pca9685_driver.h"
#include "effects.h"

extern aled_channel_t aled_ch[];

static void self_task(void*){
  // ALED: cycle RGBW on each channel for 1s
  px_rgba_t px[16]; for(int i=0;i<16;i++) px[i]=(px_rgba_t){0,0,0,0};
  const px_rgba_t cols[]={{255,0,0,0},{0,255,0,0},{0,0,255,0},{0,0,0,255}};
  for(int ch=0; ch<aled_count(); ch++){
    for(int c=0;c<4;c++){
      for(int i=0;i<16;i++) px[i]=cols[c];
      aled_rmt_write(ch, px, 16, aled_ch[ch].type, aled_ch[ch].order);
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    for(int i=0;i<16;i++) px[i]=(px_rgba_t){0,0,0,0};
    aled_rmt_write(ch, px, 16, aled_ch[ch].type, aled_ch[ch].order);
  }
  // PWM: each to 30% for 300ms then off
  for(int p=0;p<pwm_count();p++){ pca9685_set_duty(p, 0.3f); vTaskDelay(pdMS_TO_TICKS(300)); pca9685_set_duty(p, 0.f); }
  vTaskDelete(NULL);
}
void self_test_run(void){ xTaskCreate(self_task,"self_test",4096,NULL,3,NULL); }

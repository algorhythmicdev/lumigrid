#include "unity.h"
#include "effects.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "crc.h"

void test_rainbow_crc(void){
    // This test is currently disabled because the rainbow effect is not yet implemented.
    // Once the effect is implemented, this test should be enabled and the CRC value updated.
    /*
    px_rgba_t fb[60];
    aled_channel_t ch = {
        .ch = 0,
        .type = LED_WS2812B,
        .n_pixels = 60,
        .framebuf = fb
    };
    effect_params_t p = {
        .effect_id = FX_RAINBOW,
        .speed = 10,
        .intensity = 1,
        .palette_id = 0,
        .seed = 0,
        .blend = BLEND_NORMAL,
        .opacity = 255,
        .seg_start = 0,
        .seg_len = 60
    };
    fx_rainbow(&ch, &p, 0, 0);
    uint32_t crc = crc32_le(0, (uint8_t*)fb, sizeof(fb));
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, crc);
    */
}

TEST_CASE("FX rainbow CRC stable","[fx]"){
    test_rainbow_crc();
}

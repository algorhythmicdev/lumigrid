#include "unity.h"
#include "effects.h"
#include "fx_util.h"
#include "esp_rom_crc.h"
#include <string.h>

TEST_CASE("rainbow CRC", "[fx]") {
    px_rgba_t fb[60];
    aled_channel_t ch = {
        .ch = 0,
        .type = LED_WS2812B,
        .n_pixels = 60,
        .gamma = 2.2f,
        .max_brightness = 255,
        .framebuf = fb
    };

    effect_params_t params = {
        .effect_id = FX_RAINBOW,
        .speed = 0.2f,
        .intensity = 1.0f,
        .palette_id = 0,
        .blend = BLEND_NORMAL,
        .opacity = 255,
        .seg_start = 0,
        .seg_len = 0
    };

    memset(fb, 0, sizeof(fb));
    util_init_gamma(2.2f);

    const effect_vtable_t *fx = fx_lookup(FX_RAINBOW);
    TEST_ASSERT_NOT_NULL(fx);

    uint32_t sum = fx->render(&ch, &params, 1234, 0);
    uint32_t crc = esp_rom_crc32_le(0, (const uint8_t *)fb, sizeof(fb));

    TEST_ASSERT(sum > 0);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xA3B5F2D1, crc, "Update CRC after first run.");
}

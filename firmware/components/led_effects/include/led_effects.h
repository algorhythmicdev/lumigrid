#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/rmt_tx.h" // For RMT peripheral
#include "esp_err.h"       // For esp_err_t

/**
 * @brief Enum for different types of addressable LEDs.
 */
typedef enum {
  LED_WS2812B,      ///< WS2812B type LEDs
  LED_SK6812_RGBW   ///< SK6812 RGBW type LEDs
} led_type_t;

/**
 * @brief Structure to represent a single pixel with RGBA color components.
 */
typedef struct {
  uint8_t r, g, b, w; ///< Red, Green, Blue, White color components
} px_rgba_t;

/**
 * @brief Structure to define an addressable LED channel.
 */
typedef struct {
  int ch;                 ///< Channel number (0-7)
  led_type_t type;        ///< Type of LED on this channel
  uint16_t n_pixels;      ///< Number of pixels in the strip
  float gamma;            ///< Gamma correction value
  uint8_t max_brightness; ///< Maximum brightness (0-255)
  px_rgba_t *framebuf;    ///< Pointer to the frame buffer (length = n_pixels)
  rmt_channel_handle_t rmt_channel; ///< RMT channel handle for this strip
} aled_channel_t;

/**
 * @brief Structure to hold parameters for an LED effect.
 */
typedef struct {
  uint32_t effect_id;  ///< Identifier for the effect
  float speed;         ///< Speed parameter for the effect
  float intensity;     ///< Intensity parameter for the effect
  uint32_t palette_id; ///< Identifier for the color palette
  px_rgba_t color1, color2, color3; ///< Primary, secondary, and tertiary colors
} effect_params_t;

/**
 * @brief Initialize the LED effects engine.
 *
 * This function sets up the RMT channels for all addressable LED strips.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t led_effects_init(void);

/**
 * @brief Set an effect for a specific addressable LED channel.
 *
 * @param ch The channel number (0-7).
 * @param params The effect parameters.
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t aled_set_effect(int ch, const effect_params_t *params);

/**
 * @brief Render the current frame for all active LED channels.
 *
 * This function should be called periodically by the EffectEngine task.
 *
 * @param t_ms Current time in milliseconds.
 */
void led_effects_render_all_channels(uint32_t t_ms);

#endif // LED_EFFECTS_H
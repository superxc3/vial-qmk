/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Customise mode: per-key RGB control, shares g_direct_mode_colors with vialrgb_direct */

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
#define RGB_MATRIX_EFFECT_VIALRGB_CUSTOMISE
RGB_MATRIX_EFFECT(VIALRGB_CUSTOMISE)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

extern HSV g_direct_mode_colors[RGB_MATRIX_LED_COUNT];

bool VIALRGB_CUSTOMISE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t val = rgb_matrix_get_val();
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV hsv = g_direct_mode_colors[i];
        hsv.v = scale8(hsv.v, val);
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}
#    endif
#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <inttypes.h>

#define VIALRGB_PROTOCOL_VERSION 1

/* Start at 0x40 in order to not conflict with existing "enum via_lighting_value",
   even though they likely wouldn't be enabled together with vialrgb */
enum {
    vialrgb_set_mode = 0x41,
    vialrgb_direct_fastset = 0x42,
    vialrgb_set_indicator_leds = 0x45,
    vialrgb_set_indicator_colors = 0x46,
    vialrgb_set_trackpad_settings = 0x50,
    vialrgb_set_trackpad_layers   = 0x51,
    vialrgb_set_oled_config       = 0x52,
    vialrgb_set_power_settings    = 0x53,
};

enum {
    vialrgb_get_info = 0x40,
    vialrgb_get_mode = 0x41,
    vialrgb_get_supported = 0x42,
    vialrgb_get_number_leds = 0x43,
    vialrgb_get_led_info = 0x44,
    vialrgb_get_indicator_leds = 0x45,
    vialrgb_get_indicator_colors = 0x46,
    vialrgb_get_direct_colors = 0x47,
    vialrgb_get_trackpad_settings = 0x50,
    vialrgb_get_trackpad_layers   = 0x51,
    vialrgb_get_oled_config       = 0x52,
    vialrgb_get_power_settings    = 0x53,
};

void vialrgb_get_value(uint8_t *data, uint8_t length);
void vialrgb_set_value(uint8_t *data, uint8_t length);
void vialrgb_save(uint8_t *data, uint8_t length);

/* Weak overrides: implement in keymap.c to handle trackpad settings get/set.
 * Packet layout (args[0..7]):
 *   [0] cursor_dpi_level (1-6)   [1] scroll_speed_level (1-8)
 *   [2] scroll_invert_v (0/1)    [3] scroll_invert_h (0/1)
 *   [4] zoom_enabled (0/1)       [5] trackpad_enabled (0/1)
 *   [6] sniper_scale_level (1-6) [7] taps_as_clicks (0/1)
 *   [8] sniper_modifier_mask (QMK mod bits: LCTL=0x01 LSFT=0x02 LALT=0x04 LGUI=0x08 ...) */
void vialrgb_get_trackpad_settings_user(uint8_t *args);
void vialrgb_set_trackpad_settings_user(const uint8_t *args);

/* Sub-ID 0x51 — trackpad layer behaviour bitmasks (6 bytes):
 *   [0-1] scroll_layers  (LE uint16 — bit N set → layer N uses 2-finger scroll)
 *   [2-3] swipe2_layers  (LE uint16 — bit N set → layer N uses 2-finger swipe)
 *   [4-5] swipe3_layers  (LE uint16 — bit N set → layer N uses 3-finger swipe)
 * Layer 0 (base) is always cursor; unset layers are also cursor. */
void vialrgb_get_trackpad_layers_user(uint8_t *args);
void vialrgb_set_trackpad_layers_user(const uint8_t *args);

/* Sub-ID 0x52 — OLED text labels (index-based, one item per call):
 *   item = 0xFF → keyboard name (row 1, "SOFLE" by default, max 5 chars)
 *   item = 0–9  → layer name for that layer (max 5 chars, null-padded to 5 bytes)
 * GET: args[0]=item → args[1..5]=name bytes (null-padded)
 * SET: args[0]=item,  args[1..5]=name bytes */
void vialrgb_get_oled_config_user(uint8_t item, char *name_out);
void vialrgb_set_oled_config_user(uint8_t item, const char *name_in);

/* Sub-ID 0x53 — shared OLED + RGB sleep timeout.
 *   GET: args[0] = sleep timeout in whole minutes (1-30)
 *   SET: args[0] = sleep timeout in whole minutes (firmware clamps to 1-30) */
void vialrgb_get_power_settings_user(uint8_t *args);
void vialrgb_set_power_settings_user(const uint8_t *args);

/* Weak overrides: implement in keymap.c to handle indicator config get/set.
 *
 * Bitmask LED protocol (0x45):
 *   GET: args[0]=role_idx (0=Caps,1-9=Layers) → response args[0..7]: 8-byte LE uint64 mask
 *   SET: args[0]=role_idx, args[1..8]: 8-byte LE uint64 mask
 *   LEDs can belong to multiple roles simultaneously.
 *
 * Colors protocol (0x46): 30 bytes, [r,g,b] × 10 roles (unchanged) */
void vialrgb_get_indicator_leds_user(uint8_t role_idx, uint8_t *mask_out);
void vialrgb_set_indicator_leds_user(uint8_t role_idx, const uint8_t *mask_in);
void vialrgb_get_indicator_colors_user(uint8_t *colors_out);
void vialrgb_set_indicator_colors_user(const uint8_t *colors_in);

#if defined(VIALRGB_ENABLE) && !defined(RGB_MATRIX_ENABLE)
#error VIALRGB_ENABLE=yes requires RGB_MATRIX_ENABLE=yes
#endif

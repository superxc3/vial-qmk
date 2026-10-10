/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <inttypes.h>
#include <stdbool.h>

#define VIALRGB_PROTOCOL_VERSION 1

/* Start at 0x40 in order to not conflict with existing "enum via_lighting_value",
   even though they likely wouldn't be enabled together with vialrgb */
enum {
    vialrgb_set_mode = 0x41,
    vialrgb_direct_fastset = 0x42,
    vialrgb_set_indicator_leds = 0x45,
    vialrgb_set_indicator_colors = 0x46,
    vialrgb_set_indicator_led_colors = 0x48,
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
    vialrgb_get_indicator_led_colors = 0x48,
    vialrgb_get_trackpad_settings = 0x50,
    vialrgb_get_trackpad_layers   = 0x51,
    vialrgb_get_oled_config       = 0x52,
    vialrgb_get_power_settings    = 0x53,
    vialrgb_get_encoder_events    = 0x54,
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

/* Per-LED indicator colours (0x48) -- each LED in a role can have its own colour.
 * Membership still comes from the 0x45 mask; this only carries colours.
 *   GET: args[0]=role_idx, args[1]=first LED
 *        -> args[0]=VIALRGB_INDICATOR_LED_COLORS_MAGIC, args[1]=count (<= 9),
 *           args[2..]=[r,g,b] x count for LEDs first..first+count-1
 *   SET: args[0]=role_idx, args[1]=first LED, args[2]=count (<= 9),
 *        args[3..]=[r,g,b] x count
 * The GET hook returns false when the keymap does not implement per-LED colours;
 * the request is then echoed unchanged (args[0] = role_idx, never the magic), which
 * is how a host tells old firmware apart and falls back to one colour per role.
 * A 0x46 SET still repaints every LED of each role in that role's colour, so hosts
 * that only know 0x46 keep their old behaviour. */
#define VIALRGB_INDICATOR_LED_COLORS_MAGIC 0x49
#define VIALRGB_INDICATOR_LED_COLORS_MAX   9

/* Indicator masks for LEDs 64-127 (0x45 with role_idx | VIALRGB_INDICATOR_MASK_HI):
 * same 8-byte LE layout, bit i = LED 64 + i. Handled by the keymap hooks; old keymaps
 * see role >= 10 and ignore it (SET) or answer zeros (GET), so hosts only use it once
 * per-LED colours (0x48) are confirmed. Needed on 72-LED boards. */
#define VIALRGB_INDICATOR_MASK_HI 0x80

/* Encoder activity (0x54), for the matrix tester:
 *   GET -> args[0]=VIALRGB_ENCODER_EVENTS_MAGIC, args[1]=n encoders (<= 14),
 *          args[2..]=[ccw_count, cw_count] x n, each a wrapping uint8 turn counter.
 * Old firmware echoes the request (args[0] = 0). */
#define VIALRGB_ENCODER_EVENTS_MAGIC 0x45
bool vialrgb_get_indicator_led_colors_user(uint8_t role_idx, uint8_t first, uint8_t *count_out, uint8_t *rgb_out);
void vialrgb_set_indicator_led_colors_user(uint8_t role_idx, uint8_t first, uint8_t count, const uint8_t *rgb_in);

#if defined(VIALRGB_ENABLE) && !defined(RGB_MATRIX_ENABLE)
#error VIALRGB_ENABLE=yes requires RGB_MATRIX_ENABLE=yes
#endif

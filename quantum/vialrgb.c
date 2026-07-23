/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Based on https://github.com/qmk/qmk_firmware/pull/13036 */

#include "vialrgb.h"

#include <inttypes.h>
#include <string.h>
#include "rgb_matrix.h"
#include "vial.h"

typedef struct {
    uint16_t vialrgb_id;
    uint16_t qmk_id;
} vialrgb_supported_mode_t;

#include "vialrgb_effects.inc"

#define SUPPORTED_MODES_LENGTH (sizeof(supported_modes)/sizeof(*supported_modes))

#ifdef RGB_MATRIX_EFFECT_VIALRGB_DIRECT
HSV g_direct_mode_colors[RGB_MATRIX_LED_COUNT];
#endif

static void get_supported(uint8_t *args, uint8_t length) {
    /* retrieve supported effects (VialRGB IDs) with ID > gt */
    uint16_t gt;
    memcpy(&gt, args, sizeof(gt));
    memset(args, 0xFF, length);
    for (size_t i = 0; i < SUPPORTED_MODES_LENGTH; ++i) {
        uint16_t id = pgm_read_word(&supported_modes[i].vialrgb_id);
        if (id > gt && length >= sizeof(id)) {
            memcpy(args, &id, sizeof(id));
            length -= sizeof(id);
            args += sizeof(id);
        }
    }
}

static uint16_t qmk_id_to_vialrgb_id(uint16_t id) {
    for (size_t i = 0; i < SUPPORTED_MODES_LENGTH; ++i) {
        uint16_t qmk_id = pgm_read_word(&supported_modes[i].qmk_id);
        uint16_t vialrgb_id = pgm_read_word(&supported_modes[i].vialrgb_id);
        if (qmk_id == id)
            return vialrgb_id;
    }
    return 0;
}

static uint16_t vialrgb_id_to_qmk_id(uint16_t id) {
    for (size_t i = 0; i < SUPPORTED_MODES_LENGTH; ++i) {
        uint16_t qmk_id = pgm_read_word(&supported_modes[i].qmk_id);
        uint16_t vialrgb_id = pgm_read_word(&supported_modes[i].vialrgb_id);
        if (vialrgb_id == id)
            return qmk_id;
    }
    return 0;
}

static uint16_t get_mode(void) {
    /* Get current mode as vialrgb ID */
    if (!rgb_matrix_is_enabled())
        return VIALRGB_EFFECT_OFF;
    return qmk_id_to_vialrgb_id(rgb_matrix_get_mode());
}

static void set_mode(uint16_t mode) {
    /* Set a mode as vialrgb ID */
    if (mode == VIALRGB_EFFECT_OFF) {
        rgb_matrix_disable_noeeprom();
    } else {
        rgb_matrix_enable_noeeprom();
        rgb_matrix_mode_noeeprom(vialrgb_id_to_qmk_id(mode));
    }
}

#ifdef RGB_MATRIX_EFFECT_VIALRGB_DIRECT
static void get_matrix_pos_for_led(uint16_t led, uint8_t *output) {
    /* reset initially so if we cannot locate the led, it's considered not part of kb matrix */
    output[0] = output[1] = 0xFF;
    /* this is kinda O(n^2) but what can you do */
    for (size_t row = 0; row < MATRIX_ROWS; ++row)
        for (size_t col = 0; col < MATRIX_COLS; ++col)
            if (g_led_config.matrix_co[row][col] == led) {
                output[0] = row;
                output[1] = col;
                return;
            }
}

static void fast_set_leds(uint8_t *args, size_t length) {
    /* Set multiple leds HSV, first 2 bytes are index of the first led, then number of leds, followed by HSV for the leds */
    /* we have 32-2-2-1=27 total bytes available, so can set up to 9 leds per packet */
    if (length < 3) return;

    uint16_t first_index = args[0] | (args[1] << 8);
    uint8_t num_leds = args[2];
    length -= 3;
    args += 3;

    if (num_leds * 3 > length) return;

    for (size_t i = 0; i < num_leds; ++i) {
        if (i + first_index >= RGB_MATRIX_LED_COUNT)
            break;
        g_direct_mode_colors[i + first_index].h = args[i * 3 + 0];
        g_direct_mode_colors[i + first_index].s = args[i * 3 + 1];
        uint8_t val = args[i * 3 + 2];
        g_direct_mode_colors[i + first_index].v = (val > RGB_MATRIX_MAXIMUM_BRIGHTNESS) ? RGB_MATRIX_MAXIMUM_BRIGHTNESS : val;
    }
}
#endif

void vialrgb_get_value(uint8_t *data, uint8_t length) {
    if (length != VIAL_RAW_EPSIZE) return;

    /* data[0] is used by VIA command id */
    uint8_t cmd = data[1];
    uint8_t *args = &data[2];
    switch (cmd) {
    case vialrgb_get_info:
        args[0] = VIALRGB_PROTOCOL_VERSION & 0xFF;
        args[1] = VIALRGB_PROTOCOL_VERSION >> 8;
        args[2] = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
        break;
    case vialrgb_get_mode: {
        uint16_t vialrgb_id = get_mode();
        args[0] = vialrgb_id & 0xFF;
        args[1] = vialrgb_id >> 8;
        args[2] = rgb_matrix_get_speed();
        args[3] = rgb_matrix_get_hue();
        args[4] = rgb_matrix_get_sat();
        args[5] = rgb_matrix_get_val();
        break;
    }
    case vialrgb_get_supported: {
        get_supported(args, length - 2);
        break;
    }
#ifdef RGB_MATRIX_EFFECT_VIALRGB_DIRECT
    case vialrgb_get_number_leds: {
        args[0] = RGB_MATRIX_LED_COUNT & 0xFF;
        args[1] = RGB_MATRIX_LED_COUNT >> 8;
        break;
    }
    case vialrgb_get_led_info: {
        uint16_t led = (args[0] & 0xFF) | (args[1] >> 8);
        if (led >= RGB_MATRIX_LED_COUNT) return;
        // x, y
        args[0] = g_led_config.point[led].x;
        args[1] = g_led_config.point[led].y;
        // flags
        args[2] = g_led_config.flags[led];
        // position in keyboard matrix (if it's keyboard LED, otherwise 0xFF)
        get_matrix_pos_for_led(led, &args[3]);
        break;
    }
    case vialrgb_get_direct_colors: {
        /* Read back per-key colors: args[0..1] = first led index, response is
           args[0] = number of leds in this packet, then HSV per led (up to 9).
           Firmware without this command echoes the request back, so a query
           starting at led 0 parses as count=0 and the host stops cleanly. */
        uint16_t first_index = args[0] | (args[1] << 8);
        uint8_t max_leds = (length - 3) / 3;
        memset(args, 0, length - 2);
        uint8_t num_leds = 0;
        while (num_leds < max_leds && first_index + num_leds < RGB_MATRIX_LED_COUNT) {
            args[1 + num_leds * 3 + 0] = g_direct_mode_colors[first_index + num_leds].h;
            args[1 + num_leds * 3 + 1] = g_direct_mode_colors[first_index + num_leds].s;
            args[1 + num_leds * 3 + 2] = g_direct_mode_colors[first_index + num_leds].v;
            ++num_leds;
        }
        args[0] = num_leds;
        break;
    }
#endif
    case vialrgb_get_indicator_leds: {
        /* args[0] = role_idx; response args[0..7]: 8-byte LE uint64 bitmask */
        uint8_t role = args[0];
        memset(args, 0, 8);
        vialrgb_get_indicator_leds_user(role, args);
        break;
    }
    case vialrgb_get_indicator_colors: {
        /* args[0..29]: [r,g,b] per role */
        memset(args, 0, 30);
        vialrgb_get_indicator_colors_user(args);
        break;
    }
    case vialrgb_get_trackpad_settings: {
        memset(args, 0, 9);
        vialrgb_get_trackpad_settings_user(args);
        break;
    }
    case vialrgb_get_trackpad_layers: {
        memset(args, 0, 6);
        vialrgb_get_trackpad_layers_user(args);
        break;
    }
    case vialrgb_get_oled_config: {
        /* args[0] = item (0xFF=row1, 0-9=layer); args[1..5] = name (5 bytes, null-padded) */
        uint8_t item = args[0];
        memset(args + 1, 0, 5);
        vialrgb_get_oled_config_user(item, (char *)(args + 1));
        break;
    }
    case vialrgb_get_power_settings: {
        /* args[0] = shared OLED+RGB sleep timeout in whole minutes */
        memset(args, 0, 1);
        vialrgb_get_power_settings_user(args);
        break;
    }
    }
}

void vialrgb_set_value(uint8_t *data, uint8_t length) {
    if (length != VIAL_RAW_EPSIZE) return;

    /* data[0] is used by VIA command id */
    uint8_t cmd = data[1];
    uint8_t *args = &data[2];
    switch (cmd) {
    case vialrgb_set_mode: {
        uint16_t vialrgb_id = args[0] | (args[1] << 8);
        set_mode(vialrgb_id);
        rgb_matrix_set_speed_noeeprom(args[2]);
        rgb_matrix_sethsv_noeeprom(args[3], args[4], args[5]);
        break;
    }
#ifdef RGB_MATRIX_EFFECT_VIALRGB_DIRECT
    case vialrgb_direct_fastset: {
        fast_set_leds(args, length);
        break;
    }
#endif
    case vialrgb_set_indicator_leds: {
        /* args[0] = role_idx; args[1..8]: 8-byte LE uint64 bitmask */
        vialrgb_set_indicator_leds_user(args[0], args + 1);
        break;
    }
    case vialrgb_set_indicator_colors: {
        /* args[0..29]: [r,g,b] per role */
        vialrgb_set_indicator_colors_user(args);
        break;
    }
    case vialrgb_set_trackpad_settings: {
        vialrgb_set_trackpad_settings_user(args);
        break;
    }
    case vialrgb_set_trackpad_layers: {
        vialrgb_set_trackpad_layers_user(args);
        break;
    }
    case vialrgb_set_oled_config: {
        /* args[0] = item (0xFF=row1, 0-9=layer); args[1..5] = name (5 bytes) */
        vialrgb_set_oled_config_user(args[0], (const char *)(args + 1));
        break;
    }
    case vialrgb_set_power_settings: {
        /* args[0] = shared OLED+RGB sleep timeout in whole minutes */
        vialrgb_set_power_settings_user(args);
        break;
    }
    }
}

/* Override in keymap.c to persist per-key colors to EEPROM on Save. */
__attribute__((weak)) void vialrgb_save_user(void) {}

/* Weak stubs for trackpad settings — override in keymap.c. */
__attribute__((weak)) void vialrgb_get_trackpad_settings_user(uint8_t *args) { (void)args; }
__attribute__((weak)) void vialrgb_set_trackpad_settings_user(const uint8_t *args) { (void)args; }
__attribute__((weak)) void vialrgb_get_trackpad_layers_user(uint8_t *args) { (void)args; }
__attribute__((weak)) void vialrgb_set_trackpad_layers_user(const uint8_t *args) { (void)args; }
__attribute__((weak)) void vialrgb_get_oled_config_user(uint8_t item, char *name_out) { (void)item; (void)name_out; }
__attribute__((weak)) void vialrgb_set_oled_config_user(uint8_t item, const char *name_in) { (void)item; (void)name_in; }
__attribute__((weak)) void vialrgb_get_power_settings_user(uint8_t *args) { (void)args; }
__attribute__((weak)) void vialrgb_set_power_settings_user(const uint8_t *args) { (void)args; }

/* Weak stubs for indicator config — override in keymap.c. */
__attribute__((weak)) void vialrgb_get_indicator_leds_user(uint8_t role_idx, uint8_t *mask_out) {
    (void)role_idx; (void)mask_out;
}
__attribute__((weak)) void vialrgb_set_indicator_leds_user(uint8_t role_idx, const uint8_t *mask_in) {
    (void)role_idx; (void)mask_in;
}
__attribute__((weak)) void vialrgb_get_indicator_colors_user(uint8_t *colors_out) {
    (void)colors_out;
}
__attribute__((weak)) void vialrgb_set_indicator_colors_user(const uint8_t *colors_in) {
    (void)colors_in;
}

void vialrgb_save(uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;

    eeconfig_force_flush_rgb_matrix();
    vialrgb_save_user();
}

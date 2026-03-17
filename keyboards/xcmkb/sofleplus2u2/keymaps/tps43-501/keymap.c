/* Copyright 2020 Josef Adamcik
  * Modification for VIA support and RGB underglow by Jens Bonk-Wiltfang
  * TPS43 v5.01: Multitouch digitizer + runtime DPI/scroll/sniper
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */

#include QMK_KEYBOARD_H
#include <math.h>
#include "via.h"
#include "timer.h"
#include "rgb_matrix.h"
#include "eeconfig.h"
#include "os_detection.h"
#include "platforms/eeprom.h"
#include "digitizer_mouse_fallback.h"

#ifndef setPinInputPullup
#  define setPinInputPullup(pin) gpio_set_pin_input_high(pin)
#endif

__attribute__((unused)) static void tap_via_key(keypos_t key) { }

#ifdef SUPER_ALT_TAB_ENABLE
    bool is_alt_tab_active = false;
    uint16_t alt_tab_timer = 0;
#endif

// ==================== Custom Keycodes ====================

#ifdef VIA_ENABLE
    enum custom_keycodes {
    CK_ATABF = QK_KB_0,
    CK_ATABR,
    CK_ATMWU,
    CK_ATMWD,
    CK_PO,
    SCROLL_DIR_V,
    SCROLL_DIR_H,
    CURSOR_SPEED_UP,
    CURSOR_SPEED_DN,
    CURSOR_SPEED_RESET,
    SCROLL_SPEED_UP,
    SCROLL_SPEED_DOWN,
    SCROLL_SPEED_RESET,
    TRACKPAD_LAYER_SCROLL_SET,
    TRACKPAD_LAYER_SWIPE2_SET,
    TRACKPAD_LAYER_SWIPE3_SET,
    TRACKPAD_LAYER_RESET,
    TRACKPAD_TOGGLE,
    SNIPER_MO,
    SNIPER_TOG,
    SNIPER_SET_MODS,
    SNIPER_DPI_UP,
    SNIPER_DPI_DOWN,
    SNIPER_SHOW_MODS,
    OS_DETECTION_TOGGLE,
    ZMTOG,
    };
#else
    enum custom_keycodes {
    CK_ATABF = SAFE_RANGE,
    CK_ATABR,
    CK_ATMWU,
    CK_ATMWD,
    CK_PO,
    SCROLL_DIR_V,
    SCROLL_DIR_H,
    CURSOR_SPEED_UP,
    CURSOR_SPEED_DN,
    CURSOR_SPEED_RESET,
    SCROLL_SPEED_UP,
    SCROLL_SPEED_DOWN,
    SCROLL_SPEED_RESET,
    TRACKPAD_LAYER_SCROLL_SET,
    TRACKPAD_LAYER_SWIPE2_SET,
    TRACKPAD_LAYER_SWIPE3_SET,
    TRACKPAD_LAYER_RESET,
    TRACKPAD_TOGGLE,
    SNIPER_MO,
    SNIPER_TOG,
    SNIPER_SET_MODS,
    SNIPER_DPI_UP,
    SNIPER_DPI_DOWN,
    SNIPER_SHOW_MODS,
    OS_DETECTION_TOGGLE,
    ZMTOG,
    };
#endif

// ==================== Speed/Scroll Constants ====================

#define MIN_CURSOR_SPEED     1
#define MAX_CURSOR_SPEED     6
#define DEFAULT_CURSOR_SPEED 3
#define DEFAULT_SNIPER_SPEED 1

#define MIN_SCROLL_SPEED     1
#define MAX_SCROLL_SPEED     8
#define DEFAULT_SCROLL_SPEED 4

// ==================== State Variables ====================

int16_t scroll_speed = DEFAULT_SCROLL_SPEED;
static bool trackpad_enabled = true;
int16_t scroll_dir_v = 0;
int16_t scroll_dir_h = 0;

// Sniper mode state
#define LEARNING_TIMEOUT 5000
#define INFO_TIMEOUT 2000
#define INFO_DISPLAY_TIMEOUT 3000

bool sniper_mode_active = false;
uint8_t sniper_modifier_mask = 0;
bool sniper_learning_mode = false;
bool sniper_info_mode = false;
uint16_t learning_timer = 0;
uint16_t info_timer = 0;

// Zoom gesture state
bool zoom_enabled = true;

// OS detection toggle
static bool os_detection_enabled = true;

os_variant_t get_effective_os_detection(void) {
    if (!os_detection_enabled) return OS_UNSURE;
    return detected_host_os();
}

// ==================== EEPROM ====================

// EEPROM Address Map (within 4KB RP2040 limit)
// eeconfig_user (32-bit): cursor_speed[0:15], scroll_dir_v[16], scroll_dir_h[17], scroll_speed[18:23]
// 0x0FB0: sniper_modifier_mask
// 0x0FB1: sniper_scale_level
// 0x0FA0-0x0FA7: trackpad layer config
// 0x0FB2: zoom toggle
// 0x0FB3: os_detection_enabled
#define EEPROM_SNIPER_SETTINGS_OFFSET 0x0FB0
#define EEPROM_ZOOM_TOGGLE_OFFSET 0x0FB2
#define EEPROM_OS_DETECTION_OFFSET 0x0FB3

static void save_user_settings(void) {
    uint8_t cursor_level = digitizer_get_mouse_scale();
    eeconfig_update_user((cursor_level & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
}

void save_sniper_settings(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET), sniper_modifier_mask);
    eeprom_write_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET + 1), digitizer_get_sniper_scale());
}

void load_sniper_settings(void) {
    uint8_t stored_mask = eeprom_read_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET));
    uint8_t stored_speed = eeprom_read_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET + 1));

    if (stored_mask == 0xFF && stored_speed == 0xFF) {
        sniper_modifier_mask = 0;
        digitizer_set_sniper_scale(DEFAULT_SNIPER_SPEED);
        save_sniper_settings();
    } else {
        sniper_modifier_mask = stored_mask;
        if (stored_speed >= MIN_CURSOR_SPEED && stored_speed <= MAX_CURSOR_SPEED) {
            digitizer_set_sniper_scale(stored_speed);
        } else {
            digitizer_set_sniper_scale(DEFAULT_SNIPER_SPEED);
            save_sniper_settings();
        }
    }
    sniper_mode_active = false;
    digitizer_set_sniper_active(false);
}

void save_zoom_setting(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_ZOOM_TOGGLE_OFFSET), zoom_enabled ? 1 : 0);
}

void load_zoom_setting(void) {
    uint8_t stored_zoom = eeprom_read_byte((uint8_t*)(EEPROM_ZOOM_TOGGLE_OFFSET));
    if (stored_zoom == 0xFF) {
        zoom_enabled = true;
        save_zoom_setting();
    } else {
        zoom_enabled = (stored_zoom != 0);
    }
}

void os_detection_settings_init(void) {
    uint8_t stored_state = eeprom_read_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET);
    if (stored_state == 0xFF) {
        os_detection_enabled = true;
        eeprom_write_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET, 1);
    } else {
        os_detection_enabled = (stored_state != 0);
    }
}

void toggle_os_detection(void) {
    os_detection_enabled = !os_detection_enabled;
    eeprom_write_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET, os_detection_enabled ? 1 : 0);
}

// ==================== Trackpad Layer Config ====================

#define EECONFIG_USER_TRACKPAD_OFFSET 0x0FA0
#define TRACKPAD_CONFIG_MAGIC 0x7401

typedef struct {
    uint16_t scroll_layers;
    uint16_t swipe2_layers;
    uint16_t swipe3_layers;
} trackpad_layer_config_t;

static trackpad_layer_config_t user_config = {
    .scroll_layers = (1 << 1),
    .swipe2_layers = (1 << 2),
    .swipe3_layers = (1 << 3),
};

void trackpad_config_save(void) {
    uint16_t magic = TRACKPAD_CONFIG_MAGIC;
    eeprom_write_word((uint16_t*)(EECONFIG_USER_TRACKPAD_OFFSET), magic);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 2), user_config.scroll_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 3), (user_config.scroll_layers >> 8) & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 4), user_config.swipe2_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 5), (user_config.swipe2_layers >> 8) & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 6), user_config.swipe3_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 7), (user_config.swipe3_layers >> 8) & 0xFF);
}

void trackpad_config_load(void) {
    uint16_t magic = eeprom_read_word((uint16_t*)(EECONFIG_USER_TRACKPAD_OFFSET));
    if (magic == TRACKPAD_CONFIG_MAGIC) {
        uint8_t scroll_low = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 2));
        uint8_t scroll_high = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 3));
        uint8_t swipe2_low = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 4));
        uint8_t swipe2_high = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 5));
        uint8_t swipe3_low = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 6));
        uint8_t swipe3_high = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 7));
        user_config.scroll_layers = scroll_low | (scroll_high << 8);
        user_config.swipe2_layers = swipe2_low | (swipe2_high << 8);
        user_config.swipe3_layers = swipe3_low | (swipe3_high << 8);
    } else {
        trackpad_config_save();
    }
}

void trackpad_layer_reset(void) {
    uint8_t current_layer = get_highest_layer(layer_state);
    if (current_layer <= 15) {
        uint16_t layer_bit = (1 << current_layer);
        user_config.scroll_layers &= ~layer_bit;
        user_config.swipe2_layers &= ~layer_bit;
        user_config.swipe3_layers &= ~layer_bit;
        trackpad_config_save();
    }
}

// ==================== Housekeeping ====================

void housekeeping_task_user(void) {
    static layer_state_t state = 0;
    if (layer_state != state) {
        state = layer_state_set_user(layer_state);
    }
}

// ==================== RGB Layer Indicators ====================

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t led_indices[] = {4, 5, 6, 15, 16, 33, 34, 35, 44, 45};
    uint8_t brightness = rgb_matrix_get_val();
    if (brightness == 0) brightness = 1;
    #define SCALE_BRIGHTNESS(color) (((color * brightness) / 255) ?: 1)

    bool indicator_set = false;

    if (host_keyboard_led_state().caps_lock) {
        for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
            if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                rgb_matrix_set_color(led_indices[i], SCALE_BRIGHTNESS(128), 0, 0);
            }
        }
        indicator_set = true;
    }

    uint8_t current_layer = get_highest_layer(layer_state);
    if (current_layer > 0) {
        uint8_t r = 0, g = 0, b = 0;
        switch (current_layer) {
            case 1: r = 128; b = 128; break;
            case 2: r = 255; g = 215; break;
            case 3: g = 128; b = 128; break;
            case 4: r = 255; g = 128; break;
            case 5: b = 128; break;
            case 6: r = 128; b = 128; break;
            case 7: r = 255; g = 192; b = 203; break;
            case 8: g = 255; b = 127; break;
            case 9: r = 255; g = 255; b = 255; break;
            default: break;
        }
        if (r || g || b) {
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                    rgb_matrix_set_color(led_indices[i], SCALE_BRIGHTNESS(r), SCALE_BRIGHTNESS(g), SCALE_BRIGHTNESS(b));
                }
            }
            indicator_set = true;
        }
    }

    #undef SCALE_BRIGHTNESS
    return indicator_set;
}

// ==================== Function Prototypes ====================

void keyboard_post_init_user(void);
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report);
bool process_record_user(uint16_t keycode, keyrecord_t *record);

// ==================== DIP Switch ====================
// 5 standalone DIP switches on GP12-GP16, each reads keycode from virtual position [index, 6]

#if defined(DIP_SWITCH_ENABLE)
static uint16_t dip_switch_keycodes[5] = { KC_NO, KC_NO, KC_NO, KC_NO, KC_NO };

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index > 4) return true;

    keypos_t kp = { .row = index, .col = 6 };

    if (active) {
        uint16_t kc = keymap_key_to_keycode(get_highest_layer(layer_state), kp);
        dip_switch_keycodes[index] = kc;

        keyrecord_t record = {
            .event = {
                .key = kp,
                .pressed = true,
                .time = timer_read(),
                .type = KEY_EVENT
            },
            .keycode = kc
        };

        if (kc == RM_TOGG) {
            rgb_matrix_toggle();
        } else if (kc == RM_ON) {
            rgb_matrix_step();
        } else if (kc >= QK_KB_0) {
            process_record_user(kc, &record);
        } else if (IS_QK_TOGGLE_LAYER(kc)) {
            layer_invert(QK_TOGGLE_LAYER_GET_LAYER(kc));
        } else if (IS_QK_TO(kc)) {
            layer_move(QK_TO_GET_LAYER(kc));
        } else if (IS_QK_MOMENTARY(kc)) {
            layer_on(QK_MOMENTARY_GET_LAYER(kc));
        } else if (IS_QK_LAYER_TAP(kc)) {
            layer_on(QK_LAYER_TAP_GET_LAYER(kc));
            register_code16(QK_LAYER_TAP_GET_TAP_KEYCODE(kc));
        } else if (kc >= MS_UP && kc <= MS_ACL2) {
            register_code16(kc);
        } else {
            register_code16(kc);
        }
    } else {
        uint16_t kc = dip_switch_keycodes[index];
        if (kc != KC_NO) {
            keyrecord_t record = {
                .event = {
                    .key = kp,
                    .pressed = false,
                    .time = timer_read(),
                    .type = KEY_EVENT
                },
                .keycode = kc
            };

            if (IS_QK_MOMENTARY(kc)) {
                layer_off(QK_MOMENTARY_GET_LAYER(kc));
            } else if (IS_QK_LAYER_TAP(kc)) {
                layer_off(QK_LAYER_TAP_GET_LAYER(kc));
                unregister_code16(QK_LAYER_TAP_GET_TAP_KEYCODE(kc));
            } else if (kc >= MS_UP && kc <= MS_ACL2) {
                unregister_code16(kc);
            } else if (kc >= QK_KB_0) {
                process_record_user(kc, &record);
            } else if (!(kc == RM_TOGG || kc == RM_ON || IS_QK_TOGGLE_LAYER(kc) || IS_QK_TO(kc))) {
                unregister_code16(kc);
            }
            dip_switch_keycodes[index] = KC_NO;
        }
    }
    return true;
}
#endif

// ==================== Keymaps ====================

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        KC_GRAVE, KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    MS_UP, MS_BTN1,
        KC_ESC,   KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,                     KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,  KC_BSPC,
        KC_TAB,   KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                     KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN,  KC_QUOT,
        KC_LSFT,  KC_Z,   KC_X,    KC_C,    KC_V,    KC_B, KC_MUTE,    CK_PO, KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,  KC_RSFT,
                        KC_LGUI,KC_LALT,KC_LCTL, MO(1), KC_ENT,      KC_SPC,  MO(2), KC_RCTL, KC_RALT, KC_RGUI,
                        KC_LEFT, KC_UP, KC_RIGHT, KC_DOWN, MS_BTN1
    ),

    [1] = LAYOUT(
        KC_F12,         KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,                        KC_F6,        KC_F7,  KC_F8,  KC_F9,  KC_F10,         KC_F11,
        KC_GRAVE,       LSFT(KC_1), LSFT(KC_2), KC_LBRC,    KC_RBRC,    KC_SLASH,                     KC_MINUS,     KC_7,   KC_8,   KC_9,   KC_COMMA,       KC_BSPC,
        LSFT(KC_GRAVE), LSFT(KC_3), LSFT(KC_4), LSFT(KC_9), LSFT(KC_0), LSFT(KC_7),                   KC_EQUAL,     KC_4,   KC_5,   KC_6,   KC_KP_ASTERISK, KC_DELETE,
        KC_CAPS_LOCK,   LSFT(KC_5), LSFT(KC_6), KC_TRNS,    KC_TRNS,    LSFT(KC_8), KC_TRNS, KC_TRNS, KC_NUM_LOCK,  KC_1,   KC_2,   KC_3,   KC_KP_SLASH,    KC_KP_ENTER,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_KP_DOT, KC_0, KC_EQUAL,
                SCROLL_SPEED_DOWN, CURSOR_SPEED_DN, SCROLL_SPEED_UP, CURSOR_SPEED_UP, MS_BTN1
    ),

    [2] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_PGUP, KC_HOME, KC_UP, KC_END, KC_PSCR, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_PGDN, KC_LEFT, KC_DOWN, KC_RIGHT, KC_INSERT, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, RM_TOGG
    ),

    [3] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),

    [4] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),

    [5] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    )
};

#if defined(ENCODER_MAP_ENABLE)
    const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
        [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
        [1] = { ENCODER_CCW_CW(CK_ATABF, CK_ATABR), ENCODER_CCW_CW(MS_WHLD, MS_WHLU) },
        [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_F3, C(KC_F3)) },
        [3] = { ENCODER_CCW_CW(G(KC_LEFT), G(KC_RGHT)), ENCODER_CCW_CW(A(KC_RGHT), A(KC_LEFT)) },
        [4] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [5] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) }
    };
#endif

// ==================== Keyboard Init ====================

void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(GP12);

    // Load cursor/scroll settings from eeconfig_user
    uint32_t eeprom_data = eeconfig_read_user();
    uint16_t raw_cursor_speed = eeprom_data & 0xFFFF;
    uint8_t raw_scroll_speed = (eeprom_data >> 18) & 0x3F;

    if (raw_cursor_speed >= MIN_CURSOR_SPEED && raw_cursor_speed <= MAX_CURSOR_SPEED) {
        digitizer_set_mouse_scale(raw_cursor_speed);
    } else {
        digitizer_set_mouse_scale(DEFAULT_CURSOR_SPEED);
    }

    if (raw_scroll_speed >= MIN_SCROLL_SPEED && raw_scroll_speed <= MAX_SCROLL_SPEED) {
        scroll_speed = raw_scroll_speed;
        digitizer_set_scroll_scale(raw_scroll_speed);
    } else {
        scroll_speed = DEFAULT_SCROLL_SPEED;
        digitizer_set_scroll_scale(DEFAULT_SCROLL_SPEED);
    }

    scroll_dir_v = (eeprom_data >> 16) & 1;
    scroll_dir_h = (eeprom_data >> 17) & 1;

    if (raw_cursor_speed != digitizer_get_mouse_scale() || raw_scroll_speed != (uint8_t)scroll_speed) {
        save_user_settings();
    }

    trackpad_config_load();
    load_sniper_settings();
    load_zoom_setting();
    os_detection_settings_init();
}

// ==================== Matrix Scan ====================

void matrix_scan_user(void) {
#ifdef SUPER_ALT_TAB_ENABLE
    if (is_alt_tab_active) {
        if (timer_elapsed(alt_tab_timer) > 1000) {
            os_variant_t detected_os = get_effective_os_detection();
            if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                unregister_code(KC_LGUI);
            } else {
                unregister_code(KC_LALT);
            }
            is_alt_tab_active = false;
        }
    }
#endif

    if (sniper_info_mode) {
        if (timer_elapsed(info_timer) > INFO_TIMEOUT) {
            sniper_info_mode = false;
        }
    }
}

// ==================== Pointing Device Task ====================

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    if (!trackpad_enabled) {
        mouse_report.x = 0;
        mouse_report.y = 0;
        mouse_report.h = 0;
        mouse_report.v = 0;
        mouse_report.buttons = 0;
        return mouse_report;
    }

    // Handle hardware zoom gestures (OS-aware)
    static bool zoom_active = false;
    static bool zoom_using_cmd = false;

    if (zoom_enabled && (mouse_report.buttons & (1 << 6)) != 0) {
        if (!zoom_active) {
            os_variant_t detected_os = get_effective_os_detection();
            if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                register_code(KC_LGUI);
                zoom_using_cmd = true;
            } else {
                register_code(KC_LCTL);
                zoom_using_cmd = false;
            }
            register_code(KC_KP_MINUS);
            zoom_active = true;
        }
        mouse_report.buttons &= ~(1 << 6);
    } else if (zoom_enabled && (mouse_report.buttons & (1 << 7)) != 0) {
        if (!zoom_active) {
            os_variant_t detected_os = get_effective_os_detection();
            if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                register_code(KC_LGUI);
                zoom_using_cmd = true;
            } else {
                register_code(KC_LCTL);
                zoom_using_cmd = false;
            }
            register_code(KC_KP_PLUS);
            zoom_active = true;
        }
        mouse_report.buttons &= ~(1 << 7);
    } else {
        if (zoom_active) {
            unregister_code(KC_KP_PLUS);
            unregister_code(KC_KP_MINUS);
            if (zoom_using_cmd) {
                unregister_code(KC_LGUI);
            } else {
                unregister_code(KC_LCTL);
            }
        }
        zoom_active = false;
    }

    if (!zoom_enabled) {
        mouse_report.buttons &= ~((1 << 6) | (1 << 7));
    }

    // Handle modifier-based sniper activation
    if (sniper_modifier_mask != 0) {
        uint8_t current_mods = get_mods() | get_oneshot_mods() | get_weak_mods();
        bool mods_match = (current_mods & sniper_modifier_mask) == sniper_modifier_mask;
        static bool prev_mods_match = false;

        if (mods_match && !prev_mods_match) {
            sniper_mode_active = true;
            digitizer_set_sniper_active(true);
        } else if (!mods_match && prev_mods_match) {
            sniper_mode_active = false;
            digitizer_set_sniper_active(false);
        }
        prev_mods_match = mods_match;
    }

    // Handle learning mode
    if (sniper_learning_mode) {
        uint8_t current_mods = get_mods() | get_oneshot_mods() | get_weak_mods();
        if (current_mods != 0) {
            sniper_modifier_mask = current_mods;
            sniper_learning_mode = false;
            save_sniper_settings();
        } else if (timer_elapsed(learning_timer) > LEARNING_TIMEOUT) {
            sniper_learning_mode = false;
        }
    }

    // Handle info mode timeout
    if (sniper_info_mode) {
        if (timer_elapsed(info_timer) > INFO_DISPLAY_TIMEOUT) {
            sniper_info_mode = false;
        }
    }

    return mouse_report;
}

// ==================== Process Record ====================

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
#ifdef SUPER_ALT_TAB_ENABLE
        case CK_ATABF:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI);
                    } else {
                        register_code(KC_LALT);
                    }
                }
                alt_tab_timer = timer_read();
                register_code(KC_TAB);
            } else {
                unregister_code(KC_TAB);
            }
            break;

        case CK_ATABR:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI);
                    } else {
                        register_code(KC_LALT);
                    }
                }
                alt_tab_timer = timer_read();
                register_code(KC_LSFT);
                register_code(KC_TAB);
            } else {
                unregister_code(KC_LSFT);
                unregister_code(KC_TAB);
            }
            break;

        case CK_ATMWU:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI);
                    } else {
                        register_code(KC_LALT);
                    }
                }
                alt_tab_timer = timer_read();
                register_code(MS_WHLU);
            } else {
                unregister_code(MS_WHLU);
            }
            break;

        case CK_ATMWD:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI);
                    } else {
                        register_code(KC_LALT);
                    }
                }
                alt_tab_timer = timer_read();
                register_code(MS_WHLD);
            } else {
                unregister_code(MS_WHLD);
            }
            break;
#endif

        case CK_PO:
            if (record->event.pressed) {
                os_variant_t detected_os = get_effective_os_detection();
                if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                    register_code(KC_LGUI);
                    register_code(KC_D);
                    unregister_code(KC_D);
                    unregister_code(KC_LGUI);
                } else {
                    register_code(KC_LALT);
                    register_code(KC_F4);
                    unregister_code(KC_F4);
                    unregister_code(KC_LALT);
                }
            }
            break;

        case SCROLL_DIR_V:
            if (record->event.pressed) {
                scroll_dir_v = !scroll_dir_v;
                save_user_settings();
            }
            break;

        case SCROLL_DIR_H:
            if (record->event.pressed) {
                scroll_dir_h = !scroll_dir_h;
                save_user_settings();
            }
            return false;

        case CURSOR_SPEED_UP:
            if (record->event.pressed) {
                uint8_t lvl = digitizer_get_mouse_scale();
                digitizer_set_mouse_scale(lvl < MAX_CURSOR_SPEED ? lvl + 1 : MIN_CURSOR_SPEED);
                save_user_settings();
            }
            break;

        case CURSOR_SPEED_DN:
            if (record->event.pressed) {
                uint8_t lvl = digitizer_get_mouse_scale();
                if (lvl > MIN_CURSOR_SPEED) {
                    digitizer_set_mouse_scale(lvl - 1);
                    save_user_settings();
                }
            }
            break;

        case CURSOR_SPEED_RESET:
            if (record->event.pressed) {
                digitizer_set_mouse_scale(DEFAULT_CURSOR_SPEED);
                save_user_settings();
            }
            break;

        case SCROLL_SPEED_UP:
            if (record->event.pressed) {
                scroll_speed = (scroll_speed < MAX_SCROLL_SPEED) ? (scroll_speed + 1) : MIN_SCROLL_SPEED;
                digitizer_set_scroll_scale(scroll_speed);
                save_user_settings();
            }
            break;

        case SCROLL_SPEED_DOWN:
            if (record->event.pressed) {
                if (scroll_speed > MIN_SCROLL_SPEED) {
                    scroll_speed--;
                    digitizer_set_scroll_scale(scroll_speed);
                    save_user_settings();
                }
            }
            break;

        case SCROLL_SPEED_RESET:
            if (record->event.pressed) {
                scroll_speed = DEFAULT_SCROLL_SPEED;
                digitizer_set_scroll_scale(DEFAULT_SCROLL_SPEED);
                save_user_settings();
            }
            break;

        case TRACKPAD_LAYER_SCROLL_SET:
            if (record->event.pressed) {
                uint8_t current_layer = get_highest_layer(layer_state);
                if (current_layer <= 15) {
                    uint16_t layer_bit = (1 << current_layer);
                    user_config.swipe2_layers &= ~layer_bit;
                    user_config.swipe3_layers &= ~layer_bit;
                    user_config.scroll_layers ^= layer_bit;
                    trackpad_config_save();
                }
            }
            break;

        case TRACKPAD_LAYER_SWIPE2_SET:
            if (record->event.pressed) {
                uint8_t current_layer = get_highest_layer(layer_state);
                if (current_layer <= 15) {
                    uint16_t layer_bit = (1 << current_layer);
                    user_config.scroll_layers &= ~layer_bit;
                    user_config.swipe3_layers &= ~layer_bit;
                    user_config.swipe2_layers ^= layer_bit;
                    trackpad_config_save();
                }
            }
            break;

        case TRACKPAD_LAYER_SWIPE3_SET:
            if (record->event.pressed) {
                uint8_t current_layer = get_highest_layer(layer_state);
                if (current_layer <= 15) {
                    uint16_t layer_bit = (1 << current_layer);
                    user_config.scroll_layers &= ~layer_bit;
                    user_config.swipe2_layers &= ~layer_bit;
                    user_config.swipe3_layers ^= layer_bit;
                    trackpad_config_save();
                }
            }
            break;

        case TRACKPAD_LAYER_RESET:
            if (record->event.pressed) {
                trackpad_layer_reset();
            }
            break;

        case TRACKPAD_TOGGLE:
            if (record->event.pressed) {
                trackpad_enabled = !trackpad_enabled;
            }
            break;

        case SNIPER_TOG:
            if (record->event.pressed) {
                sniper_mode_active = !sniper_mode_active;
                digitizer_set_sniper_active(sniper_mode_active);
            }
            break;

        case SNIPER_MO:
            sniper_mode_active = record->event.pressed;
            digitizer_set_sniper_active(record->event.pressed);
            break;

        case SNIPER_DPI_UP:
            if (record->event.pressed) {
                uint8_t lvl = digitizer_get_sniper_scale();
                digitizer_set_sniper_scale(lvl < MAX_CURSOR_SPEED ? lvl + 1 : MIN_CURSOR_SPEED);
                save_sniper_settings();
            }
            break;

        case SNIPER_DPI_DOWN:
            if (record->event.pressed) {
                uint8_t lvl = digitizer_get_sniper_scale();
                if (lvl > MIN_CURSOR_SPEED) {
                    digitizer_set_sniper_scale(lvl - 1);
                    save_sniper_settings();
                }
            }
            break;

        case SNIPER_SET_MODS:
            if (record->event.pressed) {
                sniper_learning_mode = true;
                learning_timer = timer_read();
            }
            break;

        case SNIPER_SHOW_MODS:
            if (record->event.pressed) {
                sniper_info_mode = true;
                info_timer = timer_read();
            }
            break;

        case OS_DETECTION_TOGGLE:
            if (record->event.pressed) {
                toggle_os_detection();
            }
            break;

        case ZMTOG:
            if (record->event.pressed) {
                zoom_enabled = !zoom_enabled;
                save_zoom_setting();
            }
            break;
    }

    return true;
}

// ==================== OLED ====================
#ifdef OLED_ENABLE

#include "oled_data.h"

unsigned int animation_state = 0;

// Gesture bitmaps (32x11px each, stored as 64 bytes per bitmap)
static const char PROGMEM gesture_2finger[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x0c,
    0x34, 0x44, 0x84, 0x04, 0x84, 0x44, 0x34, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM gesture_3finger[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x0c, 0x34, 0x44, 0x84, 0x04,
    0x84, 0x44, 0x34, 0x0c, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x02,
    0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM gesture_default[] = {
    0x00, 0x20, 0x20, 0x70, 0xf8, 0xfc, 0xfe, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x84,
    0x0c, 0x3c, 0xfc, 0xfc, 0xfc, 0x3c, 0x0c, 0x04, 0x00, 0xfe, 0xfc, 0xf8, 0x70, 0x20, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM gesture_scroll[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x04,
    0x0c, 0x3c, 0xfc, 0xfc, 0xfc, 0x3c, 0x0c, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char gesture_blank[64] PROGMEM = {0};

static const char* get_trackpad_gesture_bitmap(uint8_t layer) {
    if (user_config.scroll_layers & (1 << layer)) return gesture_scroll;
    if (user_config.swipe2_layers & (1 << layer)) return gesture_2finger;
    if (user_config.swipe3_layers & (1 << layer)) return gesture_3finger;
    return gesture_default;
}

static void render_space(void) {
    char wpm = get_current_wpm();
    uint8_t render_row[128];
    int i;

    oled_set_cursor(0,0);
    for(i=0; i<wpm/4; i++) render_row[i] = pgm_read_byte(space_row_1+i+animation_state);
    for(i=wpm/4; i<128; i++) render_row[i] = (pgm_read_byte(space_row_1+i+animation_state) & pgm_read_byte(mask_row_1+i-wpm/4)) | pgm_read_byte(ship_row_1+i-wpm/4);
    oled_write_raw((const char*)render_row, 128);

    oled_set_cursor(0,1);
    for(i=0; i<wpm/4; i++) render_row[i] = pgm_read_byte(space_row_2+i+animation_state);
    for(i=wpm/4; i<128; i++) render_row[i] = (pgm_read_byte(space_row_2+i+animation_state) & pgm_read_byte(mask_row_2+i-wpm/4)) | pgm_read_byte(ship_row_2+i-wpm/4);
    oled_write_raw((const char*)render_row, 128);

    oled_set_cursor(0,2);
    for(i=0; i<wpm/4; i++) render_row[i] = pgm_read_byte(space_row_3+i+animation_state);
    for(i=wpm/4; i<128; i++) render_row[i] = (pgm_read_byte(space_row_3+i+animation_state) & pgm_read_byte(mask_row_3+i-wpm/4)) | pgm_read_byte(ship_row_3+i-wpm/4);
    oled_write_raw((const char*)render_row, 128);

    oled_set_cursor(0,3);
    for(i=0; i<wpm/4; i++) render_row[i] = pgm_read_byte(space_row_4+i+animation_state);
    for(i=wpm/4; i<128; i++) render_row[i] = (pgm_read_byte(space_row_4+i+animation_state) & pgm_read_byte(mask_row_4+i-wpm/4)) | pgm_read_byte(ship_row_4+i-wpm/4);
    oled_write_raw((const char*)render_row, 128);

    animation_state = (animation_state + 1 + (wpm/15)) % (128*2);
}

uint32_t anim_sleep = 0;

// Large layer number display
static void display_large_layer_number(uint8_t layer) {
    const uint8_t* bitmap = NULL;
    switch (layer) {
        case 0: bitmap = digit_0; break;
        case 1: bitmap = digit_1; break;
        case 2: bitmap = digit_2; break;
        case 3: bitmap = digit_3; break;
        case 4: bitmap = digit_4; break;
        case 5: bitmap = digit_5; break;
        case 6: bitmap = digit_6; break;
        case 7: bitmap = digit_7; break;
        case 8: bitmap = digit_8; break;
        case 9: bitmap = digit_9; break;
        default: bitmap = digit_0; break;
    }
    if (bitmap) {
        oled_set_cursor(0, 11);
        oled_write_raw_P((const char*)bitmap, 128);
    }
}

// Determine if we are in PTP mode (Windows/Linux sends PTP feature report)
static bool is_ptp_mode(void) {
    return !digitizer_send_mouse_reports;
}

static void print_status_narrow(void) {
    uint8_t current_layer = get_highest_layer(layer_state);

    /* Row 1: SOFLE header */
    oled_set_cursor(0, 1);
    oled_write("SOFLE", false);

    /* Row 2: OS detection status */
    oled_set_cursor(0, 2);
    if (!os_detection_enabled) {
        oled_write("PLUS+", false);
    } else {
        os_variant_t detected_os = detected_host_os();
        switch (detected_os) {
            case OS_MACOS:
            case OS_IOS:
                oled_write(" MAC ", false);
                break;
            case OS_WINDOWS:
                oled_write(" WIN ", false);
                break;
            case OS_LINUX:
                oled_write(" LNX ", false);
                break;
            default:
                oled_write("PLUS+", false);
                break;
        }
    }

    /* Row 3: Numlock indicator */
    oled_set_cursor(0, 3);
    led_t led_usb_state = host_keyboard_led_state();
    if (led_usb_state.num_lock) {
        oled_write_P(PSTR("NUMLK"), false);
    } else {
        oled_write_P(PSTR("     "), false);
    }

    /* Row 4: Gesture bitmap (only in mouse fallback mode) */
    oled_set_cursor(0, 4);
    if (is_ptp_mode()) {
        oled_write_raw_P(gesture_blank, 64);
    } else if (trackpad_enabled && !sniper_learning_mode && !sniper_info_mode) {
        const char* gesture_bitmap = get_trackpad_gesture_bitmap(current_layer);
        oled_write_raw_P(gesture_bitmap, 64);
    } else {
        oled_write_raw_P(gesture_blank, 64);
    }

    /* Clear text rows 6-9 */
    oled_set_cursor(0, 6);
    oled_write_P(PSTR("     "), false);
    oled_set_cursor(0, 7);
    oled_write_P(PSTR("     "), false);
    oled_set_cursor(0, 8);
    oled_write_P(PSTR("     "), false);
    oled_set_cursor(0, 9);
    oled_write_P(PSTR("     "), false);

    if (is_ptp_mode()) {
        /* PTP mode: Windows or Linux handles gestures natively */
        oled_set_cursor(0, 6);
        oled_write_P(PSTR(" PTP "), false);
    } else if (trackpad_enabled) {
        if (sniper_learning_mode) {
            oled_set_cursor(0, 6);
            oled_write_P(PSTR("LEARN"), false);
            oled_set_cursor(0, 7);
            oled_write_P(PSTR("SNIPE"), false);
            oled_set_cursor(0, 8);
            oled_write_P(PSTR("HOLD"), false);
            oled_set_cursor(0, 9);
            oled_write_P(PSTR("MODS"), false);
        } else {
            /* Gesture mode label */
            oled_set_cursor(0, 6);
            if (user_config.scroll_layers & (1 << current_layer)) {
                oled_write_P(PSTR("SCROL"), false);
            } else if (user_config.swipe2_layers & (1 << current_layer)) {
                oled_write_P(PSTR("2SWPE"), false);
            } else if (user_config.swipe3_layers & (1 << current_layer)) {
                oled_write_P(PSTR("3SWPE"), false);
            } else {
                oled_write_P(PSTR("CURSR"), false);
            }

            /* Cursor speed (DPI) */
            oled_set_cursor(0, 8);
            if (sniper_mode_active) {
                oled_write_P(PSTR("SNP"), false);
                oled_set_cursor(3, 8);
                char speed_str[4];
                snprintf(speed_str, sizeof(speed_str), "%d", digitizer_get_sniper_scale());
                oled_write(speed_str, false);
            } else {
                oled_write_P(PSTR("DPI"), false);
                oled_set_cursor(3, 8);
                char speed_str[4];
                snprintf(speed_str, sizeof(speed_str), "%d", digitizer_get_mouse_scale());
                oled_write(speed_str, false);
            }

            /* Scroll speed */
            oled_set_cursor(0, 9);
            oled_write_P(PSTR("SCR"), false);
            oled_set_cursor(3, 9);
            char scr_str[4];
            snprintf(scr_str, sizeof(scr_str), "%d", scroll_speed);
            oled_write(scr_str, false);
        }
    } else {
        oled_set_cursor(0, 6);
        oled_write_P(PSTR("NO"), false);
        oled_set_cursor(0, 7);
        oled_write_P(PSTR("TRKPD"), false);
    }

    /* Sniper info display override */
    if (sniper_info_mode) {
        oled_set_cursor(0, 6);
        oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 7);
        oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 8);
        oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 9);
        oled_write_P(PSTR("     "), false);

        oled_set_cursor(0, 6);
        oled_write_P(PSTR("SNIPE"), false);
        oled_set_cursor(0, 7);
        oled_write_P(PSTR("MODS:"), false);
        oled_set_cursor(0, 8);
        if (sniper_modifier_mask == 0) {
            oled_write_P(PSTR("NONE"), false);
        } else {
            char mod_str[9] = "";
            if (sniper_modifier_mask & MOD_BIT(KC_LCTL)) strcat(mod_str, "LC");
            if (sniper_modifier_mask & MOD_BIT(KC_LSFT)) strcat(mod_str, "LS");
            if (sniper_modifier_mask & MOD_BIT(KC_LALT)) strcat(mod_str, "LA");
            if (sniper_modifier_mask & MOD_BIT(KC_LGUI)) strcat(mod_str, "LG");
            if (sniper_modifier_mask & MOD_BIT(KC_RCTL)) strcat(mod_str, "RC");
            if (sniper_modifier_mask & MOD_BIT(KC_RSFT)) strcat(mod_str, "RS");
            if (sniper_modifier_mask & MOD_BIT(KC_RALT)) strcat(mod_str, "RA");
            if (sniper_modifier_mask & MOD_BIT(KC_RGUI)) strcat(mod_str, "RG");
            oled_write(mod_str, false);
        }
    }

    /* Large layer number (row 11-14) */
    display_large_layer_number(current_layer);
}

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    if (is_keyboard_master()) {
        return OLED_ROTATION_270;
    }
    return rotation;
}

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        if (is_oled_on()) {
            if (last_input_activity_elapsed() > OLED_TIMEOUT) {
                oled_off();
            } else {
                print_status_narrow();
            }
        }
        if (!is_oled_on() && last_input_activity_elapsed() < 1000) {
            oled_on();
        }
    } else {
        if (is_oled_on()) {
            if (last_input_activity_elapsed() > OLED_TIMEOUT) {
                oled_off();
            } else {
                render_space();
            }
        }
        if (!is_oled_on() && last_input_activity_elapsed() < 1000) {
            oled_on();
        }
    }
    return false;
}

#endif

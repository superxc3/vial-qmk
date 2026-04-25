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

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
#include "transactions.h"

/* Number of LEDs on the LEFT half (keyboard.json split_count[0]).
 * LED indices 0..(SPLIT_LEFT-1) are left side; SPLIT_LEFT..(total-1) are right side.
 * This is independent of which physical side is currently master. */
#define VIALRGB_SPLIT_LEFT 29u

extern HSV g_direct_mode_colors[RGB_MATRIX_LED_COUNT];

static void vialrgb_direct_sync_handler(uint8_t in_buflen, const void *in_data,
                                         uint8_t out_buflen, void *out_data) {
    /* Packet layout: [1 byte start_idx] [VIALRGB_SPLIT_LEFT * sizeof(HSV) bytes of HSV data]
     * Master embeds the slave's start index so the slave doesn't need is_keyboard_left(). */
    if (in_buflen < 1 + VIALRGB_SPLIT_LEFT * sizeof(HSV)) return;
    const uint8_t start = ((const uint8_t *)in_data)[0];
    if ((uint16_t)(start + VIALRGB_SPLIT_LEFT) > RGB_MATRIX_LED_COUNT) return;
    memcpy(&g_direct_mode_colors[start],
           (const uint8_t *)in_data + 1,
           VIALRGB_SPLIT_LEFT * sizeof(HSV));
}

#endif

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
    AP_GLOB,
    TP_INFO,
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
    AP_GLOB,
    TP_INFO,
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
bool zoom_enabled      = true;
static bool zoom_active     = false; // modifier currently held for zoom
static bool zoom_using_cmd  = false; // true = KC_LGUI, false = KC_LCTL

// Release any modifier keys held by the zoom gesture.
// Safe to call even when zoom is not active.
static void zoom_cleanup(void) {
    if (zoom_active) {
        unregister_code(KC_KP_PLUS);
        unregister_code(KC_KP_MINUS);
        if (zoom_using_cmd) {
            unregister_code(KC_LGUI);
        } else {
            unregister_code(KC_LCTL);
        }
        zoom_active = false;
    }
}

// OS detection toggle
static bool os_detection_enabled = true;

os_variant_t get_effective_os_detection(void) {
    if (!os_detection_enabled) return OS_UNSURE;
    return detected_host_os();
}

// ==================== EEPROM ====================

// EEPROM Address Map (within 4KB RP2040 limit)
// eeconfig_user (32-bit): cursor_speed[0:15], scroll_dir_v[16], scroll_dir_h[17], scroll_speed[18:23]
// 0x0FA0-0x0FA7: trackpad layer config
// 0x0FB0: sniper_modifier_mask
// 0x0FB1: sniper_scale_level
// 0x0FB2: zoom toggle
// 0x0FB3: os_detection_enabled
// 0x0FB4: taps_as_clicks (0=off, 1=on; 0xFF=unset→default off)
// 0x0FB5: trackpad_enabled (0=off, 1=on; 0xFF=unset→default on)
// 0x0FC0-0x0FC1: VialRGB colors magic (0x5243 = 'RC')
// 0x0FC2-0x106F: g_direct_mode_colors (RGB_MATRIX_LED_COUNT * sizeof(HSV) = 174 bytes)
// 0x1070-0x1071: Indicator config magic (0x494Du = 'IM')
// 0x1072-0x10DB: indicator_config_t (110 bytes: role_masks[10]*8 + colors[10][3])
// 0x10DC-0x10DF: reserved gap
// 0x10E0-0x10E1: OLED config magic (0x4F4Cu = 'OL')
// 0x10E2-0x1123: oled_config_t (66 bytes)
#define EEPROM_SNIPER_SETTINGS_OFFSET    0x0FB0
#define EEPROM_ZOOM_TOGGLE_OFFSET        0x0FB2
#define EEPROM_OS_DETECTION_OFFSET       0x0FB3
#define EEPROM_TAPS_AS_CLICKS_OFFSET     0x0FB4
#define EEPROM_TRACKPAD_ENABLED_OFFSET   0x0FB5
#define EEPROM_VIALRGB_COLORS_MAGIC_OFFSET 0x0FC0u
#define EEPROM_VIALRGB_COLORS_DATA_OFFSET  0x0FC2u
#define VIALRGB_COLORS_EEPROM_MAGIC        0x5243u  /* 'R','C' — RGB Customise */
#define EEPROM_INDICATOR_MAGIC_OFFSET      0x1070u
#define EEPROM_INDICATOR_DATA_OFFSET       0x1072u
#define INDICATOR_EEPROM_MAGIC             0x494Du  /* 'I','M' — Indicator bitmask v5 */
#define EEPROM_OLED_MAGIC_OFFSET           0x10E0u
#define EEPROM_OLED_DATA_OFFSET            0x10E2u
#define OLED_EEPROM_MAGIC                  0x4F4Cu  /* 'O','L' — OLED labels */

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

static void save_taps_as_clicks(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_TAPS_AS_CLICKS_OFFSET),
                      digitizer_taps_as_clicks ? 1 : 0);
}

static void load_taps_as_clicks(void) {
    uint8_t stored = eeprom_read_byte((uint8_t*)(EEPROM_TAPS_AS_CLICKS_OFFSET));
    if (stored == 0xFF) {
        digitizer_taps_as_clicks = false;
        save_taps_as_clicks();
    } else {
        digitizer_taps_as_clicks = (stored != 0);
    }
}

static void save_trackpad_enabled(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_TRACKPAD_ENABLED_OFFSET),
                      trackpad_enabled ? 0x01 : 0x02);
}

static void load_trackpad_enabled(void) {
    uint8_t stored = eeprom_read_byte((uint8_t*)(EEPROM_TRACKPAD_ENABLED_OFFSET));
    if (stored == 0x02) {
        trackpad_enabled = false;
    } else {
        // 0x01 = explicitly on; 0xFF/0x00/other = unset (default on)
        trackpad_enabled = true;
        if (stored != 0x01) save_trackpad_enabled();  // migrate to new encoding
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

// ==================== VialRGB EEPROM persistence ====================

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)

// ==================== Indicator Config ====================
/* Flat LED→role map: each LED byte holds a role index (0-9) or 0xFF (unassigned).
 * Role 0 = Caps Lock, roles 1-9 = Layer 1-9.
 * Caps Lock and an active layer can both be lit simultaneously (on different LEDs).
 * Unlimited LEDs per role — any number of LEDs may share the same role index.
 *
 * HID protocol for 0x45 (leds): paged, 29 LED roles per packet.
 *   args[0] = page number, args[1..29] = led_roles[page*29 .. page*29+28]
 * HID protocol for 0x46 (colors): 30 bytes, [r,g,b] × 10 roles (unchanged). */

#define INDICATOR_NUM_ROLES   10  /* 0=caps, 1-9=layers */

typedef struct {
    uint64_t role_masks[INDICATOR_NUM_ROLES]; /* bit i set = LED i belongs to role */
    uint8_t  colors[INDICATOR_NUM_ROLES][3];  /* [role][r,g,b] */
} __attribute__((packed)) indicator_config_t;  /* 80 + 30 = 110 bytes */

static indicator_config_t g_indicator_config;  /* zero-init; filled by indicator_config_init_defaults */

/* OLED label config — row1 + one name per layer, all max 5 chars + null */
typedef struct {
    char row1[6];           /* keyboard name shown on OLED row 1 (default "SOFLE") */
    char layer_names[10][6]; /* one 5-char label per layer (default "L0".."L9")   */
} __attribute__((packed)) oled_config_t;

static oled_config_t g_oled_config = {
    .row1 = "SOFLE",
    .layer_names = {
        "L0   ", "L1   ", "L2   ", "L3   ", "L4   ",
        "L5   ", "L6   ", "L7   ", "L8   ", "L9   "
    }
};

static void save_oled_config(void) {
    eeprom_write_word((uint16_t *)EEPROM_OLED_MAGIC_OFFSET, OLED_EEPROM_MAGIC);
    eeprom_write_block(&g_oled_config, (void *)EEPROM_OLED_DATA_OFFSET, sizeof(oled_config_t));
}

static void load_oled_config(void) {
    if (eeprom_read_word((const uint16_t *)EEPROM_OLED_MAGIC_OFFSET) == OLED_EEPROM_MAGIC) {
        eeprom_read_block(&g_oled_config, (const void *)EEPROM_OLED_DATA_OFFSET, sizeof(oled_config_t));
    }
}

static void indicator_config_init_defaults(void) {
    memset(&g_indicator_config, 0, sizeof(g_indicator_config));
    /* Default: one key per role (same physical keys as before) */
    static const uint8_t default_leds[INDICATOR_NUM_ROLES] = {4, 5, 6, 15, 16, 33, 34, 35, 44, 45};
    for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) {
        if (default_leds[i] < 64)
            g_indicator_config.role_masks[i] = (1ULL << default_leds[i]);
    }
    /* Default colors */
    static const uint8_t dr[INDICATOR_NUM_ROLES] = {128, 128, 255,   0, 255,   0, 128, 255,   0, 255};
    static const uint8_t dg[INDICATOR_NUM_ROLES] = {  0,   0, 215, 128, 128,   0,   0, 192, 255, 255};
    static const uint8_t db[INDICATOR_NUM_ROLES] = {  0, 128,   0, 128,   0, 128, 128, 203, 127, 255};
    for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) {
        g_indicator_config.colors[i][0] = dr[i];
        g_indicator_config.colors[i][1] = dg[i];
        g_indicator_config.colors[i][2] = db[i];
    }
}

void vialrgb_get_indicator_leds_user(uint8_t role_idx, uint8_t *out) {
    /* Returns 8 bytes: little-endian uint64 bitmask for the given role. */
    if (role_idx >= INDICATOR_NUM_ROLES) { memset(out, 0, 8); return; }
    uint64_t mask = g_indicator_config.role_masks[role_idx];
    for (uint8_t i = 0; i < 8; i++) out[i] = (uint8_t)(mask >> (i * 8));
}

void vialrgb_get_indicator_colors_user(uint8_t *out) {
    /* Returns 30 bytes: [r,g,b] for each of the 10 roles. */
    for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) {
        out[i * 3]     = g_indicator_config.colors[i][0];
        out[i * 3 + 1] = g_indicator_config.colors[i][1];
        out[i * 3 + 2] = g_indicator_config.colors[i][2];
    }
}

/* g_indicator_sync_needed: set true on boot and whenever indicator config changes.
 * housekeeping_task_user() will push the config to the slave when this is true. */
static bool g_indicator_sync_needed = true;

/* TP_INFO: one-shot RPC from master — pushes current trackpad/OS state to slave OLED */
#define TP_INFO_DURATION_MS 5000
typedef struct {
    uint8_t os_variant;
    bool    ptp_mode;
    bool    trackpad_on;
    uint8_t gesture_mode;     // 0=CURSR 1=SCROL 2=2SWPE 3=3SWPE
    uint8_t dpi;
    uint8_t sniper_dpi;
    uint8_t scroll_spd;
    bool    sniper_active;
    bool    show_overlay;     // true = TP_INFO keypress; false = background auto-sync
    bool    os_detect_enabled;// mirrors master's os_detection_enabled flag
    char    row1[5];          // keyboard name (e.g. "SOFLE")
    char    layer_names[10][6];// per-layer display names
} __attribute__((packed)) tp_info_payload_t;
static tp_info_payload_t g_tp_info_data         = {0};
static bool              tp_info_active         = false;
static uint32_t          tp_info_timer          = 0;
static bool              g_tp_info_send_pending = false;
static bool              g_slave_sync_valid     = false;  // true after first auto-sync received

void vialrgb_set_indicator_leds_user(uint8_t role_idx, const uint8_t *mask_bytes) {
    /* Receives 8 bytes: little-endian uint64 bitmask for the given role. */
    if (role_idx >= INDICATOR_NUM_ROLES) return;
    uint64_t mask = 0;
    for (uint8_t i = 0; i < 8; i++) mask |= ((uint64_t)mask_bytes[i] << (i * 8));
    g_indicator_config.role_masks[role_idx] = mask;
    g_indicator_sync_needed = true;
}

void vialrgb_set_indicator_colors_user(const uint8_t *cols) {
    /* Receives 30 bytes: [r,g,b] for each of the 10 roles. */
    for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) {
        g_indicator_config.colors[i][0] = cols[i * 3];
        g_indicator_config.colors[i][1] = cols[i * 3 + 1];
        g_indicator_config.colors[i][2] = cols[i * 3 + 2];
    }
    g_indicator_sync_needed = true;
}

static void vialrgb_indicator_sync_handler(uint8_t in_buflen, const void *in_data,
                                            uint8_t out_buflen, void *out_data) {
    /* Slave receives a full indicator_config_t from master. */
    if (in_buflen < sizeof(indicator_config_t)) return;
    memcpy(&g_indicator_config, in_data, sizeof(indicator_config_t));
}

/* Trackpad settings — exposed to Vial GUI via sub-ID 0x50.
 * Packet bytes: [0]=cursor_dpi [1]=scroll_speed [2]=invert_v [3]=invert_h
 *               [4]=zoom_en    [5]=trackpad_en  [6]=sniper_scale [7]=taps_as_clicks */
void vialrgb_get_trackpad_settings_user(uint8_t *args) {
    args[0] = digitizer_get_mouse_scale();
    args[1] = (uint8_t)scroll_speed;
    args[2] = scroll_dir_v ? 1 : 0;
    args[3] = scroll_dir_h ? 1 : 0;
    args[4] = zoom_enabled ? 1 : 0;
    args[5] = trackpad_enabled ? 1 : 0;
    args[6] = digitizer_get_sniper_scale();
    args[7] = digitizer_taps_as_clicks ? 1 : 0;
    args[8] = sniper_modifier_mask;
}

void vialrgb_set_trackpad_settings_user(const uint8_t *args) {
    digitizer_set_mouse_scale(args[0]);
    scroll_speed = args[1];
    digitizer_set_scroll_scale(args[1]);
    scroll_dir_v = args[2] ? 1 : 0;
    scroll_dir_h = args[3] ? 1 : 0;
    zoom_enabled = args[4] != 0;
    // args[5] (trackpad_enabled) intentionally ignored: the Vial GUI caches
    // the state at connect-time and overwrites any physical key toggle made
    // after that. Trackpad on/off is controlled exclusively by TRACKPAD_TOGGLE.
    digitizer_set_sniper_scale(args[6]);
    digitizer_taps_as_clicks = args[7] != 0;
    sniper_modifier_mask = args[8];
    save_user_settings();
    save_zoom_setting();
    save_taps_as_clicks();
    save_sniper_settings();
}

/* 0x51 — trackpad layer behaviour bitmasks */
void vialrgb_get_trackpad_layers_user(uint8_t *args) {
    args[0] = user_config.scroll_layers & 0xFF;
    args[1] = (user_config.scroll_layers >> 8) & 0xFF;
    args[2] = user_config.swipe2_layers & 0xFF;
    args[3] = (user_config.swipe2_layers >> 8) & 0xFF;
    args[4] = user_config.swipe3_layers & 0xFF;
    args[5] = (user_config.swipe3_layers >> 8) & 0xFF;
}

void vialrgb_set_trackpad_layers_user(const uint8_t *args) {
    user_config.scroll_layers  = args[0] | ((uint16_t)args[1] << 8);
    user_config.swipe2_layers  = args[2] | ((uint16_t)args[3] << 8);
    user_config.swipe3_layers  = args[4] | ((uint16_t)args[5] << 8);
    trackpad_config_save();
}

/* Sub-ID 0x52 — OLED label get/set.
 * item=0xFF → row1 (keyboard name); item=0-9 → layer name for that layer.
 * name_out / name_in: 5-byte buffer (null-padded, NOT null-terminated by caller). */
void vialrgb_get_oled_config_user(uint8_t item, char *name_out) {
    const char *src = NULL;
    if (item == 0xFF) {
        src = g_oled_config.row1;
    } else if (item < 10) {
        src = g_oled_config.layer_names[item];
    }
    if (src) {
        strncpy(name_out, src, 5);
        name_out[5] = '\0';
    }
}

void vialrgb_set_oled_config_user(uint8_t item, const char *name_in) {
    char *dst = NULL;
    if (item == 0xFF) {
        dst = g_oled_config.row1;
    } else if (item < 10) {
        dst = g_oled_config.layer_names[item];
    }
    if (dst) {
        strncpy(dst, name_in, 5);
        dst[5] = '\0';
        save_oled_config();
    }
}

/* Called by vialrgb_save() when the user presses Save in Vial.
 * Persists g_direct_mode_colors to EEPROM so Customise colors survive reboot
 * without needing Vial to restore them.  Both halves load these bytes on boot
 * (see keyboard_post_init_user), so the slave also gets the right colors after
 * the first 16-ms live-sync cycle. */
void vialrgb_save_user(void) {
    eeprom_write_word((uint16_t *)EEPROM_VIALRGB_COLORS_MAGIC_OFFSET,
                      VIALRGB_COLORS_EEPROM_MAGIC);
    eeprom_write_block(g_direct_mode_colors,
                       (void *)EEPROM_VIALRGB_COLORS_DATA_OFFSET,
                       RGB_MATRIX_LED_COUNT * sizeof(HSV));
    eeprom_write_word((uint16_t *)EEPROM_INDICATOR_MAGIC_OFFSET,
                      INDICATOR_EEPROM_MAGIC);
    eeprom_write_block(&g_indicator_config,
                       (void *)EEPROM_INDICATOR_DATA_OFFSET,
                       sizeof(indicator_config_t));
}
#endif

static void tp_info_sync_handler(uint8_t in_buflen, const void *in_data,
                                  uint8_t out_buflen, void *out_data) {
    if (in_buflen >= sizeof(tp_info_payload_t)) {
        memcpy(&g_tp_info_data, in_data, sizeof(tp_info_payload_t));
        g_slave_sync_valid = true;
        memcpy(g_oled_config.row1, g_tp_info_data.row1, sizeof(g_tp_info_data.row1));
        memcpy(g_oled_config.layer_names, g_tp_info_data.layer_names, sizeof(g_tp_info_data.layer_names));
        if (g_tp_info_data.show_overlay) {
            tp_info_active = true;
            tp_info_timer  = timer_read32();
        }
    }
}

// ==================== Housekeeping ====================

void housekeeping_task_user(void) {
    static layer_state_t state = 0;
    if (layer_state != state) {
        state = layer_state_set_user(layer_state);
    }

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
    if (is_keyboard_master()) {
        static uint32_t last_vialrgb_sync = 0;
        if (timer_elapsed32(last_vialrgb_sync) >= 16) {
            last_vialrgb_sync = timer_read32();
            /* Send the slave's half of g_direct_mode_colors to slave.
             * Packet: [start_idx (1 byte)] [HSV colors for slave half].
             * Embedding start_idx lets the slave write to the correct position
             * without relying on is_keyboard_left() on the slave side. */
            uint8_t slave_start = is_keyboard_left() ? VIALRGB_SPLIT_LEFT : 0;
            static uint8_t sync_buf[1 + VIALRGB_SPLIT_LEFT * sizeof(HSV)];
            sync_buf[0] = slave_start;
            memcpy(&sync_buf[1], &g_direct_mode_colors[slave_start], VIALRGB_SPLIT_LEFT * sizeof(HSV));
            transaction_rpc_exec(VIALRGB_DIRECT_SYNC, sizeof(sync_buf), sync_buf, 0, NULL);
        }
        /* Sync indicator config to slave: immediately on change, and every 3s as fallback
         * (catches boot-time failures where slave wasn't ready on first attempt). */
        static uint32_t last_indicator_sync = 0;
        if (g_indicator_sync_needed || timer_elapsed32(last_indicator_sync) >= 3000) {
            if (transaction_rpc_exec(VIALRGB_INDICATOR_SYNC,
                                     sizeof(g_indicator_config), &g_indicator_config,
                                     0, NULL)) {
                g_indicator_sync_needed = false;
                last_indicator_sync = timer_read32();
            }
        }
    }
#endif

    /* Auto-sync: push master trackpad/OS state to slave every 500ms for live OLED display */
    if (is_keyboard_master()) {
        static uint32_t last_auto_sync = 0;
        if (timer_elapsed32(last_auto_sync) >= 500) {
            uint8_t layer = get_highest_layer(layer_state);
            tp_info_payload_t payload = {
                .os_variant        = (uint8_t)detected_host_os(),
                .ptp_mode          = !digitizer_send_mouse_reports,
                .trackpad_on       = trackpad_enabled,
                .sniper_active     = sniper_mode_active,
                .dpi               = (uint8_t)digitizer_get_mouse_scale(),
                .sniper_dpi        = (uint8_t)digitizer_get_sniper_scale(),
                .scroll_spd        = (uint8_t)scroll_speed,
                .gesture_mode      = (user_config.scroll_layers  & (1 << layer)) ? 1 :
                                     (user_config.swipe2_layers  & (1 << layer)) ? 2 :
                                     (user_config.swipe3_layers  & (1 << layer)) ? 3 : 0,
                .show_overlay      = false,
                .os_detect_enabled = os_detection_enabled,
            };
            memcpy(payload.row1, g_oled_config.row1, sizeof(payload.row1));
            memcpy(payload.layer_names, g_oled_config.layer_names, sizeof(payload.layer_names));
            transaction_rpc_exec(TP_INFO_SYNC, sizeof(payload), &payload, 0, NULL);
            last_auto_sync = timer_read32();
        }
    }

    /* TP_INFO keypress: immediate sync + show overlay on slave for 5s */
    if (is_keyboard_master() && g_tp_info_send_pending) {
        uint8_t layer = get_highest_layer(layer_state);
        tp_info_payload_t payload = {
            .os_variant        = (uint8_t)detected_host_os(),
            .ptp_mode          = !digitizer_send_mouse_reports,
            .trackpad_on       = trackpad_enabled,
            .sniper_active     = sniper_mode_active,
            .dpi               = (uint8_t)digitizer_get_mouse_scale(),
            .sniper_dpi        = (uint8_t)digitizer_get_sniper_scale(),
            .scroll_spd        = (uint8_t)scroll_speed,
            .gesture_mode      = (user_config.scroll_layers  & (1 << layer)) ? 1 :
                                 (user_config.swipe2_layers  & (1 << layer)) ? 2 :
                                 (user_config.swipe3_layers  & (1 << layer)) ? 3 : 0,
            .show_overlay      = true,
            .os_detect_enabled = os_detection_enabled,
        };
        memcpy(payload.row1, g_oled_config.row1, sizeof(payload.row1));
        memcpy(payload.layer_names, g_oled_config.layer_names, sizeof(payload.layer_names));
        if (transaction_rpc_exec(TP_INFO_SYNC, sizeof(payload), &payload, 0, NULL)) {
            g_tp_info_send_pending = false;
        }
    }

    /* ---- Runtime bootloader trigger ----
     * Hold the 3 outermost top-row keys on this half for 3 s to enter the bootloader.
     *   Left half:  KC_GRAVE + KC_1 + KC_2  (matrix r0c0, r0c1, r0c2)
     *   Right half: MS_BTN1 + MS_UP + KC_9  (matrix r5c0, r5c1, r5c2)
     * Each MCU checks its own keys and jumps independently.
     * QMK stores each half's matrix at the correct global row offset on both
     * master and slave, so matrix_is_on() works identically on both sides. */
    {
        static uint32_t bl_hold_start = 0;
        bool bl_trigger;
        if (is_keyboard_left()) {
            bl_trigger = matrix_is_on(0, 0) && matrix_is_on(0, 1) && matrix_is_on(0, 2);
        } else {
            bl_trigger = matrix_is_on(5, 0) && matrix_is_on(5, 1) && matrix_is_on(5, 2);
        }
        if (bl_trigger) {
            if (bl_hold_start == 0) bl_hold_start = timer_read32();
            else if (timer_elapsed32(bl_hold_start) >= 2000) bootloader_jump();
        } else {
            bl_hold_start = 0;
        }
    }
}

// ==================== RGB Layer Indicators ====================

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t brightness = rgb_matrix_get_val();
    if (brightness == 0) brightness = 1;
    #define SCALE(c) (((c) * brightness / 255) ?: 1)

    bool set = false;

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
    /* Each role has its own LED; caps lock and layer can both be lit simultaneously. */

    bool caps = host_keyboard_led_state().caps_lock;
    uint8_t layer = get_highest_layer(layer_state);
    /* Each role has a bitmask; multiple roles can share the same LED. */
    for (uint8_t led = led_min; led < led_max; led++) {
        if (led >= 64) continue;
        uint64_t bit = (1ULL << led);
        uint8_t r = 0, g = 0, b = 0;
        bool active = false;
        /* Layer indicator (lower priority) */
        if (layer >= 1 && layer < INDICATOR_NUM_ROLES &&
                (g_indicator_config.role_masks[layer] & bit)) {
            r = g_indicator_config.colors[layer][0];
            g = g_indicator_config.colors[layer][1];
            b = g_indicator_config.colors[layer][2];
            active = true;
        }
        /* Caps Lock overrides layer */
        if (caps && (g_indicator_config.role_masks[0] & bit)) {
            r = g_indicator_config.colors[0][0];
            g = g_indicator_config.colors[0][1];
            b = g_indicator_config.colors[0][2];
            active = true;
        }
        if (active) {
            rgb_matrix_set_color(led, SCALE(r), SCALE(g), SCALE(b));
            set = true;
        }
    }

#else
    /* Fallback: hardcoded single-LED-per-role */
    static const uint8_t fallback_leds[10]  = {4,   5,   6,  15,  16,  33,  34,  35,  44,  45};
    static const uint8_t fallback_r[10]     = {128, 128, 255,   0, 255,   0, 128, 255,   0, 255};
    static const uint8_t fallback_g[10]     = {0,     0, 215, 128, 128,   0,   0, 192, 255, 255};
    static const uint8_t fallback_b[10]     = {0,   128,   0, 128,   0, 128, 128, 203, 127, 255};

    uint8_t role = 0xFF;
    if (host_keyboard_led_state().caps_lock) {
        role = 0;
    } else {
        uint8_t layer = get_highest_layer(layer_state);
        if (layer >= 1 && layer <= 9) role = layer;
    }
    if (role != 0xFF) {
        uint8_t idx = fallback_leds[role];
        if (idx >= led_min && idx < led_max) {
            rgb_matrix_set_color(idx, SCALE(fallback_r[role]), SCALE(fallback_g[role]), SCALE(fallback_b[role]));
            set = true;
        }
    }
#endif

    #undef SCALE
    return set;
}

// ==================== Function Prototypes ====================

void keyboard_post_init_user(void);
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report);
bool process_record_user(uint16_t keycode, keyrecord_t *record);

// ==================== DIP Switch ====================

#if defined(DIP_SWITCH_ENABLE)
static uint16_t dip_switch_keycode = KC_NO;

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        keypos_t kp = { .row = 4, .col = 6 };

        if (active) {
            uint16_t kc = keymap_key_to_keycode(get_highest_layer(layer_state), kp);
            dip_switch_keycode = kc;

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
            uint16_t kc = dip_switch_keycode;
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
                dip_switch_keycode = KC_NO;
            }
        }
    }
    return true;
}
#endif

// ==================== Keymaps ====================

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        KC_GRAVE, KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_MINUS,
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
    load_taps_as_clicks();
    load_trackpad_enabled();
    load_oled_config();
    os_detection_settings_init();

    transaction_register_rpc(TP_INFO_SYNC, tp_info_sync_handler);

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
    transaction_register_rpc(VIALRGB_DIRECT_SYNC, vialrgb_direct_sync_handler);
    transaction_register_rpc(VIALRGB_INDICATOR_SYNC, vialrgb_indicator_sync_handler);

    /* Load saved per-key colors from EEPROM. */
    if (eeprom_read_word((const uint16_t *)EEPROM_VIALRGB_COLORS_MAGIC_OFFSET)
            == VIALRGB_COLORS_EEPROM_MAGIC) {
        eeprom_read_block(g_direct_mode_colors,
                          (const void *)EEPROM_VIALRGB_COLORS_DATA_OFFSET,
                          RGB_MATRIX_LED_COUNT * sizeof(HSV));
    }
    /* Initialize indicator defaults first, then override with EEPROM if valid. */
    indicator_config_init_defaults();
    if (eeprom_read_word((const uint16_t *)EEPROM_INDICATOR_MAGIC_OFFSET)
            == INDICATOR_EEPROM_MAGIC) {
        eeprom_read_block(&g_indicator_config,
                          (const void *)EEPROM_INDICATOR_DATA_OFFSET,
                          sizeof(indicator_config_t));
    }
#endif
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
        zoom_cleanup();
        mouse_report.x = 0;
        mouse_report.y = 0;
        mouse_report.h = 0;
        mouse_report.v = 0;
        mouse_report.buttons = 0;
        return mouse_report;
    }

    // Handle hardware zoom gestures (OS-aware)
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
        zoom_cleanup();
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

// ==================== Layer-based Trackpad Behavior ====================
//
// Called by digitizer_mouse_fallback.c BEFORE host_mouse_send().
// Implements per-layer 1-finger gesture redirection for mouse fallback mode only.
// PTP mode (Windows): this function is never called (digitizer_send_mouse_reports=false).
//
// scroll_layer:  1-finger movement → 2-finger-style scroll (x→h, y→v)
// swipe2_layer:  1-finger horizontal movement → browser back/forward (Cmd+[/])
// swipe3_layer:  1-finger movement → 3-finger swipe keycodes (desktop switch, etc.)
//
// ROTATION_270 axis mapping (flip_x=true, switch_xy=true):
//   Physical LEFT/RIGHT → report->y  (positive=left, negative=right — opposite of intuition)
//   Physical UP/DOWN    → report->x  (positive=up, negative=down)
//
// The sub-pixel accumulators are reset on finger-lift to prevent carry-over.
#ifndef DIGITIZER_SWIPE2_THRESHOLD
#    define DIGITIZER_SWIPE2_THRESHOLD 40  // cursor pixels to trigger back/forward
#endif
#ifndef DIGITIZER_SWIPE3_THRESHOLD
#    define DIGITIZER_SWIPE3_THRESHOLD 50  // cursor pixels to trigger swipe keycode
#endif

void digitizer_pre_send_user(report_mouse_t *report) {
    // Merge keyboard-side mouse buttons (e.g. DIP center = BTN1) into the direct-send report.
    // digitizer_mouse_fallback.c calls host_mouse_send() before QMK's mousekey OR in
    // pointing_device_task(), so we must query mousekey state here directly. Saving it
    // via pointing_device_task_user() doesn't work because that callback fires before
    // the mousekey OR (see pointing_device.c: task_kb() called before mousekey_get_report()).
#ifdef MOUSEKEY_ENABLE
    report->buttons |= mousekey_get_report().buttons;
#endif

    // Per-mode state (file-scope so we can reset on finger-lift)
    static int     scroll_carry_h   = 0;
    static int     scroll_carry_v   = 0;
    static int     swipe2_accum     = 0;
    static int     swipe3_h         = 0;
    static int     swipe3_v         = 0;

    // Reset accumulators when finger lifts (contacts != 1) to prevent
    // a partial swipe from carrying over to the next gesture.
    if (digitizer_active_contacts != 1) {
        scroll_carry_h = 0; scroll_carry_v = 0;
        swipe2_accum   = 0;
        swipe3_h       = 0; swipe3_v = 0;
        return;
    }

    uint8_t  layer     = get_highest_layer(layer_state);
    uint16_t layer_bit = (1 << layer);

    if (user_config.scroll_layers & layer_bit) {
        // --- 1-finger scroll ---
        // ROTATION_270: report->x = horizontal axis, report->y = vertical axis
        // Both axes negated: sensor reports inverted sign relative to scroll direction
        int sh_acc = -(int)report->x * (int)digitizer_get_scroll_scale() + scroll_carry_h;
        int sv_acc = -(int)report->y * (int)digitizer_get_scroll_scale() + scroll_carry_v;
        scroll_carry_h   = sh_acc % 64;
        scroll_carry_v   = sv_acc % 64;
        int _sh = sh_acc / 64; report->h = _sh < -127 ? -127 : _sh > 127 ? 127 : _sh;
        int _sv = sv_acc / 64; report->v = _sv < -127 ? -127 : _sv > 127 ? 127 : _sv;
        report->x = 0;
        report->y = 0;

    } else if (user_config.swipe2_layers & layer_bit) {
        // --- 1-finger horizontal → browser back/forward ---
        // ROTATION_270: report->x = horizontal; negate because RIGHT→negative x
        swipe2_accum -= (int)report->x;
        if (swipe2_accum >= DIGITIZER_SWIPE2_THRESHOLD) {
            tap_code16(LGUI(KC_RBRC));  // Cmd+] = forward
            swipe2_accum = 0;
        } else if (swipe2_accum <= -DIGITIZER_SWIPE2_THRESHOLD) {
            tap_code16(LGUI(KC_LBRC));  // Cmd+[ = back
            swipe2_accum = 0;
        }
        report->x = 0;
        report->y = 0;

    } else if (user_config.swipe3_layers & layer_bit) {
        // --- 1-finger → 3-finger swipe keycodes ---
        // ROTATION_270: report->x = horizontal (RIGHT→negative), report->y = vertical (UP→negative)
        swipe3_h -= (int)report->x;          // negate: RIGHT→positive h → SWIPE_RIGHT_KC ✓
        swipe3_v += (int)report->y;          // UP→negative y → negative v → SWIPE_UP_KC ✓
        if (swipe3_h >= DIGITIZER_SWIPE3_THRESHOLD) {
            tap_code16(DIGITIZER_SWIPE_RIGHT_KC);
            swipe3_h = 0; swipe3_v = 0;
        } else if (swipe3_h <= -DIGITIZER_SWIPE3_THRESHOLD) {
            tap_code16(DIGITIZER_SWIPE_LEFT_KC);
            swipe3_h = 0; swipe3_v = 0;
        } else if (swipe3_v >= DIGITIZER_SWIPE3_THRESHOLD) {
            tap_code16(DIGITIZER_SWIPE_DOWN_KC);
            swipe3_h = 0; swipe3_v = 0;
        } else if (swipe3_v <= -DIGITIZER_SWIPE3_THRESHOLD) {
            tap_code16(DIGITIZER_SWIPE_UP_KC);
            swipe3_h = 0; swipe3_v = 0;
        }
        report->x = 0;
        report->y = 0;
    }
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
                if (!trackpad_enabled) zoom_cleanup();
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
                if (!zoom_enabled) zoom_cleanup();
                save_zoom_setting();
            }
            break;

        case AP_GLOB:
            if (record->event.pressed) {
                host_consumer_send(AC_NEXT_KEYBOARD_LAYOUT_SELECT);
            } else {
                host_consumer_send(0);
            }
            return false;

        case TP_INFO:
            if (record->event.pressed) {
                g_tp_info_send_pending = true;
            }
            return false;
    }

    return true;
}

// ==================== Suspend / Wakeup ====================

// Called by QMK after the host wakes the keyboard from USB suspend.
// Release any zoom modifier that was registered before suspend; the OS may
// have lost the key-down event and clear_mods() ensures our internal state
// matches reality after resume.
void suspend_wakeup_init_user(void) {
    zoom_cleanup();
    clear_mods();
}

// ==================== OLED ====================
#ifdef OLED_ENABLE

#include "oled_data.h"


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
static const char* get_trackpad_gesture_bitmap_by_mode(uint8_t mode) {
    switch (mode) {
        case 1: return gesture_scroll;
        case 2: return gesture_2finger;
        case 3: return gesture_3finger;
        default: return gesture_default;
    }
}

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
        oled_set_cursor(0, 12);
        oled_write_raw_P((const char*)bitmap, 128);
    }
}

// Determine if we are in PTP mode (Windows/Linux sends PTP feature report)
static bool is_ptp_mode(void) {
    return !digitizer_send_mouse_reports;
}

static void print_status_narrow(void) {
    uint8_t current_layer = get_highest_layer(layer_state);
    bool    is_slave      = !is_keyboard_master();

    /* Compute display values: live on master, synced on slave via auto-sync */
    uint8_t d_os         = is_slave ? g_tp_info_data.os_variant    : (uint8_t)detected_host_os();
    bool    d_ptp        = is_slave ? g_tp_info_data.ptp_mode       : is_ptp_mode();
    bool    d_trackpad   = is_slave ? g_tp_info_data.trackpad_on    : trackpad_enabled;
    uint8_t d_gesture    = is_slave ? g_tp_info_data.gesture_mode   :
                           (user_config.scroll_layers & (1 << current_layer)) ? 1 :
                           (user_config.swipe2_layers & (1 << current_layer)) ? 2 :
                           (user_config.swipe3_layers & (1 << current_layer)) ? 3 : 0;
    uint8_t d_dpi        = is_slave ? g_tp_info_data.dpi            : (uint8_t)digitizer_get_mouse_scale();
    uint8_t d_sniper_dpi = is_slave ? g_tp_info_data.sniper_dpi     : (uint8_t)digitizer_get_sniper_scale();
    uint8_t d_scroll     = is_slave ? g_tp_info_data.scroll_spd     : (uint8_t)scroll_speed;
    bool    d_sniper     = is_slave ? g_tp_info_data.sniper_active  : sniper_mode_active;
    bool    d_learn      = is_slave ? false : sniper_learning_mode;  // transient, not synced
    bool    d_info       = is_slave ? false : sniper_info_mode;      // transient, not synced
    bool    d_os_enabled = is_slave ? g_tp_info_data.os_detect_enabled : os_detection_enabled;

    /* Row 1: configurable keyboard name (default "SOFLE") */
    oled_set_cursor(0, 1);
    for (uint8_t _i = 0; _i < 5; _i++)
        oled_write_char(g_oled_config.row1[_i] ? g_oled_config.row1[_i] : ' ', false);

    /* Row 2: OS detection status */
    oled_set_cursor(0, 2);
    if (is_slave && !g_slave_sync_valid) {
        oled_write_P(PSTR("SYNC "), false);
    } else if (!d_os_enabled) {
        oled_write("PLUS+", false);
    } else {
        switch ((os_variant_t)d_os) {
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

    /* Row 3: Numlock indicator (synced automatically by QMK split) */
    oled_set_cursor(0, 3);
    led_t led_usb_state = host_keyboard_led_state();
    if (led_usb_state.num_lock) {
        oled_write_P(PSTR("NUMLK"), false);
    } else {
        oled_write_P(PSTR("     "), false);
    }

    /* Row 4: Gesture bitmap */
    oled_set_cursor(0, 4);
    if (d_ptp) {
        oled_write_raw_P(gesture_blank, 64);
    } else if (d_trackpad && !d_learn && !d_info) {
        oled_write_raw_P(get_trackpad_gesture_bitmap_by_mode(d_gesture), 64);
    } else {
        oled_write_raw_P(gesture_blank, 64);
    }

    /* Clear text rows 6-9 */
    oled_set_cursor(0, 6); oled_write_P(PSTR("     "), false);
    oled_set_cursor(0, 7); oled_write_P(PSTR("     "), false);
    oled_set_cursor(0, 8); oled_write_P(PSTR("     "), false);
    oled_set_cursor(0, 9); oled_write_P(PSTR("     "), false);

    if (d_ptp) {
        oled_set_cursor(0, 6);
        oled_write_P(PSTR(" PTP "), false);
    } else if (d_trackpad) {
        if (d_learn) {
            oled_set_cursor(0, 6); oled_write_P(PSTR("LEARN"), false);
            oled_set_cursor(0, 7); oled_write_P(PSTR("SNIPE"), false);
            oled_set_cursor(0, 8); oled_write_P(PSTR("HOLD"), false);
            oled_set_cursor(0, 9); oled_write_P(PSTR("MODS"), false);
        } else {
            /* Gesture mode label */
            oled_set_cursor(0, 6);
            switch (d_gesture) {
                case 1: oled_write_P(PSTR("SCROL"), false); break;
                case 2: oled_write_P(PSTR("2SWPE"), false); break;
                case 3: oled_write_P(PSTR("3SWPE"), false); break;
                default: oled_write_P(PSTR("CURSR"), false); break;
            }

            /* Cursor speed (DPI) */
            oled_set_cursor(0, 8);
            if (d_sniper) {
                oled_write_P(PSTR("SNP"), false);
                oled_set_cursor(3, 8);
                char speed_str[4];
                snprintf(speed_str, sizeof(speed_str), "%d", d_sniper_dpi);
                oled_write(speed_str, false);
            } else {
                oled_write_P(PSTR("DPI"), false);
                oled_set_cursor(3, 8);
                char speed_str[4];
                snprintf(speed_str, sizeof(speed_str), "%d", d_dpi);
                oled_write(speed_str, false);
            }

            /* Scroll speed */
            oled_set_cursor(0, 9);
            oled_write_P(PSTR("SCR"), false);
            oled_set_cursor(3, 9);
            char scr_str[4];
            snprintf(scr_str, sizeof(scr_str), "%d", d_scroll);
            oled_write(scr_str, false);
        }
    } else {
        oled_set_cursor(0, 6); oled_write_P(PSTR("NO"), false);
        oled_set_cursor(0, 7); oled_write_P(PSTR("TRKPD"), false);
    }

    /* Sniper info display override (master only — transient modes not synced) */
    if (d_info) {
        oled_set_cursor(0, 6); oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 7); oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 8); oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 9); oled_write_P(PSTR("     "), false);
        oled_set_cursor(0, 6); oled_write_P(PSTR("SNIPE"), false);
        oled_set_cursor(0, 7); oled_write_P(PSTR("MODS:"), false);
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

    /* Row 10: 5-char layer name (standard font); row 11 blank spacer */
    oled_set_cursor(0, 10);
    {
        const char *_ln = g_oled_config.layer_names[current_layer < 10 ? current_layer : 0];
        for (uint8_t _i = 0; _i < 5; _i++)
            oled_write_char(_ln[_i] ? _ln[_i] : ' ', false);
    }

    /* Large layer number (rows 12-15; row 11 is a natural blank spacer) */
    display_large_layer_number(current_layer);
}

/* Landscape TP_INFO overlay for slave OLED (default rotation, 128x32, rows 0-3).
 * Shows a snapshot of master-side trackpad/OS state sent via TP_INFO keycode. */
static void print_tp_info_overlay(void) {
    oled_clear();
    uint8_t layer = get_highest_layer(layer_state);
    /* Row 0: keyboard name | OS | PTP/MOUSE */
    oled_set_cursor(0, 0);
    for (uint8_t i = 0; i < 5; i++)
        oled_write_char(g_oled_config.row1[i] ? g_oled_config.row1[i] : ' ', false);
    oled_write_char(' ', false);
    switch ((os_variant_t)g_tp_info_data.os_variant) {
        case OS_MACOS: case OS_IOS: oled_write_P(PSTR("MAC"), false); break;
        case OS_WINDOWS:            oled_write_P(PSTR("WIN"), false); break;
        case OS_LINUX:              oled_write_P(PSTR("LNX"), false); break;
        default:                    oled_write_P(PSTR("???"), false); break;
    }
    oled_write_char(' ', false);
    oled_write_P(g_tp_info_data.ptp_mode ? PSTR("PTP  ") : PSTR("MOUSE"), false);
    /* Row 1: layer number + name | num lock */
    oled_set_cursor(0, 1);
    oled_write_P(PSTR("L:"), false);
    oled_write_char('0' + (layer < 10 ? layer : 0), false);
    oled_write_char(' ', false);
    { const char *ln = g_oled_config.layer_names[layer < 10 ? layer : 0];
      for (uint8_t i = 0; i < 5; i++) oled_write_char(ln[i] ? ln[i] : ' ', false); }
    oled_write_char(' ', false);
    if (host_keyboard_led_state().num_lock) oled_write_P(PSTR("NUMLK"), false);
    /* Row 2: gesture mode | DPI | scroll speed */
    oled_set_cursor(0, 2);
    if (g_tp_info_data.ptp_mode) {
        oled_write_P(PSTR("PTP mode active"), false);
    } else if (g_tp_info_data.trackpad_on) {
        switch (g_tp_info_data.gesture_mode) {
            case 1: oled_write_P(PSTR("SCROL"), false); break;
            case 2: oled_write_P(PSTR("2SWPE"), false); break;
            case 3: oled_write_P(PSTR("3SWPE"), false); break;
            default: oled_write_P(PSTR("CURSR"), false); break;
        }
        oled_write_char(' ', false);
        oled_write_P(g_tp_info_data.sniper_active ? PSTR("SNP:") : PSTR("DPI:"), false);
        char s[4]; snprintf(s, sizeof(s), "%d", g_tp_info_data.sniper_active
                                                  ? g_tp_info_data.sniper_dpi
                                                  : g_tp_info_data.dpi);
        oled_write(s, false);
        oled_write_P(PSTR(" SCR:"), false);
        char sc[4]; snprintf(sc, sizeof(sc), "%d", g_tp_info_data.scroll_spd);
        oled_write(sc, false);
    } else {
        oled_write_P(PSTR("NO TRACKPAD"), false);
    }
    /* Row 3: empty */
}

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return OLED_ROTATION_270;
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
            } else if (tp_info_active && timer_elapsed32(tp_info_timer) < TP_INFO_DURATION_MS) {
                print_tp_info_overlay();
            } else {
                tp_info_active = false;
                print_status_narrow();
            }
        }
        if (!is_oled_on() && last_input_activity_elapsed() < 1000) {
            oled_on();
        }
    }
    return false;
}

#endif

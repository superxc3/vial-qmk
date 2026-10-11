/*
Copyright 2019 @foostan
Copyright 2020 Drashna Jaelre <@drashna>
Copyright 2026 XCMKB — trackpad/VialRGB indicator/OLED status port from sofleplus2

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include QMK_KEYBOARD_H
#include "via.h"
#include "timer.h"
#include "rgb_matrix.h"
#include "eeconfig.h"
#include "os_detection.h"
#include "platforms/eeprom.h"
#include "digitizer_mouse_fallback.h"

#ifdef OLED_ENABLE
    void suspend_power_down_user(void) {
        oled_off();
    }
#endif

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
#include "transactions.h"
#include "vialrgb.h"   /* VIALRGB_INDICATOR_LED_COLORS_MAX, per-LED indicator hook prototypes */

/* Number of LEDs on the LEFT half (keyboard.json rgb_matrix.split_count[0]).
 * LED indices 0..(SPLIT_LEFT-1) are left side; SPLIT_LEFT..(total-1) are right side.
 * This is independent of which physical side is currently master. */
#define VIALRGB_SPLIT_LEFT 27u

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

#ifdef SUPER_ALT_TAB_ENABLE
    bool is_alt_tab_active = false; // Super Alt Tab Code
    uint16_t alt_tab_timer = 0;
#endif

// ==================== Custom Keycodes ====================

#ifdef VIA_ENABLE
    enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
    CK_ATABF = QK_KB_0,
    CK_ATABR,
    CK_ATMWU,   //Alt mouse wheel up
    CK_ATMWD,   //Alt mouse wheel down
    CK_PO,      //Power options
    /* Kept in the Vial "Custom" keycode list — keep these three first so their
       QK_KB indices stay aligned with vial.json customKeycodes (positional map). */
    OS_DETECTION_TOGGLE,
    AP_GLOB,
    TP_INFO,
    /* Below: still handled in process_record_user, but hidden from the Vial GUI
       (these settings live in the Trackpad tab). Not listed in vial.json. */
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
    ZMTOG,
    };
#else
    enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
    CK_ATABF = SAFE_RANGE,
    CK_ATABR,
    CK_ATMWU,
    CK_ATMWD,
    CK_PO,
    OS_DETECTION_TOGGLE,
    AP_GLOB,
    TP_INFO,
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

/* Shared OLED + RGB sleep timeout (ms), runtime-settable from Vial and
 * persisted to EEPROM. Drives OLED sleep here and RGB sleep via the core
 * g_rgb_matrix_timeout (kept in sync by apply_sleep_timeout()). */
uint32_t g_sleep_timeout = DEFAULT_SLEEP_TIMEOUT_MS;
static void apply_sleep_timeout(void) { g_rgb_matrix_timeout = g_sleep_timeout; }

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
static bool zoom_active     = false;
static bool zoom_using_cmd  = false;

bool digitizer_zoom_enabled(void) { return zoom_enabled; }

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

// EEPROM Address Map. The EEPROM is 32 KB (WEAR_LEVELING_LOGICAL_SIZE in
// config.h) and Vial's own regions -- keymap, encoders, QMK settings, tap dance,
// combos, key overrides, alt repeat keys, macros -- end at
// DYNAMIC_KEYMAP_EEPROM_MAX_ADDR = 0x0F9F, directly below this block. Both
// were missing before 2026-09-25: the EEPROM was the RP2040 default 4 KB, so
// everything here from 0x1000 up failed to save, and the macro buffer ran
// across 0x0FA0 onward. The _Static_asserts after oled_config_t hold the map.
// eeconfig_user (32-bit): cursor_speed[0:15], scroll_dir_v[16], scroll_dir_h[17], scroll_speed[18:23]
// 0x0FA0-0x0FA7: trackpad layer config
// 0x0FB0: sniper_modifier_mask
// 0x0FB1: sniper_scale_level
// 0x0FB2: zoom toggle
// 0x0FB3: os_detection_enabled
// 0x0FB4: taps_as_clicks (0=off, 1=on; 0xFF=unset→default off)
// 0x0FB5: trackpad_enabled (0=off, 1=on; 0xFF=unset→default on)
// 0x0FB8-0x0FBB: shared OLED+RGB sleep timeout, uint32 ms (moved 2026-09-25, see below)
// 0x0FC0-0x0FC1: VialRGB colors magic (0x5243 = 'RC')
// 0x0FC2-0x1063: g_direct_mode_colors (RGB_MATRIX_LED_COUNT * sizeof(HSV) = 162 bytes for 54 LEDs)
// 0x1070-0x1071: Indicator config magic (0x494Du = 'IM')
// 0x1072-0x10DF: indicator_config_t (110 bytes: role_masks[10]*8 + colors[10][3])
// 0x10E0-0x10E1: OLED config magic (0x4F4Cu = 'OL')
// 0x10E2-0x1123: oled_config_t (66 bytes)
//
// The sleep timeout used to sit at 0x10DC, documented as following an
// indicator_config_t that "ended at 0x10DB" -- but 0x1072 + 110 = 0x10E0, so it
// was the struct's last four bytes and every indicator save overwrote it. Same
// bug and same fix as sofleplus2 (2026-09-02): moved into the free 0x0FB6-0x0FBF gap.
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
#define EEPROM_SLEEP_TIMEOUT_OFFSET        0x0FB8u  /* shared OLED+RGB sleep timeout (ms); was 0x10DC, inside indicator_config_t */
#define EEPROM_OLED_MAGIC_OFFSET           0x10E0u
#define EEPROM_OLED_DATA_OFFSET            0x10E2u
#define OLED_EEPROM_MAGIC                  0x4F4Cu  /* 'O','L' — OLED labels */
/* 0x1130-0x1131: per-LED indicator colour magic; 0x1132-...: [role][led][r,g,b]
 * (10 x RGB_MATRIX_LED_COUNT x 3 = 1620 bytes for 54 LEDs, ends 0x1785). */
#define EEPROM_INDICATOR_LED_MAGIC_OFFSET  0x1130u
#define EEPROM_INDICATOR_LED_DATA_OFFSET   0x1132u
#define INDICATOR_LED_EEPROM_MAGIC         0x4943u  /* 'I','C' — per-LED indicator colours */

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
    .swipe3_layers = 0,
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

/* Per-LED colour for each role (0x48): a role's LEDs come from role_masks, and each
 * one is drawn in its own colour from here. g_indicator_config.colors[role] stays as
 * the role's default/brush colour; a 0x46 SET repaints the whole role with it. */
static uint8_t g_indicator_led_colors[INDICATOR_NUM_ROLES][RGB_MATRIX_LED_COUNT][3];

/* Roles whose per-LED colours still need pushing to the slave (bit = role). */
static uint16_t g_indicator_led_dirty = (1u << INDICATOR_NUM_ROLES) - 1;

static void indicator_fill_role_leds(uint8_t role) {
    for (uint8_t led = 0; led < RGB_MATRIX_LED_COUNT; led++) {
        memcpy(g_indicator_led_colors[role][led], g_indicator_config.colors[role], 3);
    }
    g_indicator_led_dirty |= (1u << role);
}

/* OLED label config — row1 + one name per layer, all max 5 chars + null */
typedef struct {
    char row1[6];            /* keyboard name shown on OLED row 1 (default "CORNE") */
    char layer_names[10][6]; /* one 5-char label per layer (default "L0".."L9")   */
} __attribute__((packed)) oled_config_t;

/* EEPROM map guards (see the address map above EEPROM_SNIPER_SETTINGS_OFFSET). */
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR < EECONFIG_USER_TRACKPAD_OFFSET,
               "Vial macro buffer runs into the keymap's settings block");
_Static_assert(EECONFIG_USER_TRACKPAD_OFFSET + 8 <= EEPROM_SNIPER_SETTINGS_OFFSET,
               "trackpad layer config overlaps the sniper settings");
_Static_assert(EEPROM_TRACKPAD_ENABLED_OFFSET < EEPROM_SLEEP_TIMEOUT_OFFSET &&
               EEPROM_SLEEP_TIMEOUT_OFFSET + 4 <= EEPROM_VIALRGB_COLORS_MAGIC_OFFSET,
               "sleep timeout is not in the free 0x0FB6-0x0FBF gap");
_Static_assert(EEPROM_VIALRGB_COLORS_DATA_OFFSET + RGB_MATRIX_LED_COUNT * sizeof(HSV) <= EEPROM_INDICATOR_MAGIC_OFFSET,
               "VialRGB direct colours overlap the indicator block");
_Static_assert(EEPROM_INDICATOR_DATA_OFFSET + sizeof(indicator_config_t) <= EEPROM_OLED_MAGIC_OFFSET,
               "indicator block overlaps the OLED block");
_Static_assert(EEPROM_OLED_DATA_OFFSET + sizeof(oled_config_t) <= EEPROM_INDICATOR_LED_MAGIC_OFFSET,
               "OLED block overlaps the per-LED indicator colours");
_Static_assert(EEPROM_INDICATOR_LED_DATA_OFFSET + sizeof(g_indicator_led_colors) <= WEAR_LEVELING_LOGICAL_SIZE,
               "per-LED indicator colours run past the end of the EEPROM");

static oled_config_t g_oled_config = {
    .row1 = "CORNE",
    .layer_names = {
        "L0   ", "LOWER", "RAISE", "ADJST", "L4   ",
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
    /* Defaults, from keyboard.json's rgb_matrix.layout (27 LEDs per half):
     *   0-5   left underglow     27-32 right underglow
     *   14    left thumb [3][3] (outermost thumb, KC_LGUI on layer 0)
     * Caps Lock lights the left GUI thumb; every layer role 1-9 lights the 12
     * underglow LEDs, so whichever layer is highest tints the underglow. Only
     * one layer role is ever active, so sharing the same LEDs is safe.
     * Reassign freely in vial-web's indicator tab. */
    g_indicator_config.role_masks[0] = (1ULL << 14);
    const uint64_t underglow = 0x3FULL | (0x3FULL << 27);
    for (uint8_t i = 1; i < INDICATOR_NUM_ROLES; i++) {
        g_indicator_config.role_masks[i] = underglow;
    }
    static const uint8_t dr[INDICATOR_NUM_ROLES] = {128, 128, 255,   0, 255,   0, 128, 255,   0, 255};
    static const uint8_t dg[INDICATOR_NUM_ROLES] = {  0,   0, 215, 128, 128,   0,   0, 192, 255, 255};
    static const uint8_t db[INDICATOR_NUM_ROLES] = {  0, 128,   0, 128,   0, 128, 128, 203, 127, 255};
    for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) {
        g_indicator_config.colors[i][0] = dr[i];
        g_indicator_config.colors[i][1] = dg[i];
        g_indicator_config.colors[i][2] = db[i];
        indicator_fill_role_leds(i);
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
    uint32_t sleep_timeout;   // shared OLED+RGB sleep timeout (ms)
    char    row1[5];          // keyboard name (e.g. "CORNE")
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
        /* One colour per role (old protocol): repaint the role's LEDs with it. Hosts
         * that know 0x48 send their per-LED colours after this. */
        indicator_fill_role_leds(i);
    }
    g_indicator_sync_needed = true;
}

bool vialrgb_get_indicator_led_colors_user(uint8_t role, uint8_t first, uint8_t *count_out, uint8_t *rgb_out) {
    *count_out = 0;
    if (role < INDICATOR_NUM_ROLES && first < RGB_MATRIX_LED_COUNT) {
        uint8_t n = RGB_MATRIX_LED_COUNT - first;
        if (n > VIALRGB_INDICATOR_LED_COLORS_MAX) n = VIALRGB_INDICATOR_LED_COLORS_MAX;
        memcpy(rgb_out, g_indicator_led_colors[role][first], n * 3);
        *count_out = n;
    }
    return true;  /* supported, even for an out-of-range request (count 0) */
}

void vialrgb_set_indicator_led_colors_user(uint8_t role, uint8_t first, uint8_t count, const uint8_t *rgb_in) {
    if (role >= INDICATOR_NUM_ROLES || first >= RGB_MATRIX_LED_COUNT) return;
    if (count > RGB_MATRIX_LED_COUNT - first) count = RGB_MATRIX_LED_COUNT - first;
    memcpy(g_indicator_led_colors[role][first], rgb_in, count * 3);
    g_indicator_led_dirty |= (1u << role);
}

static void vialrgb_indicator_sync_handler(uint8_t in_buflen, const void *in_data,
                                            uint8_t out_buflen, void *out_data) {
    /* Slave receives a full indicator_config_t from master. */
    if (in_buflen < sizeof(indicator_config_t)) return;
    memcpy(&g_indicator_config, in_data, sizeof(indicator_config_t));
}

/* Per-LED indicator colours for the slave's half, one role per packet:
 * [role][start_led][r,g,b x VIALRGB_SPLIT_LEFT]. Same symmetric-halves assumption
 * as VIALRGB_DIRECT_SYNC. */
#define INDICATOR_LED_SYNC_LEN (2 + VIALRGB_SPLIT_LEFT * 3)
_Static_assert(INDICATOR_LED_SYNC_LEN <= RPC_M2S_BUFFER_SIZE,
               "per-LED indicator sync packet does not fit RPC_M2S_BUFFER_SIZE");
_Static_assert(RGB_MATRIX_LED_COUNT == 2 * VIALRGB_SPLIT_LEFT,
               "per-LED indicator sync assumes both halves have the same LED count");

static void vialrgb_indicator_led_sync_handler(uint8_t in_buflen, const void *in_data,
                                                uint8_t out_buflen, void *out_data) {
    const uint8_t *p = (const uint8_t *)in_data;
    if (in_buflen < INDICATOR_LED_SYNC_LEN) return;
    if (p[0] >= INDICATOR_NUM_ROLES || (uint16_t)p[1] + VIALRGB_SPLIT_LEFT > RGB_MATRIX_LED_COUNT) return;
    memcpy(g_indicator_led_colors[p[0]][p[1]], p + 2, VIALRGB_SPLIT_LEFT * 3);
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

/* Sub-ID 0x53 — shared OLED+RGB sleep timeout (whole minutes on the wire). */
void vialrgb_get_power_settings_user(uint8_t *args) {
    args[0] = (uint8_t)(g_sleep_timeout / 60000u);
}

void vialrgb_set_power_settings_user(const uint8_t *args) {
    uint32_t ms = (uint32_t)args[0] * 60000u;
    if (ms < MIN_SLEEP_TIMEOUT_MS) ms = MIN_SLEEP_TIMEOUT_MS;
    if (ms > MAX_SLEEP_TIMEOUT_MS) ms = MAX_SLEEP_TIMEOUT_MS;
    g_sleep_timeout = ms;
    apply_sleep_timeout();
    eeprom_write_dword((uint32_t *)EEPROM_SLEEP_TIMEOUT_OFFSET, g_sleep_timeout);
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
 * the first live-sync cycle. */
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
    eeprom_write_word((uint16_t *)EEPROM_INDICATOR_LED_MAGIC_OFFSET,
                      INDICATOR_LED_EEPROM_MAGIC);
    eeprom_write_block(g_indicator_led_colors,
                       (void *)EEPROM_INDICATOR_LED_DATA_OFFSET,
                       sizeof(g_indicator_led_colors));
}
#endif

static void tp_info_sync_handler(uint8_t in_buflen, const void *in_data,
                                  uint8_t out_buflen, void *out_data) {
    if (in_buflen >= sizeof(tp_info_payload_t)) {
        memcpy(&g_tp_info_data, in_data, sizeof(tp_info_payload_t));
        g_slave_sync_valid = true;
        memcpy(g_oled_config.row1, g_tp_info_data.row1, sizeof(g_tp_info_data.row1));
        memcpy(g_oled_config.layer_names, g_tp_info_data.layer_names, sizeof(g_tp_info_data.layer_names));
        /* Adopt master's shared sleep timeout so both halves sleep together */
        if (g_tp_info_data.sleep_timeout >= MIN_SLEEP_TIMEOUT_MS &&
            g_tp_info_data.sleep_timeout <= MAX_SLEEP_TIMEOUT_MS) {
            g_sleep_timeout = g_tp_info_data.sleep_timeout;
            apply_sleep_timeout();
        }
        if (g_tp_info_data.show_overlay) {
            tp_info_active = true;
            tp_info_timer  = timer_read32();
        }
    }
}

// ==================== Housekeeping ====================

static void aurora_task(void);   /* wake-up animation, defined with the RGB indicators below */

void housekeeping_task_user(void) {
    aurora_task();

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
        /* Per-LED indicator colours: one role per pass -- dirty roles first, then a
         * slow round-robin (one role per second) as the same boot-time fallback. */
        static uint32_t last_led_sync = 0, last_led_attempt = 0;
        static uint8_t  led_sync_next = 0;
        if ((g_indicator_led_dirty || timer_elapsed32(last_led_sync) >= 1000) &&
                timer_elapsed32(last_led_attempt) >= 20) {   /* don't hammer a missing slave */
            last_led_attempt = timer_read32();
            uint8_t role = led_sync_next;
            for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) {
                if (g_indicator_led_dirty & (1u << i)) { role = i; break; }
            }
            uint8_t slave_start = is_keyboard_left() ? VIALRGB_SPLIT_LEFT : 0;
            static uint8_t led_buf[INDICATOR_LED_SYNC_LEN];
            led_buf[0] = role;
            led_buf[1] = slave_start;
            memcpy(&led_buf[2], g_indicator_led_colors[role][slave_start], VIALRGB_SPLIT_LEFT * 3);
            if (transaction_rpc_exec(VIALRGB_INDICATOR_LED_SYNC, sizeof(led_buf), led_buf, 0, NULL)) {
                g_indicator_led_dirty &= ~(1u << role);
                last_led_sync = timer_read32();
                if (role == led_sync_next) led_sync_next = (led_sync_next + 1) % INDICATOR_NUM_ROLES;
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
                .sleep_timeout     = g_sleep_timeout,
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
            .sleep_timeout     = g_sleep_timeout,
        };
        memcpy(payload.row1, g_oled_config.row1, sizeof(payload.row1));
        memcpy(payload.layer_names, g_oled_config.layer_names, sizeof(payload.layer_names));
        if (transaction_rpc_exec(TP_INFO_SYNC, sizeof(payload), &payload, 0, NULL)) {
            g_tp_info_send_pending = false;
        }
    }
}

// ==================== Wake-up animation: Aurora tide ====================
/* Plays once, ~4.5 s, when the board is plugged in or the PC starts up: two broad
 * tides roll left -> right across both halves (sharp bright crest, long tail that
 * fades through teal to deep indigo, dark ahead of it), and the second one brings
 * the user's own effect in behind it. It never touches the saved RGB mode or
 * brightness, and it does not play when RGB is switched off (the indicator
 * callback it lives in does not run then).
 *
 * Timing is kept in each half's OWN timer (timer_read32), never sync_timer:
 *  - Plug-in: both halves get power at the same instant, so each simply runs the
 *    animation from its own power-on (start = 0). No message is needed, and a
 *    half that is slow to boot (the trackpad half) still lands on the right frame.
 *  - PC start-up while already powered (a fresh USB enumeration: CONFIGURED
 *    reached from INIT; waking from sleep comes from SUSPEND and is ignored): the
 *    master starts and sends the slave "ms elapsed since start" over AURORA_SYNC,
 *    re-sent until delivered. The slave converts that to its own clock in its main
 *    loop, so the two clocks never have to agree.
 *  Some hosts reset the bus on wake instead of resuming; that counts as fresh.
 *  A PC start enumerates twice (BIOS, then the OS), and plug-in is followed by an
 *  enumeration; AURORA_REPLAY_GUARD_MS swallows those.
 * AURORA_SETTLE_MS keeps LEDs dark while the trackpad takes its power-up baseline
 * (it shares the rail with the LEDs). */
#if defined(AURORA_WAKE_ENABLE) && defined(RGB_MATRIX_ENABLE)
#include "transactions.h"
#include <lib/lib8tion/lib8tion.h>

#define AURORA_SETTLE_MS        300
#define AURORA_TIDE_GAP_MS     1900    /* second tide starts this long after the first */
#define AURORA_SPEED_NUM         16    /* crest speed = 16/100 x-units per ms (~1.7 s across) */
#define AURORA_SPEED_DEN        100
#define AURORA_START_X          (-40)  /* crest starts just off the left edge */
#define AURORA_WASH_NEAR         30    /* behind the last crest: aurora fades out from here ... */
#define AURORA_WASH_FAR          60    /* ... and the user's effect shows from here */
#define AURORA_TOTAL_MS        4500    /* last crest past the right edge + wash distance */
#define AURORA_REPLAY_GUARD_MS 120000
#define AURORA_DARK_LUM          26    /* below this the LED is off */

typedef enum { AURORA_IDLE, AURORA_PLAYING } aurora_state_t;
/* Both halves start playing at power-on (= plug-in), timed from their own boot. */
static aurora_state_t aurora_state      = AURORA_PLAYING;
static uint32_t       aurora_start      = 0;           /* local timer_read32() domain */
static uint32_t       aurora_last_play  = 0;           /* master: replay guard */
static bool           aurora_send       = false;       /* master: elapsed not yet delivered */
static uint32_t       aurora_send_at    = 0;
static volatile bool  aurora_usb_fresh  = false;       /* set from the USB ISR */
static volatile bool  aurora_rx         = false;       /* slave: set from the RPC ISR */
static volatile uint32_t aurora_rx_elapsed;

/* Runs in the USB event ISR: flags only. */
void notify_usb_device_state_change_user(struct usb_device_state s) {
    static usb_configure_state_t prev = USB_DEVICE_STATE_NO_INIT;
    if (s.configure_state == USB_DEVICE_STATE_CONFIGURED && prev != USB_DEVICE_STATE_SUSPEND) {
        aurora_usb_fresh = true;
    }
    prev = s.configure_state;
}

/* Slave, in the transport ISR: store only; aurora_task() does the clock maths. */
static void aurora_sync_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    if (in_buflen < sizeof(uint32_t)) return;
    uint32_t elapsed;
    memcpy(&elapsed, in_data, sizeof(elapsed));
    aurora_rx_elapsed = elapsed;
    aurora_rx         = true;
}

static void aurora_init(void) {
    transaction_register_rpc(AURORA_SYNC, aurora_sync_handler);
}

static void aurora_task(void) {
    uint32_t now = timer_read32();
    if (is_keyboard_master()) {
        if (aurora_usb_fresh) {
            aurora_usb_fresh = false;
            /* the power-up run counts as a play at t = 0 */
            if (TIMER_DIFF_32(now, aurora_last_play) >= AURORA_REPLAY_GUARD_MS) {
                aurora_state     = AURORA_PLAYING;
                aurora_start     = now;
                aurora_last_play = now;
                aurora_send      = true;
                aurora_send_at   = now - 50;   /* send on this pass */
            }
        }
        /* PC start-up: tell the slave how far in we are; retry every 50 ms. */
        if (aurora_send && TIMER_DIFF_32(now, aurora_send_at) >= 50) {
            aurora_send_at   = now;
            uint32_t elapsed = TIMER_DIFF_32(now, aurora_start);
            if (elapsed >= AURORA_TOTAL_MS ||
                transaction_rpc_exec(AURORA_SYNC, sizeof(elapsed), &elapsed, 0, NULL)) {
                aurora_send = false;
            }
        }
    } else if (aurora_rx) {
        aurora_rx    = false;
        aurora_start = now - aurora_rx_elapsed;
        aurora_state = AURORA_PLAYING;
    }
    if (aurora_state == AURORA_PLAYING && TIMER_DIFF_32(now, aurora_start) >= AURORA_TOTAL_MS) {
        aurora_state = AURORA_IDLE;
    }
}

/* Crest front, 255 * exp(-(d/16)^2) for d = 0..40 x-units ahead of the crest. */
static const uint8_t aurora_front[41] = {
    255, 254, 251, 246, 240, 231, 222, 211, 199, 186, 173, 159, 145, 132, 119, 106, 94, 82, 72, 62,
    53, 46, 38, 32, 27, 22, 18, 15, 12, 10, 8, 6, 5, 4, 3, 2, 2, 1, 1, 1, 0,
};

/* Brightness vs distance d from a crest (d > 0 = ahead): sharp front, long tail
 * behind that halves every 48 units (~exp(d/70)). */
static uint8_t aurora_profile(int16_t d) {
    if (d >= 0) return d > 40 ? 0 : aurora_front[d];
    uint16_t n = (uint16_t)(-d);
    uint8_t  q = n / 48, r = n % 48;
    if (q >= 8) return 0;
    uint8_t hi = 255 >> q, lo = 255 >> (q + 1);
    return hi - (uint8_t)((uint16_t)(hi - lo) * r / 48);
}

/* Draws this half's LEDs. Returns true while the aurora owns the frame. */
static bool aurora_render(uint8_t led_min, uint8_t led_max) {
    if (aurora_state == AURORA_IDLE) return false;
    uint32_t t = TIMER_DIFF_32(timer_read32(), aurora_start);
    if (t >= AURORA_TOTAL_MS) return false;
    if (t < AURORA_SETTLE_MS) {
        for (uint8_t led = led_min; led < led_max; led++) rgb_matrix_set_color(led, 0, 0, 0);
        return true;
    }

    /* Follow the user's brightness, but never so dim the show is lost. */
    uint8_t bright = rgb_matrix_get_val();
    if (bright < RGB_MATRIX_MAXIMUM_BRIGHTNESS / 2) bright = RGB_MATRIX_MAXIMUM_BRIGHTNESS / 2;

    /* Crest positions of the two tides (x-units); the second is the last. */
    int32_t c1 = AURORA_START_X + (int32_t)(t - AURORA_SETTLE_MS) * AURORA_SPEED_NUM / AURORA_SPEED_DEN;
    int32_t c2 = t >= AURORA_SETTLE_MS + AURORA_TIDE_GAP_MS
               ? AURORA_START_X + (int32_t)(t - AURORA_SETTLE_MS - AURORA_TIDE_GAP_MS) * AURORA_SPEED_NUM / AURORA_SPEED_DEN
               : INT16_MIN;

    for (uint8_t led = led_min; led < led_max; led++) {
        bool    under = g_led_config.flags[led] & LED_FLAG_UNDERGLOW;
        /* slight diagonal: lower rows (thumbs) trail the top a little */
        int16_t xs = (int16_t)g_led_config.point[led].x + ((int16_t)g_led_config.point[led].y - 32) * 45 / 128;

        int32_t d1  = xs - c1;
        uint8_t lum = (d1 > -400 && d1 < 400) ? aurora_profile((int16_t)d1) : 0;
        uint8_t a   = 255;
        if (c2 != INT16_MIN) {
            int32_t d2 = xs - c2;
            if (d2 <= -AURORA_WASH_FAR) continue;    /* washed in: leave the user's effect */
            uint8_t l2 = d2 < 400 ? aurora_profile((int16_t)d2) : 0;
            if (l2 > lum) lum = l2;
            if (d2 < -AURORA_WASH_NEAR) {
                a = (uint8_t)((d2 + AURORA_WASH_FAR) * 255 / (AURORA_WASH_FAR - AURORA_WASH_NEAR));
            }
        }

        if (lum < AURORA_DARK_LUM) {
            rgb_matrix_set_color(led, 0, 0, 0);
            continue;
        }
        /* brightness -> colour: crest aqua-cyan going white, tail deep indigo */
        uint8_t k = (uint8_t)((uint16_t)(lum - AURORA_DARK_LUM) * 255 / (255 - AURORA_DARK_LUM));
        uint8_t h = 172 - (uint8_t)((uint16_t)k * 52 / 255) + (under ? 8 : 0);   /* 172 .. 120 */
        uint8_t s = k > 209 ? 255 - (uint8_t)((uint16_t)(k - 209) * 140 / 46) : 255;
        uint8_t v = AURORA_DARK_LUM + (uint8_t)((uint16_t)k * (255 - AURORA_DARK_LUM) / 255);
        if (under) v = scale8(v, 192);
        v = scale8(scale8(v, bright), a);
        RGB rgb = hsv_to_rgb((HSV){h, s, v});
        rgb_matrix_set_color(led, rgb.r, rgb.g, rgb.b);
    }
    return true;
}
#else
static inline void aurora_init(void) {}
static inline void aurora_task(void) {}
static inline bool aurora_render(uint8_t led_min, uint8_t led_max) { return false; }
#endif

// ==================== RGB Layer Indicators ====================

#ifndef INDICATOR_BRIGHTNESS_PCT
#define INDICATOR_BRIGHTNESS_PCT 100   // 100 = no change; keymap config.h sets the real value
#endif

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
/* True when at least one LED is assigned to the role (on either half). */
static inline bool indicator_role_assigned(uint8_t role) {
    if (g_indicator_config.role_masks[role]) return true;
#if RGB_MATRIX_LED_COUNT > 64
    if (g_indicator_masks_hi[role]) return true;
#endif
    return false;
}
#endif

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    /* Wake-up aurora owns the frame while it plays (layer/caps indicators wait). */
    if (aurora_render(led_min, led_max)) return true;

    // Scale layer indicator colour to INDICATOR_BRIGHTNESS_PCT of the slider so a
    // steady indicator doesn't out-shine the (usually sub-ceiling) RGB effects.
    uint8_t brightness = rgb_matrix_get_val() * INDICATOR_BRIGHTNESS_PCT / 100;
    if (brightness == 0) brightness = 1;
    #define SCALE(c) (((c) * brightness / 255) ?: 1)

    bool set = false;

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
    /* Each role has its own LED; caps lock and layer can both be lit simultaneously. */

    bool caps = host_keyboard_led_state().caps_lock;
    uint8_t layer = get_highest_layer(layer_state);
    /* While an indicator shows (Caps Lock on, or a layer with LEDs assigned), every LED
     * not assigned to it is dark instead of running the RGB effect, so the indicator
     * colours stand alone. Base layer with Caps Lock off: the effect runs untouched.
     * Uses the whole mask, not this half's LEDs, so both halves blank together. */
    bool showing = (caps && indicator_role_assigned(0)) ||
                   (layer >= 1 && layer < INDICATOR_NUM_ROLES && indicator_role_assigned(layer));
    /* Each role has a bitmask; multiple roles can share the same LED. */
    for (uint8_t led = led_min; led < led_max; led++) {
        if (led >= 64) continue;
        uint64_t bit = (1ULL << led);
        uint8_t r = 0, g = 0, b = 0;
        bool active = false;
        /* Layer indicator (lower priority) */
        if (layer >= 1 && layer < INDICATOR_NUM_ROLES &&
                (g_indicator_config.role_masks[layer] & bit)) {
            r = g_indicator_led_colors[layer][led][0];
            g = g_indicator_led_colors[layer][led][1];
            b = g_indicator_led_colors[layer][led][2];
            active = true;
        }
        /* Caps Lock overrides layer */
        if (caps && (g_indicator_config.role_masks[0] & bit)) {
            r = g_indicator_led_colors[0][led][0];
            g = g_indicator_led_colors[0][led][1];
            b = g_indicator_led_colors[0][led][2];
            active = true;
        }
        if (active) {
            rgb_matrix_set_color(led, SCALE(r), SCALE(g), SCALE(b));
            set = true;
        } else if (showing) {
            rgb_matrix_set_color(led, 0, 0, 0);
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

// ==================== DIP Switch (disc click) ====================

#if defined(DIP_SWITCH_ENABLE)
/* SW1, the disc's centre click on GP8, is not on the matrix. It is replayed as
 * a key press at a thumb-row slot that has no switch ([3][0] left, [7][0]
 * right; the thumbs only use cols 3-5), so Vial can remap it like any other
 * key. The index is per hand (0 = left, 1 = right) whichever half is master --
 * see config.h. */
static const keypos_t disc_click_pos[] = {
    { .row = 3, .col = 0 },
    { .row = 7, .col = 0 },
};
_Static_assert(ARRAY_SIZE(disc_click_pos) == NUM_DIP_SWITCHES,
               "one disc click position per hand");

/* Matrix tester: the disc click is not on the matrix, so the position it is replayed
 * onto is reported to the tester (via_switch_matrix_extra_user, quantum/via.c) while
 * it is held. */
static matrix_row_t g_vmatrix[MATRIX_ROWS];
static void vmatrix_note(keypos_t k, bool pressed) {
    if (k.row >= MATRIX_ROWS || k.col >= MATRIX_COLS) return;
    if (pressed) g_vmatrix[k.row] |= (matrix_row_t)1 << k.col;
    else         g_vmatrix[k.row] &= ~((matrix_row_t)1 << k.col);
}
matrix_row_t via_switch_matrix_extra_user(uint8_t row) {
    return row < MATRIX_ROWS ? g_vmatrix[row] : 0;
}

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index < ARRAY_SIZE(disc_click_pos)) {
        keyevent_t event = {
            .key     = disc_click_pos[index],
            .pressed = active,
            .time    = timer_read(),
            .type    = KEY_EVENT
        };
        vmatrix_note(event.key, event.pressed);
        action_exec(event);
    }
    return true;
}
#endif

// ==================== Keymaps ====================

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,                         KC_Y,    KC_U,    KC_I,    KC_O,   KC_P,  KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,                         KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,                         KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,  KC_ESC,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, 	 MO(2),  KC_SPC,     KC_ENT, MO(1), KC_RALT,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  KC_LEFT,   KC_UP, KC_RGHT, KC_DOWN, MS_BTN1,               KC_LEFT,   KC_UP, KC_RGHT, KC_DOWN, MS_BTN1

  ),

  [1] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                         KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_LEFT, KC_DOWN,   KC_UP,KC_RIGHT, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LSFT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_RALT,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  _______, _______, _______, _______, _______,               _______, _______, _______, _______, _______
  ),

  [2] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB, KC_EXLM,   KC_AT, KC_HASH,  KC_DLR, KC_PERC,                      KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_MINS,  KC_EQL, KC_LBRC, KC_RBRC, KC_BSLS,  KC_GRV,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LSFT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_UNDS, KC_PLUS, KC_LCBR, KC_RCBR, KC_PIPE, KC_TILD,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_RALT,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  _______, _______, _______, _______, _______,               _______, _______, _______, _______, _______
  ),

  [3] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
    QK_REBOOT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      RGB_TOG, RGB_HUI, RGB_SAI, RGB_VAI, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      RGB_MOD, RGB_HUD, RGB_SAD, RGB_VAD, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_RALT,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  _______, _______, _______, _______, _______,               _______, _______, _______, _______, _______
  ),

  [4] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,               XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
  ),

  [5] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,               XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
  ),

  [6] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,               XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
  ),

  [7] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,               XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
  ),

  [8] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,               XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
  ),

  [9] = LAYOUT_split_3x6_3_disc(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX,
                                      //`--------------------------'  `--------------------------'
  //                     left disc: left       up    right     down    click        right disc: left       up    right     down    click
                                  XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,               XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX
  )
};




#if defined(ENCODER_MAP_ENABLE)
    const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
        [0] = { ENCODER_CCW_CW(CK_ATABF, CK_ATABR), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
        [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(C(KC_MINS), C(KC_EQL)) },
        [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_F3, C(KC_F3)) },
        [3] = { ENCODER_CCW_CW(G(KC_LEFT), G(KC_RGHT)), ENCODER_CCW_CW(A(KC_RGHT), A(KC_LEFT)) },
        [4] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [5] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [6] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [7] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [8] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [9] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) }
    };
#endif

// ==================== Keyboard Init ====================

void keyboard_post_init_user(void) {
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
    { /* load persisted shared sleep timeout, clamp, then mirror to RGB */
        uint32_t st = eeprom_read_dword((uint32_t *)EEPROM_SLEEP_TIMEOUT_OFFSET);
        if (st >= MIN_SLEEP_TIMEOUT_MS && st <= MAX_SLEEP_TIMEOUT_MS) g_sleep_timeout = st;
    }
    apply_sleep_timeout();   // mirror shared sleep timeout to core RGB timeout

    transaction_register_rpc(TP_INFO_SYNC, tp_info_sync_handler);
    aurora_init();

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
    transaction_register_rpc(VIALRGB_DIRECT_SYNC, vialrgb_direct_sync_handler);
    transaction_register_rpc(VIALRGB_INDICATOR_SYNC, vialrgb_indicator_sync_handler);
    transaction_register_rpc(VIALRGB_INDICATOR_LED_SYNC, vialrgb_indicator_led_sync_handler);

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
        /* Per-LED colours: saved table if present, else each role in its own colour
         * (a config saved before per-LED colours existed). */
        if (eeprom_read_word((const uint16_t *)EEPROM_INDICATOR_LED_MAGIC_OFFSET)
                == INDICATOR_LED_EEPROM_MAGIC) {
            eeprom_read_block(g_indicator_led_colors,
                              (const void *)EEPROM_INDICATOR_LED_DATA_OFFSET,
                              sizeof(g_indicator_led_colors));
        } else {
            for (uint8_t i = 0; i < INDICATOR_NUM_ROLES; i++) indicator_fill_role_leds(i);
        }
    }
    g_indicator_led_dirty = (1u << INDICATOR_NUM_ROLES) - 1;
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

    if (is_keyboard_master()) {
        // Handle modifier-based sniper activation
        if (sniper_modifier_mask != 0) {
            uint8_t current_mods = get_mods() | get_oneshot_mods() | get_weak_mods();
            bool mods_match = (current_mods & sniper_modifier_mask) == sniper_modifier_mask;
            static bool prev_mods_match   = false;
            static bool sniper_toggled_manually = false;
            static bool prev_sniper_state = false;

            // Detect a manual key toggle: sniper state changed but mods didn't cause it
            if (sniper_mode_active != prev_sniper_state) {
                if (!prev_mods_match && !mods_match) {
                    sniper_toggled_manually = sniper_mode_active;
                }
            }

            if (mods_match && !prev_mods_match) {
                sniper_mode_active = true;
                digitizer_set_sniper_active(true);
            } else if (!mods_match && prev_mods_match) {
                if (!sniper_toggled_manually) {
                    sniper_mode_active = false;
                    digitizer_set_sniper_active(false);
                }
            }
            prev_mods_match   = mods_match;
            prev_sniper_state = sniper_mode_active;
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
// The axis mapping below was written for a ROTATION_270 sensor config, which is
// what corneplus2 uses since 2026-10-10 (ROTATION_90 inverted both axes on
// hardware), so it applies as designed. Re-check scroll/swipe direction there.
//
// The sub-pixel accumulators are reset on finger-lift to prevent carry-over.
#ifndef DIGITIZER_SWIPE2_THRESHOLD
#    define DIGITIZER_SWIPE2_THRESHOLD 40  // cursor pixels to trigger back/forward
#endif
#ifndef DIGITIZER_SWIPE3_THRESHOLD
#    define DIGITIZER_SWIPE3_THRESHOLD 50  // cursor pixels to trigger swipe keycode
#endif

/* Runtime scroll-direction invert (scroll_dir_v / scroll_dir_h: Vial trackpad
 * settings 0x50 args[2]/[3], and the SCROLL_DIR_V / SCROLL_DIR_H keycodes).
 * Ported from sofleplus2 v5.11: before it, the settings were stored and toggled
 * but nothing applied them to a report, so the toggle did nothing. Called at BOTH
 * exits of digitizer_pre_send_user so it covers the core 2-finger scroll (early
 * return) and the 1-finger scroll-layer scroll written below. Independent of the
 * compile-time DIGITIZER_SCROLL_INVERT (macOS natural-scrolling default). */
static inline void apply_scroll_invert(report_mouse_t *report) {
    if (scroll_dir_v) report->v = -report->v;
    if (scroll_dir_h) report->h = -report->h;
}

void digitizer_pre_send_user(report_mouse_t *report) {
    // Merge keyboard-side mouse buttons (e.g. a DIP/thumb key mapped to BTN1) into
    // the direct-send report. digitizer_mouse_fallback.c calls host_mouse_send()
    // before QMK's mousekey OR in pointing_device_task(), so we must query mousekey
    // state here directly. Saving it via pointing_device_task_user() doesn't work
    // because that callback fires before the mousekey OR (see pointing_device.c:
    // task_kb() called before mousekey_get_report()).
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
        apply_scroll_invert(report);   /* covers the core 2-finger scroll */
        return;
    }

    uint8_t  layer     = get_highest_layer(layer_state);
    uint16_t layer_bit = (1 << layer);

    if (user_config.scroll_layers & layer_bit) {
        // --- 1-finger scroll ---
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
        swipe3_h -= (int)report->x;
        swipe3_v += (int)report->y;
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

    apply_scroll_invert(report);   /* covers the 1-finger scroll written above */
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

        case CK_ATMWU:  //Alt mouse wheel up
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
            return true;

        case CK_ATMWD:  //Alt mouse wheel down
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
            return true;
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
    #include "oled.c"
    #include <stdio.h>
#endif

// ==================== Bootmagic: bootloader WITHOUT erasing ====================
/* v5.11: hold T (left half) or Y (right half) -- [0][5] / [4][5], see config.h --
 * while plugging the USB cable into that half, and the board jumps straight to the
 * RP2040 UF2 bootloader. Stock Bootmagic also clears EEPROM first (eeconfig_disable());
 * overriding that with a no-op keeps the keymap and every setting.
 *  - A NEWLY BUILT firmware still resets the Vial keymap on its first boot (Vial's
 *    EEPROM magic is a random BUILD_ID per build); re-flashing the same .uf2 does not.
 *  - To wipe everything on purpose, use the QK_CLEAR_EEPROM (EE_CLR) key: it resets
 *    EEPROM and reboots, and keeps the EE_HANDS left/right setting. */
void bootmagic_reset_eeprom(void) {}

/* Copyright 2020 Josef Adamcik
 * Modification for VIA support and RGB underglow by Jens Bonk-Wiltfang
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

// clang-format off

/* =========================================================================
 * SoflePLUS v1.10
 *
 * The legacy relative-mouse trackpad path from v1.02b is gone. This keymap
 * runs the PTP digitizer stack and the trackpad feature set already shipping
 * on sofleplus2 v5.10 — gesture modes per layer, sniper mode, trackpad
 * toggle, zoom, and the sofleplus2 OLED — adapted to v1 hardware.
 *
 * Adapted for v1, not copied blindly:
 *   - 10x6 matrix / 60-key LAYOUT (v1) rather than 10x7 (v2 disc keys)
 *   - v1's 5-switch DIP handler kept as-is
 *   - v1's 72-LED layer-colour RGB indicator kept as-is (v2's indicator-role
 *     subsystem is NOT ported; it is tied to v2's 58-LED map)
 *   - no RDY/RST: those GPIOs are DIP inputs here. See config.h.
 *   - raw-EEPROM settings block relocated, see the address map below
 *
 * Nothing outside keyboards/xcmkb/sofleplus1/ is modified.
 * ========================================================================= */

#include QMK_KEYBOARD_H
#include "encoder.c"
#include "eeconfig.h"
#include "platforms/eeprom.h"
#include "digitizer_mouse_fallback.h"   // not pulled in by quantum.h; digitizer.h does not include it
#include "vialrgb.h"                    // prototypes for the vialrgb_*_user hooks below
#include "transactions.h"               // TP_INFO_SYNC master->slave RPC
#include "os_detection.h"

#ifdef DIP_SWITCH_ENABLE
bool dip_switch_update_user(uint8_t index, bool active) {
    switch (index) {
        case 0:
            if(active) { //If switch is pressed
                register_code(KC_BTN1);
            } else { //If switch is not pressed
                unregister_code(KC_BTN1);
            }
            break;
        case 1:
            if(active) {
                register_code(KC_DOWN); //not working
            } else {
                unregister_code(KC_DOWN);
            }
            break;
        case 2:
            if(active) {
                register_code(KC_RGHT);
            } else {
                unregister_code(KC_RGHT);
            }
            break;
        case 3:
            if(active) {
                register_code(KC_UP); //not working
            } else {
                unregister_code(KC_UP);
            }
            break;
        case 4:
            if(active) {
                register_code(KC_LEFT);
            } else {
                unregister_code(KC_LEFT);
            }
            break;
    }
    return true;
}
#endif


// ==================== Custom Keycodes ====================

/* Order matters. vial.json's customKeycodes array maps POSITIONALLY onto
 * QK_KB_0, QK_KB_1, ... so the first eight entries here must stay in the same
 * order as the eight entries in vial.json. Everything below TP_INFO is handled
 * in process_record_user but deliberately NOT listed in vial.json — exactly as
 * sofleplus2 does it, because those settings live in the Vial Trackpad tab
 * rather than being bound to physical keys. */
#ifdef VIA_ENABLE
	enum custom_keycodes {
    CK_ATABF = QK_KB_0,
    CK_ATABR,
    CK_ATMWU,           // Alt mouse wheel up
    CK_ATMWD,           // Alt mouse wheel down
    CK_PO,              // Power options
    OS_DETECTION_TOGGLE,
    AP_GLOB,
    TP_INFO,
    /* Below: hidden from the Vial GUI, not in vial.json. */
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
	enum custom_keycodes {
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


// ==================== Speed / Scroll Constants ====================

#define MIN_CURSOR_SPEED     1
#define MAX_CURSOR_SPEED     6
#define DEFAULT_CURSOR_SPEED 3
#define DEFAULT_SNIPER_SPEED 1

#define MIN_SCROLL_SPEED     1
#define MAX_SCROLL_SPEED     8
#define DEFAULT_SCROLL_SPEED 4


// ==================== State Variables ====================

int16_t     scroll_speed     = DEFAULT_SCROLL_SPEED;
static bool trackpad_enabled = true;
int16_t     scroll_dir_v     = 0;   // invert vertical scroll
int16_t     scroll_dir_h     = 0;   // invert horizontal scroll

/* Shared OLED + RGB sleep timeout (ms), runtime-settable and persisted.
 * Drives OLED sleep in oled_task_user() and RGB sleep via the core
 * g_rgb_matrix_timeout (rgb_matrix.h:282). */
uint32_t g_sleep_timeout = DEFAULT_SLEEP_TIMEOUT_MS;
static void apply_sleep_timeout(void) { g_rgb_matrix_timeout = g_sleep_timeout; }

// Sniper mode state
#define LEARNING_TIMEOUT     5000
#define INFO_TIMEOUT         2000
#define INFO_DISPLAY_TIMEOUT 3000

bool     sniper_mode_active   = false;
uint8_t  sniper_modifier_mask = 0;
bool     sniper_learning_mode = false;
bool     sniper_info_mode     = false;
uint16_t learning_timer       = 0;
uint16_t info_timer           = 0;

// Zoom gesture state
bool        zoom_enabled   = true;
static bool zoom_active    = false;
static bool zoom_using_cmd = false;

bool digitizer_zoom_enabled(void) { return zoom_enabled; }

/* Release any modifier held by the zoom gesture. Safe when zoom is inactive. */
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

static bool host_is_apple(void) {
    os_variant_t os = get_effective_os_detection();
    return (os == OS_MACOS || os == OS_IOS);
}


// ==================== EEPROM ====================

/* EEPROM ADDRESS MAP — sofleplus1-specific, deliberately NOT copied from
 * sofleplus2.
 *
 * sofleplus2 places its raw settings at 0x0FA0-0x1123. On this board that
 * range sits INSIDE Vial's dynamic-macro area: macros run from
 * DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (roughly 0x0A00 here, after keymap +
 * encoders + QMK settings + tap dance + combos + key overrides) all the way up
 * to DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (nvm_dynamic_keymap.c:109-112). Writing
 * sniper/OLED settings there would be clobbered by a large macro, and would
 * clobber the macro in return. It is latent rather than fatal on sofleplus2
 * only because few users store more than ~1.4 KB of macros.
 *
 * Fixed properly here instead of inherited: config.h lowers
 * DYNAMIC_KEYMAP_EEPROM_MAX_ADDR to 0x7BFF and this block lives above it, in
 * space Vial will never touch. Macros still get ~29 KB.
 *
 *   0x7C00-0x7C07  trackpad layer config (magic + 3 x uint16)
 *   0x7C10         sniper_modifier_mask
 *   0x7C11         sniper_scale_level
 *   0x7C12         zoom toggle
 *   0x7C13         os_detection_enabled
 *   0x7C14         taps_as_clicks   (0=off 1=on, 0xFF=unset -> default off)
 *   0x7C15         trackpad_enabled (0x01=on 0x02=off, 0xFF=unset -> default on)
 *   0x7C18-0x7C1B  shared OLED+RGB sleep timeout, uint32 ms
 *   0x7C20-0x7C21  OLED config magic (0x4F4C 'OL')
 *   0x7C22-0x7C63  oled_config_t (66 bytes)
 *
 * The VialRGB direct-colour and indicator-role blocks are NOT ported (v1 keeps
 * its own simple layer-colour indicator), so the overlap sofleplus2 documents
 * as "KNOWN, NOT YET FIXED" cannot occur here.
 *
 * Also note the 32-bit eeconfig_user word, which QMK manages separately:
 *   bits 0-7 mouse scale | 8-15 scroll speed | 16 invert v | 17 invert h
 *   (taps_as_clicks is NOT in this word — it owns the byte at 0x7C14)
 */
#define EECONFIG_USER_TRACKPAD_OFFSET    0x7C00u
#define TRACKPAD_CONFIG_MAGIC            0x7401u
#define EEPROM_SNIPER_SETTINGS_OFFSET    0x7C10u
#define EEPROM_ZOOM_TOGGLE_OFFSET        0x7C12u
#define EEPROM_OS_DETECTION_OFFSET       0x7C13u
#define EEPROM_TAPS_AS_CLICKS_OFFSET     0x7C14u
#define EEPROM_TRACKPAD_ENABLED_OFFSET   0x7C15u
#define EEPROM_SLEEP_TIMEOUT_OFFSET      0x7C18u
#define EEPROM_OLED_MAGIC_OFFSET         0x7C20u
#define EEPROM_OLED_DATA_OFFSET          0x7C22u
#define OLED_EEPROM_MAGIC                0x4F4Cu  /* 'O','L' — OLED labels */

/* Deliberately does NOT carry taps_as_clicks: that has its own byte at
 * EEPROM_TAPS_AS_CLICKS_OFFSET. Keeping it in both places would be two sources
 * of truth for one setting, with the byte silently winning at load time. */
static void save_user_settings(void) {
    uint32_t packed = ((uint32_t)digitizer_get_mouse_scale()   & 0xFF)
                    | (((uint32_t)digitizer_get_scroll_scale() & 0xFF) << 8)
                    | ((uint32_t)(scroll_dir_v ? 1 : 0) << 16)
                    | ((uint32_t)(scroll_dir_h ? 1 : 0) << 17);
    eeconfig_update_user(packed);
}

static void load_user_settings(void) {
    uint32_t d = eeconfig_read_user();
    /* A never-written or erased word reads back as 0 or 0xFFFFFFFF. Treat both
     * as "no stored settings" and leave the core defaults in place (mouse 3,
     * scroll 4 — digitizer_mouse_fallback.c:151-156); an erased word would
     * otherwise restore every bool as 1. The scale setters clamp and ignore
     * out-of-range values themselves (:175-183), so no bounds check is needed
     * here and this cannot drift out of sync with the tables. */
    if (d != 0 && d != 0xFFFFFFFFu) {
        digitizer_set_mouse_scale ((uint8_t)( d        & 0xFF));
        digitizer_set_scroll_scale((uint8_t)((d >> 8)  & 0xFF));
        scroll_speed = digitizer_get_scroll_scale();
        scroll_dir_v = (d >> 16) & 1;
        scroll_dir_h = (d >> 17) & 1;
    }
    /* taps_as_clicks is loaded separately by load_taps_as_clicks(). */
}

void save_sniper_settings(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET),     sniper_modifier_mask);
    eeprom_write_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET + 1), digitizer_get_sniper_scale());
}

void load_sniper_settings(void) {
    uint8_t stored_mask  = eeprom_read_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET));
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

static void load_zoom_setting(void) {
    uint8_t stored = eeprom_read_byte((uint8_t*)(EEPROM_ZOOM_TOGGLE_OFFSET));
    if (stored == 0xFF) {
        zoom_enabled = true;
        save_zoom_setting();
    } else {
        zoom_enabled = (stored != 0);
    }
}

static void save_taps_as_clicks(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_TAPS_AS_CLICKS_OFFSET), digitizer_taps_as_clicks ? 1 : 0);
}

static void load_taps_as_clicks(void) {
    uint8_t stored = eeprom_read_byte((uint8_t*)(EEPROM_TAPS_AS_CLICKS_OFFSET));
    if (stored == 0xFF) {
        save_taps_as_clicks();
    } else {
        digitizer_taps_as_clicks = (stored != 0);
    }
}

/* 0x01 = on, 0x02 = off. Not 0/1, so an erased 0xFF is distinguishable from a
 * deliberate "off" — same encoding sofleplus2 uses. */
static void save_trackpad_enabled(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_TRACKPAD_ENABLED_OFFSET), trackpad_enabled ? 0x01 : 0x02);
}

static void load_trackpad_enabled(void) {
    uint8_t stored = eeprom_read_byte((uint8_t*)(EEPROM_TRACKPAD_ENABLED_OFFSET));
    if (stored == 0x02) {
        trackpad_enabled = false;
    } else {
        trackpad_enabled = true;
        if (stored != 0x01) save_trackpad_enabled();  // migrate unset/garbage
    }
}

static void save_sleep_timeout(void) {
    eeprom_write_dword((uint32_t*)(EEPROM_SLEEP_TIMEOUT_OFFSET), g_sleep_timeout);
}

static void load_sleep_timeout(void) {
    uint32_t stored = eeprom_read_dword((const uint32_t*)(EEPROM_SLEEP_TIMEOUT_OFFSET));
    if (stored >= MIN_SLEEP_TIMEOUT_MS && stored <= MAX_SLEEP_TIMEOUT_MS) {
        g_sleep_timeout = stored;
    } else {
        g_sleep_timeout = DEFAULT_SLEEP_TIMEOUT_MS;
        save_sleep_timeout();
    }
    apply_sleep_timeout();
}

static void load_os_detection_setting(void) {
    uint8_t stored = eeprom_read_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET);
    if (stored == 0xFF) {
        os_detection_enabled = true;
        eeprom_write_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET, 1);
    } else {
        os_detection_enabled = (stored != 0);
    }
}

void toggle_os_detection(void) {
    os_detection_enabled = !os_detection_enabled;
    eeprom_write_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET, os_detection_enabled ? 1 : 0);
}


// ==================== Trackpad Layer Config ====================

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
    eeprom_write_word((uint16_t*)(EECONFIG_USER_TRACKPAD_OFFSET), TRACKPAD_CONFIG_MAGIC);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 2), user_config.scroll_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 3), (user_config.scroll_layers >> 8) & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 4), user_config.swipe2_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 5), (user_config.swipe2_layers >> 8) & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 6), user_config.swipe3_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 7), (user_config.swipe3_layers >> 8) & 0xFF);
}

void trackpad_config_load(void) {
    if (eeprom_read_word((uint16_t*)(EECONFIG_USER_TRACKPAD_OFFSET)) == TRACKPAD_CONFIG_MAGIC) {
        uint8_t sl = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 2));
        uint8_t sh = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 3));
        uint8_t w2l = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 4));
        uint8_t w2h = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 5));
        uint8_t w3l = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 6));
        uint8_t w3h = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 7));
        user_config.scroll_layers = sl  | (sh  << 8);
        user_config.swipe2_layers = w2l | (w2h << 8);
        user_config.swipe3_layers = w3l | (w3h << 8);
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


// ==================== OLED label config ====================

/* row1 + one name per layer, all max 5 chars + null. Editable from the Vial
 * GUI (sub-ID 0x52) and mirrored to the slave in the TP_INFO payload. */
typedef struct {
    char row1[6];
    char layer_names[10][6];
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


// ==================== TP_INFO split sync ====================

/* One master->slave RPC carrying everything the slave OLED needs. 79 bytes
 * packed, auto-pushed every 500ms from housekeeping_task_user().
 *
 * On sofleplus1's half-duplex single-wire link that is ~3.4ms of wire time at
 * 230400 baud twice a second — about 0.7% duty, which is why this is safe on a
 * TRS-style interconnect. It does require RPC_M2S_BUFFER_SIZE >= 79; config.h
 * sets it. (The digitizer itself does NOT use the RPC buffers — it syncs via
 * split_shmem->digitizer.report, transactions.c:811 — and does not even reach
 * the wire when the trackpad half is master, which is the normal cabling.) */
#define TP_INFO_DURATION_MS 5000
typedef struct {
    uint8_t  os_variant;
    bool     ptp_mode;
    bool     trackpad_on;
    uint8_t  gesture_mode;     // 0=CURSR 1=SCROL 2=2SWPE 3=3SWPE
    uint8_t  dpi;
    uint8_t  sniper_dpi;
    uint8_t  scroll_spd;
    bool     sniper_active;
    bool     show_overlay;     // true = TP_INFO keypress; false = background auto-sync
    bool     os_detect_enabled;
    uint32_t sleep_timeout;
    char     row1[5];
    char     layer_names[10][6];
} __attribute__((packed)) tp_info_payload_t;

static tp_info_payload_t g_tp_info_data         = {0};
static bool              tp_info_active         = false;
static uint32_t          tp_info_timer          = 0;
static bool              g_tp_info_send_pending = false;
static bool              g_slave_sync_valid     = false;

static void tp_info_sync_handler(uint8_t in_buflen, const void *in_data,
                                 uint8_t out_buflen, void *out_data) {
    if (in_buflen >= sizeof(tp_info_payload_t)) {
        memcpy(&g_tp_info_data, in_data, sizeof(tp_info_payload_t));
        g_slave_sync_valid = true;
        memcpy(g_oled_config.row1, g_tp_info_data.row1, sizeof(g_tp_info_data.row1));
        memcpy(g_oled_config.layer_names, g_tp_info_data.layer_names, sizeof(g_tp_info_data.layer_names));
        /* Adopt the master's sleep timeout so both halves sleep together. */
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

static void tp_info_fill(tp_info_payload_t *p, bool show_overlay) {
    uint8_t layer = get_highest_layer(layer_state);
    p->os_variant        = (uint8_t)detected_host_os();
    p->ptp_mode          = !digitizer_send_mouse_reports;
    p->trackpad_on       = trackpad_enabled;
    p->sniper_active     = sniper_mode_active;
    p->dpi               = (uint8_t)digitizer_get_mouse_scale();
    p->sniper_dpi        = (uint8_t)digitizer_get_sniper_scale();
    p->scroll_spd        = (uint8_t)scroll_speed;
    p->gesture_mode      = (user_config.scroll_layers & (1 << layer)) ? 1 :
                           (user_config.swipe2_layers & (1 << layer)) ? 2 :
                           (user_config.swipe3_layers & (1 << layer)) ? 3 : 0;
    p->show_overlay      = show_overlay;
    p->os_detect_enabled = os_detection_enabled;
    p->sleep_timeout     = g_sleep_timeout;
    memcpy(p->row1, g_oled_config.row1, sizeof(p->row1));
    memcpy(p->layer_names, g_oled_config.layer_names, sizeof(p->layer_names));
}


// ==================== Vial GUI channels ====================

/* Trackpad settings — sub-ID 0x50. Overrides the weak no-ops at
 * quantum/vialrgb.c:270-271. Packet layout is fixed by the Vial fork:
 *   [0] cursor scale  [1] scroll speed   [2] invert v      [3] invert h
 *   [4] zoom enable   [5] trackpad on    [6] sniper scale  [7] taps as clicks
 *   [8] sniper modifier mask
 * vialrgb.c memsets 9 bytes before the getter, so writing args[0..8] is exact. */
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
    /* args[5] (trackpad_enabled) intentionally ignored — same as sofleplus2:
     * the Vial GUI caches the value at connect time and would overwrite any
     * TRACKPAD_TOGGLE made since. Trackpad on/off is keycode-only. */
    digitizer_set_sniper_scale(args[6]);
    digitizer_taps_as_clicks = args[7] != 0;
    sniper_modifier_mask = args[8];
    save_user_settings();
    save_zoom_setting();
    save_taps_as_clicks();
    save_sniper_settings();
}

/* Trackpad layer behaviour bitmasks — sub-ID 0x51. 6 bytes. */
void vialrgb_get_trackpad_layers_user(uint8_t *args) {
    args[0] = user_config.scroll_layers & 0xFF;
    args[1] = (user_config.scroll_layers >> 8) & 0xFF;
    args[2] = user_config.swipe2_layers & 0xFF;
    args[3] = (user_config.swipe2_layers >> 8) & 0xFF;
    args[4] = user_config.swipe3_layers & 0xFF;
    args[5] = (user_config.swipe3_layers >> 8) & 0xFF;
}

void vialrgb_set_trackpad_layers_user(const uint8_t *args) {
    user_config.scroll_layers = args[0] | (args[1] << 8);
    user_config.swipe2_layers = args[2] | (args[3] << 8);
    user_config.swipe3_layers = args[4] | (args[5] << 8);
    trackpad_config_save();
}

/* OLED labels — sub-ID 0x52. item 0xFF = row1, 0-9 = that layer's name. */
void vialrgb_get_oled_config_user(uint8_t item, char *name_out) {
    const char *src;
    if (item == 0xFF) {
        src = g_oled_config.row1;
    } else if (item < 10) {
        src = g_oled_config.layer_names[item];
    } else {
        return;
    }
    memcpy(name_out, src, 5);
}

void vialrgb_set_oled_config_user(uint8_t item, const char *name_in) {
    char *dst;
    if (item == 0xFF) {
        dst = g_oled_config.row1;
    } else if (item < 10) {
        dst = g_oled_config.layer_names[item];
    } else {
        return;
    }
    memcpy(dst, name_in, 5);
    dst[5] = '\0';
    save_oled_config();
    g_tp_info_send_pending = true;   // push new labels to the slave OLED
}


// ==================== Super Alt Tab ====================

#ifdef SUPER_ALT_TAB_ENABLE
	bool     is_alt_tab_active = false;
	uint16_t alt_tab_timer     = 0;
	static uint16_t alt_tab_held_mod = KC_LALT; // release the same key we pressed
#endif

/* Cmd-Tab on macOS/iOS, Alt-Tab everywhere else. v1.02b hardcoded Alt. */
static uint16_t app_switch_mod(void) { return host_is_apple() ? KC_LGUI : KC_LALT; }

void matrix_scan_user(void) {
	#ifdef SUPER_ALT_TAB_ENABLE
		if (is_alt_tab_active) {
			if (timer_elapsed(alt_tab_timer) > 1000) {
				unregister_code(alt_tab_held_mod);
				is_alt_tab_active = false;
			}
		}
	#endif

    if (sniper_info_mode && timer_elapsed(info_timer) > INFO_TIMEOUT) {
        sniper_info_mode = false;
    }

    if (is_keyboard_master()) {
        /* Modifier-held sniper activation */
        if (sniper_modifier_mask != 0) {
            uint8_t current_mods = get_mods() | get_oneshot_mods() | get_weak_mods();
            bool    mods_match   = (current_mods & sniper_modifier_mask) == sniper_modifier_mask;
            static bool prev_mods_match        = false;
            static bool sniper_toggled_manually = false;
            static bool prev_sniper_state      = false;

            /* Detect a manual toggle: sniper changed without the mods causing it */
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

        /* Learning mode: first modifier pressed becomes the sniper mask */
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


// ==================== Housekeeping ====================

//trackball led and haptic not working for layer indicator, thanks Drashna for pointing out
void housekeeping_task_user(void) {
    static layer_state_t state = 0;
    if (layer_state != state) {
        state = layer_state_set_user(layer_state);
    }

    /* Auto-sync master trackpad/OS state to the slave OLED every 500ms. */
    if (is_keyboard_master()) {
        static uint32_t last_auto_sync = 0;
        if (timer_elapsed32(last_auto_sync) >= 500) {
            tp_info_payload_t payload;
            tp_info_fill(&payload, false);
            transaction_rpc_exec(TP_INFO_SYNC, sizeof(payload), &payload, 0, NULL);
            last_auto_sync = timer_read32();
        }

        /* TP_INFO keypress: immediate sync + 5s overlay on the slave. */
        if (g_tp_info_send_pending) {
            tp_info_payload_t payload;
            tp_info_fill(&payload, true);
            if (transaction_rpc_exec(TP_INFO_SYNC, sizeof(payload), &payload, 0, NULL)) {
                g_tp_info_send_pending = false;
            }
        }
    }
}


// ==================== RGB layer indicator (v1, unchanged) ====================

//Layer color indicator for rgb matrix https://docs.qmk.fm/#/feature_rgb_matrix?id=indicator-examples
//color https://github.com/qmk/qmk_firmware/blob/master/quantum/color.h
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = led_min; i < led_max; i++) {
		if (host_keyboard_led_state().caps_lock) {
                rgb_matrix_set_color(i, RGB_RED);
        };
        switch(get_highest_layer(layer_state|default_layer_state)) {
            case 1:
				rgb_matrix_set_color(i, RGB_PURPLE);
                break;
            case 2:
				rgb_matrix_set_color(i, RGB_ORANGE);
                break;
            case 3:
				rgb_matrix_set_color(i, RGB_CYAN);
                break;
            case 4:
				rgb_matrix_set_color(i, RGB_SPRINGGREEN);
                break;
            case 5:
				rgb_matrix_set_color(i, RGB_BLUE);
                break;
            case 6:
				rgb_matrix_set_color(i, RGB_MAGENTA);
                break;
            case 7:
				rgb_matrix_set_color(i, RGB_PINK);
                break;
            case 8:
				rgb_matrix_set_color(i, RGB_GOLD);
                break;
            case 9:
                break;
            default:
                break;
        }
    }
    return false;
}


// ==================== Init ====================

void keyboard_post_init_user(void) {
    load_user_settings();
    load_sniper_settings();
    load_zoom_setting();
    load_taps_as_clicks();
    load_trackpad_enabled();
    load_os_detection_setting();
    load_sleep_timeout();
    trackpad_config_load();
    load_oled_config();

    transaction_register_rpc(TP_INFO_SYNC, tp_info_sync_handler);
}


// ==================== Pointing device task ====================

/* Live because rules.mk sets POINTING_DEVICE_DRIVER = digitizer, which
 * registers digitizer_pointing_device_driver (digitizer_mouse_fallback.c:220).
 * This is where TRACKPAD_TOGGLE actually takes effect. */
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    if (!trackpad_enabled) {
        zoom_cleanup();
        mouse_report.x       = 0;
        mouse_report.y       = 0;
        mouse_report.h       = 0;
        mouse_report.v       = 0;
        mouse_report.buttons = 0;
        return mouse_report;
    }

    if (sniper_info_mode && timer_elapsed(info_timer) > INFO_DISPLAY_TIMEOUT) {
        sniper_info_mode = false;
    }

    return mouse_report;
}


// ==================== Digitizer pre-send hook ====================

#ifndef DIGITIZER_SWIPE2_THRESHOLD
#    define DIGITIZER_SWIPE2_THRESHOLD 40  // cursor pixels to trigger back/forward
#endif
#ifndef DIGITIZER_SWIPE3_THRESHOLD
#    define DIGITIZER_SWIPE3_THRESHOLD 50  // cursor pixels to trigger swipe keycode
#endif

/* Per-layer gesture modes. Split out of digitizer_pre_send_user() so the
 * user's scroll-inversion setting can be applied AFTER it — see the note in
 * digitizer_pre_send_user below. */
static void apply_layer_gestures(report_mouse_t *report) {
    static int scroll_carry_h = 0;
    static int scroll_carry_v = 0;
    static int swipe2_accum   = 0;
    static int swipe3_h       = 0;
    static int swipe3_v       = 0;

    /* Reset accumulators when the finger lifts so a partial swipe cannot
     * carry over into the next gesture. */
    if (digitizer_active_contacts != 1) {
        scroll_carry_h = 0; scroll_carry_v = 0;
        swipe2_accum   = 0;
        swipe3_h       = 0; swipe3_v = 0;
        return;
    }

    uint8_t  layer     = get_highest_layer(layer_state);
    uint16_t layer_bit = (1 << layer);

    if (user_config.scroll_layers & layer_bit) {
        /* 1-finger scroll. ROTATION_270: report->x is the horizontal axis and
         * report->y the vertical; both negated because the sensor reports the
         * inverted sign relative to scroll direction. */
        int sh_acc = -(int)report->x * (int)digitizer_get_scroll_scale() + scroll_carry_h;
        int sv_acc = -(int)report->y * (int)digitizer_get_scroll_scale() + scroll_carry_v;
        scroll_carry_h = sh_acc % 64;
        scroll_carry_v = sv_acc % 64;
        int _sh = sh_acc / 64; report->h = _sh < -127 ? -127 : _sh > 127 ? 127 : _sh;
        int _sv = sv_acc / 64; report->v = _sv < -127 ? -127 : _sv > 127 ? 127 : _sv;
        report->x = 0;
        report->y = 0;

    } else if (user_config.swipe2_layers & layer_bit) {
        /* 1-finger horizontal -> browser back/forward */
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
        /* 1-finger -> 3-finger swipe keycodes */
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
}

/* Called from digitizer_mouse_fallback.c:584, immediately before
 * host_mouse_send(). Only runs while digitizer_send_mouse_reports is true —
 * i.e. on a host that never claimed PTP: macOS, a UEFI screen, a KVM. Under
 * Windows/Linux PTP it is never called and the host owns pointer behaviour. */
void digitizer_pre_send_user(report_mouse_t *report) {
    /* Merge keyboard-side mouse buttons into the direct-send report. The
     * fallback path calls host_mouse_send() itself, bypassing QMK's mousekey
     * OR, so without this the DIP switch's KC_BTN1 (index 0, GP12 — see
     * dip_switch_update_user above) would do nothing in fallback mode. */
#ifdef MOUSEKEY_ENABLE
    report->buttons |= mousekey_get_report().buttons;
#endif

    apply_layer_gestures(report);

    /* Scroll inversion is applied LAST, deliberately, so it covers both
     * sources of scroll: the core's 2-finger scroll (already in report->v/h by
     * the time we are called, digitizer_mouse_fallback.c:480-481) and the
     * 1-finger scroll that apply_layer_gestures() writes on a scroll layer.
     * Applying it first would let the gesture engine overwrite v/h and silently
     * drop the setting on exactly the layers where scrolling is the point.
     *
     * NOTE — this is a deliberate DIVERGENCE from sofleplus2, where
     * scroll_dir_v/h are saved, loaded, exposed to the Vial GUI and toggled by
     * keycodes but never actually read to modify a report: the only inversion
     * there is the compile-time DIGITIZER_SCROLL_INVERT. The GUI toggle is
     * therefore inert on sofleplus2. Wiring it up here rather than copying a
     * dead control. This is independent of DIGITIZER_SCROLL_INVERT (config.h),
     * which stays as the macOS natural-scrolling default. */
    if (scroll_dir_v) report->v = -report->v;
    if (scroll_dir_h) report->h = -report->h;
}


// ==================== Process Record ====================

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
	switch (keycode) {
		#ifdef SUPER_ALT_TAB_ENABLE
		case CK_ATABF:
			if (record->event.pressed) {
				if (!is_alt_tab_active) {
					is_alt_tab_active = true;
					alt_tab_held_mod  = app_switch_mod();
					register_code(alt_tab_held_mod);
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
					alt_tab_held_mod  = app_switch_mod();
					register_code(alt_tab_held_mod);
				}
				alt_tab_timer = timer_read();
				register_code(KC_LSFT);
				register_code(KC_TAB);
			} else {
				unregister_code(KC_LSFT);
				unregister_code(KC_TAB);
			}
			break;

		case CK_ATMWU:	//Alt mouse wheel up
			if (record->event.pressed) {
				if (!is_alt_tab_active) {
					is_alt_tab_active = true;
					alt_tab_held_mod  = app_switch_mod();
					register_code(alt_tab_held_mod);
				}
				alt_tab_timer = timer_read();
				register_code(MS_WHLU);
			} else {
				unregister_code(MS_WHLU);
			}
			break;

		case CK_ATMWD:	//Alt mouse wheel down
			if (record->event.pressed) {
				if (!is_alt_tab_active) {
					is_alt_tab_active = true;
					alt_tab_held_mod  = app_switch_mod();
					register_code(alt_tab_held_mod);
				}
				alt_tab_timer = timer_read();
				register_code(MS_WHLD);
			} else {
				unregister_code(MS_WHLD);
			}
			break;
		#endif

        /* v1.10: matches sofleplus2. v1.02b fired Win+D, waited 500ms, then
         * Alt+F4 unconditionally — on macOS that is neither of those things.
         * Now one shortcut per host, and the blocking wait_ms(500) is gone. */
        case CK_PO:
            if (record->event.pressed) {
                if (host_is_apple()) {
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

        case OS_DETECTION_TOGGLE:
            if (record->event.pressed) toggle_os_detection();
            break;

        case AP_GLOB:
            if (record->event.pressed) {
                host_consumer_send(AC_NEXT_KEYBOARD_LAYOUT_SELECT);
            } else {
                host_consumer_send(0);
            }
            return false;

        case TP_INFO:
            if (record->event.pressed) g_tp_info_send_pending = true;
            return false;

        /* ---- Trackpad settings (hidden from the Vial GUI) ---- */
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
            break;

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

        /* ---- Per-layer gesture mode ---- */
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
            if (record->event.pressed) trackpad_layer_reset();
            break;

        case TRACKPAD_TOGGLE:
            if (record->event.pressed) {
                trackpad_enabled = !trackpad_enabled;
                if (!trackpad_enabled) zoom_cleanup();
                save_trackpad_enabled();
            }
            break;

        /* ---- Sniper ---- */
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

        case ZMTOG:
            if (record->event.pressed) {
                zoom_enabled = !zoom_enabled;
                if (!zoom_enabled) zoom_cleanup();
                save_zoom_setting();
            }
            break;
    }
	return true;
}


// ==================== Suspend / Wakeup ====================

/* Release any zoom modifier registered before suspend; the host may have lost
 * the key-down, so clear_mods() realigns our state with reality on resume. */
void suspend_wakeup_init_user(void) {
    zoom_cleanup();
    pinch_cleanup();
    clear_mods();
}

#ifdef OLED_ENABLE
void suspend_power_down_user(void) {
    oled_off();
}
#endif


// ==================== OLED ====================

#ifdef OLED_ENABLE

#include "oled_data.h"

/* Gesture bitmaps (32x11px each, 64 bytes per bitmap) */
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

static const char* get_trackpad_gesture_bitmap_by_mode(uint8_t mode) {
    switch (mode) {
        case 1:  return gesture_scroll;
        case 2:  return gesture_2finger;
        case 3:  return gesture_3finger;
        default: return gesture_default;
    }
}

/* Large layer number display */
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

/* PTP mode = the host claimed the Precision Touchpad protocol */
static bool is_ptp_mode(void) {
    return !digitizer_send_mouse_reports;
}

static void print_status_narrow(void) {
    uint8_t current_layer = get_highest_layer(layer_state);
    bool    is_slave      = !is_keyboard_master();

    /* Live on master, synced on slave via the 500ms auto-sync */
    uint8_t d_os         = is_slave ? g_tp_info_data.os_variant       : (uint8_t)detected_host_os();
    bool    d_ptp        = is_slave ? g_tp_info_data.ptp_mode         : is_ptp_mode();
    bool    d_trackpad   = is_slave ? g_tp_info_data.trackpad_on      : trackpad_enabled;
    uint8_t d_gesture    = is_slave ? g_tp_info_data.gesture_mode     :
                           (user_config.scroll_layers & (1 << current_layer)) ? 1 :
                           (user_config.swipe2_layers & (1 << current_layer)) ? 2 :
                           (user_config.swipe3_layers & (1 << current_layer)) ? 3 : 0;
    uint8_t d_dpi        = is_slave ? g_tp_info_data.dpi              : (uint8_t)digitizer_get_mouse_scale();
    uint8_t d_sniper_dpi = is_slave ? g_tp_info_data.sniper_dpi       : (uint8_t)digitizer_get_sniper_scale();
    uint8_t d_scroll     = is_slave ? g_tp_info_data.scroll_spd       : (uint8_t)scroll_speed;
    bool    d_sniper     = is_slave ? g_tp_info_data.sniper_active    : sniper_mode_active;
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
        oled_write_P(PSTR("     "), false);
    } else {
        switch ((os_variant_t)d_os) {
            case OS_MACOS:
            case OS_IOS:     oled_write(" MAC ", false); break;
            case OS_WINDOWS: oled_write(" WIN ", false); break;
            case OS_LINUX:   oled_write(" LNX ", false); break;
            default:         oled_write_P(PSTR("     "), false); break;
        }
    }

    /* Row 3: Num Lock (synced automatically by QMK split) */
    oled_set_cursor(0, 3);
    if (host_keyboard_led_state().num_lock) {
        oled_write_P(PSTR("NUMLK"), false);
    } else {
        oled_write_P(PSTR("     "), false);
    }

    /* Row 4: gesture bitmap */
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
            oled_set_cursor(0, 8); oled_write_P(PSTR("HOLD"),  false);
            oled_set_cursor(0, 9); oled_write_P(PSTR("MODS"),  false);
        } else {
            /* Gesture mode label */
            oled_set_cursor(0, 6);
            switch (d_gesture) {
                case 1:  oled_write_P(PSTR("SCROL"), false); break;
                case 2:  oled_write_P(PSTR("2SWPE"), false); break;
                case 3:  oled_write_P(PSTR("3SWPE"), false); break;
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
        oled_set_cursor(0, 6); oled_write_P(PSTR("NO"),    false);
        oled_set_cursor(0, 7); oled_write_P(PSTR("TRKPD"), false);
    }

    /* Sniper info override (master only — transient modes are not synced) */
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

    /* Row 10: 5-char layer name; row 11 is a blank spacer */
    oled_set_cursor(0, 10);
    {
        const char *_ln = g_oled_config.layer_names[current_layer < 10 ? current_layer : 0];
        for (uint8_t _i = 0; _i < 5; _i++)
            oled_write_char(_ln[_i] ? _ln[_i] : ' ', false);
    }

    /* Large layer number (rows 12-15) */
    display_large_layer_number(current_layer);
}

/* Landscape TP_INFO overlay for the slave OLED (default rotation, 128x32). */
static void print_tp_info_overlay(void) {
    oled_clear();
    uint8_t layer = get_highest_layer(layer_state);

    /* Row 0: name | OS | PTP/MOUSE */
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
            case 1:  oled_write_P(PSTR("SCROL"), false); break;
            case 2:  oled_write_P(PSTR("2SWPE"), false); break;
            case 3:  oled_write_P(PSTR("3SWPE"), false); break;
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
            if (last_input_activity_elapsed() > g_sleep_timeout) {
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
            if (last_input_activity_elapsed() > g_sleep_timeout) {
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

#endif // OLED_ENABLE


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

[0] = LAYOUT(
  /* [0][0] was CURSOR_SPEED_RESET in v1.02b. That keycode still exists in the
   * enum below, but like sofleplus2 it is now hidden from the Vial GUI (cursor
   * speed lives in the Trackpad tab), so it is a poor default for a physical
   * key. KC_MINS chosen because row 0 otherwise has no '-' and the right-hand
   * end already holds KC_GRV, so this adds a missing key rather than
   * duplicating one. Remap freely in Vial — this is only the flashed default. */
  KC_MINS,   KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    KC_0,  KC_GRV,
  KC_ESC,   KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,          KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,  KC_BSPC,
  KC_TAB,   KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                     KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN,  KC_QUOT,
  KC_LSFT,  KC_Z,   KC_X,    KC_C,    KC_V,    KC_B, KC_MUTE,    RGB_TOG,KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,  KC_RSFT,
                 KC_LGUI,KC_LALT,KC_LCTL, MO(2), KC_ENT,      KC_SPC,  MO(3), KC_RCTL, KC_RALT, KC_RGUI
),

[1] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______

),
[2] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[3] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[4] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[5] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[6] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[7] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[8] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
),
[9] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______
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

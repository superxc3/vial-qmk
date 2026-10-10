// Copyright 2026 XCMKB
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Status-readout OLED, ported from sofleplus2's tps65-507 keymap.
// Replaces the previous WPM-race / calcifer-sprite animation with a
// live trackpad/OS/layer status display, matching sofleplus2's layout.
// Both halves render at OLED_ROTATION_270.

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

__attribute__((unused)) static const char* get_trackpad_gesture_bitmap_by_mode(uint8_t mode) {
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

    /* Row 1: configurable keyboard name (default "CORNE") */
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
                oled_write_P(PSTR("     "), false);
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

/* TP_INFO overlay for slave OLED (rotated, rows 0-3 in this framebuffer's coordinates).
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

#endif

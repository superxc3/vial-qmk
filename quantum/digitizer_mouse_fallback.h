// Copyright 2025 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(POINTING_DEVICE_DRIVER_digitizer)
#    include "pointing_device.h"

const pointing_device_driver_t digitizer_pointing_device_driver;
extern bool                    digitizer_send_mouse_reports;

__attribute__((weak)) void digitizer_update_mouse_report(report_digitizer_t *report);
__attribute__((weak)) bool digitizer_update_gesture_state(void);

// Hardware gesture keycodes (mouse fallback mode only, macOS/Linux — Windows PTP unaffected).
// Override any of these in config.h to change the keycode fired for each hardware gesture.
// swipe_x_pos = physical LEFT (ROTATION_180 inverts), swipe_x_neg = physical RIGHT.
#ifndef DIGITIZER_HW_SWIPE_BACK_KC
#    define DIGITIZER_HW_SWIPE_BACK_KC    LGUI(KC_LBRC)  // Cmd+[  = back  (Safari, Chrome, Firefox)
#endif
#ifndef DIGITIZER_HW_SWIPE_FORWARD_KC
#    define DIGITIZER_HW_SWIPE_FORWARD_KC LGUI(KC_RBRC)  // Cmd+]  = forward
#endif
#ifndef DIGITIZER_HW_ZOOM_IN_KC
#    define DIGITIZER_HW_ZOOM_IN_KC       LGUI(KC_EQUAL) // Cmd+=  = zoom in  (universal)
#endif
#ifndef DIGITIZER_HW_ZOOM_OUT_KC
#    define DIGITIZER_HW_ZOOM_OUT_KC      LGUI(KC_MINUS) // Cmd+-  = zoom out (universal)
#endif

// Runtime scale control (mouse fallback mode only — affects macOS; no effect on Windows/Linux PTP)
void    digitizer_set_mouse_scale(uint8_t level);  // 1-6
uint8_t digitizer_get_mouse_scale(void);
void    digitizer_set_scroll_scale(uint8_t level);  // 1-8
uint8_t digitizer_get_scroll_scale(void);
void    digitizer_set_sniper_active(bool active);
bool    digitizer_get_sniper_active(void);
void    digitizer_set_sniper_scale(uint8_t level);  // 1-6
uint8_t digitizer_get_sniper_scale(void);
extern bool digitizer_taps_as_clicks;

// Release the Cmd key held for pinch-to-zoom and reset all pinch state.
// Call from suspend_wakeup_init_user() to ensure clean state after USB resume.
void pinch_cleanup(void);

// Current debounced finger count (0-5). Updated every digitizer frame.
// Available in digitizer_pre_send_user() to know how many fingers are down.
extern uint8_t digitizer_active_contacts;

// Called just before host_mouse_send() in the direct-send path.
// Override in keymap.c to transform the report for layer-based scroll/swipe.
// Zero out report->x/y if you redirect them to scroll or keycodes.
void digitizer_pre_send_user(report_mouse_t *report);
#endif

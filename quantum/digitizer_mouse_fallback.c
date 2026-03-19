// Copyright 2025 George Norton (@george-norton)
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(POINTING_DEVICE_DRIVER_digitizer)

// We can fallback to reporting as a mouse for hosts which do not implement trackpad support.

#    include <stdlib.h>
#    include "digitizer.h"
#    include "digitizer_mouse_fallback.h"
#    include "debug.h"
#    include "timer.h"
#    include "action.h"
#    include "quantum.h"
#    include "host.h"
// Pull in hardware gesture events when the azoteq IQS5xx is the digitizer sensor.
#    if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
#        include "drivers/sensors/azoteq_iqs5xx.h"
#    endif

#    ifndef DIGITIZER_MOUSE_TAP_DETECTION_TIMEOUT
#        define DIGITIZER_MOUSE_TAP_DETECTION_TIMEOUT 200
#    endif

#    ifndef DIGITIZER_MOUSE_TAP_DURATION
#        define DIGITIZER_MOUSE_TAP_DURATION 1
#    endif

#    ifndef DIGITIZER_MOUSE_TAP_DISTANCE
#        define DIGITIZER_MOUSE_TAP_DISTANCE 25
#    endif

// Scroll scale (fixed-point 8.8: 256 = 1.0x).
// Each scroll "tick" moves ~3 lines in most apps, so this needs to be
// much smaller than cursor scale to feel natural.
#    ifndef DIGITIZER_SCROLL_SCALE
#        define DIGITIZER_SCROLL_SCALE 10 // ~0.04x — very gentle, smooth scroll
#    endif

#    ifndef DIGITIZER_MOUSE_SWIPE_TIMEOUT
#        define DIGITIZER_MOUSE_SWIPE_TIMEOUT 1000
#    endif

#    ifndef DIGITIZER_MOUSE_SWIPE_DISTANCE
#        define DIGITIZER_MOUSE_SWIPE_DISTANCE 500
#    endif

#    ifndef DIGITIZER_MOUSE_SWIPE_THRESHOLD
#        define DIGITIZER_MOUSE_SWIPE_THRESHOLD 300
#    endif

#    ifndef DIGITIZER_SWIPE_LEFT_KC
#        define DIGITIZER_SWIPE_LEFT_KC QK_MOUSE_BUTTON_3
#    endif

#    ifndef DIGITIZER_SWIPE_RIGHT_KC
#        define DIGITIZER_SWIPE_RIGHT_KC QK_MOUSE_BUTTON_4
#    endif

#    ifndef DIGITIZER_SWIPE_UP_KC
#        define DIGITIZER_SWIPE_UP_KC KC_LEFT_GUI
#    endif

#    ifndef DIGITIZER_SWIPE_DOWN_KC
#        define DIGITIZER_SWIPE_DOWN_KC KC_ESC
#    endif

#    ifndef DIGITIZER_MIN_CPI
#        define DIGITIZER_MIN_CPI 50
#    endif

#    ifndef DIGITIZER_MAX_CPI
#        define DIGITIZER_MAX_CPI 1200
#    endif

// Constant pointer scale (fixed-point 8.8: 256 = 1.0x).
// macOS/Linux already apply their own mouse acceleration, so firmware
// uses a flat scale factor to convert touchpad deltas to mouse units.
// Sub-pixel accumulator preserves fractional data across frames.
#    ifndef DIGITIZER_MOUSE_SCALE
#        define DIGITIZER_MOUSE_SCALE 64 // 0.25x — let the OS do acceleration
#    endif

// Jitter filter: ignore single-sample deltas at or below threshold.
#    ifndef DIGITIZER_MOUSE_JITTER_THRESHOLD
#        define DIGITIZER_MOUSE_JITTER_THRESHOLD 1
#    endif

// Scroll direction: set to true to invert vertical scroll (natural scrolling).
// TPS43 with ROTATION_180 typically needs this for macOS.
#    ifndef DIGITIZER_SCROLL_INVERT
#        define DIGITIZER_SCROLL_INVERT false
#    endif

// Finger presence debounce: require tip==true for N consecutive reads
// before accepting a finger. Prevents single-frame phantom touches.
#    ifndef DIGITIZER_FINGER_DEBOUNCE
#        define DIGITIZER_FINGER_DEBOUNCE 2
#    endif

// Maximum velocity per frame (raw touchpad pixels). Deltas exceeding
// this are discarded as corrupted I2C reads.
#    ifndef DIGITIZER_MAX_VELOCITY
#        define DIGITIZER_MAX_VELOCITY 300
#    endif

// Minimum time (ms) between zoom keycode presses while hardware zoom gesture is active.
// Controls how coarse the zoom steps feel.
// Lower = faster zoom per gesture (more keypresses). Higher = slower, more deliberate.
// Safari zoom levels: 100%→115%→125%→150%→175%→200% — each Cmd+= is one step.
// At 400ms: a 3-second pinch fires ~7 steps max.
// Override in config.h to taste.
#    ifndef DIGITIZER_ZOOM_RATE_MS
#        define DIGITIZER_ZOOM_RATE_MS 400
#    endif


// DPI levels (fixed-point 8.8, 256 = 1.0x)
static const uint16_t mouse_scale_table[] = {
    16,   // Level 1 - ~0.06x (precise)
    32,   // Level 2 - ~0.12x (slow)
    64,   // Level 3 - ~0.25x (default)
    96,   // Level 4 - ~0.37x (fast)
    128,  // Level 5 - ~0.50x (very fast)
    192,  // Level 6 - ~0.75x (maximum)
};

// Scroll levels (fixed-point 8.8)
static const uint16_t scroll_scale_table[] = {
    1,    // Level 1 - ~0.004x (slowest)
    2,    // Level 2 - ~0.008x
    3,    // Level 3 - ~0.01x
    5,    // Level 4 - ~0.02x (default)
    10,   // Level 5 - ~0.04x
    18,   // Level 6 - ~0.07x
    30,   // Level 7 - ~0.12x
    50,   // Level 8 - ~0.20x (fastest)
};

#    define MOUSE_SCALE_LEVELS  (sizeof(mouse_scale_table) / sizeof(mouse_scale_table[0]))
#    define SCROLL_SCALE_LEVELS (sizeof(scroll_scale_table) / sizeof(scroll_scale_table[0]))

// Default level indices (1-based) derived from compile-time defines
#    ifndef DIGITIZER_MOUSE_SCALE_DEFAULT
#        define DIGITIZER_MOUSE_SCALE_DEFAULT 3
#    endif
#    ifndef DIGITIZER_SCROLL_SCALE_DEFAULT
#        define DIGITIZER_SCROLL_SCALE_DEFAULT 4
#    endif
#    ifndef DIGITIZER_SNIPER_SCALE_DEFAULT
#        define DIGITIZER_SNIPER_SCALE_DEFAULT 1
#    endif

static uint8_t mouse_scale_level  = DIGITIZER_MOUSE_SCALE_DEFAULT;
static uint8_t scroll_scale_level = DIGITIZER_SCROLL_SCALE_DEFAULT;
static uint8_t sniper_scale_level = DIGITIZER_SNIPER_SCALE_DEFAULT;
static bool    sniper_active      = false;

// Safety: release KC_LGUI in case it was left held by a prior firmware version.
// Called from suspend_wakeup_init_user() to ensure clean state after USB resume.
void pinch_cleanup(void) {
    unregister_code(KC_LGUI);
}

void digitizer_set_mouse_scale(uint8_t level) {
    if (level >= 1 && level <= MOUSE_SCALE_LEVELS) mouse_scale_level = level;
}
uint8_t digitizer_get_mouse_scale(void) {
    return mouse_scale_level;
}
void digitizer_set_scroll_scale(uint8_t level) {
    if (level >= 1 && level <= SCROLL_SCALE_LEVELS) scroll_scale_level = level;
}
uint8_t digitizer_get_scroll_scale(void) {
    return scroll_scale_level;
}
void digitizer_set_sniper_active(bool active) {
    sniper_active = active;
}
bool digitizer_get_sniper_active(void) {
    return sniper_active;
}
void digitizer_set_sniper_scale(uint8_t level) {
    if (level >= 1 && level <= MOUSE_SCALE_LEVELS) sniper_scale_level = level;
}
uint8_t digitizer_get_sniper_scale(void) {
    return sniper_scale_level;
}

#    ifdef DIGITIZER_REPORT_TAPS_AS_CLICKS
bool digitizer_taps_as_clicks = true;
#    else
bool digitizer_taps_as_clicks = false;
#    endif

#define CLIP(X, A, B) (X<A ? A : X>B ? B : X)

// This variable indicates that we are sending mouse reports. It will be updated
// during USB enumeration if the host sends a feature report indicating it supports
// Microsofts Precision Trackpad protocol. This variable can also be modified by users
// to force reporting as a mouse or as a digitizer.
bool                  digitizer_send_mouse_reports  = true;
static report_mouse_t mouse_report                  = {};
// Current debounced contact count — exported so keymap can use it in pre-send hook
uint8_t               digitizer_active_contacts     = 0;

static report_mouse_t digitizer_get_mouse_report(report_mouse_t _mouse_report);
static uint16_t       digitizer_get_cpi(void);
static void           digitizer_set_cpi(uint16_t cpi);
static void           digitizer_mouse_fallback_init(void);

const pointing_device_driver_t digitizer_pointing_device_driver = {.init = digitizer_mouse_fallback_init, .get_report = digitizer_get_mouse_report, .get_cpi = digitizer_get_cpi, .set_cpi = digitizer_set_cpi};

/**
 * @brief Initialize the pointing device driver.
 */
static void digitizer_mouse_fallback_init(void)
{
    // Status is set to SUCCESS by pointing_device_init() after this returns.
    // pointing_device_set_status(POINTING_DEVICE_STATUS_INIT_FAILED) can be called
    // here if hardware initialization fails.
}

/**
 * @brief Gets the current digitizer mouse report, the pointing device feature will send this is we
 * nave fallen back to mouse mode.
 *
 * @return report_mouse_t
 */
static report_mouse_t digitizer_get_mouse_report(report_mouse_t _mouse_report) {
    if (digitizer_send_mouse_reports) {
        report_mouse_t report = mouse_report;
        if (report.x || report.y || report.h || report.v) {
            dprintf("get_mouse_report: x=%d y=%d h=%d v=%d btn=0x%02X\n", report.x, report.y, report.h, report.v, report.buttons);
        }
        // Retain the button state, but drop any motion.
        memset(&mouse_report, 0, sizeof(report_mouse_t));
        mouse_report.buttons = report.buttons;
        return report;
    }
    return _mouse_report;
}

static uint16_t mouse_cpi = 400;

/**
 * @brief Gets the CPI used by the digitizer mouse fallback feature.
 *
 * @return the current CPI value
 */
static uint16_t digitizer_get_cpi(void) {
    return mouse_cpi;
}

/**
 * @brief Sets the CPI used by the digitizer mouse fallback feature.
 *
 *  @param[in] the new CPI value
 */
static void digitizer_set_cpi(uint16_t cpi) {
    mouse_cpi = CLIP(cpi, DIGITIZER_MIN_CPI, DIGITIZER_MAX_CPI);
    digitizer_set_scale((mouse_cpi * 100) / DIGITIZER_MAX_CPI);
}

// The gesture detection state machine will transition between these states.
typedef enum { None, Down, MoveScroll, Tapped, DoubleTapped, Drag, Swipe, Finished } State;

static State state     = None;
static int   tap_count = 0;

/**
 * \brief Signals that a gesture is in progress so digitizer_update_mouse_report should be called,
 * even if no new digitizer data is available.
 * @return true if update_mouse_report should run.
 */
bool digitizer_update_gesture_state(void) {
    return tap_count || state != None;
}

/**
 * \brief Generate a mouse report from the digitizer report. This function implements
 * a state machine to detect gestures and handle them.
 * @param[in] report a new digitizer report
 */
void digitizer_update_mouse_report(report_digitizer_t *report) {
    static int      contact_start_time  = 0;
    static int      contact_start_x     = 0;
    static int      contact_start_y     = 0;
    static int      tap_contacts        = 0;
    static int      last_contacts       = 0;
    static uint8_t  transition_guard    = 0; // frames to skip after contact-count change
    static uint16_t last_x              = 0;
    static uint16_t last_y              = 0;
    const uint16_t  x                  = report->fingers[0].x;
    const uint16_t  y                  = report->fingers[0].y;
    const uint32_t  duration           = timer_elapsed32(contact_start_time);
    int             contacts           = 0;

    memset(&mouse_report, 0, sizeof(report_mouse_t));

    // Fetch hardware gesture events from the IQS5xx chip (azoteq DIGITIZER driver only).
    // These are chip-disambiguated: scroll and zoom are mutually exclusive at source.
    // On other sensor drivers these remain zero (no-op).
#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
    azoteq_iqs5xx_gesture_events_0_t gesture_ev0 = {0};
    azoteq_iqs5xx_gesture_events_1_t gesture_ev1 = {0};
    int16_t gesture_x_delta = 0;
    azoteq_iqs5xx_get_digitizer_gesture_events(&gesture_ev0, &gesture_ev1, &gesture_x_delta);
#endif

    static uint8_t finger_debounce[DIGITIZER_FINGER_COUNT] = {0};
    for (int i = 0; i < DIGITIZER_FINGER_COUNT; i++) {
        if (report->fingers[i].tip) {
            if (finger_debounce[i] < DIGITIZER_FINGER_DEBOUNCE) {
                finger_debounce[i]++;
            }
        } else {
            finger_debounce[i] = 0;
        }
        if (finger_debounce[i] >= DIGITIZER_FINGER_DEBOUNCE) {
            contacts++;
        }
    }

    // Export current contact count for keymap pre-send hook
    digitizer_active_contacts = (uint8_t)contacts;

    // Load the guard on any contact-count change; both 1-finger and 2-finger
    // paths skip processing for this many frames after a change, preventing
    // cursor/scroll jumps caused by IQS5xx finger-slot reindexing.
    if (contacts != last_contacts) {
        transition_guard = 2;
    }

    switch (state) {
        case None: {
            if (contacts != 0) {
                state              = Down;
                contact_start_time = timer_read32();
                contact_start_x    = x;
                contact_start_y    = y;
                tap_contacts       = contacts;
            }
            break;
        }
        case Down: {
            const uint16_t distance_x = abs(contact_start_x - x);
            const uint16_t distance_y = abs(contact_start_y - y);
            tap_contacts              = MAX(contacts, tap_contacts);

            if (contacts == 0) {
                state              = Tapped;
                contact_start_time = timer_read32();
            } else if (contacts >= 3) {
                state = Swipe;
            } else if (duration > DIGITIZER_MOUSE_TAP_DETECTION_TIMEOUT || distance_x > DIGITIZER_MOUSE_TAP_DISTANCE || distance_y > DIGITIZER_MOUSE_TAP_DISTANCE) {
                state = MoveScroll;
            }
            break;
        }
        case Drag:
        case MoveScroll: {
            if (contacts == 0) {
                state = None;
                pinch_cleanup();
            } else if (contacts == 1) {
                // Going from 2→1 finger: release any held Cmd from pinch
                pinch_cleanup();
                if (transition_guard > 0) {
                    // Finger count recently changed — skip frame(s) to avoid
                    // cursor jump from IQS5xx finger-slot reindexing.
                } else {
                    int dx = (int)x - (int)last_x;
                    int dy = (int)y - (int)last_y;

                    // Velocity sanity check: discard impossibly large deltas
                    // (corrupted I2C read or finger identity shift).
                    if (abs(dx) > DIGITIZER_MAX_VELOCITY || abs(dy) > DIGITIZER_MAX_VELOCITY) {
                        dx = 0;
                        dy = 0;
                    }

                    // Simple jitter filter: discard tiny single-sample noise.
                    if (abs(dx) <= DIGITIZER_MOUSE_JITTER_THRESHOLD && abs(dy) <= DIGITIZER_MOUSE_JITTER_THRESHOLD) {
                        dx = 0;
                        dy = 0;
                    }

                    // Hardware 1-finger swipe (IQS5xx chip-detected, macOS/Linux only).
                    // Only fires on the default layer so special layers (scroll/swipe2/swipe3)
                    // handle their own 1-finger movement via digitizer_pre_send_user.
#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
                    if (digitizer_send_mouse_reports && get_highest_layer(layer_state) == 0) {
                        // ROTATION_180: flip_x → swipe_x_pos = physical LEFT = back
                        if (gesture_ev0.swipe_x_pos) {
                            tap_code16(DIGITIZER_HW_SWIPE_BACK_KC);
                            dx = 0; dy = 0; // suppress cursor output this frame
                        } else if (gesture_ev0.swipe_x_neg) {
                            tap_code16(DIGITIZER_HW_SWIPE_FORWARD_KC);
                            dx = 0; dy = 0;
                        }
                    }
#endif

                    // Constant scale with sub-pixel accumulator.
                    // Accumulate scaled values to preserve fractional movement
                    // across frames, giving smooth low-speed tracking.
                    static int carry_x = 0;
                    static int carry_y = 0;
                    uint16_t active_scale = sniper_active
                        ? mouse_scale_table[sniper_scale_level - 1]
                        : mouse_scale_table[mouse_scale_level - 1];
                    int sx = dx * active_scale + carry_x;
                    int sy = dy * active_scale + carry_y;
                    carry_x = sx % 256;
                    carry_y = sy % 256;
                    int out_x = sx / 256;
                    int out_y = sy / 256;

                    mouse_report.x = CLIP(out_x, -127, 127);
                    mouse_report.y = CLIP(out_y, -127, 127);
                }
            } else if (contacts == 3 && duration < DIGITIZER_MOUSE_SWIPE_TIMEOUT) {
                pinch_cleanup();
                state = Swipe;
            } else {
                if (transition_guard > 0) {
                    // Finger count recently changed — skip frame(s) to avoid
                    // scroll/zoom jump from IQS5xx finger-slot reindexing.
                } else {
                    // Hardware 2-finger zoom (IQS5xx chip-detected, mutually exclusive with scroll).
                    // Fires Cmd+= (zoom in) or Cmd+- (zoom out). Rate-limited by DIGITIZER_ZOOM_RATE_MS
                    // so a sustained pinch gesture produces a controlled number of zoom steps rather
                    // than one keypress per frame (which would zoom far too aggressively).
#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
                    if (digitizer_send_mouse_reports && gesture_ev1.zoom) {
                        static uint32_t last_zoom_time = 0;
                        if (timer_elapsed32(last_zoom_time) >= DIGITIZER_ZOOM_RATE_MS) {
                            if (gesture_x_delta > 0) {
                                tap_code16(DIGITIZER_HW_ZOOM_IN_KC);
                            } else if (gesture_x_delta < 0) {
                                tap_code16(DIGITIZER_HW_ZOOM_OUT_KC);
                            }
                            last_zoom_time = timer_read32();
                        }
                        // Zoom consumed: skip scroll output this frame.
                    } else {
#endif
                    // 2-finger scroll with sub-tick accumulator.
                    static int carry_h = 0;
                    static int carry_v = 0;
                    int raw_h = (int)x - (int)last_x;
                    int raw_v = (int)y - (int)last_y;

                    uint16_t active_scroll = scroll_scale_table[scroll_scale_level - 1];
                    int sh_acc = raw_h * active_scroll + carry_h;
                    int sv_acc = raw_v * active_scroll + carry_v;

                    carry_h = sh_acc % 256;
                    carry_v = sv_acc % 256;

                    int sh = sh_acc / 256;
                    int sv = sv_acc / 256;

                    if (DIGITIZER_SCROLL_INVERT) {
                        sv = -sv;
                        sh = -sh;
                    }

                    mouse_report.h = CLIP(sh, -127, 127);
                    mouse_report.v = CLIP(sv, -127, 127);
#if defined(DIGITIZER_DRIVER_azoteq_iqs5xx)
                    }  // end else (not zoom)
#endif
                }  // end else (transition_guard == 0)
            }  // end else (contacts >= 2)
            break;
        }
        case DoubleTapped:
        case Tapped: {
            tap_contacts = MAX(contacts, tap_contacts);
            if (contacts == 0 && last_contacts != contacts) {
                tap_count++;
                state              = DoubleTapped;
                contact_start_time = timer_read32();
            } else if (duration > DIGITIZER_MOUSE_TAP_DETECTION_TIMEOUT) {
                if (contacts > 0 && state == Tapped) {
                    state = Drag;
                } else {
                    tap_count++;
                    state = Finished;
                }
            }
            break;
        }
        case Swipe: {
            const int32_t distance_x = x - contact_start_x;
            const int32_t distance_y = y - contact_start_y;
            if (contacts == 0) {
                state = None;
            } else if (duration > DIGITIZER_MOUSE_SWIPE_TIMEOUT) {
                state = MoveScroll;
            } else if (digitizer_send_mouse_reports) {
                if (distance_x > DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_y) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                    // Swipe right
                    tap_code16(DIGITIZER_SWIPE_RIGHT_KC);
                    state = Finished;
                } else if (distance_x < -DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_y) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                    // Swipe left
                    tap_code16(DIGITIZER_SWIPE_LEFT_KC);
                    state = Finished;
                } else if (distance_y > DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_x) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                    // Swipe down
                    tap_code16(DIGITIZER_SWIPE_DOWN_KC);
                    state = Finished;
                } else if (distance_y < -DIGITIZER_MOUSE_SWIPE_DISTANCE && abs(distance_x) < DIGITIZER_MOUSE_SWIPE_THRESHOLD) {
                    // Swipe up
                    tap_code16(DIGITIZER_SWIPE_UP_KC);
                    state = Finished;
                }
            }
            break;
        }
        case Finished: {
            if (contacts == 0) {
                state = None;
            }
            break;
        }
    }
    static bool     tap      = false;
    static uint32_t tap_time = 0;
    if (tap_count) {
        if (timer_elapsed32(tap_time) > DIGITIZER_MOUSE_TAP_DURATION) {
            tap = !tap;
            if (!tap) {
                tap_count--;
            }
            tap_time = timer_read32();
        }
    }
    const bool button_pressed = tap || (state == Drag);
    // In mouse fallback mode, ignore hardware gesture buttons (report->buttonN)
    // to prevent the IQS5xx's built-in gesture engine (PRESS_AND_HOLD, TWO_FINGER_TAP)
    // from extending the hold time beyond what the software state machine intends.
    // In PTP/digitizer mode, hardware buttons pass through normally.
    const bool hw_buttons = !digitizer_send_mouse_reports;
    if ((hw_buttons && report->button1) || (tap_contacts == 1 && button_pressed)) {
        mouse_report.buttons |= 0x1;
        if (digitizer_taps_as_clicks) report->button1 = 1;
    }
    if ((hw_buttons && report->button2) || (tap_contacts == 2 && button_pressed)) {
        mouse_report.buttons |= 0x2;
        if (digitizer_taps_as_clicks) report->button2 = 1;
    }
    if ((hw_buttons && report->button3) || (tap_contacts == 3 && button_pressed)) {
        mouse_report.buttons |= 0x4;
        if (digitizer_taps_as_clicks) report->button3 = 1;
    }
    if (transition_guard > 0) transition_guard--;
    last_contacts = contacts;
    last_x        = x;
    last_y        = y;

    // Bypass the pointing_device pipeline and send mouse reports directly.
    // This works around macOS suppressing relative mouse motion from devices
    // that also expose a touchpad/digitizer HID interface.
    // Use change detection so we send button releases (all-zeros report) too.
    if (digitizer_send_mouse_reports) {
        // Allow keymap to transform the report (e.g. layer-based scroll/swipe)
        // before it is sent. The hook receives the full report including x,y
        // and may zero, redirect, or replace any fields.
        digitizer_pre_send_user(&mouse_report);
        static report_mouse_t last_direct_report = {};
        if (memcmp(&mouse_report, &last_direct_report, sizeof(report_mouse_t)) != 0) {
            host_mouse_send(&mouse_report);
            last_direct_report = mouse_report;
        }
        // Clear motion after direct send so the pointing_device pipeline
        // doesn't double-send movement deltas. Button state is kept so
        // the pipeline can still track it for its own change detection.
        mouse_report.x = 0;
        mouse_report.y = 0;
        mouse_report.h = 0;
        mouse_report.v = 0;
    }
}

// Default no-op pre-send hook. Override in keymap.c to implement layer-based
// scroll, swipe, or any other transformation of the mouse report before send.
__attribute__((weak)) void digitizer_pre_send_user(report_mouse_t *report) {}
#endif

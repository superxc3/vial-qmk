/* Copyright 2020 Josef Adamcik
 * Modification for VIA support and RGB underglow by Jens Bonk-Wiltfang
 * Modification for Vial support by Drew Petersen
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

#pragma once


#define EE_HANDS
//#define INIT_EE_HANDS_LEFT       // Auto-program EEPROM as LEFT on every boot (survives EEPROM resets)
//#define MASTER_LEFT
//#define MASTER_RIGHT
#define USB_VBUS_PIN GP19


#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP4
#define SERIAL_USART_RX_PIN GP1

/* v5.09h experiment: double the split link speed, 230400 -> 460800 baud.
 * QMK's default is SELECT_SOFT_SERIAL_SPEED=1 (230400) — the second-slowest
 * preset — which this board had never overridden. Every split transaction is a
 * BLOCKING master<->slave round trip, so link speed sets the main-loop floor,
 * and digitizer_task() only runs once per loop. At 230400 the per-loop stock
 * transaction plus the 88-byte VIALRGB_DIRECT_SYNC put the loop at ~14-20ms,
 * capping the trackpad at the observed 50-70Hz despite the chip being
 * configured for 9ms (~111Hz). Confirmed NOT I2C-bound: 400kHz + trackpad
 * alone on the master's bus changed nothing.
 * Safe here because the split interconnect is USB-C (shielded, controlled
 * impedance, discrete TX/RX conductors), not TRRS.
 *
 * REVERTED 2026-08-03: the rate increase is what regressed phantom clicks.
 * digitizer_mouse_fallback.c's `transition_guard` (:301) is counted in FRAMES
 * (2, set at :342, decremented once per call at :571), not milliseconds. Task
 * pacing is DIGITIZER_TASK_THROTTLE_MS = 10ms (the note below at "v5.07
 * RDY/RST experiment" claiming DIGITIZER_MOTION_PIN disables that throttle is
 * incorrect -- digitizer.c:197 applies it unconditionally). So the guard's
 * wall-clock coverage fell from ~28-40ms (507h, loop-paced at 14-20ms) to
 * ~20ms, against an IQS5xx contact-settling time of ~9-18ms. Spurious taps
 * then leak through and, when digitizer_taps_as_clicks is on, get injected as
 * report->button1 into the WINDOWS PTP report (fallback.c:559-569 is NOT
 * gated by digitizer_send_mouse_reports -- only host_mouse_send() at :580 is).
 * Restore 230400 until transition_guard is made time-based. */
#define SELECT_SOFT_SERIAL_SPEED 1   // 1 = 230400 baud (was 0 = 460800 in v5.09h)

/* i2c oled for left*/
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3
#define I2C1_CLOCK_SPEED 400000  // v5.08h experiment: back to in-spec 400kHz (IQS5xx Fast-mode max) to test
                                  // whether 1MHz FM+ overclock is corrupting I2C reads and causing phantom
                                  // clicks/movement (no CRC on IQS5xx I2C, so a bit error still ACKs).
                                  // Watch for OLED-animation-induced cursor stutter — that was the original
                                  // reason 1MHz was chosen (shared I2C1 bus contention with OLED).

#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9 //test from 9 to 5, 5 higher polling rate. 9 stable rgb led
//#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
#define AZOTEQ_IQS5XX_ROTATION_180 /*for tps65*/
#define POINTING_DEVICE_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)
#define DIGITIZER_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)
//#define AZOTEQ_IQS5XX_ROTATION_90 /*for tps65 rotate version*/

/* v5.07 RDY/RST experiment (this keymap only) — hand-wired IQS5xx lines:
 *   GP13 = RDY (comm-window strobe), GP14 = RST (active-low hard reset).
 * DIGITIZER_MOTION_PIN enables the quantum/digitizer.c gating hook + pin init;
 * the actual gating logic is the edge-triggered override in trackpad_rdy_rst.c
 * (one clean read per RDY rising edge = window-open moment; the other half is
 * paced by timer, never by the floating pin — both were failure modes of the
 * stock level-gate in earlier tests). NOTE: defining DIGITIZER_MOTION_PIN makes
 * quantum/digitizer.c ignore DIGITIZER_TASK_THROTTLE_MS above; pacing comes
 * from RDY edges, with automatic fallback to timed polling if RDY goes silent.
 * RST: pulsed at boot (pre-init, so the driver's register config survives) and
 * used for capped auto-recovery when RDY stops strobing. */
#define DIGITIZER_MOTION_PIN GP13
#define TRACKPAD_RST_PIN GP14



/* azoteq gesture settings */
// https://github.com/qmk/qmk_firmware/blob/master/docs/feature_pointing_device.md#gesture-settings
/* azoteq config: optimized for larger TPS65 dimensions */
#define AZOTEQ_IQS5XX_HOLD_TIME 300 // Default 300 v3.02
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 10 // Standard distance for larger TPS65 trackpad
#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE false // Disabled: Windows PTP handles this natively; hardware gesture conflicts with PTP causing phantom drag
#define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE false // Disabled: Windows PTP handles 2-finger right-click natively; hardware gesture causes double right-click
#define AZOTEQ_IQS5XX_SCROLL_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_ENABLE true //(Optional) Enable zoom gestures Zoom Out (Mouse Button 7) / Zoom In (Mouse Button 8)
// Zoom thresholds: TPS65 = 47.3 units/mm. Default initial=50 (~1mm) fires during normal scroll.
// Raised to ~3mm initial so only deliberate pinch/spread triggers zoom, not scroll finger variation.
#define AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE 150     // ~3.2mm span change to start zoom (default 50)
#define AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE 80  // ~1.7mm per step once zoom is active (default 25)
#define AZOTEQ_IQS5XX_MIN_STRENGTH 50               // Noise floor: ignore contacts with strength ≤50 to prevent phantom touches from EMI/capacitive noise
// 1-finger hardware swipe disabled: indistinguishable from fast cursor movement, causes accidental
// back/forward. Layer-based swipe (swipe2_layer) is the correct approach for deliberate swipe.
#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE false
#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE false

/* macOS-friendly 3-finger swipe keycodes (mouse fallback mode only, Windows PTP unaffected) */
/* ROTATION_270: physical RIGHT → decreasing digitizer x → invert x for correct native swipe direction */
#define DIGITIZER_SWIPE_X_INVERT 1
#define DIGITIZER_SWIPE_UP_KC    LCTL(KC_UP)     // macOS Mission Control
#define DIGITIZER_SWIPE_DOWN_KC  LCTL(KC_DOWN)   // macOS App Exposé
#define DIGITIZER_SWIPE_LEFT_KC  LCTL(KC_LEFT)   // macOS Previous Desktop
#define DIGITIZER_SWIPE_RIGHT_KC LCTL(KC_RIGHT)  // macOS Next Desktop

/* Mouse fallback tuning for macOS */
#define DIGITIZER_SCROLL_INVERT true            // Natural scrolling for macOS




/* OS detection — KVM auto-switching support intentionally dropped.
 *
 * OS_DETECTION_KEYBOARD_RESET reboots the MCU (soft_reset_keyboard -> mcu_reset)
 * on any USB bus reset, i.e. every KVM switch, resume and hub transition —
 * quantum/os_detection.c:87-99. In the field that produced Windows "random
 * reboots", immediate phantom gestures (a reset re-inits
 * digitizer_send_mouse_reports to true, so the board returns in macOS
 * mouse-fallback mode on a Windows host), and Linux failing to enumerate at all
 * (the reset lands mid-enumeration and loops). Disabled and validated on
 * tps65-507h; see that keymap's config.h for the full trace.
 *
 * Safe: the mac/win branches read detected_host_os(), which returns the live
 * detected_os (os_detection.c:179-181), independent of this macro. Known cost:
 * nothing clears setups_data, so once Windows is detected the guess latches and
 * will not return to MACOS until replug. Acceptable now that KVM is unsupported. */
  // #define OS_DETECTION_KEYBOARD_RESET

  // Only send one report per OS detection cycle
  #define OS_DETECTION_SINGLE_REPORT

  // NOTE: OS_DETECTION_INITIAL_TIMEOUT does not exist in QMK — it appears only in
  // our own config.h files and has never had any effect.
  #define OS_DETECTION_INITIAL_TIMEOUT 2500
  #define OS_DETECTION_DEBOUNCE 300          // Extended from 200ms to 300ms for stability

/* Power Management - OLED and RGB sleep/wake */
#undef OLED_TIMEOUT
#define OLED_TIMEOUT 0                // core OLED auto-off disabled; keymap owns sleep via g_sleep_timeout
#define RGB_MATRIX_TIMEOUT 60000      // boot default for g_rgb_matrix_timeout (1 min); overridden at runtime

/* v5.06: runtime-configurable shared OLED+RGB sleep timeout (set in Vial, whole minutes).
 * Stored internally in ms; clamped to [1 min, 30 min] — 0/never is disallowed to
 * protect the OLED from burn-in. */
#define DEFAULT_SLEEP_TIMEOUT_MS  60000    // 1 min
#define MIN_SLEEP_TIMEOUT_MS      60000    // 1 min
#define MAX_SLEEP_TIMEOUT_MS      1800000  // 30 min
#define RGB_MATRIX_SLEEP             // Enable RGB sleep when suspended
#define RGB_MATRIX_DEFAULT_ON true   // RGB on by default

/* v5.06: sync OLED/RGB sleep across both halves. Master broadcasts matrix +
 * encoder + pointing-device (trackpad) activity to the slave, so the slave's
 * last_input_activity_elapsed() tracks ALL input, not just its own keypresses.
 * Fixes one half sleeping while the other stays awake (e.g. from trackpad use).
 * NOTE: must be a config.h #define — the SPLIT_ACTIVITY_ENABLE=yes line in
 * rules.mk is not a recognised build feature and does nothing. */
#define SPLIT_ACTIVITY_ENABLE

/* Standardise the VialRGB "Customise" per-key effect with general effects and
 * layer indicators: track the Vial brightness slider against the max-brightness
 * ceiling instead of double-scaling against 255 (which rendered it ~39% dimmer). */
#define VIALRGB_CUSTOMISE_TRACK_MAX_BRIGHTNESS

/* v5.06 update to standardise layer indicator colour with the RGB effects.
 * Indicators track the Vial brightness slider but render at this % of the
 * ceiling, so a steady indicator doesn't out-shine the (usually sub-ceiling)
 * RGB effects. 100 = full ceiling; lower = dimmer indicators. */
#define INDICATOR_BRIGHTNESS_PCT 65




/* Keyboard name override for this keymap.
 * dd509 = dual disc, dual OLED, NO trackpad. Forked from tps65-509h; the only
 * source change is the OLED (slave renders the WPM rocket instead of a second
 * copy of the master status screen). The trackpad/digitizer config below is
 * inherited and left intact deliberately — with no IQS5xx fitted it is inert:
 * GP13 (RDY) floats, so trackpad_rdy_rst.c makes at most RST_MAX_ATTEMPTS (3)
 * bounded recovery attempts at boot and then settles into timed polling whose
 * I2C reads fast-fail on the address NACK. Cost is a dead Precision Touchpad
 * entry on the host; removing it properly would mean guarding the ~95
 * digitizer-referencing lines in keymap.c, which is deliberately out of scope. */
#undef PRODUCT
#define PRODUCT "SoflePLUS2 Dual Disc v5.09 Beta"



/* Vial UID for this specific keymap (unique — was still 508h's UID after copy,
 * which made Vial unable to tell this board apart from tps65-508h when both
 * are connected at once) */
#ifdef VIAL_ENABLE
#define VIAL_KEYBOARD_UID {0x2E, 0x77, 0xB1, 0x05, 0xC9, 0x43, 0x8A, 0xF6}
#endif

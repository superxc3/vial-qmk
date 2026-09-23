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

/* ============================================================================
 * sofleplus2legendaryrgb — Legendary RGB board (72 LEDs: 7 underglow + 29 per-key per side)
 *
 * Port of sofleplus2/keymaps/tps65-510 (v5.10 feature set: IDLE_RESEED +
 * CONFIDENCE_GATE phantom fix, OS_DETECTION_KEYBOARD_RESET removed, shared
 * OLED+RGB sleep timeout, OLED labels, scroll-inversion fix). Kept from this
 * board's own tps65-504:
 *   - I2C1 at 100 kHz and the 16 ms task throttle (bus filter caps, see below)
 *   - the spinning disc on FIVE DIP pins (GP12-GP16, sofleplus2legendaryrgb/config.h),
 *     synthesised onto matrix column 6 in keymap.c -- so GP13/GP14 are disc
 *     inputs here and the base's RDY/RST pin defines must not be used
 *   - VIALRGB_SPLIT_LEFT 36 (vial.json is identical to base)
 * Fixed for 72 LEDs in keymap.c: EEPROM indicator/OLED blocks moved above the
 * 216-byte direct-colour block, and the default indicator LEDs re-pointed at
 * the thumb keys (the base indices land on underglow here).
 * ============================================================================ */


#define EE_HANDS
//#define INIT_EE_HANDS_LEFT       // Auto-program EEPROM as LEFT on every boot (survives EEPROM resets)
//#define MASTER_LEFT
//#define MASTER_RIGHT
#define USB_VBUS_PIN GP19


#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP4
#define SERIAL_USART_RX_PIN GP1

/* i2c oled for left*/
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3
#define I2C1_CLOCK_SPEED 100000   // 100kHz standard mode — 1MHz FM+ too fast for 100pF filter caps on SDA/SCL (C1546); rise time ≈1µs at 4.7kΩ pull-up (kept from this board's tps65-504)

#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9
#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
// Poll both tasks at 16ms: each cycle costs ~5ms I2C (10-byte PD + 45-byte digitizer at
// 100kHz), giving 11ms free for RGB vs only 5ms at the base keymap's 10ms throttle.
// (kept from this board's tps65-504). These throttles are only honoured because
// DIGITIZER_MOTION_PIN is NOT defined below — see the RDY/RST note.
#define POINTING_DEVICE_TASK_THROTTLE_MS 16
#define DIGITIZER_TASK_THROTTLE_MS 16
//#define AZOTEQ_IQS5XX_ROTATION_90 /*for tps65 rotate version*/

/* RDY/RST — MUST NOT be defined on sofleplus2legendaryrgb.
 * The base tps65-510 defines DIGITIZER_MOTION_PIN GP13 / TRACKPAD_RST_PIN GP14
 * for hand-wired IQS5xx RDY/RST lines and builds trackpad_rdy_rst.c. On this
 * board GP13 and GP14 are the spinning disc's Down and Right DIP inputs
 * (DIP_SWITCH_PINS in sofleplus2legendaryrgb/config.h): quantum/digitizer.c would
 * re-init GP13 as a plain input (dropping the DIP pull-up) and
 * trackpad_rdy_rst.c would drive GP14 low at boot -- a phantom disc press and
 * two dead disc directions. Timed polling at DIGITIZER_TASK_THROTTLE_MS
 * instead, which is what this board's tps65-504 shipped on. */



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

/* --- v5.10 PHANTOM-CLICK FIX (RELEASE) --------------------------------------
 * Two independent changes: one removes the cause, one hides what is left.
 * Full investigation record kept in local notes; the driver header
 * (drivers/sensors/azoteq_iqs5xx.h) carries the datasheet argument for each.
 *
 * IDLE_RESEED -- the cause. Datasheet 3.4 refreshes the trackpad's per-channel
 * references only from LP1/LP2, and azoteq_iqs5xx_init() writes 255 ("never")
 * to the Idle-mode timeout at 0x0586, so the chip leaves LP1 minutes after boot
 * and never returns: the references are frozen for the rest of the session.
 * Measured 2026-08-30 -- the reference byte-identical across 9 h 20 min while
 * the raw counts drifted +55 counts toward the ~99-count touch threshold, then
 * 642 phantoms in 2 min 32 s once a channel crossed. RESEED re-references every
 * channel from its current counts: one I2C write on a transaction the driver
 * was making anyway, no ATI, no reset, no latency.
 *
 * CONFIDENCE_GATE -- the residue. touch_area is the number of channels the chip
 * grouped into a contact; 2138 of 2148 phantoms measured were area == 1, a
 * single Tx/Rx crossing, and the module datasheet's 7.0 mm minimum finger
 * cannot fit in one channel. Single-channel contacts are hidden rather than
 * flagged (reporting them with confidence = 0 cost tap-to-click and scroll
 * inertia on Windows PTP). Cost: 1-2 frames of onset delay on the 23 % of real
 * contacts that start at area 1 and grow, and about 4 % of very light taps
 * never reach area 2 at all and are dropped.
 *
 * Validated 2026-09-01 on the tps65-510h-probe build over a 2 h 41 m gaming
 * session: zero contacts, reference tracked continuously, no Re-ATI, and none
 * of the drift that precedes a storm.
 *
 * NOT enabled here: AZOTEQ_IQS5XX_PHANTOM_PROBE. It is diagnostic only and
 * widens the per-poll I2C read from 45 to 107 bytes. It lives in
 * tps65-510h-probe and must not be added to a client build. */
#define AZOTEQ_IQS5XX_IDLE_RESEED
#define AZOTEQ_IQS5XX_IDLE_RESEED_MS 300000     // reseed after 5 min with no area >= 2 contact
#define AZOTEQ_IQS5XX_CONFIDENCE_GATE
// #define AZOTEQ_IQS5XX_CONFIDENCE_MIN_AREA 2  // default; raise only with baseline data
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




/* KVM OS detection - Linux-optimized extended timeouts */

  /* v5.10 FIELD FIX: OS_DETECTION_KEYBOARD_RESET disabled (was enabled through
   * v5.07). os_detection.c resets the MCU whenever USB drops to
   * USB_DEVICE_STATE_INIT -- i.e. on every KVM switch, hub transition and
   * resume. In the field that produced Windows "random reboots", immediate
   * phantom gestures (a reset re-inits digitizer_send_mouse_reports to true, so
   * the board comes back in macOS mouse-fallback mode on a Windows host), and
   * Linux failing to enumerate: landing mid-probe kills the pending interface
   * requests, which is the "can't add hid device: -110" dmesg spam and the dead
   * trackpad that goes with it. Do not re-enable. */
  // #define OS_DETECTION_KEYBOARD_RESET

  // Only send one report per OS detection cycle
  #define OS_DETECTION_SINGLE_REPORT

  // Extended timeouts to prevent login loops on Linux systems
  #define OS_DETECTION_INITIAL_TIMEOUT 2500  // Extended from 1400ms to 2500ms for Linux SDDM compatibility
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




/* Keyboard name override for this keymap */
#undef PRODUCT
#define PRODUCT "SoflePLUS2 TPS65 v5.10 Legendary RGB"

/* PRODUCT_ID: board default 0x0287, same as sofleplus2/tps65-510 — the PID
 * allocation is per keymap variant (0x0287 vertical, 0x0288 horizontal,
 * 0x0289 TPS43), not per board, so a sofleplus2legendaryrgb and a sofleplus2 both
 * flashed tps65-510 collide on one host exactly as two sofleplus2 would. */



/* Vial UID: SHARED with sofleplus2/tps65-510 on purpose. This keymap's vial.json is
 * byte-identical to it (same 65-key layout, same encoder entries), so clients'
 * .vil files stay loadable across both boards. Give it its own UID only if the
 * vial.json ever diverges. */
#ifdef VIAL_ENABLE
#define VIAL_KEYBOARD_UID {0x6B, 0xE4, 0x29, 0xC7, 0x5D, 0x18, 0xA3, 0xF0}
#endif

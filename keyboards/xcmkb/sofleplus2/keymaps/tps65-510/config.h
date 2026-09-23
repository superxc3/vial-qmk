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

/* i2c oled for left*/
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3
#define I2C1_CLOCK_SPEED 1000000  // 1MHz FM+ — same board as TPS43; halves I2C hold time per read, reducing OLED lag from shared bus contention

#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9 //test from 9 to 5, 5 higher polling rate. 9 stable rgb led
#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
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




/* Keyboard name override for this keymap.
 *
 * NAMING RULE -- vial-web firmware-update detection (2026-09-16).
 * The web app reads this string over WebHID and matches it against
 * vial-web/firmware/manifest.json to tell the client which board they have and
 * whether an update exists. Keep it exactly in this form on every keymap:
 *
 *   <Family> [<TPS43|TPS65>] [Horizontal] v<major>.<minor>[<letter>] [<batch words>] [Beta ...]
 *
 *   Family       SoflePLUS2 (SoflePLUS = sofleplus1)
 *   Horizontal   h keymaps only, always paired with the h suffix (v5.10h)
 *   batch words  trailing phrase, matched whole: none = sofleplus2 (latest batch),
 *                "Legendary" = sofleplus2ano, "Legendary RGB" = sofleplus2legendaryrgb,
 *                "Signature RGB" = sofleplus2u
 *   Beta ...     pre-release, never offered as latest
 *
 * The version is compared by equality with the manifest's latest, so a release
 * changes ONLY the version token. Never rename the batch words. */
#undef PRODUCT
#define PRODUCT "SoflePLUS2 TPS65 v5.10"



/* Vial UID for this specific keymap (unique — was still 506's UID after copy) */
#ifdef VIAL_ENABLE
#define VIAL_KEYBOARD_UID {0x6B, 0xE4, 0x29, 0xC7, 0x5D, 0x18, 0xA3, 0xF0}
#endif

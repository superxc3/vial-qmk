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
//#define INIT_EE_HANDS_RIGHT   // Auto-program EEPROM as LEFT on every boot (survives EEPROM resets)
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
#define I2C1_CLOCK_SPEED 1000000  

//#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9
//#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
// Pure 10ms polling throttle. DIGITIZER_MOTION_PIN (main-loop GPIO poll of RDY) removed: after
// END_COMMS the IQS5xx needs ~200us to raise RDY; the very next scan saw RDY still low and read a
// half-closed communication window, getting stale/corrupt position data -> cursor skip.
// GP13 (RDY) is wired on PCB but intentionally unused. See George Norton's branch for rationale.
#define DIGITIZER_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)
//#define AZOTEQ_IQS5XX_ROTATION_90 /*for tps65 rotate version*/
#define AZOTEQ_IQS5XX_TPS43
#define AZOTEQ_IQS5XX_ROTATION_180 /*for tps43*/



/* azoteq gesture settings */
// https://github.com/qmk/qmk_firmware/blob/master/docs/feature_pointing_device.md#gesture-settings
/* azoteq config: optimized for larger TPS65 dimensions */
#define AZOTEQ_IQS5XX_HOLD_TIME 300 // Default 300 v3.02
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 10 // ~0.21mm (TPS43: 47.6 units/mm; same physical distance as TPS65 — resolution per mm is identical)
#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE false // Disabled: Windows PTP handles this natively; hardware gesture conflicts with PTP causing phantom drag
#define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE false // Disabled: Windows PTP handles 2-finger right-click natively; hardware gesture causes double right-click
#define AZOTEQ_IQS5XX_SCROLL_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_ENABLE true //(Optional) Enable zoom gestures Zoom Out (Mouse Button 7) / Zoom In (Mouse Button 8)
// Zoom thresholds: TPS43 = 47.6 units/mm. Default initial=50 (~1mm) fires during normal scroll.
// Raised to ~3mm initial so only deliberate pinch/spread triggers zoom, not scroll finger variation.
#define AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE 150     // ~3.2mm span change to start zoom (default 50)
#define AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE 80  // ~1.7mm per step once zoom is active (default 25)
#define AZOTEQ_IQS5XX_MIN_STRENGTH 50               // Noise floor: ignore contacts with strength ≤50 to prevent phantom touches from EMI/capacitive noise
// 1-finger hardware swipe disabled: indistinguishable from fast cursor movement, causes accidental
// back/forward. Layer-based swipe (swipe2_layer) is the correct approach for deliberate swipe.
#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE false
#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE false

/* macOS-friendly 3-finger swipe keycodes (mouse fallback mode only, Windows PTP unaffected) */
/* Swipe left/right are reversed because TPS43 uses ROTATION_180 which flips X axis */
#define DIGITIZER_SWIPE_UP_KC    LCTL(KC_UP)     // macOS Mission Control
#define DIGITIZER_SWIPE_DOWN_KC  LCTL(KC_DOWN)   // macOS App Exposé
#define DIGITIZER_SWIPE_LEFT_KC  LCTL(KC_RIGHT)  // macOS Next Desktop (reversed for ROTATION_180)
#define DIGITIZER_SWIPE_RIGHT_KC LCTL(KC_LEFT)   // macOS Previous Desktop (reversed for ROTATION_180)

/* Mouse fallback tuning for macOS (ROTATION_180 flips axes) */
#define DIGITIZER_SCROLL_INVERT true            // Natural scrolling for macOS





/* KVM OS detection - Linux-optimized extended timeouts */

  // Reset OS detection when keyboard resets/reconnects
  #define OS_DETECTION_KEYBOARD_RESET

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
#define INDICATOR_BRIGHTNESS_PCT 60



/* Keyboard name override for this keymap */
#undef PRODUCT
#define PRODUCT "SoflePLUS2 TPS43 v5.06 Beta"



/* Vial UID for this specific keymap */
#ifdef VIAL_ENABLE
#define VIAL_KEYBOARD_UID {0x7C, 0x2A, 0xD4, 0x91, 0xE6, 0x3F, 0xB8, 0x50}
#endif
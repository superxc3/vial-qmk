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
#define I2C1_CLOCK_SPEED 400000   // 400kHz FM (pcb7 removed C1546 — pull-ups give ~100ns rise, well in spec; lower current = no flicker)


//#define AZOTEQ_IQS5XX_TPS65
#define AZOTEQ_IQS5XX_REPORT_RATE 9
//#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
// DIGITIZER_TASK_THROTTLE_MS defined below near the pcb7 routing notes.
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

/* ============================================================
 * v7.00nr RIGHT-SIDE firmware (pcb7 board):
 * - GP25 = IQS5xx RDY pad (pcb7 routes RDY here). NOT used as DIGITIZER_MOTION_PIN
 *   because MOTION_PIN requires the pad to be physically soldered. Without the solder
 *   joint, GP25 floats HIGH (pulled up) and blocks all trackpad reads.
 *   Polling mode (DIGITIZER_TASK_THROTTLE_MS) is used instead — works whether or not
 *   the RDY pad is soldered.
 * - GP12 = IQS5xx RST pad. Pulsed LOW in keyboard_post_init_user. Safe even if
 *   the RST pad is not soldered (output goes nowhere, IQS5xx unaffected).
 * - col6 (GP25) removed from right matrix scan (same physical pin as RDY trace).
 * - DIP_SWITCH_ENABLE = yes (matches left side) to keep split transaction table
 *   identical on both halves. DIP_SWITCH_PINS overridden to {NO_PIN} — no physical
 *   switch on right, reads as always-OFF.
 * ============================================================ */

/* Polling mode — works with or without RDY pad soldered. */
#define DIGITIZER_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)

/* Right side matrix: col6 (GP25) shares trace with RDY pad — remove from scan. */
#define MATRIX_COL_PINS_RIGHT { GP27, GP26, GP22, GP20, GP23, GP21, NO_PIN }

/* No physical DIP switch on right. Enabled to match left-side split transaction table. */
#undef  DIP_SWITCH_PINS
#define DIP_SWITCH_PINS { NO_PIN }

// USB_VBUS_PIN GP19 handles master/slave auto-detection at runtime.





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
#define OLED_TIMEOUT 300000          // 5 minutes OLED timeout (override default)
#define RGB_MATRIX_TIMEOUT 300000    // 5 minutes RGB timeout
#define RGB_MATRIX_SLEEP             // Enable RGB sleep when suspended
#define RGB_MATRIX_DEFAULT_ON true   // RGB on by default



/* Keyboard name override for this keymap */
#undef PRODUCT
#define PRODUCT "SoflePLUS2 v7.00nr TPS43"



/* Vial UID for this specific keymap */
#ifdef VIAL_ENABLE
#define VIAL_KEYBOARD_UID {0xF2, 0x8A, 0x3D, 0x61, 0xB5, 0x9E, 0x4C, 0x29}
#endif
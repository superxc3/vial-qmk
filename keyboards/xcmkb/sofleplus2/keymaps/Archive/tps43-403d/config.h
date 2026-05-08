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


//#define EE_HANDS //since now only left 
#define MASTER_LEFT
#define USB_VBUS_PIN GP19


#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP4
#define SERIAL_USART_RX_PIN GP1

/* i2c oled for left*/
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3


//#define AZOTEQ_IQS5XX_TPS65
//#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/
//#define AZOTEQ_IQS5XX_ROTATION_90 /*for tps65 rotate version*/


#define AZOTEQ_IQS5XX_TPS43
#define AZOTEQ_IQS5XX_ROTATION_180 /*for tps43*/



/* azoteq gesture settings */
// https://github.com/qmk/qmk_firmware/blob/master/docs/feature_pointing_device.md#gesture-settings

/* azoteq config: adjust for smaller TPS43 dimensions */
#define AZOTEQ_IQS5XX_HOLD_TIME 300 // Default 300 v3.02
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 8 // Reduced for smaller trackpad (was 10 for TPS65)

#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE true //(Disabled) Emulates holding left click to select text.
#define AZOTEQ_IQS5XX_TAP_ENABLE true //(Disabled) Single tap for left click
#define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE true //(Disabled) Two-finger tap for right click
#define AZOTEQ_IQS5XX_ZOOM_ENABLE true //(Optional) Enable zoom gestures Zoom Out (Mouse Button 7) / Zoom In (Mouse Button 8)
//#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE true //(Optional) Enable swipe gestures X+ (Mouse Button 5) / X- (Mouse Button 4)
//#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE true //(Optional) Enable swipe gestures Y+ (Mouse Button 3) / Y- (Mouse Button 6)





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
#define PRODUCT "SoflePLUS2 v4.03d TPS43"

/* Vial UID for this specific keymap */
#define VIAL_KEYBOARD_UID {0x12, 0x38, 0x7D, 0x9C, 0x1C, 0x0E, 0x58, 0x43}
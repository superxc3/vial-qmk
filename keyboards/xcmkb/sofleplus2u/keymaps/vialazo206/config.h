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

//Replug issue v2.06
#define USB_VBUS_PIN GP19
//#define SPLIT_WATCHDOG_ENABLE


#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP4
#define SERIAL_USART_RX_PIN GP1

/* i2c oled for left*/
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3

/* i2c azoteq for right choc
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP3
#define I2C1_SCL_PIN GP2*/


//#define AZOTEQ_IQS5XX_TPS65
//#define AZOTEQ_IQS5XX_ROTATION_270 /*for tps65*/


#define AZOTEQ_IQS5XX_TPS43
#define AZOTEQ_IQS5XX_ROTATION_180



/* azoteq gesture settings */
// https://github.com/qmk/qmk_firmware/blob/master/docs/feature_pointing_device.md#gesture-settings

/* azoteq config: extend drag time */
#define AZOTEQ_IQS5XX_HOLD_TIME 500 // Default 300
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 10 // Default 50

#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE true //(Optional) Emulates holding left click to select text.
//#define AZOTEQ_IQS5XX_ZOOM_ENABLE true //(Optional) Enable zoom gestures Zoom Out (Mouse Button 7) / Zoom In (Mouse Button 8)
//#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE true //(Optional) Enable swipe gestures X+ (Mouse Button 5) / X- (Mouse Button 4)
//#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE true //(Optional) Enable swipe gestures Y+ (Mouse Button 3) / Y- (Mouse Button 6)





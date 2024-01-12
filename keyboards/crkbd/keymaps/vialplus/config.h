/*
Copyright 2019 @foostan
Copyright 2020 Drashna Jaelre <@drashna>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#define SPLIT_WPM_ENABLE
#define SPLIT_LAYER_STATE_ENABLE
#define SPLIT_LED_STATE_ENABLE
#define SPLIT_POINTING_ENABLE
#define POINTING_DEVICE_RIGHT
//#define SPLIT_HAPTIC_ENABLE

#define SUPER_ALT_TAB_ENABLE	//Enable super alt tab custom keycode(+178).

#define DEBOUNCE 5
#define ENCODER_RESOLUTION 2
#define TAP_CODE_DELAY 10

/* To configure amount of time between encoder keyup and keydown; added upon encoder_map; default match with the value of tap_code_delay*/
// Cannot replace with super alt tab
#    ifdef ENCODER_MAP_ENABLE
#define ENCODER_MAP_KEY_DELAY 10
#    endif

// Encoder enable callback https://github.com/qmk/qmk_firmware/blob/master/docs/feature_encoders.md
/*
#define ENCODERS_PAD_A { B2 }
#define ENCODERS_PAD_B { B6 }
#define ENCODERS_PAD_A_RIGHT { B6 }
#define ENCODERS_PAD_B_RIGHT { B2 }	
*/

#define VIAL_KEYBOARD_UID {0x3B, 0x6B, 0xA0, 0x29, 0x80, 0x56, 0xED, 0xD1}
#define VIAL_UNLOCK_COMBO_ROWS {0, 0}
#define VIAL_UNLOCK_COMBO_COLS {0, 1}


//#define USE_MATRIX_I2C
#ifdef KEYBOARD_crkbd_rev1_legacy
#    undef USE_I2C
#    define USE_SERIAL
#endif

/* Select hand configuration */

//#define MASTER_LEFT
//#define MASTER_RIGHT
#define EE_HANDS

#define USE_SERIAL_PD2

#ifdef RGBLIGHT_ENABLE
#    undef RGBLED_NUM
#    define RGBLIGHT_ANIMATIONS
#    define RGBLED_NUM 54
#    undef RGBLED_SPLIT
#    define RGBLED_SPLIT \
        { 27, 27 }
#    define RGBLIGHT_LIMIT_VAL 120
#    define RGBLIGHT_HUE_STEP  10
#    define RGBLIGHT_SAT_STEP  17
#    define RGBLIGHT_VAL_STEP  17
#endif



// ┌─────────────────────────────────────────────────┐
// │ r g b                                           │
// └─────────────────────────────────────────────────┘

//RGB MATRIX Lighting
#ifdef RGB_MATRIX_ENABLE
#define WS2812_DI_PIN D3    //    contemporary RGB data pin 
#define RGBLIGHT_LIMIT_VAL 120
#define RGBLIGHT_HUE_STEP  10
#define RGBLIGHT_SAT_STEP  17
#define RGBLIGHT_VAL_STEP  17
#define RGBLIGHT_SLEEP 			//Turn off LEDs when computer sleeping (+72)
#define RGB_DISABLE_WHEN_USB_SUSPENDED //As RGB light does not sleep, alternative code

#ifndef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 120
#endif

//#define VIALRGB_NO_DIRECT					//Save space
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS 	//This is for solidreactive effect
#define RGB_MATRIX_KEYPRESSES 			//This is for typing heatmap

// RGB Matrix lighting effect, need to add one by one

/*  RGB Mode mapped for the RGB Matrix system*/
#define ENABLE_RGB_MODE_PLAIN
#define ENABLE_RGB_MODE_BREATHE
#define ENABLE_RGB_MODE_RAINBOW
#define ENABLE_RGB_MODE_SWIRL

/*  RGB Matrix Effects*/

/* Preferred modes */
#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_CYCLE_ALL

/* All modes */
#define ENABLE_RGB_MATRIX_ALPHAS_MODS
#define ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
#define ENABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
//#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_BAND_SAT
#define ENABLE_RGB_MATRIX_BAND_VAL
#define ENABLE_RGB_MATRIX_BAND_PINWHEEL_SAT
#define ENABLE_RGB_MATRIX_BAND_PINWHEEL_VAL
#define ENABLE_RGB_MATRIX_BAND_SPIRAL_SAT
#define ENABLE_RGB_MATRIX_BAND_SPIRAL_VAL
//#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
//#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN_DUAL
#define ENABLE_RGB_MATRIX_CYCLE_PINWHEEL
#define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#define ENABLE_RGB_MATRIX_DUAL_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_PINWHEELS
#define ENABLE_RGB_MATRIX_RAINDROPS
#define ENABLE_RGB_MATRIX_JELLYBEAN_RAINDROPS
#define ENABLE_RGB_MATRIX_HUE_BREATHING
#define ENABLE_RGB_MATRIX_HUE_PENDULUM
#define ENABLE_RGB_MATRIX_HUE_WAVE
#define ENABLE_RGB_MATRIX_PIXEL_FRACTAL
#define ENABLE_RGB_MATRIX_PIXEL_FLOW
#define ENABLE_RGB_MATRIX_PIXEL_RAIN

/*These modes don't require any additional defines.*/
#define ENABLE_RGB_MATRIX_TYPING_HEATMAP
#define ENABLE_RGB_MATRIX_DIGITAL_RAIN

/*These modes also require the RGB_MATRIX_FRAMEBUFFER_EFFECTS define to be available.*/
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_WIDE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_CROSS
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTICROSS
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_NEXUS
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS
#define ENABLE_RGB_MATRIX_SPLASH
#define ENABLE_RGB_MATRIX_MULTISPLASH
#define ENABLE_RGB_MATRIX_SOLID_SPLASH
#define ENABLE_RGB_MATRIX_SOLID_MULTISPLASH

/*These modes also require the RGB_MATRIX_KEYPRESSES or RGB_MATRIX_KEYRELEASES define to be available. 
  Does not included because not per key rgb
  https://github.com/qmk/qmk_firmware/blob/master/docs/feature_rgb_matrix.md#rgb-matrix-effect-typing-heatmap-idrgb-matrix-effect-typing-heatmap
  
*/

#endif

#define OLED_FONT_H "keyboards/crkbd/lib/glcdfont.c"

/*Follow for gaming*/
#define USB_POLLING_INTERVAL_MS 1

/* Maximum of combo, tap dance in vial */
#define VIAL_TAP_DANCE_ENTRIES 32
#define VIAL_COMBO_ENTRIES 32
#define VIAL_KEY_OVERRIDE_ENTRIES 32

#ifdef MOUSEKEY_ENABLE
/* mouse settings inspired from adux */
// Set the mouse settings to a comfortable speed/accuracy trade-off,
// assuming a screen refresh rate of 60 Htz or higher
// The default is 50. This makes the mouse ~3 times faster and more accurate
#define MOUSEKEY_INTERVAL 16
// The default is 20. Since we made the mouse about 3 times faster with the previous setting,
// give it more time to accelerate to max speed to retain precise control over short distances.
#define MOUSEKEY_TIME_TO_MAX 40
// The default is 300. Let's try and make this as low as possible while keeping the cursor responsive
#define MOUSEKEY_DELAY 100
// It makes sense to use the same delay for the mouseweel
#define MOUSEKEY_WHEEL_DELAY 100
// The default is 100
#define MOUSEKEY_WHEEL_INTERVAL 50
// The default is 40
#define MOUSEKEY_WHEEL_TIME_TO_MAX 100
#endif

/* Define layer number */
#define DYNAMIC_KEYMAP_LAYER_COUNT 10

/* PLUS FEATURES */

// ┌─────────────────────────────────────────────────┐
// │ cirque pinnacle  pimoroni trackball             │
// └─────────────────────────────────────────────────┘

/* pointing device */
#ifdef POINTING_DEVICE_ENABLE
#define POINTING_DEVICE_ROTATION_180
#define PIMORONI_TRACKBALL_SCALE 1
#define POINTING_DEVICE_TASK_THROTTLE_MS 1
#endif

// ┌─────────────────────────────────────────────────┐
// │ a u d i o                                       │
// └─────────────────────────────────────────────────┘

// Audio settings // Check audio pin
// refer to https://github.com/qmk/qmk_firmware/blob/master/docs/feature_audio.md

// Audio pin cannot be defined under #ifdef AUDIO_ENABLE
// Audio cannot be defined at D4
#define AUDIO_PIN B5

#ifdef AUDIO_ENABLE
#define NO_MUSIC_MODE
#define STARTUP_SONG SONG(STARTUP_SOUND)	
#define AUDIO_PWM_DRIVER PWMD4
#define AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_B
#define AUDIO_STATE_TIMER GPTD4
#endif

	#define OLED_TIMEOUT 120000
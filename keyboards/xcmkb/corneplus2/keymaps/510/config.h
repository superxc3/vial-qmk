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
#define SPLIT_DIGITIZER_ENABLE
#define DIGITIZER_RIGHT
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
#define ENCODERS_PAD_A { GP23 }
#define ENCODERS_PAD_B { GP21 }
#define ENCODERS_PAD_A_RIGHT { GP21 }
#define ENCODERS_PAD_B_RIGHT { GP23 }	
*/

#define VIAL_KEYBOARD_UID {0x5C, 0x0D, 0xB7, 0x46, 0xA1, 0xF2, 0x38, 0xE9}
#define VIAL_UNLOCK_COMBO_ROWS {0, 0}
#define VIAL_UNLOCK_COMBO_COLS {0, 1}


//#define USE_MATRIX_I2C

/* Select hand configuration */


//#define MASTER_LEFT
//#define MASTER_RIGHT
#define EE_HANDS

/* Replug-in issue. NOTE: on xcmkb/corneplus this was supplied implicitly by
 * the rp2040_ce converter (-DUSB_VBUS_PIN=19U). Dropping CONVERT_TO drops that,
 * so it is declared explicitly here -- same pin, same behaviour. */
#define USB_VBUS_PIN GP19
#define SPLIT_WATCHDOG_ENABLE


/* Split comms: full-duplex UART on the RP2040 vendor/PIO driver, two data
 * conductors in the TRRS -- TX on GP9, RX on GP1 (the stock Corne data pin).
 * The same #define is compiled into both halves, so one half's TX wire lands
 * on the other half's RX by the cable's own crossover.
 *
 * DISC PCB ONLY: TX was GP8 on the earlier corneplus/corneplus2 PCB. The disc
 * revision moved it to GP9 and gave GP8 to SW1 (the disc click, below). This
 * keymap therefore breaks the split link on the older PCB -- use vialplus
 * (still TX GP8) there. */
#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP9
#define SERIAL_USART_RX_PIN GP1

// ┌─────────────────────────────────────────────────┐
// │ d i s c   ( 4 - w a y   +   c l i c k )          │
// └─────────────────────────────────────────────────┘

/* Same scheme as sofleplus2's disc:
 *  - the four directions are ordinary matrix keys on a 7th column, GP12
 *    (keyboard.json), crossed with the existing rows: [0..3][6] on the left
 *    half, [4..7][6] on the right, in the order left, up, right, down;
 *  - SW1, the centre click, is a plain GPIO on GP8 read as a DIP switch and
 *    replayed as a key press by dip_switch_update_user() in keymap.c, onto
 *    [3][0] (left) / [7][0] (right) -- thumb-row slots with no switch on the
 *    Corne PCB, so they are free for it.
 * Both halves get the same pins (reversible PCB). On a half with no disc
 * fitted GP8 idles high on its pull-up and GP12 is an open column, so it reads
 * as never pressed. SPLIT_DIP_SWITCH_ENABLE carries the slave half's click to
 * the master; indices are per hand (0 = left, 1 = right), see
 * quantum/dip_switch.h. */
#ifdef DIP_SWITCH_ENABLE
#define DIP_SWITCH_PINS { GP8 }
#define SPLIT_DIP_SWITCH_ENABLE
#endif

// ┌─────────────────────────────────────────────────┐
// │ e e p r o m                                      │
// └─────────────────────────────────────────────────┘

/* Wear-leveling sizes copied from sofleplus2's board config.h (32 KB logical,
 * 128 KB backing). Without them the RP2040 default is 4 KB, but keymap.c keeps
 * its own settings at 0x0FA0-0x1123: the per-key colour block, indicators, OLED
 * labels and sleep timeout all reached past 0x0FFF, the out-of-range writes
 * failed, and none of them survived a replug.
 *
 * DYNAMIC_KEYMAP_EEPROM_MAX_ADDR stops Vial's macro buffer just below that
 * block. At its default (end of EEPROM) the macro buffer runs over 0x0FA0
 * onward, and a macro reset zero-fills the whole buffer, wiping the trackpad,
 * RGB and OLED settings with it. Below the cap: eeconfig + VIA (0x29 bytes),
 * the keymap (10 layers x 8 x 7 x 2 = 1120), encoders (80), QMK settings (40),
 * tap dance / combos / key overrides (3 x 320), alt repeat keys (32 x 6 = 192,
 * on by default via build_vial.mk) -- macros start around 0x0981, which leaves
 * about 1.5 KB for them. keymap.c _Static_asserts the rest of the map. */
#define WEAR_LEVELING_LOGICAL_SIZE 32768
#define WEAR_LEVELING_BACKING_SIZE 131072
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 0x0F9F

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
#define WS2812_DI_PIN GP0    //    contemporary RGB data pin (was promicro D3)
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

#define OLED_FONT_H "keyboards/xcmkb/corneplus2/lib/glcdfont.c"

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
// │ a z o t e q   i q s 5 x x   t r a c k p a d      │
// └─────────────────────────────────────────────────┘

/* i2c bus: shared with OLED (I2C1, GP2/GP3), same as sofleplus2 */
#define I2C_DRIVER I2CD1
#define I2C1_SDA_PIN GP2
#define I2C1_SCL_PIN GP3
#define I2C1_CLOCK_SPEED 1000000

#define AZOTEQ_IQS5XX_TPS43
/* 270, not 90: on hardware (2026-10-10) ROTATION_90 moved the cursor right for a
 * left swipe and down for an up swipe -- both axes inverted. 270 keeps the same
 * X/Y swap and toggles both flips (driver: 90 = flip_y+swap, 270 = flip_x+swap). */
#define AZOTEQ_IQS5XX_ROTATION_270
#define AZOTEQ_IQS5XX_REPORT_RATE 9
#define POINTING_DEVICE_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)
#define DIGITIZER_TASK_THROTTLE_MS (AZOTEQ_IQS5XX_REPORT_RATE + 1)

/* Hand-wired IQS5xx lines: GP13 = RDY (comm-window strobe), GP14 = RST
 * (active-low hard reset). GP13 is the promicro-footprint pin formerly called
 * B0; GP14 is NOT on the Pro Micro footprint at all and is soldered straight
 * to the module pad -- which is the whole reason this board drops CONVERT_TO
 * (the converter's alias table has no name for GP14). See rules.mk.
 *
 * DIGITIZER_MOTION_PIN enables the quantum/digitizer.c gating hook + pin init;
 * the actual gating logic is the edge-triggered override in trackpad_rdy_rst.c
 * (one clean read per RDY rising edge, with automatic fallback to timed polling
 * if RDY goes silent, and RST-based recovery if the chip wedges). See
 * trackpad_rdy_rst.c for the full writeup -- this is the validated approach
 * from sofleplus2's rdyrst branch, not the stock level-gate (which reads
 * garbage / hogs the bus on this hardware). */
#define DIGITIZER_MOTION_PIN GP13
#define TRACKPAD_RST_PIN GP14

/* azoteq gesture settings, carried over from sofleplus2's TPS43 tuning */
// https://github.com/qmk/qmk_firmware/blob/master/docs/feature_pointing_device.md#gesture-settings
#define AZOTEQ_IQS5XX_HOLD_TIME 300
#define AZOTEQ_IQS5XX_SCROLL_INITIAL_DISTANCE 10
#define AZOTEQ_IQS5XX_PRESS_AND_HOLD_ENABLE false // Windows PTP handles this natively; hardware gesture conflicts with PTP causing phantom drag
#define AZOTEQ_IQS5XX_TWO_FINGER_TAP_ENABLE false // Windows PTP handles 2-finger right-click natively; hardware gesture causes double right-click
#define AZOTEQ_IQS5XX_SCROLL_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_ENABLE true
#define AZOTEQ_IQS5XX_ZOOM_INITIAL_DISTANCE 150
#define AZOTEQ_IQS5XX_ZOOM_CONSECUTIVE_DISTANCE 80
#define AZOTEQ_IQS5XX_MIN_STRENGTH 50 // Noise floor: ignore contacts with strength <=50 to prevent phantom touches
#define AZOTEQ_IQS5XX_SWIPE_X_ENABLE false
#define AZOTEQ_IQS5XX_SWIPE_Y_ENABLE false

/* Phantom-click fix, same as every sofleplus2/sofleplus1 510 keymap (the full
 * datasheet argument and measurements are in sofleplus2/keymaps/tps43-510/
 * config.h and drivers/sensors/azoteq_iqs5xx.h). Both are opt-in in the shared
 * driver, so without these defines this board ran without the fix.
 *  IDLE_RESEED     -- the cause: the driver keeps the chip out of LP1, so its
 *                     touch references freeze and the raw counts drift into
 *                     phantom contacts; this re-references every channel after
 *                     5 min with no real contact.
 *  CONFIDENCE_GATE -- the residue: hides single-channel (area 1) contacts,
 *                     which were ~99.5% of measured phantoms. */
#define AZOTEQ_IQS5XX_IDLE_RESEED
#define AZOTEQ_IQS5XX_IDLE_RESEED_MS 300000     // reseed after 5 min with no area >= 2 contact
#define AZOTEQ_IQS5XX_CONFIDENCE_GATE

/* Swipe keycodes: hardware swipes are off, and the software swipes follow the
 * cursor direction after rotation, so with ROTATION_270 giving correct cursor
 * motion the natural mapping below applies. */
#define DIGITIZER_SWIPE_UP_KC    LCTL(KC_UP)     // macOS Mission Control
#define DIGITIZER_SWIPE_DOWN_KC  LCTL(KC_DOWN)   // macOS App Exposé
#define DIGITIZER_SWIPE_LEFT_KC  LCTL(KC_LEFT)   // macOS Previous Desktop
#define DIGITIZER_SWIPE_RIGHT_KC LCTL(KC_RIGHT)  // macOS Next Desktop
#define DIGITIZER_SCROLL_INVERT true             // Natural scrolling for macOS

// ┌─────────────────────────────────────────────────┐
// │ v i a l r g b   i n d i c a t o r s / o l e d    │
// └─────────────────────────────────────────────────┘

/* Split RPC transactions used by the VialRGB indicator system, per-key colour
 * live-sync, and the OLED status sync (see keymap.c housekeeping_task_user). */
#define SPLIT_TRANSACTION_IDS_USER VIALRGB_DIRECT_SYNC, VIALRGB_INDICATOR_SYNC, TP_INFO_SYNC, VIALRGB_INDICATOR_LED_SYNC, AURORA_SYNC

/* Wake-up "Aurora tide" animation on plug-in / PC start-up (keymap.c). Comment
 * out to drop it entirely. */
#define AURORA_WAKE_ENABLE
#define RPC_M2S_BUFFER_SIZE 112

/* Standardise the VialRGB "Customise" per-key effect with general effects and
 * layer indicators: track the Vial brightness slider against the max-brightness
 * ceiling instead of double-scaling against 255. */
#define VIALRGB_CUSTOMISE_TRACK_MAX_BRIGHTNESS

/* Layer indicator colour renders at this % of the brightness ceiling, so a
 * steady indicator doesn't out-shine the (usually sub-ceiling) RGB effects.
 * 100 = full ceiling; lower = dimmer indicators. */
#define INDICATOR_BRIGHTNESS_PCT 65

/* Shared OLED + RGB sleep timeout (whole minutes on the wire, ms internally).
 * Runtime-configurable via Vial; 0/never is disallowed to protect the OLED
 * from burn-in. */
/* Sync OLED/RGB sleep across both halves: the master sends matrix, encoder and
 * trackpad activity to the slave, so each half's last_input_activity_elapsed()
 * covers ALL input. Without it each half only saw its own keys, and the half you
 * weren't touching timed out and slept alone. Same as sofleplus2 v5.06 -- and it
 * has to be a config.h #define; SPLIT_ACTIVITY_ENABLE in rules.mk does nothing. */
#define SPLIT_ACTIVITY_ENABLE

/* Mirror the master's matrix to the slave so keypress-reactive RGB effects
 * (splash, solid reactive, typing heatmap) react on both halves. Without it the
 * slave only saw its own keys: a press on the master half rippled onto the slave
 * side... never, while a slave-half press showed on both. Same as sofleplus2. */
#define SPLIT_TRANSPORT_MIRROR

#define DEFAULT_SLEEP_TIMEOUT_MS  60000    // 1 min
#define MIN_SLEEP_TIMEOUT_MS      60000    // 1 min
#define MAX_SLEEP_TIMEOUT_MS      1800000  // 30 min
#define RGB_MATRIX_SLEEP             // Enable RGB sleep when suspended
#define RGB_MATRIX_DEFAULT_ON true   // RGB on by default
#define RGB_MATRIX_TIMEOUT 60000     // boot default for g_rgb_matrix_timeout; overridden at runtime

// ┌─────────────────────────────────────────────────┐
// │ o s   d e t e c t i o n                          │
// └─────────────────────────────────────────────────┘

/* OS_DETECTION_KEYBOARD_RESET stays OFF. It was inherited from the pre-fix
 * sofleplus2 config and dropped there in d216a0e7f6 after it was traced to the
 * Windows reboot-on-connect, the phantom gestures, and the Linux
 * non-enumeration. OS_DETECTION_ENABLE itself stays on (rules.mk) -- the
 * keymap uses get_effective_os_detection() and the OS_DETECTION_TOGGLE key. */
// #define OS_DETECTION_KEYBOARD_RESET
// Only send one report per OS detection cycle
#define OS_DETECTION_SINGLE_REPORT
// Extended timeouts to prevent login loops on Linux systems
#define OS_DETECTION_INITIAL_TIMEOUT 2500
#define OS_DETECTION_DEBOUNCE 300

// ┌─────────────────────────────────────────────────┐
// │ a u d i o   ( r e m o v e d )                   │
// └─────────────────────────────────────────────────┘

/* Audio was dropped on corneplus2 (2026-09-07, at the user's request).
 * xcmkb/corneplus had AUDIO_ENABLE = yes / AUDIO_DRIVER = pwm_hardware on
 * AUDIO_PIN B5 (= GP9), driven by PWMD4 channel B with GPTD4 as the state
 * timer. Removing it also freed the PWM peripheral entirely -- nothing else
 * in this build uses it (the WS2812 vendor driver is PIO, backlight and
 * haptic are off), so HAL_USE_PWM and RP_PWM_USE_PWM4 came out of
 * halconf.h/mcuconf.h too.
 *
 * GP9 is NOT free on the disc PCB this keymap targets: it is the split TX
 * line (see the split comms block above). Audio would need another PWM pin.
 *
 * To bring audio back: AUDIO_ENABLE/AUDIO_DRIVER in rules.mk, HAL_USE_PWM in
 * halconf.h, RP_PWM_USE_PWM4 in mcuconf.h, and AUDIO_PIN GP9 +
 * AUDIO_PWM_DRIVER PWMD4 + AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_B +
 * AUDIO_STATE_TIMER GPTD4 here. GP9 is PWM slice 4 channel B, so that
 * driver/channel pairing is the one the pin requires. */

// core OLED auto-off disabled; keymap owns sleep via g_sleep_timeout (see oled.c)
#undef OLED_TIMEOUT
#define OLED_TIMEOUT 0
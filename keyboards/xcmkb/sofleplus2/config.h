#pragma once
#include "encoder.c"


//Debug Manager//
//#define DEBUG_ENABLE
//#define CONSOLE_ENABLE
#define SERIAL_NUMBER_USE_HARDWARE_ID TRUE    

#define OLED_BRIGHTNESS 100
#define SPLIT_WPM_ENABLE
#define SPLIT_LAYER_STATE_ENABLE
#define SPLIT_LED_STATE_ENABLE
#define SPLIT_HAPTIC_ENABLE
#define SPLIT_POINTING_ENABLE
#define POINTING_DEVICE_RIGHT
#define SPLIT_DIGITIZER_ENABLE
#define DIGITIZER_RIGHT



#ifdef DIP_SWITCH_ENABLE

#define DIP_SWITCH_PINS { GP12 }
#define SPLIT_DIP_SWITCH_ENABLE

#endif


/* key matrix size */
// Rows are doubled-up
#define MATRIX_ROWS 10 
#define MATRIX_COLS 7 /*original 6*/


#define DEBOUNCE 5
#define ENCODER_RESOLUTION 2
#define TAP_CODE_DELAY 10

/* Define layer number */
#define DYNAMIC_KEYMAP_LAYER_COUNT 10

/*Follow for gaming*/
#define USB_POLLING_INTERVAL_MS 1



/* To configure amount of time between encoder keyup and keydown; added upon encoder_map; default match with the value of tap_code_delay*/
#    ifdef ENCODER_MAP_ENABLE
#define ENCODER_MAP_KEY_DELAY 10
#    endif

	
/* Bootmagic Lite: hold top-left (left half) or top-right (right half) while plugging in USB */
#define BOOTMAGIC_LITE_ROW        0  // left half:  global row 0, col 0
#define BOOTMAGIC_LITE_COLUMN     0
#define BOOTMAGIC_LITE_ROW_RIGHT  5  // right half: global row 5, col 0
#define BOOTMAGIC_LITE_COLUMN_RIGHT 0

/* RP2040 boot setting */
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET // Activates the double-tap behavior
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 200U // Timeout window in ms in which the double tap can occur.
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_LED GP17 // Specify a optional status led by GPIO number which blinks when entering the bootloader

/* Improved EEPROM storage and wear leveling to prevent keymap loss after replugging */
/* v3.02 for 16mb. This configuration provides a 4:1 ratio, which is more robust and should offer better wear leveling performance */
#define WEAR_LEVELING_LOGICAL_SIZE 32768   // 32KB
#define WEAR_LEVELING_BACKING_SIZE 131072  // 128KB
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 32767  // Allow more Vial remaps

// NKRO removed

// Vial Support (UID moved to rules.mk)

#define SUPER_ALT_TAB_ENABLE	//Enable super alt tab custom keycode(+178).


/* Maximum of combo, tap dance in vial */

#ifdef VIAL_ENABLE
#define DYNAMIC_KEYMAP_MACRO_COUNT 32
#define VIAL_TAP_DANCE_ENTRIES 32
#define VIAL_COMBO_ENTRIES 32
#define VIAL_KEY_OVERRIDE_ENTRIES 32
#define VIAL_USER_ENTRIES 32
#endif


#ifdef CAPS_WORD_ENABLE
#define BOTH_SHIFTS_TURNS_ON_CAPS_WORD
#endif


#define WS2812_DI_PIN GP0




#ifdef RGB_MATRIX_ENABLE
/*RGB signature
#define RGBLED_NUM 72
#define RGB_MATRIX_LED_COUNT 72
#define RGB_MATRIX_SPLIT { 36, 36 }*/

/*RGB standard*/


/*RGB end here*/
#define DRIVER_LED_TOTAL RGBLED_NUM
#define SPLIT_TRANSPORT_MIRROR //https://github.com/qmk/qmk_firmware/blob/master/docs/config_options.md
#define RGB_MATRIX_SLEEP
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 100  // Max 120. 80 Too Dim. 100

//#define VIALRGB_NO_DIRECT					//Save space
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS 	//This is for solidreactive effect
#define RGB_MATRIX_KEYPRESSES 			//This is for typing heatmap

// RGB Matrix lighting effect, need to add one by one
// 2024.11.27 remove redundant effects
#define ENABLE_RGB_MATRIX_ALPHAS_MODS
#define ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
#define ENABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_BREATHING
#define ENABLE_RGB_MATRIX_BAND_SAT
#define ENABLE_RGB_MATRIX_BAND_VAL
#define ENABLE_RGB_MATRIX_BAND_PINWHEEL_SAT
#define ENABLE_RGB_MATRIX_BAND_PINWHEEL_VAL
#define ENABLE_RGB_MATRIX_BAND_SPIRAL_SAT
#define ENABLE_RGB_MATRIX_BAND_SPIRAL_VAL
#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
#define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN
#define ENABLE_RGB_MATRIX_CYCLE_OUT_IN_DUAL
#define ENABLE_RGB_MATRIX_CYCLE_PINWHEEL
#define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#define ENABLE_RGB_MATRIX_DUAL_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#define ENABLE_RGB_MATRIX_RAINBOW_PINWHEELS
#define ENABLE_RGB_MATRIX_FLOWER_BLOOMING
#define ENABLE_RGB_MATRIX_RAINDROPS
#define ENABLE_RGB_MATRIX_JELLYBEAN_RAINDROPS
#define ENABLE_RGB_MATRIX_HUE_BREATHING
#define ENABLE_RGB_MATRIX_HUE_PENDULUM
#define ENABLE_RGB_MATRIX_HUE_WAVE
#define ENABLE_RGB_MATRIX_PIXEL_FRACTAL
#define ENABLE_RGB_MATRIX_PIXEL_FLOW
#define ENABLE_RGB_MATRIX_PIXEL_RAIN
#define ENABLE_RGB_MATRIX_STARLIGHT
#define ENABLE_RGB_MATRIX_STARLIGHT_DUAL_HUE
#define ENABLE_RGB_MATRIX_STARLIGHT_DUAL_SAT
#define ENABLE_RGB_MATRIX_RIVERFLOW

/*These modes don't require any additional defines.*/
#define ENABLE_RGB_MATRIX_TYPING_HEATMAP
#define ENABLE_RGB_MATRIX_DIGITAL_RAIN 

#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
//These modes also require the RGB_MATRIX_FRAMEBUFFER_EFFECTS define to be available.
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

#endif


/* Per-key RGB split sync: send g_direct_mode_colors slave portion (29 LEDs x 3 bytes = 87) */
/* VIALRGB_INDICATOR_SYNC: send indicator_config_t (110 bytes) from master to slave */
/* OLED_SCREEN_B_SYNC: send screen_b widget slots (16 bytes) from master to slave */
#define SPLIT_TRANSACTION_IDS_USER VIALRGB_DIRECT_SYNC, VIALRGB_INDICATOR_SYNC, OLED_SCREEN_B_SYNC, TP_INFO_SYNC
#define RPC_M2S_BUFFER_SIZE 112

/* oled i2c */
#define OLED_TIMEOUT 120000
#undef I2C_DRIVER
#define OLED_DISPLAY_ADDRESS 0x3C




#include "sofleplus2ano.h"

//following for dragscroll
/*
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
*/
#ifdef SWAP_HANDS_ENABLE

__attribute__ ((weak))
const keypos_t PROGMEM hand_swap_config[MATRIX_ROWS][MATRIX_COLS] =
    // The LAYOUT macro could work for this, but it was harder to figure out the
    // identity using it.

    // This is the identity layout.
/*
{ \
    { {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0} }, \
    { {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1} }, \
    { {0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2} }, \
    { {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3} }, \
    { {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4} }, \
    \
    { {0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5} }, \
    { {0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6} }, \
    { {0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7} }, \
    { {0, 8}, {1, 8}, {2, 8}, {3, 8}, {4, 8}, {5, 8} }, \
    { {0, 9}, {1, 9}, {2, 9}, {3, 9}, {4, 9}, {5, 9} }, \
};
*/

    // This is the mirror, q <-> p, w <-> o, etc...
{ \
  { {0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5} }, \
  { {0, 6}, {1, 6}, {2, 6}, {3, 6}, {4, 6}, {5, 6} }, \  // Joystick buttons
  { {0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7} }, \
  { {0, 8}, {1, 8}, {2, 8}, {3, 8}, {4, 8}, {5, 8} }, \
  { {0, 9}, {1, 9}, {2, 9}, {3, 9}, {4, 9}, {5, 9} }, \
                                                     \
  { {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0} }, \
  { {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1} }, \
  { {0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2}, {5, 2} }, \
  { {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3} }, \
  { {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4}, {5, 4} }, \
};




#endif


#    ifdef ENCODER_MAP_ENABLE
const uint8_t PROGMEM encoder_hand_swap_config[NUM_ENCODERS] = {1, 0};
#    endif


//When not using encoder_map_enable=yes
bool encoder_update_kb(uint8_t index, bool clockwise) {
    return encoder_update_user(index, clockwise);
}
	
#ifdef RGB_MATRIX_ENABLE

  // Physical Layout
  // Columns
  // 0  1  2  3  4  5  6  7  8  9  10 11 12 13
  //                                           ROWS
  // 12 13 22 23 32 33       33 32 23 22 13 12  0
  //    02    03    04       04    03    02
  // 11 14 21 24 31 34       34 31 24 21 14 11  1
  //                01       01
  // 10 15 20 25 30 35       35 30 25 20 15 10  2
  //
  // 09 16 19 26 29 36       36 29 26 19 16 09  3
  //
  //     08 17 18 27 28     28 27 18 17 08      4
  //    07    06    05       05    06    07
		
		

led_config_t g_led_config = {
	//update on 2024.11.27 for LED position, remove a few redundant light effect in config.h
	//      0,   1,   2,   3,   4,  5 COL
    {
        {  35,  28,  27,  18,  17,  7 },
        {  34,  29,  26,  19,  16,  8 },
        {  33,   30,  25,  20,  15,  9},
        {  32,   31,  24,  21,  14,  10},
        {  23,  22,  13,  12,  11, NO_LED},
        {  71,  64,  63,  53,  52,  43},
        {  70,  65,  62,  54,  51,  44},
        {  69,  66,  61,  55,  50,  45},
        {  68,  67,  60,  56,  49,  46},
        {  59,  58,  57,  48,  47,NO_LED }		
    },
    {
       // Left side underglow
        {96, 40}, {16, 20}, {48, 10}, {80, 18}, {88, 60}, {56, 57}, {24,60},
        // Left side Matrix
        {102,0}, {102,16}, {102,32}, {102,48}, {102,64},
        {81,0}, {81,16}, {81,32}, {81,48}, {81,64},
        {61,0}, {61,16}, {61,32}, {61,48}, {61,64},
        {41,0}, {41,16}, {41,32}, {41,48}, {41,64},
        {20,0}, {20,16}, {20,32}, {20,48}, {20,64},
        {0,0}, {0,16}, {0,32}, {0,48},


        // Right side underglow
        {128, 40}, {208, 20}, {176, 10}, {144, 18}, {136, 60}, {168, 57}, {200,60},
        // Right side Matrix
		/*
		{224,64}, {224,48}, {224,32}, {224,16}, {224,0},	
		{204,64}, {204,48}, {204,32}, {204,16}, {204,0},	
		{183,64}, {183,48}, {183,32}, {183,16}, {183,0},	
		{163,64}, {163,48}, {163,32}, {163,16}, {163,0},	
		{143,64}, {143,48}, {143,32}, {143,16}, {143,0},*/	
		
		{224,0}, {224,16}, {224,32}, {224,48}, {224,64},
		{204,0}, {204,16}, {204,32}, {204,48}, {204,64},
		{183,0}, {183,16}, {183,32}, {183,48}, {183,64},
		{163,0}, {163,16}, {163,32}, {163,48}, {163,64},
		{143,0}, {143,16}, {143,32}, {143,48}, {143,64},
		{122,0}, {122,16}, {122,32}, {122,48},
    },
    {
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT
    }
};
#endif



#include "transactions.h"
#include <string.h>

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif // CONSOLE_ENABLE



/*
bool shutdown_kb(bool jump_to_bootloader) {
    if (!shutdown_user(jump_to_bootloader)) {
        return false;
    }}


#ifdef RGBLIGHT_ENABLE
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
    rgblight_setrgb(RGB_RED);
#endif // RGBLIGHT_ENABLE
#ifdef RGB_MATRIX_ENABLE
    void rgb_matrix_update_pwm_buffers(void);
    rgb_matrix_set_color_all(RGB_RED);
    rgb_matrix_update_pwm_buffers();
#endif // RGB_MATRIX_ENABLE
    return true;
}
*/

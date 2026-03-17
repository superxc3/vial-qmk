 /* Copyright 2020 Josef Adamcik
  * Modification for VIA support and RGB underglow by Jens Bonk-Wiltfang
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

#include QMK_KEYBOARD_H



#ifdef DIP_SWITCH_ENABLE
bool dip_switch_update_user(uint8_t index, bool active) { 
    switch (index) {
        case 0:
            if(active) { //If switch is pressed
                register_code(KC_BTN1); 
            } else { //If switch is not pressed
                unregister_code(KC_BTN1);
            }
            break;
    }
    return true;
}
#endif


#ifdef SUPER_ALT_TAB_ENABLE
	bool is_alt_tab_active = false; // Super Alt Tab Code
	uint16_t alt_tab_timer = 0;
#endif

void matrix_scan_user(void) {

	//sentence_case_task();
	#ifdef SUPER_ALT_TAB_ENABLE
		if (is_alt_tab_active) {	//Allows for use of super alt tab.
			if (timer_elapsed(alt_tab_timer) > 1000) {
				unregister_code(KC_LALT);
				is_alt_tab_active = false;
			}
		}
	#endif

}

// Variables for custom keycodes

#ifdef VIA_ENABLE
	enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
    CK_ATABF = QK_KB_0,
    CK_ATABR,
	CK_ATMWU,	//Alt mouse wheel up 
	CK_ATMWD,	//Alt mouse wheel down
    CK_PO,		//Power options
	SCROLL_DIR_V, //滾輪上下
	SCROLL_DIR_H, //滾輪左右
    CURSOR_SPEED_UP, // Increase cursor speed
    CURSOR_SPEED_DN, // Increase cursor speed
    CURSOR_SPEED_RESET, // Reset cursor speed to default
    DRAGSCROLL_MODE,
    DRAGSCROLL_MODE_TOGGLE,
	};
#else
	enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
	CK_ATABF = SAFE_RANGE,
	CK_ATABR,
	CK_ATMWU,	//Alt mouse wheel up 
	CK_ATMWD,	//Alt mouse wheel down
    CK_PO,
	SCROLL_DIR_V,
	SCROLL_DIR_H,
    CURSOR_SPEED_UP, // Increase cursor speed
    CURSOR_SPEED_DN, // Increase cursor speed
    CURSOR_SPEED_RESET, // Reset cursor speed to default
	DRAGSCROLL_MODE,
    DRAGSCROLL_MODE_TOGGLE,
	};
#endif


//trackball led and haptic not working for layer indicator, thanks Drashna for pointing out
void housekeeping_task_user(void) {
    static layer_state_t state = 0;
    if (layer_state != state) {
        state = layer_state_set_user(layer_state);
    }
}

//RGB matrix layer indicator, using HSV allows the brightness to be limited

/*
Host LED status (capslock) and layer from 1 to 8 only. layer 0 and layer 9 for preset rgb effect
the indicator for rgb underglow only 
*/

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    uint8_t led_indices[] = {0,1,2,3,4,5,6,11, 12, 13, 22, 23, 36, 37, 38, 39, 40, 41, 42, 47, 48, 49, 58, 59}; // Shared LED indices - updated on 11.27.2024 only above thumb, underglow no, this is for signature with underglow led

    if (host_keyboard_led_state().caps_lock) {
        for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
            rgb_matrix_set_color(led_indices[i], 128, 0, 0); // Dim red for Caps Lock
        }
    }

    switch (get_highest_layer(layer_state)) {
        case 1:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 128, 0, 128); // Dim purple for Layer 1
            }
            break;
        case 2:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
				rgb_matrix_set_color(led_indices[i], 0, 255, 127); // Dim spring green for Layer 4(2) 11.27.2024 updated for better color scheme

            }
            break;
        case 3:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 0, 128, 128); // Dim cyan for Layer 3
            }
            break;
        case 4:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 255, 128, 0); // Dim orange for Layer 2(4)
            }
            break;
        case 5:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 0, 0, 128); // Dim blue for Layer 5
            }
            break;
        case 6:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 128, 0, 128); // Dim magenta for Layer 6
            }
            break;
        case 7:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 255, 192, 203); // Dim pink for Layer 7
            }
            break;
        case 8:
            for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                rgb_matrix_set_color(led_indices[i], 255, 215, 0); // Dim gold for Layer 8
            }
            break;
        default:
            break;
    }

    return false; // Allow RGB matrix to handle colors when no indicators are active
}




/* AZOTEQ cofig to tame the scroll*/  //Dasky https://discord.com/channels/440868230475677696/867530303261114398/1169658779864940554
/* 20240609 - change from 6 to 8, make scrolling even slower */


#define DEFAULT_CURSOR_SPEED 1
#define DRAGSCROLL_BUFFER_SIZE 10 // Define this as needed
#define SCROLL_AVERAGE 8

// Drag scroll state structure
typedef struct {
    bool is_dragscroll_enabled;
    int16_t scroll_buffer_x;
    int16_t scroll_buffer_y;
} sofleplus_dragscroll_t;

// Global variables
static int16_t cursor_speed = DEFAULT_CURSOR_SPEED;
static bool scroll_dir_v = false;
static bool scroll_dir_h = false;
static sofleplus_dragscroll_t g_dragscroll_state = {0};

// Function Prototypes
void sofleplus_set_pointer_dragscroll_enabled(bool enable);
bool sofleplus_get_pointer_dragscroll_enabled(void);
void sofleplus_update_dragscroll_state(report_mouse_t* mouse_report);
void keyboard_post_init_user(void);
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report);
bool process_record_user(uint16_t keycode, keyrecord_t *record);

// Function Definitions
void sofleplus_set_pointer_dragscroll_enabled(bool enable) {
    g_dragscroll_state.is_dragscroll_enabled = enable;
}

bool sofleplus_get_pointer_dragscroll_enabled(void) {
    return g_dragscroll_state.is_dragscroll_enabled;
}

void sofleplus_update_dragscroll_state(report_mouse_t* mouse_report) {
    if (g_dragscroll_state.is_dragscroll_enabled) {
        g_dragscroll_state.scroll_buffer_x += mouse_report->x;
        g_dragscroll_state.scroll_buffer_y += mouse_report->y;

        mouse_report->x = 0; // Reset mouse movement
        mouse_report->y = 0;

        // Calculate absolute values
        int16_t abs_x = abs(g_dragscroll_state.scroll_buffer_x);
        int16_t abs_y = abs(g_dragscroll_state.scroll_buffer_y);

        // Determine the dominant scroll direction
        static int8_t current_scroll_direction = 0; // 0: none, 1: vertical, -1: horizontal

        // Set initial scroll direction based on the first significant movement
        if (current_scroll_direction == 0) {
            if (abs_y > DRAGSCROLL_BUFFER_SIZE && abs_y > abs_x) {
                current_scroll_direction = 1; // Vertical
            } else if (abs_x > DRAGSCROLL_BUFFER_SIZE && abs_x > abs_y) {
                current_scroll_direction = -1; // Horizontal
            }
        }

        // Update vertical scrolling if the current direction is vertical
        if (current_scroll_direction == 1) {
            if (abs_y > DRAGSCROLL_BUFFER_SIZE) {
                mouse_report->v = g_dragscroll_state.scroll_buffer_y > 0 ? 1 : -1;
                g_dragscroll_state.scroll_buffer_y = 0; // Reset vertical buffer
            }
        }

        // Update horizontal scrolling if the current direction is horizontal
        if (current_scroll_direction == -1) {
            if (abs_x > DRAGSCROLL_BUFFER_SIZE) {
                mouse_report->h = g_dragscroll_state.scroll_buffer_x > 0 ? 1 : -1;
                g_dragscroll_state.scroll_buffer_x = 0; // Reset horizontal buffer
            }
        }

        // Determine if we need to switch directions
        if (abs_y > abs_x + DRAGSCROLL_BUFFER_SIZE) {
            current_scroll_direction = 1; // Switch to vertical
        } else if (abs_x > abs_y + DRAGSCROLL_BUFFER_SIZE) {
            current_scroll_direction = -1; // Switch to horizontal
        }

        // Reset current scroll direction if movement is minor
        if (abs_x < DRAGSCROLL_BUFFER_SIZE && abs_y < DRAGSCROLL_BUFFER_SIZE) {
            current_scroll_direction = 0; // Reset if no significant movement
        }
    }
}




void keyboard_post_init_user(void) {
    uint32_t eeprom_data = eeconfig_read_user();
    cursor_speed = eeprom_data & 0xFFFF;
    scroll_dir_v = (eeprom_data >> 16) & 1;
    scroll_dir_h = (eeprom_data >> 17) & 1;
}

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    static int16_t h = 0;
    static uint8_t h_count = 0;
    static int16_t v = 0;
    static uint8_t v_count = 0;

    // Update drag scroll state
    sofleplus_update_dragscroll_state(&mouse_report);
	
    // Horizontal scrolling
    if (mouse_report.h != 0) {
        h_count++;
        h += scroll_dir_h ? -mouse_report.h : mouse_report.h;
        mouse_report.h = 0;
        if (h_count == SCROLL_AVERAGE) {
            mouse_report.h = (h < 0) ? -1 : 1;
            h = 0;
            h_count = 0;
        }
    } else {
        if (h_count) {
            mouse_report.h = (h < 0) ? -1 : 1;
        }
        h_count = 0;
        h = 0;
    }

    // Vertical scrolling
    if (mouse_report.v != 0) {
        v_count++;
        v += scroll_dir_v ? -mouse_report.v : mouse_report.v;
        mouse_report.v = 0;
        if (v_count == SCROLL_AVERAGE) {
            mouse_report.v = (v < 0) ? -1 : 1;
            v = 0;
            v_count = 0;
        }
    } else {
        if (v_count) {
            mouse_report.v = (v < 0) ? -1 : 1;
        }
        v_count = 0;
        v = 0;
    }

    // Modify mouse_report.x and mouse_report.y based on cursor_speed
    mouse_report.x *= cursor_speed;
    mouse_report.y *= cursor_speed;

    return mouse_report;
}



bool process_record_user(uint16_t keycode, keyrecord_t *record) {


	switch (keycode) {
		#ifdef SUPER_ALT_TAB_ENABLE
		    case CK_ATABF:
		    	if (record->event.pressed) {
		    		if (!is_alt_tab_active) {
		    			is_alt_tab_active = true;
		    			register_code(KC_LALT);
		    		}
		    			alt_tab_timer = timer_read();
		    			register_code(KC_TAB);
		    		} else {
		    			unregister_code(KC_TAB);
		    		}
                break;

		    case CK_ATABR:
		    	if (record->event.pressed) {
		    		if (!is_alt_tab_active) {
		    			is_alt_tab_active = true;
		    			register_code(KC_LALT);
		    		}
		    			alt_tab_timer = timer_read();
		    			register_code(KC_LSFT);
		    			register_code(KC_TAB);
		    		} else {
		    			unregister_code(KC_LSFT);
		    			unregister_code(KC_TAB);
		    		}
                break;
				
		case CK_ATMWU:	//Alt mouse wheel up 
			if (record->event.pressed) {
				if (!is_alt_tab_active) {
					is_alt_tab_active = true;
					register_code(KC_LALT);
				}
					alt_tab_timer = timer_read();
					register_code(KC_MS_WH_UP);
				} else {
					unregister_code(KC_MS_WH_UP);
				}
			break;
			
		case CK_ATMWD:	//Alt mouse wheel down 
			if (record->event.pressed) {
				if (!is_alt_tab_active) {
					is_alt_tab_active = true;
					register_code(KC_LALT);
				}
					alt_tab_timer = timer_read();
					register_code(KC_MS_WH_DOWN);
				} else {
					unregister_code(KC_MS_WH_DOWN);
				}
			break;
		#endif

        case CK_PO:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_D);
                unregister_code(KC_D);
                unregister_code(KC_LGUI);
                
                wait_ms(500);

                register_code(KC_LALT);
                register_code(KC_F4);
                unregister_code(KC_F4);
                unregister_code(KC_LALT);
            }
            break;

		
        case SCROLL_DIR_V:
            if (record->event.pressed) {
                scroll_dir_v = !scroll_dir_v;
                eeconfig_update_user((cursor_speed & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17));
            }
            break;
		
        case SCROLL_DIR_H:
            if (record->event.pressed) {
                scroll_dir_h = !scroll_dir_h;
                eeconfig_update_user((cursor_speed & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17));
            }
            return false;		
		
        case CURSOR_SPEED_UP:
            if (record->event.pressed && cursor_speed < 9) {
                cursor_speed++;
                eeconfig_update_user((cursor_speed & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17));
            }
            break;

        case CURSOR_SPEED_DN:
            if (record->event.pressed && cursor_speed > 1) {
                cursor_speed--;
                eeconfig_update_user((cursor_speed & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17));
            }
            break;

        case CURSOR_SPEED_RESET:
            if (record->event.pressed) {
                cursor_speed = DEFAULT_CURSOR_SPEED;
                eeconfig_update_user((cursor_speed & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17));
            }
            break;

        case DRAGSCROLL_MODE:
            sofleplus_set_pointer_dragscroll_enabled(record->event.pressed);
            break;

        case DRAGSCROLL_MODE_TOGGLE:
            if (record->event.pressed) {
                sofleplus_set_pointer_dragscroll_enabled(!sofleplus_get_pointer_dragscroll_enabled());
            }
            break;
    }
	
	    return true;  
}

/* OLED customisation */
#ifdef OLED_ENABLE
	#include "oled.c" //Stock OLED code

    void suspend_power_down_user(void) {
        oled_off();
		
    }

#endif


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

[0] = LAYOUT(
  CURSOR_SPEED_RESET,   KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    KC_0,  KC_GRV,
  KC_ESC,   KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,          KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,  KC_BSPC,
  KC_TAB,   KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                     KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN,  KC_QUOT,
  KC_LSFT,  KC_Z,   KC_X,    KC_C,    KC_V,    KC_B, KC_MUTE,    RGB_TOG,KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,  KC_RSFT,
                 KC_LGUI,KC_LALT,KC_LCTL, MO(2), KC_ENT,      KC_SPC,  MO(3), KC_RCTL, KC_RALT, KC_RGUI,
				 KC_LEFT, KC_UP, KC_RIGHT, KC_DOWN, KC_LGUI
),

[1] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______

),
[2] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[3] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[4] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[5] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[6] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[7] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[8] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
),
[9] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______, _______,
  _______, _______, _______, _______, _______, _______, _______,  _______, _______, _______, _______, _______, _______, _______,
           _______, _______, _______, _______, _______,                    _______, _______, _______, _______, _______,
		   _______, _______, _______, _______, _______
)
};

#if defined(ENCODER_MAP_ENABLE)
    const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
        [0] = { ENCODER_CCW_CW(CK_ATABF, CK_ATABR), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
        [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(C(KC_MINS), C(KC_EQL)) },
        [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_F3, C(KC_F3)) },
        [3] = { ENCODER_CCW_CW(G(KC_LEFT), G(KC_RGHT)), ENCODER_CCW_CW(A(KC_RGHT), A(KC_LEFT)) },
        [4] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [5] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [6] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [7] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [8] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [9] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) }
    };
#endif

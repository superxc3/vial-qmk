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

#include QMK_KEYBOARD_H


/* OLED customisation */
#ifdef OLED_ENABLE
	#include "oled.c" //xcmkb sofle
	#include <stdio.h>

    void suspend_power_down_user(void) {
        oled_off();
		
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
    CK_DL,
    CK_TBSRL,	//Hold For Trackball Scroll
	CK_TBSRLT,	//Enable Trackball Scroll
	CK_TBPOIT,	//Enable Trackball Precision
    CK_TBINS,
    CK_TBDES,
    CK_TBRST,
	CK_SCASE,
	//CK_SCTG,	//Sentence case toggle
	};
#else
	enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
	CK_ATABF = SAFE_RANGE,
	CK_ATABR,
	CK_ATMWU,	//Alt mouse wheel up 
	CK_ATMWD,	//Alt mouse wheel down
    CK_PO,
    CK_DL,
    CK_TBSRL,
	CK_TBSRLT,
	CK_TBPOIT,
    CK_TBINS,
    CK_TBDES,
    CK_TBRST,
	CK_SCASE
	//CK_SCTG,	//Sentence case toggle
	};
#endif



/* trackball 1 begins */
typedef union {
  uint32_t raw;
  struct {
    int8_t trackball_movement_ratio;
    int8_t mode;
  };
} user_config_t;

user_config_t user_config;
/* trackball 1 ends */

/* trackball 3 begins */
enum click_state {
    NONE = 0,
    WAITING,    // マウスレイヤーが有効になるのを待つ。 Wait for mouse layer to activate.
    CLICKABLE,  // マウスレイヤー有効になりクリック入力が取れる。 Mouse layer is enabled to take click input.
    CLICKING,   // クリック中。 Clicking.
    SCROLLING   // スクロール中。 Scrolling.
};

enum click_state state;     // 現在のクリック入力受付の状態 Current click input reception status
uint16_t click_timer;       // タイマー。状態に応じて時間で判定する。 Timer. Time to determine the state of the system.

uint16_t to_clickable_time = 10;   // この秒数(千分の一秒)、WAITING状態ならクリックレイヤーが有効になる。  For this number of seconds (milliseconds), if in WAITING state, the click layer is activated.
uint16_t to_reset_time = 1000; // この秒数(千分の一秒)、CLICKABLE状態ならクリックレイヤーが無効になる。 For this number of seconds (milliseconds), the click layer is disabled if in CLICKABLE state.

uint16_t click_layer = 9;   // マウス入力が可能になった際に有効になるレイヤー。Layers enabled when mouse input is enabled

int16_t scroll_v_mouse_interval_counter;   // 垂直スクロールの入力をカウントする。　Counting Vertical Scroll Inputs
int16_t scroll_h_mouse_interval_counter;   // 水平スクロールの入力をカウントする。  Counts horizontal scrolling inputs.

int16_t scroll_v_threshold = 30;    // この閾値を超える度に垂直スクロールが実行される。 Vertical scrolling is performed each time this threshold is exceeded.
int16_t scroll_h_threshold = 30;    // この閾値を超える度に水平スクロールが実行される。 Each time this threshold is exceeded, horizontal scrolling is performed.

int16_t after_click_lock_movement = 0;      // クリック入力後の移動量を測定する変数。 Variable that measures the amount of movement after a click input.

int16_t mouse_record_threshold = 30;    // ポインターの動きを一時的に記録するフレーム数。 Number of frames in which the pointer movement is temporarily recorded.

int16_t mouse_record_x;
int16_t mouse_record_y;
int16_t mouse_record_count;

int16_t mouse_move_remain_count;

bool is_record_mouse;

bool is_mouse_move_x_min;
int16_t mouse_move_x_sign;
int16_t mouse_move_y_sign;

double mouse_interval_delta;
double mouse_interval_counter;

///

void eeconfig_init_user(void) {
    user_config.raw = 0;
    user_config.trackball_movement_ratio = 10;
    user_config.mode = 0;
    eeconfig_update_user(user_config.raw);
}

void keyboard_post_init_user(void) {
    user_config.raw = eeconfig_read_user();
}

// クリック用のレイヤーを有効にする。　Enable layers for clicks
void enable_click_layer(void) {
    layer_on(click_layer);
    click_timer = timer_read();
	state = CLICKABLE; 
}

// クリック用のレイヤーを無効にする。 Disable layers for clicks.
void disable_click_layer(void) {
    state = NONE;
    layer_off(click_layer);
    scroll_v_mouse_interval_counter = 0;
    scroll_h_mouse_interval_counter = 0;
}

// 自前の絶対数を返す関数。 Functions that return absolute numbers.
int16_t my_abs(int16_t num) {
    if (num < 0) {
        num = -num;
    }

    return num;
}

// 自前の符号を返す関数。 Function to return the sign.
int16_t my_sign(int16_t num) {
    if (num < 0) {
        return -1;
    }

    return 1;
}

// 現在クリックが可能な状態か。 Is it currently clickable?
bool is_clickable_mode(void) {
    return state == CLICKABLE || state == CLICKING || state == SCROLLING;
	//return state == CLICKABLE || state == CLICKING; //滾輪時候不要自動跳到鼠標層。
}

/* trackball 3 ends */
/* trackball 4 begins*/

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {

    if (user_config.mode == 0)
    {
        if (!is_record_mouse) {
            if (mouse_report.x != 0 || mouse_report.y != 0) {
                is_record_mouse = true;
                mouse_record_x = 0;
                mouse_record_y = 0;
                mouse_record_count = 0;
            }
        }

        if (is_record_mouse) {
            mouse_record_x += mouse_report.x; // * user_config.trackball_movement_ratio;
            mouse_record_y += mouse_report.y; // * user_config.trackball_movement_ratio;
            mouse_record_count++;

            if (mouse_record_count >= mouse_record_threshold) {
                mouse_interval_counter = 0;
                int16_t absX = my_abs(mouse_record_x);
                int16_t absY = my_abs(mouse_record_y);
                is_mouse_move_x_min = absX < absY;

                mouse_move_remain_count = is_mouse_move_x_min ? absY : absX;
                mouse_move_remain_count *= user_config.trackball_movement_ratio;

                mouse_move_x_sign = my_sign(mouse_record_x);
                mouse_move_y_sign = my_sign(mouse_record_y);

                if (is_mouse_move_x_min) {
                    if (mouse_record_x == 0) {
                        mouse_interval_delta = 0;
                    } else {
                        mouse_interval_delta = (double)absX / (double)absY;
                    }
                } else {
                    if (mouse_record_y == 0) {
                        mouse_interval_delta = 0;
                    } else {
                        mouse_interval_delta = (double)absY / (double)absX;
                    }
                }

                is_record_mouse = false;
                mouse_record_count = 0;
            }
        }

        if (mouse_move_remain_count > 0) {
            mouse_interval_counter += mouse_interval_delta;

            bool can_move_min = mouse_interval_counter >= 0.99;

            if (can_move_min) {
                mouse_interval_counter -= 0.99;
            }

            if (is_mouse_move_x_min) {
                
                mouse_report.y = mouse_move_y_sign;

                if (can_move_min) {
                    mouse_report.x = mouse_move_x_sign;
                }
            } else {
                
                mouse_report.x = mouse_move_x_sign;

                if (can_move_min) {
                    mouse_report.y = mouse_move_y_sign;
                } 
            }

            mouse_report.x *= 1 + mouse_move_remain_count / 10;
            mouse_report.y *= 1 + mouse_move_remain_count / 10;

            mouse_move_remain_count--;
        } else {
            mouse_report.x = 0;
            mouse_report.y = 0;
        }
    }
    else
    {
        mouse_report.x *= user_config.trackball_movement_ratio;
        mouse_report.y *= user_config.trackball_movement_ratio;
    }
    
    int16_t current_x = mouse_report.x;
    int16_t current_y = mouse_report.y;
    int16_t current_h = 0;
    int16_t current_v = 0;

    if (current_x != 0 || current_y != 0) {
        
        switch (state) {
            case CLICKABLE:
                click_timer = timer_read();
                break;

            case CLICKING:
                after_click_lock_movement -= my_abs(current_x) + my_abs(current_y);

                if (after_click_lock_movement > 0) {
                    current_x = 0;
                    current_y = 0;
                }

                break;

            case SCROLLING:
            {
				
                int8_t rep_v = 0;
                int8_t rep_h = 0;
				//pimoroni_trackball_set_rgbw(100,149,237,0); //corn flower blue
				
                // 垂直スクロールの方の感度を高める。 Increase sensitivity toward vertical scrolling.
                if (my_abs(current_y) * 2 > my_abs(current_x)) {

                    scroll_v_mouse_interval_counter += current_y;
                    while (my_abs(scroll_v_mouse_interval_counter) > scroll_v_threshold) {
                        if (scroll_v_mouse_interval_counter < 0) {
                            scroll_v_mouse_interval_counter += scroll_v_threshold;
                            rep_v += scroll_v_threshold;
                        } else {
                            scroll_v_mouse_interval_counter -= scroll_v_threshold;
                            rep_v -= scroll_v_threshold;
                        }
                        
                    }
                } else {
                    scroll_h_mouse_interval_counter += current_x;

                    while (my_abs(scroll_h_mouse_interval_counter) > scroll_h_threshold) {
                        if (scroll_h_mouse_interval_counter < 0) {
                            scroll_h_mouse_interval_counter += scroll_h_threshold;
                            rep_h += scroll_h_threshold;
                        } else {
                            scroll_h_mouse_interval_counter -= scroll_h_threshold;
                            rep_h -= scroll_h_threshold;
                        }
                    }
                }

                current_h = rep_h / scroll_h_threshold;
                current_v = -rep_v / scroll_v_threshold;
                current_x = 0;
                current_y = 0;
            }
                break;

            case WAITING:
                if (timer_elapsed(click_timer) > to_clickable_time) {
                    enable_click_layer();
                }
                break;

            default:
                click_timer = timer_read();
                state = WAITING;
        }
    }
    else
    {
        switch (state) {
            case CLICKING:
            case SCROLLING:

                break;

            case CLICKABLE:
                if (timer_elapsed(click_timer) > to_reset_time) {
                    disable_click_layer();
                }
                break;

             case WAITING:
                if (timer_elapsed(click_timer) > 50) {
                    state = NONE;
                }
                break;

            default:
                state = NONE;

        }
    }

    mouse_report.x = current_x;
    mouse_report.y = current_y;
    mouse_report.h = current_h;
    mouse_report.v = current_v;

    return mouse_report;
}

//trackball led and haptic not working for layer indicator, thanks Drashna for pointing out
void housekeeping_task_user(void) {
    static layer_state_t state = 0;
    if (layer_state != state) {
        state = layer_state_set_user(layer_state);
    }
}

 //trackball as layer indicator
 layer_state_t layer_state_set_user(layer_state_t state) {

     switch (get_highest_layer(state)) {
		case 1:
			//DRV_pulse(13);		//soft_fuzz
			break;
		case 2:
			//DRV_pulse(10);		//dbl_click
			break;
		case 3:
			//DRV_pulse(24);		//sharp_tick1
			break;
		case 4:
			//DRV_pulse(69);		//transition_hum_10
			break;		
		case 5:
			//DRV_pulse(69);		//transition_hum_10
			break;
		case 6:
			//DRV_pulse(69);		//transition_hum_10
			break;
		case 7:
			//DRV_pulse(69);		//transition_hum_10
			break;
		case 8:
			//DRV_pulse(69);		//transition_hum_10
			break;		
        case 9:
             pimoroni_trackball_set_rgbw(0, 0, 0, 255);
			 //DRV_pulse(69);		//transition_hum_10
             break;
        default:
			pimoroni_trackball_set_rgbw(0,0,0, 0x00);
             break;

     }

   return state;
 }
  /* trackball 4 ends */



//trackball as state indicator
 bool led_update_user(led_t led_state)	//Lock key status indicators
	{
		if(led_state.caps_lock){
			pimoroni_trackball_set_rgbw(255, 0, 0, 0); //red
			//DRV_pulse(7);		//soft_bump	
		} else if(led_state.num_lock) {
			pimoroni_trackball_set_rgbw(0, 247, 255,0); //blue
		} else {
			pimoroni_trackball_set_rgbw(0,0,0, 0x00); //turn off
			}
		return true;
	}	
	
//trackball as capsword indicator
#ifdef CAPS_WORD_ENABLE
void caps_word_set_user(bool active) {
    if (active) {
        // Do something when Caps Word activates.
		pimoroni_trackball_set_rgbw(0, 247, 255,0); //blue
		//DRV_pulse(7);		//soft_bump	
    } else {
        // Do something when Caps Word deactivates.
		pimoroni_trackball_set_rgbw(0,0,0, 0x00); //turn off
    }
}
#endif

//haptic https://github.com/qmk/qmk_firmware/blob/master/docs/feature_haptic_feedback.md

//https://getreuer.info/posts/keyboards/sentence-case/

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
			return true;
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
			return true;
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

// For snipping - unable to return to user's desired dpi, hence default 10.
        case CK_DL:
			
			if (record->event.pressed) {
				user_config.trackball_movement_ratio = 2; //RST=10, SNIP=5/2?
		    } else {
				user_config.trackball_movement_ratio = 10; //default dpi, unable to return to user's set dpi
				//eeconfig_read_user(); //only read, but does not return
            }
            break;
			
        case CK_TBSRL:
            if (record->event.pressed) {
                state = SCROLLING;
            } else {
                enable_click_layer();
            }
            break;
		
        case CK_TBSRLT:
            if (record->event.pressed) {
                state = SCROLLING;
            }
            break;
		
        case CK_TBPOIT:
            if (record->event.pressed) {
                state = CLICKING;
            } else {
                disable_click_layer();
            }
            break;

        case CK_TBINS:
            if (record->event.pressed) {
                user_config.trackball_movement_ratio += 1;
                eeconfig_update_user(user_config.raw);
            }
            break;

         case CK_TBDES:
            if (record->event.pressed) {
                if (user_config.trackball_movement_ratio > 1) user_config.trackball_movement_ratio -= 1;
                eeconfig_update_user(user_config.raw);
            }
            break;

         case CK_TBRST:
            if (record->event.pressed) {
                user_config.trackball_movement_ratio = 10;
                eeconfig_update_user(user_config.raw);
            }
            break;


    }

	return true;

}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB,    KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,                         KC_Y,    KC_U,    KC_I,    KC_O,   KC_P,  KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL,    KC_A,    KC_S,    KC_D,    KC_F,    KC_G,                         KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN, KC_QUOT,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LSFT,    KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,                         KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,  KC_ESC,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, 	 MO(2),  KC_SPC,     KC_ENT, MO(1), KC_RALT
                                      //`--------------------------'  `--------------------------'

  ),

  [1] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB,    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                         KC_6,    KC_7,    KC_8,    KC_9,    KC_0, KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_LEFT, KC_DOWN,   KC_UP,KC_RIGHT, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LSFT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_RALT
                                      //`--------------------------'  `--------------------------'
  ),

  [2] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
       KC_TAB, KC_EXLM,   KC_AT, KC_HASH,  KC_DLR, KC_PERC,                      KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_BSPC,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LCTL, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_MINS,  KC_EQL, KC_LBRC, KC_RBRC, KC_BSLS,  KC_GRV,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      KC_LSFT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      KC_UNDS, KC_PLUS, KC_LCBR, KC_RCBR, KC_PIPE, KC_TILD,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_RALT
                                      //`--------------------------'  `--------------------------'
  ),

  [3] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
    QK_REBOOT, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      RGB_TOG, RGB_HUI, RGB_SAI, RGB_VAI, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      RGB_MOD, RGB_HUD, RGB_SAD, RGB_VAD, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_LGUI, _______,  KC_SPC,     KC_ENT, _______, KC_RALT
                                      //`--------------------------'  `--------------------------'
  ),

  [4] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX
                                      //`--------------------------'  `--------------------------'
  ),

  [5] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX
                                      //`--------------------------'  `--------------------------'
  ),

  [6] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX
                                      //`--------------------------'  `--------------------------'
  ),

  [7] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX
                                      //`--------------------------'  `--------------------------'
  ),

  [8] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX
                                      //`--------------------------'  `--------------------------'
  ),

  [9] = LAYOUT_split_3x6_3(
  //,-----------------------------------------------------.                    ,-----------------------------------------------------.
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,                      XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          XXXXXXX, XXXXXXX, XXXXXXX,     XXXXXXX, XXXXXXX, XXXXXXX
                                      //`--------------------------'  `--------------------------'
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
/* Copyright 2020 Josef Adamcik
  * Modification for VIA support and RGB underglow by Jens Bonk-Wiltfang
  * TPS65 Optimization by xcmkb
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
  #include <math.h>
  #include <string.h>
  #include "via.h"
  //#include "vial.h"
  #include "timer.h"
  #include "rgb_matrix.h"
  #include "eeconfig.h"
  #include "os_detection.h"
  #include "platforms/eeprom.h"

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
  #include "transactions.h"

  /* Number of LEDs on the LEFT half (keyboard.json split_count[0]).
   * LED indices 0..(SPLIT_LEFT-1) are left; SPLIT_LEFT..(total-1) are right.
   * Independent of which physical side is currently master. */
  #define VIALRGB_SPLIT_LEFT 29u

  extern HSV g_direct_mode_colors[RGB_MATRIX_LED_COUNT];

  static void vialrgb_direct_sync_handler(uint8_t in_buflen, const void *in_data,
                                           uint8_t out_buflen, void *out_data) {
      /* Packet layout: [1 byte start_idx] [VIALRGB_SPLIT_LEFT * sizeof(HSV) bytes of HSV data]
       * Master embeds the slave's start index so the slave doesn't need is_keyboard_left(). */
      if (in_buflen < 1 + VIALRGB_SPLIT_LEFT * sizeof(HSV)) return;
      const uint8_t start = ((const uint8_t *)in_data)[0];
      if ((uint16_t)(start + VIALRGB_SPLIT_LEFT) > RGB_MATRIX_LED_COUNT) return;
      memcpy(&g_direct_mode_colors[start],
             (const uint8_t *)in_data + 1,
             VIALRGB_SPLIT_LEFT * sizeof(HSV));
  }
#endif
  
  #ifndef setPinInputPullup
  #  define setPinInputPullup(pin) gpio_set_pin_input_high(pin)
  #endif
  
  // Helper to tap any action from the keymap at runtime (avoids complex initializers)
  __attribute__((unused)) static void tap_via_key(keypos_t key) {
  
  }
  
  
  
  
  
  #ifdef SUPER_ALT_TAB_ENABLE
      bool is_alt_tab_active = false; // Super Alt Tab Code
      uint16_t alt_tab_timer = 0;
  #endif
  
  
  
  
  
  // Variables for custom keycodes
  
  #ifdef VIA_ENABLE
      enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
      CK_ATABF = QK_KB_0,
      CK_ATABR,
      CK_ATMWU,	//Alt mouse wheel up 
      CK_ATMWD,	//Alt mouse wheel down
      CK_PO,		//Power options
      };
  #else
      enum custom_keycodes { // Use USER 00 instead of SAFE_RANGE for Via. VIA json must include the custom keycode.
      CK_ATABF = SAFE_RANGE,
      CK_ATABR,
      CK_ATMWU,	//Alt mouse wheel up 
      CK_ATMWD,	//Alt mouse wheel down
	  CK_PO,
      };
  #endif
  

 
  
  //trackball led and haptic not working for layer indicator, thanks Drashna for pointing out
  void housekeeping_task_user(void) {
      static layer_state_t state = 0;
      if (layer_state != state) {
          state = layer_state_set_user(layer_state);
      }

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
      if (is_keyboard_master()) {
          static uint32_t last_vialrgb_sync = 0;
          if (timer_elapsed32(last_vialrgb_sync) >= 16) {
              last_vialrgb_sync = timer_read32();
              /* Send slave's half: if master=LEFT send right half, if master=RIGHT send left half.
               * Embed start_idx as first byte so slave doesn't need is_keyboard_left(). */
              uint8_t slave_start = is_keyboard_left() ? VIALRGB_SPLIT_LEFT : 0;
              static uint8_t sync_buf[1 + VIALRGB_SPLIT_LEFT * sizeof(HSV)];
              sync_buf[0] = slave_start;
              memcpy(&sync_buf[1], &g_direct_mode_colors[slave_start], VIALRGB_SPLIT_LEFT * sizeof(HSV));
              transport_execute_transaction(VIALRGB_DIRECT_SYNC,
                                            sync_buf, sizeof(sync_buf),
                                            NULL, 0);
          }
      }
#endif
  }

  void keyboard_post_init_user(void) {
#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT)
      transaction_register_rpc(VIALRGB_DIRECT_SYNC, vialrgb_direct_sync_handler);
      /* Set the buffer size so transport_execute_transaction knows how many bytes
       * to send (master) and receive (slave) per sync. transaction_register_rpc
       * sets the offset and callback but leaves buffer_size at 0. */
      split_transaction_table[VIALRGB_DIRECT_SYNC].initiator2target_buffer_size = 1 + VIALRGB_SPLIT_LEFT * sizeof(HSV);
#endif
  }
  
  
  // RGB Layer indicator to meet vial brightness set by user
  
  bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
      uint8_t led_indices[] = {4, 5, 6, 15, 16, 33, 34, 35, 44, 45}; // Shared LED indices - updated on 11.27.2024
      
      // Get the current RGB brightness value (0-255)
      uint8_t brightness = rgb_matrix_get_val();
      
      // Ensure brightness is never 0 to avoid completely dark indicators
      if (brightness == 0) brightness = 1;
      
      // Helper function to scale color based on brightness
      // Added minimum value of 1 to ensure visibility at low brightness
      #define SCALE_BRIGHTNESS(color) (((color * brightness) / 255) ?: 1)
      
      bool indicator_set = false;
      
      // Check for Caps Lock
      if (host_keyboard_led_state().caps_lock) {
          for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
              if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                  rgb_matrix_set_color(led_indices[i], 
                      SCALE_BRIGHTNESS(128), 0, 0); // Red for Caps Lock
              }
          }
          indicator_set = true;
      }
      
      // Check layer state
      uint8_t current_layer = get_highest_layer(layer_state);
      if (current_layer > 0) {
          switch (current_layer) {
              case 1:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              SCALE_BRIGHTNESS(128), 0, SCALE_BRIGHTNESS(128)); // Purple for Layer 1
                      }
                  }
                  indicator_set = true;
                  break;
              case 2:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                            SCALE_BRIGHTNESS(255), SCALE_BRIGHTNESS(215), 0); // Gold for Layer 2
                              
                      }
                  }
                  indicator_set = true;
                  break;
              case 3:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              0, SCALE_BRIGHTNESS(128), SCALE_BRIGHTNESS(128)); // Cyan for Layer 3
                      }
                  }
                  indicator_set = true;
                  break;
              case 4:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              SCALE_BRIGHTNESS(255), SCALE_BRIGHTNESS(128), 0); // Orange for Layer 4
                      }
                  }
                  indicator_set = true;
                  break;
              case 5:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              0, 0, SCALE_BRIGHTNESS(128)); // Blue for Layer 5
                      }
                  }
                  indicator_set = true;
                  break;
              case 6:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              SCALE_BRIGHTNESS(128), 0, SCALE_BRIGHTNESS(128)); // Magenta for Layer 6
                      }
                  }
                  indicator_set = true;
                  break;
              case 7:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              SCALE_BRIGHTNESS(255), SCALE_BRIGHTNESS(192), SCALE_BRIGHTNESS(203)); // Pink for Layer 7
                      }
                  }
                  indicator_set = true;
                  break;
              case 8:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                            0, SCALE_BRIGHTNESS(255), SCALE_BRIGHTNESS(127)); // Spring green for Layer 8
                      }
                  }
                  indicator_set = true;
                  break;
              case 9:
                  for (uint8_t i = 0; i < sizeof(led_indices) / sizeof(led_indices[0]); i++) {
                      if (led_indices[i] >= led_min && led_indices[i] <= led_max) {
                          rgb_matrix_set_color(led_indices[i], 
                              SCALE_BRIGHTNESS(255), SCALE_BRIGHTNESS(255), SCALE_BRIGHTNESS(255)); // White for Layer 9
                      }
                  }
                  indicator_set = true;
                  break;
              default:
                  break;
          }
      }
      
      #undef SCALE_BRIGHTNESS
      
      // Return true if we set any indicators, false to allow normal RGB processing
      return indicator_set;
  }
  
 

// Function Prototypes
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report);
bool process_record_user(uint16_t keycode, keyrecord_t *record);

#if defined(DIP_SWITCH_ENABLE)
static uint16_t dip_switch_keycode = KC_NO;  // Store the pressed keycode

bool dip_switch_update_user(uint8_t index, bool active) {
    if (index == 0) {
        keypos_t kp = { .row = 4, .col = 6 };
        
        if (active) {
            // Press event - get keycode and store it
            uint16_t kc = keymap_key_to_keycode(get_highest_layer(layer_state), kp);
            dip_switch_keycode = kc;
            
            // Create keyrecord for proper QMK processing
            keyrecord_t record = {
                .event = {
                    .key = kp,
                    .pressed = true,
                    .time = timer_read(),
                    .type = KEY_EVENT
                },
                .keycode = kc
            };
            
            // Handle different keycode types properly
            if (kc == RM_TOGG) {
                rgb_matrix_toggle();
            } else if (kc == RM_ON) {
                rgb_matrix_step();
            } else if (kc >= QK_KB_0) {
                // Custom keycodes - use process_record_user
                process_record_user(kc, &record);
            } else if (IS_QK_TOGGLE_LAYER(kc)) {
                // TG() functions - toggle layer state
                uint8_t layer = QK_TOGGLE_LAYER_GET_LAYER(kc);
                layer_invert(layer);
            } else if (IS_QK_TO(kc)) {
                // TO() functions - set default layer
                uint8_t layer = QK_TO_GET_LAYER(kc);
                layer_move(layer);
            } else if (IS_QK_MOMENTARY(kc)) {
                // MO() functions - momentary layer (hold behavior)
                uint8_t layer = QK_MOMENTARY_GET_LAYER(kc);
                layer_on(layer);
            } else if (IS_QK_LAYER_TAP(kc)) {
                // LT() functions - activate layer and register tap key
                uint8_t layer = QK_LAYER_TAP_GET_LAYER(kc);
                uint16_t tap_kc = QK_LAYER_TAP_GET_TAP_KEYCODE(kc);
                layer_on(layer);
                register_code16(tap_kc);
            } else if (kc >= MS_UP && kc <= MS_ACL2) {
                // Mouse keys - register for hold behavior
                register_code16(kc);
            } else {
                // Basic keycodes (a-z, numbers, etc.)
                register_code16(kc);
            }
        } else {
            // Release event - use stored keycode
            uint16_t kc = dip_switch_keycode;
            
            if (kc != KC_NO) {
                // Create keyrecord for proper QMK processing
                keyrecord_t record = {
                    .event = {
                        .key = kp,
                        .pressed = false,
                        .time = timer_read(),
                        .type = KEY_EVENT
                    },
                    .keycode = kc
                };
                
                // Handle release for keys that need it
                if (IS_QK_MOMENTARY(kc)) {
                    // MO() functions - turn off layer on release
                    uint8_t layer = QK_MOMENTARY_GET_LAYER(kc);
                    layer_off(layer);
                } else if (IS_QK_LAYER_TAP(kc)) {
                    // LT() functions - turn off layer and unregister tap key
                    uint8_t layer = QK_LAYER_TAP_GET_LAYER(kc);
                    uint16_t tap_kc = QK_LAYER_TAP_GET_TAP_KEYCODE(kc);
                    layer_off(layer);
                    unregister_code16(tap_kc);
                } else if (kc >= MS_UP && kc <= MS_ACL2) {
                    // Mouse keys - unregister on release
                    unregister_code16(kc);
                } else if (kc >= QK_KB_0) {
                    // Custom keycodes - use process_record_user
                    process_record_user(kc, &record);
                } else if (!(kc == RM_TOGG || kc == RM_ON || IS_QK_TOGGLE_LAYER(kc) || IS_QK_TO(kc))) {
                    // Basic keycodes - unregister on release
                    unregister_code16(kc);
                }
                
                dip_switch_keycode = KC_NO;  // Clear stored keycode
            }
        }
    }
    return true;
}
#endif

// Keymaps and encoder configuration (65 arguments for LAYOUT)
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        KC_GRAVE, KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    MS_UP, MS_BTN1,
        KC_ESC,   KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,                     KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,  KC_BSPC,
        KC_TAB,   KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                     KC_H,    KC_J,    KC_K,    KC_L, KC_SCLN,  KC_QUOT,
        KC_LSFT,  KC_Z,   KC_X,    KC_C,    KC_V,    KC_B, KC_MUTE,    CK_PO, KC_N,    KC_M, KC_COMM,  KC_DOT, KC_SLSH,  KC_RSFT,
                        KC_LGUI,KC_LALT,KC_LCTL, MO(1), KC_ENT,      KC_SPC,  MO(2), KC_RCTL, KC_RALT, KC_RGUI,
                        KC_LEFT, KC_UP, KC_RIGHT, KC_DOWN, MS_BTN1
    ),

    [1] = LAYOUT(
        KC_F12,         KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,                        KC_F6,        KC_F7,  KC_F8,  KC_F9,  KC_F10,         KC_F11,
        KC_GRAVE,       LSFT(KC_1), LSFT(KC_2), KC_LBRC,    KC_RBRC,    KC_SLASH,                     KC_MINUS,     KC_7,   KC_8,   KC_9,   KC_COMMA,       KC_BSPC,
        LSFT(KC_GRAVE), LSFT(KC_3), LSFT(KC_4), LSFT(KC_9), LSFT(KC_0), LSFT(KC_7),                   KC_EQUAL,     KC_4,   KC_5,   KC_6,   KC_KP_ASTERISK, KC_DELETE,
        KC_CAPS_LOCK,   LSFT(KC_5), LSFT(KC_6), KC_TRNS,    KC_TRNS,    LSFT(KC_8), KC_TRNS, KC_TRNS, KC_NUM_LOCK,  KC_1,   KC_2,   KC_3,   KC_KP_SLASH,    KC_KP_ENTER, 
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_KP_DOT, KC_0, KC_EQUAL,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, MS_BTN1
    ),

    [2] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_PGUP, KC_HOME, KC_UP, KC_END, KC_PSCR, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_PGDN, KC_LEFT, KC_DOWN, KC_RIGHT, KC_INSERT, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, RM_TOGG
    ),

    [3] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),

    [4] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),

    [5] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    )
};

#if defined(ENCODER_MAP_ENABLE)
    const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
        [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
        [1] = { ENCODER_CCW_CW(CK_ATABF, CK_ATABR), ENCODER_CCW_CW(MS_WHLD, MS_WHLU) },
        [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_F3, C(KC_F3)) },
        [3] = { ENCODER_CCW_CW(G(KC_LEFT), G(KC_RGHT)), ENCODER_CCW_CW(A(KC_RGHT), A(KC_LEFT)) },
        [4] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [5] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) }
    };
#endif

bool process_detected_host_os_user(os_variant_t detected_os) {
    switch (detected_os) {
        case OS_MACOS:
        case OS_IOS:
            keymap_config.swap_lctl_lgui = true;
            keymap_config.swap_rctl_rgui = true;
            break;
        case OS_WINDOWS:
        case OS_LINUX:
        default:
            keymap_config.swap_lctl_lgui = false;
            keymap_config.swap_rctl_rgui = false;
            break;
    }
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
#ifdef SUPER_ALT_TAB_ENABLE
        case CK_ATABF:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    register_code(KC_LALT); // Default to Alt
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
                    register_code(KC_LALT); // Default to Alt
                }
                alt_tab_timer = timer_read();
                register_code(KC_LSFT);
                register_code(KC_TAB);
            } else {
                unregister_code(KC_LSFT);
                unregister_code(KC_TAB);
            }
            break;
            
        case CK_ATMWU:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    register_code(KC_LALT); // Default to Alt
                }
                alt_tab_timer = timer_read();
                register_code(MS_WHLU);
            } else {
                unregister_code(MS_WHLU);
            }
            break;
            
        case CK_ATMWD:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    register_code(KC_LALT); // Default to Alt
                }
                alt_tab_timer = timer_read();
                register_code(MS_WHLD);
            } else {
                unregister_code(MS_WHLD);
            }
            break;
#endif

        case CK_PO:
            if (record->event.pressed) {
                // Defaulting to Alt+F4 (Windows Close App)
                register_code(KC_LALT);
                register_code(KC_F4);
                unregister_code(KC_F4);
                unregister_code(KC_LALT);
            }
            break;
    }
    
    return true;  
}
/* OLED customisation */
#ifdef OLED_ENABLE

#include "oled_data.h"

unsigned int animation_state = 0;

// Expose scroll_speed from keymap
extern int16_t scroll_speed;

static void render_space(void) {
    char wpm = get_current_wpm();
    uint8_t render_row[128];
    int i;
    
    oled_set_cursor(0,0);
    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_1+i+animation_state);
    }
    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_1+i+animation_state) & pgm_read_byte(mask_row_1+i-wpm/4)) | pgm_read_byte(ship_row_1+i-wpm/4);
    }
    // FIX: Cast to (const char*) to solve signedness warning
    oled_write_raw((const char*)render_row, 128);

    oled_set_cursor(0,1);
    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_2+i+animation_state);
    }
    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_2+i+animation_state) & pgm_read_byte(mask_row_2+i-wpm/4)) | pgm_read_byte(ship_row_2+i-wpm/4);
    }
    // FIX: Cast to (const char*)
    oled_write_raw((const char*)render_row, 128);

    oled_set_cursor(0,2);
    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_3+i+animation_state);
    }
    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_3+i+animation_state) & pgm_read_byte(mask_row_3+i-wpm/4)) | pgm_read_byte(ship_row_3+i-wpm/4);
    }
    // FIX: Cast to (const char*)
    oled_write_raw((const char*)render_row, 128);

    oled_set_cursor(0,3);
    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_4+i+animation_state);
    }
    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_4+i+animation_state) & pgm_read_byte(mask_row_4+i-wpm/4)) | pgm_read_byte(ship_row_4+i-wpm/4);
    }
    // FIX: Cast to (const char*)
    oled_write_raw((const char*)render_row, 128);

    animation_state = (animation_state + 1 + (wpm/15)) % (128*2);
}

/* timers */
//uint32_t anim_timer = 0;
uint32_t anim_sleep = 0;

// Large layer number display using image2cpp generated bitmaps
static void display_large_layer_number(uint8_t layer) {
    const uint8_t* bitmap = NULL;
    
    switch (layer) {
        case 0: bitmap = digit_0; break;
        case 1: bitmap = digit_1; break;  
        case 2: bitmap = digit_2; break;
        case 3: bitmap = digit_3; break;
        case 4: bitmap = digit_4; break;
        case 5: bitmap = digit_5; break;
        case 6: bitmap = digit_6; break;
        case 7: bitmap = digit_7; break;
        case 8: bitmap = digit_8; break;
        case 9: bitmap = digit_9; break;
        default: bitmap = digit_0; break; // Use 0 for any layer > 9
    }
    
    if (bitmap) {
        oled_set_cursor(0, 11);
        // FIX: Cast to (const char*)
        oled_write_raw_P((const char*)bitmap, 128); 
    }
}

static void print_status_narrow(void) {
    /* SOFLE header and OS detection status */
    oled_set_cursor(0,1);
    oled_write("SOFLE", false);
    oled_set_cursor(0,2);

    /* Numlock indicator */
    oled_set_cursor(0,3);
    led_t led_usb_state = host_keyboard_led_state();
    if (led_usb_state.num_lock) {
        oled_write_P(PSTR("NUMLK"), false);
    } else {
        oled_write_P(PSTR("     "), false); 
    }

    /* Always clear the text areas first to prevent artifacts */
    oled_set_cursor(0, 6);
    oled_write_P(PSTR("        "), false); 
    oled_set_cursor(0, 7);
    oled_write_P(PSTR("        "), false); 
    oled_set_cursor(0, 8);
    oled_write_P(PSTR("        "), false); 
    oled_set_cursor(0, 9);
    oled_write_P(PSTR("        "), false); 

    /* Large layer number bitmap display (32x32px at row 11-14) */
    display_large_layer_number(get_highest_layer(layer_state));
}

// ---------------------------------------------------------
// NOTE: I removed the intermediate #endif here so the block continues!
// ---------------------------------------------------------

/*oled setup for sofleplus*/
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    
    if (is_keyboard_master()) {
        return OLED_ROTATION_270;
    }
    return rotation;

}

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        // Master (left) OLED power management
        if (is_oled_on()) {
            // Ensure OLED_TIMEOUT is defined in config.h or use a number like 30000
            if (last_input_activity_elapsed() > OLED_TIMEOUT) {
                oled_off();
            } else {
                print_status_narrow();
            }
        }
        // Wake OLED on activity
        if (!is_oled_on() && last_input_activity_elapsed() < 1000) {
            oled_on();
        }
    } else {
        // Slave (right) OLED power management
        if (is_oled_on()) {
            if (last_input_activity_elapsed() > OLED_TIMEOUT) {
                oled_off();
            } else {
                render_space();
            }
        }
        // Wake OLED on activity
        if (!is_oled_on() && last_input_activity_elapsed() < 1000) {
            oled_on();
        }
    }
    return false;
}

#endif

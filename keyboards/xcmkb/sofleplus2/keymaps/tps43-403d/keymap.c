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
  #include "via.h"
  #include "vial.h"
  #include "timer.h"
  #include "rgb_matrix.h"
  #include "eeconfig.h"
  #include "os_detection.h"
  
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
      SCROLL_DIR_V, //滾輪上下
      SCROLL_DIR_H, //滾輪左右
      CURSOR_SPEED_UP, // Increase cursor speed
      CURSOR_SPEED_DN, // Increase cursor speed
      CURSOR_SPEED_RESET, // Reset cursor speed to default
      SCROLL_SPEED_UP,    // Increase scroll speed
      SCROLL_SPEED_DOWN,  // Decrease scroll speed
      SCROLL_SPEED_RESET, // Reset scroll speed to default
      TRACKPAD_LAYER_SCROLL_SET, // Set which layer uses scroll mode
      TRACKPAD_LAYER_SWIPE2_SET, // Set which layer uses 2-finger swipe mode
      TRACKPAD_LAYER_SWIPE3_SET, // Set which layer uses 3-finger swipe mode
      TRACKPAD_LAYER_RESET,      // Reset current layer to default cursor behavior
      TRACKPAD_TOGGLE,           // Toggle trackpad on/off
      SNIPER_MO,                 // Sniper momentary
      SNIPER_TOG,                // Sniper toggle
      SNIPER_SET_MODS,           // Learn sniper modifiers
      SNIPER_DPI_UP,             // Sniper DPI up
      SNIPER_DPI_DOWN,           // Sniper DPI down
      SNIPER_SHOW_MODS,          // Show sniper modifiers
      OS_DETECTION_TOGGLE,       // Toggle OS detection on/off
      ZMTOG,                     // Toggle zoom gestures on/off
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
      SCROLL_SPEED_UP,    // Increase scroll speed
      SCROLL_SPEED_DOWN,  // Decrease scroll speed
      SCROLL_SPEED_RESET, // Reset scroll speed to default
      TRACKPAD_LAYER_SCROLL_SET, // Set which layer uses scroll mode
      TRACKPAD_LAYER_SWIPE2_SET, // Set which layer uses 2-finger swipe mode
      TRACKPAD_LAYER_SWIPE3_SET, // Set which layer uses 3-finger swipe mode
      TRACKPAD_LAYER_RESET,      // Reset current layer to default cursor behavior
      TRACKPAD_TOGGLE,           // Toggle trackpad on/off
      SNIPER_MO,                 // Sniper momentary
      SNIPER_TOG,                // Sniper toggle
      SNIPER_SET_MODS,           // Learn sniper modifiers
      SNIPER_DPI_UP,             // Sniper DPI up
      SNIPER_DPI_DOWN,           // Sniper DPI down
      SNIPER_SHOW_MODS,          // Show sniper modifiers
      OS_DETECTION_TOGGLE,       // Toggle OS detection on/off
      ZMTOG,                     // Toggle zoom gestures on/off
      };
  #endif
  
  // OS detection toggle implementation
  #define EEPROM_OS_DETECTION_OFFSET 42  // Use a safe EEPROM offset for user data
  static bool os_detection_enabled = false; // Default OFF for Linux-Safe By Default
  
  // Function to get effective OS detection result
  os_variant_t get_effective_os_detection(void) {
      if (!os_detection_enabled) {
          return OS_UNSURE; // Return unsure when disabled
      }
      return detected_host_os();
  }
  
  // Initialize OS detection state from EEPROM
  void os_detection_init(void) {
      uint8_t stored_state = eeprom_read_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET);
      if (stored_state == 0xFF) {
          // First boot - write default state (disabled) to EEPROM
          os_detection_enabled = false;
          eeprom_write_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET, 0);
      } else {
          // Load saved state
          os_detection_enabled = (stored_state != 0);
      }
  }
  
  // Toggle OS detection and save to EEPROM
  void toggle_os_detection(void) {
      os_detection_enabled = !os_detection_enabled;
      eeprom_write_byte((uint8_t*)EEPROM_OS_DETECTION_OFFSET, os_detection_enabled ? 1 : 0);
  }
  
  
  //trackball led and haptic not working for layer indicator, thanks Drashna for pointing out
  void housekeeping_task_user(void) {
      static layer_state_t state = 0;
      if (layer_state != state) {
          state = layer_state_set_user(layer_state);
      }
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
  
  
  
  /* TPS65 Optimized Configuration */

/* AZOTEQ config - TPS65 trackpad optimizations */

// TPS65-optimized DPI/Cursor speed settings (larger trackpad = different scaling)
#define MIN_CURSOR_SPEED     1
#define MAX_CURSOR_SPEED     6
#define DEFAULT_CURSOR_SPEED 3  // Good default for TPS65
#define DEFAULT_SNIPER_SPEED 1  // Default sniper DPI for precision

// TPS65 acceleration curve settings (adjusted for 65mm trackpad)
#define ACCEL_THRESHOLD_LOW    8.0f   // Higher threshold for larger trackpad
#define ACCEL_THRESHOLD_MED    15.0f  // Adjusted for TPS65 sensitivity
#define ACCEL_THRESHOLD_HIGH   25.0f  // Higher for larger movement range
#define ACCEL_BASE_MULTIPLIER  0.15f  // More conservative for TPS65

// Conservative acceleration factors to prevent overflow
#define ACCEL_FACTOR_HIGH  1.5f  // Reduced to prevent bounce
#define ACCEL_FACTOR_MED   1.3f  // Conservative values  
#define ACCEL_FACTOR_LOW   1.1f  // Minimal acceleration

// TPS65-optimized base scaling factors (adjusted for larger trackpad)
static const float dpi_base_scale[] = {
    0.4f,  // Speed 1 - Precise (more precise for larger trackpad)
    0.6f,  // Speed 2 - Slow
    0.8f,  // Speed 3 - Normal (reduced from 1.0 to prevent overflow)
    1.0f,  // Speed 4 - Fast (was 1.3)
    1.2f,  // Speed 5 - Very fast (was 1.6)
    1.4f   // Speed 6 - Maximum (was 2.0, reduced to prevent overflow)
};

// TPS65-optimized scroll speed settings (faster to match hardware scroll)
#define MIN_SCROLL_SPEED     1
#define MAX_SCROLL_SPEED     8  
#define DEFAULT_SCROLL_SPEED 4  // Faster default to match hardware scroll
int16_t scroll_speed = DEFAULT_SCROLL_SPEED;
static bool trackpad_enabled = true; // Trackpad on/off state

// Forward declaration for helper function (defined after variables)
static int16_t get_current_cursor_speed(void);

// TPS65 scroll dividers (reduced to match hardware scroll speed)
#define SCROLL_DIVIDER_SLOWEST   16  // Level 1 - reduced from 32
#define SCROLL_DIVIDER_SLOWER    12  // Level 2 - reduced from 24
#define SCROLL_DIVIDER_SLOW      8   // Level 3 - reduced from 18
#define SCROLL_DIVIDER_MEDIUM    6   // Level 4 - reduced from 14
#define SCROLL_DIVIDER_FAST      4   // Level 5 - reduced from 10
#define SCROLL_DIVIDER_FASTER    3   // Level 6 - reduced from 7
#define SCROLL_DIVIDER_FASTEST   2   // Level 7 - reduced from 4
#define SCROLL_DIVIDER_TURBO     1   // Level 8 - reduced from 2

// Swipe gesture settings
#define SWIPE_THRESHOLD        8   // Higher threshold for larger trackpad
#define SWIPE_COOLDOWN_MS      300 // Milliseconds before next swipe can trigger

// Gesture timing variables
static bool two_finger_gesture_active = false;
static bool three_finger_gesture_active = false;
static uint32_t last_swipe_time = 0;

// EEPROM settings
// Legacy cursor_speed renamed for dual DPI system
int16_t cursor_speed_normal = DEFAULT_CURSOR_SPEED;  // Regular DPI for fast movement
int16_t cursor_speed_sniper = DEFAULT_SNIPER_SPEED; // Sniper DPI for precision work
int16_t scroll_dir_v = 0;
int16_t scroll_dir_h = 0;

// Sniper mode state variables
// Sniper mode timing
#define LEARNING_TIMEOUT 5000           // 5 seconds timeout
#define INFO_TIMEOUT 2000               // 2 seconds info display timeout

// EEPROM Address Map - FIXED: Use addresses within 4KB RP2040 EEPROM limit
// 0x0000-0x0EFF:               Vial dynamic keymaps and QMK data
// 0x0FB0-0x0FB1:               Sniper settings (2 bytes)
// 0x0FA0-0x0FA9:               Trackpad layer configuration (10 bytes)  
// 0x0FB2:                      Zoom toggle setting (1 byte)
// 0x0FC0+:                     Available for future features
#define EEPROM_SNIPER_SETTINGS_OFFSET 0x0FB0
#define EEPROM_ZOOM_TOGGLE_OFFSET 0x0FB2

// Sniper mode state variables
bool sniper_mode_active = false;        // Current mode state
uint8_t sniper_modifier_mask = 0;       // User-defined modifier combination  
bool sniper_learning_mode = false;      // Learning state
bool sniper_info_mode = false;          // Show modifier info mode
uint16_t learning_timer = 0;            // Auto-timeout for learning
uint16_t info_timer = 0;                // Auto-timeout for info display
#define INFO_DISPLAY_TIMEOUT 3000       // 3 seconds timeout

// Zoom gesture state variable
bool zoom_enabled = true;               // Zoom gestures enabled by default

// Function to save sniper settings to EEPROM
void save_sniper_settings(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET), sniper_modifier_mask);
    eeprom_write_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET + 1), cursor_speed_sniper);
    // Note: sniper_mode_active is intentionally NOT saved - it should always start as false
}

// Function to load sniper settings from EEPROM
void load_sniper_settings(void) {
    uint8_t stored_mask = eeprom_read_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET));
    uint8_t stored_speed = eeprom_read_byte((uint8_t*)(EEPROM_SNIPER_SETTINGS_OFFSET + 1));
    
    // Check if EEPROM is uninitialized (all 0xFF values)
    if (stored_mask == 0xFF && stored_speed == 0xFF) {
        // First boot - use defaults and save them
        sniper_modifier_mask = 0;  // No modifiers by default
        cursor_speed_sniper = DEFAULT_SNIPER_SPEED;
        save_sniper_settings();  // Save defaults to EEPROM
    } else {
        // Load and validate saved values
        sniper_modifier_mask = stored_mask;
        cursor_speed_sniper = (stored_speed >= MIN_CURSOR_SPEED && stored_speed <= MAX_CURSOR_SPEED) ? 
                             stored_speed : DEFAULT_SNIPER_SPEED;
        
        // Save corrected values if validation changed them
        if (stored_speed != cursor_speed_sniper) {
            save_sniper_settings();
        }
    }
    
    // Always start with sniper mode disabled, regardless of saved state
    sniper_mode_active = false;
}

// Function to save zoom toggle setting to EEPROM
void save_zoom_setting(void) {
    eeprom_write_byte((uint8_t*)(EEPROM_ZOOM_TOGGLE_OFFSET), zoom_enabled ? 1 : 0);
}

// Function to load zoom toggle setting from EEPROM
void load_zoom_setting(void) {
    uint8_t stored_zoom = eeprom_read_byte((uint8_t*)(EEPROM_ZOOM_TOGGLE_OFFSET));
    
    // Check if EEPROM is uninitialized (0xFF value)
    if (stored_zoom == 0xFF) {
        // First boot - use default (enabled) and save it
        zoom_enabled = true;
        save_zoom_setting();
    } else {
        // Load saved value
        zoom_enabled = (stored_zoom != 0);
    }
}

// Helper function implementation
static int16_t get_current_cursor_speed(void) {
    return sniper_mode_active ? cursor_speed_sniper : cursor_speed_normal;
}

// Use safe address within 4KB EEPROM range for trackpad config
#define EECONFIG_USER_TRACKPAD_OFFSET 0x0FA0

// Trackpad layer configuration magic number and structure
#define TRACKPAD_CONFIG_MAGIC 0x7401  // Magic number for validation

typedef struct {
    uint16_t scroll_layers;  // Bitmask for layers that enable scroll mode
    uint16_t swipe2_layers;  // Bitmask for layers that enable 2-finger swipe
    uint16_t swipe3_layers;  // Bitmask for layers that enable 3-finger swipe
} trackpad_layer_config_t;

// Default configuration - these layers enable specific trackpad modes
static trackpad_layer_config_t user_config = {
    .scroll_layers = (1 << 1),  // Layer 1 enabled for scroll
    .swipe2_layers = (1 << 2),  // Layer 2 enabled for 2-finger swipe
    .swipe3_layers = (1 << 3),  // Layer 3 enabled for 3-finger swipe
};

// EEPROM functions for trackpad layer configuration
void trackpad_config_save(void) {
    uint16_t magic = TRACKPAD_CONFIG_MAGIC;
    eeprom_write_word((uint16_t*)(EECONFIG_USER_TRACKPAD_OFFSET), magic);
    
    // Write bitmasks to EEPROM - offset by 2 since magic is now 16-bit
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 2), user_config.scroll_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 3), (user_config.scroll_layers >> 8) & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 4), user_config.swipe2_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 5), (user_config.swipe2_layers >> 8) & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 6), user_config.swipe3_layers & 0xFF);
    eeprom_write_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 7), (user_config.swipe3_layers >> 8) & 0xFF);
}

void trackpad_config_load(void) {
    uint16_t magic = eeprom_read_word((uint16_t*)(EECONFIG_USER_TRACKPAD_OFFSET));
    if (magic == TRACKPAD_CONFIG_MAGIC) {
        // Load bitmasks from EEPROM - offset by 2 since magic is now 16-bit
        uint8_t scroll_low = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 2));
        uint8_t scroll_high = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 3));
        uint8_t swipe2_low = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 4));
        uint8_t swipe2_high = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 5));
        uint8_t swipe3_low = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 6));
        uint8_t swipe3_high = eeprom_read_byte((uint8_t*)(EECONFIG_USER_TRACKPAD_OFFSET + 7));
        
        user_config.scroll_layers = scroll_low | (scroll_high << 8);
        user_config.swipe2_layers = swipe2_low | (swipe2_high << 8);
        user_config.swipe3_layers = swipe3_low | (swipe3_high << 8);
    } else {
        // First time setup - save defaults
        trackpad_config_save();
    }
}

void trackpad_config_reset(void) {
    user_config.scroll_layers = (1 << 1);  // Layer 1 enabled for scroll
    user_config.swipe2_layers = (1 << 2);  // Layer 2 enabled for 2-finger swipe
    user_config.swipe3_layers = (1 << 3);  // Layer 3 enabled for 3-finger swipe
    trackpad_config_save();
}

// Reset current layer to default cursor behavior (remove from all gesture types)
void trackpad_layer_reset(void) {
    uint8_t current_layer = get_highest_layer(layer_state);
    if (current_layer <= 15) {
        uint16_t layer_bit = (1 << current_layer);
        // Remove current layer from all gesture types
        user_config.scroll_layers &= ~layer_bit;
        user_config.swipe2_layers &= ~layer_bit;
        user_config.swipe3_layers &= ~layer_bit;
        trackpad_config_save();
    }
}

// Function Prototypes
void keyboard_post_init_user(void);
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
            if (kc == RGB_TOG) {
                rgb_matrix_toggle();
            } else if (kc == RGB_MOD) {
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
            } else if (kc >= KC_MS_UP && kc <= KC_MS_ACCEL2) {
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
                } else if (kc >= KC_MS_UP && kc <= KC_MS_ACCEL2) {
                    // Mouse keys - unregister on release
                    unregister_code16(kc);
                } else if (kc >= QK_KB_0) {
                    // Custom keycodes - use process_record_user
                    process_record_user(kc, &record);
                } else if (!(kc == RGB_TOG || kc == RGB_MOD || IS_QK_TOGGLE_LAYER(kc) || IS_QK_TO(kc))) {
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
        KC_GRAVE, KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    KC_0,  KC_MINUS,
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
                SCROLL_SPEED_DOWN, CURSOR_SPEED_DN, SCROLL_SPEED_UP, CURSOR_SPEED_UP, MS_BTN1
    ),

    [2] = LAYOUT(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_PGUP, KC_HOME, KC_UP, KC_END, KC_PSCR, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_PGDN, KC_LEFT, KC_DOWN, KC_RIGHT, KC_INSERT, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, RGB_TOG
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
        [1] = { ENCODER_CCW_CW(CK_ATABF, CK_ATABR), ENCODER_CCW_CW(KC_MS_WH_DOWN, KC_MS_WH_UP) },
        [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU), ENCODER_CCW_CW(KC_F3, C(KC_F3)) },
        [3] = { ENCODER_CCW_CW(G(KC_LEFT), G(KC_RGHT)), ENCODER_CCW_CW(A(KC_RGHT), A(KC_LEFT)) },
        [4] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) },
        [5] = { ENCODER_CCW_CW(KC_TRNS, KC_TRNS), ENCODER_CCW_CW(KC_TRNS, KC_TRNS) }
    };
#endif

// Keyboard initialization
void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(GP12);
    uint32_t eeprom_data = eeconfig_read_user();
    
    // Check if EEPROM data is valid
    uint16_t raw_cursor_speed = eeprom_data & 0xFFFF;
    uint8_t raw_scroll_speed = (eeprom_data >> 18) & 0x3F;
    
    // Initialize with defaults if values are out of range
    if (raw_cursor_speed < MIN_CURSOR_SPEED || raw_cursor_speed > MAX_CURSOR_SPEED) {
        cursor_speed_normal = DEFAULT_CURSOR_SPEED;
    } else {
        cursor_speed_normal = raw_cursor_speed;
    }
    
    if (raw_scroll_speed < MIN_SCROLL_SPEED || raw_scroll_speed > MAX_SCROLL_SPEED) {
        scroll_speed = DEFAULT_SCROLL_SPEED;
    } else {
        scroll_speed = raw_scroll_speed;
    }
    
    scroll_dir_v = (eeprom_data >> 16) & 1;
    scroll_dir_h = (eeprom_data >> 17) & 1;
    
    // Save corrected values back to EEPROM if needed
    if (raw_cursor_speed != cursor_speed_normal || raw_scroll_speed != scroll_speed) {
        eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
    }
    
    // Load trackpad layer configuration from EEPROM
    trackpad_config_load();
    
    // Load saved sniper settings from EEPROM
    load_sniper_settings();
    
    // Load saved zoom setting from EEPROM
    load_zoom_setting();
    
    // Initialize OS detection toggle state
    os_detection_init();
    
#ifdef VIAL_USER_CONFIG_ENABLE
    vial_user_config_init(&user_config, sizeof(user_config));
#endif
}

void matrix_scan_user(void) {
    // Maintain split keyboard communication timing for animation sync
    static uint16_t sync_timer = 0;
    if (timer_elapsed(sync_timer) > 50) {  // Regular background processing every 50ms
        sync_timer = timer_read();
        // This regular timer check maintains communication patterns between halves
        // similar to what OS detection provided in the working version
    }
    
#ifdef SUPER_ALT_TAB_ENABLE
    if (is_alt_tab_active) {
        if (timer_elapsed(alt_tab_timer) > 1000) {
            os_variant_t detected_os = get_effective_os_detection();
            if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                unregister_code(KC_LGUI); // Cmd key on macOS/iOS
            } else {
                unregister_code(KC_LALT); // Alt key on Linux/Windows/Default (universal)
            }
            is_alt_tab_active = false;
        }
    }
#endif

    // Handle sniper info mode timeout
    if (sniper_info_mode) {
        if (timer_elapsed(info_timer) > INFO_TIMEOUT) {
            sniper_info_mode = false;
        }
    }
}

// TPS65-optimized pointing device task with overflow protection
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    // Check if trackpad is disabled
    if (!trackpad_enabled) {
        // Return empty mouse report to disable all trackpad input
        mouse_report.x = 0;
        mouse_report.y = 0;
        mouse_report.h = 0;
        mouse_report.v = 0;
        mouse_report.buttons = 0; // Disable all mouse buttons from trackpad
        return mouse_report;
    }
    
    // Handle hardware zoom gestures - OS-aware numpad approach (if enabled)
    static bool zoom_active = false;
    static bool zoom_using_cmd = false;
    
    if (zoom_enabled && (mouse_report.buttons & (1 << 6)) != 0) { // Mouse Button 7 - Zoom Out
        if (!zoom_active) {
            os_variant_t detected_os = get_effective_os_detection();
            if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                // Send Cmd+Numpad- for zoom out on macOS
                register_code(KC_LGUI);
                zoom_using_cmd = true;
            } else {
                // Send Ctrl+Numpad- for zoom out on Windows/Linux
                register_code(KC_LCTL);
                zoom_using_cmd = false;
            }
            register_code(KC_KP_MINUS);
            zoom_active = true;
        }
        mouse_report.buttons &= ~(1 << 6); // Clear the button
    } else if (zoom_enabled && (mouse_report.buttons & (1 << 7)) != 0) { // Mouse Button 8 - Zoom In
        if (!zoom_active) {
            os_variant_t detected_os = get_effective_os_detection();
            if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                // Send Cmd+Numpad+ for zoom in on macOS
                register_code(KC_LGUI);
                zoom_using_cmd = true;
            } else {
                // Send Ctrl+Numpad+ for zoom in on Windows/Linux
                register_code(KC_LCTL);
                zoom_using_cmd = false;
            }
            register_code(KC_KP_PLUS);
            zoom_active = true;
        }
        mouse_report.buttons &= ~(1 << 7); // Clear the button
    } else {
        if (zoom_active) {
            // Release numpad keys first, then modifier
            unregister_code(KC_KP_PLUS);
            unregister_code(KC_KP_MINUS);
            if (zoom_using_cmd) {
                unregister_code(KC_LGUI);
            } else {
                unregister_code(KC_LCTL);
            }
        }
        zoom_active = false; // Reset when no zoom buttons pressed
    }
    
    // Clear zoom buttons if zoom is disabled
    if (!zoom_enabled) {
        mouse_report.buttons &= ~((1 << 6) | (1 << 7)); // Clear both zoom buttons
    }
    
    // Check for modifier-based sniper activation (if configured)
    static bool sniper_toggled_manually = false;
    if (sniper_modifier_mask != 0) {
        uint8_t current_mods = get_mods() | get_oneshot_mods() | get_weak_mods();
        bool mods_match = (current_mods & sniper_modifier_mask) == sniper_modifier_mask;
        static bool prev_mods_match = false;
        static bool prev_sniper_state = false;
        // Detect state changes to track manual vs modifier-based activation
        if (sniper_mode_active != prev_sniper_state) {
            // Sniper mode changed - check if it was due to modifiers or manual toggle
            if (!prev_mods_match && !mods_match) {
                // Neither previous nor current modifiers match, so this was manual
                sniper_toggled_manually = sniper_mode_active;
            }
        }
        
        // Handle modifier-based activation/deactivation
        if (mods_match && !prev_mods_match) {
            // Modifiers just became active - enable sniper mode
            sniper_mode_active = true;
        } else if (!mods_match && prev_mods_match) {
            // Modifiers just became inactive - disable sniper mode (unless manually toggled)
            if (!sniper_toggled_manually) {
                sniper_mode_active = false;
            }
        }
        
        prev_mods_match = mods_match;
        prev_sniper_state = sniper_mode_active;
    }
    
    // Handle learning mode for modifier detection
    if (sniper_learning_mode) {
        uint16_t current_time = timer_read();
        uint8_t current_mods = get_mods() | get_oneshot_mods() | get_weak_mods();
        
        if (current_mods != 0) {
            // User is holding modifiers - learn them
            sniper_modifier_mask = current_mods;
            sniper_learning_mode = false;
            // Save learned modifiers to EEPROM for persistence
            save_sniper_settings();
        } else if (current_time - learning_timer > LEARNING_TIMEOUT) {
            // Timeout - exit learning mode
            sniper_learning_mode = false;
        }
    }
    
    // Handle info mode timeout
    if (sniper_info_mode) {
        uint16_t current_time = timer_read();
        if (current_time - info_timer > INFO_DISPLAY_TIMEOUT) {
            sniper_info_mode = false;
        }
    }
    
    // Get raw trackpad values - these are already filtered by the TPS65 hardware
    int8_t raw_x = mouse_report.x;
    int8_t raw_y = mouse_report.y;
    
    // Get current layer for gesture detection
    uint8_t layer = get_highest_layer(layer_state);
    
    // Handle layer-specific gestures first
    if (user_config.scroll_layers & (1 << layer)) {
        // SCROLL MODE - Convert cursor movement to scroll
        static int16_t scroll_buffer_x = 0;
        static int16_t scroll_buffer_y = 0;
        
        // Calculate scroll divider based on scroll speed setting (TPS65 optimized)
        int16_t divider = SCROLL_DIVIDER_MEDIUM; // Default fallback
        switch (scroll_speed) {
            case 1: divider = SCROLL_DIVIDER_SLOWEST; break; // 16
            case 2: divider = SCROLL_DIVIDER_SLOWER;  break; // 12  
            case 3: divider = SCROLL_DIVIDER_SLOW;    break; // 8
            case 4: divider = SCROLL_DIVIDER_MEDIUM;  break; // 6 (new default)
            case 5: divider = SCROLL_DIVIDER_FAST;    break; // 4
            case 6: divider = SCROLL_DIVIDER_FASTER;  break; // 3
            case 7: divider = SCROLL_DIVIDER_FASTEST; break; // 2
            case 8: divider = SCROLL_DIVIDER_TURBO;   break; // 1
        }
        
        // Accumulate movement
        scroll_buffer_x += raw_x;
        scroll_buffer_y += raw_y;
        
        // Convert to scroll when buffer exceeds threshold
        if (abs(scroll_buffer_x) >= divider) {
            mouse_report.h = (scroll_buffer_x / divider) * (scroll_dir_h ? -1 : 1);
            scroll_buffer_x = scroll_buffer_x % divider;
        }
        if (abs(scroll_buffer_y) >= divider) {
            mouse_report.v = (scroll_buffer_y / divider) * (scroll_dir_v ? -1 : 1);
            scroll_buffer_y = scroll_buffer_y % divider;
        }
        
        // Clear cursor movement
        mouse_report.x = 0;
        mouse_report.y = 0;
        return mouse_report;
        
    } else if (user_config.swipe2_layers & (1 << layer)) {
        // 2-FINGER SWIPE MODE (horizontal gestures)
        uint32_t current_time = timer_read();
        
        if (abs(raw_x) > SWIPE_THRESHOLD) {
            if (!two_finger_gesture_active && 
                (current_time - last_swipe_time) > SWIPE_COOLDOWN_MS) {
                
                os_variant_t detected_os = get_effective_os_detection();
                if (raw_x > 0) {
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        tap_code16(G(KC_LEFT));  // Browser back on macOS/iOS (universal)
                    } else {
                        tap_code16(A(KC_LEFT));  // Browser back on Linux/Windows/Default (universal)
                    }
                } else {
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        tap_code16(G(KC_RIGHT)); // Browser forward on macOS/iOS (universal)
                    } else {
                        tap_code16(A(KC_RIGHT)); // Browser forward on Linux/Windows/Default (universal)
                    }
                }
                
                two_finger_gesture_active = true;
                last_swipe_time = current_time;
            }
        } else {
            two_finger_gesture_active = false;
        }
        
        // Clear all movement
        mouse_report.x = 0;
        mouse_report.y = 0;
        return mouse_report;
        
    } else if (user_config.swipe3_layers & (1 << layer)) {
        // 3-FINGER SWIPE MODE (4-directional gestures)
        uint32_t current_time = timer_read();
        
        if ((abs(raw_x) > SWIPE_THRESHOLD || abs(raw_y) > SWIPE_THRESHOLD)) {
            if (!three_finger_gesture_active && 
                (current_time - last_swipe_time) > SWIPE_COOLDOWN_MS) {
                
                // Determine primary direction
                if (abs(raw_y) > abs(raw_x)) {
                    // Vertical swipe - Mission Control/Task View/Activities
                    os_variant_t detected_os = get_effective_os_detection();
                    if (raw_y > 0) {
                        if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                            tap_code16(C(KC_DOWN));  // App Exposé on macOS/iOS
                        } else {
                            tap_code16(G(KC_D));     // Show Desktop on Linux/Windows/Default (universal)
                        }
                    } else {
                        if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                            tap_code16(C(KC_UP));    // Mission Control on macOS/iOS
                        } else {
                            tap_code16(G(KC_TAB));   // Activities/Task View on Linux/Windows/Default (universal)
                        }
                    }
                } else {
                    // Horizontal swipe - Desktop switching
                    os_variant_t detected_os = get_effective_os_detection();
                    if (raw_x > 0) {
                        if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                            tap_code16(C(KC_LEFT));  // Previous desktop on macOS/iOS
                        } else if (detected_os == OS_WINDOWS) {
                            tap_code16(G(C(KC_LEFT))); // Previous virtual desktop on Windows
                        } else {
                            tap_code16(C(A(KC_LEFT))); // Previous desktop on Linux/Default (GNOME/KDE/XFCE)
                        }
                    } else {
                        if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                            tap_code16(C(KC_RIGHT)); // Next desktop on macOS/iOS
                        } else if (detected_os == OS_WINDOWS) {
                            tap_code16(G(C(KC_RIGHT))); // Next virtual desktop on Windows
                        } else {
                            tap_code16(C(A(KC_RIGHT))); // Next desktop on Linux/Default (GNOME/KDE/XFCE)
                        }
                    }
                }
                
                three_finger_gesture_active = true;
                last_swipe_time = current_time;
            }
        } else {
            three_finger_gesture_active = false;
        }
        
        // Clear all movement
        mouse_report.x = 0;
        mouse_report.y = 0;
        return mouse_report;
    }
    
    // NORMAL CURSOR MODE with TPS65-optimized acceleration and overflow protection
    
    // Apply base scaling first using current active speed
    int16_t current_speed = get_current_cursor_speed();
    float base_scale = dpi_base_scale[current_speed - 1];
    float scaled_x = raw_x * base_scale;
    float scaled_y = raw_y * base_scale;
    
    // Calculate velocity from scaled movement
    float velocity = sqrtf(scaled_x * scaled_x + scaled_y * scaled_y);
    
    // Apply conservative acceleration to prevent overflow
    float accel_factor = 1.0f;
    if (velocity > ACCEL_THRESHOLD_LOW) {
        float speed_bonus = (current_speed - 1) * ACCEL_BASE_MULTIPLIER;
        
        if (velocity > ACCEL_THRESHOLD_HIGH) {
            // High velocity - conservative acceleration
            accel_factor = 1.0f + speed_bonus * ACCEL_FACTOR_HIGH;
        } else if (velocity > ACCEL_THRESHOLD_MED) {
            // Medium velocity - moderate acceleration
            accel_factor = 1.0f + speed_bonus * ACCEL_FACTOR_MED;
        } else {
            // Low velocity - mild acceleration
            accel_factor = 1.0f + speed_bonus * ACCEL_FACTOR_LOW;
        }
    }
    
    // Calculate final movement with overflow protection
    int16_t final_x = (int16_t)(scaled_x * accel_factor);
    int16_t final_y = (int16_t)(scaled_y * accel_factor);
    
    // CRITICAL: Clamp values to prevent int8_t overflow that causes cursor bounce
    mouse_report.x = (final_x > 127) ? 127 : (final_x < -127) ? -127 : (int8_t)final_x;
    mouse_report.y = (final_y > 127) ? 127 : (final_y < -127) ? -127 : (int8_t)final_y;
    
    // Ensure minimum movement of 1 pixel if there was any input (but respect clamping)
    if (raw_x != 0 && mouse_report.x == 0 && final_x != 0) {
        mouse_report.x = (final_x > 0) ? 1 : -1;
    }
    if (raw_y != 0 && mouse_report.y == 0 && final_y != 0) {
        mouse_report.y = (final_y > 0) ? 1 : -1;
    }
    
    // Handle hardware 2-finger scroll wheel events from TPS65
    // This section processes scroll events and applies speed/direction settings
    static int16_t h_wheel = 0;
    static uint8_t h_wheel_count = 0;
    if (mouse_report.h != 0) {
        h_wheel_count++;
        h_wheel += scroll_dir_h ? -mouse_report.h : mouse_report.h;
        mouse_report.h = 0;
        
        // Use scroll speed setting for hardware scroll divider
        // Halve the dividers to match 1-finger scroll speed (user reported 2x slower)
        int16_t hw_divider = SCROLL_DIVIDER_MEDIUM / 2; // Default
        switch (scroll_speed) {
            case 1: hw_divider = SCROLL_DIVIDER_SLOWEST / 2; break; // 8 (was 16)
            case 2: hw_divider = SCROLL_DIVIDER_SLOWER / 2;  break; // 6 (was 12)
            case 3: hw_divider = SCROLL_DIVIDER_SLOW / 2;    break; // 4 (was 8)
            case 4: hw_divider = SCROLL_DIVIDER_MEDIUM / 2;  break; // 3 (was 6)
            case 5: hw_divider = SCROLL_DIVIDER_FAST / 2;    break; // 2 (was 4)
            case 6: hw_divider = SCROLL_DIVIDER_FASTER / 2;  break; // 1.5->1 (was 3)
            case 7: hw_divider = SCROLL_DIVIDER_FASTEST / 2; break; // 1 (was 2)
            case 8: hw_divider = 1; break; // 1 (minimum, was 1)
        }
        // Ensure minimum divider of 1
        if (hw_divider < 1) hw_divider = 1;
        
        if (h_wheel_count >= hw_divider) {
            mouse_report.h = (h_wheel > 0) ? 1 : -1;
            h_wheel = 0;
            h_wheel_count = 0;
        }
    } else if (h_wheel_count > 0) {
        mouse_report.h = (h_wheel > 0) ? 1 : -1;
        h_wheel = 0;
        h_wheel_count = 0;
    }
    
    static int16_t v_wheel = 0;
    static uint8_t v_wheel_count = 0;
    if (mouse_report.v != 0) {
        v_wheel_count++;
        v_wheel += scroll_dir_v ? -mouse_report.v : mouse_report.v;
        mouse_report.v = 0;
        
        // Use scroll speed setting for hardware scroll divider
        // Halve the dividers to match 1-finger scroll speed (user reported 2x slower)
        int16_t hw_divider = SCROLL_DIVIDER_MEDIUM / 2; // Default
        switch (scroll_speed) {
            case 1: hw_divider = SCROLL_DIVIDER_SLOWEST / 2; break; // 8 (was 16)
            case 2: hw_divider = SCROLL_DIVIDER_SLOWER / 2;  break; // 6 (was 12)
            case 3: hw_divider = SCROLL_DIVIDER_SLOW / 2;    break; // 4 (was 8)
            case 4: hw_divider = SCROLL_DIVIDER_MEDIUM / 2;  break; // 3 (was 6)
            case 5: hw_divider = SCROLL_DIVIDER_FAST / 2;    break; // 2 (was 4)
            case 6: hw_divider = SCROLL_DIVIDER_FASTER / 2;  break; // 1.5->1 (was 3)
            case 7: hw_divider = SCROLL_DIVIDER_FASTEST / 2; break; // 1 (was 2)
            case 8: hw_divider = 1; break; // 1 (minimum, was 1)
        }
        // Ensure minimum divider of 1
        if (hw_divider < 1) hw_divider = 1;
        
        if (v_wheel_count >= hw_divider) {
            mouse_report.v = (v_wheel > 0) ? 1 : -1;
            v_wheel = 0;
            v_wheel_count = 0;
        }
    } else if (v_wheel_count > 0) {
        mouse_report.v = (v_wheel > 0) ? 1 : -1;
        v_wheel = 0;
        v_wheel_count = 0;
    }
    
    return mouse_report;
}

// Process record user - handle custom keycodes
#include <eeprom.h>

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
#ifdef SUPER_ALT_TAB_ENABLE
        case CK_ATABF:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI); // Cmd key on macOS/iOS
                    } else {
                        register_code(KC_LALT); // Alt key on Linux/Windows/Default (universal)
                    }
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
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI); // Cmd key on macOS/iOS
                    } else {
                        register_code(KC_LALT); // Alt key on Linux/Windows/Default (universal)
                    }
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
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI); // Cmd key on macOS/iOS
                    } else {
                        register_code(KC_LALT); // Alt key on Linux/Windows/Default (universal)
                    }
                }
                alt_tab_timer = timer_read();
                register_code(KC_MS_WH_UP);
            } else {
                unregister_code(KC_MS_WH_UP);
            }
            break;
            
        case CK_ATMWD:
            if (record->event.pressed) {
                if (!is_alt_tab_active) {
                    is_alt_tab_active = true;
                    os_variant_t detected_os = get_effective_os_detection();
                    if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                        register_code(KC_LGUI); // Cmd key on macOS/iOS
                    } else {
                        register_code(KC_LALT); // Alt key on Linux/Windows/Default (universal)
                    }
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
                os_variant_t detected_os = get_effective_os_detection();
                if (detected_os == OS_MACOS || detected_os == OS_IOS) {
                    register_code(KC_LGUI);
                    register_code(KC_D);
                    unregister_code(KC_D);
                    unregister_code(KC_LGUI);
                } else {
                    register_code(KC_LALT);
                    register_code(KC_F4);
                    unregister_code(KC_F4);
                    unregister_code(KC_LALT);
                }
            }
            break;

        case SCROLL_DIR_V:
            if (record->event.pressed) {
                scroll_dir_v = !scroll_dir_v;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;
        
        case SCROLL_DIR_H:
            if (record->event.pressed) {
                scroll_dir_h = !scroll_dir_h;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            return false;        
        
        case CURSOR_SPEED_UP:
            if (record->event.pressed) {
                cursor_speed_normal = (cursor_speed_normal < MAX_CURSOR_SPEED) ? (cursor_speed_normal + 1) : MIN_CURSOR_SPEED;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;

        case CURSOR_SPEED_DN:
            if (record->event.pressed && cursor_speed_normal > MIN_CURSOR_SPEED) {
                cursor_speed_normal--;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;

        case CURSOR_SPEED_RESET:
            if (record->event.pressed) {
                cursor_speed_normal = DEFAULT_CURSOR_SPEED;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;

        case SCROLL_SPEED_UP:
            if (record->event.pressed) {
                scroll_speed = (scroll_speed < MAX_SCROLL_SPEED) ? (scroll_speed + 1) : MIN_SCROLL_SPEED;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;

        case SCROLL_SPEED_DOWN:
            if (record->event.pressed && scroll_speed > MIN_SCROLL_SPEED) {
                scroll_speed--;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;

        case SCROLL_SPEED_RESET:
            if (record->event.pressed) {
                scroll_speed = DEFAULT_SCROLL_SPEED;
                eeconfig_update_user((cursor_speed_normal & 0xFFFF) | (scroll_dir_v << 16) | (scroll_dir_h << 17) | (scroll_speed << 18));
            }
            break;

        case TRACKPAD_LAYER_SCROLL_SET:
            if (record->event.pressed) {
                uint8_t current_layer = get_highest_layer(layer_state);
                if (current_layer <= 15) {
                    uint16_t layer_bit = (1 << current_layer);
                    user_config.swipe2_layers &= ~layer_bit;
                    user_config.swipe3_layers &= ~layer_bit;
                    user_config.scroll_layers ^= layer_bit;
                    trackpad_config_save();
                }
            }
            break;
            
        case TRACKPAD_LAYER_SWIPE2_SET:
            if (record->event.pressed) {
                uint8_t current_layer = get_highest_layer(layer_state);
                if (current_layer <= 15) {
                    uint16_t layer_bit = (1 << current_layer);
                    user_config.scroll_layers &= ~layer_bit;
                    user_config.swipe3_layers &= ~layer_bit;
                    user_config.swipe2_layers ^= layer_bit;
                    trackpad_config_save();
                }
            }
            break;
            
        case TRACKPAD_LAYER_SWIPE3_SET:
            if (record->event.pressed) {
                uint8_t current_layer = get_highest_layer(layer_state);
                if (current_layer <= 15) {
                    uint16_t layer_bit = (1 << current_layer);
                    user_config.scroll_layers &= ~layer_bit;
                    user_config.swipe2_layers &= ~layer_bit;
                    user_config.swipe3_layers ^= layer_bit;
                    trackpad_config_save();
                }
            }
            break;
            
        case TRACKPAD_LAYER_RESET:
            if (record->event.pressed) {
                trackpad_layer_reset();
            }
            break;
            
        case TRACKPAD_TOGGLE:
            if (record->event.pressed) {
                trackpad_enabled = !trackpad_enabled;
            }
            break;
            
        case SNIPER_TOG:
            if (record->event.pressed) {
                sniper_mode_active = !sniper_mode_active;
            }
            break;
            
        case SNIPER_MO:
            // Momentary sniper - active while key is held
            sniper_mode_active = record->event.pressed;
            break;
            
        case SNIPER_DPI_UP:
            if (record->event.pressed) {
                cursor_speed_sniper = (cursor_speed_sniper < MAX_CURSOR_SPEED) ? (cursor_speed_sniper + 1) : MIN_CURSOR_SPEED;
                save_sniper_settings();  // Save DPI change to EEPROM
            }
            break;
            
        case SNIPER_DPI_DOWN:
            if (record->event.pressed && cursor_speed_sniper > MIN_CURSOR_SPEED) {
                cursor_speed_sniper--;
                save_sniper_settings();  // Save DPI change to EEPROM
            }
            break;
            
        case SNIPER_SET_MODS:
            if (record->event.pressed) {
                sniper_learning_mode = true;
                learning_timer = timer_read();
            }
            break;
            
        case SNIPER_SHOW_MODS:
            if (record->event.pressed) {
                sniper_info_mode = true;
                info_timer = timer_read();
            }
            break;
            
        case OS_DETECTION_TOGGLE:
            if (record->event.pressed) {
                toggle_os_detection();
            }
            break;
            
        case ZMTOG:
            if (record->event.pressed) {
                zoom_enabled = !zoom_enabled;
                save_zoom_setting();  // Save to EEPROM
            }
            break;
    }
    
    return true;  
}

/* OLED customisation */
#ifdef OLED_ENABLE


unsigned int animation_state = 0;

// Expose scroll_speed from keymap
extern int16_t scroll_speed;

static const char PROGMEM space_row_1[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xc0, 0xfc, 0xff, 0xff, 0xff, 0xe1, 0xc0, 0xc0, 0xc0, 0x80, 0x80, 0x80, 0x38, 0x38,
    0x78, 0x78, 0x7b, 0x7f, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc3, 0x03, 0x03, 0x1f, 0xff,
    0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x70, 0x60, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xc0, 0x80, 0x00, 0x00, 0x00,
    0xf8, 0xfc, 0xe6, 0xb2, 0x3e, 0xbe, 0xfe, 0xec, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xff, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,
    0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xc0, 0xfc, 0xff, 0xff, 0xff, 0xe1, 0xc0, 0xc0, 0xc0, 0x80, 0x80, 0x80, 0x38, 0x38,
    0x78, 0x78, 0x7b, 0x7f, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc3, 0x03, 0x03, 0x1f, 0xff,
    0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x70, 0x60, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM space_row_2[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x1f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0,
    0xc0, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x3f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x38, 0x38, 0x00, 0x00, 0x03, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x06, 0x07, 0x05, 0x03, 0x00, 0x00,
    0x00, 0x01, 0x03, 0x03, 0x02, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xc0, 0xc0, 0x40, 0x60, 0x20, 0x20,
    0x38, 0x3f, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xe0, 0xf0, 0x70, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x1f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0,
    0xc0, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x3f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x38, 0x38, 0x00, 0x00, 0x03, 0x03, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM space_row_3[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0f, 0x0f, 0x1f, 0x3f, 0x3f, 0x3f, 0x7f, 0x7f,
    0x7f, 0x7c, 0x7c, 0x7c, 0x7f, 0x7f, 0x7f, 0x3f, 0x3f, 0x1f, 0x1f, 0x0f, 0x07, 0x07, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0x70, 0xb8, 0x38, 0x3c, 0x7c, 0xfc, 0xfc,
    0xfc, 0xfe, 0xfe, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xc0, 0xe0, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0e, 0x0e, 0x0e, 0x02, 0x02, 0x03, 0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x06, 0x0c, 0x18, 0x30,
    0xe0, 0xc0, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0f, 0x0f, 0x1f, 0x3f, 0x3f, 0x3f, 0x7f, 0x7f,
    0x7f, 0x7c, 0x7c, 0x7c, 0x7f, 0x7f, 0x7f, 0x3f, 0x3f, 0x1f, 0x1f, 0x0f, 0x07, 0x07, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0x70, 0xb8, 0x38, 0x3c, 0x7c, 0xfc, 0xfc,
    0xfc, 0xfe, 0xfe, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xc0, 0xe0, 0xc0, 0x00, 0x00
};

static const char PROGMEM space_row_4[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0e, 0x0e,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x1e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x0e, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0f, 0x1f, 0x38, 0x31, 0x71, 0x70, 0x30, 0x3c, 0x1f,
    0x0f, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x03, 0x01, 0x03, 0x03, 0x02, 0x02, 0x06, 0x04, 0x04, 0x0c, 0x0c, 0x08, 0x18, 0x18,
    0x10, 0x38, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x07, 0x0f, 0x1f, 0x3f, 0x3f, 0x3f, 0x7f, 0x7e, 0x7e, 0x7c, 0x3c, 0x3c, 0x1c, 0x1c, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x38, 0x38, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0e, 0x0e,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x1e, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x0e, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0f, 0x1f, 0x38, 0x31, 0x71, 0x70, 0x30, 0x3c, 0x1f,
    0x0f, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00
};

static const char PROGMEM ship_row_1[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0xc0, 0xc0, 0xc0, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM ship_row_2[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0x7e, 0x3e, 0x7c, 0xfc, 0xf8, 0xf0, 0xf0, 0xe0, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM ship_row_3[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x60, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x1e, 0x1f, 0x1f, 0x0f, 0x07, 0x07, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM ship_row_4[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char PROGMEM mask_row_1[] = {
    0xff, 0xff, 0xff, 0xff, 0x1f, 0x1f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
    0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static const char PROGMEM mask_row_2[] = {
    0xff, 0xff, 0xff, 0xff, 0xfe, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x0f,
    0x1f, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static const char PROGMEM mask_row_3[] = {
    0xff, 0xff, 0xff, 0xff, 0x3f, 0x1c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0xc0, 0xe0, 0xf0, 0xf0,
    0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static const char PROGMEM mask_row_4[] = {
    0xff, 0xff, 0xff, 0xff, 0xfc, 0xfc, 0xfc, 0xf8, 0xf8, 0xf8, 0xf0, 0xf0, 0xf0, 0xf8, 0xf8, 0xfc,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static void render_space(void) {

    char wpm = get_current_wpm();
    char render_row[128];
    int i;
    oled_set_cursor(0,0);

    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_1+i+animation_state);
    }

    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_1+i+animation_state)&pgm_read_byte(mask_row_1+i-wpm/4)) | pgm_read_byte(ship_row_1+i-wpm/4);
    }

    oled_write_raw(render_row, 128);
    oled_set_cursor(0,1);

    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_2+i+animation_state);
    }

    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_2+i+animation_state)&pgm_read_byte(mask_row_2+i-wpm/4)) | pgm_read_byte(ship_row_2+i-wpm/4);
    }

    oled_write_raw(render_row, 128);
    oled_set_cursor(0,2);

    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_3+i+animation_state);
    }

    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_3+i+animation_state)&pgm_read_byte(mask_row_3+i-wpm/4)) | pgm_read_byte(ship_row_3+i-wpm/4);
    }

    oled_write_raw(render_row, 128);
    oled_set_cursor(0,3);

    for(i=0; i<wpm/4; i++) {
        render_row[i] = pgm_read_byte(space_row_4+i+animation_state);
    }

    for(i=wpm/4; i<128; i++) {
        render_row[i] = (pgm_read_byte(space_row_4+i+animation_state)&pgm_read_byte(mask_row_4+i-wpm/4)) | pgm_read_byte(ship_row_4+i-wpm/4);
    }

    oled_write_raw(render_row, 128);
    animation_state = (animation_state + 1 + (wpm/15)) % (128*2);

}



/* timers */
//uint32_t anim_timer = 0;
uint32_t anim_sleep = 0;







// Trackpad gesture bitmaps (32x11 pixels each)
// Generated using image2cpp - verified working format

// '2finger gestures', 32x11px
static const char PROGMEM gesture_2finger[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x0c, 
0x34, 0x44, 0x84, 0x04, 0x84, 0x44, 0x34, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
0x00, 0x00, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// '3fingers gestures', 32x11px
static const char PROGMEM gesture_3finger[] = {
0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x0c, 0x34, 0x44, 0x84, 0x04, 
0x84, 0x44, 0x34, 0x0c, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x02, 
0x01, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00
};

// 'default gesture', 32x11px
static const char PROGMEM gesture_default[] = {
0x00, 0x20, 0x20, 0x70, 0xf8, 0xfc, 0xfe, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x84, 
0x0c, 0x3c, 0xfc, 0xfc, 0xfc, 0x3c, 0x0c, 0x04, 0x00, 0xfe, 0xfc, 0xf8, 0x70, 0x20, 0x20, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
0x01, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'scroll gesture', 32x11px  
static const char PROGMEM gesture_scroll[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x04, 
0x0c, 0x3c, 0xfc, 0xfc, 0xfc, 0x3c, 0x0c, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Blank bitmap for clearing gesture area when trackpad is disabled (32x11px = 44 bytes)
static const char gesture_blank[44] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Function to get the appropriate gesture bitmap
static const char* get_trackpad_gesture_bitmap(uint8_t layer) {
    if (user_config.scroll_layers & (1 << layer)) {
        return gesture_scroll;
    } else if (user_config.swipe2_layers & (1 << layer)) {
        return gesture_2finger;
    } else if (user_config.swipe3_layers & (1 << layer)) {
        return gesture_3finger;
    } else {
        return gesture_default;
    }
}

// Large LCD-style digit bitmaps (32x32 pixels, 4 rows tall)
// Generated using image2cpp - verified working format

// Digit 0: '0', 32x32px
static const char PROGMEM digit_0[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf0, 0xf8, 0xf8, 0xf8, 0x7c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x3c, 0x3c, 0x7c, 0xf8, 0xf8, 0xf8, 0xf0, 0xf0, 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x0f, 0x3f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xc0, 0x80, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x80, 0xc0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x3f, 0x0f, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x07, 0x07, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x07, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 1: '1', 32x32px
static const char PROGMEM digit_1[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xc0, 0xc0, 0xe0, 0xf0, 
    0xf8, 0xfc, 0xfc, 0xfc, 0xfc, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x1e, 0x1e, 0x1e, 0x1f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x03, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 2: '2', 32x32px
static const char PROGMEM digit_2[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf0, 0xf8, 0xf8, 0xf8, 0x7c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x3c, 0x3c, 0x7c, 0x7c, 0xf8, 0xf8, 0xf8, 0xf0, 0xf0, 0xe0, 0xc0, 0x80, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x80, 0x80, 0xc0, 0xe0, 0xf0, 0xff, 0xff, 0xff, 0x7f, 0x7f, 0x1f, 0x0f, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xe0, 0xf0, 0xf8, 0xf8, 0xfc, 0x7e, 0x3e, 0x3f, 
    0x1f, 0x0f, 0x0f, 0x07, 0x07, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x06, 0x00, 0x00
};

// Digit 3: '3', 32x32px
static const char PROGMEM digit_3[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0xe0, 0xf0, 0xf8, 0xf8, 0xf8, 0x7c, 0x3c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x3c, 0x7c, 0x7c, 0xfc, 0xf8, 0xf8, 0xf8, 0xf0, 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x01, 0x01, 0x60, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
    0xf0, 0xf0, 0xf8, 0xf8, 0xff, 0xff, 0xff, 0xdf, 0xdf, 0x8f, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x20, 0xf0, 0xf8, 0xf8, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x80, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x81, 0x81, 0xc3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3c, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x07, 0x07, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x07, 0x07, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 4: '4', 32x32px
static const char PROGMEM digit_4[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 
    0xe0, 0xf0, 0xf8, 0xf8, 0xfc, 0xfc, 0xfc, 0xfc, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0x0f, 
    0x07, 0x03, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x38, 0x7c, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7b, 0x79, 0x78, 0x78, 0x78, 0x78, 0x78, 
    0x78, 0x78, 0x78, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x78, 0x78, 0x78, 0x30, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 5: '5', 32x32px
static const char PROGMEM digit_5[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf8, 0xfc, 0xfc, 0xfc, 0xfc, 0x3c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xe0, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0x70, 0x70, 0x78, 0x78, 
    0x78, 0x78, 0x78, 0xf8, 0xf8, 0xf0, 0xf0, 0xf0, 0xe0, 0xe0, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x60, 0xf0, 0xf1, 0xf3, 0xf3, 0xe1, 0xc1, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x80, 0x80, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x3e, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x07, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x07, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 6: '6', 32x32px
static const char PROGMEM digit_6[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf0, 0xf8, 0xf8, 0x7c, 0x7c, 0x3c, 
    0x3c, 0x3c, 0x3c, 0x3c, 0x7c, 0x7c, 0xf8, 0xf8, 0xf8, 0xf0, 0xf0, 0xe0, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xe0, 0xf0, 0x70, 0x78, 
    0x78, 0x78, 0x78, 0x78, 0xf8, 0xf8, 0xf8, 0xf1, 0xf1, 0xe1, 0xe1, 0xc1, 0x80, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe1, 0xc0, 0x80, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x07, 0x07, 0x07, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x07, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00
};

// Digit 7: '7', 32x32px
static const char PROGMEM digit_7[] = {
    0x00, 0x00, 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x3c, 0x3c, 0x3c, 0xbc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0xfc, 0x7c, 0x3c, 0x18, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 
    0xf0, 0xfc, 0xfe, 0xff, 0x7f, 0x3f, 0x0f, 0x07, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xf8, 0xfe, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 8: '8', 32x32px
static const char PROGMEM digit_8[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xf8, 0xf8, 0x7c, 0x7c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x3c, 0x3c, 0x7c, 0xfc, 0xf8, 0xf8, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x03, 0x0f, 0x9f, 0x9f, 0xff, 0xff, 0xff, 0xf8, 0xf8, 0xf0, 0xf0, 0xf0, 
    0xf0, 0xf0, 0xf0, 0xf8, 0xfc, 0xff, 0xff, 0xdf, 0x9f, 0x8f, 0x0f, 0x03, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x7c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc3, 0x81, 0x81, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x81, 0x81, 0xc3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7c, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x03, 0x07, 0x07, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x07, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Digit 9: '9', 32x32px
static const char PROGMEM digit_9[] = {
    0x00, 0x00, 0x00, 0x80, 0xe0, 0xe0, 0xf0, 0xf8, 0xf8, 0xf8, 0xfc, 0x7c, 0x3c, 0x3c, 0x3c, 0x3c, 
    0x3c, 0x7c, 0x7c, 0xf8, 0xf8, 0xf8, 0xf0, 0xf0, 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x3e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe1, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x80, 0xc0, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x80, 0xc1, 0xc3, 0xc3, 0xc7, 0xc7, 0x8f, 0x8f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x07, 0x87, 0x83, 0xe3, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x3f, 0x07, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x07, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
    0x0f, 0x0f, 0x0f, 0x07, 0x07, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Large layer number display using image2cpp generated bitmaps
static void display_large_layer_number(uint8_t layer) {
    const char* bitmap = NULL;
    
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
        oled_write_raw_P(bitmap, 128); // Write 32x32 bitmap (128 bytes)
    }
}

static void print_status_narrow(void) {
   

    /* SOFLE header and OS detection status */
        oled_set_cursor(0,1);
        oled_write("SOFLE", false);
        oled_set_cursor(0,2);
        
        // Display OS detection status
        if (!os_detection_enabled) {
            oled_write("PLUS+", false);  // Default branding when disabled
        } else {
            os_variant_t detected_os = detected_host_os();  // Get actual OS when enabled
            switch(detected_os) {
                case OS_MACOS:
                case OS_IOS:
                    oled_write(" MAC ", false);
                    break;
                case OS_WINDOWS:
                    oled_write(" WIN ", false);
                    break;
                case OS_LINUX:
                    oled_write("LINUX", false);
                    break;
                default:
                    oled_write("PLUS+", false);  // Fallback to default branding
                    break;
            }
        }

        /* Numlock indicator */
        oled_set_cursor(0,3);
        led_t led_usb_state = host_keyboard_led_state();
        if (led_usb_state.num_lock) {
            oled_write_P(PSTR("NUMLK"), false);
        } else {
            oled_write_P(PSTR("     "), false); // Clear if num lock off
        }

        /* Trackpad gesture bitmap display (32x11px at row 4) - only when trackpad enabled and not in special modes */
        uint8_t current_layer = get_highest_layer(layer_state);
        if (trackpad_enabled && !sniper_learning_mode && !sniper_info_mode) {
            const char* gesture_bitmap = get_trackpad_gesture_bitmap(current_layer);
            oled_set_cursor(0, 4);
            oled_write_raw_P(gesture_bitmap, 44); // 32x11 = 44 bytes
        } else {
            /* Clear gesture bitmap area when trackpad is disabled or in special modes */
            oled_set_cursor(0, 4);
            oled_write_raw_P(gesture_blank, 44); // Clear with blank bitmap (same size as gesture bitmaps)
        }

        /* Always clear the text areas first to prevent artifacts */
        oled_set_cursor(0, 6);
        oled_write_P(PSTR("        "), false); // Clear line 6
        oled_set_cursor(0, 7);
        oled_write_P(PSTR("        "), false); // Clear line 7
        oled_set_cursor(0, 8);
        oled_write_P(PSTR("        "), false); // Clear line 8
        oled_set_cursor(0, 9);
        oled_write_P(PSTR("        "), false); // Clear line 9

        if (trackpad_enabled) {
            // Check for special display modes first
            if (sniper_learning_mode) {
                /* Learning mode display */
                oled_set_cursor(0, 6);
                oled_write_P(PSTR("LEARN"), false);
                oled_set_cursor(0, 7);
                oled_write_P(PSTR("SNIPE"), false);
                
                /* Show "HOLD MODS" instruction */
                oled_set_cursor(0, 8);
                oled_write_P(PSTR("HOLD"), false);
                oled_set_cursor(0, 9);
                oled_write_P(PSTR("MODS"), false);
            } else {
                /* Normal gesture mode text explanation */
                oled_set_cursor(0, 6);
                if (user_config.scroll_layers & (1 << current_layer)) {
                    oled_write_P(PSTR("SCROL"), false);
                } else if (user_config.swipe2_layers & (1 << current_layer)) {
                    oled_write_P(PSTR("2SWPE"), false);
                } else if (user_config.swipe3_layers & (1 << current_layer)) {
                    oled_write_P(PSTR("3SWPE"), false);
                } else {
                    oled_write_P(PSTR("CURSR"), false);
                }

                /* Cursor speed (DPI) */
                oled_set_cursor(0, 8);		
                if (sniper_mode_active) {
                    oled_write_P(PSTR("SNP"), false);	
                } else {
                    oled_write_P(PSTR("NORM"), false);
                }
                // Only show speed/scroll info in normal mode (not learning mode)
                if (!sniper_learning_mode) {
                    oled_set_cursor(4, 8);
                    char speed_str[8]; // Generous buffer to avoid any truncation warnings
                    snprintf(speed_str, sizeof(speed_str), "%d", get_current_cursor_speed());
                    oled_write(speed_str, false); 

                    /* Scroll speed display */
                    oled_set_cursor(0, 9);
                    oled_write_P(PSTR("SCR"), false);
                    oled_set_cursor(4, 9);
                    char scr_str[8]; // Generous buffer to avoid any truncation warnings
                    snprintf(scr_str, sizeof(scr_str), "%d", scroll_speed);
                    oled_write(scr_str, false);
                }
            }
        } else {
            /* No trackpad mode display */
            oled_set_cursor(0, 6);
            oled_write_P(PSTR("NO"), false);
            oled_set_cursor(0, 7);
            oled_write_P(PSTR("TRACKPAD"), false);
        }

        /* Handle sniper info display mode */
        if (sniper_info_mode) {
            /* Clear display area */
            oled_set_cursor(0, 6);
            oled_write_P(PSTR("        "), false);
            oled_set_cursor(0, 7);
            oled_write_P(PSTR("        "), false);
            oled_set_cursor(0, 8);
            oled_write_P(PSTR("        "), false);
            oled_set_cursor(0, 9);
            oled_write_P(PSTR("        "), false);
            
            /* Show sniper modifier info */
            oled_set_cursor(0, 6);
            oled_write_P(PSTR("SNIPER"), false);
            oled_set_cursor(0, 7);
            oled_write_P(PSTR("MODS:"), false);
            
            /* Display learned modifiers */
            oled_set_cursor(0, 8);
            if (sniper_modifier_mask == 0) {
                oled_write_P(PSTR("NONE"), false);
            } else {
                char mod_str[9] = "";  // 8 chars max + null terminator
                if (sniper_modifier_mask & MOD_BIT(KC_LCTL)) strcat(mod_str, "LC");
                if (sniper_modifier_mask & MOD_BIT(KC_LSFT)) strcat(mod_str, "LS");
                if (sniper_modifier_mask & MOD_BIT(KC_LALT)) strcat(mod_str, "LA");
                if (sniper_modifier_mask & MOD_BIT(KC_LGUI)) strcat(mod_str, "LG");
                if (sniper_modifier_mask & MOD_BIT(KC_RCTL)) strcat(mod_str, "RC");
                if (sniper_modifier_mask & MOD_BIT(KC_RSFT)) strcat(mod_str, "RS");
                if (sniper_modifier_mask & MOD_BIT(KC_RALT)) strcat(mod_str, "RA");
                if (sniper_modifier_mask & MOD_BIT(KC_RGUI)) strcat(mod_str, "RG");
                oled_write(mod_str, false);
            }
        }

        /* Large layer number bitmap display (32x32px at row 11-14) */
        display_large_layer_number(current_layer);
}



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

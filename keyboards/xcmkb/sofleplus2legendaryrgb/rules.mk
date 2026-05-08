############ MCU & BOOTLOADER ############
# MCU name
MCU = RP2040

# Bootloader selection
BOOTLOADER = rp2040

# Ignore some warnings during the build, likely to be fixed before RP2040 PR is merged
ALLOW_WARNINGS = yes

# LTO must be disabled for RP2040 builds
LTO_ENABLE = no

# PIO serial/WS2812 drivers must be used on RP2040
SERIAL_DRIVER = vendor
WS2812_DRIVER = vendor 

################## BASIC ##################

SPLIT_KEYBOARD = yes
SPLIT_ACTIVITY_ENABLE = yes     # Sync master activity timestamps to slave so slave RGB timeout uses trackpad events, not just slave keypresses
BOOTMAGIC_ENABLE = yes      # Enable Bootmagic Lite， This is great for boards that don't have a physical reset button
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = yes         # Console for debug
COMMAND_ENABLE = no         # Commands for debug and configuration https://docs.qmk.fm/features/command
SWAP_HANDS_ENABLE = no
NKRO_ENABLE = yes           # Enable N-Key Rollover
ENCODER_MAP_ENABLE = yes	# New update for encoder
ENCODER_ENABLE = yes		# Callbacks for encoder, replace with encoder_map_enable

#DEFAULT_FOLDER = sofle/rev1

#EXTRAFLAGS += -flto

# Build Options
#   change yes to no to disable

# RGB Underglow
RGBLIGHT_ENABLE = no
RGB_MATRIX_ENABLE = yes
VIALRGB_ENABLE = yes

# OLED
OLED_ENABLE = yes
OLED_DRIVER = ssd1306
WPM_ENABLE = yes
OLED_TRANSPORT = i2c

# Mousekey
MOUSEKEY_ENABLE = yes

# V3.03 From DIP to Direct Pin
DIP_SWITCH_ENABLE = no
DIP_SWITCH_MAP_ENABLE = no

DYNAMIC_KEYMAP_ENABLE = yes


#### OS SWITCH ####
OS_DETECTION_ENABLE = yes
KEYBOARD_HOOK_ENABLE = yes
SRC += quantum/os_detection.c

# Vial 0.7.4 - 2025.07.13
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes


# POINTING DEVICE
#POINTING_DEVICE_ENABLE = yes
#POINTING_DEVICE_DRIVER = azoteq_iqs5xx 

DIGITIZER_ENABLE = yes
DIGITIZER_DRIVER = azoteq_iqs5xx

# MULTI-TOUCH
# I2C_DRIVER_REQUIRED = yes
# DIGITIZER_DRIVER = maxtouch
# POINTING_DEVICE_DRIVER = digitizer




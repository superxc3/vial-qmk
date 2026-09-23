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
BOOTMAGIC_ENABLE = no      # Enable Bootmagic Lite
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = yes         # Console for debug
COMMAND_ENABLE = no         # Commands for debug and configuration
SWAP_HANDS_ENABLE = no
NKRO_ENABLE = yes           # Enable N-Key Rollover
ENCODER_MAP_ENABLE = yes	# New update for encoder
ENCODER_ENABLE = yes		# Callbacks for encoder, replace with encoder_map_enable
#DEFAULT_FOLDER = sofle/rev1

# Was: EXTRAFLAGS += -flto
# Contradicted LTO_ENABLE = no above, which is mandatory on RP2040. sofleplus2
# has the same line commented out for the same reason. Left here as a marker so
# it does not get re-added.
#EXTRAFLAGS += -flto

# Build Options
#   change yes to no to disable

# RGB Underglow
RGBLIGHT_ENABLE = no
RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = ws2812
VIALRGB_ENABLE = yes

# OLED
OLED_ENABLE = yes
OLED_DRIVER = ssd1306
WPM_ENABLE = yes
OLED_TRANSPORT = i2c

# Mousekey
MOUSEKEY_ENABLE = yes

# Haptic BZZZ // all defined and added in haptic.c， config.h， and keymap.c
#HAPTIC_ENABLE = yes				# Enable Pimoroni Haptic Bzzz LRA (+1192)
#HAPTIC_DRIVER += drv2605l

# AUDIO // config.h
AUDIO_ENABLE = no
AUDIO_DRIVER = pwm_hardware

# POINTING DEVICE - PIMORONI TRACKBALL
POINTING_DEVICE_ENABLE = yes
#POINTING_DEVICE_DRIVER = pimoroni_trackball

DIP_SWITCH_ENABLE = yes
DIP_SWITCH_MAP_ENABLE = no




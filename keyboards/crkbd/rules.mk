# Build Options
#   change yes to no to disable
#

DEFAULT_FOLDER = crkbd/rev1

RGBLIGHT_SUPPORTED = yes
RGB_MATRIX_SUPPORTED = yes

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


# CONVERTER - if you use a listed MCU comment the first line and uncomment the appropiate line

#CONVERT_TO = promicro_rp2040
CONVERT_TO = rp2040_ce  
#this is for rp2040 promicro with qwiic i2c connector (like sparkfun's), thanks Drashna  

#CONVERT_TO = kb2040
#CONVERT_TO = blok
#CONVERT_TO = elite_pi


# Following for rp2040
# check halconf.h, mucuconf.h

############ MCU & BOOTLOADER ############

# MCU name
#MCU = atmega32u4

# Bootloader selection
#BOOTLOADER = caterina

################## BASIC ##################

BOOTMAGIC_ENABLE = no      # Enable Bootmagic Lite
EXTRAKEY_ENABLE = yes       # Audio control and System control
CONSOLE_ENABLE = no         # Console for debug
COMMAND_ENABLE = no         # Commands for debug and configuration
SWAP_HANDS_ENABLE = no
NKRO_ENABLE = yes           # Enable N-Key Rollover


# RGB Underglow
RGBLIGHT_ENABLE = no
RGB_MATRIX_ENABLE = yes
RGB_MATRIX_DRIVER = ws2812
VIALRGB_ENABLE = yes

# OLED
OLED_ENABLE = yes
OLED_DRIVER = ssd1306
WPM_ENABLE = yes

# Mousekey
MOUSEKEY_ENABLE = yes



########### PLUS ###########

# Haptic BZZZ // all defined and added in haptic.c， config.h， and keymap.c
#HAPTIC_ENABLE = yes				# Enable Pimoroni Haptic Bzzz LRA (+1192)
#HAPTIC_DRIVER += DRV2605L

# AUDIO // config.h
AUDIO_ENABLE = yes
AUDIO_DRIVER = pwm_hardware

# POINTING DEVICE - PIMORONI TRACKBALL
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = pimoroni_trackball

# Encoder
ENCODER_MAP_ENABLE = yes	# New update for encoder
ENCODER_ENABLE = yes 	# Callbacks for encoder, replace with encoder_map_enable




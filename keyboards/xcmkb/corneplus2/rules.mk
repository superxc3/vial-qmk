# Build Options
#   change yes to no to disable
#

# Ignore some warnings during the build, likely to be fixed before RP2040 PR is merged
ALLOW_WARNINGS = yes

# LTO must be disabled for RP2040 builds
LTO_ENABLE = no

# PIO serial/WS2812 drivers must be used on RP2040
SERIAL_DRIVER = vendor
WS2812_DRIVER = vendor


# NATIVE RP2040 - no CONVERT_TO.
#
# The PCB is a Pro Micro footprint populated with an RP2040 Pro Micro
# replacement, but this board addresses the MCU directly: processor/bootloader
# come from keyboard.json ("RP2040"/"rp2040"), BOARD defaults to
# GENERIC_PROMICRO_RP2040, and every pin is named GPxx.
#
# Why not CONVERT_TO = rp2040_ce (what xcmkb/corneplus does): the converter
# ships its own _pin_defs.h that shadows the RP2040 vendor one, so GP0..GP29
# are undefined in a converted build. The trackpad needs GP13/GP14 for RDY/RST
# and GP14 is not even on the Pro Micro footprint, so the promicro alias layer
# cannot express this hardware. See readme.md.
#
# Trade-off: the module pinout is baked in. Moving to a kb2040/elite_pi means
# re-deriving the GPxx numbers in keyboard.json, not flipping one line.

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

# AUDIO - removed on corneplus2. Was: AUDIO_ENABLE = yes / AUDIO_DRIVER = pwm_hardware
# on GP9 (promicro B5). See the audio block in keymaps/vialplus/config.h to restore.
AUDIO_ENABLE = no

# POINTING DEVICE - AZOTEQ IQS5XX TRACKPAD (right half)
# POINTING_DEVICE_ENABLE/DRIVER for the mouse-fallback bridge is set per-keymap
# (see keymaps/vialplus/rules.mk), matching sofleplus2's pattern.
DIGITIZER_ENABLE = yes
DIGITIZER_DRIVER = azoteq_iqs5xx

# Encoder
ENCODER_MAP_ENABLE = yes	# New update for encoder
ENCODER_ENABLE = yes 	# Callbacks for encoder, replace with encoder_map_enable

########### VIA VIAL ###########

# VIA VIAL
VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
VIAL_ENCODERS_ENABLE = yes

# Reduce size of Vial
QMK_SETTINGS = yes
COMBO_ENABLE = yes
TAP_DANCE_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes

########### TRACKPAD ###########

# macOS/Linux dual-mode: mouse fallback for hosts without Precision Touchpad
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = digitizer

# IQS5xx RDY (GP13) edge-gated reads + RST (GP14) hard reset/recovery
SRC += trackpad_rdy_rst.c

########### OS DETECTION ###########
OS_DETECTION_ENABLE = yes

########### DIP SWITCH ###########
# SW1, the disc centre click on GP8 -- see config.h and keymap.c
DIP_SWITCH_ENABLE = yes

########### BOOTMAGIC ###########
# v5.11: hold top-left (left half) / top-right (right half) while plugging in = bootloader,
# without erasing EEPROM (bootmagic_reset_eeprom() is a no-op in keymap.c)
BOOTMAGIC_ENABLE = yes

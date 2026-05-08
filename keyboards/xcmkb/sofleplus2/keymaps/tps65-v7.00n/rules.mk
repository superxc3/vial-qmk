
############ CAPS WORD ############
# read more here https://docs.qmk.fm/features/caps_word, but it may relates to COMMAND_ENABLE = no, already disabled by default so no conflict here.
CAPS_WORD_ENABLE = yes
TRI_LAYER_ENABLE = yes
LEADER_ENABLE = yes


########### VIA VIAL ###########

VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
VIAL_ENCODERS_ENABLE = yes
VIAL_KEYBOARD_UID = {0x5B,0xE9,0x17,0xA4,0x6C,0x30,0xD8,0xF5}
VIAL_USER_CONFIG_ENABLE = yes

QMK_SETTINGS = yes
COMBO_ENABLE = yes
TAP_DANCE_ENABLE = yes
KEY_OVERRIDE_ENABLE = yes
########### PLUS ###########

# macOS dual-mode: mouse fallback for non-PTP hosts (macOS/Linux)
POINTING_DEVICE_ENABLE = yes
POINTING_DEVICE_DRIVER = digitizer

########### OS DETECTION ###########
OS_DETECTION_ENABLE = yes

########### DIP SWITCH ###########
# v7.00n has no DIP switch — base rules.mk already sets DIP_SWITCH_ENABLE = no

########### VIAL 0.7.4 ###########
REPEAT_KEY_ENABLE = yes
LAYER_LOCK_ENABLE = yes

